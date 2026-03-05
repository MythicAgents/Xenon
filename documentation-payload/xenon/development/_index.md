+++
title = "Development"
chapter = false
weight = 50
pre = "<b>5. </b>"
+++

## Development

This page describes how to add new agent functionality and commands to Xenon. Adding a command requires work in two places: the **Mythic Python layer** (operator-facing definition) and the **C agent layer** (on-implant execution). Both sides must be in sync.

---

### Architecture Overview

```
Operator (Mythic UI)
        |
        v
 [Python CommandBase]       ← defines parameters, packs arguments, processes results
        |
        v
 [Translator Container]     ← serializes task data into binary TLV format
        |
        v
 [C Agent – TaskDispatch]   ← routes by command ID, calls the handler function
        |
        v
 [Tasks/*.c handler]        ← runs the logic, builds a Package, calls PackageComplete
```

Tasks flow top-to-bottom; results flow back up the same path.

---

### Part 1 — Python Side (Mythic Command Definition)

Every command needs a `.py` file inside:

```
Payload_Type/xenon/xenon/mythic/agent_functions/
```

The file is auto-discovered by `__init__.py` via glob, so no registration is needed — just drop it in.

#### 1.1 File Structure

A command file always contains two classes:

| Class | Purpose |
|---|---|
| `<Name>Arguments(TaskArguments)` | Declares and parses operator-supplied parameters |
| `<Name>Command(CommandBase)` | Declares metadata and implements tasking/response hooks |

#### 1.2 `TaskArguments` — Declaring Parameters

```python
from mythic_container.MythicCommandBase import *

class MyCommandArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="target_path",
                cli_name="Path",
                display_name="Target Path",
                type=ParameterType.String,
                description="Path to operate on.",
                parameter_group_info=[
                    ParameterGroupInfo(required=True, group_name="Default", ui_position=1)
                ]
            ),
        ]

    # Called when the command is typed in the CLI
    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a path")
        self.add_arg("target_path", self.command_line)

    # Called when the command is submitted via the modal UI
    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)
```

**Supported `ParameterType` values:** `String`, `Boolean`, `Integer`, `File`, `ChooseOne`, `Array`, `TypedArray`, `Number`.

#### 1.3 `CommandBase` — Command Metadata

```python
class MyCommand(CommandBase):
    cmd = "my_command"          # Must match the INCLUDE_CMD_ macro name (lowercase)
    needs_admin = False
    help_cmd = "my_command <path>"
    description = "Does something useful."
    version = 1
    author = "@you"
    argument_class = MyCommandArguments
    attackmapping = ["T1083"]   # MITRE ATT&CK technique IDs
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[SupportedOS.Windows],
        suggested_command=False,
    )
```

> **Important:** The value of `cmd` is used by the builder to generate the `#define INCLUDE_CMD_<CMD.upper()>` preprocessor macro that gates C-side compilation. For example, `cmd = "my_command"` produces `#define INCLUDE_CMD_MY_COMMAND`.

#### 1.4 `create_go_tasking` — Pre-flight Hook

This async method runs on the Mythic server **before** the task is sent to the agent. Use it to validate inputs, enrich arguments, call Mythic RPCs, or set the display string shown in the UI.

```python
async def create_go_tasking(
    self, taskData: PTTaskMessageAllData
) -> PTTaskCreateTaskingMessageResponse:

    response = PTTaskCreateTaskingMessageResponse(
        TaskID=taskData.Task.ID,
        Success=True,
    )
    # Show a useful summary in the task list
    response.DisplayParams = taskData.args.get_arg("target_path")
    return response
```

To abort a task, set `Success=False` and populate `Error`:

```python
response.Success = False
response.Error = "Path cannot be empty"
return response
```

#### 1.5 `process_response` — Response Hook

Called by Mythic when the agent returns data for this task. For most commands the default pass-through is sufficient, but you can parse structured data and create artifacts, file downloads, etc. here.

```python
async def process_response(
    self, task: PTTaskMessageAllData, response: any
) -> PTTaskProcessResponseMessageResponse:
    resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
    return resp
```

#### 1.6 Minimal Example (`example.py`)

The file `example.py` ships as a no-argument template:

```python
class ExampleArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = []

    async def parse_arguments(self):
        if len(self.command_line) > 0:
            raise Exception("example command takes no parameters.")

class ExampleCommand(CommandBase):
    cmd = "example"
    needs_admin = False
    help_cmd = "example"
    description = "This is an example command"
    version = 1
    author = "@c0rnbread"
    argument_class = ExampleArguments
    attributes = CommandAttributes(builtin=False, supported_os=[SupportedOS.Windows])
    attackmapping = []

    async def create_go_tasking(self, taskData):
        return PTTaskCreateTaskingMessageResponse(TaskID=taskData.Task.ID, Success=True)

    async def process_response(self, task, response):
        return PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
```

---

### Part 2 — C Agent Side (Command Implementation)

#### 2.1 Assign a Command ID

Open `Payload_Type/xenon/xenon/agent_code/Include/Task.h` and add a `#define` for the new command in the `// Commands` section. Pick an unused hex value:

```c
// Commands
// ...existing defines...
#define MY_COMMAND_CMD   0x62   // pick an unused value
```

#### 2.2 Add the `INCLUDE_CMD_` Guard to `Task.c`

Open `Payload_Type/xenon/xenon/agent_code/Src/Task.c` and add a `case` block inside `TaskDispatch()`:

```c
#ifdef INCLUDE_CMD_MY_COMMAND
    case MY_COMMAND_CMD:
    {
        _dbg("MY_COMMAND_CMD was called");
        MyCommandHandler(taskUuid, taskParser);
        return;
    }
#endif
```

The `#ifdef` gate is populated at build time by the builder from the operator's command selection; commands not selected by the operator are excluded from the binary.

#### 2.3 Declare the Handler in a Header

Either add to an existing `Include/Tasks/*.h` (e.g. `FileSystem.h` for file-related commands) or create a new one:

```c
// Include/Tasks/MyCategory.h
#pragma once
#ifndef MY_CATEGORY_H
#define MY_CATEGORY_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_MY_COMMAND
VOID MyCommandHandler(PCHAR taskUuid, PPARSER arguments);
#endif

#endif // MY_CATEGORY_H
```

Include the header in `Src/Task.c`:

```c
#include "Tasks/MyCategory.h"
```

#### 2.4 Implement the Handler in C

Create `Src/Tasks/MyCategory.c` (or add to an existing file):

```c
#include "Tasks/MyCategory.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"

#ifdef INCLUDE_CMD_MY_COMMAND
VOID MyCommandHandler(PCHAR taskUuid, PPARSER arguments)
{
    // 1. Read argument count (always first)
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    if (nbArg == 0)
        return;

    // 2. Read each argument in the order packed by the translator
    SIZE_T size     = 0;
    PCHAR  target   = ParserStringCopy(arguments, &size);  // allocates; must free

    // 3. Do work
    // ...

    // 4a. Success — send output back
    PPackage data = PackageInit(0, FALSE);
    PackageAddFormatPrintf(data, FALSE, "Operated on: %s\n", target);
    PackageComplete(taskUuid, data);

    // 5. Cleanup
    PackageDestroy(data);
    LocalFree(target);
    return;

    // 4b. OR: success with no output
    // PackageComplete(taskUuid, NULL);

    // 4c. OR: failure
    // DWORD error = GetLastError();
    // PackageError(taskUuid, error);
}
#endif
```

#### 2.5 Parser API — Reading Arguments

Arguments arrive pre-packed by the translator in the order they appear in `self.args`. Read them in exactly the same order:

| Function | Returns | Use for |
|---|---|---|
| `ParserGetByte(p)` | `BYTE` | single byte |
| `ParserGetInt32(p)` | `UINT32` | 32-bit integer / arg count |
| `ParserGetInt64(p)` | `UINT64` | 64-bit integer |
| `ParserGetString(p, &size)` | `PCHAR` (no alloc) | string (pointer into buffer) |
| `ParserStringCopy(p, &size)` | `PCHAR` (heap alloc) | string (caller must `LocalFree`) |
| `ParserGetBytes(p, &size)` | `PBYTE` | raw byte blob |

The **first** `ParserGetInt32` call always reads the argument count.

#### 2.6 Package API — Building a Response

| Function | Purpose |
|---|---|
| `PackageInit(0, FALSE)` | Allocate a new output package |
| `PackageAddString(pkg, str, FALSE)` | Append a null-terminated string |
| `PackageAddInt32(pkg, value)` | Append a 32-bit integer |
| `PackageAddBytes(pkg, buf, len, FALSE)` | Append raw bytes |
| `PackageAddFormatPrintf(pkg, FALSE, fmt, ...)` | Append printf-formatted text |
| `PackageComplete(uuid, pkg)` | Mark task done and enqueue result (`pkg` may be `NULL`) |
| `PackageUpdate(uuid, pkg)` | Send an intermediate update without marking done |
| `PackageError(uuid, errorCode)` | Mark task failed with a Win32 error code |
| `PackageDestroy(pkg)` | Free the package after sending |

#### 2.7 Add to the Makefile

If you created a new `.c` file, add it to the source list in `agent_code/Makefile`:

```makefile
SRCS += Src/Tasks/MyCategory.c
```

---

### Part 3 — End-to-End Data Flow

```
1. Operator submits task in Mythic UI
       |
       v
2. create_go_tasking() runs → validates args, sets DisplayParams
       |
       v
3. Translator packs arguments into TLV binary:
       [UINT32 arg_count] [UINT32 size][bytes] [UINT32 size][bytes] ...
       |
       v
4. Agent receives GET_TASKING response, calls TaskDispatch(cmd_id, uuid, parser)
       |
       v
5. Handler reads args with Parser API, executes logic
       |
       v
6. Handler calls PackageComplete(uuid, package) → enqueued in POST_RESPONSE
       |
       v
7. process_response() on Mythic server receives the raw output
```
