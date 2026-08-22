from translator.utils import *
import ipaddress, logging
from .utils import parse_file_browser_tlv, parse_ps_tsv

logging.basicConfig(level=logging.INFO)


def checkin_to_mythic_format(data):
    """
    Parse check-in message from Agent and return JSON in Mythic format.
    """
    
    # First 36 bytes are agent UUID
    callback_uuid = data[:36]
    data = data[36:]
    
    # Retrieve IPs
    num_ips = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]
    i = 0
    IPs = []
    while i < num_ips:
        ip = data[:4]
        data = data[4:]
        addr = str(ipaddress.ip_address(ip))
        IPs.append(addr)
        i += 1
        
    # Retrieve OS
    target_os, data = get_bytes_with_size(data)
    
    # Retrive Architecture
    arch_os = data[0]
    if arch_os == 0x64:
        arch_os = "x64"
    elif arch_os == 0x86:
        arch_os = "x86"
    else:
        arch_os = ""
    data = data[1:]
    
    # Retrieve HostName
    hostname, data = get_bytes_with_size(data)

    # Retrieve Username
    username, data = get_bytes_with_size(data)

    # Retrieve Domaine
    domain, data = get_bytes_with_size(data)
    
    # Retrieve PID
    pid = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]

    # Retrieve Process Name
    process_name, data = get_bytes_with_size(data)

    #Retrieve External IP
    external_ip, data = get_bytes_with_size(data)

    # Integrity level (Mythic 0-4; >2 shows elevated in UI). Default to medium
    if len(data) >= 1:
        integrity_level = data[0]
        data = data[1:]
    else:
        # logging.warning("checkin missing integrity_level byte; defaulting to 2 (medium)")
        integrity_level = 2

    # Optional cwd (length-prefixed). Older implants omit this field.
    cwd = None
    if len(data) >= 4:
        cwd_bytes, data = get_bytes_with_size(data)
        cwd = cwd_bytes.decode('cp850', errors='ignore')

    # Mythic check-in format
    mythic_json = {
            "action": "checkin",
            "ips": IPs,
            "os": target_os.decode('cp850'),
            "user": username.decode('cp850'),
            "host": hostname.decode('cp850'),
            "domain": domain.decode('UTF-16LE'),
            "process_name":process_name.decode('cp850'),
            "pid": pid,
            "uuid": callback_uuid.decode('cp850'),
            "architecture": arch_os ,
            "external_ip": external_ip.decode('cp850'),
            "integrity_level": integrity_level,
        }
    if cwd:
        mythic_json["cwd"] = cwd
    
    return mythic_json



# Handle get_tasking from agent
'''
------------------------------------------
Key	            Key Len (bytes)	    Type
------------------------------------------
Number tasks	4	                Uint32
'''
# def get_tasking_to_mythic_format(data):
#     """
#     Process a Agent -> Mythic get_tasking message
#     """
#     numTasks = int.from_bytes(data[0:4], byteorder='big')
#     data = data[4:]
    
#     mythic_json = { 
#             "action": "get_tasking", 
#             "tasking_size": numTasks 
#         }
    
#     return mythic_json, data


def post_response_handler(data):
    """
    Process one or more Agent -> Mythic post_response messages
    """
    mythic_messages = []
    mythic_delegates = []
    mythic_edges = []
    mythic_socks = []
    mythic_rpfwd = []

    # Number of tasks to return to agent
    num_of_tasks = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]


    while len(data) > 0:
        if len(data) < 1:
            break

        response_type = data[0]
        data = data[1:]
    
        if response_type == MYTHIC_TASK_RESPONSE:
            result = post_response_to_mythic_format(data)
            if result is None:
                logging.error("post_response_to_mythic_format returned None")
                break
            task_json, data = result
            logging.info(f"[MYTHIC_TASK_RESPONSE]")
            
        elif response_type == MYTHIC_INIT_DOWNLOAD:
            task_json, data = download_init_to_mythic_format(data)
            logging.info(f"[MYTHIC_INIT_DOWNLOAD]")
            
        elif response_type == MYTHIC_CONT_DOWNLOAD:
            task_json, data = download_cont_to_mythic_format(data)
            logging.info(f"[MYTHIC_CONT_DOWNLOAD]")
            
        elif response_type == MYTHIC_UPLOAD_CHUNKED:
            task_json, data = upload_to_mythic_format(data)
            logging.info(f"[MYTHIC_UPLOAD_CHUNKED]")

        elif response_type == MYTHIC_P2P_CHECK_IN:
            task_json, delegates, data = p2p_checkin_to_mythic_format(data)
            logging.info(f"[MYTHIC_P2P_CHECK_IN]")
            if delegates:
                mythic_delegates.extend(delegates)

        elif response_type == MYTHIC_P2P_MSG:
            task_json, delegates, data = p2p_to_mythic_format(data)
            logging.info(f"[MYTHIC_P2P_MSG]")
            if delegates:
                mythic_delegates.extend(delegates)
        
        elif response_type == MYTHIC_P2P_REMOVE:
            task_json, edges, data = p2p_remove_to_mythic_format(data)
            logging.info(f"[MYTHIC_P2P_REMOVE]")
            if edges:
                mythic_edges.extend(edges)
        
        elif response_type == MYTHIC_SOCKS_DATA:
            task_json, socks_msg, data = socks_to_mythic_format(data)
            logging.info(f"[MYTHIC_SOCKS_DATA]")
            if socks_msg:
                mythic_socks.append(socks_msg)

        elif response_type == MYTHIC_RPORTFWD_DATA:
            task_json, rpfwd_msg, data = rportfwd_to_mythic_format(data)
            logging.info(f"[MYTHIC_RPORTFWD_DATA]")
            if rpfwd_msg:
                mythic_rpfwd.append(rpfwd_msg)

        elif response_type == MYTHIC_FILE_BROWSER:
            task_json, data = file_browser_to_mythic_format(data)
            logging.info(f"[MYTHIC_FILE_BROWSER]")

        elif response_type == MYTHIC_PROCESS_BROWSER:
            task_json, data = process_browser_to_mythic_format(data)
            logging.info(f"[MYTHIC_PROCESS_BROWSER]")
        else:
            logging.info(f"[UNKNOWN_RESPONSE]: {response_type}")
            continue

        # Normalize to list
        if task_json is not None:
            if isinstance(task_json, list):
                mythic_messages.extend(task_json)
            else:
                mythic_messages.append(task_json)

    mythic_json = {
        "action": "get_tasking",
        "tasking_size": num_of_tasks,
        "responses": mythic_messages,
    }
    
    if mythic_delegates:
        mythic_json["delegates"] = mythic_delegates
    
    if mythic_edges:
        mythic_json["edges"] = mythic_edges
    
    if mythic_socks:
        mythic_json["socks"] = mythic_socks

    if mythic_rpfwd:
        mythic_json["rpfwd"] = mythic_rpfwd

    return mythic_json


def post_response_to_mythic_format(data):
    """
    Process simple post response message -> Mythic format
    
    :param data: Raw data from Agent
    """
    original_len = len(data)

    # --- Task UUID ---
    if len(data) < 36:
        logging.error("Remaining buffer too small for task UUID")
        return None, data

    task_uuid = data[:36].decode("cp850")
    data = data[36:]
    

    # --- Output Buffer ---
    if len(data) < 4:
        logging.error("Remaining buffer too small for output buffer length")
        return None, data
    
    output, data = get_bytes_with_size(data)
    output_length = len(output)

    
    # --- Status Byte ---
    if len(data) < 1:
        logging.error("Missing status byte")
        # Return error response instead of None
        task_json = {
            "task_id": task_uuid,
            "user_output": "[!] Error: Missing status byte in response",
            "status": "error",
            "completed": True
        }
        return task_json, data
        
    status_byte = data[0]
    data = data[1:]

    if status_byte == 0x95:
        status = "success"          # Succeeded
    elif status_byte == 0x97:
        status = "running"     # Still processing
    elif status_byte == 0x99:
        status = "error"            # Failed
    else:
        status = "unknown"

    error_code = None

    # --- Optional Error Code ---
    if status == "error":
        if len(data) < 4:
            logging.info("Missing error code for error status")
            error_code = 0  # Default to 0 if missing
        else:
            error_code_bytes = data[:4]
            data = data[4:]
            error_code = int.from_bytes(error_code_bytes, byteorder="big")

    # --- Operator Output ---
    if status == "running":
        user_output = output.decode('cp850', errors='ignore')
    elif status == "success" or status == "error":
        msg1 = f"[+] agent called home, sent: {original_len} bytes\n"
        msg2 = f"[+] received output:\n\n{output.decode('cp850', errors='ignore')}" if output_length > 0 else ""
        user_output = msg1 + msg2

    if status == "error":
        error = ERROR_CODES.get(
            error_code,
            {
                "name": "UNKNOWN_ERROR",
                "description": f"Error code {error_code}",
            },
        )
        user_output += f"\n[!] {error['name']} : {error['description']}"

    task_json = {
            "task_id": task_uuid,
            "user_output": user_output,
            "status": status,
            "completed": status in ("success", "error")
        }

    return task_json, data


def file_browser_to_mythic_format(data):
    """
    Parse file-browser message from Agent (message type MYTHIC_FILE_BROWSER already consumed).
    Format: task_uuid (36), status_byte (1), then raw TLV payload (no length prefix).
    The agent builds one package: type, uuid, status, then TLV.
    Returns (task_json, remaining_data) for Mythic file_browser response.
    """
    if len(data) < 36 + 1:
        logging.error("file_browser_to_mythic_format: buffer too small for task_uuid + status")
        return None, data

    task_uuid = data[:36].decode("cp850")
    data = data[36:]
    status_byte = data[0]
    data = data[1:]

    if status_byte == 0x95:
        status = "success"
    elif status_byte == 0x97:
        status = None
    elif status_byte == 0x99:
        status = "error"
    else:
        status = "unknown"

    # Rest of buffer is the raw TLV (agent sends type, uuid, status, then TLV with no length prefix)
    tlv_payload = data
    data = b""

    file_browser_data = parse_file_browser_tlv(tlv_payload) if len(tlv_payload) > 0 else None
    
    if status == "success":
        user_output = "[+] file browser listing\n" if file_browser_data else "[+] file browser (parse error)\n"
    elif status == "error":
        user_output = "[!] file browser failed\n"
    else:
        user_output = "[+] file browser listing\n"

    task_json = {
        "task_id": task_uuid,
        "user_output": user_output,
        "status": status,
        "completed": status in ("success", "error"),
    }
    if file_browser_data is not None:
        task_json["file_browser"] = file_browser_data

    return task_json, data


def process_browser_to_mythic_format(data):
    """
    Parse process-browser message from Agent (message type MYTHIC_PROCESS_BROWSER already consumed).
    Format: task_uuid (36), status_byte (1), UINT32 host_len + host, UINT32 tsv_len + tsv.
    Returns (task_json, remaining_data) with Mythic v4 `processes` map for Process Browser:
      {"host", "os", "update_deleted", "processes": [...]}
    """
    if len(data) < 36 + 1:
        logging.error("process_browser_to_mythic_format: buffer too small for task_uuid + status")
        return None, data

    task_uuid = data[:36].decode("cp850")
    data = data[36:]
    status_byte = data[0]
    data = data[1:]

    if status_byte == 0x95:
        status = "success"
    elif status_byte == 0x97:
        status = None
    elif status_byte == 0x99:
        status = "error"
    else:
        status = "unknown"

    if len(data) < 4:
        logging.error("process_browser_to_mythic_format: missing host length prefix")
        return {
            "task_id": task_uuid,
            "user_output": "[!] process browser: truncated response\n",
            "status": "error",
            "completed": True,
        }, data

    host_bytes, data = get_bytes_with_size(data)
    host = host_bytes.decode("cp850", errors="ignore") if host_bytes else ""

    if len(data) < 4:
        logging.error("process_browser_to_mythic_format: missing TSV length prefix")
        return {
            "task_id": task_uuid,
            "user_output": "[!] process browser: truncated response\n",
            "status": "error",
            "completed": True,
        }, data

    tsv_bytes, data = get_bytes_with_size(data)
    processes = parse_ps_tsv(tsv_bytes) if tsv_bytes else []

    if status == "success":
        user_output = (
            f"[+] agent called home, sent: {len(tsv_bytes)} bytes\n"
            f"[+] received output:\n\n{tsv_bytes.decode('cp850', errors='ignore')}"
        )
    elif status == "error":
        user_output = "[!] process listing failed\n"
    else:
        user_output = "[+] process listing\n"

    task_json = {
        "task_id": task_uuid,
        "user_output": user_output,
        "status": status,
        "completed": status in ("success", "error"),
    }
    if processes:
        processes_payload = {
            "os": "windows",
            "update_deleted": True,
            "processes": processes,
        }
        host_value = (host or "").strip().upper()
        if host_value:
            processes_payload["host"] = host_value
        task_json["processes"] = processes_payload

    return task_json, data


def download_init_to_mythic_format(data):
    """
    Parse download initialize message from Agent and return JSON in Mythic v4 offset mode.
    {
        "action": "post_response",
        "responses": [
            {
                "task_id": "UUID here",
                "download": {
                    "total_size": 1048576,
                    "full_path": "/test/test2/test3.file",
                    "is_screenshot": false,
                }
            }
        ]
    }
    Binary: task_uuid (36), UINT64 total_size, UINT32 path_len + path, UINT32 chunk_size
    (chunk_size is agent-local and omitted from Mythic JSON).
    """

    # First 36 bytes are task UUID
    task_uuid = data[:36]
    data = data[36:]

    # UINT64: exact file size in bytes
    total_size = int.from_bytes(data[0:8], byteorder='big')
    data = data[8:]

    # Retrieve full path of file
    full_path, data = get_bytes_with_size(data)

    # Skip UINT32 CHUNK_SIZE, we are using Offset mode now
    data = data[4:]

    task_json = {
        "task_id": task_uuid.decode('cp850'),
        "download": {
            "total_size": total_size,
            "full_path": full_path.decode('cp850'),
            "is_screenshot": False,
            }
    }

    return task_json, data


def download_cont_to_mythic_format(data):
    """
    Parse download chunk message from Agent and return JSON in Mythic v4 offset mode.
    {
        "action": "post_response",
        "responses": [
            {
                "task_id": "task uuid",
                "download": {
                    "file_id": "UUID From previous response",
                    "chunk_offset": 0,
                    "chunk_data": "base64_blob==",
                }
            }
        ]
    }
    Binary: task_uuid (36), UINT64 chunk_offset, file_id (36), UINT32 data_len + data
    """

    # First 36 bytes are task UUID
    task_uuid = data[:36]
    data = data[36:]

    # UINT64: zero-based byte offset of this block
    chunk_offset = int.from_bytes(data[0:8], byteorder='big')
    data = data[8:]

    # Retrieve UUID from previous response
    file_id = data[:36]
    data = data[36:]

    # Retrive chunk data
    chunk_data, data = get_bytes_with_size(data)
    bs64_chunk_data = base64.b64encode(chunk_data).decode('utf-8')

    task_json = {
        "task_id": task_uuid.decode('cp850'),
        "download": {
            "file_id": file_id.decode('cp850'),
            "chunk_offset": chunk_offset,
            "chunk_data": bs64_chunk_data,
            }
    }

    return task_json, data


def upload_to_mythic_format(data):
    """
    Parse upload message from Agent and return JSON in Mythic format.
    {
        "action": "post_response",
        "responses": [
            {
                "task_id": task_id                                          // the associated task that caused the agent to pull down this file
                "upload": {
                    "chunk_num": #,                                         // which chunk are we currently pulling down
                    "file_id": UUID,                                        //the file specified to pull down to the target
                    "full_path": "full path to uploaded file on target"     //optional
                    "chunk_size": 512000,                                   //bytes of file per chunk
                }
            }
        ]
    }
    """
    
    # First 36 bytes are task UUID
    task_uuid = data[:36]
    data = data[36:]
    
    # Retrieve current chunk number
    chunk_num = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]
        
    # Retrieve UUID from previous response
    file_id = data[:36]
    data = data[36:]
    
    # Retrive full path to location
    full_path, data = get_bytes_with_size(data)

    # Retrieve chunk size
    chunk_size = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]

    response_task = []

    task_json = {
        "task_id": task_uuid.decode('cp850'),
        "upload": {
            "chunk_num": chunk_num,
            "file_id": file_id.decode('cp850'),
            "full_path": full_path.decode('cp850'), 
            "chunk_size": chunk_size
            }
    }
    
    #logging.info(f"[UPLOAD] IMPLANT -> C2: \n\t task_id:{task_uuid.decode('cp850')}, \n\t chunk_num:{chunk_num}, \n\t file_id:{file_id.decode('cp850')}, \n\t full_path:{full_path.decode('cp850')}, \n\t chunk_size:{chunk_size}")
    
    return task_json, data


def p2p_checkin_to_mythic_format(data):
    """
    P2P Agents have a specific JSON field in Mythic "delegates"
    {
        "action": "some action here",
        "delegates": [
            {
                "message": "base64 agent message",
                "uuid": "some uuid Agent1 made up",
                "c2_profile": "ProfileName"
            }
        ]
    }   
    """
    
    # 36-bytes: Task UUID
    task_uuid = data[:36]
    data = data[36:]
    
    # 4-byte int: Task Result
    status_byte = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]
    if status_byte == 0:
        status = "success"
        user_output = "[+] Established link to agent"
    else:
        status = "error"
        error = ERROR_CODES.get(status_byte, {"name": "UNKNOWN_ERROR", "description": f"Error code {status_byte}"})
        user_output += f"[!] {error['name']} : {error['description']}\n"
    
    # 4-byte int: Link ID
    link_id = int.from_bytes(data[0:4], byteorder='big')
    # 4-byte int: Link Type
    link_type = int.from_bytes(data[4:8], byteorder='big')
    link_type_str = "smb" if (link_type == 1) else "tcp"
    data = data[8:]
    
    # Rest of bytes are msg from Link
    output, data = get_bytes_with_size(data)
    
    task_json = {
        "task_id": task_uuid.decode('cp850'),
        "user_output": user_output,
        "status": status,                # Include the status
        "completed": status in ("success", "error")
    }
    
    delegates = [
        {
            "message": output.decode('cp850'),
            "uuid": str(link_id),               # Randomly generated by SMB Agent
            "c2_profile": link_type_str
        }
    ]
    
    #logging.info(f"[P2P_CHECKIN] IMPLANT -> C2: \n\t message: {output.decode('cp850')}, \n\t uuid: {str(link_id)}, \n\t c2_profile: {link_type_str}")
    
    return task_json, delegates, data


def p2p_to_mythic_format(data):
    """
    P2P Agents have a specific JSON field in Mythic "delegates"
    {
        "action": "some action here",
        "delegates": [
            {
                "message": "base64 agent message",
                "uuid": "some uuid Agent1 made up",
                "c2_profile": "ProfileName"
            }
        ]
    }   
    """

    # 36-bytes: Payload ID
    payload_uuid = data[:36]
    data = data[36:]
    link_type = int.from_bytes(data[0:4], byteorder='big')
    link_type_str = "smb" if (link_type == 1) else "tcp"
    data = data[4:]
    
    # Rest of bytes are for Link agent
    output, data = get_bytes_with_size(data)  # The size doesn't include the status byte at the end or the error int32
    
    task_json = None

    delegates = [
        {
            "message": output.decode('cp850'),
            # Use 'mythic_uuid' now that it is set
            "uuid": payload_uuid.decode('cp850'),
            "c2_profile": link_type_str
        }
    ]
    
    #logging.info(f"[P2P] IMPLANT -> C2: \n\t message: {len(output)} bytes, \n\t mythic_uuid: {payload_uuid.decode('cp850')}, \n\t c2_profile: smb")
        
    return task_json, delegates, data


def p2p_remove_to_mythic_format(data):
    """
    Handle P2P Remove message from Parent Agent.
    """
    task_json = None

    # 1-byte: BOOL: Is this from a Task?
    is_from_task = data[0]
    data = data[1:]

    if is_from_task:
        # 36-bytes: Task UUID
        task_uuid = data[:36]
        data = data[36:]

    # 36-bytes: Parent Agent UUID
    parent_uuid = data[:36]
    data = data[36:]

    # 36-bytes: P2P Agent UUID
    p2p_uuid = data[:36]
    data = data[36:]
    link_type = int.from_bytes(data[0:4], byteorder='big')
    link_type_str = "smb" if (link_type == 1) else "tcp"
    data = data[4:]

    if is_from_task:
        task_json = {
            "task_id": task_uuid.decode('cp850'),
            "user_output": f"[+] Unlinked Agent [{p2p_uuid.decode('cp850')}]",
            "status": "success",
            "completed": True
        }

    edges = [
        {
            "source": parent_uuid.decode('cp850'),
            "destination": p2p_uuid.decode('cp850'),
            "action": "remove",
            "c2_profile": link_type_str
        }
    ]

    return task_json, edges, data


def socks_to_mythic_format(data):
    """
    Parse SOCKS data message from Agent and return JSON in Mythic format.
    
    SOCKS messages are forwarded to Mythic in the "socks" array:
    {
        "action": "get_tasking",
        "socks": [
            {
                "server_id": 12345,
                "data": "base64_encoded_data",
                "exit": false
            }
        ]
    }
    
    Binary format from agent:
        UINT32: server_id
        UINT32: data_length
        BYTES:  data
        BYTE:   exit_flag (0x00 or 0x01)
    """
    task_json = None
    
    # UINT32: server_id
    if len(data) < 4:
        logging.error("[SOCKS] Insufficient data for server_id")
        return None, None, data
    
    server_id = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]
    
    # UINT32: data_length + BYTES: data
    if len(data) < 4:
        logging.error("[SOCKS] Insufficient data for data_length")
        return None, None, data
    
    socks_data, data = get_bytes_with_size(data)
    
    # BYTE: exit_flag
    if len(data) < 1:
        logging.error("[SOCKS] Insufficient data for exit_flag")
        return None, None, data
    
    exit_flag = data[0] == 0x01
    data = data[1:]
    
    # Base64 encode the data for Mythic
    data_b64 = base64.b64encode(socks_data).decode('utf-8') if socks_data else ""
    
    socks_msg = {
        "server_id": server_id,
        "data": data_b64,
        "exit": exit_flag
    }
    
    #logging.info(f"[SOCKS] IMPLANT -> C2: server_id={server_id}, data_len={len(socks_data)}, exit={exit_flag}")
    
    return task_json, socks_msg, data


def rportfwd_to_mythic_format(data):
    """
    Parse reverse port forward data message from Agent and return JSON in Mythic format.

    RPORTFWD messages are forwarded to Mythic in the "rpfwd" array:
    {
        "action": "get_tasking",
        "rpfwd": [
            {
                "server_id": 12345,
                "port": 445,
                "data": "base64_encoded_data",
                "exit": false
            }
        ]
    }

    Binary format from agent:
        UINT32: server_id
        UINT32: port
        UINT32: data_length
        BYTES:  data
        BYTE:   exit_flag (0x00 or 0x01)
    """
    task_json = None

    if len(data) < 4:
        logging.error("[RPORTFWD] Insufficient data for server_id")
        return None, None, data

    server_id = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]

    if len(data) < 4:
        logging.error("[RPORTFWD] Insufficient data for port")
        return None, None, data

    port = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]

    if len(data) < 4:
        logging.error("[RPORTFWD] Insufficient data for data_length")
        return None, None, data

    rpfwd_data, data = get_bytes_with_size(data)

    if len(data) < 1:
        logging.error("[RPORTFWD] Insufficient data for exit_flag")
        return None, None, data

    exit_flag = data[0] == 0x01
    data = data[1:]

    data_b64 = base64.b64encode(rpfwd_data).decode('utf-8') if rpfwd_data else ""

    rpfwd_msg = {
        "server_id": server_id,
        "port": port,
        "data": data_b64,
        "exit": exit_flag
    }

    #logging.info(f"[RPORTFWD] IMPLANT -> C2: server_id={server_id}, port={port}, data_len={len(rpfwd_data)}, exit={exit_flag}")

    return task_json, rpfwd_msg, data
