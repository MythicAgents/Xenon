"""
TLV (Type-Length-Value) Binary Packer for Xenon Agent

This module provides a unified interface for packing binary data in the format
expected by the Xenon C agent. All values are packed in big-endian (network byte order).

Binary Format Specification:
- UINT32: 4 bytes, big-endian
- UINT16: 2 bytes, big-endian  
- BYTE: 1 byte
- String: UINT32 length + data (no null terminator)
- Wide String: UINT32 length + UTF-16BE data (no null terminator)
- Boolean: 1 byte (0x00 = False, 0x01 = True)
- Bytes: UINT32 length + data
"""

import struct
from typing import Union, Optional


class TlvPacker:
    """
    Type-Length-Value packer for Xenon agent binary protocol.
    All integers are packed in big-endian format to match agent's BYTESWAP32/BYTESWAP64.
    """
    
    def __init__(self):
        self.buffer = bytearray()
    
    def add_byte(self, value: int) -> None:
        """Add a single byte (0-255)"""
        if not 0 <= value <= 255:
            raise ValueError(f"Byte value must be 0-255, got {value}")
        self.buffer.append(value)
    
    def add_uint32(self, value: int) -> None:
        """Add a 32-bit unsigned integer (big-endian)"""
        if value < 0 or value > 0xFFFFFFFF:
            raise ValueError(f"UINT32 value out of range: {value}")
        self.buffer.extend(struct.pack(">I", value))
    
    def add_uint16(self, value: int) -> None:
        """Add a 16-bit unsigned integer (big-endian)"""
        if value < 0 or value > 0xFFFF:
            raise ValueError(f"UINT16 value out of range: {value}")
        self.buffer.extend(struct.pack(">H", value))
    
    def add_int32(self, value: int) -> None:
        """Add a 32-bit signed integer (big-endian)"""
        if value < -0x80000000 or value > 0x7FFFFFFF:
            raise ValueError(f"INT32 value out of range: {value}")
        self.buffer.extend(struct.pack(">i", value))
    
    def add_bool(self, value: bool) -> None:
        """Add a boolean as 1 byte (0x00 = False, 0x01 = True)"""
        self.buffer.append(0x01 if value else 0x00)
    
    def add_string(self, value: str, include_length: bool = True) -> None:
        """
        Add a string with optional length prefix.
        
        Args:
            value: String to add
            include_length: If True, prepend UINT32 length (default: True)
        """
        if value is None:
            value = ""
        
        data = value.encode('utf-8')
        if include_length:
            self.add_uint32(len(data))
        self.buffer.extend(data)
    
    def add_wstring(self, value: str, include_length: bool = True) -> None:
        """
        Add a wide string (UTF-16BE) with optional length prefix.
        
        Args:
            value: String to add
            include_length: If True, prepend UINT32 length (default: True)
        """
        if value is None:
            value = ""
        
        # UTF-16BE encoding (big-endian)
        data = value.encode('utf-16-be')
        if include_length:
            # Length in bytes (not characters)
            self.add_uint32(len(data))
        self.buffer.extend(data)
    
    def add_bytes(self, value: bytes, include_length: bool = True) -> None:
        """
        Add raw bytes with optional length prefix.
        
        Args:
            value: Bytes to add
            include_length: If True, prepend UINT32 length (default: True)
        """
        if value is None:
            value = b""
        
        if include_length:
            self.add_uint32(len(value))
        self.buffer.extend(value)
    
    def add_raw(self, value: bytes) -> None:
        """Add raw bytes without length prefix"""
        self.buffer.extend(value)
    
    def get_buffer(self) -> bytes:
        """Get the packed buffer as bytes"""
        return bytes(self.buffer)
    
    def clear(self) -> None:
        """Clear the buffer"""
        self.buffer.clear()
    
    def __len__(self) -> int:
        """Return the current buffer length"""
        return len(self.buffer)


def pack_uint32(value: int) -> bytes:
    """Helper: Pack a single UINT32"""
    if value < 0 or value > 0xFFFFFFFF:
        raise ValueError(f"UINT32 value out of range: {value}")
    return struct.pack(">I", value)


def pack_uint16(value: int) -> bytes:
    """Helper: Pack a single UINT16"""
    if value < 0 or value > 0xFFFF:
        raise ValueError(f"UINT16 value out of range: {value}")
    return struct.pack(">H", value)


def pack_byte(value: int) -> bytes:
    """Helper: Pack a single byte"""
    if not 0 <= value <= 255:
        raise ValueError(f"Byte value must be 0-255, got {value}")
    return struct.pack("B", value)

