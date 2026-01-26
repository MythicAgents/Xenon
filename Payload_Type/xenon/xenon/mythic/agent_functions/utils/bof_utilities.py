'''
Ref: https://github.com/MythicAgents/Athena/blob/main/Payload_Type/athena/athena/mythic/agent_functions/athena_utils/bof_utilities.py
'''
import struct
import subprocess
import os
from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging
from .mythicrpc_utilities import *


# This function merge the output of the subtasks and mark the parent task as completed.
async def default_coff_completion_callback(completionMsg: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    out = ""
    response = PTTaskCompletionFunctionMessageResponse(Success=True, TaskStatus="success", Completed=True)
    responses = await SendMythicRPCResponseSearch(MythicRPCResponseSearchMessage(TaskID=completionMsg.SubtaskData.Task.ID))
    for output in responses.Responses:
        out += str(output.Response)
            
    await SendMythicRPCResponseCreate(MythicRPCResponseCreateMessage(
        TaskID=completionMsg.TaskData.Task.ID,
        Response=f"{out}"
    ))
    return response

class CoffCommandBase(CommandBase):
    completion_functions = {"coff_completion_callback": default_coff_completion_callback}


async def upload_module_if_missing(file_name: str, taskData):
    """
    Upload a BOF to Mythic only if it doesn't already exist for the given task.
    """

    try:
        # Search for module by filename
        search_resp = await SendMythicRPCFileSearch(
            MythicRPCFileSearchMessage(
                TaskID=taskData.Task.ID,
                Filename=file_name,
                LimitByCallback=False,
                MaxResults=1,
            )
        )

        # File search failed
        if not search_resp.Success:
            logging.error(f"[Module Upload] File search failed for {file_name}: {search_resp.Error}")
            return False

        # Already uploaded to Mythic
        existing_names = {f.Filename for f in search_resp.Files}
        if file_name in existing_names:
            logging.info(f"[Module Upload] {file_name} already exists in Mythic, skipping upload.")
            return True

        # Path to module on disk
        module_path = (
            Path("xenon/agent_code/Modules/bin")
            / file_name
        )

        # Read and upload the module
        with open(module_path, "rb") as module_file:
            module_bytes = module_file.read()
            
        upload_resp = await SendMythicRPCFileCreate(
            MythicRPCFileCreateMessage(
                TaskID=taskData.Task.ID,
                Filename=file_name,
                DeleteAfterFetch=False,
                FileContents=module_bytes,
            )
        )

        if upload_resp.Success:
            logging.info(f"[Module Upload] Successfully uploaded: {file_name}")
            return True
        else:
            logging.error(f"[Module Upload] Failed to upload {file_name}: {upload_resp.Error}")
            return False

    except FileNotFoundError:
        logging.error(f"[Module Upload] File not found: {module_path}")
        return False
    except Exception as e:
        logging.exception(f"[Module Upload] Unexpected error while processing {file_name}: {str(e)}")
        return False