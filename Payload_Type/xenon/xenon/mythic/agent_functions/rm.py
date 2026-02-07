from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging


class RmArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="path", 
                type=ParameterType.String, 
                description="Path to file or directory",
                parameter_group_info=[ParameterGroupInfo(
                    required=True
                )]
            ),
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a path to a file")
        self.add_arg("path", self.command_line)

    async def parse_dictionary(self, dictionary):
        if "host" in dictionary:
            # Tasking from File Browser UI: use full_path if present, else path + "\\" + file
            logging.info(f"Command came from File Browser UI - {dictionary}")
            full_path = dictionary.get("full_path")
            if full_path is not None and full_path != "":
                self.add_arg("full_path", full_path)
            else:
                path = dictionary.get("path", "")
                file = dictionary.get("file", "")
                self.add_arg("full_path", (path + "\\" + file) if file else (path.rstrip("\\") or path))
        
        self.load_args_from_dictionary(dictionary)

class RmCommand(CommandBase):
    cmd = "rm"
    needs_admin = False
    help_cmd = "rm C:\\path\\to\\directoryOrFile"
    description = "Remove directory or file"
    version = 1
    supported_ui_features = ["file_browser:remove"]
    author = "@c0rnbread"
    attackmapping = []
    argument_class = RmArguments
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

        taskData.args.remove_arg("path")

        logging.info(f"Arguments: {taskData.args}")
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        # Report removed file to File Browser so UI shows strikethrough
        if response.get("status") == "success" and response.get("completed"):
            file = task.args.get_arg("full_path")
            if file:
                host = (task.Callback.Host if task.Callback else "") or ""
                resp.RemovedFiles = [{"host": host, "full_path": file}]
        return resp