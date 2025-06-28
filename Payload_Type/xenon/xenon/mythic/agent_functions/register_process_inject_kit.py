from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging
from .utils.agent_global_settings import PROCESS_INJECT_KIT

class RegisterProcessInjectKitArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="enabled",
                cli_name="-enabled",
                display_name="Enable Process Inject Kit",
                type=ParameterType.Boolean,
                description="Enables using a custom BOF for process injection.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True
                )]
            ),
            CommandParameter(
                name="inject_spawn",
                cli_name="-inject_spawn",
                display_name="PROCESS_INJECT_SPAWN",
                type=ParameterType.File,
                default_value=False,
                description="Custom BOF file for fork & run injection",
                parameter_group_info=[ParameterGroupInfo(
                    required=False
                )]
            ),
            CommandParameter(
                name="inject_explicit",
                cli_name="-inject_explicit",
                display_name="PROCESS_INJECT_EXPLICIT",
                type=ParameterType.File,
                default_value=False,
                description="Custom BOF file for explicit injection",
                parameter_group_info=[ParameterGroupInfo(
                    required=False
                )]
            ),
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a path to change directory")

    async def parse_dictionary(self, dictionary):
        self.load_args_from_dictionary(dictionary)
        # TODO - Make sure they don't do Enabled with no files

class RegisterProcessInjectKitCommand(CommandBase):
    cmd = "register_process_inject_kit"
    needs_admin = False
    help_cmd = "register_process_inject_kit (pops modal)"
    description = "Register a custom BOF to use for process injection (CS compatible). See documentation for requirements."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = RegisterProcessInjectKitArguments
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
        
        # TODO - Create a second UI component to select already uploaded files that are named inject_spawn.x64.o OR inject_explicit.x64.o
        
        is_enabled = taskData.args.get_arg("enabled")
        inject_spawn = taskData.args.get_arg("inject_spawn")
        inject_explicit = taskData.args.get_arg("inject_explicit")

        if is_enabled:
            if inject_spawn:
                inject_spawn_file = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID, 
                    AgentFileID=taskData.args.get_arg("inject_spawn")
                ))
                if inject_spawn_file.Success:
                    if len(inject_spawn_file.Files) > 0:
                        PROCESS_INJECT_KIT.set_inject_spawn(inject_spawn_file.Files[0].AgentFileId)
                    else:
                        raise Exception("Failed to find that file")
                else:
                    raise Exception("Error from Mythic trying to get file: " + str(inject_spawn_file.Error))
            
            if inject_explicit:
                inject_explicit_file = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID, 
                    AgentFileID=taskData.args.get_arg("inject_explicit")
                ))
                if inject_explicit_file.Success:
                    if len(inject_explicit_file.Files) > 0:
                        PROCESS_INJECT_KIT.set_inject_explicit(inject_explicit_file.Files[0].AgentFileId)
                    else:
                        raise Exception("Failed to find that file")
                else:
                    raise Exception("Error from Mythic trying to get file: " + str(inject_explicit_file.Error))
        else:
            pass
        
        response.DisplayParams = "--enabled {} --inject_spawn {} --inject_explicit: {}".format(
            "True" if is_enabled else "False",
            inject_spawn_file.Files[0].Filename if inject_spawn else "",
            inject_explicit_file.Files[0].Filename if inject_explicit else ""
        )
        
        logging.info(taskData.args.to_json())
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp