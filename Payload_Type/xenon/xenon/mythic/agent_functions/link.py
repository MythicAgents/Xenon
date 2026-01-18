from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging

class LinkArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="target",
                display_name="Target",
                type=ParameterType.String, 
                description="Host or IP address of pivot agent.",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=1
                    )
                ]
                
            ),
            CommandParameter(
                name="named_pipe",
                display_name="Named Pipe",
                type=ParameterType.String, 
                description="Named pipe to connect to.",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Default",
                        ui_position=2
                    )
                ]
            ),
            CommandParameter(
                name="tcp_port",
                display_name="TCP Port",
                type=ParameterType.Number, 
                description="TCP port to connect to.",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Default",
                        ui_position=3
                    )
                ]
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
    help_cmd = "link [target] [named pipe|tcp port]"      
    description = "Connect to an SMB/TCP Link Agent."
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

        named_pipe = taskData.args.get_arg('named_pipe')
        
        if ((named_pipe != "") and (named_pipe != None)):
            named_pipe_string = f"\\\\{taskData.args.get_arg('target')}\\pipe\\{taskData.args.get_arg('named_pipe')}"       # \\<hostname>\pipe\<string>
            
            taskData.args.add_arg("type", 1, ParameterType.Number)
            taskData.args.set_arg("pipe_name", named_pipe_string)
            

            # Set display parameters
            response.DisplayParams = "{} {}".format(
                taskData.args.get_arg("target"),
                taskData.args.get_arg("named_pipe")
            )

            # Don't send these
            taskData.args.remove_arg("target")
            taskData.args.remove_arg("named_pipe")
            taskData.args.remove_arg("tcp_port")
        else:
            target = taskData.args.get_arg("target")
            tcp_port = taskData.args.get_arg("tcp_port")
            
            taskData.args.remove_arg("target")
            taskData.args.remove_arg("named_pipe")
            taskData.args.remove_arg("tcp_port")
            
            taskData.args.add_arg("type", 2, ParameterType.Number)
            taskData.args.add_arg("target", target)
            taskData.args.add_arg("tcp_port", tcp_port, ParameterType.Number)
            
            # Set display parameters
            response.DisplayParams = "{} {}".format(
                taskData.args.get_arg("target"),
                taskData.args.get_arg("tcp_port")
            )

        logging.info(f"Arguments: {taskData.args}")
        
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
