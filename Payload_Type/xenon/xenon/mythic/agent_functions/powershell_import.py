from mythic_container.MythicCommandBase import *
import json
from .utils.mythicrpc_utilities import *
from .utils.agent_global_settings import *
import logging

logging.basicConfig(level=logging.INFO)

class PowerShellImportArguments(TaskArguments):

    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="script_name",
                cli_name="Script",
                display_name="Script",
                type=ParameterType.ChooseOne,
                dynamic_query_function=self.get_files,
                description="Already existing PowerShell script to import (e.g. ps1_Invoke-Mimikatz.ps1)",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False,
                        group_name="Default",
                        ui_position=1
                    )
                ]),
            CommandParameter(
                name="script_file",
                display_name="New Script",
                type=ParameterType.File,
                description="A new PowerShell script to import. After uploading once, you can just supply the -Script parameter",
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False, 
                        group_name="New Script", 
                        ui_position=1,
                    )
                ]
            ),
            CommandParameter(
                name="clear",
                display_name="Clear",
                cli_name="-clear",
                type=ParameterType.Boolean,
                description="Clear the PowerShell import cache.",
                default_value=False,
                parameter_group_info=[
                    ParameterGroupInfo(
                        required=False, 
                        group_name="Default", 
                        ui_position=2,
                    ),
                    ParameterGroupInfo(
                        required=False, 
                        group_name="New Script", 
                        ui_position=2,
                    ),
                ],
            ),
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
                if f.Filename not in file_names and f.Filename.startswith("ps1_"):
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
            raise Exception(
                "Requires an argument.\n\tUsage: {}".format(
                    PowerShellImportCommand.help_cmd
                )
            )
        if self.command_line[0] == "{":
            self.load_args_from_json_string(self.command_line)
        else:
            parts = self.command_line.split(" ", maxsplit=1)
            self.add_arg("script_name", parts[1])
            if len(parts) == 2:
                self.add_arg("script_file", parts[1])
            else:
                self.add_arg("script_file", "")

class PowerShellImportCommand(CommandBase):
    cmd = "powershell_import"
    needs_admin = False
    help_cmd = "powershell_import -File [script.ps1] | --clear"
    description = "Import PowerShell script to cache."
    version = 1
    script_only = True
    author = "@c0rnbread"
    argument_class = PowerShellImportArguments
    attackmapping = []
    attributes = CommandAttributes(
        dependencies=["powerchell"],
        alias=True,
        suggested_command=False
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )

        try:
            # Clear cache
            if taskData.args.get_arg("clear"):
                POWER_SHELL_IMPORT.set_file("")
                response.TaskStatus = MythicStatus.Success
                response.Completed = True
                await SendMythicRPCResponseCreate(MythicRPCResponseCreateMessage(
                    TaskID=taskData.Task.ID,
                    Response=f"[+] Cleared PowerShell import cache"
                ))

                # display parameters
                response.DisplayParams = "--clear"
                return response

            groupName = taskData.args.get_parameter_group_name()

            # New Script
            if groupName == "New Script":
                # Get file
                script_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                        TaskID=taskData.Task.ID,
                        AgentFileID=taskData.args.get_arg("script_file")
                    ))
                if script_resp.Success:
                    if len(script_resp.Files) > 0:
                        pass
                    else:
                        raise Exception("Failed to find that file")
                else:
                    raise Exception("Error from Mythic trying to get file: " + str(script_resp.Error))

                # Get script contents
                script_contents = await SendMythicRPCFileGetContent(MythicRPCFileGetContentMessage(AgentFileId=script_resp.Files[0].AgentFileId))
                if not script_contents.Success:
                    raise Exception("Failed to get script contents from Mythic (ID: {})".format(script_resp.Files[0].AgentFileId))
                    
                script_name = "ps1_" + script_resp.Files[0].Filename

                # Upload Named Script to Mythic
                script_resp = await SendMythicRPCFileCreate(
                    MythicRPCFileCreateMessage(
                        TaskID=taskData.Task.ID, 
                        FileContents=script_contents.Content, 
                        DeleteAfterFetch=False, 
                        Filename=script_name
                ))
                
                if script_resp.Success:
                    script_file_id = script_resp.AgentFileId
                else:
                    raise Exception("Failed to register PowerShell script: " + script_resp.Error)
                
            # Existing Script  
            elif groupName == "Default":
                script_name = taskData.args.get_arg("script_name")
                script_resp = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(
                    TaskID=taskData.Task.ID,
                    Filename=script_name,
                    LimitByCallback=False,
                    MaxResults=1
                ))
                if script_resp.Success:
                    if len(script_resp.Files) > 0:
                        script_file_id = script_resp.Files[0].AgentFileId
                        logging.info(f"Found existing Script with File ID : {script_resp.Files[0].AgentFileId}")
                    elif len(script_resp.Files) == 0:
                        raise Exception("Failed to find the named file. Have you uploaded it before? Did it get deleted?")
                else:
                    raise Exception("Error from Mythic trying to search files:\n" + str(script_resp.Error))
            
            ######################################

            # TODO - Delete old script if it exists
           
            # Set the file UUID for the PowerShell script
            POWER_SHELL_IMPORT.set_file(script_file_id)

            # Set display parameters
            response.DisplayParams = "{}".format(script_name)

            # Send response
            response.TaskStatus = MythicStatus.Success
            response.Completed = True
            await SendMythicRPCResponseCreate(MythicRPCResponseCreateMessage(
                        TaskID=taskData.Task.ID,
                        Response=f"[+] Imported script {script_name}"
                    ))

            return response

        except Exception as e:
            raise Exception("Error from Mythic: " + str(sys.exc_info()[-1].tb_lineno) + " : " + str(e))

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp