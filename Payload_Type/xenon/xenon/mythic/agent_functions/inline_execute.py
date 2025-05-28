from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *

from ..utils.packer import serialize_int, serialize_bool, serialize_string

import logging, sys

logging.basicConfig(level=logging.INFO)



class InlineExecuteArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="bof_name",
                cli_name="BOF",
                display_name="BOF",
                type=ParameterType.ChooseOne,
                dynamic_query_function=self.get_files,
                description="Already existing BOF to execute (e.g. whoami.x64.o)",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=1
                    )
                ]
            ),
            CommandParameter(
                name="bof_file",
                display_name="New BOF",
                type=ParameterType.File,
                description="A new BOF to execute. After uploading once, you can just supply the bof_name parameter",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True, 
                        group_name="New", 
                        ui_position=1,
                    )
                ]
            ),
            CommandParameter(
                name="bof_arguments",
                cli_name="Arguments",
                display_name="Arguments",
                type=ParameterType.TypedArray,
                default_value=[],
                choices=["int16", "int32", "string", "wchar", "base64"],
                description="""Arguments to pass to the BOF via the following way:
                -s:123 or int16:123
                -i:123 or int32:123
                -z:hello or string:hello
                -Z:hello or wchar:hello
                -b:abc== or base64:abc==""",
                typedarray_parse_function=self.get_arguments,
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Default",
                        ui_position=4
                    ),
                    ParameterGroupInfo(
                        required=False,
                        group_name="New",
                        ui_position=4
                    ),
                ]
            ),
        ]
        
    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply arguments")
        raise ValueError("Must supply named arguments or use the modal")

    async def parse_dictionary(self, dictionary_arguments):
         # Allowed argument keys
        expected_args = {"bof_arguments", "bof_file", "bof_name"}

        # Check if dictionary contains only allowed keys
        invalid_keys = set(dictionary_arguments.keys()) - expected_args
        if invalid_keys:
            raise ValueError(f"Invalid arguments provided: {', '.join(invalid_keys)}")
        
        # Load only allowed arguments
        filtered_arguments = {k: v for k, v in dictionary_arguments.items() if k in expected_args}
        self.load_args_from_dictionary(filtered_arguments)
        
        #self.load_args_from_dictionary(dictionary_arguments)
   

    async def get_arguments(self, arguments: PTRPCTypedArrayParseFunctionMessage) -> PTRPCTypedArrayParseFunctionMessageResponse:
        argumentSplitArray = []
        for argValue in arguments.InputArray:
            argSplitResult = argValue.split(" ")
            for spaceSplitArg in argSplitResult:
                argumentSplitArray.append(spaceSplitArg)
        bof_arguments = []
        for argument in argumentSplitArray:
            argType,value = argument.split(":",1)
            value = value.strip("\'").strip("\"")
            if argType == "":
                pass
            elif argType == "int16" or argType == "-s" or argType == "s":
                bof_arguments.append(["int16", int(value)])
            elif argType == "int32" or argType == "-i" or argType == "i":
                bof_arguments.append(["int32", int(value)])
            elif argType == "string" or argType == "-z" or argType == "z":
                bof_arguments.append(["string",value])
            elif argType == "wchar" or argType == "-Z" or argType == "Z":
                bof_arguments.append(["wchar",value])
            elif argType == "base64" or argType == "-b" or argType == "b":
                bof_arguments.append(["base64",value])
            else:
                return PTRPCTypedArrayParseFunctionMessageResponse(Success=False,
                                                                   Error=f"Failed to parse argument: {argument}: Unknown value type.")

        argumentResponse = PTRPCTypedArrayParseFunctionMessageResponse(Success=True, TypedArray=bof_arguments)
        return argumentResponse

    async def get_files(self, callback: PTRPCDynamicQueryFunctionMessage) -> PTRPCDynamicQueryFunctionMessageResponse:
        response = PTRPCDynamicQueryFunctionMessageResponse()
        file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
            CallbackID=callback.Callback,
            LimitByCallback=False,
            IsDownloadFromAgent=False,
            IsScreenshot=False,
            IsPayload=False,
            Filename="",
        ))
        if file_resp.Success:
            file_names = []
            for f in file_resp.Files:
                if f.Filename not in file_names and f.Filename.endswith(".o"):
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


class InlineExecuteCommand(CommandBase):
    cmd = "inline_execute"
    needs_admin = False
    help_cmd = "inline_execute -BOF [COFF.o] [-Arguments [optional arguments]]"
    description = "Execute a Beacon Object File in the current process thread. (e.g., inline_execute -BOF listmods.x64.o -Arguments int32:1234) \n [!!WARNING!! Incorrect argument types can crash the Agent process.]"
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = InlineExecuteArguments
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
            groupName = taskData.args.get_parameter_group_name()
            if groupName == "New":
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    AgentFileID=taskData.args.get_arg("bof_file")
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        pass
                    else:
                        raise Exception("Failed to find that file")
                else:
                    raise Exception("Error from Mythic trying to get file: " + str(file_resp.Error))
            elif groupName == "Default":
                # We're trying to find an already existing file and use that
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename=taskData.args.get_arg("bof_name"),
                    LimitByCallback=False,
                    MaxResults=1
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        logging.info(f"Found existing BOF file replacing with file_id : {file_resp.Files[0].AgentFileId}")
                        taskData.args.add_arg("bof_file", file_resp.Files[0].AgentFileId)
                        taskData.args.remove_arg("bof_name")    # Don't need this anymore
                        
                        # Set display parameters
                        response.DisplayParams = "-bof_file {} -bof_arguments {}".format(
                            file_resp.Files[0].Filename,
                            taskData.args.get_arg("bof_arguments")
                        )

                    elif len(file_resp.Files) == 0:
                        raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
                else:
                    raise Exception("Error from Mythic trying to search files:\n" + str(file_resp.Error))
        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        
        # Debugging
        # logging.info(taskData.args.to_json())
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp