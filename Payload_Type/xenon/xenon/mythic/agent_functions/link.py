from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging

class LinkArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="named_pipe", 
                type=ParameterType.String, 
                description="Named pipe to connect to."
            ),
            CommandParameter(
                name="target", 
                type=ParameterType.String, 
                description="Host or IP address of pivot agent."
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a command to run")
        self.add_arg("command", self.command_line)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)

class LinkCommand(CommandBase):
    cmd = "link"
    needs_admin = False
    help_cmd = "link [target] [named pipe]"      
    description = "Connect to an SMB pivot Agent."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = LinkArguments
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
        
        named_pipe_string = f"\\\\{taskData.args.get_arg('target')}\\pipe\\{taskData.args.get_arg('named_pipe')}"       # \\<hostname>\pipe\<string>
        
        taskData.args.set_arg("pipe_name", named_pipe_string)
        
        # Don't send these
        taskData.args.remove_arg("target")
        taskData.args.remove_arg("named_pipe")
        
        
        logging.info(f"Arugments: {taskData.args}")
        
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp