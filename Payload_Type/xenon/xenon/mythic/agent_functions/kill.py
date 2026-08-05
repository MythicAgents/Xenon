from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json


def _pid_from_process_browser_dict(dictionary_arguments: dict):
    """Map Process Browser / modal JSON to a pid int, or None."""
    if "process_id" in dictionary_arguments and dictionary_arguments["process_id"] is not None:
        return int(dictionary_arguments["process_id"])
    if "pid" in dictionary_arguments and dictionary_arguments["pid"] is not None:
        return int(dictionary_arguments["pid"])
    return None


class KillArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="pid",
                cli_name="PID",
                display_name="PID",
                type=ParameterType.Number,
                description="Process ID to terminate.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=1
                )]
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line.strip()) == 0:
            raise Exception("No PID given.\n\tUsage: {}".format(KillCommand.help_cmd))
        if self.command_line[0] == "{":
            supplied = json.loads(self.command_line)
            pid = _pid_from_process_browser_dict(supplied)
            if pid is None:
                raise Exception(f"No pid/process_id in JSON: {self.command_line}")
            self.add_arg("pid", pid, ParameterType.Number)
        else:
            try:
                self.add_arg("pid", int(self.command_line.strip()), ParameterType.Number)
            except ValueError:
                raise Exception(
                    "Failed to parse integer PID from: {}\n\tUsage: {}".format(
                        self.command_line, KillCommand.help_cmd
                    )
                )

    async def parse_dictionary(self, dictionary_arguments):
        pid = _pid_from_process_browser_dict(dictionary_arguments)
        if pid is None:
            raise Exception(f"kill requires pid or process_id: {dictionary_arguments}")
        self.add_arg("pid", pid, ParameterType.Number)


class KillCommand(CommandBase):
    cmd = "kill"
    needs_admin = False
    help_cmd = "kill [pid]"
    description = "Terminate a process by PID."
    version = 1
    author = "@c0rnbread"
    attackmapping = ["T1106"]
    argument_class = KillArguments
    supported_ui_features = ["kill", "process_browser:kill"]
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[ SupportedOS.Windows ],
        suggested_command=True
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        response.DisplayParams = "-PID {}".format(taskData.args.get_arg("pid"))
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
