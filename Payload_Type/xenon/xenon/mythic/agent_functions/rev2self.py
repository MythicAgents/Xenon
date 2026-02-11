from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *


class Rev2SelfArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = []

    async def parse_arguments(self):
        if len(self.command_line) > 0:
            raise ValueError("rev2self command takes no parameters.")
        

class Rev2SelfCommand(CommandBase):
    cmd = "rev2self"
    needs_admin = False
    help_cmd = "rev2self"
    description = "Revert identity to the original process's token."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = Rev2SelfArguments
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
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp