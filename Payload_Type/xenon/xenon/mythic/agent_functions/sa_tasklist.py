from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from xenon.mythic.agent_functions.inline_execute import *
import logging, sys
import json
# BOF utilities
from .utils.mythicrpc_utilities import *
from .utils.bof_utilities import *

logging.basicConfig(level=logging.INFO)


class SaTasklistArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="server", 
                type=ParameterType.String, 
                default_value="",
                description="Target server to run tasklist for (optional, leave empty for local)",
                parameter_group_info=[ParameterGroupInfo(
                    required=False
                )]
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            # No arguments provided, use empty string for local
            self.add_arg("server", "")
        else:
            self.add_arg("server", self.command_line)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)

class SaTasklistAlias(CoffCommandBase):
    cmd = "sa_tasklist"
    needs_admin = False
    help_cmd = "sa_tasklist [server]"
    description = "[SituationalAwareness] tasklist implementation in BOF"
    version = 1
    script_only = True
    author = "@c0rnbread"
    argument_class = SaTasklistArguments
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
        file_name = "tasklist.x64.o"
        arguments = [
            [
                "string", 
                taskData.args.get_arg("server") or ""
            ]
        ]
        
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

