from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *

import logging, sys
import base64

logging.basicConfig(level=logging.INFO)


class AsyncExecuteArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="bof_name",
                cli_name="BOF",
                display_name="BOF",
                type=ParameterType.ChooseOne,
                dynamic_query_function=self.get_files,
                description="Already existing BOF to execute asynchronously (e.g. whoami.x64.o)",
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
                description="A new BOF to execute asynchronously. After uploading once, you can just supply the bof_name parameter",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="New",
                        ui_position=1,
                    )
                ]
            ),
            CommandParameter(
                name="bof_entrypoint",
                display_name="Entrypoint",
                type=ParameterType.String,
                description="The entrypoint to the BOF.",
                default_value="go",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Default",
                        ui_position=2,
                    ),
                    ParameterGroupInfo(
                        required=False,
                        group_name="New",
                        ui_position=2,
                    ),
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
        expected_args = {"bof_arguments", "bof_file", "bof_name", "bof_entrypoint"}
        invalid_keys = set(dictionary_arguments.keys()) - expected_args
        if invalid_keys:
            raise ValueError(f"Invalid arguments provided: {', '.join(invalid_keys)}")
        filtered_arguments = {k: v for k, v in dictionary_arguments.items() if k in expected_args}
        self.load_args_from_dictionary(filtered_arguments)

    async def get_arguments(self, arguments: PTRPCTypedArrayParseFunctionMessage) -> PTRPCTypedArrayParseFunctionMessageResponse:
        argumentSplitArray = []
        for argValue in arguments.InputArray:
            for spaceSplitArg in argValue.split(" "):
                argumentSplitArray.append(spaceSplitArg)
        bof_arguments = []
        for argument in argumentSplitArray:
            argType, value = argument.split(":", 1)
            value = value.strip("\'").strip("\"")
            if argType == "":
                pass
            elif argType in ("int16", "-s", "s"):
                bof_arguments.append(["int16", int(value)])
            elif argType in ("int32", "-i", "i"):
                bof_arguments.append(["int32", int(value)])
            elif argType in ("string", "-z", "z"):
                bof_arguments.append(["string", value])
            elif argType in ("wchar", "-Z", "Z"):
                bof_arguments.append(["wchar", value])
            elif argType in ("base64", "-b", "b"):
                bof_arguments.append(["base64", value])
            else:
                return PTRPCTypedArrayParseFunctionMessageResponse(
                    Success=False,
                    Error=f"Failed to parse argument: {argument}: Unknown value type.")
        return PTRPCTypedArrayParseFunctionMessageResponse(Success=True, TypedArray=bof_arguments)

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
        await SendMythicRPCOperationEventLogCreate(MythicRPCOperationEventLogCreateMessage(
            CallbackId=callback.Callback,
            Message=f"Failed to get files: {file_resp.Error}",
            MessageLevel="warning"
        ))
        response.Error = f"Failed to get files: {file_resp.Error}"
        return response


class AsyncExecuteCommand(CommandBase):
    cmd = "async_execute"
    needs_admin = False
    help_cmd = "async_execute -BOF [COFF.o] [-Arguments [optional arguments]]"
    description = "Execute a Beacon Object File asynchronously in a background thread. Output streams via updates. Supports BeaconWakeup / BeaconGetStopJobEvent. Use jobkill to stop."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = AsyncExecuteArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[SupportedOS.Windows],
        suggested_command=True
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        try:
            groupName = taskData.args.get_parameter_group_name()
            bof_file_id = None
            bof_filename = None

            if groupName == "New":
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    AgentFileID=taskData.args.get_arg("bof_file")
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        bof_file_id = file_resp.Files[0].AgentFileId
                        bof_filename = file_resp.Files[0].Filename
                    else:
                        raise Exception("Failed to find that file")
                else:
                    raise Exception("Error from Mythic trying to get file: " + str(file_resp.Error))
            elif groupName == "Default":
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename=taskData.args.get_arg("bof_name"),
                    LimitByCallback=False,
                    MaxResults=1
                ))
                if file_resp.Success:
                    if len(file_resp.Files) > 0:
                        bof_file_id = file_resp.Files[0].AgentFileId
                        bof_filename = file_resp.Files[0].Filename
                        taskData.args.remove_arg("bof_name")
                    elif len(file_resp.Files) == 0:
                        raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
                else:
                    raise Exception("Error from Mythic trying to search files:\n" + str(file_resp.Error))

            if bof_file_id:
                bof_contents = await SendMythicRPCFileGetContent(
                    MythicRPCFileGetContentMessage(AgentFileId=bof_file_id)
                )
                if not bof_contents.Success:
                    raise Exception("Failed to fetch BOF file from Mythic (ID: {})".format(bof_file_id))

                bof_data_b64 = base64.b64encode(bof_contents.Content).decode('utf-8')
                taskData.args.add_arg("bof_data", bof_data_b64, parameter_group_info=[ParameterGroupInfo(
                    group_name=groupName
                )])
                taskData.args.remove_arg("bof_file")
                taskData.args.remove_arg("bof_entrypoint")
                response.DisplayParams = "{} {}".format(
                    bof_filename,
                    " ".join([f"{arg[0]}:{arg[1]}" for arg in taskData.args.get_arg("bof_arguments")])
                )
            else:
                raise Exception("Failed to get BOF file ID")
        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
