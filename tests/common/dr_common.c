
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _MSC_VER
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>   /* So we can seed the random number generator based on time. */

#if !defined(_WIN32)
#include <sys/time.h>
#endif

typedef unsigned int dr_bool32;
#define DR_FALSE    0
#define DR_TRUE     1

/*
Return Values:
  0:  Success
  22: EINVAL
  34: ERANGE

Not using symbolic constants for errors because I want to avoid #including errno.h
*/
int dr_strcpy_s(char* dst, size_t dstSizeInBytes, const char* src)
{
    size_t i;

    if (dst == 0) {
        return 22;
    }
    if (dstSizeInBytes == 0) {
        return 34;
    }
    if (src == 0) {
        dst[0] = '\0';
        return 22;
    }

    for (i = 0; i < dstSizeInBytes && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (i < dstSizeInBytes) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return 34;
}

int dr_strncpy_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
    size_t maxcount;
    size_t i;

    if (dst == 0) {
        return 22;
    }
    if (dstSizeInBytes == 0) {
        return 34;
    }
    if (src == 0) {
        dst[0] = '\0';
        return 22;
    }

    maxcount = count;
    if (count == ((size_t)-1) || count >= dstSizeInBytes) {        /* -1 = _TRUNCATE */
        maxcount = dstSizeInBytes - 1;
    }

    for (i = 0; i < maxcount && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (src[i] == '\0' || i == count || count == ((size_t)-1)) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return 34;
}

int dr_strcat_s(char* dst, size_t dstSizeInBytes, const char* src)
{
    char* dstorig;

    if (dst == 0) {
        return 22;
    }
    if (dstSizeInBytes == 0) {
        return 34;
    }
    if (src == 0) {
        dst[0] = '\0';
        return 22;
    }

    dstorig = dst;

    while (dstSizeInBytes > 0 && dst[0] != '\0') {
        dst += 1;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes == 0) {
        return 22;  /* Unterminated. */
    }


    while (dstSizeInBytes > 0 && src[0] != '\0') {
        *dst++ = *src++;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes > 0) {
        dst[0] = '\0';
    } else {
        dstorig[0] = '\0';
        return 34;
    }

    return 0;
}

int dr_strncat_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
    char* dstorig;

    if (dst == 0) {
        return 22;
    }
    if (dstSizeInBytes == 0) {
        return 34;
    }
    if (src == 0) {
        return 22;
    }

    dstorig = dst;

    while (dstSizeInBytes > 0 && dst[0] != '\0') {
        dst += 1;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes == 0) {
        return 22;  /* Unterminated. */
    }


    if (count == ((size_t)-1)) {        /* _TRUNCATE */
        count = dstSizeInBytes - 1;
    }

    while (dstSizeInBytes > 0 && src[0] != '\0' && count > 0) {
        *dst++ = *src++;
        dstSizeInBytes -= 1;
        count -= 1;
    }

    if (dstSizeInBytes > 0) {
        dst[0] = '\0';
    } else {
        dstorig[0] = '\0';
        return 34;
    }

    return 0;
}

/*
String Helpers
*/
int dr_append_path(char* dst, size_t dstSize, const char* base, const char* other)
{
    int err;
    size_t  len;

    /* TODO: Return the correct error codes here. */
    if (dst == NULL) {
        return -1;
    }
    if (base == NULL || other == NULL) {
        return -1;
    }

    err = dr_strcpy_s(dst, dstSize, base);
    if (err != 0) {
        return err;
    }

    len = strlen(dst);
    if (len > 0) {
        /* Append the slash if required. */
        if (dst[len-1] != '/' && dst[len-1] != '\\') {
            err = dr_strcat_s(dst, dstSize, "/");
            if (err != 0) {
                dst[0] = '\0';
                return err;
            }

            len += 1;   /* +1 to the length to account for the slash. */
        }
    }

    err = dr_strcat_s(dst, dstSize, other);
    if (err != 0) {
        dst[0] = '\0';
        return err;
    }

    /* Success. */
    return 0;
}

const char* dr_path_file_name(const char* path)
{
    const char* fileName = path;

    if (path == NULL) {
        return NULL;
    }

    /* We just loop through the path until we find the last slash. */
    while (path[0] != '\0') {
        if (path[0] == '/' || path[0] == '\\') {
            fileName = path;
        }

        path += 1;
    }

    /* At this point the file name is sitting on a slash, so just move forward. */
    while (fileName[0] != '\0' && (fileName[0] == '/' || fileName[0] == '\\')) {
        fileName += 1;
    }

    return fileName;
}

const char* dr_extension(const char* path)
{
    const char* extension = path;
    const char* lastoccurance = NULL;

    if (path == NULL) {
        return NULL;
    }

    /* Just find the last '.' and return. */
    while (extension[0] != '\0') {
        if (extension[0] == '.') {
            extension    += 1;
            lastoccurance = extension;
        }

        extension += 1;
    }

    return (lastoccurance != 0) ? lastoccurance : extension;
}

dr_bool32 dr_extension_equal(const char* path, const char* extension)
{
    const char* ext1;
    const char* ext2;

    if (path == NULL || extension == NULL) {
        return 0;
    }

    ext1 = extension;
    ext2 = dr_extension(path);

#ifdef _MSC_VER
    return _stricmp(ext1, ext2) == 0;
#else
    return strcasecmp(ext1, ext2) == 0;
#endif
}




/*
File Iterator

dr_file_iterator state;
dr_file_iterator* pFile = dr_file_iterator_begin("the/folder/path", &state);
while (pFile != NULL) {
    // Do something with pFile.
    pFile = dr_file_iterator_next(pFile);
}

Limitations:
  - Only supports file paths up to 256 characters.
*/
typedef struct
{
    char folderPath[256];
    char relativePath[256]; /* Relative to the original folder path. */
    char absolutePath[256]; /* Concatenation of folderPath and relativePath. */
    dr_bool32 isDirectory;
#ifdef _WIN32
    HANDLE hFind;
#else
#endif
} dr_file_iterator;

dr_file_iterator* dr_file_iterator_begin(const char* pFolderPath, dr_file_iterator* pState)
{
#ifdef _WIN32
    char searchQuery[MAX_PATH];
    unsigned int searchQueryLength;
    WIN32_FIND_DATAA ffd;
    HANDLE hFind;
#endif

    if (pState == NULL) {
        return NULL;
    }

    memset(pState, 0, sizeof(*pState));

    if (pFolderPath == NULL) {
        return NULL;
    }

#ifdef _WIN32
    strcpy_s(searchQuery, sizeof(searchQuery), pFolderPath);

    searchQueryLength = (unsigned int)strlen(searchQuery);
    if (searchQueryLength >= MAX_PATH - 3) {
        return NULL; /* Path is too long. */
    }

    searchQuery[searchQueryLength + 0] = '\\';
    searchQuery[searchQueryLength + 1] = '*';
    searchQuery[searchQueryLength + 2] = '\0';

    hFind = FindFirstFileA(searchQuery, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL; /* Failed to begin search. */
    }

    /* Skip past "." and ".." directories. */
    while (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) {
        if (!FindNextFileA(hFind, &ffd)) {
            return NULL;    /* Couldn't find anything. */
        }
    }

    pState->hFind = hFind;


    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        pState->isDirectory = 1;
    } else {
        pState->isDirectory = 0;
    }

    strcpy_s(pState->relativePath, sizeof(pState->relativePath), ffd.cFileName);
#else
    return NULL;    /* Not yet implemented. */
#endif

    /* Getting here means everything was successful. We can now set some state before returning. */
    dr_strcpy_s(pState->folderPath, sizeof(pState->folderPath), pFolderPath);
    dr_append_path(pState->absolutePath, sizeof(pState->absolutePath), pState->folderPath, pState->relativePath);

    return pState;
}

dr_file_iterator* dr_file_iterator_next(dr_file_iterator* pState)
{
#ifdef _WIN32
    WIN32_FIND_DATAA ffd;
#endif

    if (pState == NULL) {
        return NULL;
    }

#ifdef _WIN32
    if (!FindNextFileA(pState->hFind, &ffd)) {
        /* Couldn't find anything else. */
        FindClose(pState->hFind);
        return NULL;
    }

    /* Found something. */
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        pState->isDirectory = 1;
    } else {
        pState->isDirectory = 0;
    }

    strcpy_s(pState->relativePath, sizeof(pState->relativePath), ffd.cFileName);
#else
    return NULL; /* Not yet implemented. */
#endif

    /* Success */
    dr_append_path(pState->absolutePath, sizeof(pState->absolutePath), pState->folderPath, pState->relativePath);
    return pState;
}


/*
File Management

Free file data with free().
*/
FILE* dr_fopen(const char* filePath, const char* openMode)
{
    FILE* pFile;
#ifdef _MSC_VER
    if (fopen_s(&pFile, filePath, openMode) != 0) {
        return NULL;
    }
#else
    pFile = fopen(filePath, openMode);
    if (pFile == NULL) {
        return NULL;
    }
#endif

    return pFile;
}

void* dr_open_and_read_file_with_extra_data(const char* pFilePath, size_t* pFileSizeOut, size_t extraBytes)
{
    FILE* pFile;
    size_t fileSize;
    size_t bytesRead;
    void* pFileData;

    /* Safety. */
    if (pFileSizeOut) {
        *pFileSizeOut = 0;   
    }

    if (pFilePath == NULL) {
        return NULL;
    }

    /* TODO: Use 64-bit versions of the FILE APIs. */

    pFile = dr_fopen(pFilePath, "rb");
    if (pFile == NULL) {
        return NULL;
    }

    fseek(pFile, 0, SEEK_END);
    fileSize = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    if (fileSize + extraBytes > SIZE_MAX) {
        fclose(pFile);
        return NULL;    /* File is too big. */
    }

    pFileData = malloc((size_t)fileSize + extraBytes);    /* <-- Safe cast due to the check above. */
    if (pFileData == NULL) {
        fclose(pFile);
        return NULL;    /* Failed to allocate memory for the file. Good chance the file is too big. */
    }

    bytesRead = fread(pFileData, 1, (size_t)fileSize, pFile);
    if (bytesRead != fileSize) {
        free(pFileData);
        fclose(pFile);
        return NULL;    /* Failed to read every byte from the file. */
    }

    fclose(pFile);

    if (pFileSizeOut) {
        *pFileSizeOut = (size_t)fileSize;
    }

    return pFileData;
}

void* dr_open_and_read_file(const char* pFilePath, size_t* pFileSizeOut)
{
    return dr_open_and_read_file_with_extra_data(pFilePath, pFileSizeOut, 0);
}


dr_bool32 dr_argv_is_set(int argc, char** argv, const char* value)
{
    int iarg;
    for (iarg = 0; iarg < argc; ++iarg) {
        if (strcmp(argv[iarg], value) == 0) {
            return DR_TRUE;
        }
    }

    return DR_FALSE;
}


int dr_vprintf_fixed(int width, const char* const format, va_list args)
{
    int i;
    int len;
    char buffer[4096];

    if (width <= 0) {
        return -1;  /* Width cannot be negative or 0. */
    }

    if (width > sizeof(buffer)) {
        return -1;  /* Width is too big. */
    }
    
    /* We need to print this into a string (truncated). */
#if defined(_MSC_VER) || ((defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500) || defined(_ISOC99_SOURCE) || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L))
    len = vsnprintf(buffer, width+1, format, args);
#else
    len = vsprintf(buffer, format, args);
#endif

    printf("%s", buffer);
    for (i = len; i < width; ++i) {
        printf(" ");
    }

    return len;
}

int dr_printf_fixed(int width, const char* const format, ...)
{
    int result;

    va_list args;
    va_start(args, format);
    {
        result = dr_vprintf_fixed(width, format, args);
    }
    va_end(args);

    return result;
}

int dr_vprintf_fixed_with_margin(int width, int margin, const char* const format, va_list args)
{
    int i;

    /* Margin. */
    for (i = 0; i < margin; ++i) {
        printf(" ");
    }

    return dr_vprintf_fixed(width - margin, format, args);
}

int dr_printf_fixed_with_margin(int width, int margin, const char* const format, ...)
{
    int result;
    
    va_list args;
    va_start(args, format);
    {
        result = dr_vprintf_fixed_with_margin(width, margin, format, args);
    }
    va_end(args);

    return result;
}



#ifdef _WIN32
static LARGE_INTEGER g_DRTimerFrequency = {{0}};
double dr_timer_now()
{
    LARGE_INTEGER counter;

    if (g_DRTimerFrequency.QuadPart == 0) {
        QueryPerformanceFrequency(&g_DRTimerFrequency);
    }

    QueryPerformanceCounter(&counter);

    return counter.QuadPart / (double)g_DRTimerFrequency.QuadPart;
}
#else
#if _POSIX_C_SOURCE >= 199309L
    #if defined(CLOCK_MONOTONIC)
        #define MA_CLOCK_ID CLOCK_MONOTONIC
    #else
        #define MA_CLOCK_ID CLOCK_REALTIME
    #endif
    double dr_timer_now()
    {
        struct timespec newTime;
        clock_gettime(CLOCK_MONOTONIC, &newTime);

        return ((newTime.tv_sec * 1000000000LL) + newTime.tv_nsec) / 1000000000.0;
    }
#else
    double dr_timer_now()
    {
        struct timeval newTime;
        gettimeofday(&newTime, NULL);

        return ((newTime.tv_sec * 1000000) + newTime.tv_usec) / 1000000.0;
    }
#endif
#endif


float dr_scale_to_range_f32(float x, float lo, float hi)
{
    return lo + x*(hi-lo);
}


#define DR_LCG_M   2147483647
#define DR_LCG_A   48271
#define DR_LCG_C   0
static int g_maLCG;

void dr_seed(int seed)
{
    g_maLCG = seed;
}

int dr_rand_s32()
{
    int lcg = g_maLCG;
    int r = (DR_LCG_A * lcg + DR_LCG_C) % DR_LCG_M;
    g_maLCG = r;
    return r;
}

unsigned int dr_rand_u32()
{
    return (unsigned int)dr_rand_s32();
}

double dr_rand_f64()
{
    return dr_rand_s32() / (double)0x7FFFFFFF;
}

float dr_rand_f32()
{
    return (float)dr_rand_f64();
}

float dr_rand_range_f32(float lo, float hi)
{
    return dr_scale_to_range_f32(dr_rand_f32(), lo, hi);
}

int dr_rand_range_s32(int lo, int hi)
{
    if (lo == hi) {
        return lo;
    }

    return lo + dr_rand_u32() / (0xFFFFFFFF / (hi - lo + 1) + 1);
}