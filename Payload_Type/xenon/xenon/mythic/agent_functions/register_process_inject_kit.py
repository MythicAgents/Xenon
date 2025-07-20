from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging
from .utils.agent_global_settings import PROCESS_INJECT_KIT

logging.basicConfig(level=logging.INFO)

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
                parameter_group_info=[
                    ParameterGroupInfo(required=True, group_name="Existing", ui_position=1),
                    ParameterGroupInfo(required=True, group_name="New Kit", ui_position=1),
                ]
            ),
            CommandParameter(
                name="inject_spawn_file",
                cli_name="-inject_spawn",
                display_name="PROCESS_INJECT_SPAWN upload",
                type=ParameterType.File,
                description="Custom BOF file for fork & run injection",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="New Kit",
                        ui_position=2
                )]
            ),
            CommandParameter(
                name="inject_spawn_choose",
                cli_name="-inject_spawn",
                display_name="PROCESS_INJECT_SPAWN file",
                type=ParameterType.ChooseOne,
                dynamic_query_function=self.get_files,
                description="Already existing process injection kit to choose.",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Existing",
                        ui_position=2
                )]
            ),
            # TODO maybe
            # CommandParameter(
            #     name="inject_explicit",
            #     cli_name="-inject_explicit",
            #     display_name="PROCESS_INJECT_EXPLICIT",
            #     type=ParameterType.File,
            #     default_value=False,
            #     description="Custom BOF file for explicit injection",
            #     parameter_group_info=[ParameterGroupInfo(
            #         required=False
            #     )]
            # ),
        ]
        
    async def get_files(self, callback: PTRPCDynamicQueryFunctionMessage) -> PTRPCDynamicQueryFunctionMessageResponse:
        response = PTRPCDynamicQueryFunctionMessageResponse()
        file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
            CallbackID=callback.Callback,
            LimitByCallback=False,
            Filename="",
        ))
        if file_resp.Success:
            file_names = []
            for f in file_resp.Files:
                if f.Filename not in file_names and f.Filename.endswith(".o"):
                    file_names.append(f.Filename)
            response.Success = True
            response.Choices = file_names
            return response
        else:
            await SendMythicRPCOperationEventLogCreate(MythicRPCOperationEventLogCreateMessage(
                CallbackId=callback.Callback,
                Message=f"Failed to get files: {file_resp.Error}",
                MessageLevel="warning"
            ))
            response.Error = f"Failed to get files: {file_resp.Error}"
            return response

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a path to change directory")

    async def parse_dictionary(self, dictionary):
        self.load_args_from_dictionary(dictionary)

class RegisterProcessInjectKitCommand(CommandBase):
    cmd = "register_process_inject_kit"
    needs_admin = False
    help_cmd = "register_process_inject_kit (pops modal)"
    description = "Register a custom BOF to use for process injection (CS compatible). See documentation for requirements."
    version = 1
    author = "@c0rnbread"
    script_only = True
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
        
        is_enabled = taskData.args.get_arg("enabled")
        inject_spawn_file = taskData.args.get_arg("inject_spawn_file")
        inject_spawn_choice = taskData.args.get_arg("inject_spawn_choose")
        
        # logging.info(f"Got previous kit file: {inject_spawn_file}")
        # logging.info(f"Uploaded new kit file: {inject_spawn_choice}")
        
        groupName = taskData.args.get_parameter_group_name()
        
        
        if groupName == "New Kit":
            file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    AgentFileID=taskData.args.get_arg("inject_spawn_file")
                ))
            if file_resp.Success:
                if len(file_resp.Files) > 0:
                    kit_spawn_file_id = file_resp.Files[0].AgentFileId
                    logging.info(f"New kit uploaded with File ID : {kit_spawn_file_id}")
                else:
                    raise Exception("Failed to find that file")
            else:
                raise Exception("Error from Mythic trying to get file: " + str(file_resp.Error))
            
            # taskData.args.add_arg("inject_spawn_file", file_resp.Files[0].Filename)
            # taskData.args.add_arg("inject_spawn")
            # taskData.args.remove_arg("inject_spawn_file")
            
        elif groupName == "Existing":
            # We're trying to find an already existing file and use that
            file_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                TaskID=taskData.Task.ID,
                Filename=taskData.args.get_arg("inject_spawn_choose"),
                LimitByCallback=False,
                MaxResults=1
            ))
            if file_resp.Success:
                if len(file_resp.Files) > 0:
                    kit_spawn_file_id = file_resp.Files[0].AgentFileId
                    logging.info(f"Found existing Kit with File ID : {kit_spawn_file_id}")


                    # taskData.args.remove_arg("inject_spawn_choose")    # Don't need this anymore
                    # taskData.args.add_arg("inject_spawn_file", kit_spawn_file_id)
                    

                elif len(file_resp.Files) == 0:
                    raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
            else:
                raise Exception("Error from Mythic trying to search files:\n" + str(file_resp.Error))
        
        
        # inject_explicit = taskData.args.get_arg("inject_explicit")
        
        # Set the file UUID for the Kit
        if is_enabled:            
            PROCESS_INJECT_KIT.set_inject_spawn(kit_spawn_file_id)
            #PROCESS_INJECT_KIT.set_inject_explicit(kit_explicit_file_id)
        else:
            PROCESS_INJECT_KIT.set_inject_spawn("")         
            #PROCESS_INJECT_KIT.set_inject_explicit("")
        
        response.DisplayParams = "--enabled {} --inject_spawn {} ".format(
            "True" if is_enabled else "False",
            file_resp.Files[0].Filename if file_resp.Success else ""
            # inject_explicit_file.Files[0].Filename if inject_explicit else ""
        )
        
        # logging.info(taskData.args.to_json())
        
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp