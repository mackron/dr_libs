// Public Domain. See "unlicense" statement at the end of this file.

#include "easy_util.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>     // For memmove()


/////////////////////////////////////////////////////////
// String Helpers

void easyutil_strrmchar(char* str, char c)
{
    char* src = str;
    char* dst = str;

    while (src[0] != '\0')
    {
        dst[0] = src[0];

        if (dst[0] != c) {
            dst += 1;
        }

        src += 1;
    }

    dst[0] = '\0';
}



/////////////////////////////////////////////////////////
// Key/Value Pair Parsing

void easyutil_parse_key_value_pairs(key_value_read_proc onRead, key_value_pair_proc onPair, key_value_error_proc onError, void* pUserData)
{
    if (onRead == NULL) {
        return;
    }

    // We keep track of the line we're on so we can log the line if an error occurs.
    unsigned int iLine = 1;

    // Sometimes we'll load a new chunk while in the middle of processing a line. This keeps track of that for us.
    bool moveToNextLineAfterNextChunkRead = 0;



    char chunkData[4096];
    unsigned int chunkSize = 0;

    // Keep looping so long as there is still data available.
    bool isMoreDataAvailable = 1;
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
            char* pK = NULL;
            char* pV = NULL;
            
            if (moveToNextLineAfterNextChunkRead) {
                goto move_to_end_of_line;
            }


            //// Key ////
            pK = pL;

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

            if (pKEnd == pChunkEnd)
            {
                if (!isMoreDataAvailable)
                {
                    if (pKEnd < chunkData + sizeof(chunkData)) {
                        pKEnd[0] = '\0';
                    }

                    pL = pKEnd;
                    pV = NULL;
                    goto post_on_pair;
                }

                break;  // Ran out of data.
            }

            pKEnd[0] = '\0';




            //// Value ////
            pV = pKEnd + 1;

            // Leading whitespace.
            while (pV < pChunkEnd && (pV[0] == ' ' || pV[0] == '\t' || pV[0] == '\r')) {
                pV += 1;
            }

            if (pV == pChunkEnd) {
                break;  // Ran out of data.
            }


            // Validation. If we got to the end of the line before finding a value, we just assume it was a value-less key which we'll consider to be valid.
            if (pV[0] == '\n' || pV[0] == '#')
            {
                pL = pV;
                pV = NULL;
                goto post_on_pair;
            }
            

            // Don't include double quotes in the result.
            if (pV[0] == '"') {
                pV += 1;
            }


            // Trailing whitespace. Keep looping until the end of the line, but track the last non-whitespace character.
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
            post_on_pair:
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

bool easyutil_get_config_folder_path(char* pathOut, unsigned int pathOutSize)
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

bool easyutil_get_log_folder_path(char* pathOut, unsigned int pathOutSize)
{
    return easyutil_get_config_folder_path(pathOut, pathOutSize);
}
#else
#include <sys/types.h>
#include <pwd.h>

bool easyutil_get_config_folder_path(char* pathOut, unsigned int pathOutSize)
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

bool easyutil_get_log_folder_path(char* pathOut, unsigned int pathOutSize)
{
    return strcpy_s(pathOut, pathOutSize, "var/log");
}

#endif


/////////////////////////////////////////////////////////
// DPI Awareness

#if defined(_WIN32) || defined(_WIN64)

typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

typedef BOOL    (__stdcall * PFN_SetProcessDPIAware)     (void);
typedef HRESULT (__stdcall * PFN_SetProcessDpiAwareness) (PROCESS_DPI_AWARENESS);

void win32_make_dpi_aware()
{
    bool fallBackToDiscouragedAPI = false;

    // We can't call SetProcessDpiAwareness() directly because otherwise on versions of Windows < 8.1 we'll get an error at load time about
    // a missing DLL.
    HMODULE hSHCoreDLL = LoadLibraryW(L"shcore.dll");
    if (hSHCoreDLL != NULL)
    {
        PFN_SetProcessDpiAwareness _SetProcessDpiAwareness = (PFN_SetProcessDpiAwareness)GetProcAddress(hSHCoreDLL, "SetProcessDpiAwareness");
        if (_SetProcessDpiAwareness != NULL)
        {
            if (_SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE) != S_OK)
            {
                fallBackToDiscouragedAPI = false;
            }
        }
        else
        {
            fallBackToDiscouragedAPI = false;
        }

        FreeLibrary(hSHCoreDLL);
    }
    else
    {
        fallBackToDiscouragedAPI = false;
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
// Date / Time

time_t easyutil_now()
{
    return time(NULL);
}

void easyutil_datetime_short(time_t t, char* strOut, unsigned int strOutSize)
{
#if defined(_MSC_VER)
	struct tm local;
	localtime_s(&local, &t);
    strftime(strOut, strOutSize, "%x %H:%M:%S", &local);
#else
	struct tm *local = localtime(&t);
	strftime(strOut, strOutSize, "%x %H:%M:%S", local);
#endif
}



/////////////////////////////////////////////////////////
// Command Line

typedef struct
{
    easyutil_cmdline* pCmdLine;
    char* value;

    // Win32 style data.
    char* win32_payload;
    char* valueEnd;

    // argv style data.
    int iarg;   // <-- This starts at -1 so that the first call to next() increments it to 0.

} easyutil_cmdline_iterator;

easyutil_cmdline_iterator easyutil_cmdline_begin(easyutil_cmdline* pCmdLine)
{
    easyutil_cmdline_iterator i;
    i.pCmdLine      = pCmdLine;
    i.value         = NULL;
    i.win32_payload = NULL;
    i.valueEnd      = NULL;

    if (pCmdLine != NULL)
    {
        if (pCmdLine->win32 != NULL)
        {
            // Win32 style
            size_t length = strlen(pCmdLine->win32);
            i.win32_payload = malloc(length + 2);         // +2 for a double null terminator.
            strcpy_s(i.win32_payload, length + 2, pCmdLine->win32);
            i.win32_payload[length + 1] = '\0';

            i.valueEnd = i.win32_payload;
        }
        else
        {
            // argv style
            i.iarg = -1;        // <-- Begin this at -1 so that the first call to next() increments to 0.
        }
    }

    return i;
}

bool easyutil_cmdline_next(easyutil_cmdline_iterator* i)
{
    if (i != NULL && i->pCmdLine != NULL)
    {
        if (i->pCmdLine->win32 != NULL)
        {
            // Win32 style
            if (i->value == NULL) {
                i->value    = i->win32_payload;
                i->valueEnd = i->value;
            } else {
                i->value = i->valueEnd + 1;
            }


            // Move to the start of the next argument.
            while (i->value[0] != '\0' && i->value[0] == ' ') {
                i->value += 1;
            }


            // If at this point we are sitting on the null terminator it means we have finished iterating.
            if (i->value[0] == '\0')
            {
                free(i->win32_payload);
                i->win32_payload = NULL;
                i->pCmdLine      = NULL;
                i->value         = NULL;
                i->valueEnd      = NULL;

                return false;
            }


            // Move to the end of the token. If the argument begins with a double quote, we iterate until we find
            // the next unescaped double-quote.
            if (i->value[0] == '\"')
            {
                // Go to the last unescaped double-quote.
                i->value += 1;
                i->valueEnd = i->value + 1;

                while (i->valueEnd[0] != '\0' && i->valueEnd[0] != '\"')
                {
                    if (i->valueEnd[0] == '\\') {
                        i->valueEnd += 1;

                        if (i->valueEnd[0] == '\0') {
                            break;
                        }
                    }

                    i->valueEnd += 1;
                }
                i->valueEnd[0] = '\0';
            }
            else
            {
                // Go to the next space.
                i->valueEnd = i->value + 1;

                while (i->valueEnd[0] != '\0' && i->valueEnd[0] != ' ')
                {
                    i->valueEnd += 1;
                }
                i->valueEnd[0] = '\0';
            }


            // The argument needs to have escape characters removed.
            easyutil_strrmchar(i->value, '\\');

            return true;
        }
        else
        {
            // argv style
            i->iarg += 1;
            if (i->iarg < i->pCmdLine->argc)
            {
                i->value = i->pCmdLine->argv[i->iarg];
                return true;
            }
            else
            {
                i->value = NULL;
                return false;
            }
        }
    }

    return false;
}


bool easyutil_init_cmdline(easyutil_cmdline* pCmdLine, int argc, char** argv)
{
    if (pCmdLine == NULL) {
        return false;
    }

    pCmdLine->argc  = argc;
    pCmdLine->argv  = argv;
    pCmdLine->win32 = NULL;

    return true;
}

bool easyutil_init_cmdline_win32(easyutil_cmdline* pCmdLine, char* args)
{
    if (pCmdLine == NULL) {
        return false;
    }

    pCmdLine->argc  = 0;
    pCmdLine->argv  = NULL;
    pCmdLine->win32 = args;

    return true;
}

void easyutil_parse_cmdline(easyutil_cmdline* pCmdLine, easyutil_cmdline_parse_proc callback, void* pUserData)
{
    if (pCmdLine == NULL || callback == NULL) {
        return;
    }


    char pTemp[2] = {0};

    char* pKey = NULL;
    char* pVal = NULL;

    easyutil_cmdline_iterator arg = easyutil_cmdline_begin(pCmdLine);
    if (easyutil_cmdline_next(&arg))
    {
        callback("[path]", arg.value, pUserData);
    }

    while (easyutil_cmdline_next(&arg))
    {
        if (arg.value[0] == '-')
        {
            // key

            // If the key is non-null, but the value IS null, it means we hit a key with no value in which case it will not yet have been posted.
            if (pKey != NULL && pVal == NULL)
            {
                callback(pKey, pVal, pUserData);
                pKey = NULL;
            }
            else
            {
                // Need to ensure the key and value are reset before doing any further processing.
                pKey = NULL;
                pVal = NULL;
            }



            if (arg.value[1] == '-')
            {
                // --argument style
                pKey = arg.value + 2;
            }
            else
            {
                // -a -b -c -d or -abcd style
                if (arg.value[1] != '\0')
                {
                    if (arg.value[2] == '\0')
                    {
                        // -a -b -c -d style
                        pTemp[0] = arg.value[1];
                        pKey = pTemp;
                        pVal = NULL;
                    }
                    else
                    {
                        // -abcd style.
                        int i = 1;
                        while (arg.value[i] != '\0')
                        {
                            pTemp[0] = arg.value[i];
                            callback(pTemp, NULL, pUserData);
                            pKey = NULL;
                            pVal = NULL;

                            i += 1;
                        }
                    }
                }
            }
        }
        else
        {
            // value

            pVal = arg.value;
            callback(pKey, pVal, pUserData);
        }
    }


    // There may be a key without a value that needs posting.
    if (pKey != NULL && pVal == NULL) {
        callback(pKey, pVal, pUserData);
    }
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