# Binary Format Specification

This document describes the binary TLV (Type-Length-Value) format used for communication between the Mythic C2 server and the Xenon agent.

## Endianness

All multi-byte integers are in **big-endian** (network byte order) to match the agent's `BYTESWAP32`/`BYTESWAP64` functions.

## Basic Types

- **BYTE**: 1 byte (0x00 - 0xFF)
- **UINT16**: 2 bytes, big-endian
- **UINT32**: 4 bytes, big-endian
- **INT32**: 4 bytes, signed, big-endian
- **UINT64**: 8 bytes, big-endian
- **String**: UINT32 length + UTF-8 data (no null terminator)
- **WString**: UINT32 length + UTF-16BE data (no null terminator)
- **Boolean**: 1 byte (0x00 = False, 0x01 = True)
- **Bytes**: UINT32 length + raw data

## Message Types

### Check-in Response (C2 → Agent)

```
BYTE:    0xA1 (MYTHIC_CHECK_IN)
BYTES[36]: new_uuid (no null terminator)
BYTE:    0x01 (success indicator)
```

### Get Tasking (C2 → Agent)

```
BYTE:    0xA2 (MYTHIC_GET_TASKING)
UINT32:  task_count
...      task_data (for each task)
```

### Task Format

Each task is packed as:

```
UINT32:  total_size (includes command_id + uuid + params)
BYTE:    command_id
BYTES[36]: task_uuid (no null terminator)
UINT32:  parameter_count
...      parameters (packed)
```

### Parameter Format

Parameters are packed as:

```
UINT32:  parameter_count
For each parameter:
    UINT32:  size
    BYTES:   data
```

#### Parameter Types

- **String**: UINT32 length + UTF-8 bytes
- **Integer**: UINT32 value (4 bytes)
- **Boolean**: 1 byte (0x00 or 0x01)
- **Bytes**: UINT32 length + raw bytes
- **List** (for inline_execute): 
  - UINT32: total_size
  - Packed TLV data (uses Packer class format)

#### Special Parameter: chunk_data

When `param_name == "chunk_data"`, the value is expected to be a base64-encoded string that gets decoded before packing.

### Task Response (Agent → C2)

```
BYTE:    0xA4 (MYTHIC_TASK_RESPONSE)
BYTES[36]: task_uuid
UINT32:  output_size
BYTES:   output_data
BYTE:    status (0x95=complete, 0x97=update, 0x99=failed)
[UINT32: error_code] (if status == 0x99)
```

#### File Browser Listing (ls with file_browser=true)

When the task is `ls` with `file_browser=true`, the agent sends a dedicated message type instead of embedding in task response:

```
BYTE:    0x09 (MYTHIC_FILE_BROWSER)
BYTES[36]: task_uuid
BYTE:    status (0x95=complete, 0x97=update, 0x99=failed)
BYTES:   TLV payload (see below; no length prefix—rest of message is raw TLV)
```

TLV payload:

```
UINT32:  parent_path_len
BYTES:   parent_path (UTF-8)
UINT32:  name_len
BYTES:   name (folder/file name)
BYTE:    is_file (0=folder, 1=file)
UINT64:  size (bytes)
UINT64:  access_time (Windows FILETIME, 100-ns since 1601)
UINT64:  modify_time (Windows FILETIME)
BYTE:    success (0/1)
...      for each directory entry:
  UINT32:  name_len
  BYTES:   name
  BYTE:    is_file
  UINT64:  size
  UINT64:  access_time
  UINT64:  modify_time
```

The translator converts FILETIME to Unix milliseconds and builds Mythic's `file_browser` JSON via `file_browser_to_mythic_format()`.

### Download/Upload Messages

These are handled as special task types:
- `download_resp` (0xCC): Download response task
- `upload_resp` (0xCD): Upload response task

### P2P Delegate Messages

P2P messages are converted to `p2p_resp` tasks with parameters:
- `is_checkin`: Boolean
- `link_id`: UINT32 (random int32 during checkin, 0 otherwise)
- `p2p_uuid`: String (36 bytes)
- `base64_msg`: String (base64-encoded message)

### SOCKS Proxy Messages

SOCKS messages allow the agent to act as a TCP proxy for Mythic. The Mythic server handles the SOCKS5 protocol negotiation with clients; the agent simply tunnels raw TCP data.

#### SOCKS Data (C2 → Agent)

SOCKS messages from Mythic are converted to `socks_resp` (0xCE) tasks with parameters:
- `server_id`: UINT32 - Unique connection identifier
- `data`: Bytes - Base64-decoded data to forward (length-prefixed)
- `exit`: Boolean - Whether to close the connection after sending

For new connections, the first message's data contains:
```
BYTES[4]:  target_ip (network byte order)
BYTES[2]:  target_port (network byte order)
BYTES[...]: initial_data (optional)
```

Binary format as task parameters:
```
UINT32:  parameter_count (3)
UINT32:  server_id
UINT32:  data_length
BYTES:   data (decoded from base64)
BYTE:    exit (0x00=false, 0x01=true)
```

#### SOCKS Response (Agent → C2)

```
BYTE:    0x08 (MYTHIC_SOCKS_DATA)
UINT32:  server_id
UINT32:  data_length
BYTES:   data
BYTE:    exit_flag (0x00=false, 0x01=true)
```

The translator converts this to Mythic's JSON format:
```json
{
  "socks": [
    {
      "server_id": 12345,
      "data": "base64_encoded_data",
      "exit": false
    }
  ]
}
```

#### SOCKS Connection Lifecycle

1. **New Connection**: First message for a `server_id` contains target IP:port in data
2. **Data Transfer**: Subsequent messages forward raw TCP data
3. **Close Connection**: Message with `exit=true` signals connection closure
4. **Error Handling**: Socket errors trigger `exit=true` response to Mythic

## Agent Parsing

The agent uses the following parsing functions (from `Parser.c`):

- `ParserGetByte()`: Reads 1 byte
- `ParserGetInt32()`: Reads 4 bytes, swaps endianness
- `ParserGetInt64()`: Reads 8 bytes, swaps endianness
- `ParserGetBytes(size)`: Reads size bytes (if size=0, reads UINT32 size first)
- `ParserGetString(size)`: Same as `ParserGetBytes`, returns as char*

## Notes

1. **No Null Terminators**: Strings are length-prefixed and do not include null terminators
2. **UUID Format**: All UUIDs are exactly 36 bytes (standard UUID string format)
3. **Parameter Ordering**: Parameters are packed in dictionary iteration order (Python 3.7+)
4. **Error Handling**: Invalid data should be rejected with appropriate error codes

