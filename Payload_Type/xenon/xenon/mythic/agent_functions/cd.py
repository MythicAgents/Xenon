from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *


class CdArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="filepath", 
                cli_name="Path",
                type=ParameterType.String, 
                description="Path to change working directory.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True
                )]
            ),
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a path to change directory")
        self.add_arg("filepath", self.command_line)

    async def parse_dictionary(self, dictionary):
        self.load_args_from_dictionary(dictionary)

class CdCommand(CommandBase):
    cmd = "cd"
    needs_admin = False
    help_cmd = "cd C:\\path\\to\\directory"
    description = "Change working directory."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = CdArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[ SupportedOS.Windows ],
        suggested_command=True
    )

    # async def create_tasking(self, task: MythicTask) -> MythicTask:
    #     task.display_params = task.args.get_arg("command")
    #     return task
    
    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        
        # Set display parameters
        response.DisplayParams = taskData.args.get_arg("filepath")

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp