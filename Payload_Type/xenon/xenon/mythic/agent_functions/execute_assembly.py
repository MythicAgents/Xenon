from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from ..utils.packer import serialize_int, serialize_bool, serialize_string
import logging, sys
import os
import tempfile
from .utils.bof_utilities import *
from .utils.crystal_utilities import *

logging.basicConfig(level=logging.INFO)


class ExecuteAssemblyArguments(TaskArguments):
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
                        required=True, group_name="Default", ui_position=2,
                    ),
                    ParameterGroupInfo(
                        required=True, group_name="New Assembly", ui_position=2
                    ),
                ],
            ),
            CommandParameter(
                name="patch_exit",
                cli_name="-patchexit",
                display_name="patchexit",
                type=ParameterType.Boolean,
                description="Patches System.Environment.Exit to prevent Beacon process from exiting",
                default_value=True,
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
                default_value=True,
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
                default_value=True,
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False, group_name="Default", ui_position=5,
                    ),
                    ParameterGroupInfo(
                        required=False, group_name="New Assembly", ui_position=5
                    ),
                ],
            )
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
                    ExecuteAssemblyCommand.help_cmd
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

class ExecuteAssemblyCommand(CoffCommandBase):
    cmd = "execute_assembly"
    needs_admin = False
    help_cmd = "execute_assembly -File [Assmbly Filename] [-Arguments [optional arguments]]"
    description = "Execute a .NET Assembly. Use an already uploaded assembly file or upload one with the command. (e.g., execute_assembly -File SharpUp.exe -Arguments \"audit\")"
    version = 1
    author = "@c0rnbread"
    script_only = True
    attackmapping = []
    argument_class = ExecuteAssemblyArguments
    attributes = CommandAttributes(
        builtin=False,
        dependencies=["inline_execute", "inject_shellcode"],
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

            
            # TODO
            # Check if execute_assembly PICO capability is built, if not build it
            #

            #   /root/Xenon/Payload_Type/xenon/xenon/agent_code/modules/core/execute-assembly/bin/execute_assembly.x64.bin
            #   /root/Xenon/Payload_Type/xenon/xenon/agent_code/modules/core/execute-assembly/bin/loader.x64.bin  
                        
            #
            # Link COFF -> PIC with Crystal Palace linker
            #
            
            # Args
            assembly_args = taskData.args.get_arg("assembly_arguments")
            is_patchexit = taskData.args.get_arg("patch_exit")
            is_patchamsi = taskData.args.get_arg("amsi")
            is_patchetw = taskData.args.get_arg("etw")
            # Convert to PIC
            assembly_shellcode_contents = await convert_dotnet_to_pic(
                file_resp.Files[0].AgentFileId, 
                assembly_args, 
                "x64",
                is_patchexit,
                is_patchamsi,
                is_patchetw
            )
                        
            logging.info(f"Converted .NET into Shellcode {len(assembly_shellcode_contents)} bytes")
            
            # .NET shellcode stub in Mythic
            shellcode_file_resp = await SendMythicRPCFileCreate(
                MythicRPCFileCreateMessage(TaskID=taskData.Task.ID, FileContents=assembly_shellcode_contents, DeleteAfterFetch=True)
            )
            
            if shellcode_file_resp.Success:
                shellcode_file_uuid = shellcode_file_resp.AgentFileId
            else:
                raise Exception("Failed to register execute_assembly binary: " + shellcode_file_resp.Error)
            
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