import base64
import os
from struct import pack, calcsize


# Mythic messages
MYTHIC_CHECK_IN = 0xf1
MYTHIC_GET_TASKING = 0x00
MYTHIC_POST_RESPONSE = 0x01
MYTHIC_INIT_DOWNLOAD = 0x02
MYTHIC_CONT_DOWNLOAD = 0x03
MYTHIC_UPLOAD_CHUNKED = 0x04

commands = {
    "status": 0x37,
    "sleep": 0x38,
    "example": 0x40, 
    "rm": 0x39, 
    "ls": 0x41, 
    "cd": 0x42, 
    "pwd": 0x43, 
    "mkdir": 0x44, 
    "cp": 0x45, 
    "cat": 0x46, 
    "upload": 0x50, 
    "download": 0x51,
    "ps": 0x52, 
    "inline_execute": 0x53,
    "execute_assembly": 0x54,
    "spawnto": 0x55,
    "shell": 0x60, 
    "powershell": 0x61, 
    "getuid": 0x70, 
    "steal_token": 0x71, 
    "make_token": 0x72, 
    "rev2self": 0x73, 
    "exit": 0x80
}

def get_operator_command(command_name):
    """
    Returns the byte indicator for a given command string.
    """
    return commands.get(command_name, None)  # Returns None if command isn't found


def get_bytes_with_size(data):
    """
    Parse a length-prefixed buffer and return size, data.
    """
    size = int.from_bytes(data[0:4], byteorder='big')
    data = data[4:]
    return data[:size], data[size:]

class Packer:
    """
    Packed data into length-prefixed binary format.

    Reference: From Havoc framework https://github.com/HavocFramework/Modules/blob/main/Packer/packer.py
    """
    def __init__(self):
        self.buffer : bytes = b''
        self.size   : int   = 0

    def getbuffer(self):
        return pack("<L", self.size) + self.buffer

    def addstr(self, s):
        if s is None:
            s = ''
        if isinstance(s, str):
            s = s.encode("utf-8" )
        fmt = "<L{}s".format(len(s) + 1)
        self.buffer += pack(fmt, len(s)+1, s)
        self.size   += calcsize(fmt)

    def addWstr(self, s):
        if s is None:
            s = ''
        s = s.encode("utf-16_le")
        fmt = "<L{}s".format(len(s) + 2)
        self.buffer += pack(fmt, len(s)+2, s)
        self.size   += calcsize(fmt)

    def addbytes(self, b):
        if b is None:
            b = b''
        fmt = "<L{}s".format(len(b))
        self.buffer += pack(fmt, len(b), b)
        self.size   += calcsize(fmt)

    def addbool(self, b):
        fmt = '<I'
        self.buffer += pack(fmt, 1 if b else 0)
        self.size   += 4

    def adduint32(self, n):
        fmt = '<I'
        self.buffer += pack(fmt, n)
        self.size   += 4

    def addint(self, dint):
        self.buffer += pack("<i", dint)
        self.size   += 4

    def addshort(self, n):
        fmt = '<h'
        self.buffer += pack(fmt, n)
        self.size   += 2


# Common Windows error codes. Adding them as I go
ERROR_CODES = {
    1111: {'description': 'Mythic download initialization failed.', 'name': 'MYTHIC_ERROR_DOWNLOAD'},
    1112: {'description': 'Unknown error occurred during Mythic upload.', 'name': 'MYTHIC_ERROR_UPLOAD'},
    1113: {'description': 'Unknown error occurred during BOF execution.', 'name': 'MYTHIC_ERROR_BOF'},
    0: {'description': 'The operation completed successfully.',
     'name': 'ERROR_SUCCESS'},
    1: {'description': 'Incorrect function.', 'name': 'ERROR_INVALID_FUNCTION'},
    2: {'description': 'The system cannot find the file specified.',
        'name': 'ERROR_FILE_NOT_FOUND'},
    3: {'description': 'The system cannot find the path specified.',
        'name': 'ERROR_PATH_NOT_FOUND'},
    4: {'description': 'The system cannot open the file.',
        'name': 'ERROR_TOO_MANY_OPEN_FILES'},
    5: {'description': 'Access is denied.', 'name': 'ERROR_ACCESS_DENIED'},
    6: {'description': 'The handle is invalid.', 'name': 'ERROR_INVALID_HANDLE'},
    7: {'description': 'The storage control blocks were destroyed.',
        'name': 'ERROR_ARENA_TRASHED'},
    8: {'description': 'Not enough memory resources are available to process this '
                        'command.',
        'name': 'ERROR_NOT_ENOUGH_MEMORY'},
    9: {'description': 'The storage control block address is invalid.',
        'name': 'ERROR_INVALID_BLOCK'},
    10: {'description': 'The environment is incorrect.',
        'name': 'ERROR_BAD_ENVIRONMENT'},
    11: {'description': 'An attempt was made to load a program with an incorrect '
                        'format.',
        'name': 'ERROR_BAD_FORMAT'},
    12: {'description': 'The access code is invalid.',
        'name': 'ERROR_INVALID_ACCESS'},
    13: {'description': 'The data is invalid.', 'name': 'ERROR_INVALID_DATA'},
    14: {'description': 'Not enough storage is available to complete this '
                        'operation.',
        'name': 'ERROR_OUTOFMEMORY'},
    15: {'description': 'The system cannot find the drive specified.',
        'name': 'ERROR_INVALID_DRIVE'},
    16: {'description': 'The directory cannot be removed.',
        'name': 'ERROR_CURRENT_DIRECTORY'},
    17: {'description': 'The system cannot move the file to a different disk '
                        'drive.',
        'name': 'ERROR_NOT_SAME_DEVICE'},
    18: {'description': 'There are no more files.', 'name': 'ERROR_NO_MORE_FILES'},
    19: {'description': 'The media is write protected.',
        'name': 'ERROR_WRITE_PROTECT'},
    20: {'description': 'The system cannot find the device specified.',
        'name': 'ERROR_BAD_UNIT'},
    21: {'description': 'The device is not ready.', 'name': 'ERROR_NOT_READY'},
    22: {'description': 'The device does not recognize the command.',
        'name': 'ERROR_BAD_COMMAND'},
    23: {'description': 'Data error (cyclic redundancy '
                        'check).:ERROR_BAD_LENGTH:24:The program issued a command '
                        'but the command length is incorrect.',
        'name': 'ERROR_CRC'},
    25: {'description': 'The drive cannot locate a specific area or track on the '
                        'disk.',
        'name': 'ERROR_SEEK'},
    26: {'description': 'The specified disk or diskette cannot be accessed.',
        'name': 'ERROR_NOT_DOS_DISK'},
    27: {'description': 'The drive cannot find the sector requested.',
        'name': 'ERROR_SECTOR_NOT_FOUND'},
    28: {'description': 'The printer is out of paper.',
        'name': 'ERROR_OUT_OF_PAPER'},
    29: {'description': 'The system cannot write to the specified device.',
        'name': 'ERROR_WRITE_FAULT'},
    30: {'description': 'The system cannot read from the specified device.',
        'name': 'ERROR_READ_FAULT'},
    31: {'description': 'A device attached to the system is not functioning.',
        'name': 'ERROR_GEN_FAILURE'},
    32: {'description': 'The process cannot access the file because it is being '
                        'used by another process.',
        'name': 'ERROR_SHARING_VIOLATION'},
    33: {'description': 'The process cannot access the file because another '
                        'process has locked a portion of the file.',
        'name': 'ERROR_LOCK_VIOLATION'},
    34: {'description': 'The wrong diskette is in the drive. Insert %2 (Volume '
                        'Serial Number: %3) into drive %1.',
        'name': 'ERROR_WRONG_DISK'},
    36: {'description': 'Too many files opened for sharing.',
        'name': 'ERROR_SHARING_BUFFER_EXCEEDED'},
    38: {'description': 'Reached the end of the file.',
        'name': 'ERROR_HANDLE_EOF'},
    39: {'description': 'The disk is full.', 'name': 'ERROR_HANDLE_DISK_FULL'},
    50: {'description': 'The request is not supported.',
        'name': 'ERROR_NOT_SUPPORTED'},
    51: {'description': 'Windows cannot find the network path. Verify that the '
                        'network path is correct and the destination computer is '
                        'not busy or turned off. If Windows still cannot find the '
                        'network path, contact your network administrator.',
        'name': 'ERROR_REM_NOT_LIST'},
    52: {'description': 'You were not connected because a duplicate name exists '
                        'on the network. If joining a domain, go to System in '
                        'Control Panel to change the computer name and try again. '
                        'If joining a workgroup, choose another workgroup name.',
        'name': 'ERROR_DUP_NAME'},
    53: {'description': 'The network path was not found.',
        'name': 'ERROR_BAD_NETPATH'},
    54: {'description': 'The network is busy.', 'name': 'ERROR_NETWORK_BUSY'},
    55: {'description': 'The specified network resource or device is no longer '
                        'available.',
        'name': 'ERROR_DEV_NOT_EXIST'},
    56: {'description': 'The network BIOS command limit has been reached.',
        'name': 'ERROR_TOO_MANY_CMDS'},
    57: {'description': 'A network adapter hardware error '
                        'occurred.:ERROR_BAD_NET_RESP:58:The specified server '
                        'cannot perform the requested operation.',
        'name': 'ERROR_ADAP_HDW_ERR'},
    59: {'description': 'An unexpected network error '
                        'occurred.:ERROR_BAD_REM_ADAP:60:The remote adapter is '
                        'not compatible.',
        'name': 'ERROR_UNEXP_NET_ERR'},
    61: {'description': 'The printer queue is full.', 'name': 'ERROR_PRINTQ_FULL'},
    62: {'description': 'Space to store the file waiting to be printed is not '
                        'available on the server.',
        'name': 'ERROR_NO_SPOOL_SPACE'},
    63: {'description': 'Your file waiting to be printed was deleted.',
        'name': 'ERROR_PRINT_CANCELLED'},
    64: {'description': 'The specified network name is no longer available.',
        'name': 'ERROR_NETNAME_DELETED'},
    65: {'description': 'Network access is denied.',
        'name': 'ERROR_NETWORK_ACCESS_DENIED'},
    66: {'description': 'The network resource type is not correct.',
        'name': 'ERROR_BAD_DEV_TYPE'},
    67: {'description': 'The network name cannot be found.',
        'name': 'ERROR_BAD_NET_NAME'},
    68: {'description': 'The name limit for the local computer network adapter '
                        'card was exceeded.',
        'name': 'ERROR_TOO_MANY_NAMES'},
    69: {'description': 'The network BIOS session limit was exceeded.',
        'name': 'ERROR_TOO_MANY_SESS'},
    70: {'description': 'The remote server has been paused or is in the process '
                        'of being started.',
        'name': 'ERROR_SHARING_PAUSED'},
    71: {'description': 'No more connections can be made to this remote computer '
                        'at this time because there are already as many '
                        'connections as the computer can accept.',
        'name': 'ERROR_REQ_NOT_ACCEP'},
    72: {'description': 'The specified printer or disk device has been paused.',
        'name': 'ERROR_REDIR_PAUSED'},
    80: {'description': 'The file exists.', 'name': 'ERROR_FILE_EXISTS'},
    82: {'description': 'The directory or file cannot be created.',
        'name': 'ERROR_CANNOT_MAKE'},
    83: {'description': 'Fail on INT 24.', 'name': 'ERROR_FAIL_I24'},
    84: {'description': 'Storage to process this request is not available.',
        'name': 'ERROR_OUT_OF_STRUCTURES'},
    85: {'description': 'The local device name is already in use.',
        'name': 'ERROR_ALREADY_ASSIGNED'},
    86: {'description': 'The specified network password is not correct.',
        'name': 'ERROR_INVALID_PASSWORD'},
    87: {'description': 'The parameter is incorrect.',
        'name': 'ERROR_INVALID_PARAMETER'},
    88: {'description': 'A write fault occurred on the network.',
        'name': 'ERROR_NET_WRITE_FAULT'},
    89: {'description': 'The system cannot start another process at this time.',
        'name': 'ERROR_NO_PROC_SLOTS'},
    100: {'description': 'Cannot create another system semaphore.',
        'name': 'ERROR_TOO_MANY_SEMAPHORES'},
    101: {'description': 'The exclusive semaphore is owned by another process.',
        'name': 'ERROR_EXCL_SEM_ALREADY_OWNED'},
    102: {'description': 'The semaphore is set and cannot be closed.',
        'name': 'ERROR_SEM_IS_SET'},
    103: {'description': 'The semaphore cannot be set again.',
        'name': 'ERROR_TOO_MANY_SEM_REQUESTS'},
    104: {'description': 'Cannot request exclusive semaphores at interrupt time.',
        'name': 'ERROR_INVALID_AT_INTERRUPT_TIME'},
    105: {'description': 'The previous ownership of this semaphore has ended.',
        'name': 'ERROR_SEM_OWNER_DIED'},
    106: {'description': 'Insert the diskette for drive %1.',
        'name': 'ERROR_SEM_USER_LIMIT'},
    107: {'description': 'The program stopped because an alternate diskette was '
                        'not inserted.',
        'name': 'ERROR_DISK_CHANGE'},
    108: {'description': 'The disk is in use or locked by another process.',
        'name': 'ERROR_DRIVE_LOCKED'},
    109: {'description': 'The pipe has been ended.', 'name': 'ERROR_BROKEN_PIPE'},
    110: {'description': 'The system cannot open the device or file specified.',
        'name': 'ERROR_OPEN_FAILED'},
    111: {'description': 'The file name is too long.',
        'name': 'ERROR_BUFFER_OVERFLOW'},
    112: {'description': 'There is not enough space on the disk.',
        'name': 'ERROR_DISK_FULL'},
    113: {'description': 'No more internal file identifiers available.',
        'name': 'ERROR_NO_MORE_SEARCH_HANDLES'},
    114: {'description': 'The target internal file identifier is incorrect.',
        'name': 'ERROR_INVALID_TARGET_HANDLE'},
    117: {'description': 'The IOCTL call made by the application program is not '
                        'correct.',
        'name': 'ERROR_INVALID_CATEGORY'},
    118: {'description': 'The verify-on-write switch parameter value is not '
                        'correct.',
        'name': 'ERROR_INVALID_VERIFY_SWITCH'},
    119: {'description': 'The system does not support the command requested.',
        'name': 'ERROR_BAD_DRIVER_LEVEL'},
    120: {'description': 'This function is not supported on this system.',
        'name': 'ERROR_CALL_NOT_IMPLEMENTED'},
    121: {'description': 'The semaphore timeout period has expired.',
        'name': 'ERROR_SEM_TIMEOUT'},
    122: {'description': 'The data area passed to a system call is too small.',
        'name': 'ERROR_INSUFFICIENT_BUFFER'},
    123: {'description': 'The filename, directory name, or volume label syntax is '
                        'incorrect.',
        'name': 'ERROR_INVALID_NAME'},
    124: {'description': 'The system call level is not correct.',
        'name': 'ERROR_INVALID_LEVEL'},
    125: {'description': 'The disk has no volume label.',
        'name': 'ERROR_NO_VOLUME_LABEL'},
    126: {'description': 'The specified module could not be found.',
        'name': 'ERROR_MOD_NOT_FOUND'},
    127: {'description': 'The specified procedure could not be found.',
        'name': 'ERROR_PROC_NOT_FOUND'},
    128: {'description': 'There are no child processes to wait for.',
        'name': 'ERROR_WAIT_NO_CHILDREN'},
    129: {'description': 'The %1 application cannot be run in Win32 mode.',
        'name': 'ERROR_CHILD_NOT_COMPLETE'},
    130: {'description': 'Attempt to use a file handle to an open disk partition '
                        'for an operation other than raw disk I/O.',
        'name': 'ERROR_DIRECT_ACCESS_HANDLE'},
    131: {'description': 'An attempt was made to move the file pointer before the '
                        'beginning of the file.',
        'name': 'ERROR_NEGATIVE_SEEK'},
    132: {'description': 'The file pointer cannot be set on the specified device '
                        'or file.',
        'name': 'ERROR_SEEK_ON_DEVICE'},
    133: {'description': 'A JOIN or SUBST command cannot be used for a drive that '
                        'contains previously joined drives.',
        'name': 'ERROR_IS_JOIN_TARGET'},
    134: {'description': 'An attempt was made to use a JOIN or SUBST command on a '
                        'drive that has already been joined.',
        'name': 'ERROR_IS_JOINED'},
    135: {'description': 'An attempt was made to use a JOIN or SUBST command on a '
                        'drive that has already been substituted.',
        'name': 'ERROR_IS_SUBSTED'},
    136: {'description': 'The system tried to delete the JOIN of a drive that is '
                        'not joined.',
        'name': 'ERROR_NOT_JOINED'},
    137: {'description': 'The system tried to delete the substitution of a drive '
                        'that is not substituted.',
        'name': 'ERROR_NOT_SUBSTED'},
    138: {'description': 'The system tried to join a drive to a directory on a '
                        'joined drive.',
        'name': 'ERROR_JOIN_TO_JOIN'},
    139: {'description': 'The system tried to substitute a drive to a directory '
                        'on a substituted drive.',
        'name': 'ERROR_SUBST_TO_SUBST'},
    140: {'description': 'The system tried to join a drive to a directory on a '
                        'substituted drive.',
        'name': 'ERROR_JOIN_TO_SUBST'},
    141: {'description': 'The system tried to SUBST a drive to a directory on a '
                        'joined drive.',
        'name': 'ERROR_SUBST_TO_JOIN'},
    142: {'description': 'The system cannot perform a JOIN or SUBST at this time.',
        'name': 'ERROR_BUSY_DRIVE'},
    143: {'description': 'The system cannot join or substitute a drive to or for '
                        'a directory on the same drive.',
        'name': 'ERROR_SAME_DRIVE'},
    144: {'description': 'The directory is not a subdirectory of the root '
                        'directory.',
        'name': 'ERROR_DIR_NOT_ROOT'},
    145: {'description': 'The directory is not empty.',
        'name': 'ERROR_DIR_NOT_EMPTY'},
    146: {'description': 'The path specified is being used in a substitute.',
        'name': 'ERROR_IS_SUBST_PATH'},
    147: {'description': 'Not enough resources are available to process this '
                        'command.',
        'name': 'ERROR_IS_JOIN_PATH'},
    148: {'description': 'The path specified cannot be used at this time.',
        'name': 'ERROR_PATH_BUSY'},
    149: {'description': 'An attempt was made to join or substitute a drive for '
                        'which a directory on the drive is the target of a '
                        'previous substitute.',
        'name': 'ERROR_IS_SUBST_TARGET'},
    150: {'description': 'System trace information was not specified in your '
                        'CONFIG.SYS file, or tracing is disallowed.',
        'name': 'ERROR_SYSTEM_TRACE'},
    151: {'description': 'The number of specified semaphore events for '
                        'DosMuxSemWait is not correct.',
        'name': 'ERROR_INVALID_EVENT_COUNT'},
    152: {'description': 'DosMuxSemWait did not execute; too many semaphores are '
                        'already set.',
        'name': 'ERROR_TOO_MANY_MUXWAITERS'},
    153: {'description': 'The DosMuxSemWait list is not correct.',
        'name': 'ERROR_INVALID_LIST_FORMAT'},
    154: {'description': 'The volume label you entered exceeds the label '
                        'character limit of the target file system.',
        'name': 'ERROR_LABEL_TOO_LONG'},
    155: {'description': 'Cannot create another thread.',
        'name': 'ERROR_TOO_MANY_TCBS'},
    156: {'description': 'The recipient process has refused the signal.',
        'name': 'ERROR_SIGNAL_REFUSED'},
    157: {'description': 'The segment is already discarded and cannot be locked.',
        'name': 'ERROR_DISCARDED'},
    158: {'description': 'The segment is already unlocked.',
        'name': 'ERROR_NOT_LOCKED'},
    159: {'description': 'The address for the thread ID is not correct.',
        'name': 'ERROR_BAD_THREADID_ADDR'},
    160: {'description': 'One or more arguments are not correct.',
        'name': 'ERROR_BAD_ARGUMENTS'},
    161: {'description': 'The specified path is invalid.',
        'name': 'ERROR_BAD_PATHNAME'},
    162: {'description': 'A signal is already pending.',
        'name': 'ERROR_SIGNAL_PENDING'},
    164: {'description': 'No more threads can be created in the system.',
        'name': 'ERROR_MAX_THRDS_REACHED'},
    167: {'description': 'Unable to lock a region of a file.',
        'name': 'ERROR_LOCK_FAILED'},
    170: {'description': 'The requested resource is in use.',
        'name': 'ERROR_BUSY'},
    171: {'description': "Device's command support detection is in progress.",
        'name': 'ERROR_DEVICE_SUPPORT_IN_PROGRESS'},
    173: {'description': 'A lock request was not outstanding for the supplied '
                        'cancel region.',
        'name': 'ERROR_CANCEL_VIOLATION'},
    174: {'description': 'The file system does not support atomic changes to the '
                        'lock type.',
        'name': 'ERROR_ATOMIC_LOCKS_NOT_SUPPORTED'},
    180: {'description': 'The system detected a segment number that was not '
                        'correct.',
        'name': 'ERROR_INVALID_SEGMENT_NUMBER'},
    182: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_INVALID_ORDINAL'},
    183: {'description': 'Cannot create a file when that file already exists.',
        'name': 'ERROR_ALREADY_EXISTS'},
    186: {'description': 'The flag passed is not correct.',
        'name': 'ERROR_INVALID_FLAG_NUMBER'},
    187: {'description': 'The specified system semaphore name was not found.',
        'name': 'ERROR_SEM_NOT_FOUND'},
    188: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_INVALID_STARTING_CODESEG'},
    189: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_INVALID_STACKSEG'},
    190: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_INVALID_MODULETYPE'},
    191: {'description': 'Cannot run %1 in Win32 mode.',
        'name': 'ERROR_INVALID_EXE_SIGNATURE'},
    192: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_EXE_MARKED_INVALID'},
    193: {'description': '%1 is not a valid Win32 application.',
        'name': 'ERROR_BAD_EXE_FORMAT'},
    194: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_ITERATED_DATA_EXCEEDS_64k'},
    195: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_INVALID_MINALLOCSIZE'},
    196: {'description': 'The operating system cannot run this application '
                        'program.',
        'name': 'ERROR_DYNLINK_FROM_INVALID_RING'},
    197: {'description': 'The operating system is not presently configured to run '
                        'this application.',
        'name': 'ERROR_IOPL_NOT_ENABLED'},
    198: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_INVALID_SEGDPL'},
    199: {'description': 'The operating system cannot run this application '
                        'program.',
        'name': 'ERROR_AUTODATASEG_EXCEEDS_64k'},
    200: {'description': 'The code segment cannot be greater than or equal to '
                        '64K.',
        'name': 'ERROR_RING2SEG_MUST_BE_MOVABLE'},
    201: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_RELOC_CHAIN_XEEDS_SEGLIM'},
    202: {'description': 'The operating system cannot run %1.',
        'name': 'ERROR_INFLOOP_IN_RELOC_CHAIN'},
    203: {'description': 'The system could not find the environment option that '
                        'was entered.',
        'name': 'ERROR_ENVVAR_NOT_FOUND'},
    205: {'description': 'No process in the command subtree has a signal handler.',
        'name': 'ERROR_NO_SIGNAL_SENT'},
    206: {'description': 'The filename or extension is too long.',
        'name': 'ERROR_FILENAME_EXCED_RANGE'},
    207: {'description': 'The ring 2 stack is in use.',
        'name': 'ERROR_RING2_STACK_IN_USE'},
    208: {'description': 'The global filename characters, * or ?, are entered '
                        'incorrectly or too many global filename characters are '
                        'specified.',
        'name': 'ERROR_META_EXPANSION_TOO_LONG'},
    209: {'description': 'The signal being posted is not correct.',
        'name': 'ERROR_INVALID_SIGNAL_NUMBER'},
    210: {'description': 'The signal handler cannot be set.',
        'name': 'ERROR_THREAD_1_INACTIVE'},
    212: {'description': 'The segment is locked and cannot be reallocated.',
        'name': 'ERROR_LOCKED'},
    214: {'description': 'Too many dynamic-link modules are attached to this '
                        'program or dynamic-link module.',
        'name': 'ERROR_TOO_MANY_MODULES'},
    215: {'description': 'Cannot nest calls to LoadModule.',
        'name': 'ERROR_NESTING_NOT_ALLOWED'},
    216: {'description': 'This version of %1 is not compatible with the version '
                        "of Windows you're running. Check your computer's system "
                        'information and then contact the software publisher.',
        'name': 'ERROR_EXE_MACHINE_TYPE_MISMATCH'},
    217: {'description': 'The image file %1 is signed, unable to modify.',
        'name': 'ERROR_EXE_CANNOT_MODIFY_SIGNED_BINARY'},
    218: {'description': 'The image file %1 is strong signed, unable to modify.',
        'name': 'ERROR_EXE_CANNOT_MODIFY_STRONG_SIGNED_BINARY'},
    220: {'description': 'This file is checked out or locked for editing by '
                        'another user.',
        'name': 'ERROR_FILE_CHECKED_OUT'},
    221: {'description': 'The file must be checked out before saving changes.',
        'name': 'ERROR_CHECKOUT_REQUIRED'},
    222: {'description': 'The file type being saved or retrieved has been '
                        'blocked.',
        'name': 'ERROR_BAD_FILE_TYPE'},
    223: {'description': 'The file size exceeds the limit allowed and cannot be '
                        'saved.',
        'name': 'ERROR_FILE_TOO_LARGE'},
    224: {'description': 'Access Denied. Before opening files in this location, '
                        'you must first add the web site to your trusted sites '
                        'list, browse to the web site, and select the option to '
                        'login automatically.',
        'name': 'ERROR_FORMS_AUTH_REQUIRED'},
    225: {'description': 'Operation did not complete successfully because the '
                        'file contains a virus or potentially unwanted software.',
        'name': 'ERROR_VIRUS_INFECTED'},
    226: {'description': 'This file contains a virus or potentially unwanted '
                        'software and cannot be opened. Due to the nature of '
                        'this virus or potentially unwanted software, the file '
                        'has been removed from this location.',
        'name': 'ERROR_VIRUS_DELETED'},
    229: {'description': 'The pipe is local.', 'name': 'ERROR_PIPE_LOCAL'},
    230: {'description': 'The pipe state is invalid.', 'name': 'ERROR_BAD_PIPE'},
    231: {'description': 'All pipe instances are busy.',
        'name': 'ERROR_PIPE_BUSY'},
    232: {'description': 'The pipe is being closed.', 'name': 'ERROR_NO_DATA'},
    233: {'description': 'No process is on the other end of the pipe.',
        'name': 'ERROR_PIPE_NOT_CONNECTED'},
    234: {'description': 'More data is available.', 'name': 'ERROR_MORE_DATA'},
    240: {'description': 'The session was canceled.',
        'name': 'ERROR_VC_DISCONNECTED'},
    254: {'description': 'The specified extended attribute name was invalid.',
        'name': 'ERROR_INVALID_EA_NAME'},
    255: {'description': 'The extended attributes are inconsistent.',
        'name': 'ERROR_EA_LIST_INCONSISTENT'},
    258: {'description': 'The wait operation timed out.', 'name': 'WAIT_TIMEOUT'},
    259: {'description': 'No more data is available.',
        'name': 'ERROR_NO_MORE_ITEMS'},
    266: {'description': 'The copy functions cannot be used.',
        'name': 'ERROR_CANNOT_COPY'},
    267: {'description': 'The directory name is invalid.',
        'name': 'ERROR_DIRECTORY'},
    275: {'description': 'The extended attributes did not fit in the buffer.',
        'name': 'ERROR_EAS_DIDNT_FIT'},
    276: {'description': 'The extended attribute file on the mounted file system '
                        'is corrupt.',
        'name': 'ERROR_EA_FILE_CORRUPT'},
    277: {'description': 'The extended attribute table file is full.',
        'name': 'ERROR_EA_TABLE_FULL'},
    278: {'description': 'The specified extended attribute handle is invalid.',
        'name': 'ERROR_INVALID_EA_HANDLE'},
    282: {'description': 'The mounted file system does not support extended '
                        'attributes.',
        'name': 'ERROR_EAS_NOT_SUPPORTED'},
    288: {'description': 'Attempt to release mutex not owned by caller.',
        'name': 'ERROR_NOT_OWNER'},
    298: {'description': 'Too many posts were made to a semaphore.',
        'name': 'ERROR_TOO_MANY_POSTS'},
    299: {'description': 'Only part of a ReadProcessMemory or WriteProcessMemory '
                        'request was completed.',
        'name': 'ERROR_PARTIAL_COPY'},
    300: {'description': 'The oplock request is denied.',
        'name': 'ERROR_OPLOCK_NOT_GRANTED'},
    301: {'description': 'An invalid oplock acknowledgment was received by the '
                        'system.',
        'name': 'ERROR_INVALID_OPLOCK_PROTOCOL'},
    302: {'description': 'The volume is too fragmented to complete this '
                        'operation.',
        'name': 'ERROR_DISK_TOO_FRAGMENTED'},
    303: {'description': 'The file cannot be opened because it is in the process '
                        'of being deleted.',
        'name': 'ERROR_DELETE_PENDING'},
    304: {'description': 'Short name settings may not be changed on this volume '
                        'due to the global registry setting.',
        'name': 'ERROR_INCOMPATIBLE_WITH_GLOBAL_SHORT_NAME_REGISTRY_SETTING'},
    305: {'description': 'Short names are not enabled on this volume.',
        'name': 'ERROR_SHORT_NAMES_NOT_ENABLED_ON_VOLUME'},
    306: {'description': 'The security stream for the given volume is in an '
                        'inconsistent state. Please run CHKDSK on the volume.',
        'name': 'ERROR_SECURITY_STREAM_IS_INCONSISTENT'},
    307: {'description': 'A requested file lock operation cannot be processed due '
                        'to an invalid byte range.',
        'name': 'ERROR_INVALID_LOCK_RANGE'},
    308: {'description': 'The subsystem needed to support the image type is not '
                        'present.',
        'name': 'ERROR_IMAGE_SUBSYSTEM_NOT_PRESENT'},
    309: {'description': 'The specified file already has a notification GUID '
                        'associated with it.',
        'name': 'ERROR_NOTIFICATION_GUID_ALREADY_DEFINED'},
    310: {'description': 'An invalid exception handler routine has been detected.',
        'name': 'ERROR_INVALID_EXCEPTION_HANDLER'},
    311: {'description': 'Duplicate privileges were specified for the token.',
        'name': 'ERROR_DUPLICATE_PRIVILEGES'},
    312: {'description': 'No ranges for the specified operation were able to be '
                        'processed.',
        'name': 'ERROR_NO_RANGES_PROCESSED'},
    313: {'description': 'Operation is not allowed on a file system internal '
                        'file.',
        'name': 'ERROR_NOT_ALLOWED_ON_SYSTEM_FILE'},
    314: {'description': 'The physical resources of this disk have been '
                        'exhausted.',
        'name': 'ERROR_DISK_RESOURCES_EXHAUSTED'},
    315: {'description': 'The token representing the data is invalid.',
        'name': 'ERROR_INVALID_TOKEN'},
    316: {'description': 'The device does not support the command feature.',
        'name': 'ERROR_DEVICE_FEATURE_NOT_SUPPORTED'},
    317: {'description': 'The system cannot find message text for message number '
                        '0x%1 in the message file for %2.',
        'name': 'ERROR_MR_MID_NOT_FOUND'},
    318: {'description': 'The scope specified was not found.',
        'name': 'ERROR_SCOPE_NOT_FOUND'},
    319: {'description': 'The Central Access Policy specified is not defined on '
                        'the target machine.',
        'name': 'ERROR_UNDEFINED_SCOPE'},
    320: {'description': 'The Central Access Policy obtained from Active '
                        'Directory is invalid.',
        'name': 'ERROR_INVALID_CAP'},
    321: {'description': 'The device is unreachable.',
        'name': 'ERROR_DEVICE_UNREACHABLE'},
    322: {'description': 'The target device has insufficient resources to '
                        'complete the operation.',
        'name': 'ERROR_DEVICE_NO_RESOURCES'},
    323: {'description': 'A', 'name': 'ERROR_DATA_CHECKSUM_ERROR'},
    324: {'description': 'An attempt was made to modify both a KERNEL and normal '
                        'Extended Attribute (EA) in the same operation.',
        'name': 'ERROR_INTERMIXED_KERNEL_EA_OPERATION'},
    326: {'description': 'Device does not support file-level TRIM.',
        'name': 'ERROR_FILE_LEVEL_TRIM_NOT_SUPPORTED'},
    327: {'description': 'The command specified a data offset that does not align '
                        "to the device's granularity/alignment.",
        'name': 'ERROR_OFFSET_ALIGNMENT_VIOLATION'},
    328: {'description': 'The command specified an invalid field in its parameter '
                        'list.',
        'name': 'ERROR_INVALID_FIELD_IN_PARAMETER_LIST'},
    329: {'description': 'An operation is currently in progress with the device.',
        'name': 'ERROR_OPERATION_IN_PROGRESS'},
    330: {'description': 'An attempt was made to send down the command via an '
                        'invalid path to the target device.',
        'name': 'ERROR_BAD_DEVICE_PATH'},
    331: {'description': 'The command specified a number of descriptors that '
                        'exceeded the maximum supported by the device.',
        'name': 'ERROR_TOO_MANY_DESCRIPTORS'},
    332: {'description': 'Scrub is disabled on the specified file.',
        'name': 'ERROR_SCRUB_DATA_DISABLED'},
    333: {'description': 'The storage device does not provide redundancy.',
        'name': 'ERROR_NOT_REDUNDANT_STORAGE'},
    334: {'description': 'An operation is not supported on a resident file.',
        'name': 'ERROR_RESIDENT_FILE_NOT_SUPPORTED'},
    335: {'description': 'An operation is not supported on a compressed file.',
        'name': 'ERROR_COMPRESSED_FILE_NOT_SUPPORTED'},
    336: {'description': 'An operation is not supported on a directory.',
        'name': 'ERROR_DIRECTORY_NOT_SUPPORTED'},
    337: {'description': 'The specified copy of the requested data could not be '
                        'read.',
        'name': 'ERROR_NOT_READ_FROM_COPY'},
    350: {'description': 'No action was taken as a system reboot is required.',
        'name': 'ERROR_FAIL_NOACTION_REBOOT'},
    351: {'description': 'The shutdown operation failed.',
        'name': 'ERROR_FAIL_SHUTDOWN'},
    352: {'description': 'The restart operation failed.',
        'name': 'ERROR_FAIL_RESTART'},
    353: {'description': 'The maximum number of sessions has been reached.',
        'name': 'ERROR_MAX_SESSIONS_REACHED'},
    400: {'description': 'The thread is already in background processing mode.',
        'name': 'ERROR_THREAD_MODE_ALREADY_BACKGROUND'},
    401: {'description': 'The thread is not in background processing mode.',
        'name': 'ERROR_THREAD_MODE_NOT_BACKGROUND'},
    402: {'description': 'The process is already in background processing mode.',
        'name': 'ERROR_PROCESS_MODE_ALREADY_BACKGROUND'},
    403: {'description': 'The process is not in background processing mode.',
        'name': 'ERROR_PROCESS_MODE_NOT_BACKGROUND'},
    487: {'description': 'Attempt to access invalid address.',
        'name': 'ERROR_INVALID_ADDRESS'},
    1326: {'description': '', 'name': 'ERROR_LOGON_FAILURE'}
}