import json
from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from xenon.mythic.agent_functions.inline_execute import *
# BOF utilities
from .utils.mythicrpc_utilities import *
from .utils.bof_utilities import *

class SaWhoamiArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
        ]

    async def parse_arguments(self):
        if len(self.command_line) > 0:
            if self.command_line[0] == "{":
                self.load_args_from_json_string(self.command_line)

class SaWhoamiAlias(CoffCommandBase):
    cmd = "sa_whoami"
    help_cmd = "sa_whoami"
    description = "[SituationalAwareness] List whoami /all"
    needs_admin = False
    argument_class = SaWhoamiArguments
    version = 1
    script_only = True
    author = "@trustedsec"
    attributes=CommandAttributes(
        dependencies=["inline_execute"],
        alias=True
    )
    
    async def create_go_tasking(self, taskData: MythicCommandBase.PTTaskMessageAllData) -> MythicCommandBase.PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        
        # No user args for this BOF

        file_name = "whoami.x64.o"
        arguments = []

        # Upload desired BOF if it hasn't been before (per payload uuid)
        succeeded = await upload_sa_bof_if_missing(file_name=file_name, taskData=taskData)
        if not succeeded:
            response.Success = False
            response.Error = f"Failed to upload or check BOF \"{file_name}\"."

        # Run inline_execute subtask
        subtask = await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                taskData.Task.ID,
                CommandName="inline_execute",
                SubtaskCallbackFunction="coff_completion_callback",
                Params=json.dumps({
                    "bof_name": file_name,
                    "bof_arguments": arguments
                }),
                Token=taskData.Task.TokenID,
            )
        )


        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp