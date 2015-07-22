// Version 0.1 - Public Domain. Use at your own risk. No waranty implied.

// NOTES:
//
// Win32 and ReadDirectoryChangesW
//
// Here is how watching for changes via the ReadDirectoryChangesW() works:
//  1) You create a handle to the directory with CreateFile()
//  2) You pass this handle to ReadDirectoryChangesW(), including a pointer to a function that is called when changes to the directory are made.
//  3) From the aforementioned callback, ReadDirectoryChangesW() needs to be called again
//
// There are, however, a lot of details that need to be handled correctly in order for this to work
//
// First of all, the callback passed to ReadDirectoryChangesW() will not be called unless the calling thread is in an alertable state. A thread
// is put into an alertable state with WaitForMultipleObjectsEx() (the Ex version is important since it has an extra parameter that lets you
// put the thread into an alertable state). Using this blocks the thread which means you need to create a worker thread in the background.

#include "easy_fsw.h"

/////////////////////////////////////////////
// Config.

// The maximum size of the event queue before it overflows.
#define EVENT_QUEUE_SIZE        1024

// The number of FILE_NOTIFY_INFORMATION structures in the buffer that's passed to ReadDirectoryChangesW()
#define WIN32_RDC_FNI_COUNT     EVENT_QUEUE_SIZE


/////////////////////////////////////////////
// Platform detection
#if   !defined(EASYFSW_PLATFORM_WINDOWS) && (defined(__WIN32__) || defined(_WIN32) || defined(_WIN64))
#define EASYFSW_PLATFORM_WINDOWS
#elif !defined(EASFSWY_PLATFORM_LINUX)   &&  defined(__linux__)
#define EASYFSW_PLATFORM_LINUX
#elif !defined(EASYFSW_PLATFORM_MAC)     && (defined(__APPLE__) && defined(__MACH__))
#define	EASYFSW_PLATFORM_MAC
#endif


#include <stdlib.h>
#include <assert.h>
#include <string.h>

//#include <stdio.h>  // For testing. Delete this later.


#if defined(EASYFSW_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void* easyfsw_malloc(size_t sizeInBytes)
{
    return HeapAlloc(GetProcessHeap(), 0, sizeInBytes);
}

void easyfsw_free(void* p)
{
    HeapFree(GetProcessHeap(), 0, p);
}

void easyfsw_memcpy(void* dst, const void* src, size_t sizeInBytes)
{
    CopyMemory(dst, src, sizeInBytes);
}

void easyfsw_zeromemory(void* dst, size_t sizeInBytes)
{
    ZeroMemory(dst, sizeInBytes);
}
#else
void* easyfsw_malloc(size_t sizeInBytes)
{
    return malloc(sizeInBytes);
}

void easyfsw_free(void* p)
{
    free(p);
}

void easyfsw_memcpy(void* dst, const void* src, size_t sizeInBytes)
{
    memcpy(dst, src, sizeInBytes);
}

void easyfsw_zeromemory(void* dst, size_t sizeInBytes)
{
    memset(dst, 0, sizeInBytes);
}
#endif


int easyfsw_event_init(easyfsw_event* pEvent, easyfsw_event_type type, const char* absolutePath, const char* absolutePathNew)
{
    if (pEvent != NULL)
    {
        pEvent->type = type;
        
        if (absolutePath != NULL)
        {
#if defined(EASYFSW_PLATFORM_WINDOWS)
            strcpy_s(pEvent->absolutePath, EASYFSW_MAX_PATH, absolutePath);
#else
            strcpy(pEvent->absolutePath, absolutePath);
#endif
        }
        else
        {
            easyfsw_zeromemory(pEvent->absolutePath, EASYFSW_MAX_PATH);
        }

        if (absolutePathNew != NULL)
        {
#if defined(EASYFSW_PLATFORM_WINDOWS)
            strcpy_s(pEvent->absolutePathNew, EASYFSW_MAX_PATH, absolutePathNew);
#else
            strcpy(pEvent->absolutePathNew, absolutePathNew);
#endif
        }
        else
        {
            easyfsw_zeromemory(pEvent->absolutePathNew, EASYFSW_MAX_PATH);
        }

        return 1;
    }

    return 0;
}


typedef struct
{
    // The buffer containing the events in the queue.
    easyfsw_event* pBuffer;

    // The size of the buffer, in easyfsw_event's.
    unsigned int bufferSize;

    // The number of items in the queue.
    unsigned int count;

    // The index of the first item in the queue.
    unsigned int indexFirst;

#if defined(EASYFSW_PLATFORM_WINDOWS)
    // The semaphore for blocking in easyfsw_nextevent().
    HANDLE hSemaphore;

    // The mutex for synchronizing access to the buffer. This is needed because easyfsw_nextevent() will need to read the buffer while
    // another thread is filling it with events. In addition, it will help to keep easyfsw_nextevent() and easyfsw_peekevent() playing
    // nicely with each other.
    HANDLE hLock;
#endif

} easyfsw_event_queue;

int easyfsw_event_queue_init(easyfsw_event_queue* pQueue)
{
    if (pQueue != NULL)
    {
        pQueue->pBuffer    = NULL;
        pQueue->bufferSize = 0;
        pQueue->indexFirst = 0;
        pQueue->count      = 0;

#if defined(EASYFSW_PLATFORM_WINDOWS)
        pQueue->hSemaphore = CreateSemaphoreW(NULL, 0, EVENT_QUEUE_SIZE, NULL);
        if (pQueue->hSemaphore == NULL)
        {
            easyfsw_free(pQueue->pBuffer);
            return 0;
        }

        pQueue->hLock = CreateEventW(NULL, FALSE, TRUE, NULL);
        if (pQueue->hLock == NULL)
        {
            CloseHandle(pQueue->hSemaphore);
            easyfsw_free(pQueue->pBuffer);
            return 0;
        }
#endif

        return 1;
    }

    return 0;
}

void easyfsw_event_queue_uninit(easyfsw_event_queue* pQueue)
{
    if (pQueue != NULL)
    {
        easyfsw_free(pQueue->pBuffer);
        pQueue->pBuffer = NULL;

        pQueue->bufferSize = 0;
        pQueue->indexFirst = 0;
        pQueue->count      = 0;

#if defined(EASYFSW_PLATFORM_WINDOWS)
        CloseHandle(pQueue->hSemaphore);
        pQueue->hSemaphore = NULL;

        CloseHandle(pQueue->hLock);
        pQueue->hLock = NULL;
#endif
    }
}

easyfsw_event_queue* easyfsw_event_queue_create()
{
    easyfsw_event_queue* pQueue = easyfsw_malloc(sizeof(easyfsw_event_queue));
    if (pQueue != NULL)
    {
        if (!easyfsw_event_queue_init(pQueue))
        {
            easyfsw_free(pQueue);
            pQueue = NULL;
        }
    }

    return pQueue;
}

void easyfsw_event_queue_delete(easyfsw_event_queue* pQueue)
{
    if (pQueue != NULL)
    {
        easyfsw_event_queue_uninit(pQueue);
        easyfsw_free(pQueue);
    }
}

unsigned int easyfsw_event_queue_getcount(easyfsw_event_queue* pQueue)
{
    if (pQueue != NULL)
    {
        return pQueue->count;
    }

    return 0;
}

void easyfsw_event_queue_inflate(easyfsw_event_queue* pQueue)
{
    if (pQueue != NULL)
    {
        unsigned int newBufferSize = pQueue->bufferSize + 1;
        if (pQueue->bufferSize > 0)
        {
            newBufferSize = pQueue->bufferSize*2;
        }

        easyfsw_event* pOldBuffer = pQueue->pBuffer;
        easyfsw_event* pNewBuffer = easyfsw_malloc(newBufferSize * sizeof(easyfsw_event));

        for (unsigned int iDst = 0; iDst < pQueue->count; ++iDst)
        {
            unsigned int iSrc = (pQueue->indexFirst + iDst) % pQueue->bufferSize;
            easyfsw_memcpy(pNewBuffer + iDst, pOldBuffer + iSrc, sizeof(easyfsw_event));
        }


        pQueue->bufferSize = newBufferSize;
        pQueue->pBuffer    = pNewBuffer;
        pQueue->indexFirst = 0;
    }
}

int easyfsw_event_queue_pushback(easyfsw_event_queue* pQueue, easyfsw_event* pEvent)
{
    if (pQueue != NULL)
    {
        if (pEvent != NULL)
        {
            unsigned int count = easyfsw_event_queue_getcount(pQueue);
            if (count == EVENT_QUEUE_SIZE)
            {
                // We've hit the limit.
                return 0;
            }

            if (count == pQueue->bufferSize)
            {
                easyfsw_event_queue_inflate(pQueue);
                assert(count < pQueue->bufferSize);
            }


            // Insert the value.
            unsigned int iDst = (pQueue->indexFirst + 1) % pQueue->bufferSize;
            easyfsw_memcpy(pQueue->pBuffer + iDst, pEvent, sizeof(easyfsw_event));


            // Increment the counter.
            pQueue->count += 1;


            return 1;
        }
    }

    return 0;
}

int easyfsw_event_queue_pop(easyfsw_event_queue* pQueue, easyfsw_event* pEventOut)
{
    if (pQueue != NULL && pQueue->count > 0)
    {
        if (pEventOut != NULL)
        {
            easyfsw_memcpy(pEventOut, pQueue->pBuffer + pQueue->indexFirst, sizeof(easyfsw_event));
            pQueue->indexFirst = (pQueue->indexFirst + 1) % pQueue->bufferSize;
        }

        pQueue->count -= 1;


        return 1;
    }

    return 0;
}



// A simple function for appending a relative path to an absolute path. This does not resolve "." and ".." components.
int MakeAbsolutePath(const char* absolutePart, const char* relativePart, char absolutePathOut[EASYFSW_MAX_PATH])
{
    size_t absolutePartLength = strlen(absolutePart);
    size_t relativePartLength = strlen(relativePart);

    if (absolutePartLength > 0)
    {
        if (absolutePart[absolutePartLength - 1] == '/')
        {
            absolutePartLength -= 1;
        }

        if (absolutePartLength > EASYFSW_MAX_PATH)
        {
            absolutePartLength = EASYFSW_MAX_PATH - 1;
        }
    }

    if (absolutePartLength + relativePartLength + 1 > EASYFSW_MAX_PATH)
    {
        relativePartLength = EASYFSW_MAX_PATH - 1 - absolutePartLength - 1;     // -1 for the null terminate and -1 for the slash.
    }
    

    // Absolute part.
    memcpy(absolutePathOut, absolutePart, absolutePartLength);

    // Slash.
    absolutePathOut[absolutePartLength] = '/';

    // Relative part.
    memcpy(absolutePathOut + absolutePartLength + 1, relativePart, relativePartLength);

    // Null terminator.
    absolutePathOut[absolutePartLength + 1 + relativePartLength] = '\0';


    return 1;
}


typedef struct
{
    // A pointer to the buffer containing pointers to the objects.
    void** buffer;

    // The size of the buffer, in pointers.
    unsigned int bufferSize;

    // The number of pointers in the list.
    unsigned int count;

} easyfsw_list;

int easyfsw_list_init(easyfsw_list* pList)
{
    if (pList != NULL)
    {
        pList->buffer     = NULL;
        pList->bufferSize = 0;
        pList->count      = 0;

        return 1;
    }

    return 0;
}

void easyfsw_list_uninit(easyfsw_list* pList)
{
    if (pList != NULL)
    {
        easyfsw_free(pList->buffer);
    }
}

void easyfsw_list_inflate(easyfsw_list* pList)
{
    if (pList != NULL)
    {
        unsigned int newBufferSize = pList->bufferSize + 1;
        if (pList->bufferSize > 0)
        {
            newBufferSize = pList->bufferSize*2;
        }

        void** pOldBuffer = pList->buffer;
        void** pNewBuffer = easyfsw_malloc(newBufferSize*sizeof(void*));

        // Move everything over to the new buffer.
        for (unsigned int i = 0; i < pList->count; ++i)
        {
            pNewBuffer[i] = pOldBuffer[i];
        }


        pList->bufferSize = newBufferSize;
        pList->buffer     = pNewBuffer;
    }
}

void easyfsw_list_pushback(easyfsw_list* pList, void* pItem)
{
    if (pList != NULL)
    {
        if (pList->count == pList->bufferSize)
        {
            easyfsw_list_inflate(pList);
            assert(pList->count < pList->bufferSize);

            pList->buffer[pList->count] = pItem;
            pList->count += 1;
        }
    }
}

void easyfsw_list_removebyindex(easyfsw_list* pList, unsigned int index)
{
    if (pList != NULL)
    {
        assert(index < pList->count);
        assert(pList->count > 0);

        // Just move everything down one slot.
        for (int i = index; index < pList->count - 1; ++i)
        {
            pList->buffer[i] = pList->buffer[i + 1];
        }

        pList->count -= 1;
    }
}

void easyfsw_list_popback(easyfsw_list* pList)
{
    if (pList != NULL)
    {
        assert(pList->count > 0);
        
        easyfsw_list_removebyindex(pList, pList->count - 1);
    }
}




#if defined(EASYFSW_PLATFORM_WINDOWS)

#define EASYFSW_MAX_PATH_W      (EASYFSW_MAX_PATH / sizeof(wchar_t))


///////////////////////////////////////////////
// ReadDirectoryChangesW

static const int WIN32_RDC_PENDING_WATCH  = (1 << 0);
static const int WIN32_RDC_PENDING_DELETE = (1 << 1);


// Replaces the forward slashes to back slashes for use with Win32. This operates on the string in place.
void ToBackSlashesWCHAR(wchar_t* path)
{
    if (path != NULL)
    {
        while (*path++ != L'\0')
        {
            if (*path == L'/')
            {
                *path = L'\\';
            }
        }
    }
}

// Replaces the back slashes with forward slashes in the given string. This operates on the string in place.
int ToForwardSlashes(char* path)
{
    if (path != NULL)
    {
        int counter = 0;
        while (*path++ != '\0' && counter++ < EASYFSW_MAX_PATH)
        {
            if (*path == '\\')
            {
                *path = '/';
            }
        }

        return 1;
    }

    return 0;
}


// Converts a UTF-8 string to wchar_t for use with Win32. Free the returned pointer with easyfsw_free().
wchar_t* UTF8ToWCHAR(const char* str)
{
    int wcharCount = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (wcharCount > 0)
    {
        int bufferSizeInBytes = sizeof(wchar_t) * wcharCount;

        wchar_t* wstr = easyfsw_malloc(bufferSizeInBytes);
        if (wstr != NULL)
        {
            MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, bufferSizeInBytes);
            return wstr;
        }
    }

    return NULL;
}

int WCHARToUTF8(const wchar_t* wstr, int wstrCC, char pathOut[EASYFSW_MAX_PATH])
{
    int bytesWritten = WideCharToMultiByte(CP_UTF8, 0, wstr, wstrCC, pathOut, EASYFSW_MAX_PATH - 1, NULL, NULL);
    if (bytesWritten > 0)
    {
        assert(bytesWritten < EASYFSW_MAX_PATH);
        pathOut[bytesWritten] = '\0';

        return 1;
    }

    return 0;
}

// Converts a UTF-8 path to wchar_t and converts the slashes to backslashes for use with Win32. Free the returned pointer with easyfsw_free().
wchar_t* ToWin32PathWCHAR(const char* path)
{
    wchar_t* wpath = UTF8ToWCHAR(path);
    if (wpath != NULL)
    {
        ToBackSlashesWCHAR(wpath);
    }

    return wpath;
}

// Converts a wchar_t Win32 path to a char unix style path (forward slashes instead of back).
int FromWin32Path(const wchar_t* wpath, int wpathCC, char pathOut[EASYFSW_MAX_PATH])
{
    if (WCHARToUTF8(wpath, wpathCC, pathOut))
    {
        return ToForwardSlashes(pathOut);
    }

    return 0;
}


VOID CALLBACK easyfsw_win32_completionroutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);


typedef struct
{
    // Thelist containing pointers to the watched directory objects. This is not thread safe.
    easyfsw_list list;

    // The lock for synchronizing access to the list.
    HANDLE hLock;

} easyfsw_directory_list_win32;

// Structure representing the watcher context for the Win32 RDC method.
typedef struct
{
    // The list of watched directories.
    easyfsw_directory_list_win32 watchedDirectories;

    // The event queue.
    easyfsw_event_queue eventQueue;


    // A handle to the watcher thread.
    HANDLE hThread;

    // The list of events to wait on in the watcher thread.
    //   0 = The event to wait on when the context is deleted.
    //   1 = The event to wait on when a directory needs to being watching.
    //   2 = The event to wait on when a directory needs to be deleted.
    HANDLE hEvents[3];

    // The semaphore which is used when deleting a watched folder. This starts off at 0, and the maximum count is 1. When a watched
    // directory is removed, the calling thread will wait on this semaphore while the worker thread does the deletion.
    HANDLE hDeleteDirSemaphore;

    // Whether or not the watch thread needs to be terminated.
    BOOL terminateThread;

} easyfsw_context_win32;

easyfsw_context* easyfsw_create_context_win32();
void easyfsw_delete_context_win32(easyfsw_context_win32* pContext);
int easyfsw_add_directory_win32(easyfsw_context_win32* pContext, const char* absolutePath);
void easyfsw_remove_directory_win32(easyfsw_context_win32* pContext, const char* absolutePath);
void easyfsw_remove_directory_no_lock_win32(easyfsw_context_win32* pContext, const char* absolutePath);
void easyfsw_remove_all_directories_win32(easyfsw_context_win32* pContext);
int easyfsw_is_watching_directory_win32(easyfsw_context_win32* pContext, const char* absolutePath);
int easyfsw_nextevent_win32(easyfsw_context_win32* pContext, easyfsw_event* pEventOut);
int easyfsw_peekevent_win32(easyfsw_context_win32* pContext, easyfsw_event* pEventOut);

void easyfsw_postevent_win32(easyfsw_context_win32* pContext, easyfsw_event* pEvent);


// Structure representing a directory that's being watched with the Win32 RDC method.
typedef struct
{
    // A pointer to the context that owns this directory.
    easyfsw_context_win32* pContext;

    // The absolute path of the directory being watched.
    char* absolutePath;

    // The handle representing the directory. This is created with CreateFile() which means the directory itself will become locked
    // because the operating system see's it as "in use". It is possible to modify the files and folder inside the directory, though.
    HANDLE hDirectory;

    // This is required for for ReadDirectoryChangesW().
	OVERLAPPED overlapped;

    // A pointer to the buffer containing the notification objects that is passed to the notification callback specified with
    // ReadDirectoryChangesW(). This must be aligned to a DWORD boundary, but easyfsw_malloc() will do that for us, so that should not
    // be an issue.
    FILE_NOTIFY_INFORMATION* pFNIBuffer;

    // The size of the file notification information buffer, in bytes.
    DWORD fniBufferSizeInBytes;

    // Flags describing the state of the directory.
    int flags;

} easyfsw_directory_win32;

int easyfsw_directory_win32_beginwatch(easyfsw_directory_win32* pDirectory);


void easyfsw_directory_win32_uninit(easyfsw_directory_win32* pDirectory)
{
    if (pDirectory != NULL)
    {
        easyfsw_free(pDirectory->absolutePath);
        pDirectory->absolutePath = NULL;

        if (pDirectory->hDirectory != NULL)
        {
            CloseHandle(pDirectory->hDirectory);
            pDirectory->hDirectory = NULL;
        }

        easyfsw_free(pDirectory->pFNIBuffer);
        pDirectory->pFNIBuffer = NULL;
    }
}

int easyfsw_directory_win32_init(easyfsw_directory_win32* pDirectory, easyfsw_context_win32* pContext, const char* absolutePath)
{
    if (pDirectory != NULL)
    {
        pDirectory->pContext             = pContext;
        pDirectory->absolutePath         = NULL;
        pDirectory->hDirectory           = NULL;
        pDirectory->pFNIBuffer           = NULL;
        pDirectory->fniBufferSizeInBytes = 0;
        pDirectory->flags                = 0;

        size_t length = strlen(absolutePath);
        if (length > 0)
        {
            pDirectory->absolutePath = easyfsw_malloc(length + 1);
            if (pDirectory->absolutePath != NULL)
            {
                strcpy_s(pDirectory->absolutePath, length + 1, absolutePath);

                wchar_t* absolutePathWithBackSlashes = ToWin32PathWCHAR(absolutePath);
                if (absolutePathWithBackSlashes != NULL)
                {
                    pDirectory->hDirectory = CreateFileW(
                        absolutePathWithBackSlashes,
                        FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                        NULL);
                    if (pDirectory->hDirectory != INVALID_HANDLE_VALUE)
                    {
                        // From MSDN:
                        //
                        // Using a completion routine. To receive notification through a completion routine, do not associate the directory with a
                        // completion port. Specify a completion routine in lpCompletionRoutine. This routine is called whenever the operation has
                        // been completed or canceled while the thread is in an alertable wait state. The hEvent member of the OVERLAPPED structure
                        // is not used by the system, so you can use it yourself.
                        ZeroMemory(&pDirectory->overlapped, sizeof(pDirectory->overlapped));
                        pDirectory->overlapped.hEvent = pDirectory;

                        pDirectory->fniBufferSizeInBytes = WIN32_RDC_FNI_COUNT * sizeof(FILE_NOTIFY_INFORMATION);
                        pDirectory->pFNIBuffer = easyfsw_malloc(pDirectory->fniBufferSizeInBytes);
                        if (pDirectory->pFNIBuffer != NULL)
                        {
                            // At this point the directory is initialized, however it is not yet being watched. The watch needs to be triggered from
                            // the worker thread. To do this, we need to signal hPendingWatchEvent, however that needs to be done after the context
                            // has added the directory to it's internal list.
                            return 1;
                        }
                        else
                        {
                            easyfsw_directory_win32_uninit(pDirectory);
                        }
                    }
                    else
                    {
                        easyfsw_directory_win32_uninit(pDirectory);
                    }
                }
                else
                {
                    easyfsw_directory_win32_uninit(pDirectory);
                }
            }
        }
    }

    return 0;
}

int easyfsw_directory_win32_schedulewatch(easyfsw_directory_win32* pDirectory)
{
    if (pDirectory != NULL)
    {
        pDirectory->flags |= WIN32_RDC_PENDING_WATCH;
        SetEvent(pDirectory->pContext->hEvents[1]);

        return 1;
    }

    return 0;
}

int easyfsw_directory_win32_scheduledelete(easyfsw_directory_win32* pDirectory)
{
    if (pDirectory != NULL)
    {
        pDirectory->flags |= WIN32_RDC_PENDING_DELETE;
        SetEvent(pDirectory->pContext->hEvents[2]);

        return 1;
    }

    return 0;
}

int easyfsw_directory_win32_beginwatch(easyfsw_directory_win32* pDirectory)
{
    // This function should only be called from the worker thread.

    if (pDirectory != NULL)
    {
        DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;
        DWORD dwBytes        = 0;
        if (ReadDirectoryChangesW(pDirectory->hDirectory, pDirectory->pFNIBuffer, pDirectory->fniBufferSizeInBytes, TRUE, dwNotifyFilter, &dwBytes, &pDirectory->overlapped, easyfsw_win32_completionroutine))
        {
            return 1;
        }
    }

    return 0;
}




int easyfsw_directory_list_win32_init(easyfsw_directory_list_win32* pDirectories)
{
    if (pDirectories != NULL)
    {
        easyfsw_list_init(&pDirectories->list);

        pDirectories->hLock = CreateEvent(NULL, FALSE, TRUE, NULL);
        if (pDirectories->hLock != NULL)
        {
            return 1;
        }
    }

    return 0;
}

void easyfsw_directory_list_win32_uninit(easyfsw_directory_list_win32* pDirectories)
{
    if (pDirectories != NULL)
    {
        easyfsw_list_uninit(&pDirectories->list);

        CloseHandle(pDirectories->hLock);
        pDirectories->hLock = NULL;
    }
}





VOID CALLBACK easyfsw_win32_completionroutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
    easyfsw_directory_win32* pDirectory = (easyfsw_directory_win32*)lpOverlapped->hEvent;
    if (pDirectory != NULL)
    {
        if (dwErrorCode == ERROR_OPERATION_ABORTED)
        {
            // When we get here, CancelIo() was called on the directory. We treat this as a signal that the context has requested that the directory
            // be deleted. At this point the directory has been removed from the context's internal list and we just need to uninitialize and free
            // the directory object.
            easyfsw_context_win32* pContext = pDirectory->pContext;
            easyfsw_directory_win32_uninit(pDirectory);
            easyfsw_free(pDirectory);

            ReleaseSemaphore(pContext->hDeleteDirSemaphore, 1, NULL);
            return;
        }

        assert(dwNumberOfBytesTransfered >= sizeof(FILE_NOTIFY_INFORMATION));

        // At this point we're not actually watching the directory - there is a chance that as we're executing this section there
        // are changes to the file system whose events will go undetected. We need to call ReadDirectoryChangesW() again as soon as
        // possible. To do this we just create and fill a local event cache, then schedule the re-watch, and then go ahead and post
        // events through the main context.
        //
        // TODO: Look at optimizing this. I think we can just have two FNI pointers and swap between them. I need to test, but I think
        //       this function is always called on the same thread ReadDirectoryChangesW() was called on. If so, we can just do a
        //       simple pointer swap system which'll be much more efficient.
        easyfsw_event_queue* localQueue = easyfsw_event_queue_create();
        if (localQueue != NULL)
        {
            char absolutePathOld[EASYFSW_MAX_PATH];

            char* pFNI8 = (char*)pDirectory->pFNIBuffer;
            for (;;)
            {
                FILE_NOTIFY_INFORMATION* pFNI = (FILE_NOTIFY_INFORMATION*)pFNI8;

                char relativePath[EASYFSW_MAX_PATH];
                if (FromWin32Path(pFNI->FileName, pFNI->FileNameLength / sizeof(wchar_t), relativePath))
                {
                    char absolutePath[EASYFSW_MAX_PATH];
                    if (MakeAbsolutePath(pDirectory->absolutePath, relativePath, absolutePath))
                    {
                        switch (pFNI->Action)
                        {
                        case FILE_ACTION_ADDED:
                            {
                                easyfsw_event e;
                                if (easyfsw_event_init(&e, easyfsw_event_type_created, absolutePath, NULL))
                                {
                                    easyfsw_event_queue_pushback(localQueue, &e);
                                }

                                break;
                            }

                        case FILE_ACTION_REMOVED:
                            {
                                easyfsw_event e;
                                if (easyfsw_event_init(&e, easyfsw_event_type_deleted, absolutePath, NULL))
                                {
                                    easyfsw_event_queue_pushback(localQueue, &e);
                                }

                                break;
                            }

                        case FILE_ACTION_RENAMED_OLD_NAME:
                            {
                                strcpy_s(absolutePathOld, EASYFSW_MAX_PATH, absolutePath);

                                break;
                            }

                        case FILE_ACTION_RENAMED_NEW_NAME:
                            {
                                easyfsw_event e;
                                if (easyfsw_event_init(&e, easyfsw_event_type_renamed, absolutePathOld, absolutePath))
                                {
                                    easyfsw_event_queue_pushback(localQueue, &e);
                                }

                                break;
                            }

                        case FILE_ACTION_MODIFIED:
                            {
                                easyfsw_event e;
                                if (easyfsw_event_init(&e, easyfsw_event_type_updated, absolutePath, NULL))
                                {
                                    easyfsw_event_queue_pushback(localQueue, &e);
                                }

                                break;
                            }

                        default: break;
                        }
                    }
                    //easyfsw_free(absolutePath);
                }
                //easyfsw_free(relativePath);

            

                if (pFNI->NextEntryOffset == 0)
                {
                    break;
                }

                pFNI8 += pFNI->NextEntryOffset;
            }
        }


        // Begin watching again (call ReadDirectoryChangesW() again). To do this we need to send a signal to the worker thread which
        // will do the actual call to ReadDirectoryChangesW().
        easyfsw_directory_win32_schedulewatch(pDirectory);


        // We now need to loop through all of our events and post them to the context.
        for (unsigned int i = 0; i < localQueue->count; ++i)
        {
            unsigned int iSrc = (localQueue->indexFirst + i) % localQueue->bufferSize;
            easyfsw_postevent_win32(pDirectory->pContext, localQueue->pBuffer + iSrc);
        }


        // We're done so we can now delete the local event queue we created earlier.
        easyfsw_event_queue_delete(localQueue);
    }
}





DWORD WINAPI _WatcherThreadProc_RDC(easyfsw_context_win32 *pContextRDC)
{
    while (!pContextRDC->terminateThread)
    {
        // Important that we use the Ex version here because we need to put the thread into an alertable state (last argument). If the thread is not put into
        // an alertable state, ReadDirectoryChangesW() won't ever call the notification event.
        DWORD rc = WaitForMultipleObjectsEx(sizeof(pContextRDC->hEvents) / sizeof(pContextRDC->hEvents[0]), pContextRDC->hEvents, FALSE, INFINITE, TRUE);
        switch (rc)
        {
        case WAIT_OBJECT_0 + 0:
            {
                // The context has signaled that it needs to be deleted.
                pContextRDC->terminateThread = TRUE;
                break;
            }

        case WAIT_OBJECT_0 + 1:
            {
                // A folder is wanting to be watched. We just cycle over each directory and check those that are pending watching.
                WaitForSingleObject(pContextRDC->watchedDirectories.hLock, INFINITE);
                {
                    for (unsigned int iDirectory = 0; iDirectory < pContextRDC->watchedDirectories.list.count; ++iDirectory)
                    {
                        easyfsw_directory_win32* pDirectory = pContextRDC->watchedDirectories.list.buffer[iDirectory];
                        if (pDirectory != NULL)
                        {
                            if ((pDirectory->flags & WIN32_RDC_PENDING_WATCH) != 0)
                            {
                                easyfsw_directory_win32_beginwatch(pDirectory);
                            }
                        }
                    }
                }
                SetEvent(pContextRDC->watchedDirectories.hLock);

                break;
            }

        case WAIT_OBJECT_0 + 2:
            {
                // A folder is wanting to be deleted. We just cycle over each directory and chose those that are pending deletion.
                // NOTE: We intentionally do not lock here because that synchronization is done at a higher level. A lock here 
                //       would cause a deadlock.
                for (unsigned int iDirectory = 0; iDirectory < pContextRDC->watchedDirectories.list.count; ++iDirectory)
                {
                    easyfsw_directory_win32* pDirectory = pContextRDC->watchedDirectories.list.buffer[iDirectory];
                    if (pDirectory != NULL)
                    {
                        if ((pDirectory->flags & WIN32_RDC_PENDING_DELETE) != 0)
                        {
                            // We don't free the directory structure from here. Instead we just call CancelIo(). This will trigger
                            // the ERROR_OPERATION_ABORTED error in the notification callback which is where the real delete will
                            // occur. That is also where the synchronization lock is released that the thread that called
                            // easyfsw_delete_directory() is waiting on.
                            CancelIo(pDirectory->hDirectory);

                            // The directory needs to be removed from the context's list. The directory object will be freed in the
                            // notification callback in response to ERROR_OPERATION_ABORTED which will be triggered by the previous
                            // call to CancelIo().
                            easyfsw_list_removebyindex(&pDirectory->pContext->watchedDirectories.list, iDirectory);

                            break;
                        }
                    }
                }

                break;
            }

        case WAIT_IO_COMPLETION:
        default:
            {
                // Nothing to do.
                break;
            }
        }
    }

    return 0;
}

easyfsw_context* easyfsw_create_context_win32()
{
    easyfsw_context_win32* pContext = easyfsw_malloc(sizeof(easyfsw_context_win32));
    if (pContext != NULL)
    {
        if (easyfsw_directory_list_win32_init(&pContext->watchedDirectories))
        {
            if (easyfsw_event_queue_init(&pContext->eventQueue))
            {
                pContext->hEvents[0] = CreateEvent(NULL, FALSE, FALSE, NULL);
                pContext->hEvents[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
                pContext->hEvents[2] = CreateEvent(NULL, FALSE, FALSE, NULL);
                pContext->hDeleteDirSemaphore = CreateSemaphoreW(NULL, 0, 1, NULL);
                pContext->terminateThread = FALSE;

                if (pContext->hEvents[0] != NULL && pContext->hEvents[1])
                {
                    pContext->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_WatcherThreadProc_RDC, pContext, 0, NULL);
                    if (pContext->hThread != NULL)
                    {
                        // Everything went well.
                    }
                    else
                    {
                        CloseHandle(pContext->hEvents[0]);

                        easyfsw_free(pContext);
                        pContext = NULL;
                    }
                }
                else
                {
                    easyfsw_free(pContext);
                    pContext = NULL;
                }
            }
        }
    }

    return (easyfsw_context*)pContext;
}

void easyfsw_delete_context_win32(easyfsw_context_win32* pContext)
{
    if (pContext != NULL)
    {
        // Every watched directory needs to be removed.
        easyfsw_remove_all_directories_win32(pContext);


        // Signal the close event, and wait for the thread to finish.
        SignalObjectAndWait(pContext->hEvents[0], pContext->hThread, INFINITE, FALSE);

        // The thread has finished, so close the handle.
        CloseHandle(pContext->hThread);
        pContext->hThread;


        // We need to wait for the event queue to finish up before deleting the context for real. If we don't do this nextevent() may try
        // to access the context and then crash.
        WaitForSingleObject(pContext->eventQueue.hLock, INFINITE);
        {
            easyfsw_event_queue_uninit(&pContext->eventQueue);
        }
        SetEvent(pContext->eventQueue.hLock);


        // The worker thread events need to be closed.
        for (int i = 0; i < sizeof(pContext->hEvents) / sizeof(pContext->hEvents[0]); ++i)
        {
            CloseHandle(pContext->hEvents[i]);
            pContext->hEvents[i] = NULL;
        }

        // The semaphore we use for deleting directories.
        CloseHandle(pContext->hDeleteDirSemaphore);
        pContext->hDeleteDirSemaphore = NULL;


        // Free the memory.
        easyfsw_free(pContext);
    }
}

easyfsw_directory_win32* easyfsw_find_directory_win32(easyfsw_context_win32* pContext, const char* absolutePath, int* pIndexOut)
{
    assert(pContext != NULL);

    for (unsigned int iDirectory = 0; iDirectory < pContext->watchedDirectories.list.count; ++iDirectory)
    {
        easyfsw_directory_win32* pDirectory = pContext->watchedDirectories.list.buffer[iDirectory];
        if (pDirectory != NULL)
        {
            if (strcmp(absolutePath, pDirectory->absolutePath) == 0)
            {
                if (pIndexOut != NULL)
                {
                    *pIndexOut = iDirectory;
                }

                return pDirectory;
            }
        }
    }

    return NULL;
}

int easyfsw_add_directory_win32(easyfsw_context_win32* pContext, const char* absolutePath)
{
    if (pContext != NULL)
    {
        easyfsw_directory_win32* pDirectory = easyfsw_malloc(sizeof(easyfsw_directory_win32));
        if (pDirectory != NULL)
        {
            if (!easyfsw_is_watching_directory_win32(pContext, absolutePath))
            {
                if (easyfsw_directory_win32_init(pDirectory, pContext, absolutePath))
                {
                    // At this point the directory has been initialized, but it's not yet being watched. To do this, we need to call ReadDirectoryChangesW() from
                    // the worker thread which means we need to signal an event which the worker thread will be waiting on. When the worker thread receives the
                    // signal, it will iterate over each directory in the context and check for the ones that are pending watching. Therefore, before signaling
                    // the event, we need to make sure the directory is added to the context's list.
                    WaitForSingleObject(pContext->watchedDirectories.hLock, INFINITE);
                    {
                        easyfsw_list_pushback(&pContext->watchedDirectories.list, pDirectory);
                    }
                    SetEvent(pContext->watchedDirectories.hLock);

                    // The directory is now in the list and we can send the signal.
                    easyfsw_directory_win32_schedulewatch(pDirectory);


                    return 1;
                }
                else
                {
                    easyfsw_free(pDirectory);
                    pDirectory = NULL;
                }
            }
        }
    }

    

    // An error occured if we got here.
    return 0;
}

void easyfsw_remove_directory_win32_no_lock(easyfsw_context_win32* pContext, const char* absolutePath)
{
    assert(pContext != NULL);

    int index;
    easyfsw_directory_win32* pDirectory = easyfsw_find_directory_win32(pContext, absolutePath, &index);
    if (pDirectory != NULL)
    {
        // When removing a directory, we need to call CancelIo() on the file handle we created for the directory. The problem is that
        // this needs to be called on the worker thread in order for watcher notification callback function to get the correct error
        // code. To do this we need to signal an event which the worker thread is waiting on. The worker thread will then call CancelIo()
        // which in turn will trigger the correct error code in the notification callback. The notification callback is where the 
        // the object will be deleted for real and will release the synchronization lock that this function is waiting on below.
        easyfsw_directory_win32_scheduledelete(pDirectory);

        // Now we need to wait for the relevant event to become signaled. This will become signaled when the worker thread has finished
        // deleting the file handle and whatnot from it's end.
        WaitForSingleObject(pContext->hDeleteDirSemaphore, INFINITE);
    }
}

void easyfsw_remove_directory_win32(easyfsw_context_win32* pContext, const char* absolutePath)
{
    if (pContext != NULL)
    {
        WaitForSingleObject(pContext->watchedDirectories.hLock, INFINITE);
        {
            easyfsw_remove_directory_win32_no_lock(pContext, absolutePath);
        }
        SetEvent(pContext->watchedDirectories.hLock);
    }
}

void easyfsw_remove_all_directories_win32(easyfsw_context_win32* pContext)
{
    if (pContext != NULL)
    {
        WaitForSingleObject(pContext->watchedDirectories.hLock, INFINITE);
        {
            for (unsigned int i = pContext->watchedDirectories.list.count; i > 0; --i)
            {
                easyfsw_remove_directory_win32_no_lock(pContext, ((easyfsw_directory_win32*)pContext->watchedDirectories.list.buffer[i - 1])->absolutePath);
            }
        }
        SetEvent(pContext->watchedDirectories.hLock);
    }
}

int easyfsw_is_watching_directory_win32(easyfsw_context_win32* pContext, const char* absolutePath)
{
    if (pContext != NULL)
    {
        return easyfsw_find_directory_win32(pContext, absolutePath, NULL) != NULL;
    }

    return 0;
}


int easyfsw_nextevent_win32(easyfsw_context_win32* pContext, easyfsw_event* pEvent)
{
    int result = 0;
    if (pContext != NULL && !pContext->terminateThread)
    {
        // Wait for either the semaphore or the thread to terminate.
        HANDLE hEvents[2];
        hEvents[0] = pContext->hThread;
        hEvents[1] = pContext->eventQueue.hSemaphore;

        DWORD rc = WaitForMultipleObjects(sizeof(hEvents) / sizeof(hEvents[0]), hEvents, FALSE, INFINITE);
        switch (rc)
        {
        case WAIT_OBJECT_0 + 0:
            {
                // The thread has been terminated.
                result = 0;
                break;
            }

        case WAIT_OBJECT_0 + 1:
            {
                // We're past the semaphore block, so now we can copy of the event. We need to lock the queue before doing this in case another thread wants
                // to try pushing another event onto the queue.
                if (!pContext->terminateThread)
                {
                    DWORD eventLockResult = WaitForSingleObject(pContext->eventQueue.hLock, INFINITE);
                    if (eventLockResult == WAIT_OBJECT_0)
                    {
                        easyfsw_event_queue_pop(&pContext->eventQueue, pEvent);
                    }
                    SetEvent(pContext->eventQueue.hLock);

                    if (eventLockResult == WAIT_OBJECT_0)
                    {
                        result = 1;
                    }
                    else
                    {
                        // The lock returned early for some reason which means there must have been some kind of error or the context has been destroyed.
                        result = 0;
                    }
                }

                break;
            }

        default: break;
        }
    }

    return result;
}

int easyfsw_peekevent_win32(easyfsw_context_win32* pContext, easyfsw_event* pEvent)
{
    int result = 0;
    if (pContext != NULL)
    {
        DWORD eventLockResult = WaitForSingleObject(pContext->eventQueue.hLock, INFINITE);
        {
            // Make sure we decrement our semaphore counter. Don't block here.
            WaitForSingleObject(pContext->eventQueue.hSemaphore, 0);

            // Now we just copy over the data by popping it from the queue.
            if (eventLockResult == WAIT_OBJECT_0)
            {
                if (easyfsw_event_queue_getcount(&pContext->eventQueue) > 0)
                {
                    easyfsw_event_queue_pop(&pContext->eventQueue, pEvent);
                    result = 1;
                }
                else
                {
                    result = 0;
                }
            }
            else
            {
                // Waiting on the event queue lock failed for some reason. It could mean that the context has been deleted.
                result = 0;
            }
        }
        SetEvent(pContext->eventQueue.hLock);
    }

    return result;
}

void easyfsw_postevent_win32(easyfsw_context_win32* pContext, easyfsw_event* pEvent)
{
    if (pContext != NULL && pEvent != NULL)
    {
        // Add the event to the queue.
        WaitForSingleObject(pContext->eventQueue.hLock, INFINITE);
        {
            easyfsw_event_queue_pushback(&pContext->eventQueue, pEvent);
        }
        SetEvent(pContext->eventQueue.hLock);


        // Release the semaphore so that easyfsw_nextevent() can handle it.
        ReleaseSemaphore(pContext->eventQueue.hSemaphore, 1, NULL);
    }
}



////////////////////////////////////
// Public API for Win32

easyfsw_context* easyfsw_create_context()
{
    return easyfsw_create_context_win32();
}

void easyfsw_delete_context(easyfsw_context* pContext)
{
    easyfsw_delete_context_win32((easyfsw_context_win32*)pContext);
}


int easyfsw_add_directory(easyfsw_context* pContext, const char* absolutePath)
{
    return easyfsw_add_directory_win32((easyfsw_context_win32*)pContext, absolutePath);
}

void easyfsw_remove_directory(easyfsw_context* pContext, const char* absolutePath)
{
    easyfsw_remove_directory_win32((easyfsw_context_win32*)pContext, absolutePath);
}

void easyfsw_remove_all_directories(easyfsw_context* pContext)
{
    easyfsw_remove_all_directories_win32((easyfsw_context_win32*)pContext);
}

int easyfsw_is_watching_directory(easyfsw_context* pContext, const char* absolutePath)
{
    return easyfsw_is_watching_directory_win32((easyfsw_context_win32*)pContext, absolutePath);
}


int easyfsw_nextevent(easyfsw_context* pContext, easyfsw_event* pEventOut)
{
    return easyfsw_nextevent_win32((easyfsw_context_win32*)pContext, pEventOut);
}

int easyfsw_peekevent(easyfsw_context* pContext, easyfsw_event* pEventOut)
{
    return easyfsw_peekevent_win32((easyfsw_context_win32*)pContext, pEventOut);
}


#endif  // Win32


