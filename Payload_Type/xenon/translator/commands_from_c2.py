"""
Commands from C2 Server to Agent

This module handles converting Mythic C2 messages into the binary format
expected by the Xenon agent.
"""

import logging
from .utils import MYTHIC_CHECK_IN
from .tlv_packer import TlvPacker
from .task_formatter import format_get_tasking_message

logger = logging.getLogger(__name__)


def checkin_to_agent_format(uuid: str) -> bytes:
    """
    Responds to Agent check-in request with new callback UUID.
    
    Format:
        BYTE: MYTHIC_CHECK_IN (0xA1)
        BYTES[36]: new_uuid
        BYTE: 0x01 (success indicator)
    
    Args:
        uuid: New UUID for agent (must be 36 characters)

    Returns:
        bytes: Packed check-in response
    """
    if len(uuid) != 36:
        raise ValueError(f"UUID must be 36 characters, got {len(uuid)}")
    
    packer = TlvPacker()
    packer.add_byte(MYTHIC_CHECK_IN)
    packer.add_raw(uuid.encode('utf-8'))
    packer.add_byte(0x01)  # Success indicator
    
    return packer.get_buffer()


def get_responses_to_agent_format(inputMsg) -> bytes:
    """
    Pack get_tasking message with tasks, responses, and delegates.
    
    This is the main entry point for converting Mythic get_tasking messages
    into the binary format expected by the agent.
    
    Args:
        inputMsg: Translation message with:
            - "tasks": List of normal tasks
            - "responses": List of task responses (download/upload)
            - "delegates": List of P2P delegate messages
    
    Returns:
        bytes: Packed binary data to be sent to agent
    """
    tasks = inputMsg.Message.get("tasks", [])
    responses = inputMsg.Message.get("responses", [])
    delegates = inputMsg.Message.get("delegates", [])
    
    return format_get_tasking_message(tasks, responses, delegates)
