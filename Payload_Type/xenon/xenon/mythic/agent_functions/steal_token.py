from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json


class StealTokenArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = []

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("steal_token requires a PID.")
        try:
            if self.command_line[0] == "{":
                supplied_dict = json.loads(self.command_line)
                if "pid" in supplied_dict:
                    self.add_arg("pid", int(supplied_dict["pid"]), type=ParameterType.Number)
                elif "process_id" in supplied_dict:
                    self.add_arg("pid", int(supplied_dict["process_id"]), type=ParameterType.Number)
                else:
                    self.load_args_from_json_string(self.command_line)
            else:
                self.add_arg("pid", int(self.command_line.strip()), type=ParameterType.Number)
        except Exception:
            raise ValueError(f"Invalid PID: {self.command_line}")

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


class StealTokenCommand(CommandBase):
    cmd = "steal_token"
    needs_admin = False
    help_cmd = "steal_token <pid>"
    description = "Steal and impersonate the token of a target process."
    version = 2
    author = "@c0rnbread"
    attackmapping = ["T1134", "T1528"]
    supported_ui_features = ["steal_token", "process_browser:steal_token"]
    argument_class = StealTokenArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[SupportedOS.Windows],
        suggested_command=False,
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        taskData.args.set_manual_args(f"{taskData.args.get_arg('pid')}")
        response.DisplayParams = f"{taskData.args.get_arg('pid')}"
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
