
#ifdef _WIN32
#include <windows.h>
#endif

#if defined(_MSC_VER) || defined(__DMC__)
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>   /* So we can seed the random number generator based on time. */
#include <errno.h>

#if !defined(_WIN32)
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <dlfcn.h>
#endif

#include <stddef.h> /* For size_t. */

/* Sized types. Prefer built-in types. Fall back to stdint. */
#ifdef _MSC_VER
    #if defined(__clang__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wlanguage-extension-token"
        #pragma GCC diagnostic ignored "-Wlong-long"
        #pragma GCC diagnostic ignored "-Wc++11-long-long"
    #endif
    typedef   signed __int8  dr_int8;
    typedef unsigned __int8  dr_uint8;
    typedef   signed __int16 dr_int16;
    typedef unsigned __int16 dr_uint16;
    typedef   signed __int32 dr_int32;
    typedef unsigned __int32 dr_uint32;
    typedef   signed __int64 dr_int64;
    typedef unsigned __int64 dr_uint64;
    #if defined(__clang__)
        #pragma GCC diagnostic pop
    #endif
#else
    #define MA_HAS_STDINT
    #include <stdint.h>
    typedef int8_t   dr_int8;
    typedef uint8_t  dr_uint8;
    typedef int16_t  dr_int16;
    typedef uint16_t dr_uint16;
    typedef int32_t  dr_int32;
    typedef uint32_t dr_uint32;
    typedef int64_t  dr_int64;
    typedef uint64_t dr_uint64;
#endif

#ifdef MA_HAS_STDINT
    typedef uintptr_t dr_uintptr;
#else
    #if defined(_WIN32)
        #if defined(_WIN64)
            typedef dr_uint64 dr_uintptr;
        #else
            typedef dr_uint32 dr_uintptr;
        #endif
    #elif defined(__GNUC__)
        #if defined(__LP64__)
            typedef dr_uint64 dr_uintptr;
        #else
            typedef dr_uint32 dr_uintptr;
        #endif
    #else
        typedef dr_uint64 dr_uintptr;   /* Fallback. */
    #endif
#endif

typedef dr_uint8    dr_bool8;
typedef dr_uint32   dr_bool32;
#define DR_TRUE     1
#define DR_FALSE    0

typedef void* dr_handle;
typedef void* dr_ptr;
typedef void (* dr_proc)(void);

#if defined(SIZE_MAX)
    #define DR_SIZE_MAX SIZE_MAX
#else
    #define DR_SIZE_MAX 0xFFFFFFFF
#endif

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

#if defined(_MSC_VER) || defined(__DMC__)
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
    DIR* dir;
#endif
} dr_file_iterator;

dr_file_iterator* dr_file_iterator_begin(const char* pFolderPath, dr_file_iterator* pState)
{
#ifdef _WIN32
    char searchQuery[MAX_PATH];
    unsigned int searchQueryLength;
    WIN32_FIND_DATAA ffd;
    HANDLE hFind;
#else
    DIR* dir;
#endif

    if (pState == NULL) {
        return NULL;
    }

    memset(pState, 0, sizeof(*pState));

    if (pFolderPath == NULL) {
        return NULL;
    }

#ifdef _WIN32
    dr_strcpy_s(searchQuery, sizeof(searchQuery), pFolderPath);

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
            FindClose(hFind);
            return NULL;    /* Couldn't find anything. */
        }
    }

    pState->hFind = hFind;


    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        pState->isDirectory = 1;
    } else {
        pState->isDirectory = 0;
    }

    dr_strcpy_s(pState->relativePath, sizeof(pState->relativePath), ffd.cFileName);
#else
    dir = opendir(pFolderPath);
    if (dir == NULL) {
        return NULL;    /* Couldn't find anything. */
    }

    /* Select the first file. */
    for (;;) {
        struct dirent* info;
        struct stat fileinfo;
        char filePath[4096];

        info = readdir(dir);
        if (info == NULL) {
            closedir(dir);
            return NULL;
        }

        if (strcmp(info->d_name, ".") == 0 || strcmp(info->d_name, "..") == 0) {
            continue;   /* Skip past "." and ".." directories. */
        }

        dr_strcpy_s(pState->relativePath, sizeof(pState->relativePath), info->d_name);
        dr_append_path(filePath, sizeof(filePath), pFolderPath, pState->relativePath);

        if (stat(filePath, &fileinfo) != 0) {
            continue;
        }

        if (S_ISDIR(fileinfo.st_mode)) {
            pState->isDirectory = 1;
        } else {
            pState->isDirectory = 0;
        }

        break;
    }

    pState->dir = dir;
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
        pState->hFind = NULL;
        return NULL;
    }

    /* Found something. */
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        pState->isDirectory = 1;
    } else {
        pState->isDirectory = 0;
    }

    dr_strcpy_s(pState->relativePath, sizeof(pState->relativePath), ffd.cFileName);
#else
    /* Enter a loop here so we can skip past "." and ".." directories. */
    for (;;) {
        struct dirent* info;
        struct stat fileinfo;
        char filePath[4096];

        info = readdir(pState->dir);
        if (info == NULL) {
            closedir(pState->dir);
            pState->dir = NULL;
            return NULL;
        }

        if (strcmp(info->d_name, ".") == 0 || strcmp(info->d_name, "..") == 0) {
            continue;   /* Skip past "." and ".." directories. */
        }

        dr_strcpy_s(pState->relativePath, sizeof(pState->relativePath), info->d_name);
        dr_append_path(filePath, sizeof(filePath), pState->folderPath, pState->relativePath);

        /*printf("Done: %s\n", pState->relativePath);*/

        if (stat(filePath, &fileinfo) != 0) {
            continue;
        }

        if (S_ISDIR(fileinfo.st_mode)) {
            pState->isDirectory = 1;
        } else {
            pState->isDirectory = 0;
        }

        break;
    }
#endif

    /* Success */
    dr_append_path(pState->absolutePath, sizeof(pState->absolutePath), pState->folderPath, pState->relativePath);
    return pState;
}

void dr_file_iterator_end(dr_file_iterator* pState)
{
    if (pState == NULL) {
        return;
    }

#ifdef _WIN32
    FindClose(pState->hFind);
    pState->hFind = NULL;
#else
    closedir(pState->dir);
    pState->dir = NULL;
#endif
}


/*
File Management

Free file data with free().
*/
static int dr_fopen(FILE** ppFile, const char* pFilePath, const char* pOpenMode)
{
#if defined(_MSC_VER) && _MSC_VER >= 1400
    errno_t err;
#endif

    if (ppFile != NULL) {
        *ppFile = NULL;  /* Safety. */
    }

    if (pFilePath == NULL || pOpenMode == NULL || ppFile == NULL) {
        return -1;  /* Invalid args. */
    }

#if defined(_MSC_VER) && _MSC_VER >= 1400
    err = fopen_s(ppFile, pFilePath, pOpenMode);
    if (err != 0) {
        return err;
    }
#else
#if defined(_WIN32) || defined(__APPLE__)
    *ppFile = fopen(pFilePath, pOpenMode);
#else
    #if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64 && defined(_LARGEFILE64_SOURCE)
        *ppFile = fopen64(pFilePath, pOpenMode);
    #else
        *ppFile = fopen(pFilePath, pOpenMode);
    #endif
#endif
    if (*ppFile == NULL) {
        return errno;
    }
#endif

    return 0;
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

    if (dr_fopen(&pFile, pFilePath, "rb") != 0) {
        return NULL;
    }

    fseek(pFile, 0, SEEK_END);
    fileSize = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    /* Need to make sure we have enough room for the extra bytes, if any. */
    if (fileSize == DR_SIZE_MAX && extraBytes > 0) {
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

    if ((unsigned int)width > sizeof(buffer)) {
        return -1;  /* Width is too big. */
    }
    
    /* We need to print this into a string (truncated). */
#if (defined(_MSC_VER) && _MSC_VER > 1200) || ((defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500) || defined(_ISOC99_SOURCE) || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L))
    len = vsnprintf(buffer, width+1, format, args);
#else
    len = vsprintf(buffer, format, args);
    if (len > width) {
        len = width;
    }
    
    buffer[len] = '\0';
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
static int g_drLCG;

void dr_seed(int seed)
{
    g_drLCG = seed;
}

int dr_rand_s32()
{
    int lcg = g_drLCG;
    int r = (DR_LCG_A * lcg + DR_LCG_C) % DR_LCG_M;
    g_drLCG = r;
    return r;
}

unsigned int dr_rand_u32()
{
    return (unsigned int)dr_rand_s32();
}

dr_uint64 dr_rand_u64()
{
    return ((dr_uint64)dr_rand_u32() << 32) | dr_rand_u32();
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

dr_uint64 dr_rand_range_u64(dr_uint64 lo, dr_uint64 hi)
{
    if (lo == hi) {
        return lo;
    }

    return lo + dr_rand_u64() / ((~(dr_uint64)0) / (hi - lo + 1) + 1);
}



void dr_pcm_s32_to_f32(void* dst, const void* src, dr_uint64 count)
{
    float* dst_f32 = (float*)dst;
    const dr_int32* src_s32 = (const dr_int32*)src;

    dr_uint64 i;
    for (i = 0; i < count; i += 1) {
        double x = src_s32[i];

#if 0
        x = x + 2147483648.0;
        x = x * 0.0000000004656612873077392578125;
        x = x - 1;
#else
        x = x / 2147483648.0;
#endif

        dst_f32[i] = (float)x;
    }
}

void dr_pcm_s32_to_s16(void* dst, const void* src, dr_uint64 count)
{
    dr_int16* dst_s16 = (dr_int16*)dst;
    const dr_int32* src_s32 = (const dr_int32*)src;

    dr_uint64 i;
    for (i = 0; i < count; i += 1) {
        dr_int32 x = src_s32[i];
        x = x >> 16;
        dst_s16[i] = (dr_int16)x;
    }
}




dr_handle dr_dlopen(const char* filename)
{
    dr_handle handle;

#ifdef _WIN32
    handle = (dr_handle)LoadLibraryA(filename);
#else
    handle = (dr_handle)dlopen(filename, RTLD_NOW);
#endif

    return handle;
}

void dr_dlclose(dr_handle handle)
{
#ifdef _WIN32
    FreeLibrary((HMODULE)handle);
#else
    dlclose((void*)handle);
#endif
}

dr_proc dr_dlsym(dr_handle handle, const char* symbol)
{
    dr_proc proc;

#ifdef _WIN32
    proc = (dr_proc)GetProcAddress((HMODULE)handle, symbol);
#else
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
#endif
    proc = (dr_proc)dlsym((void*)handle, symbol);
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
    #pragma GCC diagnostic pop
#endif
#endif

    return proc;
}
