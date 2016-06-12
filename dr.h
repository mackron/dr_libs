// Public Domain. See "unlicense" statement at the end of this file.

// USAGE
//
// This is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_IMPLEMENTATION
//   #include "dr.h"
//
// You can then #include dr.h in other parts of the program as you would with any other header file.
//
//
//
// OPTIONS
//
// #define DR_UTIL_WIN32_USE_CRITICAL_SECTION_MUTEX
//   - Use the Win32 CRITICAL_SECTION API for mutex objects.

#ifndef dr_util_h
#define dr_util_h

#ifdef __cplusplus
extern "C" {
#endif

// Disable MSVC compatibility if we're compiling with it.
#if defined(_MSC_VER)
    #define DR_NO_MSVC_COMPAT
#endif

#if defined(_MSC_VER)
#define DR_INLINE static __inline
#else
#define DR_INLINE static inline
#endif


#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef DR_NO_MSVC_COMPAT
#include <errno.h>
#endif


#define STRINGIFY(x)    #x
#define TOSTRING(x)     STRINGIFY(x)


/////////////////////////////////////////////////////////
// Annotations

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)x)
#endif


/////////////////////////////////////////////////////////
// min/max/clamp

#ifndef dr_min
#define dr_min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef dr_max
#define dr_max(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef dr_clamp
#define dr_clamp(x, low, high) (dr_max(low, dr_min(x, high)))
#endif


/////////////////////////////////////////////////////////
// MSVC Compatibility

// A basic implementation of MSVC's strcpy_s().
int dr_strcpy_s(char* dst, size_t dstSizeInBytes, const char* src);

// A basic implementation of MSVC's strncpy_s().
int dr_strncpy_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count);

// A basic implementation of MSVC's strcat_s().
int dr_strcat_s(char* dst, size_t dstSizeInBytes, const char* src);

// A basic implementation of MSVC's strncat_s()
int dr_strncat_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count);

// A basic implementation of MSVC's _atoi_s()
int dr_itoa_s(int value, char* dst, size_t dstSizeInBytes, int radix);

#ifndef DR_NO_MSVC_COMPAT
#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif

DR_INLINE int strcpy_s(char* dst, size_t dstSizeInBytes, const char* src)
{
    return dr_strcpy_s(dst, dstSizeInBytes, src);
}

DR_INLINE int strncpy_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
    return dr_strncpy_s(dst, dstSizeInBytes, src, count);
}

DR_INLINE int strcat_s(char* dst, size_t dstSizeInBytes, const char* src)
{
    return dr_strcat_s(dst, dstSizeInBytes, src);
}

DR_INLINE int strncat_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
    return dr_strncat_s(dst, dstSizeInBytes, src, count);
}

#ifndef __MINGW32__
DR_INLINE int _stricmp(const char* string1, const char* string2)
{
    return strcasecmp(string1, string2);
}
#endif

DR_INLINE int _itoa_s(int value, char* dst, size_t dstSizeInBytes, int radix)
{
    return dr_itoa_s(value, dst, dstSizeInBytes, radix);
}
#endif


/////////////////////////////////////////////////////////
// String Helpers

/// Removes every occurance of the given character from the given string.
void dr_strrmchar(char* str, char c);

/// Finds the first non-whitespace character in the given string.
const char* dr_first_non_whitespace(const char* str);

/// Finds the first occurance of a whitespace character in the given string.
const char* dr_first_whitespace(const char* str);

/// Finds the beginning of the next line.
const char* dr_next_line(const char* str);

/// Makes a copy of the first line of the given string.
size_t dr_copy_line(const char* str, char* lineOut, size_t lineOutSize);


/////////////////////////////////////////////////////////
// Unicode Utilities

/// Converts a UTF-32 character to UTF-16.
///
/// @param utf16 [in] A pointer to an array of at least two 16-bit values that will receive the UTF-16 character.
///
/// @return 2 if the returned character is a surrogate pair, 1 if it's a simple UTF-16 code point, or 0 if it's an invalid character.
///
/// @remarks
///     It is assumed the <utf16> is large enough to hold at least 2 unsigned shorts. <utf16> will be padded with 0 for unused
///     components.
DR_INLINE int dr_utf32_to_utf16_ch(unsigned int utf32, unsigned short utf16[2])
{
    if (utf16 == NULL) {
        return 0;
    }

    if (utf32 < 0xD800 || (utf32 >= 0xE000 && utf32 <= 0xFFFF))
    {
        utf16[0] = (unsigned short)utf32;
        utf16[1] = 0;
        return 1;
    }
    else
    {
        if (utf32 >= 0x10000 && utf32 <= 0x10FFFF)
        {
            utf16[0] = (unsigned short)(0xD7C0 + (unsigned short)(utf32 >> 10));
            utf16[1] = (unsigned short)(0xDC00 + (unsigned short)(utf32 & 0x3FF));
            return 2;
        }
        else
        {
            // Invalid.
            utf16[0] = 0;
            utf16[0] = 0;
            return 0;
        }
    }
}

/// Converts a UTF-16 character to UTF-32.
DR_INLINE unsigned int dr_utf16_to_utf32_ch(unsigned short utf16[2])
{
    if (utf16 == NULL) {
        return 0;
    }

    if (utf16[0] < 0xD800 || utf16[0] > 0xDFFF)
    {
        return utf16[0];
    }
    else
    {
        if ((utf16[0] & 0xFC00) == 0xD800 && (utf16[1] & 0xFC00) == 0xDC00)
        {
            return ((unsigned int)utf16[0] << 10) + utf16[1] - 0x35FDC00;
        }
        else
        {
            // Invalid.
            return 0;
        }
    }
}

/// Converts a UTF-16 surrogate pair to UTF-32.
DR_INLINE unsigned int dr_utf16pair_to_utf32_ch(unsigned short utf160, unsigned short utf161)
{
    unsigned short utf16[2];
    utf16[0] = utf160;
    utf16[1] = utf161;
    return dr_utf16_to_utf32_ch(utf16);
}


/////////////////////////////////////////////////////////
// Aligned Allocations

#ifndef DRUTIL_NO_ALIGNED_MALLOC
DR_INLINE void* dr_aligned_malloc(size_t alignment, size_t size)
{
#if defined(_WIN32) || defined(_WIN64)
    return _aligned_malloc(size, alignment);
#else
    void* pResult;
    if (posix_memalign(&pResult, alignment, size) == 0) {
        return pResult;
    }

    return 0;
#endif
}

DR_INLINE void dr_aligned_free(void* ptr)
{
#if defined(_WIN32) || defined(_WIN64)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}
#endif  // !DRUTIL_NO_ALIGNED_MALLOC



/////////////////////////////////////////////////////////
// Key/Value Pair Parsing

typedef size_t (* dr_key_value_read_proc) (void* pUserData, void* pDataOut, size_t bytesToRead);
typedef void   (* dr_key_value_pair_proc) (void* pUserData, const char* key, const char* value);
typedef void   (* dr_key_value_error_proc)(void* pUserData, const char* message, unsigned int line);

/// Parses a series of simple Key/Value pairs.
///
/// @remarks
///     This function is suitable for parsing simple key/value config files.
///     @par
///     This function will never allocate memory on the heap. Because of this there is a minor restriction in the length of an individual
///     key/value pair which is 4KB.
///     @par
///     Formatting rules are as follows:
///      - The basic syntax for a key/value pair is [key][whitespace][value]. Example: MyProperty 1234
///      - All key/value pairs must be declared on a single line, and a single line cannot contain more than a single key/value pair.
///      - Comments begin with the '#' character and continue until the end of the line.
///      - A key cannot contain spaces but are permitted in values.
///      - The value will have any leading and trailing whitespace trimmed.
///      - A value can be wrapped in double-quote characters in which case the last double-quote character acts as the end point. The quotes
///        will be trimmed.
///     @par
///     If an error occurs, that line will be skipped and processing will continue.
void dr_parse_key_value_pairs(dr_key_value_read_proc onRead, dr_key_value_pair_proc onPair, dr_key_value_error_proc onError, void* pUserData);

// This will only return false if the file fails to open. It will still return true even if there are syntax error or whatnot.
bool dr_parse_key_value_pairs_from_file(const char* filePath, dr_key_value_pair_proc onPair, dr_key_value_error_proc onError, void* pUserData);



/////////////////////////////////////////////////////////
// Basic Tokenizer

/// Retrieves the first token in the given string.
///
/// @remarks
///     This function is suitable for doing a simple whitespace tokenization of a null-terminated string.
///     @par
///     The return value is a pointer to one character past the last character of the next token. You can use the return value to execute
///     this function in a loop to parse an entire string.
///     @par
///     <tokenOut> can be null. If the buffer is too small to contain the entire token it will be set to an empty string. The original
///     input string combined with the return value can be used to reliably find the token.
///     @par
///     This will handle double-quoted strings, so a string such as "My \"Complex String\"" contains two tokens: "My" and "\"Complex String\"".
///     @par
///     This function has no dependencies.
const char* dr_next_token(const char* tokens, char* tokenOut, unsigned int tokenOutSize);



/////////////////////////////////////////////////////////
// Known Folders

/// Retrieves the path of the executable.
///
/// @remarks
///     Currently only works on Windows and Linux. Other platforms will be added as they're needed.
bool dr_get_executable_path(char* pathOut, size_t pathOutSize);

/// Retrieves the path of the directory containing the executable.
///
/// @remarks
///     Currently only works on Windows and Linux. Other platforms will be added as they're needed.
bool dr_get_executable_directory_path(char* pathOut, size_t pathOutSize);

/// Retrieves the path of the user's config directory.
///
/// @remarks
///     On Windows this will typically be %APPDATA% and on Linux it will usually be ~/.config
bool dr_get_config_folder_path(char* pathOut, size_t pathOutSize);

/// Retrieves the path of the user's log directory.
///
/// @remarks
///     On Windows this will typically be %APPDATA% and on Linux it will usually be var/log
bool dr_get_log_folder_path(char* pathOut, size_t pathOutSize);

/// Retrieves the current directory.
const char* dr_get_current_directory(char* pathOut, size_t pathOutSize);

/// Sets the current directory.
bool dr_set_current_directory(const char* path);



/////////////////////////////////////////////////////////
// Basic File Management

// Helper for opening a stdio FILE.
FILE* dr_fopen(const char* fileName, const char* openMode);

// Retrieves the file data of the given file. Free the returned pointer with dr_free_file_data().
void* dr_open_and_read_file(const char* filePath, size_t* pFileSizeOut);

// Retrieves the file data of the given file as a null terminated string. Free the returned pointer with dr_free_file_data(). The
// returned file size is the length of the string not including the null terminator.
char* dr_open_and_read_text_file(const char* filePath, size_t* pFileSizeOut);

// Frees the file data returned by dr_open_and_read_file().
void dr_free_file_data(void* valueReturnedByOpenAndReadFile);

// Determines whether or not the given file path is to a file.
//
// This will return false if the path points to a directory.
bool dr_file_exists(const char* filePath);

// Determines whether or not the given file path points to a directory.
//
// This will return false if the path points to a file.
bool dr_directory_exists(const char* directoryPath);

// Moves a file.
//
// This uses rename() on POSIX platforms and MoveFileEx(oldPath, newPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH).
bool dr_move_file(const char* oldPath, const char* newPath);


/////////////////////////////////////////////////////////
// DPI Awareness

#if defined(_WIN32)
/// Win32 Only: Makes the application DPI aware.
void dr_win32_make_dpi_aware();

/// Win32 Only: Retrieves the base DPI to use as a reference when calculating DPI scaling.
void dr_win32_get_base_dpi(int* pDPIXOut, int* pDPIYOut);

/// Win32 Only: Retrieves the system-wide DPI.
void dr_win32_get_system_dpi(int* pDPIXOut, int* pDPIYOut);

/// Win32 Only: Retrieves the actual DPI of the monitor at the given index.
///
/// @remarks
///     If per-monitor DPI is not supported, the system wide DPI settings will be used instead.
///     @par
///     This runs in linear time.
void dr_win32_get_monitor_dpi(int monitor, int* pDPIXOut, int* pDPIYOut);

/// Win32 Only: Retrieves the number of monitors active at the time of calling.
///
/// @remarks
///     This runs in linear time.
int dr_win32_get_monitor_count();
#endif



/////////////////////////////////////////////////////////
// Date / Time

/// Retrieves a time_t as of the time the function was called.
time_t dr_now();

/// Formats a data/time string.
void dr_datetime_short(time_t t, char* strOut, unsigned int strOutSize);

// Returns a date string in YYYYMMDD format.
void dr_date_YYYYMMDD(time_t t, char* strOut, unsigned int strOutSize);



/////////////////////////////////////////////////////////
// Command Line
//
// The command line functions below are just simple iteration functions. This command line system is good for
// simple command lines, but probably not the best for programs requiring complex command line work.
//
// For argv style command lines, parse_cmdline() will run without any heap allocations. With a Win32 style
// command line there will be one malloc() per call fo parse_cmdline(). This is the only function that will do
// a malloc().
//
// Below is an example:
//
// dr_cmdline cmdline;
// if (dr_init_cmdline(&cmdline, argc, argv)) {
//     dr_parse_cmdline(&cmdline, my_cmdline_handler, pMyUserData);
// }
//
// void my_cmdline_handler(const char* key, const char* value, void* pUserData)
// {
//     // Do something...
// }
//
//
// When parsing the command line, the first iteration will be the program path and the key will be "[path]".
//
// For segments such as "-abcd", the callback will be called for "a", "b", "c", "d" individually, with the
// value set to NULL.
//
// For segments such as "--server", the callback will be called for "server", with the value set to NULL.
//
// For segments such as "-f file.txt", the callback will be called with the key set to "f" and the value set
// to "file.txt".
//
// For segments such as "-f file1.txt file2.txt", the callback will be called twice, once for file1.txt and
// again for file2.txt, with with the key set to "f" in both cases.
//
// For segments where there is no leading key, the values will be posted as annonymous (key set to NULL). An example
// is "my_program.exe file1.txt file2.txt", in which case the first iteration will be the program path, the second iteration
// will be "file1.txt", with the key set to NULL. The third iteration will be "file2.txt" with the key set to NULL.
//
// For segments such as "-abcd file.txt", "a", "b", "c", "d" will be sent with NULL values, and "file.txt" will be
// posted with a NULL key.

typedef struct dr_cmdline dr_cmdline;
struct dr_cmdline
{
    // argv style.
    int argc;
    char** argv;

    // Win32 style
    const char* win32;

};

typedef bool dr_cmdline_parse_proc(const char* key, const char* value, void* pUserData);


/// Initializes a command line object.
bool dr_init_cmdline(dr_cmdline* pCmdLine, int argc, char** argv);

/// Initializes a command line object using a Win32 style command line.
bool dr_init_cmdline_win32(dr_cmdline* pCmdLine, const char* args);

/// Parses the given command line.
void dr_parse_cmdline(dr_cmdline* pCmdLine, dr_cmdline_parse_proc callback, void* pUserData);

/// Helper for determining whether or not the given key exists.
bool dr_cmdline_key_exists(dr_cmdline* pCmdLine, const char* key);



/////////////////////////////////////////////////////////
// Threading

/// Puts the calling thread to sleep for approximately the given number of milliseconds.
///
/// @remarks
///     This is not 100% accurate and should be considered an approximation.
void dr_sleep(unsigned int milliseconds);

/// Retrieves the number of logical cores on system.
unsigned int dr_get_logical_processor_count();


/// Thread.
typedef void* dr_thread;
typedef int (* dr_thread_entry_proc)(void* pData);

/// Creates and begins executing a new thread.
///
/// @remarks
///     This will not return until the thread has entered into it's entry point.
///     @par
///     Creating a thread should be considered an expensive operation. For high performance, you should create threads
///     at load time and cache them.
dr_thread dr_create_thread(dr_thread_entry_proc entryProc, void* pData);

/// Deletes the given thread.
///
/// @remarks
///     This does not actually exit the thread, but rather deletes the memory that was allocated for the thread
///     object returned by dr_create_thread().
///     @par
///     It is usually best to wait for the thread to terminate naturally with dr_wait_thread() before calling
///     this function, however it is still safe to do something like the following.
///     @code
///     dr_delete_thread(dr_create_thread(my_thread_proc, pData))
///     @endcode
void dr_delete_thread(dr_thread thread);

/// Waits for the given thread to terminate.
void dr_wait_thread(dr_thread thread);

/// Helper function for waiting for a thread and then deleting the handle after it has terminated.
void dr_wait_and_delete_thread(dr_thread thread);



/// Mutex
typedef void* dr_mutex;

/// Creates a mutex object.
///
/// @remarks
///     If an error occurs, 0 is returned. Otherwise a handle the size of a pointer is returned.
dr_mutex dr_create_mutex();

/// Deletes a mutex object.
void dr_delete_mutex(dr_mutex mutex);

/// Locks the given mutex.
void dr_lock_mutex(dr_mutex mutex);

/// Unlocks the given mutex.
void dr_unlock_mutex(dr_mutex mutex);



/// Semaphore
typedef void* dr_semaphore;

/// Creates a semaphore object.
///
/// @remarks
///     If an error occurs, 0 is returned. Otherwise a handle the size of a pointer is returned.
dr_semaphore dr_create_semaphore(int initialValue);

/// Deletes the given semaphore.
void dr_delete_semaphore(dr_semaphore semaphore);

/// Waits on the given semaphore object and decrements it's counter by one upon returning.
bool dr_wait_semaphore(dr_semaphore semaphore);

/// Releases the given semaphore and increments it's counter by one upon returning.
bool dr_release_semaphore(dr_semaphore semaphore);



/////////////////////////////////////////////////////////
// Timing

typedef struct
{
    int64_t counter;
} dr_timer;

// Initializes a high-resolution timer.
void dr_timer_init(dr_timer* pTimer);

// Ticks the timer and returns the number of seconds since the previous tick.
//
// The maximum return value is about 140 years or so.
double dr_timer_tick(dr_timer* pTimer);




/////////////////////////////////////////////////////////
// Random

// Generates a random double between 0 and 1. This is bassed of C standard rand().
double dr_randd();

// Generates a random float between 0 and 1. This is based off C standard rand().
float dr_randf();



/////////////////////////////////////////////////////////
// Miscellaneous Stuff.

// Helper for clearing the given object to 0.
#define dr_zero_object(pObject) memset(pObject, 0, sizeof(*pObject));

// Converts an ASCII hex character to it's integral equivalent. Returns false if it's not a valid hex character.
bool dr_hex_char_to_uint(char ascii, unsigned int* out);


/////////////////////////////////////////////////////////
// C++ Specific

#ifdef __cplusplus

// Use this to prevent objects of the given class or struct from being copied. This is also useful for eliminating some
// compiler warnings.
//
// Note for structs - this sets the access mode to private, so place this at the end of the declaration.
#define NO_COPY(classname) \
    private: \
        classname(const classname &); \
        classname & operator=(const classname &);


#ifndef DR_NO_MSVC_COMPAT
extern "C++"
{

template <size_t dstSizeInBytes>
int strcpy_s(char (&dst)[dstSizeInBytes], const char* src)
{
    return strcpy_s(dst, dstSizeInBytes, src);
}

template <size_t dstSizeInBytes>
int strncpy_s(char (&dst)[dstSizeInBytes], const char* src, size_t count)
{
    return strncpy_s(dst, dstSizeInBytes, src, count);
}

template <size_t dstSizeInBytes>
int strcat_s(char (&dst)[dstSizeInBytes], const char* src)
{
    return strcat_s(dst, dstSizeInBytes, src);
}

template <size_t dstSizeInBytes>
int strncat_s(char (&dst)[dstSizeInBytes], const char* src, size_t count)
{
    return strncat_s(dst, dstSizeInBytes, src, count);
}

}
#endif

#endif



#ifdef __cplusplus
}
#endif

#endif  //dr_util_h



///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////

#ifdef DR_IMPLEMENTATION
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>     // For memmove()
#include <errno.h>

int dr_strcpy_s(char* dst, size_t dstSizeInBytes, const char* src)
{
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return ERANGE;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    size_t i;
    for (i = 0; i < dstSizeInBytes && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (i < dstSizeInBytes) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
}

int dr_strncpy_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return EINVAL;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    size_t maxcount = count;
    if (count == ((size_t)-1) || count >= dstSizeInBytes) {        // -1 = _TRUNCATE
        maxcount = dstSizeInBytes - 1;
    }

    size_t i;
    for (i = 0; i < maxcount && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (src[i] == '\0' || i == count || count == ((size_t)-1)) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
}

int dr_strcat_s(char* dst, size_t dstSizeInBytes, const char* src)
{
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return ERANGE;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    char* dstorig = dst;

    while (dstSizeInBytes > 0 && dst[0] != '\0') {
        dst += 1;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes == 0) {
        return EINVAL;  // Unterminated.
    }


    while (dstSizeInBytes > 0 && src[0] != '\0') {
        *dst++ = *src++;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes > 0) {
        dst[0] = '\0';
    } else {
        dstorig[0] = '\0';
        return ERANGE;
    }

    return 0;
}

int dr_strncat_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return ERANGE;
    }
    if (src == 0) {
        return EINVAL;
    }

    char* dstorig = dst;

    while (dstSizeInBytes > 0 && dst[0] != '\0') {
        dst += 1;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes == 0) {
        return EINVAL;  // Unterminated.
    }


    if (count == ((size_t)-1)) {        // _TRUNCATE
        count = dstSizeInBytes - 1;
    }

    while (dstSizeInBytes > 0 && src[0] != '\0' && count > 0)
    {
        *dst++ = *src++;
        dstSizeInBytes -= 1;
        count -= 1;
    }

    if (dstSizeInBytes > 0) {
        dst[0] = '\0';
    } else {
        dstorig[0] = '\0';
        return ERANGE;
    }

    return 0;
}

int dr_itoa_s(int value, char* dst, size_t dstSizeInBytes, int radix)
{
    if (dst == NULL || dstSizeInBytes == 0) {
        return EINVAL;
    }
    if (radix < 2 || radix > 36) {
        dst[0] = '\0';
        return EINVAL;
    }

    int sign = (value < 0 && radix == 10) ? -1 : 1;     // The negative sign is only used when the base is 10.

    unsigned int valueU;
    if (value < 0) {
        valueU = -value;
    } else {
        valueU = value;
    }

    char* dstEnd = dst;
    do
    {
        int remainder = valueU % radix;
        if (remainder > 9) {
            *dstEnd = (char)((remainder - 10) + 'a');
        } else {
            *dstEnd = (char)(remainder + '0');
        }

        dstEnd += 1;
        dstSizeInBytes -= 1;
        valueU /= radix;
    } while (dstSizeInBytes > 0 && valueU > 0);

    if (dstSizeInBytes == 0) {
        dst[0] = '\0';
        return EINVAL;  // Ran out of room in the output buffer.
    }

    if (sign < 0) {
        *dstEnd++ = '-';
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes == 0) {
        dst[0] = '\0';
        return EINVAL;  // Ran out of room in the output buffer.   
    }

    *dstEnd = '\0';


    // At this point the string will be reversed.
    dstEnd -= 1;
    while (dst < dstEnd) {
        char temp = *dst;
        *dst = *dstEnd;
        *dstEnd = temp;

        dst += 1;
        dstEnd -= 1;
    }

    return 0;
}

/////////////////////////////////////////////////////////
// String Helpers

void dr_strrmchar(char* str, char c)
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

const char* dr_first_non_whitespace(const char* str)
{
    if (str == NULL) {
        return NULL;
    }

    while (str[0] != '\0' && !(str[0] != ' ' && str[0] != '\t' && str[0] != '\n' && str[0] != '\v' && str[0] != '\f' && str[0] != '\r')) {
        str += 1;
    }

    return str;
}

const char* dr_first_whitespace(const char* str)
{
    if (str == NULL) {
        return NULL;
    }

    while (str[0] != '\0' && (str[0] != ' ' && str[0] != '\t' && str[0] != '\n' && str[0] != '\v' && str[0] != '\f' && str[0] != '\r')) {
        str += 1;
    }

    return str;
}

const char* dr_next_line(const char* str)
{
    if (str == NULL) {
        return NULL;
    }

    while (str[0] != '\0' && (str[0] != '\n' && (str[0] != '\r' || str[1] == '\n'))) {
        str += 1;
    }

    if (str[0] == '\0') {
        return NULL;
    }

    if (str[0] == '\r') {
        return str + 2;
    }

    return str + 1;
}

size_t dr_copy_line(const char* str, char* lineOut, size_t lineOutSize)
{
    if (str == NULL) {
        return 0;
    }

    if (str == NULL) {
        return 0;
    }

    size_t length = 0;
    while (lineOutSize > 0 && str[0] != '\0' && (str[0] != '\n' && (str[0] != '\r' || str[1] == '\n'))) {
        *lineOut++ = *str++;
        lineOutSize -= 1;
        length += 1;
    }

    if (lineOutSize == 0) {
        return 0;
    }

    *lineOut = '\0';
    return length;
}


/////////////////////////////////////////////////////////
// Key/Value Pair Parsing

void dr_parse_key_value_pairs(dr_key_value_read_proc onRead, dr_key_value_pair_proc onPair, dr_key_value_error_proc onError, void* pUserData)
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
            while (pC < pChunkEnd && (pC[0] == ' ' || pC[0] == '\t' || pC[0] == '\r')) {
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
            while (pC < pChunkEnd && (pC[0] == ' ' || pC[0] == '\t' || pC[0] == '\r')) {
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
            while (pC < pChunkEnd && (pC[0] == ' ' || pC[0] == '\t' || pC[0] == '\r')) {
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
                    while (pC < pChunkEnd && (pC[0] == ' ' || pC[0] == '\t' || pC[0] == '\r')) {
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


typedef struct
{
    FILE* pFile;
    dr_key_value_pair_proc onPair;
    dr_key_value_error_proc onError;
    void* pOriginalUserData;
} dr_parse_key_value_pairs_from_file_data;

size_t dr_parse_key_value_pairs_from_file__on_read(void* pUserData, void* pDataOut, size_t bytesToRead)
{
    dr_parse_key_value_pairs_from_file_data* pData = (dr_parse_key_value_pairs_from_file_data*)pUserData;
    assert(pData != NULL);

    return fread(pDataOut, 1, bytesToRead, pData->pFile);
}

void dr_parse_key_value_pairs_from_file__on_pair(void* pUserData, const char* key, const char* value)
{
    dr_parse_key_value_pairs_from_file_data* pData = (dr_parse_key_value_pairs_from_file_data*)pUserData;
    assert(pData != NULL);

    pData->onPair(pData->pOriginalUserData, key, value);
}

void dr_parse_key_value_pairs_from_file__on_error(void* pUserData, const char* message, unsigned int line)
{
    dr_parse_key_value_pairs_from_file_data* pData = (dr_parse_key_value_pairs_from_file_data*)pUserData;
    assert(pData != NULL);

    pData->onError(pData->pOriginalUserData, message, line);
}

bool dr_parse_key_value_pairs_from_file(const char* filePath, dr_key_value_pair_proc onPair, dr_key_value_error_proc onError, void* pUserData)
{
    dr_parse_key_value_pairs_from_file_data data;
    data.pFile = dr_fopen(filePath, "rb");
    if (data.pFile == NULL) {
        if (onError) onError(pUserData, "Could not open file.", 0);
        return false;
    }

    data.onPair = onPair;
    data.onError = onError;
    data.pOriginalUserData = pUserData;
    dr_parse_key_value_pairs(dr_parse_key_value_pairs_from_file__on_read, dr_parse_key_value_pairs_from_file__on_pair, dr_parse_key_value_pairs_from_file__on_error, &data);

    fclose(data.pFile);
    return true;
}


/////////////////////////////////////////////////////////
// Basic Tokenizer

const char* dr_next_token(const char* tokens, char* tokenOut, unsigned int tokenOutSize)
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
            prevChar = strEnd[0];
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


    // If the output buffer is large enough to hold the token, copy the token into it. When we copy the token we need to
    // ensure we don't include the escape character.
    //assert(strEnd >= strBeg);

    while (tokenOutSize > 1 && strBeg < strEnd)
    {
        if (strBeg[0] == '\\' && strBeg[1] == '\"' && strBeg < strEnd) {
            strBeg += 1;
        }

        *tokenOut++ = *strBeg++;
        tokenOutSize -= 1;
    }

    // Null-terminate.
    if (tokenOutSize > 0) {
        *tokenOut = '\0';
    }


    // Skip past the double-quote character before returning.
    if (strEnd[0] == '\"') {
        strEnd += 1;
    }

    return strEnd;
}




/////////////////////////////////////////////////////////
// Known Folders

#if defined(_WIN32)
#include <shlobj.h>

bool dr_get_executable_path(char* pathOut, size_t pathOutSize)
{
    if (pathOut == NULL || pathOutSize == 0) {
        return 0;
    }

    DWORD length = GetModuleFileNameA(NULL, pathOut, (DWORD)pathOutSize);
    if (length == 0) {
        pathOut[0] = '\0';
        return false;
    }

    // Force null termination.
    if (length == pathOutSize) {
        pathOut[length - 1] = '\0';
    }

    // Back slashes need to be normalized to forward.
    while (pathOut[0] != '\0') {
        if (pathOut[0] == '\\') {
            pathOut[0] = '/';
        }

        pathOut += 1;
    }

    return true;
}

bool dr_get_executable_directory_path(char* pathOut, size_t pathOutSize)
{
    if (!dr_get_executable_path(pathOut, pathOutSize)) {
        return false;
    }

    // A null terminator needs to be placed at the last slash.
    char* lastSlash = pathOut;
    while (pathOut[0] != '\0') {
        if (pathOut[0] == '/' || pathOut[0] == '\\') {
            lastSlash = pathOut;
        }
        pathOut += 1;
    }

    lastSlash[0] = '\0';
    return true;
}

bool dr_get_config_folder_path(char* pathOut, size_t pathOutSize)
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

bool dr_get_log_folder_path(char* pathOut, size_t pathOutSize)
{
    return dr_get_config_folder_path(pathOut, pathOutSize);
}

const char* dr_get_current_directory(char* pathOut, size_t pathOutSize)
{
    DWORD result = GetCurrentDirectoryA((DWORD)pathOutSize, pathOut);
    if (result == 0) {
        return NULL;
    }

    return pathOut;
}

bool dr_set_current_directory(const char* path)
{
    return SetCurrentDirectoryA(path) != 0;
}
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

bool dr_get_executable_path(char* pathOut, size_t pathOutSize)
{
    if (pathOut == NULL || pathOutSize == 0) {
        return 0;
    }

    ssize_t length = readlink("/proc/self/exe", pathOut, pathOutSize);
    if (length == -1) {
        pathOut[0] = '\0';
        return false;
    }

    if ((size_t)length == pathOutSize) {
        pathOut[length - 1] = '\0';
    } else {
        pathOut[length] = '\0';
    }

    return true;
}

bool dr_get_config_folder_path(char* pathOut, size_t pathOutSize)
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

bool dr_get_log_folder_path(char* pathOut, size_t pathOutSize)
{
    return strcpy_s(pathOut, pathOutSize, "var/log");
}

const char* dr_get_current_directory(char* pathOut, size_t pathOutSize)
{
    return getcwd(pathOut, pathOutSize);
}

bool dr_set_current_directory(const char* path)
{
    return chdir(path) == 0;
}
#endif



/////////////////////////////////////////////////////////
// Basic File Management

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

static void* dr_open_and_read_file_with_extra_data(const char* filePath, size_t* pFileSizeOut, size_t extraBytes)
{
    if (pFileSizeOut) *pFileSizeOut = 0;   // For safety.

    if (filePath == NULL) {
        return NULL;
    }

    // TODO: Use 64-bit versions of the FILE APIs.

    FILE* pFile = dr_fopen(filePath, "rb");
    if (pFile == NULL) {
        return NULL;
    }

    fseek(pFile, 0, SEEK_END);
    uint64_t fileSize = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    if (fileSize + extraBytes > SIZE_MAX) {
        fclose(pFile);
        return NULL;    // File is too big.
    }

    void* pFileData = malloc((size_t)fileSize + extraBytes);    // <-- Safe cast due to the check above.
    if (pFileData == NULL) {
        fclose(pFile);
        return NULL;    // Failed to allocate memory for the file. Good chance the file is too big.
    }

    size_t bytesRead = fread(pFileData, 1, (size_t)fileSize, pFile);
    if (bytesRead != fileSize) {
        free(pFileData);
        fclose(pFile);
        return NULL;    // Failed to read every byte from the file.
    }

    fclose(pFile);

    if (pFileSizeOut) *pFileSizeOut = (size_t)fileSize;
    return pFileData;
}

void* dr_open_and_read_file(const char* filePath, size_t* pFileSizeOut)
{
    return dr_open_and_read_file_with_extra_data(filePath, pFileSizeOut, 0);
}

char* dr_open_and_read_text_file(const char* filePath, size_t* pFileSizeOut)
{
    if (pFileSizeOut) *pFileSizeOut = 0;   // For safety.

    size_t fileSize;
    char* pFileData = (char*)dr_open_and_read_file_with_extra_data(filePath, &fileSize, 1);     // <-- 1 extra byte for the null terminator.
    if (pFileData == NULL) {
        return NULL;
    }

    pFileData[fileSize] = '\0';

    if (pFileSizeOut) *pFileSizeOut = fileSize;
    return pFileData;
}

void dr_free_file_data(void* valueReturnedByOpenAndReadFile)
{
    free(valueReturnedByOpenAndReadFile);
}

bool dr_file_exists(const char* filePath)
{
    if (filePath == NULL) {
        return false;
    }

#if _WIN32
    DWORD attributes = GetFileAttributesA(filePath);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat info;
    if (stat(filePath, &info) != 0) {
        return false;   // Likely the folder doesn't exist.
    }

    return (info.st_mode & S_IFDIR) == 0;
#endif
}

bool dr_directory_exists(const char* directoryPath)
{
    if (directoryPath == NULL) {
        return false;
    }

#if _WIN32
    DWORD attributes = GetFileAttributesA(directoryPath);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    if (stat(directoryPath, &info) != 0) {
        return false;   // Likely the folder doesn't exist.
    }

    return (info.st_mode & S_IFDIR) != 0;
#endif
}

bool dr_move_file(const char* oldPath, const char* newPath)
{
    if (oldPath == NULL || newPath == NULL) {
        return false;
    }

#if _WIN32
    return MoveFileExA(oldPath, newPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
#else
    return rename(oldPath, newPath) == 0;
#endif
}



/////////////////////////////////////////////////////////
// DPI Awareness

#if defined(_WIN32) || defined(_WIN64)

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

typedef BOOL    (__stdcall * PFN_SetProcessDPIAware)     (void);
typedef HRESULT (__stdcall * PFN_SetProcessDpiAwareness) (PROCESS_DPI_AWARENESS);
typedef HRESULT (__stdcall * PFN_GetDpiForMonitor)       (HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT *dpiX, UINT *dpiY);

void dr_win32_make_dpi_aware()
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

void dr_win32_get_base_dpi(int* pDPIXOut, int* pDPIYOut)
{
    if (pDPIXOut != NULL) {
        *pDPIXOut = 96;
    }

    if (pDPIYOut != NULL) {
        *pDPIYOut = 96;
    }
}

void dr_win32_get_system_dpi(int* pDPIXOut, int* pDPIYOut)
{
    if (pDPIXOut != NULL) {
        *pDPIXOut = GetDeviceCaps(GetDC(NULL), LOGPIXELSX);
    }

    if (pDPIYOut != NULL) {
        *pDPIYOut = GetDeviceCaps(GetDC(NULL), LOGPIXELSY);
    }
}


typedef struct
{
    int monitorIndex;
    int i;
    int dpiX;
    int dpiY;
    PFN_GetDpiForMonitor _GetDpiForMonitor;

} win32_get_monitor_dpi_data;

static BOOL CALLBACK win32_get_monitor_dpi_callback(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    (void)hdcMonitor;
    (void)lprcMonitor;

    win32_get_monitor_dpi_data* pData = (win32_get_monitor_dpi_data*)dwData;
    if (pData->monitorIndex == pData->i)
    {
        UINT dpiX;
        UINT dpiY;
        if (pData->_GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY) == S_OK)
        {
            pData->dpiX = (int)dpiX;
            pData->dpiY = (int)dpiY;
        }
        else
        {
            dr_win32_get_system_dpi(&pData->dpiX, &pData->dpiY);
        }

        return FALSE;   // Return false to terminate the enumerator.
    }

    pData->i += 1;
    return TRUE;
}

void dr_win32_get_monitor_dpi(int monitor, int* pDPIXOut, int* pDPIYOut)
{
    // If multi-monitor DPI awareness is not supported we will need to fall back to system DPI.
    HMODULE hSHCoreDLL = LoadLibraryW(L"shcore.dll");
    if (hSHCoreDLL == NULL) {
        dr_win32_get_system_dpi(pDPIXOut, pDPIYOut);
        return;
    }

    PFN_GetDpiForMonitor _GetDpiForMonitor = (PFN_GetDpiForMonitor)GetProcAddress(hSHCoreDLL, "GetDpiForMonitor");
    if (_GetDpiForMonitor == NULL) {
        dr_win32_get_system_dpi(pDPIXOut, pDPIYOut);
        FreeLibrary(hSHCoreDLL);
        return;
    }


    win32_get_monitor_dpi_data data;
    data.monitorIndex = monitor;
    data.i = 0;
    data.dpiX = 0;
    data.dpiY = 0;
    data._GetDpiForMonitor = _GetDpiForMonitor;
    EnumDisplayMonitors(NULL, NULL, win32_get_monitor_dpi_callback, (LPARAM)&data);

    if (pDPIXOut) {
        *pDPIXOut = data.dpiX;
    }

    if (pDPIYOut) {
        *pDPIYOut = data.dpiY;
    }


    FreeLibrary(hSHCoreDLL);
}


static BOOL CALLBACK win32_get_monitor_count_callback(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    (void)hMonitor;
    (void)hdcMonitor;
    (void)lprcMonitor;

    int *count = (int*)dwData;
    (*count)++;

    return TRUE;
}

int dr_win32_get_monitor_count()
{
    int count = 0;
    if (EnumDisplayMonitors(NULL, NULL, win32_get_monitor_count_callback, (LPARAM)&count)) {
        return count;
    }

    return 0;
}
#endif



/////////////////////////////////////////////////////////
// Date / Time

time_t dr_now()
{
    return time(NULL);
}

void dr_datetime_short(time_t t, char* strOut, unsigned int strOutSize)
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

void dr_date_YYYYMMDD(time_t t, char* strOut, unsigned int strOutSize)
{
#if defined(_MSC_VER)
	struct tm local;
	localtime_s(&local, &t);
    strftime(strOut, strOutSize, "%Y%m%d", &local);
#else
	struct tm *local = localtime(&t);
	strftime(strOut, strOutSize, "%Y%m%d", local);
#endif
}



/////////////////////////////////////////////////////////
// Command Line

typedef struct
{
    dr_cmdline* pCmdLine;
    char* value;

    // Win32 style data.
    char* win32_payload;
    char* valueEnd;

    // argv style data.
    int iarg;   // <-- This starts at -1 so that the first call to next() increments it to 0.

} drutil_cmdline_iterator;

drutil_cmdline_iterator drutil_cmdline_begin(dr_cmdline* pCmdLine)
{
    drutil_cmdline_iterator i;
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
            i.win32_payload = (char*)malloc(length + 2);         // +2 for a double null terminator.
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

bool drutil_cmdline_next(drutil_cmdline_iterator* i)
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


bool dr_init_cmdline(dr_cmdline* pCmdLine, int argc, char** argv)
{
    if (pCmdLine == NULL) {
        return false;
    }

    pCmdLine->argc  = argc;
    pCmdLine->argv  = argv;
    pCmdLine->win32 = NULL;

    return true;
}

bool dr_init_cmdline_win32(dr_cmdline* pCmdLine, const char* args)
{
    if (pCmdLine == NULL) {
        return false;
    }

    pCmdLine->argc  = 0;
    pCmdLine->argv  = NULL;
    pCmdLine->win32 = args;

    return true;
}

void dr_parse_cmdline(dr_cmdline* pCmdLine, dr_cmdline_parse_proc callback, void* pUserData)
{
    if (pCmdLine == NULL || callback == NULL) {
        return;
    }


    char pTemp[2] = {0};

    char* pKey = NULL;
    char* pVal = NULL;

    drutil_cmdline_iterator arg = drutil_cmdline_begin(pCmdLine);
    if (drutil_cmdline_next(&arg))
    {
        if (!callback("[path]", arg.value, pUserData)) {
            return;
        }
    }

    while (drutil_cmdline_next(&arg))
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

typedef struct
{
    bool exists;
    const char* key;
} dr_cmdline_key_exists_data;

bool dr_cmdline_key_exists_callback(const char* key, const char* value, void* pUserData)
{
    (void)value;

    dr_cmdline_key_exists_data* pData = (dr_cmdline_key_exists_data*)pUserData;
    assert(pData != NULL);

    if (strcmp(pData->key, key) == 0) {
        pData->exists = true;
        return false;
    }

    return true;
}

bool dr_cmdline_key_exists(dr_cmdline* pCmdLine, const char* key)
{
    dr_cmdline_key_exists_data data;
    data.exists = false;
    data.key = key;
    dr_parse_cmdline(pCmdLine, dr_cmdline_key_exists_callback, &data);

    return data.exists;
}




/////////////////////////////////////////////////////////
// Threading

#if defined(_WIN32)
#include <windows.h>

void dr_sleep(unsigned int milliseconds)
{
    Sleep((DWORD)milliseconds);
}

unsigned int dr_get_logical_processor_count()
{
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);

    return (unsigned int)sysinfo.dwNumberOfProcessors;
}


typedef struct
{
    /// The Win32 thread handle.
    HANDLE hThread;

    /// The entry point.
    dr_thread_entry_proc entryProc;

    /// The user data to pass to the thread's entry point.
    void* pData;

    /// Set to true by the entry function. We use this to wait for the entry function to start.
    bool isInEntryProc;

} drutil_thread_win32;

static DWORD WINAPI drutil_thread_entry_proc_win32(LPVOID pUserData)
{
    drutil_thread_win32* pThreadWin32 = (drutil_thread_win32*)pUserData;
    assert(pThreadWin32 != NULL);

    void* pEntryProcData = pThreadWin32->pData;
    dr_thread_entry_proc entryProc = pThreadWin32->entryProc;
    assert(entryProc != NULL);

    pThreadWin32->isInEntryProc = true;

    return (DWORD)entryProc(pEntryProcData);
}

dr_thread dr_create_thread(dr_thread_entry_proc entryProc, void* pData)
{
    if (entryProc == NULL) {
        return NULL;
    }

    drutil_thread_win32* pThreadWin32 = (drutil_thread_win32*)malloc(sizeof(*pThreadWin32));
    if (pThreadWin32 != NULL)
    {
        pThreadWin32->entryProc     = entryProc;
        pThreadWin32->pData         = pData;
        pThreadWin32->isInEntryProc = false;

        pThreadWin32->hThread = CreateThread(NULL, 0, drutil_thread_entry_proc_win32, pThreadWin32, 0, NULL);
        if (pThreadWin32 == NULL) {
            free(pThreadWin32);
            return NULL;
        }

        // Wait for the new thread to enter into it's entry point before returning. We need to do this so we can safely
        // support something like dr_delete_thread(dr_create_thread(my_thread_proc, pData)).
        //
        // On Win32 there are times when this can get stuck in an infinite loop - I expect it's something to do with some
        // bad scheduling by the OS. This can be "fixed" by sleeping for a bit.
        while (!pThreadWin32->isInEntryProc) { dr_sleep(0); }
    }

    return (dr_thread)pThreadWin32;
}

void dr_delete_thread(dr_thread thread)
{
    drutil_thread_win32* pThreadWin32 = (drutil_thread_win32*)thread;
    if (pThreadWin32 != NULL)
    {
        CloseHandle(pThreadWin32->hThread);
    }

    free(pThreadWin32);
}

void dr_wait_thread(dr_thread thread)
{
    drutil_thread_win32* pThreadWin32 = (drutil_thread_win32*)thread;
    if (pThreadWin32 != NULL)
    {
        WaitForSingleObject(pThreadWin32->hThread, INFINITE);
    }
}

void dr_wait_and_delete_thread(dr_thread thread)
{
    dr_wait_thread(thread);
    dr_delete_thread(thread);
}


#ifdef DR_UTIL_WIN32_USE_CRITICAL_SECTION_MUTEX
dr_mutex dr_create_mutex()
{
    dr_mutex mutex = malloc(sizeof(CRITICAL_SECTION));
    if (mutex != NULL)
    {
        InitializeCriticalSection(mutex);
    }

    return mutex;
}

void dr_delete_mutex(dr_mutex mutex)
{
    DeleteCriticalSection(mutex);
    free(mutex);
}

void dr_lock_mutex(dr_mutex mutex)
{
    EnterCriticalSection(mutex);
}

void dr_unlock_mutex(dr_mutex mutex)
{
    LeaveCriticalSection(mutex);
}
#else
dr_mutex dr_create_mutex()
{
    return CreateEventA(NULL, FALSE, TRUE, NULL);
}

void dr_delete_mutex(dr_mutex mutex)
{
    CloseHandle((HANDLE)mutex);
}

void dr_lock_mutex(dr_mutex mutex)
{
    WaitForSingleObject((HANDLE)mutex, INFINITE);
}

void dr_unlock_mutex(dr_mutex mutex)
{
    SetEvent((HANDLE)mutex);
}
#endif


dr_semaphore dr_create_semaphore(int initialValue)
{
    return (void*)CreateSemaphoreA(NULL, initialValue, LONG_MAX, NULL);
}

void dr_delete_semaphore(dr_semaphore semaphore)
{
    CloseHandle(semaphore);
}

bool dr_wait_semaphore(dr_semaphore semaphore)
{
    return WaitForSingleObject((HANDLE)semaphore, INFINITE) == WAIT_OBJECT_0;
}

bool dr_release_semaphore(dr_semaphore semaphore)
{
    return ReleaseSemaphore((HANDLE)semaphore, 1, NULL) != 0;
}
#else
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>

void dr_sleep(unsigned int milliseconds)
{
    usleep(milliseconds * 1000);    // <-- usleep is in microseconds.
}

unsigned int dr_get_logical_processor_count()
{
    return (unsigned int)sysconf(_SC_NPROCESSORS_ONLN);
}


typedef struct
{
    /// The Win32 thread handle.
    pthread_t pthread;

    /// The entry point.
    dr_thread_entry_proc entryProc;

    /// The user data to pass to the thread's entry point.
    void* pData;

    /// Set to true by the entry function. We use this to wait for the entry function to start.
    bool isInEntryProc;

} drutil_thread_posix;

static void* drutil_thread_entry_proc_posix(void* pDataIn)
{
    drutil_thread_posix* pThreadPosix = pDataIn;
    assert(pThreadPosix != NULL);

    void* pEntryProcData = pThreadPosix->pData;
    dr_thread_entry_proc entryProc = pThreadPosix->entryProc;
    assert(entryProc != NULL);

    pThreadPosix->isInEntryProc = true;

    return (void*)(size_t)entryProc(pEntryProcData);
}

dr_thread dr_create_thread(dr_thread_entry_proc entryProc, void* pData)
{
    if (entryProc == NULL) {
        return NULL;
    }

    drutil_thread_posix* pThreadPosix = malloc(sizeof(*pThreadPosix));
    if (pThreadPosix != NULL)
    {
        pThreadPosix->entryProc     = entryProc;
        pThreadPosix->pData         = pData;
        pThreadPosix->isInEntryProc = false;

        if (pthread_create(&pThreadPosix->pthread, NULL, drutil_thread_entry_proc_posix, pThreadPosix) != 0) {
            free(pThreadPosix);
            return NULL;
        }

        // Wait for the new thread to enter into it's entry point before returning. We need to do this so we can safely
        // support something like dr_delete_thread(dr_create_thread(my_thread_proc, pData)).
        while (!pThreadPosix->isInEntryProc) {}
    }

    return (dr_thread)pThreadPosix;
}

void dr_delete_thread(dr_thread thread)
{
    free(thread);
}

void dr_wait_thread(dr_thread thread)
{
    drutil_thread_posix* pThreadPosix = (drutil_thread_posix*)thread;
    if (pThreadPosix != NULL)
    {
        pthread_join(pThreadPosix->pthread, NULL);
    }
}



dr_mutex dr_create_mutex()
{
    pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(mutex, NULL) != 0) {
        free(mutex);
        mutex = NULL;
    }

    return mutex;
}

void dr_delete_mutex(dr_mutex mutex)
{
    pthread_mutex_destroy(mutex);
}

void dr_lock_mutex(dr_mutex mutex)
{
    pthread_mutex_lock(mutex);
}

void dr_unlock_mutex(dr_mutex mutex)
{
    pthread_mutex_unlock(mutex);
}



dr_semaphore dr_create_semaphore(int initialValue)
{
    sem_t* semaphore = malloc(sizeof(sem_t));
    if (sem_init(semaphore, 0, (unsigned int)initialValue) == -1) {
        free(semaphore);
        semaphore = NULL;
    }

    return semaphore;
}

void dr_delete_semaphore(dr_semaphore semaphore)
{
    sem_close(semaphore);
}

bool dr_wait_semaphore(dr_semaphore semaphore)
{
    return sem_wait(semaphore) != -1;
}

bool dr_release_semaphore(dr_semaphore semaphore)
{
    return sem_post(semaphore) != -1;
}
#endif



/////////////////////////////////////////////////////////
// Timing

#ifdef _WIN32
static LARGE_INTEGER g_DRTimerFrequency = {0};
void dr_timer_init(dr_timer* pTimer)
{
    if (g_DRTimerFrequency.QuadPart == 0) {
        QueryPerformanceFrequency(&g_DRTimerFrequency);
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    pTimer->counter = (uint64_t)counter.QuadPart;
}

double dr_timer_tick(dr_timer* pTimer)
{
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter)) {
        return 0;
    }

    uint64_t newTimeCounter = counter.QuadPart;
    uint64_t oldTimeCounter = pTimer->counter;

    pTimer->counter = newTimeCounter;

    return (newTimeCounter - oldTimeCounter) / (double)g_DRTimerFrequency.QuadPart;
}
#else
void dr_timer_init(dr_timer* pTimer)
{
    struct timespec newTime;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &newTime);

    pTimer->counter = (newTime.tv_sec * 1000000000LL) + newTime.tv_nsec;
}

double dr_timer_tick(dr_timer* pTimer)
{
    struct timespec newTime;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &newTime);

    uint64_t newTimeCounter = (newTime.tv_sec * 1000000000LL) + newTime.tv_nsec;
    uint64_t oldTimeCounter = pTimer->counter;

    pTimer->counter = newTimeCounter;

    return (newTimeCounter - oldTimeCounter) / 1000000000.0;
}
#endif


/////////////////////////////////////////////////////////
// Random

double dr_randd()
{
    return (double)rand() / (double)RAND_MAX;
}

float dr_randf()
{
    return (float)dr_randd();
}



/////////////////////////////////////////////////////////
// Miscellaneous Stuff.

bool dr_hex_char_to_uint(char ascii, unsigned int* out)
{
    if (ascii >= '0' && ascii <= '9') {
        if (out) *out = ascii - '0';
        return true;
    }

    if (ascii >= 'A' && ascii <= 'F') {
        if (out) *out = 10 + (ascii - 'A');
        return true;
    }

    if (ascii >= 'a' && ascii <= 'f') {
        if (out) *out = 10 + (ascii - 'a');
        return true;
    }

    if (out) *out = 0;
    return false;
}

#endif  //DR_IMPLEMENTATION


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
