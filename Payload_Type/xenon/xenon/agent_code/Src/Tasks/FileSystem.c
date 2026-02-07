#include "Tasks/FileSystem.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"

#ifdef INCLUDE_CMD_CD
VOID FileSystemCd(PCHAR taskUuid, PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);
    if (nbArg == 0)
    {
        return;
    }

    SIZE_T  size        = 0;
    PCHAR   inputPath   = ParserStringCopy(arguments, &size);

    _dbg("Using path %s ", inputPath);

    if (!SetCurrentDirectoryA(inputPath))
    {       
        DWORD error = GetLastError();
        _err("Could not change directory to %s : ERROR CODE %d", inputPath, error);
        PackageError(taskUuid, error);
        goto end;
    }
    
    // success
    PackageComplete(taskUuid, NULL);

end:
    // Cleanup
    LocalFree(inputPath);
}
#endif

#ifdef INCLUDE_CMD_PWD
VOID FileSystemPwd(PCHAR taskUuid, PPARSER arguments)
{
    char dir[2048];
    int length = GetCurrentDirectoryA(sizeof(dir), dir);
    if (length == 0)
    {
        DWORD error = GetLastError();
        PackageError(taskUuid, error);
        goto end;
    }
        
    // Response package
    PPackage data = PackageInit(0, FALSE);
    PackageAddString(data, dir, FALSE);

    // Success
    PackageComplete(taskUuid, data);

end:
    PackageDestroy(data);
}
#endif

#ifdef INCLUDE_CMD_MKDIR
VOID FileSystemMkdir(PCHAR taskUuid, PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);
    if (nbArg == 0)
    {
        return;
    }
    
    SIZE_T size = 0;
    PCHAR dirname = ParserStringCopy(arguments, &size);

    _dbg("Creating directory: \"%s\"", dirname);

    // Create the directory
    if (!CreateDirectoryA(dirname, NULL))
    {
        char *lasterror = GetLastErrorAsStringA();
        _err("Could not create directory %s : %s", dirname, lasterror);
        
        DWORD error = GetLastError();
        PackageError(taskUuid, error);

        goto end;
    }

    // success
    PackageComplete(taskUuid, NULL);

end:
    // Cleanup
    LocalFree(dirname);
}
#endif

#ifdef INCLUDE_CMD_CP
VOID FileSystemCopy(PCHAR taskUuid, PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);
    if (nbArg == 0)
    {
        return;
    }

    SIZE_T size     = 0;
    SIZE_T size2    = 0;
    PCHAR existingFileName = ParserStringCopy(arguments, &size);
    PCHAR newFileName = ParserStringCopy(arguments, &size2);

    _dbg("Copying file \"%s\" to \"%s\"", existingFileName, newFileName);

    // Copy the file
    if (!CopyFileA(existingFileName, newFileName, FALSE))
    {
        char *lastError = GetLastErrorAsStringA();
        _err("Copy failed: %s", lastError);

        DWORD error = GetLastError();
        PackageError(taskUuid, error);

        goto end;
    }

    // success
    PackageComplete(taskUuid, NULL);

end:;
    // Cleanup
    LocalFree(newFileName);
    LocalFree(existingFileName);
}
#endif

#ifdef INCLUDE_CMD_LS

#define MAX_FILENAME 0x4000
#define LS_SOURCE_DIRECTORY "\\*"

/* Helper */
static VOID LsSplitParentAndName(PCHAR pathNoStar, PCHAR outParent, SIZE_T parentSize, PCHAR outName, SIZE_T nameSize)
{
    SIZE_T len = strlen(pathNoStar);
    if (len == 0) { outParent[0] = outName[0] = '\0'; return; }
    PCHAR lastSlash = strrchr(pathNoStar, '\\');
    if (!lastSlash || lastSlash == pathNoStar)
    {
        outParent[0] = '\0';
        strncpy_s(outName, nameSize, pathNoStar, _TRUNCATE);
        return;
    }
    /* If path ends with backslash (e.g. "C:\") treat as root of that drive: parent = path minus trailing \, name = "" */
    if (*(lastSlash + 1) == '\0')
    {
        SIZE_T parentLen = (SIZE_T)(lastSlash - pathNoStar);
        if (parentLen >= parentSize) parentLen = parentSize - 1;
        memcpy(outParent, pathNoStar, parentLen);
        outParent[parentLen] = '\0';
        outName[0] = '\0';
        _dbg("Parent: %s, Name: (root)", outParent);
        return;
    }
    SIZE_T parentLen = (SIZE_T)(lastSlash - pathNoStar + 1);
    if (parentLen >= parentSize) parentLen = parentSize - 1;
    memcpy(outParent, pathNoStar, parentLen);
    outParent[parentLen] = '\0';
    strncpy_s(outName, nameSize, lastSlash + 1, _TRUNCATE);

    _dbg("Parent: %s, Name: %s", outParent, outName);
}

/* Convert FILETIME to UINT64 (100-nanosecond intervals since 1601). */

// TODO: Can I move this to the translation container utils

static ULONGLONG FileTimeToUint64(FILETIME const *ft)
{
    ULARGE_INTEGER u;
    u.LowPart = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    return u.QuadPart;
}

VOID FileSystemList(PCHAR taskUuid, PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);
    if (nbArg != 3)
    {
        return;
    }

    SIZE_T flen = 0;
    SIZE_T hlen = 0;
    PCHAR File        = ParserStringCopy(arguments, &flen);                    // allocates
    BYTE  FileBrowser = (nbArg >= 2) ? ParserGetByte(arguments) : 0;
    PCHAR Host        = ParserGetString(arguments, &hlen);

    /* Construct full file path (with hostname) */
    char filename[MAX_FILENAME];
    if ( Host && hlen > 0 )
    {
        snprintf(filename, sizeof(filename), "\\\\%s\\%s", Host, File);
    }
    else
    {
        strcpy_s(filename, sizeof(filename), File);
    }

    /* Current Directory */
    if ( flen == 0 )
    {
        GetCurrentDirectoryA(sizeof(filename), filename);
        strncat_s(filename, sizeof(filename), LS_SOURCE_DIRECTORY, _TRUNCATE);
    }
    else  /* Add '\*' to end. C:\Windows\* */
    {
        SIZE_T pathLen = strlen(filename);
        if (pathLen > 0 && filename[pathLen - 1] != '\\')
            filename[pathLen++] = '\\';
        filename[pathLen++] = '*';
        filename[pathLen] = '\0';
    }

    WIN32_FIND_DATAA findData;
    HANDLE firstFile = FindFirstFileA(filename, &findData);
    if ( firstFile == INVALID_HANDLE_VALUE )
    {
        DWORD error = GetLastError();
        _err("Could not open %s : ERROR CODE %d", filename, error);
        PackageError(taskUuid, error);
        LocalFree(File);
        return;
    }

    /* Normal output for CLI (ls) */
    if ( !FileBrowser )
    {
        PPackage temp = PackageInit(0, FALSE);
        PackageAddFormatPrintf(temp, FALSE, "%s\n", filename);
        SYSTEMTIME systemTime, localTime;
        do
        {
            FileTimeToSystemTime(&findData.ftLastWriteTime, &systemTime);
            SystemTimeToTzSpecificLocalTime(NULL, &systemTime, &localTime);
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                PackageAddFormatPrintf(
                    temp, 
                    FALSE, 
                    "D\t0\t%02d/%02d/%02d %02d:%02d:%02d\t%s\n",
                    localTime.wMonth, 
                    localTime.wDay, 
                    localTime.wYear,
                    localTime.wHour, 
                    localTime.wMinute, 
                    localTime.wSecond, 
                    findData.cFileName
                );
            }
            else
            {
                PackageAddFormatPrintf(
                    temp, 
                    FALSE, 
                    "F\t%I64d\t%02d/%02d/%02d %02d:%02d:%02d\t%s\n",
                    ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow,
                    localTime.wMonth, 
                    localTime.wDay, 
                    localTime.wYear,
                    localTime.wHour, 
                    localTime.wMinute, 
                    localTime.wSecond, 
                    findData.cFileName
                );
            }
        } while (FindNextFileA(firstFile, &findData));
        
        FindClose(firstFile);
        
        /* Send response */
        PackageComplete(taskUuid, temp);
        
        /* Clean up and finish */
        PackageDestroy(temp);
        LocalFree(File);
        return;
    }
    else  /* File Browser UI output format */
    {
        char pathNoStar[MAX_FILENAME];
        SIZE_T starOff = strlen(filename);
        if (starOff >= 2 && filename[starOff - 1] == '*' && filename[starOff - 2] == '\\')
        {
            starOff -= 2;
            memcpy(pathNoStar, filename, starOff);
            pathNoStar[starOff] = '\0';
        }
        else
        {
            strcpy_s(pathNoStar, sizeof(pathNoStar), filename);
        }

        char parentPath[MAX_FILENAME];
        char folderName[MAX_FILENAME];
        LsSplitParentAndName(pathNoStar, parentPath, sizeof(parentPath), folderName, sizeof(folderName));

        WIN32_FILE_ATTRIBUTE_DATA attrData;
        ULONGLONG accessTime = 0;
        ULONGLONG modifyTime = 0;
        if (GetFileAttributesExA(pathNoStar, GetFileExInfoStandard, &attrData))
        {
            accessTime = FileTimeToUint64(&attrData.ftLastAccessTime);
            modifyTime = FileTimeToUint64(&attrData.ftLastWriteTime);
        }

        /* Create FILE_BROWSER Package */
        PPackage data = PackageInit(0, FALSE);
        PackageAddByte(data, FILE_BROWSER);
        PackageAddString(data, taskUuid, FALSE);
        PackageAddByte(data, TASK_COMPLETE);

        /* Data for Parent File (folder) */
        if (parentPath[0] == '\0') PackageAddInt32(data, 0); else PackageAddString(data, parentPath, TRUE); // CHAR:    Parent Path (empty string is length 0)
        PackageAddString(data, folderName, TRUE);       // CHAR:    Folder name
        PackageAddByte(data, 0);                        // BYTE:    Is Folder
        PackageAddInt64(data, 0);                       // INT64:   Size
        PackageAddInt64(data, accessTime);              // INT64:   Access time 
        PackageAddInt64(data, modifyTime);              // INT64:   Modify time
        PackageAddByte(data, 1);                        // BYTE:    Success

        /* Data for each file in dir */
        do
        {
            if ( strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0 ) {
                continue;
            }
            PackageAddString(data, findData.cFileName, TRUE);                                               // CHAR:    File name
            PackageAddByte(data, (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1);           // BYTE:    Is file
            PackageAddInt64(data, ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow);       // INT64:   Size
            PackageAddInt64(data, FileTimeToUint64(&findData.ftLastAccessTime));                            // INT64:   Access time
            PackageAddInt64(data, FileTimeToUint64(&findData.ftLastWriteTime));                             // INT64:   Modify time
        
        } while (FindNextFileA(firstFile, &findData));

        FindClose(firstFile);

        /* Send response */
        PackageQueue(data);

        /* Clean up and finish */
        LocalFree(File);
    }
}
#endif

#ifdef INCLUDE_CMD_RM
BOOL FilesystemIsDirectory(char *filename)
{
    return GetFileAttributesA(filename) & FILE_ATTRIBUTE_DIRECTORY;
}

VOID FilesystemRemoveRecursiveCallback(const char *a1, const char *a2, BOOL isDirectory)
{
    char *lpPathName = (char *)malloc(0x4000);
    _snprintf(lpPathName, 0x4000, "%s\\%s", a1, a2);
    if (isDirectory)
        RemoveDirectoryA(lpPathName);
    else
        DeleteFileA(lpPathName);
    free(lpPathName);
}

VOID FilesystemFindAndProcess(char *filename, WIN32_FIND_DATAA *findData)
{
#define MAX_FILENAME 0x8000
    char *lpFileName;

    lpFileName = malloc(MAX_FILENAME);
    snprintf(lpFileName, MAX_FILENAME, "%s\\*", filename);
    LPWIN32_FIND_DATAA lpCurrentFindFileData = findData;
    HANDLE hFindFile = FindFirstFileA(lpFileName, lpCurrentFindFileData);
    free(lpFileName);

    if (hFindFile == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (lpCurrentFindFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (strcmp(lpCurrentFindFileData->cFileName, ".") && strcmp(lpCurrentFindFileData->cFileName, ".."))
            {
                char *lpFileNameInternal = malloc(MAX_FILENAME);
                snprintf(lpFileNameInternal, MAX_FILENAME, "%s", lpCurrentFindFileData->cFileName);

                lpFileName = malloc(MAX_FILENAME);
                snprintf(lpFileName, MAX_FILENAME, "%s\\%s", filename, findData->cFileName);
                FilesystemFindAndProcess(lpFileName, findData);
                free(lpFileName);

                FilesystemRemoveRecursiveCallback(filename, lpFileNameInternal, TRUE);
                free(lpFileNameInternal);
            }

            lpCurrentFindFileData = findData;
        }
        else
        {
            FilesystemRemoveRecursiveCallback(filename, lpCurrentFindFileData->cFileName, FALSE);
        }
    } while (FindNextFileA(hFindFile, lpCurrentFindFileData));
    FindClose(hFindFile);
}

VOID FilesystemRemoveDirectoryChildren(char *filepath)
{
    WIN32_FIND_DATAA findData;

    FilesystemFindAndProcess(
        filepath,
        &findData);
}

VOID FileSystemRemove(PCHAR taskUuid, PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);

    if (nbArg == 0)
        return;

    SIZE_T size = 0;
    PCHAR filepath = ParserStringCopy(arguments, &size);

    if (FilesystemIsDirectory(filepath))
    {
        FilesystemRemoveDirectoryChildren(filepath);
        RemoveDirectoryA(filepath);
    }
    else
    {
        DeleteFileA(filepath);
    }

    // success
    PackageComplete(taskUuid, NULL);

end:;
    // Cleanup
    LocalFree(filepath);
}
#endif
