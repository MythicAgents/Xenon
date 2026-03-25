from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging

logging.basicConfig(level=logging.INFO)


class StealTokenArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="pid",
                cli_name="Pid",
                type=ParameterType.Number, 
                description="Process ID",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=1
                )]
            )
        ]

    async def parse_arguments(self):
        logging.info(f"parse_arguments : {self.command_line}")
        if len(self.command_line) == 0:
            raise ValueError("Must supply a command to run")
        self.add_arg("command", self.command_line)
    
    async def parse_dictionary(self, dictionary_arguments):
        logging.info(f"parse_dictionary : {dictionary_arguments}")
        self.load_args_from_dictionary(dictionary_arguments)

class StealTokenCommand(CommandBase):
    cmd = "steal_token"
    needs_admin = False
    help_cmd = "steal_token <pid>"
    description = "Steal and impersonate the token of a target process."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = StealTokenArguments
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

        # Set display parameters
        response.DisplayParams = "{}".format(
            taskData.args.get_arg("pid")
        )

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp