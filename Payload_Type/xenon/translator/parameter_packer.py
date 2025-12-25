"""
Parameter Packing for Xenon Agent Tasks

This module handles packing task parameters into the binary format expected by the agent.
Parameters are packed as: UINT32 count + (UINT32 size + data) for each parameter.
"""

import json
import base64
import logging
from typing import Dict, Any, Union

from .tlv_packer import TlvPacker
from .utils import Packer

logger = logging.getLogger(__name__)


def pack_parameter_value(packer: TlvPacker, param_name: str, param_value: Any) -> None:
    """
    Pack a single parameter value based on its type.
    
    Args:
        packer: TlvPacker instance to add data to
        param_name: Name of the parameter (for error messages)
        param_value: Value to pack (str, int, bool, bytes, list)
    
    Raises:
        TypeError: If parameter type is unsupported
    """
    # String parameters
    if isinstance(param_value, str):
        # Special handling for base64-encoded chunk data
        if param_name == "chunk_data":
            try:
                decoded = base64.b64decode(param_value)
                packer.add_bytes(decoded, include_length=True)
            except Exception as e:
                raise ValueError(f"Invalid base64 chunk_data: {e}")
        else:
            packer.add_string(param_value, include_length=True)
    
    # Boolean parameters (1 byte: 0x00 or 0x01)
    elif isinstance(param_value, bool):
        packer.add_bool(param_value)
    
    # Integer parameters (4 bytes, big-endian)
    elif isinstance(param_value, int):
        packer.add_uint32(param_value)
    
    # Raw bytes
    elif isinstance(param_value, bytes):
        packer.add_bytes(param_value, include_length=True)
    
    # List parameters (for inline_execute and similar commands)
    elif isinstance(param_value, list):
        pack_typed_list(packer, param_value)
    
    else:
        raise TypeError(f"Unsupported parameter type for '{param_name}': {type(param_value)}")


def pack_typed_list(packer: TlvPacker, param_list: list) -> None:
    """
    Pack a typed list parameter (used for inline_execute arguments).
    
    The list contains tuples of (type, value):
    - ("int16", value) -> INT16
    - ("int32", value) -> UINT32
    - ("bytes", hex_string) -> bytes from hex
    - ("string", value) -> UTF-8 string
    - ("wchar", value) -> UTF-16BE string
    - ("base64", value) -> base64 decoded bytes
    
    Args:
        packer: TlvPacker instance
        param_list: List of (type, value) tuples
    """
    if not param_list:
        # Empty list: just send 4 zero bytes
        packer.add_uint32(0)
        return
    
    # Use the Packer class for typed list serialization
    # This maintains compatibility with existing inline_execute format
    typed_packer = Packer()
    
    for item in param_list:
        if not isinstance(item, (list, tuple)) or len(item) != 2:
            raise ValueError(f"Invalid list item format, expected (type, value): {item}")
        
        item_type, item_value = item
        
        if item_type == "int16":
            typed_packer.addshort(int(item_value))
        elif item_type == "int32":
            typed_packer.adduint32(int(item_value))
        elif item_type == "bytes":
            # Convert hex string to bytes
            typed_packer.addbytes(bytes.fromhex(item_value))
        elif item_type == "string":
            typed_packer.addstr(item_value)
        elif item_type == "wchar":
            typed_packer.addWstr(item_value)
        elif item_type == "base64":
            try:
                decoded = base64.b64decode(item_value)
                typed_packer.addstr(decoded)
            except Exception as e:
                raise ValueError(f"Invalid base64 string: {item_value} - {e}")
        else:
            raise ValueError(f"Unknown typed list item type: {item_type}")
    
    # Get the packed buffer (includes length prefix from Packer.getbuffer())
    packed_data = typed_packer.getbuffer()
    
    # Add the total size of the packed data, then the data itself
    packer.add_uint32(len(packed_data))
    packer.add_raw(packed_data)


def pack_parameters(parameters: Dict[str, Any]) -> bytes:
    """
    Pack a dictionary of parameters into binary format.
    
    Format:
        UINT32: parameter_count
        For each parameter:
            UINT32: size
            BYTES: data
    
    Args:
        parameters: Dictionary of parameter name -> value
    
    Returns:
        bytes: Packed parameter data
    """
    packer = TlvPacker()
    
    # Add parameter count
    packer.add_uint32(len(parameters))
    
    # Pack each parameter
    for param_name, param_value in parameters.items():
        pack_parameter_value(packer, param_name, param_value)
    
    return packer.get_buffer()


def unpack_parameters(data: bytes) -> Dict[str, Any]:
    """
    Unpack parameters from binary format (for testing/debugging).
    
    Note: This is not currently used by the agent, but useful for validation.
    """
    if len(data) < 4:
        raise ValueError("Insufficient data for parameter count")
    
    param_count = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]
    
    params = {}
    
    for i in range(param_count):
        if len(data) < 4:
            raise ValueError(f"Insufficient data for parameter {i} size")
        
        param_size = int.from_bytes(data[0:4], byteorder='big')
        data = data[4:]
        
        if len(data) < param_size:
            raise ValueError(f"Insufficient data for parameter {i} value")
        
        param_value = data[:param_size]
        data = data[param_size:]
        
        # Try to decode as string, otherwise keep as bytes
        try:
            params[f"param_{i}"] = param_value.decode('utf-8')
        except UnicodeDecodeError:
            params[f"param_{i}"] = param_value
    
    return params

