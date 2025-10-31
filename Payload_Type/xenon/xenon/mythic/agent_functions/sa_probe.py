from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from xenon.mythic.agent_functions.inline_execute import *
import logging, sys
# BOF utilities
from .utils.mythicrpc_utilities import *
from .utils.bof_utilities import *

logging.basicConfig(level=logging.INFO)


class SaProbeArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="host", 
                type=ParameterType.String, 
                description="IP Address or hostname"
            ),
            CommandParameter(
                name="port", 
                type=ParameterType.Number, 
                description="Port"
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply host and port.")
        self.add_arg("command", self.command_line)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)

class SaProbeAlias(CoffCommandBase):
    cmd = "sa_probe"
    needs_admin = False
    help_cmd = "sa_probe [host] [port]"
    description = "[SituationalAwareness] Check if a specific port is open"
    version = 1
    script_only = True
    author = "@trustedsec"
    argument_class = SaProbeArguments
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

        file_name = "probe.x64.o"
        arguments = [
            [
                "string", 
                taskData.args.get_arg("host")], 
            [
                "int32", 
                taskData.args.get_arg("port")
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