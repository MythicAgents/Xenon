from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from xenon.mythic.agent_functions.inline_execute import *
import logging, sys
# BOF utilities
from .utils.mythicrpc_utilities import *
from .utils.bof_utilities import *

logging.basicConfig(level=logging.INFO)


class SaArpArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
        ]

    async def parse_arguments(self):
        if len(self.command_line) > 0:
            if self.command_line[0] == "{":
                self.load_args_from_json_string(self.command_line)

class SaArpAlias(CoffCommandBase):
    cmd = "sa_arp"
    needs_admin = False
    help_cmd = "sa_arp"
    description = "[SituationalAwareness] List ARP table"
    version = 1
    script_only = True
    author = "@trustedsec"
    argument_class = SaArpArguments
    attributes=CommandAttributes(
        dependencies=["inline_execute"],
        alias=True
    )
    
    async def create_go_tasking(self, taskData: MythicCommandBase.PTTaskMessageAllData) -> MythicCommandBase.PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        
        # Arguments depend on the BOF

        file_name = "arp.x64.o"
        arguments = []

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