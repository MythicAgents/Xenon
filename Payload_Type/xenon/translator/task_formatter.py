"""
Task Formatting for Xenon Agent

This module handles converting Mythic tasks, responses, and delegates into
the binary task format expected by the agent.
"""

import json
import logging
from typing import Dict, Any, Optional, List

from .tlv_packer import TlvPacker
from .parameter_packer import pack_parameters
from .utils import get_operator_command, MYTHIC_GET_TASKING

logger = logging.getLogger(__name__)


def format_normal_task(task: Dict[str, Any]) -> bytes:
    """
    Format a normal Mythic task into binary format.
    
    Format:
        UINT32: total_size (includes command_id + uuid + params)
        BYTE: command_id
        BYTES[36]: task_uuid (no null terminator)
        UINT32: parameter_count
        BYTES: parameters (packed)
    
    Args:
        task: Task dictionary with "command", "id", and optional "parameters"
    
    Returns:
        bytes: Packed task data
    
    Raises:
        ValueError: If command is unknown or UUID is invalid
        KeyError: If required task fields are missing
    """
    try:
        command_name = task["command"]
        task_uuid = task["id"]
    except KeyError as e:
        raise KeyError(f"Missing required task field: {e}")
    
    parameters_json = task.get("parameters", "")
    
    # Get command byte code
    command_id = get_operator_command(command_name)
    if command_id is None:
        raise ValueError(f"Unknown command: {command_name}")
    
    # Parse parameters if provided
    if parameters_json:
        try:
            parameters = json.loads(parameters_json)
            if not isinstance(parameters, dict):
                raise ValueError("Parameters must be a JSON object")
        except json.JSONDecodeError as e:
            logger.warning(f"Failed to parse parameters JSON: {e}")
            parameters = {}
    else:
        parameters = {}
    
    # Build task data
    packer = TlvPacker()
    
    # Command ID (1 byte)
    packer.add_byte(command_id)
    
    # Task UUID (36 bytes, no null terminator)
    if len(task_uuid) != 36:
        raise ValueError(f"Task UUID must be exactly 36 characters, got {len(task_uuid)}")
    packer.add_raw(task_uuid.encode('utf-8'))
    
    # Parameters
    if parameters:
        try:
            param_data = pack_parameters(parameters)
            packer.add_raw(param_data)
        except Exception as e:
            logger.error(f"Failed to pack parameters: {e}")
            raise ValueError(f"Parameter packing failed: {e}") from e
    else:
        # Zero parameters
        packer.add_uint32(0)
    
    # Get the task body
    task_body = packer.get_buffer()
    
    # Prepend total size
    size_packer = TlvPacker()
    size_packer.add_uint32(len(task_body))
    
    return size_packer.get_buffer() + task_body


def format_task_response_as_task(response: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """
    Convert a Mythic task response (download/upload) into a task format.
    
    This allows the agent to handle responses as tasks, which is useful for
    download chunks, upload confirmations, etc.
    
    Args:
        response: Response dictionary with task_id, file_id, chunk_data, etc.
    
    Returns:
        Task dictionary or None if not applicable
    """
    task_id = response.get("task_id")
    file_id = response.get("file_id")
    total_chunks = response.get("total_chunks")
    chunk_num = response.get("chunk_num")
    chunk_data = response.get("chunk_data")
    
    params = {}
    
    # Upload response
    if file_id and chunk_data:
        response_type = "upload_resp"
        if total_chunks is not None:
            params["total_chunks"] = total_chunks
        if chunk_num is not None:
            params["chunk_num"] = chunk_num
        if chunk_data:
            params["chunk_data"] = chunk_data  # Will be decoded in pack_parameters
    
    # Download response
    elif file_id and not chunk_data:
        response_type = "download_resp"
        params["file_id"] = file_id
    
    # Normal response (no special handling needed)
    else:
        return None
    
    return {
        "command": response_type,
        "parameters": json.dumps(params),
        "id": task_id
    }


def format_delegate_as_task(delegate: Dict[str, Any]) -> Dict[str, Any]:
    """
    Convert a P2P delegate message into a task format.
    
    Args:
        delegate: Delegate dictionary with:
            - "uuid": P2P agent UUID
            - "new_uuid": New UUID (for checkin)
            - "message": Base64 encoded message
    
    Returns:
        Task dictionary for p2p_resp command
    """
    uuid = delegate.get('uuid')
    new_uuid = delegate.get('new_uuid')
    base64_msg = delegate.get('message')
    
    if not base64_msg:
        raise ValueError("Delegate message is required")
    
    # Determine if this is a checkin or regular tasking
    is_checkin = new_uuid is not None
    
    if is_checkin:
        # P2P Checkin: link_id is the random int32 from the agent
        try:
            link_id = int(uuid)  # UUID is actually a random int32 during checkin
        except (ValueError, TypeError):
            link_id = 0
        p2p_uuid = new_uuid
    else:
        # P2P Tasking: link_id is 0, use the UUID
        link_id = 0
        p2p_uuid = uuid
    
    params = {
        "is_checkin": is_checkin,
        "link_id": link_id,
        "p2p_uuid": p2p_uuid,
        "base64_msg": base64_msg,
    }
    
    return {
        "command": "p2p_resp",
        "parameters": json.dumps(params),
        "id": "00000000-0000-0000-0000-000000000000"  # Not a real task
    }


def format_get_tasking_message(
    tasks: List[Dict[str, Any]],
    responses: Optional[List[Dict[str, Any]]] = None,
    delegates: Optional[List[Dict[str, Any]]] = None
) -> bytes:
    """
    Format a complete get_tasking message with tasks, responses, and delegates.
    
    Format:
        BYTE: message_type (MYTHIC_GET_TASKING)
        UINT32: task_count
        BYTES: task_data (for each task)
    
    Args:
        tasks: List of normal tasks
        responses: List of task responses (converted to tasks)
        delegates: List of delegate messages (converted to tasks)
    
    Returns:
        bytes: Complete packed message
    
    Raises:
        ValueError: If task formatting fails
    """
    # Combine all task types
    all_tasks = list(tasks) if tasks else []
    
    # Convert responses to tasks
    if responses:
        for response in responses:
            try:
                task_response = format_task_response_as_task(response)
                if task_response is not None:
                    all_tasks.append(task_response)
            except Exception as e:
                logger.error(f"Failed to format response as task: {e}")
                continue
    
    # Convert delegates to tasks
    if delegates:
        for delegate in delegates:
            try:
                delegate_task = format_delegate_as_task(delegate)
                all_tasks.append(delegate_task)
            except Exception as e:
                logger.error(f"Failed to format delegate as task: {e}")
                continue
    
    # Build message
    packer = TlvPacker()
    
    # Message type
    packer.add_byte(MYTHIC_GET_TASKING)
    
    # Task count
    packer.add_uint32(len(all_tasks))
    
    # Pack each task
    for i, task in enumerate(all_tasks):
        try:
            task_data = format_normal_task(task)
            packer.add_raw(task_data)
        except Exception as e:
            logger.error(f"Failed to format task {i}: {e}")
            raise ValueError(f"Task formatting failed for task {i}: {e}") from e
    
    return packer.get_buffer()

