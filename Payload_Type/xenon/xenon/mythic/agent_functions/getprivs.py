from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *


class GetPrivsArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = []

    async def parse_arguments(self):
        pass


class GetPrivsCommand(CommandBase):
    cmd = "getprivs"
    needs_admin = False
    help_cmd = "getprivs"
    description = (
        "Attempt to enable as many token privileges as possible on the current process token. "
        "Reports each privilege as enabled or not held. "
        "Key privileges targeted: SeDebugPrivilege, SeImpersonatePrivilege, SeShutdownPrivilege, "
        "SeChangeNotifyPrivilege, SeUndockPrivilege, and more."
    )
    version = 1
    author = "@Lavender-exe"
    attackmapping = ["T1134"]
    argument_class = GetPrivsArguments
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
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
