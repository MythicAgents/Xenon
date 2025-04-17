from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from ..utils.packer import serialize_int, serialize_bool, serialize_string
import logging, sys
import os
import tempfile
import donut

logging.basicConfig(level=logging.INFO)


class InjectShellcodeArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="shellcode_name",
                cli_name="File",
                display_name="File",
                type=ParameterType.ChooseOne,
                dynamic_query_function=self.get_files,
                description="Already existing PIC Shellcode to execute (e.g. mimi.bin)",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=1
                    )
                ]),
            CommandParameter(
                name="shellcode_file",
                display_name="New File",
                type=ParameterType.File,
                description="A new PIC shellcode to execute. After uploading once, you can just supply the -File parameter",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True, 
                        group_name="New File", 
                        ui_position=1,
                    )
                ]
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
            raise ValueError("Must supply arguments")
        raise ValueError("Must supply named arguments or use the modal")

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise Exception(
                "Require shellcode to execute.\n\tUsage: {}".format(
                    InjectShellcodeCommand.help_cmd
                )
            )
        self.load_args_from_json_string(self.command_line)
        # if self.command_line[0] == "{":
        #     self.load_args_from_json_string(self.command_line)
        # else:
        #     parts = self.command_line.split(" ", maxsplit=1)
        #     self.add_arg("shellcode_name", parts[0])
        #     self.add_arg("arguments", "")
        #     if len(parts) == 2:
        #         self.add_arg("arguments", parts[1])
        
class InjectShellcodeCommand(CommandBase):
    cmd = "inject_shellcode"
    needs_admin = False
    help_cmd = "inject_shellcode -File [mimi.bin]"
    description = "Execute PIC shellcode. (e.g., inject_shellcode -File mimi.bin"
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
            #   Group (New File | Default)   #
            #                                    #
            ######################################
            groupName = taskData.args.get_parameter_group_name()
            
            if groupName == "New File":
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    AgentFileID=taskData.args.get_arg("shellcode_file")
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        taskData.args.add_arg("shellcode_name", file_resp.Files[0].Filename)
                        #taskData.args.remove_arg("shellcode_file")
                    else:
                        raise Exception("Failed to find that file")
                else:
                    raise Exception("Error from Mythic trying to get file: " + str(file_resp.Error))
                                
            elif groupName == "Default":
                # We're trying to find an already existing file and use that
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename=taskData.args.get_arg("shellcode_name"),
                    LimitByCallback=False,
                    MaxResults=1
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        # taskData.args.remove_arg("shellcode_name")
                        pass
                    else:
                        raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
                else:
                    raise Exception("Error from Mythic trying to search files:\n" + str(file_resp.Error))
            
            # Set display parameters (only visual)
            response.DisplayParams = "-File {}".format(
                file_resp.Files[0].Filename
            )
            # Remove these args 
            #taskData.args.remove_arg("shellcode_file")
            #taskData.args.remove_arg("shellcode_name")
            
            # Only needs UUID so Agent can download file with it
            #file_uuid = file_resp.Files[0].AgentFileId
            #taskData.args.add_arg("shellcode_file_id", file_uuid)
            
            # Debugging
            logging.info(taskData.args.to_json())
            
            return response

        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp