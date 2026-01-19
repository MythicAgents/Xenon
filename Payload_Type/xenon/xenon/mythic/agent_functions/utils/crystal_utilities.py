import os, logging, tempfile, asyncio
from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from .mythicrpc_utilities import *

logging.basicConfig(level=logging.INFO)

async def convert_dll_to_pic(file_id: str) -> bytes:
    """
    Convert DLL to PIC with Crystal Palace
    
    :param self: Description
    :param file_id: Mythic file UUID
    :return: Shellcode Mythic file UUID
    """
    
    # Directories and files
    cwd = os.getcwd()                                                                   # /root/Xenon/Payload_Type/xenon
    agent_code_path = os.path.join(cwd, "xenon", "agent_code")                          # /root/Xenon/Payload_Type/xenon/xenon/agent_code
    crystal_palace_path = os.path.join(agent_code_path, "Loader", "crystal-linker")     # /root/Xenon/Payload_Type/xenon/xenon/agent_code/Loader/crystal-linker
    post_ex_path = os.path.join(agent_code_path, "Loader", "post-ex")                   # /root/Xenon/Payload_Type/xenon/xenon/agent_code/Loader/post-ex

    # Get DLL bytes from Mythic
    dll_contents = await SendMythicRPCFileGetContent(MythicRPCFileGetContentMessage(AgentFileID=file_id))
    
    if not dll_contents.Success:
        raise Exception("[CRYSTAL] Failed to fetch find file from Mythic (ID: {})".format(file_id))
    
    # Temporarily write DLL to file
    fd, temppath = tempfile.mkstemp(suffix='.dll')
    logging.info(f"[CRYSTAL] Writing DLL to temporary file \"{temppath}\"")
    with os.fdopen(fd, 'wb') as tmp:
        tmp.write(dll_contents.Content)
    
    # Run Crystal Palace linker on DLL
    # ./link {post-ex}/loader.spec temppath out.x64.bin
    output_file = f"{post_ex_path}/out.x64.bin"
    command = f"./link {post_ex_path}/loader.spec {temppath} {output_file}"
    
    proc = await asyncio.create_subprocess_shell(command, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE, cwd=crystal_palace_path)
    stdout, stderr = await proc.communicate()
    stdout_err = ""
    if proc.returncode != 0:
        logging.error(f"Command failed with exit code {proc.returncode}")
        logging.error(f"[stderr]: {stderr.decode()}")
        stdout_err += f'[stderr]\n{stderr.decode()}' + "\n" + command
        raise Exception("Crystal palace linking failed: " + stdout_err)
    else:
        logging.info(f"[stdout]: {stdout.decode()}")
        stdout_err += f'\n[stdout]\n{stdout.decode()}\n'
        logging.info(f"[CRYSTAL] Linker converted DLL to PIC. Output written to {output_file}")


    with open(output_file, "rb") as f:
        pic_bytes = f.read()
    
    # Clean up files
    os.remove(temppath)
    os.remove(output_file)
    
    return pic_bytes