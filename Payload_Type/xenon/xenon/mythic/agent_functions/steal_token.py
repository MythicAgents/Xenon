from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json
import logging

logging.basicConfig(level=logging.INFO)


def _pid_from_process_browser_dict(dictionary_arguments: dict):
    """Map Process Browser / modal / CLI JSON to a pid int, or None.

    Mythic CLI serializes using cli_name ("Pid"); process browser uses
    process_id; modal/internal often uses the parameter name ("pid").
    """
    for key in ("process_id", "pid", "Pid", "PID"):
        if key in dictionary_arguments and dictionary_arguments[key] is not None:
            return int(dictionary_arguments[key])
    return None


class StealTokenArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="pid",
                cli_name="Pid",
                display_name="PID",
                type=ParameterType.Number,
                description="Process ID",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=1
                )]
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line.strip()) == 0:
            raise ValueError("steal_token requires a PID.")
        if self.command_line[0] == "{":
            supplied = json.loads(self.command_line)
            pid = _pid_from_process_browser_dict(supplied)
            if pid is None:
                raise ValueError(f"No pid/process_id/Pid in JSON: {self.command_line}")
            self.add_arg("pid", pid, ParameterType.Number)
        else:
            try:
                self.add_arg("pid", int(self.command_line.strip()), ParameterType.Number)
            except ValueError:
                raise ValueError(f"Invalid PID: {self.command_line}")

    async def parse_dictionary(self, dictionary_arguments):
        pid = _pid_from_process_browser_dict(dictionary_arguments)
        if pid is None:
            raise ValueError(f"steal_token requires pid, process_id, or Pid: {dictionary_arguments}")
        self.add_arg("pid", pid, ParameterType.Number)


class StealTokenCommand(CommandBase):
    cmd = "steal_token"
    needs_admin = False
    help_cmd = "steal_token <pid>"
    description = "Steal and impersonate the token of a target process."
    version = 2
    author = "@c0rnbread"
    attackmapping = ["T1134"]
    argument_class = StealTokenArguments
    supported_ui_features = ["steal_token", "process_browser:steal_token"]
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
        response.DisplayParams = "{}".format(
            taskData.args.get_arg("pid")
        )
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
