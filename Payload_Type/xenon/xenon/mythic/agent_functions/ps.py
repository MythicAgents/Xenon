from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import json


class PsArguments(TaskArguments):

    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = []

    async def parse_arguments(self):
        if len(self.command_line) > 0:
            raise Exception("ps command takes no parameters.")


class PsCommand(CommandBase):
    cmd = "ps"
    needs_admin = False
    help_cmd = "ps"
    description = "List host processes."
    version = 1
    supported_ui_features = ["callback_table:ps"]
    author = "@c0rnbread"
    argument_class = PsArguments
    browser_script = BrowserScript(
        script_name="ps_new", author="@c0rnbread", for_new_ui=True
    )
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[ SupportedOS.Windows ],
        suggested_command=True
    )
    attackmapping = []

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        try:
            # Normalise bytes vs str
            if isinstance(response, bytes):
                text = response.decode("utf-8", errors="replace")
            else:
                text = str(response) if response is not None else ""

            host = task.Callback.Host if task.Callback else ""

            processes = []
            # Agent emits tab-separated lines: name\tppid\tpid[\tarch\tuser\tsession]
            for line in text.split("\n"):
                parts = line.rstrip("\r").split("\t")
                if len(parts) < 3 or not parts[2].strip().isdigit():
                    continue
                processes.append(MythicRPCProcessCreateData(
                    Host=host,
                    Name=parts[0],
                    ParentProcessID=int(parts[1]) if parts[1].strip().lstrip("-").isdigit() else 0,
                    ProcessID=int(parts[2]),
                    Architecture=parts[3] if len(parts) > 3 else "",
                    User=parts[4] if len(parts) > 4 else "",
                ))

            if processes:
                await SendMythicRPCProcessCreate(MythicRPCProcessesCreateMessage(
                    TaskID=task.Task.ID,
                    Processes=processes,
                ))
        except Exception as e:
            resp.Error = str(e)
            resp.Success = False
        return resp