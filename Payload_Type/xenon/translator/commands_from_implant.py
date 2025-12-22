from translator.utils import *
import ipaddress, logging

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
        }
    
    return mythic_json



# Handle get_tasking from agent
'''
------------------------------------------
Key	            Key Len (bytes)	    Type
------------------------------------------
Number tasks	4	                Uint32
'''
def get_tasking_to_mythic_format(data):
    """
    Parse get_tasking message from Agent and return JSON in Mythic format.
    """
    numTasks = int.from_bytes(data[0:4], byteorder='big')
    mythic_json = { 
            "action": "get_tasking", 
            "tasking_size": numTasks 
        }
    return mythic_json


"""
Parse one or more post_response messages from Agent and return JSON in Mythic format.

Each response layout:
    [36 bytes]  task_uuid (string)
    [4 bytes]   output_size (uint32 BE)
    [N bytes]   output
    [1 byte]    status (0x95 success, 0x99 error)
    [4 bytes]?  error code (only if status == error)
"""
# def post_response_to_mythic_format(data):
#     """
#     Parse post_response message from Agent and return JSON in Mythic format.
#     {
#         "action": "post_response",
#         "responses": {
#                         "task_id": 0x00,
#                         "user_output": b'',
#                         "status": "success|error"
#                     }
#     }
#     """

#     response_task = []
    
#     # Check the last byte for status
#     status_byte = data[-1]
#     status = "error" if status_byte == 0x99 else "success" if status_byte == 0x95 else "unknown"
    
#     # Add any error codes
#     if status == "error":
#         # Get the Windows Error code from last 4 bytes
#         error_code_bytes = data[-5:-1]
#         error_code = int.from_bytes(error_code_bytes, byteorder='big')
        
#         logging.info(f"ERROR CODE - bytes: {error_code_bytes} code: {error_code}")
#         logging.info(f"RAW BYTES - {data}")
    
#     logging.info(f"POST_RESPONSE status : {(hex(status_byte))} = {status} ")
 
#     # Next 36 bytes are task uuid
#     task_uuid = data[:36]

#     # Get the task buffer
#     data = data[36:]
#     output, data = get_bytes_with_size(data)  # The size doesn't include the status byte at the end or the error int32
    
#     # Prepend a response
#     output_length = len(output)
    
#     # Create the response message for the operator
#     if output_length > 1:
#         user_output = f"[+] agent called home, sent: {output_length} bytes\n[+] received output: \n\n{output.decode('cp850')}"
#     else:
#         user_output = f"[+] agent called home, sent: {output_length} bytes\n"    
    
#     # Add errors here after that stuff above
#     if status == "error":
#         error = ERROR_CODES.get(error_code, {"name": "UNKNOWN_ERROR", "description": f"Error code {error_code}"})
#         user_output += f"[!] {error['name']} : {error['description']}\n"
    
#     task_json = {
#         "task_id": task_uuid.decode('cp850'),
#         "user_output": user_output,
#         "status": status                # Include the status
#     }

#     task_json["completed"] = True
    
#     response_task.append(task_json)
    
#     mythic_json = {
#             "action": "post_response",
#             "responses": response_task
#         }   

#     return mythic_json

def post_response_handler(data):
    """
    Process one or more Agent -> Mythic post_response messages
    """
    mythic_messages = []

    while len(data) > 0:
        if len(data) < 1:
            break

        response_type = data[0]
        data = data[1:]

        logging.info(f"PACKAGE TYPE {response_type}")
    
        if response_type == MYTHIC_POST_RESPONSE:
            task_json, data = post_response_to_mythic_format(data)
            logging.info(f"[MYTHIC_POST_RESPONSE] {task_json}")
            
        elif response_type == MYTHIC_INIT_DOWNLOAD:
            task_json, data = download_init_to_mythic_format(data)

        elif response_type == MYTHIC_CONT_DOWNLOAD:
            task_json, data = download_cont_to_mythic_format(data)

        elif response_type == MYTHIC_UPLOAD_CHUNKED:
            task_json, data = upload_to_mythic_format(data)

        elif response_type == MYTHIC_P2P_CHECK_IN:
            task_json, data = p2p_checkin_to_mythic_format(data)

        elif response_type == MYTHIC_P2P_MSG:
            task_json, data = p2p_to_mythic_format(data)

        else:
            raise ValueError(f"Unknown response type: {hex(response_type)}")

        
        # Normalize to list
        if isinstance(task_json, list):
            mythic_messages.extend(task_json)
        else:
            mythic_messages.append(task_json)

    mythic_json = {
        "action": "post_response",
        "responses": mythic_messages,
    }

    logging.info(f"Final Mythic Message:\n\t{mythic_json}")

    return mythic_json




def post_response_to_mythic_format(data):
    """
    Process simple post response message -> Mythic format
    
    :param data: Raw data from Agent
    """
    logging.info(f"Raw data from agent {data}")

    # --- Task UUID ---
    if len(data) < 36:
        logging.info("Remaining buffer too small for task UUID")

    task_uuid = data[:36].decode("cp850")
    data = data[36:]
    

    # --- Output Buffer ---
    output, data = get_bytes_with_size(data)
    output_length = len(output)

    
    # --- Status Byte ---
    if len(data) < 1:
        logging.info("Missing status byte")
        return
        
    status_byte = data[0]
    data = data[1:]

    if status_byte == 0x95:
        status = "success"          # Succeeded
    elif status_byte == 0x97:
        status = None               # Still processing
    elif status_byte == 0x99:
        status = "error"            # Failed
    else:
        status = "unknown"

    error_code = None

    # --- Optional Error Code ---
    if status == "error":
        if len(data) < 4:
            logging.info("Missing error code for error status")
            

        error_code_bytes = data[:4]
        data = data[4:]
        error_code = int.from_bytes(error_code_bytes, byteorder="big")

        logging.info(
            f"ERROR CODE - bytes: {error_code_bytes} code: {error_code}"
        )

    logging.info(f"POST_RESPONSE [{task_uuid}] status: {hex(status_byte)}")

    # --- Operator Output ---
    if output_length > 0:
        user_output = (
            f"[+] agent called home, sent: {output_length} bytes\n"
            f"[+] received output:\n\n{output.decode('cp850', errors='ignore')}"
        )
    else:
        user_output = "[+] agent called home, no output\n"

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



def download_init_to_mythic_format(data):
    """
    Parse download initialize message from Agent and return JSON in Mythic format.
    {
        "action": "post_response", 
        "responses": [
            {
                "task_id": "UUID here",
                "download": {
                    "total_chunks": 4, 
                    "full_path": "/test/test2/test3.file",                                      // optional full path to the file downloaded
                    "host": "hostname the file is downloaded from",                             // optional
                    "filename": "filename for Mythic/operator if full_path doesn't make sense", // optional
                    "is_screenshot": false,                                                     //indicate if this is a file or screenshot (default is false)
                    "chunk_size": 512000,                                                       // indicate chunk size if intending to send chunks out of order or paralellized
                }
            }
        ]
    }
    """
    
    logging.info(f"DOWNLOAD_INIT Response - \n\t{data}")
    
    # First 36 bytes are task UUID
    task_uuid = data[:36]
    data = data[36:]
    
    # Retrieve total chunks for file
    total_chunks = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]
    
    logging.info(f"total_chunks : {total_chunks}", )
        
    # Retrieve full path of file
    full_path, data = get_bytes_with_size(data)
    
    logging.info(f"full_path : {full_path.decode('cp850')}")
    
    # Retrive chunk size of file chunks
    chunk_size = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]

    logging.info(f"chunk_size : {chunk_size}")

    response_task = []

    task_json = {
        "task_id": task_uuid.decode('cp850'),
        "download": {
            "total_chunks": total_chunks,
            "full_path": full_path.decode('cp850'),
            "is_screenshot": False,     # Agent can ignore this field
            "chunk_size": chunk_size
            }
    }
    
    logging.info(f"[DOWNLOAD_INIT] IMPLANT -> C2: task_id:{task_uuid.decode('cp850')}, total_chunks:{total_chunks}, full_path:{full_path.decode('cp850')}, chunk_size:{chunk_size}")
    
    return task_json, data
    
    response_task.append(task_json)
    
    mythic_json = {
            "action": "post_response",
            "responses": response_task
        }

    logging.info(mythic_json)

    return mythic_json


def download_cont_to_mythic_format(data):
    """
    Parse download chunk message from Agent and return JSON in Mythic format.
    {
        "action": "post_response", 
        "responses": [
            {
                "task_id": "task uuid",
                "download": {
                    "chunk_num": 1, 
                    "file_id": "UUID From previous response", 
                    "chunk_data": "base64_blob==",
                    "chunk_size": 512000, // this is optional, but required if you're not sending it with the initial registration message and planning on sending chunks out of order
                }
            }
        ]
    }
    """
    logging.info("Download Chunks")

    # First 36 bytes are task UUID
    task_uuid = data[:36]
    data = data[36:]
    
    logging.info(f"\tTASK UUID: {task_uuid}")
    
    # Retrieve current chunk
    chunk_num = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]
    
    logging.info(f"\tchunk_num: {chunk_num}")
    
    
    # Retrieve UUID from previous response
    file_id = data[:36]
    data = data[36:]
    
    logging.info(f"\tfile_id: {file_id}")
    
    
    # Retrive chunk data
    chunk_data, data = get_bytes_with_size(data)
    bs64_chunk_data = base64.b64encode(chunk_data).decode('utf-8')      # base64 encode file bytes on translator side here

    logging.info(f"\tbs64_chunk_data: {bs64_chunk_data}")

    # Retrieve chunk size
    chunk_size = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]
    
    logging.info(f"\tchunk_size: {chunk_size}")
    

    response_task = []

    task_json = {
        "task_id": task_uuid.decode('cp850'),
        "download": {
            "chunk_num": chunk_num,
            "file_id": file_id.decode('cp850'),
            "chunk_data": bs64_chunk_data, 
            "chunk_size": chunk_size
            }
    }
    
    return task_json, data
    
    response_task.append(task_json)
    
    mythic_json = {
            "action": "post_response",
            "responses": response_task
        }   

    return mythic_json


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
    
    logging.info(f"[UPLOAD] IMPLANT -> C2: task_id:{task_uuid.decode('cp850')}, chunk_num:{chunk_num}, file_id:{file_id.decode('cp850')}, full_path:{full_path.decode('cp850')}, chunk_size:{chunk_size}")
    
    return task_json, data

    response_task.append(task_json)
    
    mythic_json = {
            "action": "post_response",
            "responses": response_task
        }

    return mythic_json



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
    data = data[4:]
    
    
    # Rest of bytes are from Link Pipe
    output, data = get_bytes_with_size(data)  # The size doesn't include the status byte at the end or the error int32
    
    # Format response with Delegates
    response_task = []
    
    task_json = {
        "task_id": task_uuid.decode('cp850'),
        "user_output": user_output,
        "status": status                # Include the status
    }

    task_json["completed"] = True
    
    response_task.append(task_json)
    
    mythic_json = {
            "action": "post_response",
            "responses": response_task,
            "delegates": [
                {
                    "message": output.decode('cp850'),
                    "uuid": str(link_id),               # Randomly generated by SMB Agent
                    "c2_profile": "smb"
                }
            ]
        }

    logging.info(f"[P2P Check-In] Agent -> C2 : {mythic_json}")

    return mythic_json


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
    
    # logging.info(f"P2P Message from Agent : [{payload_uuid}]")
    
    
    # Rest of bytes are for Link agent
    output, data = get_bytes_with_size(data)  # The size doesn't include the status byte at the end or the error int32
    
    # Format response with Delegates
    response_task = []
    
    task_json = { 
            "action": "get_tasking", 
            "tasking_size": 0 
        }
        
    response_task.append(task_json)
    
    # The only purpose of this message is to forward the delegate to Mythic
    mythic_json = {
            "action": "get_tasking",
            # We do a get_tasking with size 0 cause we don't actually want any tasks for the Agent 1
            "tasking_size": 0,
            "delegates": [
                {
                    "message": output.decode('cp850'),
                    # Use 'mythic_uuid' now that it is set
                    "mythic_uuid": payload_uuid.decode('cp850'),
                    "c2_profile": "smb"
                }
            ]
        }
    
    logging.info(f"[P2P Msg] Agent -> C2 : {mythic_json}")          # TODO - Fix bug where pipe sends Agent 2 "checkin" multiple times. (results in several p2p agents populating)

    return mythic_json