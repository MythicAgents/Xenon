'''
Ref: https://github.com/MythicAgents/Athena/blob/main/Payload_Type/athena/athena/mythic/agent_functions/athena_utils/mythicrpc_utilities.py
'''
from mythic_container.MythicCommandBase import *  # import the basics
import json, base64, logging  # import any other code you might need
# import the code for interacting with Files on the Mythic server
from mythic_container.MythicRPC import *

async def get_mythic_file(file_id: str) -> str:
    file = await SendMythicRPCFileGetContent(MythicRPCFileGetContentMessage(AgentFileId=file_id))

    if not file.Success:
        raise Exception("Failed to get file contents: " + file.Error)
    
    return base64.b64encode(file.Content).decode("utf-8")
    
async def get_mythic_file_name(file_id: str) -> str:
    file_data = await SendMythicRPCFileSearch(MythicRPCFileSearchMessage(AgentFileId=file_id))

    if not file_data.Success:
        raise Exception("Failed to get file contents: " + file_data.Error)
    
    if len(file_data.Files) == 0:
        raise Exception(f"File with ID: {file_id} not found.")
    
    return file_data.Files[0].Filename

async def create_mythic_file(task_id: str, file_contents, file_name: str, delete_after_fetch: bool) -> MythicRPCFileCreateMessageResponse:
        fileCreate = MythicRPCFileCreateMessage(task_id, DeleteAfterFetch = delete_after_fetch, FileContents = file_contents, Filename = file_name)
        fileCreateRPC = await SendMythicRPCFileCreate(fileCreate)

        if not fileCreateRPC.Success:
            raise Exception("Failed to create file: " + fileCreateRPC.Error)

        return fileCreateRPC


def _task_succeeded(status: str) -> bool:
    s = (status or "").lower()
    return "error" not in s and "fail" not in s


def _decode_response_text(response) -> str:
    raw = getattr(response, "Response", "") or ""
    if isinstance(raw, bytes):
        return raw.decode("utf-8", errors="ignore")
    return str(raw)


def _responses_indicate_error(response_text: str) -> bool:
    """Xenon translator prefixes failures with [!] ERROR_NAME : description."""
    return "[!]" in (response_text or "")


def extract_agent_output_line(response_text: str) -> str:
    """Return the last real agent payload line, or empty if none / task failed.

    Failed Xenon tasks often have no payload, only:
      [+] agent called home, sent: N bytes
      [!] ERROR_ACCESS_DENIED : Access is denied.
    Those must not be treated as cwd or impersonation_context.
    """
    if not response_text or _responses_indicate_error(response_text):
        return ""
    marker = "[+] received output:"
    if marker not in response_text:
        return ""
    body = response_text.split(marker, 1)[1]
    lines = [ln.strip() for ln in body.splitlines() if ln.strip()]
    lines = [ln for ln in lines if not ln.startswith("[!]") and not ln.startswith("[+]")]
    return lines[-1] if lines else ""


async def _get_task_response_text(task_id) -> str:
    search = await SendMythicRPCResponseSearch(MythicRPCResponseSearchMessage(TaskID=task_id))
    if not search.Success or not search.Responses:
        return ""
    return "".join(_decode_response_text(r) for r in search.Responses)


async def get_last_agent_output_line(task_id) -> str:
    return extract_agent_output_line(await _get_task_response_text(task_id))


def _should_update_callback(status: str, response_text: str) -> bool:
    return _task_succeeded(status) and not _responses_indicate_error(response_text)


async def update_callback_context(task_id, cwd=None, impersonation_context=None):
    """Update Mythic callback cwd and/or impersonation_context tabs."""
    kwargs = {"TaskID": task_id}
    if cwd is not None:
        kwargs["Cwd"] = cwd
    if impersonation_context is not None:
        kwargs["ImpersonationContext"] = impersonation_context
    resp = await SendMythicRPCCallbackUpdate(MythicRPCCallbackUpdateMessage(**kwargs))
    if not resp.Success:
        logging.warning(f"SendMythicRPCCallbackUpdate failed: {resp.Error}")
    return resp


async def update_callback_cwd_on_success(completionMsg: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True)
    task_id = completionMsg.TaskData.Task.ID
    text = await _get_task_response_text(task_id)
    if not _should_update_callback(getattr(completionMsg.TaskData.Task, "Status", ""), text):
        return response
    cwd = extract_agent_output_line(text)
    if cwd:
        await update_callback_context(task_id, cwd=cwd)
    return response


async def update_callback_impersonation_on_success(completionMsg: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True)
    task_id = completionMsg.TaskData.Task.ID
    text = await _get_task_response_text(task_id)
    if not _should_update_callback(getattr(completionMsg.TaskData.Task, "Status", ""), text):
        return response
    identity = extract_agent_output_line(text)
    # IdentityGetUserInfo always formats DOMAIN\user
    if identity and "\\" in identity:
        await update_callback_context(task_id, impersonation_context=identity)
    return response


async def update_callback_impersonation_from_make_token(completionMsg: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    """Use the credentials passed to make_token, not LookupAccountSid output.

    LOGON32_LOGON_NEW_CREDENTIALS tokens still look like the original user
    to Win32 identity APIs, so agent output is the wrong source.
    """
    response = PTTaskCompletionFunctionMessageResponse(Success=True)
    task_id = completionMsg.TaskData.Task.ID
    text = await _get_task_response_text(task_id)
    if not _should_update_callback(getattr(completionMsg.TaskData.Task, "Status", ""), text):
        return response
    domain = completionMsg.TaskData.args.get_arg("domain") or ""
    user = completionMsg.TaskData.args.get_arg("username") or ""
    if "\\" in domain and not user:
        identity = domain
    elif domain or user:
        identity = f"{domain}\\{user}"
    else:
        return response
    await update_callback_context(task_id, impersonation_context=identity)
    return response


async def clear_callback_impersonation_on_success(completionMsg: PTTaskCompletionFunctionMessage) -> PTTaskCompletionFunctionMessageResponse:
    response = PTTaskCompletionFunctionMessageResponse(Success=True)
    task_id = completionMsg.TaskData.Task.ID
    text = await _get_task_response_text(task_id)
    if not _should_update_callback(getattr(completionMsg.TaskData.Task, "Status", ""), text):
        return response
    await update_callback_context(task_id, impersonation_context="")
    return response