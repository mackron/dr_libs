// Public Domain. See "unlicense" statement at the end of this file.

#include "easy_util.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>     // For memmove()

void easyutil_parse_key_value_pairs(key_value_read_proc onRead, key_value_pair_proc onPair, key_value_error_proc onError, void* pUserData)
{
    if (onRead == NULL) {
        return;
    }

    // We keep track of the line we're on so we can log the line if an error occurs.
    unsigned int iLine = 1;

    // Sometimes we'll load a new chunk while in the middle of processing a line. This keeps track of that for us.
    easyutil_bool moveToNextLineAfterNextChunkRead = 0;



    char chunkData[4096];
    unsigned int chunkSize = 0;

    // Keep looping so long as there is still data available.
    easyutil_bool isMoreDataAvailable = 1;
    do
    {
        // Load more data to begin with.
        unsigned int bytesToRead = sizeof(chunkData) - chunkSize;
        chunkSize = onRead(pUserData, chunkData + chunkSize, bytesToRead);
        if (chunkSize < bytesToRead) {
            isMoreDataAvailable = 0;
        }

        char* pChunkEnd = chunkData + chunkSize;

        unsigned int chunkBytesRemaining = chunkSize;
        while (chunkBytesRemaining > 0)
        {
            char* pL = chunkData + (chunkSize - chunkBytesRemaining);       // The current position in the line.
            
            if (moveToNextLineAfterNextChunkRead) {
                goto move_to_end_of_line;
            }


            //// Key ////
            char* pK = pL;

            // Leading whitespace.
            while (pK < pChunkEnd && (pK[0] == ' ' || pK[0] == '\t' || pK[0] == '\r' || pK[0] == '\n' || pK[0] == '#')) {
                if (pK[0] == '\n') {
                    pL = pK;
                    goto move_to_end_of_line;
                }

                if (pK[0] == '#')
                {
                    pL = pK;
                    goto move_to_end_of_line;
                }

                pK += 1;
            }

            if (pK == pChunkEnd) {
                break;  // Ran out of data.
            }


            // Loop until first whitespace. This is where the null terminator for the key will be placed. Validation will be done below when trying
            // to parse the value.
            char* pKEnd = pK;
            while (pKEnd < pChunkEnd && pKEnd[0] != ' ' && pKEnd[0] != '\t' && pKEnd[0] != '\r') {
                pKEnd += 1;
            }

            if (pKEnd == pChunkEnd) {
                break;  // Ran out of data.
            }

            pKEnd[0] = '\0';




            //// Value ////
            char* pV = pKEnd + 1;

            // Leading whitespace.
            while (pV < pChunkEnd && (pV[0] == ' ' || pV[0] == '\t' || pV[0] == '\r')) {
                pV += 1;
            }

            if (pV == pChunkEnd) {
                break;  // Ran out of data.
            }


            // Validation.
            if (pV[0] == '\n') {
                if (onError) {
                    char msg[4096];
                    snprintf(msg, sizeof(msg), "%s - %s", pK, "Unexpected new-line character. Pairs must be placed on a single line.");
                    onError(pUserData, msg, iLine);
                }

                pL = pV;
                goto move_to_end_of_line;
            }
            if (pV[0] == '#') {
                if (onError) {
                    char msg[4096];
                    snprintf(msg, sizeof(msg), "%s - %s", pK, "Unexpected end of key/pair declaration. Pairs must be decalared as <key><whitespace><value>.");
                    onError(pUserData, msg, iLine);
                }

                pL = pV;
                goto move_to_end_of_line;
            }


            // Don't include double quotes in the result.
            if (pV[0] == '"') {
                pV += 1;
            }


            // Keep looping until the end of the line, but track the last non-whitespace character.
            char* pEOL = pV;
            char* pVEnd = pV;
            while (pEOL < pChunkEnd && pEOL[0] != '\n' && pEOL[0] != '#')
            {
                if (pEOL[0] != ' ' && pEOL[0] != '\t' && pEOL[0] != '\r') {
                    pVEnd = pEOL;
                }

                pEOL += 1;
            }

            if (pVEnd == pChunkEnd) {
                break;  // Ran out of data.
            }
            
            if (pVEnd[0] != '"')
            {
                pVEnd += 1;

                if (pVEnd == pChunkEnd) {
                    break;  // Ran out of data.
                }
            }            

            pVEnd[0] = '\0';

            assert(pV < pVEnd);
            if (pV[0] == '"') {
                pV += 1;
            }



            // We should have a valid pair at this point.
            if (onPair) {
                onPair(pUserData, pK, pV);
            }



            // Move to the end of the line.
            move_to_end_of_line:
            while (pL < pChunkEnd && pL[0] != '\n') {
                pL += 1;
            }

            if (pL == pChunkEnd)
            {
                // If we get here it means we ran out of data in the chunk. We need to load a new chunk, but immediate skip to the end of the line after doing so.
                moveToNextLineAfterNextChunkRead = 1;
                break;
            }

            // At this point we are at the end of the line and need to move to the next one. The check above ensures we still have
            // data available in the chunk at this point.
            assert(pL[0] == '\n');
            pL += 1;

            moveToNextLineAfterNextChunkRead = 0;

            assert(pL >= chunkData);
            chunkBytesRemaining = chunkSize - ((unsigned int)(pL - chunkData));

            iLine += 1;
        }

        
        // If we get here it means we've run out of data in the chunk. If there is more data available there will be bytes in chunkData that have not
        // yet been read. What we need to do is move that data to the beginning of the buffer and read just enough bytes to fill the remaining space
        // in the chunk buffer.
        if (isMoreDataAvailable)
        {
            // When moving to the next chunk there is something we need to consider - If we weren't able to read anything up until this point it means a
            // key/value pair was too long. To fix this we just skip over it.
            if (chunkBytesRemaining == chunkSize)
            {
                // If we get here there is a chance the key/value pair is too long, but there is also a chance we have just been
                // wanting to move to the next line as a result of us reaching the end of the chunk while tring to seek past the
                // end of the line. If we are not trying to seek past the line it means the key/value pair was too long.
                if (moveToNextLineAfterNextChunkRead == 0)
                {
                    if (onError) {
                        char msg[4096];
                        snprintf(msg, sizeof(msg), "%s", "Key/value pair is too long. A single line cannot exceed 4KB.");
                        onError(pUserData, msg, iLine);
                    }

                    moveToNextLineAfterNextChunkRead = 1;
                }
                

                // Setting the chunk size to 0 causes an entire chunk to be loaded in the next iteration as opposed to a partial chunk as in the else branch below.
                chunkSize = 0;
            }
            else
            {
                memmove(chunkData, chunkData + (chunkSize - chunkBytesRemaining), chunkBytesRemaining);
                chunkSize = chunkBytesRemaining;
            }
        }

    }while(isMoreDataAvailable);
}



/////////////////////////////////////////////////////////
// Known Folders

#if defined(_WIN32) || defined(_WIN64)
#include <shlobj.h>

easyutil_bool easyutil_get_config_folder_path(char* pathOut, unsigned int pathOutSize)
{
    // The documentation for SHGetFolderPathA() says that the output path should be the size of MAX_PATH. We'll enforce
    // that just to be safe.
    if (pathOutSize >= MAX_PATH)
    {
        SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, pathOut);
    }
    else
    {
        char pathOutTemp[MAX_PATH];
        SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, pathOutTemp);

        if (strcpy_s(pathOut, pathOutSize, pathOutTemp) != 0) {
            return 0;
        }
    }
    

    // Back slashes need to be normalized to forward.
    while (pathOut[0] != '\0') {
        if (pathOut[0] == '\\') {
            pathOut[0] = '/';
        }

        pathOut += 1;
    }

    return 1;
}
#else
#include <sys/types.h>
#include <pwd.h>

easyutil_bool easyutil_get_config_folder_path(char* pathOut, unsigned int pathOutSize)
{
    const char* configdir = getenv("XDG_CONFIG_HOME");
    if (configdir != NULL)
    {
        return strcpy_s(pathOut, pathOutSize, configdir) == 0;
    }
    else
    {
        const char* homedir = getenv("HOME");
        if (homedir == NULL) {
            homedir = getpwuid(getuid())->pw_dir;
        }

        if (homedir != NULL)
        {
            if (strcpy_s(pathOut, pathOutSize, homedir) == 0)
            {
                size_t homedirLength = strlen(homedir);
                pathOut     += homedirLength;
                pathOutSize -= homedirLength;

                if (pathOutSize > 0)
                {
                    pathOut[0] = '/';
                    pathOut     += 1;
                    pathOutSize -= 1;

                    return strcpy_s(pathOut, pathOutSize, ".config") == 0;
                }
            }
        }
    }

    return 0;
}

#endif


/////////////////////////////////////////////////////////
// DPI Awareness

#if defined(_WIN32) || defined(_WIN64)

// Normally we would include the two files below, but of course they are not in the latest build
// of MinGW at the time of writing. Thus, we need to just copy the structures from the MSDN
// documentation until MinGW adds these files.
#if 0
#include <VersionHelpers.h>
#include <ShellScalingApi.h>
#else
#if defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

#define VERSIONHELPERAPI FORCEINLINE BOOL

VERSIONHELPERAPI IsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    DWORDLONG dwlConditionMask_Major = VerSetConditionMask(0,                      VER_MAJORVERSION,     VER_GREATER_EQUAL);
    DWORDLONG dwlConditionMask_Minor = VerSetConditionMask(dwlConditionMask_Major, VER_MINORVERSION,     VER_GREATER_EQUAL);
    DWORDLONG dwlConditionMask       = VerSetConditionMask(dwlConditionMask_Minor, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    OSVERSIONINFOEXW osvi;
    ZeroMemory(&osvi, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

VERSIONHELPERAPI IsWindows8Point1OrGreater()
{
    return IsWindowsVersionOrGreater(HIBYTE(0x0603), LOBYTE(0x0603), 0);
}
#if defined(__clang__)
    #pragma GCC diagnostic pop
#endif
#endif

typedef BOOL    (__stdcall * PFN_SetProcessDPIAware)     (void);
typedef HRESULT (__stdcall * PFN_SetProcessDpiAwareness) (PROCESS_DPI_AWARENESS);
typedef HRESULT (__stdcall * PFN_GetDpiForMonitor)       (HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT *dpiX, UINT *dpiY);

void win32_make_dpi_aware()
{
    easyutil_bool fallBackToDiscouragedAPI = EASYUTIL_FALSE;

    // The application should be DPI aware.
    if (IsWindows8Point1OrGreater())
    {
        // We can't call SetProcessDpiAwareness() directly because otherwise on versions of Windows < 8.1 we'll get an error at load time about
        // a missing DLL.
        HMODULE hSHCoreDLL = LoadLibraryW(L"shcore.dll");
        if (hSHCoreDLL != NULL)
        {
            PFN_SetProcessDpiAwareness _SetProcessDpiAwareness = (PFN_SetProcessDpiAwareness)GetProcAddress(hSHCoreDLL, "SetProcessDpiAwareness");
            if (_SetProcessDpiAwareness != NULL) {
                _SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
            } else {
                // Couldn't find SetProcessDpiAwareness() so fall back to the discouraged API.
                fallBackToDiscouragedAPI = EASYUTIL_TRUE;
            }

            FreeLibrary(hSHCoreDLL);
        }
        else
        {
            // Couldn't find shcore.dll so fall back to the discouraged API.
            fallBackToDiscouragedAPI = EASYUTIL_TRUE;
        }
    }
    else
    {
        // Not running at least Windows 8.1 so fall back to the discouraged API.
        fallBackToDiscouragedAPI = EASYUTIL_TRUE;
    }


    if (fallBackToDiscouragedAPI)
    {
        HMODULE hUser32DLL = LoadLibraryW(L"user32.dll");
        if (hUser32DLL != NULL)
        {
            PFN_SetProcessDPIAware _SetProcessDPIAware = (PFN_SetProcessDPIAware)GetProcAddress(hUser32DLL, "SetProcessDPIAware");
            if (_SetProcessDPIAware != NULL) {
                _SetProcessDPIAware();
            }

            FreeLibrary(hUser32DLL);
        }
    }
}
#endif



/////////////////////////////////////////////////////////
// Command Line

struct easyutil_cmdline
{
    int unused;
};

easyutil_cmdline* easyutil_create_command_line(int argc, char** argv)
{
    easyutil_cmdline* pCmdLine = malloc(sizeof(easyutil_cmdline));
    if (pCmdLine != NULL) {
        pCmdLine->unused = 0;
    }

    return pCmdLine;
}

easyutil_cmdline* easyutil_create_command_line_win32(char* args)
{
    easyutil_cmdline* pCmdLine = malloc(sizeof(easyutil_cmdline));
    if (pCmdLine != NULL) {
        pCmdLine->unused = 0;
    }

    return pCmdLine;
}

void easyutil_delete_command_line(easyutil_cmdline* pCmdLine)
{
    free(pCmdLine);
}



/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/