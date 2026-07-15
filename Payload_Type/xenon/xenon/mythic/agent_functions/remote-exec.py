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


class RemoteExecArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="module",
                cli_name="Module",
                display_name="Module",
                type=ParameterType.ChooseOne,
                choices=[
                    "winrm",
                    "wmi",
                    "scshell"
                ],
                description="The module to execute on the remote machine.",
                default_value="winrm",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=1,
                    )
                ]
            ),
            CommandParameter(
                name="target",
                cli_name="Target",
                display_name="Target",
                type=ParameterType.ChooseOne,
                dynamic_query_function=self.get_hostnames,
                description="The target to execute the module on.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=2,
                    )
                ]
            ),
            CommandParameter(
                name="command",
                cli_name="Command",
                display_name="Command",
                type=ParameterType.String,
                description="The command to execute on the remote machine.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=3,
                    )
                ]
            ),
            CommandParameter(
                name="domain",
                cli_name="Domain",
                display_name="Domain",
                type=ParameterType.String,
                description="The domain to use for the remote machine.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Default",
                        ui_position=5,
                    )
                ]
            ),
            CommandParameter(
                name="username",
                cli_name="Username",
                display_name="Username",
                type=ParameterType.String,
                description="The username to use for the remote machine.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Default",
                        ui_position=6,
                    )
                ]
            ),
            CommandParameter(
                name="password",
                cli_name="Password",
                display_name="Password",
                type=ParameterType.String,
                description="The password to use for the remote machine.",
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Default",
                        ui_position=7,
                    )
                ]
            ),
        ]

    async def get_hostnames(
        self, callback: PTRPCDynamicQueryFunctionMessage
    ) -> PTRPCDynamicQueryFunctionMessageResponse:
        response = PTRPCDynamicQueryFunctionMessageResponse(Success=False)
        callback_response = await SendMythicRPCCallbackSearch(
            MythicRPCCallbackSearchMessage(CallbackID=callback.Callback)
        )
        if not callback_response.Success:
            response.Error = callback_response.Error
            return response

        response.Success = True
        response.Choices = sorted(
            {result.Host for result in callback_response.Results if result.Host},
            key=str.casefold,
        )
        return response

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise Exception(
                "Require a module, target, and command to execute.\n\tUsage: {}".format(
                    RemoteExecCommand.help_cmd
                )
            )
        if self.command_line[0] == "{":
            try:
                self.load_args_from_json_string(self.command_line)
                return
            except Exception as e:
                raise Exception(f"Error loading arguments from JSON string: {e}")
        else:
            try:
                parts = self.command_line.split(" ")
                self.add_arg("module", parts[0])
                self.add_arg("target", parts[1])
                self.add_arg("command", parts[2])
            except Exception as e:
                raise Exception(f"Error loading arguments from command line: {e}")
            

class RemoteExecCommand(CoffCommandBase):
    cmd = "remote_exec"
    needs_admin = False
    help_cmd = "remote_exec -Module [module] -Target [target] -Command [command + args] -Domain [domain] -Username [username] -Password [password]"
    description = "Execute a command on a remote machine using WMI, WinRM, or SCShell."
    version = 1
    author = "@c0rnbread"
    script_only = True
    attackmapping = ["T1047", "T1021.006"]
    argument_class = RemoteExecArguments
    attributes = CommandAttributes(
        dependencies=["inline_execute"],
        alias=True,
        suggested_command=False
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )

        try:

            module = taskData.args.get_arg("module")
            target = taskData.args.get_arg("target")
            command = taskData.args.get_arg("command")
            # Optional
            domain = taskData.args.get_arg("domain")
            username = taskData.args.get_arg("username")
            password = taskData.args.get_arg("password")

            # Set display parameters
            response.DisplayParams = "{} {} {} {} {} {}".format(
                taskData.args.get_arg("module"),
                taskData.args.get_arg("target"),
                taskData.args.get_arg("command"),
                taskData.args.get_arg("domain") if taskData.args.get_arg("domain") else "",
                taskData.args.get_arg("username") if taskData.args.get_arg("username") else "",
                taskData.args.get_arg("password") if taskData.args.get_arg("password") else "",
            )

            if module == "winrm":
                bof_file = "remote-exec-winrm.x64.o"
            elif module == "wmi":
                bof_file = "remote-exec-wmi.x64.o"
            elif module == "scshell":
                bof_file = "remote-exec-scshell.x64.o"
            else:
                raise Exception(f"Invalid module: {module}")

            # Upload desired BOF if it hasn't been before (per payload uuid)
            succeeded = await upload_module_if_missing(file_name=bof_file, taskData=taskData)
            if not succeeded:
                response.Success = False
                response.Error = f"Failed to upload or check module \"{bof_file}\"."
                return response

            # Setup arguments
            arguments = [
                [
                    "wchar", 
                    target
                ],
                [
                    "wchar", 
                    command
                ],
                [
                    "wchar", 
                    domain
                ],
                [
                    "wchar", 
                    username
                ],
                [
                    "wchar", 
                    password
                ]
            ]
            
            # Run inline_execute subtask
            subtask = await SendMythicRPCTaskCreateSubtask(
                MythicRPCTaskCreateSubtaskMessage(
                    taskData.Task.ID,
                    CommandName="inline_execute",
                    SubtaskCallbackFunction="coff_completion_callback",
                    Params=json.dumps({
                        "bof_name": bof_file,
                        "bof_arguments": arguments
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