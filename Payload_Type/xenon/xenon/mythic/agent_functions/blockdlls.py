from mythic_container.MythicCommandBase import *


class BlockDllsArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = []

    async def parse_arguments(self):
        if not self.command_line:
            raise Exception("Must supply 'start' or 'stop' as an argument")
        arg = self.command_line.strip().lower()
        if arg not in ["start", "stop"]:
            raise Exception("Must supply 'start' or 'stop' as an argument")
        self.add_arg("state", arg)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


class BlockDllsCommand(CommandBase):
    cmd = "blockdlls"
    needs_admin = False
    help_cmd = "blockdlls [start|stop]"
    description = "Enable or disable blocking of non-Microsoft (unsigned) DLLs from loading into child processes spawned by the agent"
    version = 1
    is_exit = False
    supported_ui_features = ["blockdlls"]
    author = "@Lavender-exe"
    attackmapping = ["T1562"]
    argument_class = BlockDllsArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[SupportedOS.Windows],
        suggested_command=False,
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
            DisplayParams=f"{taskData.args.get_arg('state')}",
        )
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
