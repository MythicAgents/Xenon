from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging

class UnlinkArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="payload_id", 
                type=ParameterType.String, 
                description="Payload UUID for the P2P Agent."
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a command to run")
        self.add_arg("command", self.command_line)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)

class UnlinkCommand(CommandBase):
    cmd = "unlink"
    needs_admin = False
    help_cmd = "unlink [uuid]"      
    description = "Disconnect from an SMB Link Agent."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = UnlinkArguments
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
        
        
        # Set display parameters
        response.DisplayParams = "{}".format(taskData.args.get_arg("payload_id"))
        
        logging.info(f"Arguments: {taskData.args}")
        
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp