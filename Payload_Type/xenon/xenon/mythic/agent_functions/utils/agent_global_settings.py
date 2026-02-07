from mythic_container.MythicRPC import *
import os, asyncio, logging

logging.basicConfig(level=logging.INFO)

class ProcessInjectKit:
    ''' 
    Manage the custom BOF files operators can upload for Process Injection Kit
    '''
    def __init__(self, inject_spawn: str = "", inject_explicit: str = "", named_pipe_stub: bytes = b""):
        self._default_kit_path = os.path.join(os.getcwd(), "xenon", "agent_code", "Loader", "inject-kits")
        self._inject_spawn = inject_spawn
        self._inject_explicit = inject_explicit
        self._named_pipe_stub = named_pipe_stub

    # Getter for inject_spawn
    def get_inject_spawn(self) -> str:
        return self._inject_spawn

    # Setter for inject_spawn
    def set_inject_spawn(self, value: str):
        if not isinstance(value, str):
            raise TypeError("inject_spawn must be a string")
        self._inject_spawn = value

    # Getter for inject_explicit
    def get_inject_explicit(self) -> str:
        return self._inject_explicit

    # Setter for inject_explicit
    def set_inject_explicit(self, value: str):
        if not isinstance(value, str):
            raise TypeError("inject_explicit must be a string")
        self._inject_explicit = value

    # Build default kit
    async def build_default(self, taskId: str):
        """
        Compile default Process Injection Kit and set file UUIDs
        """
        command = "make"
        
        proc = await asyncio.create_subprocess_shell(command, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE, cwd=self._default_kit_path)
        stdout, stderr = await proc.communicate()
        stdout_err = ""
        if proc.returncode != 0:
            logging.error(f"Command failed with exit code {proc.returncode}")
            logging.error(f"[stderr]: {stderr.decode()}")
            stdout_err += f'[stderr]\n{stderr.decode()}' + "\n" + command
            raise Exception("Process inject kit failed to compile: " + stdout_err)
        else:
            logging.info(f"[stdout]: {stdout.decode()}")
            stdout_err += f'\n[stdout]\n{stdout.decode()}\n'
            logging.info(f"[PIK] Compiled default process injection kit")
        
        spawn_kit_filename = "inject_spawn.x64.o"
        explicit_kit_filename = "inject_explicit.x64.o"
        
        spawn_path = os.path.join(self._default_kit_path, "bin", spawn_kit_filename)
        explicit_path = os.path.join(self._default_kit_path, "bin", explicit_kit_filename)
        
        # Upload Spawn Kit
        with open(spawn_path, "rb") as f:
            spawn_bytes = f.read()
        upload_resp = await SendMythicRPCFileCreate(
            MythicRPCFileCreateMessage(
                TaskID=taskId,
                Filename=spawn_kit_filename,
                DeleteAfterFetch=False,
                FileContents=spawn_bytes,
        ))
        if upload_resp.Success:
            logging.info(f"[PIK] Successfully uploaded: {spawn_kit_filename}")
            self.set_inject_spawn(upload_resp.AgentFileId)
        else:
            raise Exception(f"[PIK] Failed to upload {spawn_kit_filename}: {upload_resp.Error}")
        
        # Upload Explicit Kit
        with open(explicit_path, "rb") as f:
            explicit_bytes = f.read()
        upload_resp = await SendMythicRPCFileCreate(
            MythicRPCFileCreateMessage(
                TaskID=taskId,
                Filename=explicit_kit_filename,
                DeleteAfterFetch=False,
                FileContents=explicit_bytes,
        ))
        if upload_resp.Success:
            logging.info(f"[PIK] Successfully uploaded: {explicit_kit_filename}")
            self.set_inject_explicit(upload_resp.AgentFileId)
        else:
            raise Exception(f"[PIK] Failed to upload {explicit_kit_filename}: {upload_resp.Error}")

# Global
PROCESS_INJECT_KIT = ProcessInjectKit()


