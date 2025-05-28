import argparse
import struct
import sys


# def patch_stub_pipename(filename: str, pipename: str, max_length=256):
#     """
#     Replaces the hardcoded pipe name placeholder (\xAA\xAA\xAA\xAA) in the binary file
#     with the specified pipename string, null-terminated and padded to max_length.
#     """
#     placeholder = b'\xAA\xBB\xCC\xDD'

#     with open(filename, 'rb') as f:
#         data = bytearray(f.read())

#     pipename = f"\\\\.\\pipe\\{pipename}"

#     offset = data.find(placeholder)
#     if offset == -1:
#         raise ValueError("Pipe name placeholder not found in file.")

#     encoded = pipename.encode('ascii') + b'\x00'
#     if len(encoded) > max_length:
#         raise ValueError(f"Pipename too long. Max {max_length-1} characters.")

#     padded = encoded.ljust(max_length, b'\x00')

#     print(f"[+] Patching pipename at offset {offset:#x} with '{pipename}'")
#     data[offset:offset+max_length] = padded

#     with open(filename, 'wb') as f:
#         f.write(data)

#     print("[+] Pipe name patched successfully.")


def patch_stub_length(filename):
    with open(filename, 'rb') as f:
        data = f.read()

    placeholder = b'\xBB\xBB\xBB\xBB'
    offset = data.find(placeholder)

    if offset == -1:
        print("[!] Placeholder not found in the file.")
        sys.exit(1)

    length = len(data)
    patched_length = struct.pack('<I', length)

    print(f"[+] Found placeholder at offset {offset:#x}")
    print(f"[+] Patching in shellcode length: {length} bytes ({patched_length.hex()})")

    patched_data = data[:offset] + patched_length + data[offset+4:]

    with open(filename, 'wb') as f:
        f.write(patched_data)

    print("[+] Patch successful.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("file", help="Path to stub.bin")
    # parser.add_argument("--pipename", required=True, help="Pipe name to patch in")
    args = parser.parse_args()

    patch_stub_length(args.file)
    
    # patch_stub_pipename(args.file, args.pipename)
