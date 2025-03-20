
# Define constants for transformation types
TRANSFORM_TYPES = {
    "none": 0x0,
    "append": 0x1,
    "prepend": 0x2,
    "base64": 0x3,
    "base64url": 0x4,
    "xor": 0x5,
    "query": 0x6,
    "header": 0x7,
    "cookie": 0x8,
    "body": 0x9,
    "_parameter": 0x10,
    "_header": 0xA,
    "_cookie": 0xB,           # not implemented
    "_body": 0xC,
    "_hostheader": 0xD        # not implemented
}

# Binary serialization functions
def serialize_string(string, pack_size=True):
    """Serialize a string with an optional size prefix."""
    data = b""
    if pack_size:
        data += len(string).to_bytes(4, "big")
    data += string.encode()
    
    return data

def serialize_int(data):
    """Serialize an integer as a 4-byte big-endian value."""
    return data.to_bytes(4, "big")

def serialize_bool(data):
    """Serialize a boolean as a byte."""
    x = 1 if data else 0
    return x.to_bytes(1, "big")


# Generate serialized hex strings for GET and POST C2 malleable profiles
def generate_raw_c2_transform_definitions(data):
    def extract_transforms(transforms, is_server=False):
        transform_data = b""
        if transforms:
            for transform in transforms:
                action = transform["action"].lower()
                type_value = TRANSFORM_TYPES.get(action, 0x0)
                transform_data += serialize_int(type_value)
                
                # Add associated data with size prefix if applicable
                if action == "xor":
                    xor_key = transform.get("value", "")
                    transform_data += serialize_string(xor_key)
                elif action == "prepend" and is_server:
                    # Only serialize the size of prepend value for server
                    prepend_value = transform.get("value", "")
                    transform_data += serialize_int(len(prepend_value))
                elif action == "append" and is_server:
                    # Only serialize the size of append value for server
                    append_value = transform.get("value", "")
                    transform_data += serialize_int(len(append_value))
                elif action in ["prepend", "append"]:
                    # Serialize full value for client or non-server case
                    value = transform.get("value", "")
                    transform_data += serialize_string(value)
        return transform_data
    
    def build_definitions(req_type, config, section):
        definitions = b""
        is_server = section == 'server'
        
        # These sections only matters for CLIENT
        if not is_server:
            # Pack URIs
            uris = config.get("uris", [])
            definitions += serialize_int(len(uris))  # Number of URIs
            for uri in uris:
                definitions += serialize_string(uri)  # Serialize each URI
            
            # Headers (excluding User-Agent)
            headers = config[section].get('headers', {})
            if headers:
                for key, value in headers.items():
                    # User-agent is not packed here due to when it is needed on agent
                    if key.lower() == 'user-agent':
                        continue
                    
                    # Handle arbitrary cookies
                    elif key.lower() == 'cookie':
                        type_header = TRANSFORM_TYPES.get('_cookie', 0x0)
                        definitions += serialize_int(type_header)
                        definitions += serialize_string(f'{key}: {value}')
                    
                    # TODO host header handled special
                    
                    # Handle all other arbitrary headers
                    else:
                        type_header = TRANSFORM_TYPES.get('_header', 0x0)
                        definitions += serialize_int(type_header)
                        definitions += serialize_string(f'{key}: {value}')
            
            # GET Url parameters
            parameters = config[section].get('parameters', {})
            if parameters:
                for key, value in parameters.items():
                    type_parameter = TRANSFORM_TYPES.get('_parameter', 0x0)         # Add arbitrary URL param
                    definitions += serialize_int(type_parameter)
                    definitions += serialize_string(f'{key}={value}')           
        
        # Extract transformations (b64, xor, pre/append)
        transforms = config[section].get('transforms', [])
        is_server = section == 'server'
        if is_server:
            transforms = transforms[::-1]  # Reverse order for server transforms
        if transforms:
            definitions += extract_transforms(transforms, is_server=is_server)
        
        # DON'T MOVE ORDER
        # Insert payload location action LAST, because the client will unpack buffer in order
        # Find payload location (header, cookie, query, body)
        # Find payload location (header, cookie, query, body)
        message_location = config[section].get('message', {}).get('location', "").lower()
        message_location_value = TRANSFORM_TYPES.get(message_location, 0x0)
        definitions += serialize_int(message_location_value)

        # Some locations need a 'key' name (query, header, cookie) 
        if message_location in {"header", "cookie", "query"}:
            message_name = config[section].get('message', {}).get('name', "")
            definitions += serialize_string(message_name)

        
        return definitions
    
    # Serialize definitions for each section and convert to hex strings
    get_client = ""
    get_server = ""
    post_client = ""
    post_server = ""
    
    if "get" in data:
        get_client_data = build_definitions("GET", data["get"], "client")
        get_client = ''.join(f'\\x{byte:02X}' for byte in get_client_data)
        
        get_server_data = build_definitions("GET", data["get"], "server")
        get_server = ''.join(f'\\x{byte:02X}' for byte in get_server_data)
    
    if "post" in data:
        post_client_data = build_definitions("POST", data["post"], "client")
        post_client = ''.join(f'\\x{byte:02X}' for byte in post_client_data)
        
        post_server_data = build_definitions("POST", data["post"], "server")
        post_server = ''.join(f'\\x{byte:02X}' for byte in post_server_data)
    
    return get_client, post_client, get_server, post_server
