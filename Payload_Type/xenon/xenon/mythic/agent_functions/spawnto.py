from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *


class SpawntoArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="path", 
                type=ParameterType.String, 
                description="Path of the Windows process to spawn."
            ),
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a path to run")
        self.add_arg("path", self.command_line)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)

class SpawntoCommand(CommandBase):
    cmd = "spawnto"
    needs_admin = False
    help_cmd = "spawnto svchost.exe"
    description = "Change the spawnto process for spawn & inject commands. Program must be in C:\\Windows\\System32 or C:\\Windows\\SysWOW64 (e.g., svchost.exe)"
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = SpawntoArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[ SupportedOS.Windows ],
        suggested_command=False
    )

    # async def create_tasking(self, task: MythicTask) -> MythicTask:
    #     task.display_params = task.args.get_arg("command")
    #     return task
    
    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp