from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json


class WmiExecuteArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="command",
                cli_name="command",
                display_name="command",
                type=ParameterType.String,
                description="Full command line to execute via WMI Win32_Process::Create.",
                parameter_group_info=[
                    ParameterGroupInfo(required=True, ui_position=1)
                ],
            ),
            CommandParameter(
                name="host",
                cli_name="host",
                display_name="Host",
                type=ParameterType.String,
                description="Remote host. If empty, execute locally.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=2)
                ],
            ),
            CommandParameter(
                name="username",
                cli_name="username",
                display_name="username",
                type=ParameterType.String,
                description="Optional username for WMIC auth.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=3)
                ],
            ),
            CommandParameter(
                name="password",
                cli_name="password",
                display_name="password",
                type=ParameterType.String,
                description="Optional password for WMIC auth.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=4)
                ],
            ),
            CommandParameter(
                name="domain",
                cli_name="domain",
                display_name="domain",
                type=ParameterType.String,
                description="Optional domain for WMIC auth.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, ui_position=5)
                ],
            ),
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise Exception("Require JSON blob, but got empty command line.")
        if self.command_line[0] != "{":
            raise Exception("Require JSON blob, but got raw command line.")
        self.load_args_from_json_string(self.command_line)


class wmiexecuteCommand(CommandBase):
    cmd = "wmiexecute"
    needs_admin = False
    help_cmd = "wmiexecute -command [command] -host [host]"
    description = "Script-only WMI wrapper that launches WMIC through shell."
    version = 1
    author = "@Lavender-exe"
    argument_class = WmiExecuteArguments
    attackmapping = ["T1047"]
    script_only = True
    attributes = CommandAttributes(suggested_command=True)

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(TaskID=taskData.Task.ID, Success=True)

        command = taskData.args.get_arg("command")
        host = taskData.args.get_arg("host") or ""
        username = taskData.args.get_arg("username") or ""
        password = taskData.args.get_arg("password") or ""
        domain = taskData.args.get_arg("domain") or ""

        display_params = f"-command {command}"
        if host:
            display_params += f" -host {host}"
        if username:
            display_params += f" -username {username}"
        response.DisplayParams = display_params

        wmic_parts = ["wmic"]
        if host:
            wmic_parts.append(f"/node:\"{host}\"")

        if username:
            if domain:
                wmic_parts.append(f"/user:\"{domain}\\{username}\"")
            else:
                wmic_parts.append(f"/user:\"{username}\"")
            if password:
                wmic_parts.append(f"/password:\"{password}\"")

        # Wrap in cmd.exe /Q /c so the process spawns a shell on the remote host.
        # This matches impacket's wmiexec behaviour and is required for reliable
        # remote process creation via Win32_Process::Create.
        wrapped = f"cmd.exe /Q /c {command}"
        escaped = wrapped.replace('"', '\\"')
        wmic_parts.append(f"process call create \"{escaped}\"")
        shell_cmd = " ".join(wmic_parts)

        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=taskData.Task.ID,
                CommandName="shell",
                Params=json.dumps({"command": shell_cmd}),
            )
        )

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
