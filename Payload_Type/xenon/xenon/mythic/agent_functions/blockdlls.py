from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *


class BlockDllsArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="state",
                cli_name="State",
                display_name="State",
                type=ParameterType.ChooseOne,
                choices=["start", "stop"],
                description="Start or stop blocking of unsigned DLLs in spawned child processes.",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=True,
                        ui_position=1
                    )
                ]
            ),
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply 'start' or 'stop'")
        arg = self.command_line.strip().lower()
        if arg not in ("start", "stop"):
            raise ValueError("Argument must be 'start' or 'stop'")
        self.add_arg("state", arg)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


class BlockDllsCommand(CommandBase):
    cmd = "blockdlls"
    needs_admin = False
    help_cmd = "blockdlls <start|stop>"
    description = (
        "Enable or disable blocking of non-Microsoft (unsigned) DLLs from loading "
        "into child processes spawned by the agent. "
        "Uses PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON."
    )
    version = 1
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
        )
        response.DisplayParams = taskData.args.get_arg("state")
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
