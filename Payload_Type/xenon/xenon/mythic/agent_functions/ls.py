from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import re
import string, json

import logging

logging.basicConfig(level=logging.INFO)

class LsArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="filepath", 
                type=ParameterType.String, 
                description="Path of file or folder on the current system to list",
                parameter_group_info=[ParameterGroupInfo(
                    required=False
                )]
            ),
        ]

    async def parse_arguments(self):
        logging.info("Parse Aguments")
        pass

    async def parse_dictionary(self, dictionary):
        logging.info("Parse Dictionary")
        if "host" in dictionary:
            # Tasking from File Browser UI: use full_path if present, else path + "\\" + file (handle empty file for root)
            logging.info(f"Command came from File Browser UI - {dictionary}")
            self.add_arg("file_browser", type=ParameterType.Boolean, value=True)
            full_path = dictionary.get("full_path")
            self.add_arg("filepath", full_path)
            logging.info(f"Host: {dictionary.get('host', '')}")
            self.add_arg("host", dictionary.get("host", ""))
            
        else:
            # Arguments came from command line
            logging.info(f"Command came from CMDLINE - {dictionary}")
            self.add_arg("file_browser", type=ParameterType.Boolean, value=False)
            arg_path = dictionary.get("filepath")
            if arg_path:
                self.add_arg("filepath", arg_path)
            else:
                self.add_arg("filepath", "")
            self.add_arg("host", dictionary.get("host", ""))
        
        self.load_args_from_dictionary(dictionary)

class LsCommand(CommandBase):
    cmd = "ls"
    needs_admin = False
    help_cmd = "ls [directory]"
    description = "List directory information for <directory>"
    version = 1
    supported_ui_features = ["file_browser:list"]
    author = "@c0rnbread"
    attackmapping = ["T1106", "T1083"]
    argument_class = LsArguments
    browser_script = BrowserScript(
        script_name="ls_new", author="@c0rnbread", for_new_ui=True
    )
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[ SupportedOS.Windows ],
        suggested_command=True
    )

    # async def create_tasking(self, task: MythicTask) -> MythicTask:
    #     task.display_params = task.args.get_arg("command")
    #     return task
    
    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        
        is_file_browser = taskData.args.get_arg("file_browser")
        path = taskData.args.get_arg("filepath")

        # Fix drive letter
        if path.endswith(":"):
            taskData.args.set_arg("filepath", path + "\\")

        # Fix current directory
        if path == ".":
            taskData.args.set_arg("filepath", "")


        # UNC Path
        if uncmatch := re.match(r"^\\\\(?P<host>[^\\]+)\\(?P<path>.*)$", path):
            taskData.args.set_arg("host", uncmatch.group("host"))
            path = uncmatch.group("path")
            taskData.args.set_arg("filepath", path)

        # Uppercase the host
        if host := taskData.args.get_arg("host"):
            host = host.upper()
            # Dont include hostname if its current one
            if host == taskData.Callback.Host:
                host = ""

            taskData.args.set_arg("host", host)

        logging.info(f"Arguments: {taskData.args}")
        
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp