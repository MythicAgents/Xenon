from mythic_container.MythicCommandBase import *
import json
from .utils.bof_utilities import *
from .utils.mythicrpc_utilities import *

class PowerShellImportArguments(TaskArguments):

    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="file",
                cli_name="File",
                display_name="File",
                type=ParameterType.File,
                description="PowerShell script to import.",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=1
                    )
                ]
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise Exception(
                "Require an assembly to execute.\n\tUsage: {}".format(
                    ExecuteAssemblyCommand.help_cmd
                )
            )
        if self.command_line[0] == "{":
            self.load_args_from_json_string(self.command_line)
        else:
            parts = self.command_line.split(" ", maxsplit=1)
            self.add_arg("file", parts[1])

class PowerShellImportCommand(CoffCommandBase):
    cmd = "powershell_import"
    needs_admin = False
    help_cmd = "powershell_import -File [script.ps1]"
    description = "Import PowerShell script into the current process."
    version = 1
    script_only = True
    author = "@c0rnbread"
    argument_class = PowerShellImportArguments
    attackmapping = []
    attributes = CommandAttributes(
        dependencies=["powerchell"],
        alias=True
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )

        # Set display parameters
        response.DisplayParams = "{}".format(taskData.args.get_arg("file"))

        # Upload PowerShell script file
        script_name = "powershell_import_" + taskData.args.get_arg("file")
        upload_resp = await create_mythic_file(taskData.Task.ID, taskData.args.get_arg("file"), script_name, False)
        if not upload_resp.Success:
            response.Success = False
            response.Error = f"Failed to upload PowerShell script: {upload_resp.Error}"
            return response
        
        # Set the file UUID for the PowerShell script
        POWER_SHELL_IMPORT.set_file(upload_resp.AgentFileId)

        response.TaskStatus = MythicStatus.Success
        response.Completed = True
        await SendMythicRPCResponseCreate(MythicRPCResponseCreateMessage(
                    TaskID=taskData.Task.ID,
                    Response=f"PowerShell script uploaded with File ID: [{upload_resp.AgentFileId}]\n".encode()
                ))

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp