<div align="center">
  <img width="300px" src="images/v1-transparent.png" />
  <h1>Xenon</h1>
  <br/>

  <p><i>Xenon is a Cobalt Strike-like Windows agent for Mythic, created by <a href="https://github.com/nickswink">@c0rnbread</a>.</i></p>
  <br />

  <img src="images/1.png" width="90%" /><br />
  <img src="images/2.png" width="90%" /><br />
</div>

> :warning: Xenon is in an early state of release. It is not opsec safe and could contain memory issues causing crashes. Test thoroughly if planning to use in a live environment.


### OPSEC Disclaimer
Xenon makes no claims about evasion. The default configuration will not be OPSEC safe. The goal for Xenon is to allow the operator to customize features in order to accomplish their goals.


## Quick Start
Installing Xenon on an already existing Mythic server is very easy. If you do not have a Mythic server set up yet, to do that go to [Mythic project page](https://github.com/its-a-feature/Mythic/). 

For further customizations and evasion refer to the [Wiki](https://github.com/MythicAgents/Xenon/wiki).

From the Mythic install directory, use the following command to install Xenon as the **root** user:

```
./mythic-cli install github https://github.com/MythicAgents/Xenon.git
```

From the Mythic install directory, use the following command to install Xenon as a **non-root** user:

```
sudo -E ./mythic-cli install github https://github.com/MythicAgents/Xenon.git
```

## Features
- Modular command inclusion
- Malleable C2 Profiles
- Supported comms: [httpx](https://github.com/MythicC2Profiles/httpx), [smb](https://github.com/MythicC2Profiles/smb), [tcp](https://github.com/MythicC2Profiles/tcp)
- Uses [forge](https://github.com/MythicAgents/forge) for BOF modules and SharpCollections
- User-Defined Reflective Dll Loaders (based on Crystal Palace)
- Compatible with CS Process Inject Kits


## Supported Commands

| Command         | Usage                                               | Description |
|----------------|-----------------------------------------------------|-------------|
| `pwd`          | `pwd`                                               | Show present working directory. |
| `ls`           | `ls [path]`                                    | List directory information for `<directory>`. |
| `cd`           | `cd <directory>`                           | Change working directory. |
| `cp`           | `cp <source file> <destination file>`             | Copy a file to a new destination. |
| `rm`           | `rm <path\|file>`                     | Remove a directory or file. |
| `mkdir`        | `mkdir <path>`                            | Create a new directory. |
| `getuid`       | `getuid`                                            | Get the current identity. |
| `make_token`   | `make_token <DOMAIN> <username> <password> [LOGON_TYPE]` | Create a token and impersonate it using plaintext credentials. |
| `steal_token`  | `steal_token <pid>`                                 | Steal and impersonate the token of a target process. |
| `rev2self`     | `rev2self`                                          | Revert identity to the original process's token. |
| `ps`           | `ps`                                                | List host processes. |
| `shell`        | `shell <command>`                                   | Runs `{command}` in a terminal. |
| `sleep`        | `sleep <seconds> [jitter]`                          | Change sleep timer and jitter. |
| `inline_execute` | `inline_execute -BOF [COFF.o] [-Arguments [optional arguments]]` | Execute a Beacon Object File in the current process thread and see output. **Warning:** Incorrect argument types can crash the Agent process. |
| `inline_execute_assembly` | `inline_execute_assembly -Assembly [file] [-Arguments [assembly args] [--patchexit] [--amsi] [--etw]]` | Execute a .NET Assembly in the current process using @EricEsquivel's BOF "Inline-EA" (e.g., inline_execute_assembly -Assembly SharpUp.exe -Arguments "audit" --patchexit --amsi --etw) |
| `execute_assembly` | `execute_assembly -Assembly [SharpUp.exe] [-Arguments [assembly arguments]]` | Execute a .NET Assembly in a remote processes and retrieve the output. |
| `execute_dll` | `execute_dll -File [mimikatz.x64.dll]` | Execute a Dynamic Link Library as PIC. (e.g., execute_dll -File mimikatz.x64.dll) |
| `spawnto` | `spawnto -path [C:\Windows\System32\svchost.exe]` | Set the full path of the process to use for spawn & inject commands. |
| `powerchell` | `powerchell -Command <command>` | Execute PowerShell script using PowerChell post-ex DLL. |
| `powershell_import` | `powershell_import -File [script.ps1] \| --clear` | Import PowerShell script to cache. |
| `download`     | `download -path <file path>`                           | Download a file off the target system (supports UNC path). |
| `upload`       | `upload (modal)`                                            | Upload a file to the target machine by selecting a file from your computer. |
| `status`         | `status`                                              | List C2 connection hosts and their status. |
| `link`           | `link <target> [<named pipe>\|<tcp_port>]`                          | Connect to an SMB/TCP Link Agent. |
| `unlink`         | `unlink <Display Id>`                                 | Disconnect from an SMB/TCP Link Agent. |
| `socks` | `socks <start/stop> <port number>` | Enable SOCKS 5 compliant proxy to send data to the target network. |
| `register_process_inject_kit`       | `register_process_inject_kit (pops modal)`                                            | Register a custom BOF to use for process injection (CS compatible). See documentation for requirements. |
| `exit`         | `exit`                                              | Task the implant to exit. |

---

### Forge
Forge is a command augmentation container that I highly recommend you use for extending Xenon's capabilities.
It includes support out of the box for:

* @Flangvik's [SharpCollection](https://github.com/Flangvik/SharpCollection)
* Sliver's [Armory](https://github.com/sliverarmory/armory)

To use forge with Xenon you just have to install the container:
```
sudo -E ./mythic-cli install github https://github.com/MythicAgents/forge.git
```

Then just "enable" the commands by checking the icon ✅ from within your callbacks!

```
forge_collections -collectionName SharpCollection

forge_collections -collectionName SliverArmory
```

#### SharpCollection Assemblies

![SharpCollection Forge 1](images/forge-sharpcollection-1.png)

![SharpCollection Forge 2](images/forge-sharpcollection-2.png)

#### Sliver Armory BOFs

![Sliver Armory Forge 1](images/forge-sliverarmory-1.png)

![Sliver Armory Forge 2](images/forge-sliverarmory-2.png)



### Post-Ex Commands
These are post-ex commands that follow the classic **fork & run** style injection. They are implemented as DLLs turned to PIC with Crystal Palace, with the exception of `mimikatz`.

| Command                  | Usage                                                         | Description |
|--------------------------|---------------------------------------------------------------|-------------|
| `mimikatz`          | `mimikatz [args]`                                               | Execute mimikatz in a remote process. |
| `execute_assembly` | `execute_assembly -Assembly [SharpUp.exe] [-Arguments [assembly arguments]]` | Execute a .NET Assembly in a remote processes and retrieve the output. |
| `powerchell` | `powerchell -Command <command>` | Execute PowerShell script using PowerChell post-ex DLL. |


## Supported C2 Profiles

### [HTTPX Profile](https://github.com/MythicC2Profiles/httpx)

Xenon currently supports these features of the HTTPX profile:

* Callback Domains (array of values)
* Domain Rotation (fail-over, round-robin, random)
* Domain Fallback Threshold (for fail-over how many failed attempts before moving to the next)
* Callback Jitter and Sleep intervals
* Agent Message and Server Response configurations provided via JSON or TOML files at Build time that offer:
  * Message location in cookies, headers, query parameters, or body
  * Message transforms with base64, base64url, append, prepend, xor, netbios/netbiosu
  * Custom Client/Server headers
  * Custom Client query parameters

See the configuration guide on the [Wiki](https://github.com/MythicAgents/Xenon/wiki/Setup-Malleable-C2-Traffic-with-Httpx).


### [SMB Profile](https://github.com/MythicC2Profiles/smb)
Xenon agents can be generated with the SMB comms profile to link agents in a peer-to-peer way.

### [TCP Profile](https://github.com/MythicC2Profiles/tcp)
Xenon agents can be generated with the TCP comms profile to link agents in a peer-to-peer way.

## Roadmap
If you have suggestions/requests open an issue or you can message me on discord.

### Features
- [x] Socks5 proxy
- [x] Support File Browser UI
- [x] `powerchell` command
- [ ] Mythic features (process browser, TTPs)
- [ ] Support dns external transport

### Bugs
- [X] Work on memory issues (duplicate buffers etc)
- [X] Fix initial install files not found
- [x] Random named pipes per payload generation
- [ ] Weirdness with File Browser UI (remote hosts, etc)
- [ ] `execute_assembly` can cause PIPE_BUSY if doesnt exit properly
- [ ] Issues executing BOFs compiled with MSVC

## Contributors
Special thanks to all contributors who help improve this project.

- **@c0rnbread** — Author & Maintainer
- **@dstepanov** — TCP Transport support
- **vnp-dev**

If you would like to contribute to the project, please work off of the **next version branch** (named like "v1.2.3") as merges will go into that.

## Credits

I referenced and copied code from a bunch of different projects in the making of this project. If I directly copied code or only made slight modifications, I tried to add detailed references in the comments. Hopefully I didn't miss anything and piss someone off. 

- https://github.com/Red-Team-SNCF/ceos
- https://github.com/MythicAgents/Apollo
- https://github.com/MythicAgents/Athena
- https://github.com/kyxiaxiang/Beacon_Source
- https://github.com/HavocFramework/Havoc/tree/main/payloads/Demon
- https://github.com/Ap3x/COFF-Loader
- https://github.com/kokke/tiny-AES-c
