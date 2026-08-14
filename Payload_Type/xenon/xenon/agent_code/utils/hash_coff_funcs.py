import struct

# FNV-1a hashing function
HASH_SEED = 0x811C9DC5
HASH_PRIME = 0x01000193

def fnv1a_hash(data: str) -> int:
    """Computes a 32-bit FNV-1a hash of the given string."""
    hash_value = HASH_SEED
    for char in data:
        hash_value ^= ord(char)
        hash_value *= HASH_PRIME
        hash_value &= 0xFFFFFFFF  # Keep it 32-bit
    return hash_value

# Original function names
internal_functions = [
    "BeaconDataParse",
    "BeaconDataInt",
    "BeaconDataShort",
    "BeaconDataLength",
    "BeaconDataExtract",
    "BeaconFormatAlloc",
    "BeaconFormatReset",
    "BeaconFormatFree",
    "BeaconFormatAppend",
    "BeaconFormatPrintf",
    "BeaconFormatToString",
    "BeaconFormatInt",
    "BeaconPrintf",
    "BeaconOutput",
    "BeaconUseToken",
    "BeaconRevertToken",
    "BeaconIsAdmin",
    "BeaconGetSpawnTo",
    "BeaconSpawnTemporaryProcess",
    "BeaconInjectProcess",
    "BeaconInjectTemporaryProcess",
    "BeaconCleanupProcess",
    "toWideChar",
    "LoadLibraryA",
    "GetProcAddress",
    "GetModuleHandleA",
    "FreeLibrary",
    "__C_specific_handler",
    "BeaconWakeup",
    "BeaconGetStopJobEvent",
    "BeaconRegisterThreadCallback",
    "BeaconUnregisterThreadCallback",
]

# Function pointers (manually specified or replaced with actual function addresses)
function_pointers = [
    "BeaconDataParse",
    "BeaconDataInt",
    "BeaconDataShort",
    "BeaconDataLength",
    "BeaconDataExtract",
    "BeaconFormatAlloc",
    "BeaconFormatReset",
    "BeaconFormatFree",
    "BeaconFormatAppend",
    "BeaconFormatPrintf",
    "BeaconFormatToString",
    "BeaconFormatInt",
    "BeaconPrintf",
    "BeaconOutput",
    "BeaconUseToken",
    "BeaconRevertToken",
    "BeaconIsAdmin",
    "BeaconGetSpawnTo",
    "BeaconSpawnTemporaryProcess",
    "BeaconInjectProcess",
    "BeaconInjectTemporaryProcess",
    "BeaconCleanupProcess",
    "toWideChar",
    "LoadLibraryA",
    "GetProcAddress",
    "GetModuleHandleA",
    "FreeLibrary",
    "NULL",
    "BeaconWakeup",
    "BeaconGetStopJobEvent",
    "BeaconRegisterThreadCallback",
    "BeaconUnregisterThreadCallback",
]

# Compute hashes for function names
hashed_functions = [(fnv1a_hash(name), pointer) for name, pointer in zip(internal_functions, function_pointers)]

# Generate C array
c_array = "/* Function Parsing */\n"
c_array += "unsigned char* InternalFunctions[34][2] = {\n"
for hash_val, func_ptr in hashed_functions:
    c_array += f"    {{(uint32_t)0x{hash_val:08X}, (unsigned char*){func_ptr}}},\n"
c_array += "};\n"

# Save to a file (optional)
with open("hashed_functions.c", "w") as f:
    f.write(c_array)

# Print to console
print(c_array)
