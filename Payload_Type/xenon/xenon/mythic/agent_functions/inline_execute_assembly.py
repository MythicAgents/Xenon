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

# Wrapper function for Inline-EA
# BoF Credits: https://github.com/EricEsquivel/Inline-EA

class InlineExecuteAssemblyArguments(TaskArguments):
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
                    InlineExecuteAssemblyCommand.help_cmd
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

class InlineExecuteAssemblyCommand(CoffCommandBase):
    cmd = "inline_execute_assembly"
    needs_admin = False
    help_cmd = "inline_execute_assembly -Assembly [file] [-Arguments [assembly args] [--patchexit] [--amsi] [--etw]]"
    description = "Execute a .NET Assembly in the current process using @EricEsquivel's BOF \"Inline-EA\" (e.g., inline_execute_assembly -Assembly SharpUp.exe -Arguments \"audit\" --patchexit --amsi --etw)"
    version = 1
    author = "@c0rnbread"
    script_only = True
    attackmapping = []
    argument_class = InlineExecuteAssemblyArguments
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
                    a. .net name or file
                    b. .net arguments
                    c. patchexit, amsi, etc
                2. Get byte contents of the assembly
                3. pass arguments into subtask in order
                    a. AssemblyBytes
                    b. AssemblyLength
                    c. DotnetArguments
                    d. PatchExit
                    e. PatchAmsi
                    f. PatchEtw
        '''
        
        try:
            ######################################
            #                                    #
            #   Group (New Assembly | Default)   #
            #                                    #
            ######################################
            groupName = taskData.args.get_parameter_group_name()
            
            if groupName == "New Assembly":
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    AgentFileID=taskData.args.get_arg("assembly_file")
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        pass
                    else:
                        raise Exception("Failed to find that file")
                else:
                    raise Exception("Error from Mythic trying to get file: " + str(file_resp.Error))
                
                # Set display parameters
                response.DisplayParams = "-Assembly {} -Arguments {} --patchexit {} --amsi {} --etw {}".format(
                    file_resp.Files[0].Filename,
                    taskData.args.get_arg("assembly_arguments"),
                    taskData.args.get_arg("patch_exit"),
                    taskData.args.get_arg("amsi"),
                    taskData.args.get_arg("etw")
                )
                
                taskData.args.add_arg("assembly_name", file_resp.Files[0].Filename)
                taskData.args.remove_arg("assembly_file")
            
            elif groupName == "Default":
                # We're trying to find an already existing file and use that
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename=taskData.args.get_arg("assembly_name"),
                    LimitByCallback=False,                                
                    MaxResults=1
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        logging.info(f"Found existing Assembly with File ID : {file_resp.Files[0].AgentFileId}")

                        taskData.args.remove_arg("assembly_name")    # Don't need this anymore
                        
                        # Set display parameters
                        response.DisplayParams = "-Assembly {} -Arguments {} --patchexit {} --amsi {} --etw {}".format(
                            file_resp.Files[0].Filename,
                            taskData.args.get_arg("assembly_arguments"),
                            taskData.args.get_arg("patch_exit"),
                            taskData.args.get_arg("amsi"),
                            taskData.args.get_arg("etw")
                        )

                    elif len(file_resp.Files) == 0:
                        raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
                else:
                    raise Exception("Error from Mythic trying to search files:\n" + str(file_resp.Error))

            ######################################
            #                                    #
            #   Send SubTask to inline_execute   #
            #                                    #
            ######################################      
                  
            # Get the file contents of the .NET assembly ( base64 ( assembly bytes ) )
            assembly_contents = await SendMythicRPCFileGetContent(
                MythicRPCFileGetContentMessage(AgentFileId=file_resp.Files[0].AgentFileId)
            )
            
            #b64_assembly_contents = assembly_contents.Content.hex()
            

            #logging.info(b64_assembly_contents[:100])
            
            
            # Arguments depend on the BOF
            file_name = "inline-ea.x64.o"
            arguments = [
                [
                    "bytes", 
                    assembly_contents.Content.hex()                             # Raw bytes of Assembly
                ],
                [
                    "int32",               
                    len(assembly_contents.Content)                             # Assembly length
                ],
                [
                    "wchar", 
                    taskData.args.get_arg("assembly_arguments")                 # Assembly argument string
                ],
                [
                    "int32",
                    taskData.args.get_arg("patch_exit") # BOOL
                ],
                [
                    "int32",
                    taskData.args.get_arg("amsi") # BOOL
                ],
                [
                    "int32",
                    taskData.args.get_arg("etw") # BOOL
                ]
                
            ]
            
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
                        "bof_arguments": arguments
                    }),
                    Token=taskData.Task.TokenID,
                )
            )
            
            return response
            
            
            # Don't actually need to send any of these to the Agent
            # taskData.args.remove_arg("assembly_file")
            # taskData.args.remove_arg("assembly_name")
            # taskData.args.remove_arg("assembly_arguments")
            
            # Debugging
            # logging.info(taskData.args.to_json())
            
            #return response

        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp