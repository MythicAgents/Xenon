from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from xenon.mythic.agent_functions.inline_execute import *
import logging, sys
# BOF utilities
from .utils.mythicrpc_utilities import *
from .utils.bof_utilities import *

logging.basicConfig(level=logging.INFO)


class SaNslookupArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="hostname", 
                type=ParameterType.String, 
                default_value="",
                description="DNS name to lookup.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True
                )]
                
            ),
            CommandParameter(
                name="dns_server", 
                type=ParameterType.String,
                default_value="",
                description="DNS server IP or hostname.",
                parameter_group_info=[ParameterGroupInfo(
                    required=False
                )]
            ),
            # CommandParameter(
            #     name="record_type",
            #     type=ParameterType.ChooseOne,
            #     default_value=1,        # DNS_TYPE_A
            #     choices=[
            #         {"DNS_TYPE_A": 1},
            #         {"DNS_TYPE_NS": 2},
            #         {"DNS_TYPE_MD": 3},
            #         {"DNS_TYPE_MF": 4},
            #         {"DNS_TYPE_CNAME": 5},
            #         {"DNS_TYPE_SOA": 6},
            #         {"DNS_TYPE_MB": 7},
            #         {"DNS_TYPE_MG": 8},
            #         {"DNS_TYPE_MR": 9},
            #         {"DNS_TYPE_NULL": 10},
            #         {"DNS_TYPE_WKS": 11},
            #         {"DNS_TYPE_PTR": 12},
            #         {"DNS_TYPE_HINFO": 13},
            #         {"DNS_TYPE_MINFO": 14},
            #         {"DNS_TYPE_MX": 15},
            #         {"DNS_TYPE_TEXT": 16},
            #         {"DNS_TYPE_RP": 17},
            #         {"DNS_TYPE_AFSDB": 18},
            #         {"DNS_TYPE_X25": 19},
            #         {"DNS_TYPE_ISDN": 20},
            #         {"DNS_TYPE_RT": 21},
            #         {"DNS_TYPE_NSAP": 22},
            #         {"DNS_TYPE_NSAPPTR": 23},
            #         {"DNS_TYPE_SIG": 24},
            #         {"DNS_TYPE_KEY": 25},
            #         {"DNS_TYPE_PX": 26},
            #         {"DNS_TYPE_GPOS": 27},
            #         {"DNS_TYPE_AAAA": 28},
            #         {"DNS_TYPE_LOC": 29},
            #         {"DNS_TYPE_NXT": 30},
            #         {"DNS_TYPE_EID": 31},
            #         {"DNS_TYPE_NIMLOC": 32},
            #         {"DNS_TYPE_SRV": 33},
            #         {"DNS_TYPE_ATMA": 34},
            #         {"DNS_TYPE_NAPTR": 35},
            #         {"DNS_TYPE_KX": 36},
            #         {"DNS_TYPE_CERT": 37},
            #         {"DNS_TYPE_A6": 38},
            #         {"DNS_TYPE_DNAME": 39},
            #         {"DNS_TYPE_SINK": 40},
            #         {"DNS_TYPE_OPT": 41},
            #         {"DNS_TYPE_DS": 43},
            #         {"DNS_TYPE_RRSIG": 46},
            #         {"DNS_TYPE_NSEC": 47},
            #         {"DNS_TYPE_DNSKEY": 48},
            #         {"DNS_TYPE_DHCID": 49},
            #         {"DNS_TYPE_UINFO": 100},
            #         {"DNS_TYPE_UID": 101},
            #         {"DNS_TYPE_GID": 102},
            #         {"DNS_TYPE_UNSPEC": 103},
            #         {"DNS_TYPE_ADDRS": 248},
            #         {"DNS_TYPE_TKEY": 249},
            #         {"DNS_TYPE_TSIG": 250},
            #         {"DNS_TYPE_IXFR": 251},
            #         {"DNS_TYPE_AXFR": 252},
            #         {"DNS_TYPE_MAILB": 253},
            #         {"DNS_TYPE_MAILA": 254},
            #         {"DNS_TYPE_ALL": 255},
            #         {"DNS_TYPE_ANY": 255},
            #         {"DNS_TYPE_WINS": 65281},
            #         {"DNS_TYPE_WINSR": 65282},
            #     ],
            #     description="Select the DNS record type.",
            # )     
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply host and port.")
        self.add_arg("command", self.command_line)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)

class SaNslookupAlias(CoffCommandBase):
    cmd = "sa_nslookup"
    needs_admin = False
    help_cmd = "sa_nslookup [hostname] [opt:dns server] [opt: record type]"
    description = "[SituationalAwareness] Make a DNS query. DNS server is the server you want to query (do not specify or 0 for default) record type is something like A, AAAA, or ANY. Some situations are limited due to observed crashes"
    version = 1
    script_only = True
    author = "@trustedsec"
    argument_class = SaNslookupArguments
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

        file_name = "nslookup.x64.o"
        arguments = [
            [
                "string", 
                taskData.args.get_arg("hostname")
            ], 
            [
                "string", 
                taskData.args.get_arg("dns_server")
            ],
            [
                "int16", 
                1 #A record
                # taskData.args.get_arg("record_type")
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