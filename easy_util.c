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

const char* easyutil_first_non_whitespace(const char* str)
{
    if (str == NULL) {
        return NULL;
    }

    while (str[0] != '\0' && !(str[0] != ' ' && str[0] != '\t' && str[0] != '\n' && str[0] != '\v' && str[0] != '\f' && str[0] != '\r')) {
        str += 1;
    }

    return str;
}

const char* easyutil_first_whitespace(const char* str)
{
    if (str == NULL) {
        return NULL;
    }

    while (str[0] != '\0' && (str[0] != ' ' && str[0] != '\t' && str[0] != '\n' && str[0] != '\v' && str[0] != '\f' && str[0] != '\r')) {
        str += 1;
    }

    return str;
}



/////////////////////////////////////////////////////////
// Key/Value Pair Parsing

void easyutil_parse_key_value_pairs(key_value_read_proc onRead, key_value_pair_proc onPair, key_value_error_proc onError, void* pUserData)
{
    if (onRead == NULL) {
        return;
    }

    char pChunk[4096];
    size_t chunkSize = 0;

    unsigned int currentLine = 1;

    bool moveToNextLineBeforeProcessing = false;
    bool skipWhitespaceBeforeProcessing = false;

    // Just keep looping. We'll break from this loop when we have run out of data.
    for (;;)
    {
        // Start the iteration by reading as much data as we can.
        chunkSize = onRead(pUserData, pChunk, sizeof(pChunk));
        if (chunkSize == 0) {
            // No more data available.
            return;
        }

        char* pChunkEnd = pChunk + chunkSize;
        char* pC = pChunk;  // Chunk pointer. This is as the chunk is processed.

        if (moveToNextLineBeforeProcessing)
        {
            move_to_next_line:
            while (pC < pChunkEnd && pC[0] != '\n') {
                pC += 1;
            }

            if (pC == pChunkEnd) {
                // Ran out of data. Load the next chunk and keep going.
                moveToNextLineBeforeProcessing = true;
                continue;
            }

            pC += 1;     // pC[0] == '\n' - skip past the new line character.
            currentLine += 1;
            moveToNextLineBeforeProcessing = false;
        }

        if (skipWhitespaceBeforeProcessing)
        {
            while (pC < pChunkEnd && pC[0] == ' ' || pC[0] == '\t' || pC[0] == '\r') {
                pC += 1;
            }

            if (pC == pChunkEnd) {
                // Ran out of data.
                skipWhitespaceBeforeProcessing = true;
                continue;
            }

            skipWhitespaceBeforeProcessing = false;
        }


        // We loop character by character. When we run out of data, we start again.
        while (pC < pChunkEnd)
        {
            //// Key ////

            // Skip whitespace.
            while (pC < pChunkEnd && pC[0] == ' ' || pC[0] == '\t' || pC[0] == '\r') {
                pC += 1;
            }

            if (pC == pChunkEnd) {
                // Ran out of data.
                skipWhitespaceBeforeProcessing = true;
                continue;
            }

            if (pC[0] == '\n') {
                // Found the end of the line. 
                pC += 1;
                currentLine += 1;
                continue;
            }

            if (pC[0] == '#') {
                // Found a comment. Move to the end of the line and continue.
                goto move_to_next_line;
            }

            char* pK = pC;
            while (pC < pChunkEnd && pC[0] != ' ' && pC[0] != '\t' && pC[0] != '\r' && pC[0] != '\n' && pC[0] != '#') {
                pC += 1;
            }

            if (pC == pChunkEnd)
            {
                // Ran out of data. We need to move what we have of the key to the start of the chunk buffer, and then read more data.
                if (chunkSize == sizeof(pChunk))
                {
                    size_t lineSizeSoFar = pC - pK;
                    memmove(pChunk, pK, lineSizeSoFar);

                    chunkSize = lineSizeSoFar + onRead(pUserData, pChunk + lineSizeSoFar, sizeof(pChunk) - lineSizeSoFar);
                    pChunkEnd = pChunk + chunkSize;

                    pK = pChunk;
                    pC = pChunk + lineSizeSoFar;
                    while (pC < pChunkEnd && pC[0] != ' ' && pC[0] != '\t' && pC[0] != '\r' && pC[0] != '\n' && pC[0] != '#') {
                        pC += 1;
                    }
                }

                if (pC == pChunkEnd) {
                    if (chunkSize == sizeof(pChunk)) {
                        if (onError) {
                            onError(pUserData, "Line is too long. A single line cannot exceed 4KB.", currentLine);
                        }

                        goto move_to_next_line;
                    } else {
                        // No more data. Just treat this one as a value-less key and return.
                        if (onPair) {
                            pC[0] = '\0';
                            onPair(pUserData, pK, NULL);
                        }

                        return;
                    }
                }
            }

            char* pKEnd = pC;

            //// Value ////

            // Skip whitespace.
            while (pC < pChunkEnd && pC[0] == ' ' || pC[0] == '\t' || pC[0] == '\r') {
                pC += 1;
            }

            if (pC == pChunkEnd)
            {
                // Ran out of data. We need to move what we have of the key to the start of the chunk buffer, and then read more data.
                if (chunkSize == sizeof(pChunk))
                {
                    size_t lineSizeSoFar = pC - pK;
                    memmove(pChunk, pK, lineSizeSoFar);

                    chunkSize = lineSizeSoFar + onRead(pUserData, pChunk + lineSizeSoFar, sizeof(pChunk) - lineSizeSoFar);
                    pChunkEnd = pChunk + chunkSize;

                    pKEnd = pChunk + (pKEnd - pK);
                    pK = pChunk;
                    pC = pChunk + lineSizeSoFar;
                    while (pC < pChunkEnd && pC[0] == ' ' || pC[0] == '\t' || pC[0] == '\r') {
                        pC += 1;
                    }
                }

                if (pC == pChunkEnd) {
                    if (chunkSize == sizeof(pChunk)) {
                        if (onError) {
                            onError(pUserData, "Line is too long. A single line cannot exceed 4KB.", currentLine);
                        }

                        goto move_to_next_line;
                    } else {
                        // No more data. Just treat this one as a value-less key and return.
                        if (onPair) {
                            pKEnd[0] = '\0';
                            onPair(pUserData, pK, NULL);
                        }

                        return;
                    }
                }
            }

            if (pC[0] == '\n') {
                // Found the end of the line. Treat it as a value-less key.
                pKEnd[0] = '\0';
                if (onPair) {
                    onPair(pUserData, pK, NULL);
                }

                pC += 1;
                currentLine += 1;
                continue;
            }

            if (pC[0] == '#') {
                // Found a comment. Treat is as a value-less key and move to the end of the line.
                pKEnd[0] = '\0';
                if (onPair) {
                    onPair(pUserData, pK, NULL);
                }

                goto move_to_next_line;
            }

            char* pV = pC;

            // Find the last non-whitespace character.
            char* pVEnd = pC;
            while (pC < pChunkEnd && pC[0] != '\n' && pC[0] != '#') {
                if (pC[0] != ' ' && pC[0] != '\t' && pC[0] != '\r') {
                    pVEnd = pC;
                }

                pC += 1;
            }

            if (pC == pChunkEnd)
            {
                // Ran out of data. We need to move what we have of the key to the start of the chunk buffer, and then read more data.
                if (chunkSize == sizeof(pChunk))
                {
                    size_t lineSizeSoFar = pC - pK;
                    memmove(pChunk, pK, lineSizeSoFar);

                    chunkSize = lineSizeSoFar + onRead(pUserData, pChunk + lineSizeSoFar, sizeof(pChunk) - lineSizeSoFar);
                    pChunkEnd = pChunk + chunkSize;

                    pVEnd = pChunk + (pVEnd - pK);
                    pKEnd = pChunk + (pKEnd - pK);
                    pV = pChunk + (pV - pK);
                    pK = pChunk;
                    pC = pChunk + lineSizeSoFar;
                    while (pC < pChunkEnd && pC[0] != '\n' && pC[0] != '#') {
                        if (pC[0] != ' ' && pC[0] != '\t' && pC[0] != '\r') {
                            pVEnd = pC;
                        }

                        pC += 1;
                    }
                }

                if (pC == pChunkEnd) {
                    if (chunkSize == sizeof(pChunk)) {
                        if (onError) {
                            onError(pUserData, "Line is too long. A single line cannot exceed 4KB.", currentLine);
                        }

                        goto move_to_next_line;
                    }
                }
            }


            // Remove double-quotes from the value.
            if (pV[0] == '\"') {
                pV += 1;

                if (pVEnd[0] == '\"') {
                    pVEnd -= 1;
                }
            }

            // Before null-terminating the value we first need to determine how we'll proceed after posting onPair.
            bool wasOnNL = pVEnd[1] == '\n';

            pKEnd[0] = '\0';
            pVEnd[1] = '\0';
            if (onPair) {
                onPair(pUserData, pK, pV);
            }

            if (wasOnNL)
            {
                // Was sitting on a new-line character.
                pC += 1;
                currentLine += 1;
                continue;
            }
            else
            {
                // Was sitting on a comment - just to the next line.
                goto move_to_next_line;
            }
        }
    }
}


/////////////////////////////////////////////////////////
// Basic Tokenizer

const char* easyutil_next_token(const char* tokens, char* tokenOut, unsigned int tokenOutSize)
{
    if (tokens == NULL) {
        return NULL;
    }

    // Skip past leading whitespace.
    while (tokens[0] != '\0' && !(tokens[0] != ' ' && tokens[0] != '\t' && tokens[0] != '\n' && tokens[0] != '\v' && tokens[0] != '\f' && tokens[0] != '\r')) {
        tokens += 1;
    }

    if (tokens[0] == '\0') {
        return NULL;
    }


    const char* strBeg = tokens;
    const char* strEnd = strBeg;
    
    if (strEnd[0] == '\"')
    {
        // It's double-quoted - loop until the next unescaped quote character.

        // Skip past the first double-quote character.
        strBeg += 1;
        strEnd += 1;

        // Keep looping until the next unescaped double-quote character.
        char prevChar = '\0';
        while (strEnd[0] != '\0' && (strEnd[0] != '\"' || prevChar == '\\'))
        {
            strEnd += 1;
        }
    }
    else
    {
        // It's not double-quoted - just loop until the first whitespace.
        while (strEnd[0] != '\0' && (strEnd[0] != ' ' && strEnd[0] != '\t' && strEnd[0] != '\n' && strEnd[0] != '\v' && strEnd[0] != '\f' && strEnd[0] != '\r')) {
            strEnd += 1;
        }
    }


    // If the output buffer is large enough to hold the token, copy the token into it.
    //assert(strEnd >= strBeg);

    size_t tokenLength = (size_t)(strEnd - strBeg);
    if ((size_t)tokenOutSize > tokenLength)
    {
        // The output buffer is large enough.
        for (size_t i = 0; i < tokenLength; ++i) {
            tokenOut[i] = strBeg[i];
        }

        tokenOut[tokenLength] = '\0';
    }
    else
    {
        // The output buffer is too small. Set it to an empty string.
        if (tokenOutSize > 0) {
            tokenOut[0] = '\0';
        }
    }


    // Skip past the double-quote character before returning.
    if (strEnd[0] == '\"') {
        strEnd += 1;
    }

    return strEnd;
}




/////////////////////////////////////////////////////////
// Known Folders

#if defined(_WIN32) || defined(_WIN64)
#include <shlobj.h>

bool easyutil_get_config_folder_path(char* pathOut, size_t pathOutSize)
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

bool easyutil_get_log_folder_path(char* pathOut, size_t pathOutSize)
{
    return easyutil_get_config_folder_path(pathOut, pathOutSize);
}
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

bool easyutil_get_config_folder_path(char* pathOut, size_t pathOutSize)
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

bool easyutil_get_log_folder_path(char* pathOut, size_t pathOutSize)
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

bool easyutil_init_cmdline_win32(easyutil_cmdline* pCmdLine, const char* args)
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
        if (!callback("[path]", arg.value, pUserData)) {
            return;
        }
    }

    while (easyutil_cmdline_next(&arg))
    {
        if (arg.value[0] == '-')
        {
            // key

            // If the key is non-null, but the value IS null, it means we hit a key with no value in which case it will not yet have been posted.
            if (pKey != NULL && pVal == NULL)
            {
                if (!callback(pKey, pVal, pUserData)) {
                    return;
                }

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

                            if (!callback(pTemp, NULL, pUserData)) {
                                return;
                            }

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
            if (!callback(pKey, pVal, pUserData)) {
                return;
            }
        }
    }


    // There may be a key without a value that needs posting.
    if (pKey != NULL && pVal == NULL) {
        callback(pKey, pVal, pUserData);
    }
}





/////////////////////////////////////////////////////////
// Threading

#if defined(_WIN32)
#include <windows.h>

void easyutil_sleep(unsigned int milliseconds)
{
    Sleep((DWORD)milliseconds);
}


typedef struct
{
    /// The Win32 thread handle.
    HANDLE hThread;

    /// The entry point.
    easyutil_thread_entry_proc entryProc;

    /// The user data to pass to the thread's entry point.
    void* pData;

    /// Set to true by the entry function. We use this to wait for the entry function to start.
    bool isInEntryProc;

} easyutil_thread_win32;

static DWORD WINAPI easyutil_thread_entry_proc_win32(easyutil_thread_win32* pThreadWin32)
{
    assert(pThreadWin32 != NULL);

    void* pEntryProcData = pThreadWin32->pData;
    easyutil_thread_entry_proc entryProc = pThreadWin32->entryProc;
    assert(entryProc != NULL);

    pThreadWin32->isInEntryProc = true;

    return (DWORD)entryProc(pEntryProcData);
}

easyutil_thread easyutil_create_thread(easyutil_thread_entry_proc entryProc, void* pData)
{
    if (entryProc == NULL) {
        return NULL;
    }

    easyutil_thread_win32* pThreadWin32 = malloc(sizeof(*pThreadWin32));
    if (pThreadWin32 != NULL)
    {
        pThreadWin32->entryProc     = entryProc;
        pThreadWin32->pData         = pData;
        pThreadWin32->isInEntryProc = false;

        pThreadWin32->hThread = CreateThread(NULL, 0, easyutil_thread_entry_proc_win32, pThreadWin32, 0, NULL);
        if (pThreadWin32 == NULL) {
            free(pThreadWin32);
            return NULL;
        }

        // Wait for the new thread to enter into it's entry point before returning. We need to do this so we can safely
        // support something like easyutil_delete_thread(easyutil_create_thread(my_thread_proc, pData)).
        while (!pThreadWin32->isInEntryProc) {}
    }

    return (easyutil_thread)pThreadWin32;
}

void easyutil_delete_thread(easyutil_thread thread)
{
    easyutil_thread_win32* pThreadWin32 = (easyutil_thread_win32*)thread;
    if (pThreadWin32 != NULL)
    {
        CloseHandle(pThreadWin32->hThread);
    }

    free(pThreadWin32);
}

void easyutil_wait_thread(easyutil_thread thread)
{
    easyutil_thread_win32* pThreadWin32 = (easyutil_thread_win32*)thread;
    if (pThreadWin32 != NULL)
    {
        WaitForSingleObject(pThreadWin32->hThread, INFINITE);
    }
}

void easyutil_wait_and_delete_thread(easyutil_thread thread)
{
    easyutil_wait_thread(thread);
    easyutil_delete_thread(thread);
}



#if 1
easyutil_mutex easyutil_create_mutex()
{
    easyutil_mutex mutex = malloc(sizeof(CRITICAL_SECTION));
    if (mutex != NULL)
    {
        InitializeCriticalSection(mutex);
    }

    return mutex;
}

void easyutil_delete_mutex(easyutil_mutex mutex)
{
    DeleteCriticalSection(mutex);
    free(mutex);
}

void easyutil_lock_mutex(easyutil_mutex mutex)
{
    EnterCriticalSection(mutex);
}

void easyutil_unlock_mutex(easyutil_mutex mutex)
{
    LeaveCriticalSection(mutex);
}
#else
#if 1
easyutil_mutex easyutil_create_mutex()
{
    return (void*)CreateEventA(NULL, FALSE, TRUE, NULL);
}

void easyutil_delete_mutex(easyutil_mutex mutex)
{
    CloseHandle((HANDLE)mutex);
}

void easyutil_lock_mutex(easyutil_mutex mutex)
{
    WaitForSingleObject((HANDLE)mutex, INFINITE);
}

void easyutil_unlock_mutex(easyutil_mutex mutex)
{
    SetEvent((HANDLE)mutex);
}
#else
easyutil_mutex easyutil_create_mutex()
{
    return (void*)CreateMutexA(NULL, FALSE, NULL);
}

void easyutil_delete_mutex(easyutil_mutex mutex)
{
    CloseHandle((HANDLE)mutex);
}

void easyutil_lock_mutex(easyutil_mutex mutex)
{
    WaitForSingleObject((HANDLE)mutex, INFINITE);
}

void easyutil_unlock_mutex(easyutil_mutex mutex)
{
    ReleaseMutex((HANDLE)mutex);
}
#endif
#endif


easyutil_semaphore easyutil_create_semaphore(int initialValue)
{
    return (void*)CreateSemaphoreA(NULL, initialValue, LONG_MAX, NULL);
}

void easyutil_delete_semaphore(easyutil_semaphore semaphore)
{
    CloseHandle(semaphore);
}

bool easyutil_wait_semaphore(easyutil_semaphore semaphore)
{
    return WaitForSingleObject((HANDLE)semaphore, INFINITE) == WAIT_OBJECT_0;
}

bool easyutil_release_semaphore(easyutil_semaphore semaphore)
{
    return ReleaseSemaphore((HANDLE)semaphore, 1, NULL);
}
#else
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>

void easyutil_sleep(unsigned int milliseconds)
{
    usleep(milliseconds * 1000);    // <-- usleep is in microseconds.
}


typedef struct
{
    /// The Win32 thread handle.
    pthread_t pthread;

    /// The entry point.
    easyutil_thread_entry_proc entryProc;

    /// The user data to pass to the thread's entry point.
    void* pData;

    /// Set to true by the entry function. We use this to wait for the entry function to start.
    bool isInEntryProc;

} easyutil_thread_posix;

static void* easyutil_thread_entry_proc_posix(void* pDataIn)
{
    easyutil_thread_posix* pThreadPosix = pDataIn;
    assert(pThreadPosix != NULL);

    void* pEntryProcData = pThreadPosix->pData;
    easyutil_thread_entry_proc entryProc = pThreadPosix->entryProc;
    assert(entryProc != NULL);

    pThreadPosix->isInEntryProc = true;

    return (void*)(size_t)entryProc(pEntryProcData);
}

easyutil_thread easyutil_create_thread(easyutil_thread_entry_proc entryProc, void* pData)
{
    if (entryProc == NULL) {
        return NULL;
    }

    easyutil_thread_posix* pThreadPosix = malloc(sizeof(*pThreadPosix));
    if (pThreadPosix != NULL)
    {
        pThreadPosix->entryProc     = entryProc;
        pThreadPosix->pData         = pData;
        pThreadPosix->isInEntryProc = false;

        if (pthread_create(&pThreadPosix->pthread, NULL, easyutil_thread_entry_proc_posix, pThreadPosix) != 0) {
            free(pThreadPosix);
            return NULL;
        }

        // Wait for the new thread to enter into it's entry point before returning. We need to do this so we can safely
        // support something like easyutil_delete_thread(easyutil_create_thread(my_thread_proc, pData)).
        while (!pThreadPosix->isInEntryProc) {}
    }

    return (easyutil_thread)pThreadPosix;
}

void easyutil_delete_thread(easyutil_thread thread)
{
    free(thread);
}

void easyutil_wait_thread(easyutil_thread thread)
{
    easyutil_thread_posix* pThreadPosix = (easyutil_thread_posix*)thread;
    if (pThreadPosix != NULL)
    {
        pthread_join(pThreadPosix->pthread, NULL);
    }
}



easyutil_mutex easyutil_create_mutex()
{
    pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(mutex, NULL) != 0) {
        free(mutex);
        mutex = NULL;
    }

    return mutex;
}

void easyutil_delete_mutex(easyutil_mutex mutex)
{
    pthread_mutex_destroy(mutex);
}

void easyutil_lock_mutex(easyutil_mutex mutex)
{
    pthread_mutex_lock(mutex);
}

void easyutil_unlock_mutex(easyutil_mutex mutex)
{
    pthread_mutex_unlock(mutex);
}



easyutil_semaphore easyutil_create_semaphore(int initialValue)
{
    sem_t* semaphore = malloc(sizeof(sem_t));
    if (sem_init(semaphore, 0, (unsigned int)initialValue) == -1) {
        free(semaphore);
        semaphore = NULL;
    }

    return semaphore;
}

void easyutil_delete_semaphore(easyutil_semaphore semaphore)
{
    sem_close(semaphore);
}

bool easyutil_wait_semaphore(easyutil_semaphore semaphore)
{
    return sem_wait(semaphore) != -1;
}

bool easyutil_release_semaphore(easyutil_semaphore semaphore)
{
    return sem_post(semaphore) != -1;
}
#endif




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
