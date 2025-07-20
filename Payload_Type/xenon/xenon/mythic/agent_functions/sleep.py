from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging

logging.basicConfig(level=logging.INFO)


class SleepArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="seconds", 
                type=ParameterType.Number, 
                description="Sleep time in seconds.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=1
                )]
            ),
            CommandParameter(
                name="jitter", 
                type=ParameterType.Number, 
                description="Jitter percentage in seconds.",
                parameter_group_info=[ParameterGroupInfo(
                    required=False,
                    ui_position=2
                )]
            ),
        ]

    async def parse_arguments(self):
        logging.info(f"parse_arguments : {self.command_line}")
        
        if len(self.command_line) == 0:
            raise ValueError("Must supply a command to run")
        self.add_arg("command", self.command_line)
    
    async def parse_dictionary(self, dictionary_arguments):
        logging.info(f"parse_dictionary : {dictionary_arguments}")
        
        seconds = dictionary_arguments.get("seconds")
        jitter = dictionary_arguments.get("jitter", 0)  # Default jitter to 0 if not in the dictionary

        if seconds is None:
            raise ValueError("The 'seconds' key is required in the dictionary.")
        if not isinstance(seconds, int) or seconds < 0:
            raise ValueError("The 'seconds' value must be a non-negative integer.")

        # Explicitly map parsed dictionary values to parameters
        self.add_arg("seconds", seconds)
        self.add_arg("jitter", jitter)
        

class SleepCommand(CommandBase):
    cmd = "sleep"
    needs_admin = False
    help_cmd = "sleep <seconds> [jitter]"
    description = "Change sleep timer and jitter."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = SleepArguments
    attributes = CommandAttributes(
        builtin=True,
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
        
        # Display parameters
        response.DisplayParams = "{} {}".format(
            taskData.args.get_arg('seconds'),
            taskData.args.get_arg('jitter')
        )
        
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp