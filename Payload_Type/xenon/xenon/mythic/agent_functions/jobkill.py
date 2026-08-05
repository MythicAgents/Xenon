from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json


class JobkillArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="task_uuid",
                cli_name="TaskUUID",
                display_name="Task UUID",
                type=ParameterType.String,
                description="Mythic agent task UUID of the async_execute job to stop.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=1
                )]
            )
        ]

    async def parse_arguments(self):
        if len(self.command_line.strip()) == 0:
            raise Exception("No task UUID given.\n\tUsage: {}".format(JobkillCommand.help_cmd))
        if self.command_line[0] == "{":
            # Mythic CLI sends {"TaskUUID": "..."} using cli_name keys
            self.load_args_from_json_string(self.command_line)
        else:
            self.add_arg("task_uuid", self.command_line.strip(), ParameterType.String)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


class JobkillCommand(CommandBase):
    cmd = "jobkill"
    needs_admin = False
    help_cmd = "jobkill [task_uuid]"
    description = "Stop a running async BOF by its Mythic agent task UUID (signals BeaconGetStopJobEvent)."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = JobkillArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[SupportedOS.Windows],
        suggested_command=False,
        dependencies=["async_execute"],
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        response.DisplayParams = "-TaskUUID {}".format(taskData.args.get_arg("task_uuid"))
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
