from mythic_container.MythicCommandBase import *


class ExitArguments(TaskArguments):

    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="method",
                cli_name="Method",
                display_name="Exit method",
                type=ParameterType.ChooseOne,
                choices=["thread", "process"],
                default_value="thread",
                description="Exit method for Agent. e.g., thread or process.",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        group_name="Default",
                        ui_position=1,
                    )
                ],
            ),
        ]

    async def parse_arguments(self):
        line = self.command_line.strip() if self.command_line else ""
        if len(line) == 0:
            self.add_arg("method", "process")
            return
        if line.startswith("{"):
            self.load_args_from_json_string(self.command_line)
            return
        raise Exception("Exit command takes no shell parameters; use the UI or JSON.")

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


class ExitCommand(CommandBase):
    cmd = "exit"
    needs_admin = False
    help_cmd = "exit [-Scope thread|process]"
    description = "Task the implant to exit."
    version = 3
    supported_ui_features = ["callback_table:exit"]
    author = "@djhohnstein"
    argument_class = ExitArguments
    attributes = CommandAttributes(
        builtin=True,
        supported_os=[ SupportedOS.Windows ],
        suggested_command=True
    )
    attackmapping = []

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        scope = taskData.args.get_arg("method") or "process"
        method = 1 if scope == "process" else 0
        taskData.args.remove_arg("method")
        taskData.args.add_arg("method", method, ParameterType.Number)
        
        response.DisplayParams = "{}".format(scope)
        
        # logging.info(f"Arguments: {taskData.args}")
        
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp