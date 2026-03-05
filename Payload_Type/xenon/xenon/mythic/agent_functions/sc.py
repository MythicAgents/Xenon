from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json


class ScArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(name="query", cli_name="Query", display_name="Query", type=ParameterType.Boolean, default_value=False, parameter_group_info=[ParameterGroupInfo(required=True, group_name="Query")]),
            CommandParameter(name="start", cli_name="Start", display_name="Start", type=ParameterType.Boolean, default_value=False, parameter_group_info=[ParameterGroupInfo(required=True, group_name="Start")]),
            CommandParameter(name="stop", cli_name="Stop", display_name="Stop", type=ParameterType.Boolean, default_value=False, parameter_group_info=[ParameterGroupInfo(required=True, group_name="Stop")]),
            CommandParameter(name="create", cli_name="Create", display_name="Create", type=ParameterType.Boolean, default_value=False, parameter_group_info=[ParameterGroupInfo(required=True, group_name="Create")]),
            CommandParameter(name="delete", cli_name="Delete", display_name="Delete", type=ParameterType.Boolean, default_value=False, parameter_group_info=[ParameterGroupInfo(required=True, group_name="Delete")]),
            CommandParameter(
                name="computer",
                cli_name="Computer",
                display_name="Computer",
                type=ParameterType.String,
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, group_name="Query"),
                    ParameterGroupInfo(required=False, group_name="Start"),
                    ParameterGroupInfo(required=False, group_name="Stop"),
                    ParameterGroupInfo(required=False, group_name="Create"),
                    ParameterGroupInfo(required=False, group_name="Delete"),
                ],
            ),
            CommandParameter(
                name="service",
                cli_name="ServiceName",
                display_name="Service Name",
                type=ParameterType.String,
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, group_name="Query"),
                    ParameterGroupInfo(required=True, group_name="Start"),
                    ParameterGroupInfo(required=True, group_name="Stop"),
                    ParameterGroupInfo(required=True, group_name="Create"),
                    ParameterGroupInfo(required=True, group_name="Delete"),
                ],
            ),
            CommandParameter(
                name="display_name",
                cli_name="DisplayName",
                display_name="Display Name of Service",
                type=ParameterType.String,
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=False, group_name="Create"),
                ],
            ),
            CommandParameter(
                name="binpath",
                cli_name="BinPath",
                display_name="Binary Path",
                type=ParameterType.String,
                default_value="",
                parameter_group_info=[
                    ParameterGroupInfo(required=True, group_name="Create"),
                ],
            ),
        ]

    async def parse_arguments(self):
        if self.command_line[0] == "{":
            self.load_args_from_json_string(self.command_line)
        else:
            raise Exception("Require JSON.")


class ScCommand(CommandBase):
    cmd = "sc"
    needs_admin = False
    help_cmd = "sc"
    description = "Service control manager wrapper function (script-only)."
    version = 1
    author = "@Lavender-exe"
    argument_class = ScArguments
    attackmapping = ["T1569.002"]
    supported_ui_features = ["sc:start", "sc:stop", "sc:delete"]
    script_only = True

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(TaskID=taskData.Task.ID, Success=True)

        computer = taskData.args.get_arg("computer") or ""
        service = taskData.args.get_arg("service") or ""
        display_name = taskData.args.get_arg("display_name") or ""
        binpath = taskData.args.get_arg("binpath") or ""

        query = taskData.args.get_arg("query")
        start = taskData.args.get_arg("start")
        stop = taskData.args.get_arg("stop")
        create = taskData.args.get_arg("create")
        delete = taskData.args.get_arg("delete")

        if not any([query, start, stop, create, delete]):
            raise Exception("Failed to get a valid action to perform.")

        action = "query"
        response.DisplayParams = "-Query"
        if start:
            action = "start"
            response.DisplayParams = "-Start"
        if stop:
            action = "stop"
            response.DisplayParams = "-Stop"
        if create:
            action = "create"
            response.DisplayParams = "-Create"
        if delete:
            action = "delete"
            response.DisplayParams = "-Delete"

        if computer:
            response.DisplayParams += f" -Computer {computer}"
        if service:
            response.DisplayParams += f" -Service {service}"
        if display_name:
            response.DisplayParams += f" -DisplayName '{display_name}'"
        if binpath:
            response.DisplayParams += f" -BinPath '{binpath}'"

        remote_prefix = f"\\\\{computer} " if computer else ""

        if action == "query":
            shell_cmd = f"sc {remote_prefix}query"
            if service:
                shell_cmd += f" {service}"
        elif action == "start":
            shell_cmd = f"sc {remote_prefix}start {service}"
        elif action == "stop":
            shell_cmd = f"sc {remote_prefix}stop {service}"
        elif action == "delete":
            shell_cmd = f"sc {remote_prefix}delete {service}"
        else:
            # spacing around '=' is required by sc.exe parser
            disp = f' DisplayName= "{display_name}"' if display_name else ""
            shell_cmd = f'sc {remote_prefix}create {service} binPath= "{binpath}"{disp}'

        await SendMythicRPCTaskCreateSubtask(
            MythicRPCTaskCreateSubtaskMessage(
                TaskID=taskData.Task.ID,
                CommandName="shell",
                Params=json.dumps({"command": shell_cmd}),
            )
        )

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
