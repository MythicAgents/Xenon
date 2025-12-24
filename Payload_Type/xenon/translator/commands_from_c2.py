import json, logging, base64
import binascii
from .utils import *

logging.basicConfig(level=logging.INFO)


def checkin_to_agent_format(uuid):
    """
    Responds to Agent check-in request with new callback UUID (in packed serialized format).
    
    Args:
        uuid (str): New UUID for agent

    Returns:
        bytes: Packed data with new uuid (check-in byte + new uuid + \x01)
    """
    data = MYTHIC_CHECK_IN.to_bytes(1, "big") + uuid.encode() + b"\x01"
    return data


def pack_parameters(parameters):
    """
    Encodes parameters dynamically based on their type (string, int, bool).
    
    Args:
        parameters (dict): Parameters to encode.

    Returns:
        bytes: Encoded parameters.
    """
    # TODO - Use packer for all serialization
    encoded = b""

    for param_name, param_value in parameters.items():
        # File Chunk
        if isinstance(param_value, str) and param_name == "chunk_data":
            param_bytes = base64.b64decode(param_value)
            encoded += len(param_bytes).to_bytes(4, "big")
            encoded += param_bytes
            
        elif isinstance(param_value, str):
            param_bytes = param_value.encode()
            encoded += len(param_bytes).to_bytes(4, "big") + param_bytes
        elif isinstance(param_value, bool):
            encoded += b"\x01" if param_value else b"\x00"
            # encoded += param_value.to_bytes(4, "big")
        elif isinstance(param_value, bytes):                # TODO - This won't work because it comes from ParameterType class which doesnt have an option for bytes
            param_bytes = param_value
            encoded += len(param_bytes).to_bytes(4, "big") + param_bytes
        elif isinstance(param_value, int):
            encoded += param_value.to_bytes(4, "big")
        elif isinstance(param_value, list):
            # Mainly used for inline-execute
            
            # logging.info(f"[Arg-list] {param_value}")
            # No arguments
            if param_value == []:
                encoded += b"\x00\x00\x00\x00"
                return encoded

            # Use packer class to pack serialized arguments
            packer = Packer()
            # Handle TypedList as single length-prefixed argument to Agent (right now ONLY used by inline_execute function)
            for item in param_value:
                item_type, item_value = item
                if item_type == "int16":
                    packer.addshort(int(item_value))
                elif item_type == "int32":
                    packer.adduint32(int(item_value))
                elif item_type == "bytes":
                    packer.addbytes(bytes.fromhex(item_value))
                elif item_type == "string":
                    packer.addstr(item_value)
                elif item_type == "wchar":
                    packer.addWstr(item_value)
                elif item_type == "base64":
                    try:
                        decoded_value = base64.b64decode(item_value)
                        packer.addstr(decoded_value)
                    except Exception:
                        raise ValueError(f"Invalid base64 string: {item_value}")

            # Size + Packed Data
            packed_params = packer.getbuffer()    # Returns length-prefixed buffer
            logging.info(f"Packed params {packed_params}")
            encoded += len(packed_params).to_bytes(4, "big") + packed_params
            
        else:
            raise TypeError(f"Unsupported parameter type for '{param_name}': {type(param_value)}")
    
    return encoded

def pack_task(task):
    """
    Encodes a single task into binary format.
    
    Args:
        task (dict): A single task dictionary.

    Returns:
        bytes: Encoded task data.
    """

    # Parse JSON
    command_to_run = task["command"]
    task_uuid = task["id"].encode()
    parameters = task.get("parameters", "")
    
    hex_code = get_operator_command(command_to_run).to_bytes(1, "big")
    
    data = hex_code + task_uuid
    
    # Process parameters
    if parameters:
        parameters = json.loads(parameters)
        # Total size of parameters
        data += len(parameters).to_bytes(4, "big")
        # Serialize and add each param
        data += pack_parameters(parameters)
    else:
        data += b"\x00\x00\x00\x00"     # Zero parameters
    
    return len(data).to_bytes(4, "big") + data


def format_task_response_as_task(response):
    """
    Tell agent what to do with response by packing it as a task.
    {
        "task_id": UUID,
        "status": "success" or "error",
        "error": 'error message if it exists',
        "download": {......
        "upload": {........
    }
    """    
    task_id = response.get("task_id")
    
    # Download/Upload specific fields
    file_id = response.get("file_id")
    total_chunks = response.get("total_chunks")
    chunk_num = response.get("chunk_num")
    chunk_data = response.get("chunk_data")
    
    params = {}
    
    # params["task_id"] = task_id
    
    # Uploads
    if file_id and chunk_data:
        response_type = "upload_resp"
        # params["file_id"] = file_id
        if total_chunks:
            params["total_chunks"] = total_chunks
        if chunk_num:
            params["chunk_num"] = chunk_num
        if chunk_data:
            params["chunk_data"] = chunk_data     # Gets decoded in pack_parameters

    # Downloads
    elif file_id and not chunk_data:
        response_type = "download_resp"
        params["file_id"] = file_id
        
    # Normal response (blank)
    else:
        return None
        # response_type = "normal_resp"
            
    params = json.dumps(params)

    task_json = {
            "command": response_type,
            "parameters": params,
            "id": task_id
        }
    
    logging.info("[FORMAT RESP AS TASK]")
    
    return task_json


def format_delegate_as_task(delegate):
    """
    Format delegate Msg as individial tasks for Agent
    
    Args:
        delegates (list): [
            {
                "message": "agentMsg",
                "uuid": "UUID",
                "c2_profile": "profilename"
            }
        ] 
        
    Returns:
        task (list): [
            {
                "timestamp":1766426896,
                "command":"delegate_msg",
                "parameters":"<base64_msg>",
                "id": int.to_bytes(0, byteorder="big")
            }
        ]
    """

    # The first delegate message from P2P agent contains "uuid", "mythic_uuid", and "new_uuid"
    # All subsequent delegate messages only contain "uuid"
    
    uuid = delegate.get('uuid')
    new_uuid = delegate.get('new_uuid')
    if new_uuid:
        is_checkin = True
    else:
        is_checkin = False
    
    
    base64_msg = delegate.get('message')
    
    # P2P Checkin - during P2P Checkin LinkID is a random int32
    if is_checkin:
        link_id = int(uuid)
        p2p_uuid = new_uuid
    # P2P Tasking
    else:
        link_id = 0
        p2p_uuid = uuid
        
    params = json.dumps({
        "is_checkin": is_checkin,
        "link_id": link_id,
        "p2p_uuid": p2p_uuid,
        "base64_msg": base64_msg,
        })
    
    response = {
            "command": "p2p_resp",
            "parameters": params,
            "id": "00000000-0000-0000-0000-000000000000"   # Not a real task so send blank ID
        }
    
    return response



def get_responses_to_agent_format(inputMsg):
    """
    Pack get_tasking and task responses as well
    
    Args:
        inputMsg: {
            "action": "get_tasking",
            "tasks": [
                {
                    "command": "command name",
                    "parameters": "command param string",
                    "timestamp": 1578706611.324671, //timestamp provided to help with ordering
                    "id": "task uuid",
                }
            ],
            "responses": [
                {
                    "task_id": UUID,
                    "status": "success" or "error",
                    "error": 'error message if it exists'
                }
            ],
            "delegates": [
                {"message": agentMessage, "c2_profile": "ProfileName", "uuid": "uuid here"},
            ],
        }

    Returns:
        bytes: Packed binary data to be sent.
    """
    
    # Combine get_tasking and post_response packages into one format
    # PACKET FORMAT:
    # - BYTE:    Msg Type
    # - INT32:   Numbr of tasks
    # - DWORD:   Size of task
    # - BYTE:    Task ID
    # - CHAR:    Task UUID
    # - CHAR:    Task arguments
    

    tasks = inputMsg.Message.get("tasks")
    responses = inputMsg.Message.get("responses")
    delegates = inputMsg.Message.get("delegates")
    
    # Task responses are packed as tasks
    if responses:
        for task_response in responses:
            task_response_as_task_json = format_task_response_as_task(task_response)
            if task_response_as_task_json is not None:
                tasks.append(task_response_as_task_json)
            
    # Delegate Msgs are packed as tasks
    if delegates:
        for delegate in delegates:
            delegate_as_task_json = format_delegate_as_task(delegate)
            tasks.append(delegate_as_task_json)
        
    ############################################################
    ################ Build Response Packet #####################
    ############################################################
    
    # Packet Header
    # - Type + NumOfMsgs
    data_head = MYTHIC_GET_TASKING.to_bytes(1, "big") + len(tasks).to_bytes(4, "big")
    
    # Packet Body
    # - Tasks (size + id + uuid + params(size + bytes) )
    data_task = b"".join(pack_task(task) for task in tasks)

    
    return data_head + data_task
    
    



'''
    LEGACY CODE BELOW
'''


def get_tasking_to_agent_format(inputMsg):
    """
    Processes task data from Mythic server and pack them for Agent.
    
    Args:
        tasks (list): List of task dictionaries containing "command", "id", and "parameters".

    Returns:
        bytes: Packed binary data to be sent.
    """
    
    def format_delegate_as_task(delegate):
        """
        Format delegate Msg as individial tasks for Agent
        
        Args:
            delegates (list): [
                {
                    "message": "agentMsg",
                    "uuid": "UUID",
                    "c2_profile": "profilename"
                }
            ] 
            
        Returns:
            task (list): [
                {
                    "timestamp":1766426896,
                    "command":"delegate_msg",
                    "parameters":"<base64_msg>",
                    "id": int.to_bytes(0, byteorder="big")
                }
            ]
        """
        new_uuid = delegate.get('new_uuid')
        uuid = delegate.get('uuid')
        base64_msg = delegate.get('message')
        # P2P Checkin
        if new_uuid:
            p2p_uuid = new_uuid
        # P2P Tasking
        else:
            p2p_uuid = uuid
            
        params = json.dumps({
            "p2p_uuid": p2p_uuid, 
            "base64_msg": base64_msg
            })
        
        response = {
                "command": "p2p_resp",
                "parameters": params,
                "id": "00000000-0000-0000-0000-000000000000"                                            # Not a real task so send blank ID
            }
        
        return response

    def pack_parameters(parameters):
        """
        Encodes parameters dynamically based on their type (string, int, bool).
        
        Args:
            parameters (dict): Parameters to encode.

        Returns:
            bytes: Encoded parameters.
        """
        # TODO - Use packer for all serialization
        encoded = b""
    
        for param_name, param_value in parameters.items():
            if isinstance(param_value, str):
                param_bytes = param_value.encode()
                encoded += len(param_bytes).to_bytes(4, "big") + param_bytes
            elif isinstance(param_value, bool):
                encoded += param_value.to_bytes(4, "big")
            elif isinstance(param_value, bytes):                # TODO - This won't work because it comes from ParameterType class which doesnt have an option for bytes
                param_bytes = bytes.fromhex(param_value)
                encoded += len(param_bytes).to_bytes(4, "big") + param_bytes
            elif isinstance(param_value, int):
                encoded += param_value.to_bytes(4, "big")
            elif isinstance(param_value, list):
                # logging.info(f"[Arg-list] {param_value}")
                # No arguments
                if param_value == []:
                    encoded += b"\x00\x00\x00\x00"
                    return encoded

                # Use packer class to pack serialized arguments
                packer = Packer()
                # Handle TypedList as single length-prefixed argument to Agent (right now ONLY used by inline_execute function)
                for item in param_value:
                    item_type, item_value = item
                    if item_type == "int16":
                        packer.addshort(int(item_value))
                    elif item_type == "int32":
                        packer.adduint32(int(item_value))
                    elif item_type == "bytes":
                        packer.addbytes(bytes.fromhex(item_value))
                    elif item_type == "string":
                        packer.addstr(item_value)
                    elif item_type == "wchar":
                        packer.addWstr(item_value)
                    elif item_type == "base64":
                        try:
                            decoded_value = base64.b64decode(item_value)
                            packer.addstr(decoded_value)
                        except Exception:
                            raise ValueError(f"Invalid base64 string: {item_value}")

                # Size + Packed Data
                packed_params = packer.getbuffer()    # Returns length-prefixed buffer
                encoded += len(packed_params).to_bytes(4, "big") + packed_params
                
            else:
                raise TypeError(f"Unsupported parameter type for '{param_name}': {type(param_value)}")
        
        return encoded

    def pack_task(task):
        """
        Encodes a single task into binary format.
        
        Args:
            task (dict): A single task dictionary.

        Returns:
            bytes: Encoded task data.
        """

        # Parse JSON
        command_to_run = task["command"]
        task_uuid = task["id"].encode()
        parameters = task.get("parameters", "")
        
        hex_code = get_operator_command(command_to_run).to_bytes(1, "big")
        
        data = hex_code + task_uuid
        
        # Process parameters
        if parameters:
            parameters = json.loads(parameters)
            # Total size of parameters
            data += len(parameters).to_bytes(4, "big")
            # Serialize and add each param
            data += pack_parameters(parameters)
        else:
            data += b"\x00\x00\x00\x00"     # Zero parameters
        
        return len(data).to_bytes(4, "big") + data

############
### Main ###
############

    tasks = inputMsg.Message.get("tasks")
    delegates = inputMsg.Message.get("delegates")
    
    # We repurpose delegate msgs as Tasks for agent
    if delegates:
        for delegate in delegates:
            formatted = format_delegate_as_task(delegate)
            tasks.append(formatted)
    
    # HEADER
    #   - BYTE  Get_Tasking ID 
    #   - INT32 Number of Tasks
    data_head = MYTHIC_GET_TASKING.to_bytes(1, "big") + len(tasks).to_bytes(4, "big")
    
    # BODY
    #   - INT32 SizeOfBody
    #   - BYTE  Command ID
    #   - BYTES Task UUID
    #   - INT32 Size of Params
    #   - Params
    #       - INT32 Size
    #       - BYTES Data
    #   
    # Encode the data for each task into single byte string
    data_task = b"".join(pack_task(task) for task in tasks)

    return data_head + data_task


def post_response_to_agent_format(inputMsg):
    """
    Processes task results from Mythic server and sends results to Agent.
    - For normal tasks, this is either "success" or "error". 
    - Special tasks like download or upload, might have additional fields.
    
    Args:
        responses (list): List of JSON containing results of tasks

    Returns:
        bytes: Packed data for task results (status-byte + optional other data)
    """
    data = b""
    
    
    responses = inputMsg.Message.get("responses")
    delegates = inputMsg.Message.get("delegates")
    
    
        
    # Process Delegate Messages
    if delegates:
        for delegate in delegates:
            id = delegate.get("uuid")
            new_uuid = delegate.get("new_uuid")
            mythic_uuid = delegate.get("mythic_uuid")
            base64_msg = delegate.get('message')
            
            if id:
                is_checkin = True
            else:
                is_checkin = False
            
            # Response codes
            data += b"\x01" # Status: Success
            # Msg Type Delegate
            data += MYTHIC_DELEGATE_RESP.to_bytes(1, byteorder="big")
            
            
        # P2P Checkin Response
            if is_checkin:
                # IsCheckin
                data += b"\x01"
                # Include Temp ID for linking
                data += int(id).to_bytes(4, byteorder="big", signed=False)
                # Mythic UUID
                data += len(new_uuid).to_bytes(4, 'big') + new_uuid.encode()
                # P2P msg
                bytes = base64_msg.encode()
                data += len(bytes).to_bytes(4, "big") + bytes
                
        # P2P Tasking Response
            else:
                # IsCheckin
                data += b"\x00"
                # Mythic UUID
                data += len(mythic_uuid).to_bytes(4, 'big') + mythic_uuid.encode()
                # P2P msg
                bytes = base64_msg.encode()
                data += len(bytes).to_bytes(4, "big") + bytes
    
    # Process Task Responses
    for response in responses:
        status = response["status"]
        download = False
        upload = False
        
        # Response codes
        if status == "success":
            data += b"\x01"
        elif status == "error": 
            data += b"\x00"
        else:
            data += b"\x0A"
        
        # Download/Upload specific fields
        file_id = response.get("file_id")
        total_chunks = response.get("total_chunks")
        chunk_num = response.get("chunk_num")
        chunk_data = response.get("chunk_data")
        
        if file_id and chunk_data:
            upload = True
        elif file_id and not chunk_data:
            download = True
        
# DOWNLOADS
        # Download responses include a field for file_id
        if download:
            data += MYTHIC_DOWNLOAD_RESP.to_bytes(1, byteorder="big")
            data += response.get("task_id").encode()
            data += file_id.encode()

# UPLOADS
        elif upload:
            data += MYTHIC_UPLOAD_RESP.to_bytes(1, byteorder="big")
            data += response.get("task_id").encode()
            
            # Currently a workaround for handling upload responses, since
            # they have additional fields in response that the agent needs
            if total_chunks:
                logging.info(f"[UPLOAD] total_chunks : {total_chunks}")
                data += total_chunks.to_bytes(4, "big")
            
            if chunk_num:
                logging.info(f"[UPLOAD] chunk_num : {chunk_num}")
                data += chunk_num.to_bytes(4, "big")
            
            if chunk_data:
                logging.info(f"[UPLOAD] chunk_data(bs64) : {len(chunk_data)} bytes")
                raw_data = base64.b64decode(chunk_data)
                data += len(raw_data).to_bytes(4, "big")
                data += raw_data

# NORMAL (nothing else needed)
        else:
            data += MYTHIC_NORMAL_RESP.to_bytes(1, byteorder="big")
            # data += response.get("task_id").encode()

    return data


def delegates_to_agent_format(inputMsg):
    """
    Process the "delegates" section of responses from Mythic. 
    
    Args:
        delegates (list): List of JSON containing results of delegates

    Returns:
        bytes: Packed data for delegate messages. ( hasDelegates + NumOfDelegates + ( UUID + SizeOfMessage + BASE64_MESSAGE ) )
    """
    
    delegates = inputMsg.Message.get("delegates")
    
    if delegates:
        
        # logging.info(f"\tHas Delegates.")

        # (Boolean) Contains Delegate
        packed = b"\x01"

        # Number of delegate messages
        num_of_delegates = len(delegates)
        packed += num_of_delegates.to_bytes(4, "big")

        # Iterate delegate messages
        for msg in delegates:
            new_uuid = msg.get('new_uuid')
            uuid = msg.get('uuid')
            base64_msg = msg.get('message')
            
            # P2P Checkin Response
            if new_uuid:
                # Mythic UUID
                packed += len(new_uuid).to_bytes(4, 'big') + new_uuid.encode()
                # P2P msg
                bytes = base64_msg.encode()
                packed += len(bytes).to_bytes(4, "big") + bytes
                
            # P2P Tasking Response
            else:
                # Mythic UUID
                packed += len(uuid).to_bytes(4, 'big') + uuid.encode()
                # P2P msg
                bytes = base64_msg.encode()
                packed += len(bytes).to_bytes(4, "big") + bytes
             
            return packed
    
    else:
        packed = b"\x00"
        return packed
        
        '''
        P2P Checkin
        
        [
            {
                'message': '<base64>', 
                'uuid': '12345',            # UINT32 ID that Agent 2 made up
                'c2_profile': 'smb', 
                'mythic_uuid': '8ea5353e-dbf8-46f4-9ab3-122cb7b48e54', 
                'new_uuid': '8ea5353e-dbf8-46f4-9ab3-122cb7b48e54'
            }
        ]
        ....
        P2P Tasking
        [
            {
                'message': '<base64>', 
                'uuid': '8ea5353e-dbf8-46f4-9ab3-122cb7b48e54', 
                'c2_profile': 'smb'
            }
        ]
        '''