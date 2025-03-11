from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from xenon.mythic.agent_functions.inline_execute import *
import logging, sys
# BOF utilities
from .utils.mythicrpc_utilities import *
from .utils.bof_utilities import *

logging.basicConfig(level=logging.INFO)


class SaScQcArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="service_name", 
                type=ParameterType.String, 
                default_value="",
                description="Name of Windows service to run sc qc on",
                parameter_group_info=[ParameterGroupInfo(
                    required=True
                )]
            ),
            CommandParameter(
                name="server", 
                type=ParameterType.String, 
                default_value="",
                description="Optional, name of server to enumerate services",
                parameter_group_info=[ParameterGroupInfo(
                    required=False
                )]
                
            ) 
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply host and port.")
        self.add_arg("command", self.command_line)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)

class SaScQcAlias(CoffCommandBase):
    cmd = "sa_sc_qc"
    needs_admin = False
    help_cmd = "sa_sc_qc [service name] [opt:server]"
    description = "[SituationalAwareness] sc qc impelmentation in BOF"
    version = 1
    script_only = True
    author = "@trustedsec"
    argument_class = SaScQcArguments
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

        file_name = "sc_qc.x64.o"
        arguments = [
            [
                "string", 
                taskData.args.get_arg("service_name")
            ],
            [
                "string", 
                taskData.args.get_arg("server")
            ]
        ]
        
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