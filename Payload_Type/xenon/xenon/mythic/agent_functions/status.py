from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *


class StatusArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = []

    async def parse_arguments(self):
        if len(self.command_line) > 0:
            raise ValueError("status command takes no parameters.")
        

class StatusCommand(CommandBase):
    cmd = "status"
    needs_admin = False
    help_cmd = "status"
    description = "List C2 connection hosts and their status. Hosts are marked dead after exceeding a MAX_FAILED threshold."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = StatusArguments
    attributes = CommandAttributes(
        builtin=True,
        supported_os=[ SupportedOS.Windows ],
        suggested_command=True
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp