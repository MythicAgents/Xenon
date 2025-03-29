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


def get_tasking_to_agent_format(tasks):
    """
    Processes task data from Mythic server and pack them for Agent.
    
    Args:
        tasks (list): List of task dictionaries containing "command", "id", and "parameters".

    Returns:
        bytes: Packed binary data to be sent.
    """

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
            elif isinstance(param_value, int):
                encoded += param_value.to_bytes(4, "big")
            elif isinstance(param_value, list):
                logging.info(f"[Arg-list] {param_value}")
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
        command_to_run = task["command"]
        
        hex_code = get_operator_command(command_to_run).to_bytes(1, "big")
        
        task_uuid = task["id"].encode()
        
        data = hex_code + task_uuid
        
        # Process parameters
        parameters = task.get("parameters", "")
        if parameters:
            parameters = json.loads(parameters)
            # Total size of parameters
            data += len(parameters).to_bytes(4, "big")
            # Serialize and add each param
            data += pack_parameters(parameters)
        else:
            data += b"\x00\x00\x00\x00"     # Zero parameters
        
        return len(data).to_bytes(4, "big") + data

    # One byte for command ID + int32 for Number Of Tasks
    data_head = MYTHIC_GET_TASKING.to_bytes(1, "big") + len(tasks).to_bytes(4, "big")
    
    # Encode the data for each task into single byte string
    data_task = b"".join(pack_task(task) for task in tasks)

    return data_head + data_task





def post_response_to_agent_format(responses):
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
    
    for response in responses:
        status = response["status"]
        
        # Response codes
        if status == "success":
            data += b"\x01"
        elif status == "error": 
            data += b"\x00"
        else:
            data += b"\x00"

        # TODO - organize and make better for differnt types of responses.

        # Download responses include a field for file_id
        file_id = response.get("file_id")
        if file_id:
            data += file_id.encode()
        
        
        # Currently a workaround for handling upload responses, since
        # they have additional fields in response that the agent needs
        
        total_chunks = response.get("total_chunks")
        if total_chunks:
            logging.info(f"[UPLOAD] total_chunks : {total_chunks}")
            data += total_chunks.to_bytes(4, "big")
        
        chunk_num = response.get("chunk_num")
        if chunk_num:
            logging.info(f"[UPLOAD] chunk_num : {chunk_num}")
            data += chunk_num.to_bytes(4, "big")
        
        chunk_data = response.get("chunk_data")
        if chunk_data:
            logging.info(f"[UPLOAD] chunk_data(bs64) : {len(chunk_data)} bytes")
            raw_data = base64.b64decode(chunk_data)
            data += len(raw_data).to_bytes(4, "big")
            data += raw_data

    return data
