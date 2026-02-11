from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging

logging.basicConfig(level=logging.INFO)


class MakeTokenArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="domain", 
                cli_name="Domain",
                type=ParameterType.String,
                description="Domain of the account credentials. e.g., acme.corp",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=1
                )]
            ),
            CommandParameter(
                name="username", 
                cli_name="Username",
                type=ParameterType.String, 
                description="Username of the account.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=2
                )]
            ),
            CommandParameter(
                name="password", 
                cli_name="Password",
                type=ParameterType.String, 
                description="The plaintext password for the account.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=3
                )]
            ),
            CommandParameter(
                name="logon_type", 
                cli_name="LogonType",
                type=ParameterType.Number, 
                default_value=9,    # LOGON32_LOGON_NEW_CREDENTIALS 
                description="The type of logon operation to perform. (optional) default=LOGON32_LOGON_NEW_CREDENTIALS",
                parameter_group_info=[ParameterGroupInfo(
                    required=False,
                    ui_position=4
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

class MakeTokenCommand(CommandBase):
    cmd = "make_token"
    needs_admin = False
    help_cmd = "make_token <DOMAIN> <username> <password> [LOGON_TYPE]"
    description = "Create a token and impersonate it using plaintext credentials."
    version = 1
    author = "@c0rnbread"
    attackmapping = ["T1134.003"]
    argument_class = MakeTokenArguments
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
        response.DisplayParams = "{} {}".format(
            taskData.args.get_arg("domain") + "\\" + taskData.args.get_arg("username"),
            taskData.args.get_arg("password")
        )

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp