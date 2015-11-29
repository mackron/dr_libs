// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.
//
// NOTES:
//  - Files that are not on the machine's local file system will not be detected (such as files on a network drive).
//  - In some cases, renaming files won't be detected. Instead it may be implemented as a delete/create pair.
//
//  - Win32: Every directory that is watched becomes "in use" by the operating system. It is still possible
//    to modify the files and folders inside the watched directory, however.
//  - Win32: There is a known issue with the ReadDirectoryChangesW() watch technique (which is used internally)
//    where some events won't get processed if a large number of files change in a short period of time.

#ifndef easy_fsw
#define easy_fsw

#ifdef __cplusplus
extern "C" {
#endif

// The maximum length of a path in bytes, including the null terminator. If a path exceeds this amount, it will be truncated and thus
// won't contain a meaningful value. When this is changed the source file will need to be recompiled. Most of the time leaving this
// at 256 is fine, but it's not a problem to increase the size if you are encountering truncation issues. Note that increasing this
// value will increase memory usage. You should not need make this any higher than 4096.
#define EASYFSW_MAX_PATH    256U
//#define EASYFSW_MAX_PATH    1024U
//#define EASYFSW_MAX_PATH    4096U

// The maximum size of the event queue before it overflows.
#define EASYFSW_EVENT_QUEUE_SIZE        1024U


// The different event types.
typedef enum 
{
    easyfsw_event_type_created,
    easyfsw_event_type_deleted,
    easyfsw_event_type_renamed,
    easyfsw_event_type_updated

} easyfsw_event_type;


// Structure containing information about an event.
typedef struct
{
    // The type of the event: created, deleted, renamed or updated.
    easyfsw_event_type type;

    // The absolute path of the file. For renamed events, this is the old name.
    char absolutePath[EASYFSW_MAX_PATH];

    // The new file name. This is only used for renamed events. For other event types, this will be an empty string.
    char absolutePathNew[EASYFSW_MAX_PATH];

    // The absolute base path. For renamed events, this is the old base path.
    char absoluteBasePath[EASYFSW_MAX_PATH];

    // The absolute base path for the new file name. This is only used for renamed events. For other event types, this will be an empty string.
    char absoluteBasePathNew[EASYFSW_MAX_PATH];

} easyfsw_event;


typedef void* easyfsw_context;


// Creates a file system watcher.
//
// This will create a background thread that will do the actual checking.
easyfsw_context* easyfsw_create_context(void);

// Deletes the given file system watcher.
//
// This will not return until the thread watching for changes has returned.
//
// You do not need to remove the watched directories beforehand - this function will make sure everything is cleaned up properly.
void easyfsw_delete_context(easyfsw_context* pContext);


// Adds a directory to watch. This will watch for files and folders recursively.
int easyfsw_add_directory(easyfsw_context* pContext, const char* absolutePath);

// Removes a watched directory.
void easyfsw_remove_directory(easyfsw_context* pContext, const char* absolutePath);

// Helper for removing every watched directory.
void easyfsw_remove_all_directories(easyfsw_context* pContext);

// Determines whether or not the given directory is being watched.
int easyfsw_is_watching_directory(easyfsw_context* pContext, const char* absolutePath);


// Waits for an event from the file system.
//
// This is a blocking function. Call easyfsw_peek_event() to do a non-blocking call. If an error occurs, or the context is deleted, 0
// will be returned and the memory pointed to by pEventOut will be undefined.
//
// This can be called from any thread, however it should not be called from multiple threads simultaneously.
//
// Use caution when using this combined with easyfsw_peek_event(). In almost all cases you should use just one or the other at any
// given time.
//
// It is up to the application to ensure the context is still valid before calling this function.
//
// Example Usage:
//
// void MyFSWatcher() {
//     easyfsw_event e;
//     while (isMyContextStillAlive && easyfsw_next_event(context, e)) {
//         // Do something with the event...
//     }
// }
int easyfsw_next_event(easyfsw_context* pContext, easyfsw_event* pEventOut);

// Checks to see if there is a pending event, and if so, returns non-zero and fills the given structure with the event details. This
// removes the event from the queue.
//
// This can be called from any thread, however it should not be called from multiple threads simultaneously.
//
// It is up to the application to ensure the context is still valid before calling this function.
int easyfsw_peek_event(easyfsw_context* pContext, easyfsw_event* pEventOut);


#ifdef __cplusplus
}
#endif

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
