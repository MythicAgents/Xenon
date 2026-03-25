import os, logging, tempfile, asyncio
from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from .mythicrpc_utilities import *

logging.basicConfig(level=logging.INFO)

async def convert_postex_dll_to_pic(file_id: str, dll_arguments: str, dll_arguments_encoded: str = None, agent_uuid: int = None) -> bytes:
    """
    Convert DLL to PIC with Crystal Palace

    :param file_id: Mythic file UUID
    :param dll_arguments: Arguments to pass to the DLL
    :param dll_arguments_encoded: Encoded arguments to pass to the DLL
    :param agent_uuid: Optional agent UUID to look up agent's build config for custom postex loader
    :return: PIC bytes
    """
    # Directories and files
    cwd = os.getcwd()
    agent_code_path = os.path.join(cwd, "xenon", "agent_code")
    crystal_palace_path = os.path.join(agent_code_path, "Loader", "crystal-linker")
    default_post_ex_path = os.path.join(agent_code_path, "Loader", "post-ex")

    # Resolve which post-ex loader to use based on payload build config
    post_ex_path = default_post_ex_path
    if agent_uuid is not None:
        try:
            callback_resp = await SendMythicRPCCallbackSearch(MythicRPCCallbackSearchMessage(AgentCallbackID=agent_uuid))
            if callback_resp.Success:
                callback = callback_resp.Results[0]
                payload_build_uuid = callback.RegisteredPayloadUUID
                custom_postex_path = os.path.join(agent_code_path, "Loader", "postex_" + payload_build_uuid)
                if os.path.exists(custom_postex_path):
                    logging.info(f"[CRYSTAL] Using Custom Post-Ex DLL Loader (postex_{payload_build_uuid})")
                    post_ex_path = custom_postex_path
                else:
                    logging.info(f"[CRYSTAL] Using Default Post-Ex DLL Loader")
        except Exception as e:
            logging.warning(f"[CRYSTAL] Payload lookup failed, using default loader: {e}")

    # Get DLL bytes from Mythic
    dll_contents = await SendMythicRPCFileGetContent(MythicRPCFileGetContentMessage(AgentFileId=file_id))

    if not dll_contents.Success:
        raise Exception("[CRYSTAL] Failed to fetch find file from Mythic (ID: {})".format(file_id))

    # Temporarily write DLL to file
    fd, temppath = tempfile.mkstemp(suffix='.dll')
    logging.info(f"[CRYSTAL] Writing DLL to temporary file \"{temppath}\"")
    with os.fdopen(fd, 'wb') as tmp:
        tmp.write(dll_contents.Content)

    # Temporarily write Arguments to file
    fd, dll_arg_file = tempfile.mkstemp(suffix='.args')
    logging.info(f"[CRYSTAL] Writing DLL arguments to temporary file \"{dll_arg_file}\"")
    dll_arguments = base64.b64decode(dll_arguments_encoded) if dll_arguments_encoded else dll_arguments.encode("utf-8")
    with os.fdopen(fd, 'wb') as tmp:
        tmp.write(dll_arguments)
        tmp.write(b"\0")

    # ./link {post-ex}/loader.spec temppath out.x64.bin
    output_file = f"{post_ex_path}/out.x64.bin"
    command = f"./link {post_ex_path}/loader.spec {temppath} {output_file} %ARGFILE='{dll_arg_file}'"

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
    os.remove(dll_arg_file)
    os.remove(output_file)
    
    return pic_bytes
