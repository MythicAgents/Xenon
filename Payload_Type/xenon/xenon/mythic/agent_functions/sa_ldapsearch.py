from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from xenon.mythic.agent_functions.inline_execute import *
import logging, sys
# BOF utilities
from .utils.mythicrpc_utilities import *
from .utils.bof_utilities import *

logging.basicConfig(level=logging.INFO)


class SaLdapsearchArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="query", 
                type=ParameterType.String, 
                default_value="",
                description="E.g., (objectclass=*)",
                parameter_group_info=[ParameterGroupInfo(
                    required=True
                )]
                
            ),
            CommandParameter(
                name="ldap_attributes", 
                type=ParameterType.String,
                default_value="",
                description="Attributes filter. e.g., objectSID,name",
                parameter_group_info=[ParameterGroupInfo(
                    required=False
                )]
            ),
            CommandParameter(
                name="results_limit", 
                type=ParameterType.Number,
                default_value=0,
                description="Limit objects results",
                parameter_group_info=[ParameterGroupInfo(
                    required=False
                )]
            ),
            CommandParameter(
                name="hostname", 
                type=ParameterType.String,
                default_value="",
                description="Limit objects results",
                parameter_group_info=[ParameterGroupInfo(
                    required=False
                )]
            ),
            CommandParameter(
                name="domain", 
                type=ParameterType.String,
                default_value="",
                description="Limit objects results",
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

class SaLdapsearchAlias(CoffCommandBase):
    cmd = "sa_ldapsearch"
    needs_admin = False
    help_cmd = "sa_ldapsearch [query] [opt: attribute] [opt: results_limit] [opt: DC hostname or IP] [opt: Distingished Name]"
    description = "[SituationalAwareness] Execute LDAP searches (NOTE: specify *,ntsecuritydescriptor as attribute parameter if you want all attributes + base64 encoded ACL of the objects, this can then be resolved using BOFHound. Could possibly break pagination, although everything seemed fine during testing.)"
    version = 1
    script_only = True
    author = "@trustedsec"
    argument_class = SaLdapsearchArguments
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

        file_name = "ldapsearch.x64.o"
        arguments = [
            [
                "string", 
                taskData.args.get_arg("query")
            ], 
            [
                "string", 
                taskData.args.get_arg("ldap_attributes")
            ],
            [
                "int32",
                taskData.args.get_arg("results_limit")
            ],
            [
                "int32",
                int(0)
            ],
            [
                "string", 
                taskData.args.get_arg("hostname")], 
            [
                "string", 
                taskData.args.get_arg("domain")
            ],
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