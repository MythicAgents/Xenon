/*
 * JobKill - cancel a queued/in-progress upload task.
 *
 * Scans xenonConfig->UploadQueue for the given task UUID.  If found, the
 * file handle is closed, the partial file is deleted (best-effort), the node
 * is unlinked from the queue, and an error response is sent to Mythic for the
 * target task so it appears as failed in the UI.
 *
 * The job_kill task itself always receives a PackageComplete reply describing
 * whether the target task was found and cancelled, or was not found.
 */

#include "Xenon.h"
#include "Tasks/JobKill.h"
#include "Tasks/Upload.h"
#include "Package.h"
#include "Parser.h"
#include "Debug.h"

#ifdef INCLUDE_CMD_JOB_KILL

VOID JobKill(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    SIZE_T sz        = 0;
    PCHAR  targetId  = NULL;

    UINT32 nbArg = ParserGetInt32(arguments);
    if (nbArg < 1)
    {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    targetId = ParserStringCopy(arguments, &sz);
    if (!targetId || !targetId[0])
    {
        if (targetId) LocalFree(targetId);
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    _dbg("[job_kill] Cancelling task %s", targetId);

    /* Walk the upload queue looking for a matching task UUID */
    PFILE_UPLOAD prev    = NULL;
    PFILE_UPLOAD current = xenonConfig->UploadQueue;

    while (current)
    {
        if (strncmp(current->TaskUuid, targetId, 36) == 0)
            break;
        prev    = current;
        current = (PFILE_UPLOAD)current->Next;
    }

    if (!current)
    {
        _dbg("[job_kill] Task %s not found in upload queue", targetId);
        PPackage data = PackageInit(0, FALSE);
        PackageAddFormatPrintf(data, FALSE,
            "Task %s was not found in the job queue.\n", targetId);
        PackageComplete(taskUuid, data);
        PackageDestroy(data);
        LocalFree(targetId);
        return;
    }

    /* Unlink from queue */
    if (prev)
        prev->Next = current->Next;
    else
        xenonConfig->UploadQueue = (PFILE_UPLOAD)current->Next;

    /* Close the file handle and delete the partial drop (best-effort) */
    if (current->hFile && current->hFile != INVALID_HANDLE_VALUE)
        CloseHandle(current->hFile);
    current->hFile = INVALID_HANDLE_VALUE;

    if (current->filepath[0])
        DeleteFileA(current->filepath);

    /* Report the target task as errored in Mythic */
    PackageError(current->TaskUuid, ERROR_OPERATION_ABORTED);

    /* Free the node (also closes LmLogonToken and LocalFrees all Lm* fields) */
    current->Next  = NULL;
    UploadFree(current);

    _dbg("[job_kill] Task %s cancelled successfully", targetId);

    PPackage data = PackageInit(0, FALSE);
    PackageAddFormatPrintf(data, FALSE,
        "Task %s was found and cancelled.\n", targetId);
    PackageComplete(taskUuid, data);
    PackageDestroy(data);

    LocalFree(targetId);
}

#endif // INCLUDE_CMD_JOB_KILL
