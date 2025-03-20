from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *

class CpArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="existing_file", 
                type=ParameterType.String, 
                description="Source path to copy file"
            ),
            CommandParameter(
                name="new_file", 
                type=ParameterType.String, 
                description="Destination path to copy file"
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a command to run")
        self.add_arg("command", self.command_line)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)

class CpCommand(CommandBase):
    cmd = "cp"
    needs_admin = False
    help_cmd = "cp C:\\source\\path C:\\destination\\path"      
    description = "Copy a file to new destination."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = CpArguments
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