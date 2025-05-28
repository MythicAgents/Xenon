def hash_string_ascii(s: str) -> int:
    """djb2 hash for ASCII strings."""
    h = 5381
    for c in s:
        h = ((h << 5) + h) + ord(c)  # h * 33 + ord(c)
        h &= 0xFFFFFFFF  # simulate 32-bit overflow
    return h

def hash_string_unicode(s: str) -> int:
    """djb2 hash for wide strings (Unicode, like wchar_t*)."""
    h = 5381
    for c in s:
        h = ((h << 5) + h) + ord(c)  # same formula
        h &= 0xFFFFFFFF
    return h

if __name__ == "__main__":
    test_strings = [
        "KERNEL32.DLL",
        "GetProcAddress",
        "LoadLibraryA",
        "LoadLibraryW",
	"CreateFileA",
	"SetStdHandle",
    ]

    print("ASCII Hashes:")
    for s in test_strings:
        print(f"#define {s}_HASH 	0x{hash_string_ascii(s):08X}")

    print("\nUnicode Hashes (e.g., for BaseDllName):")
    for s in test_strings:
        print(f"#define {s}_HASH	0x{hash_string_unicode(s):08X}")
