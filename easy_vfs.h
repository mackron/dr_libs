// Public Domain. See "unlicense" statement at the end of this file.

// NOTE: This is very early in development and should be considered unstable. Expect many APIs to change.

// ABOUT
//
// easy_vfs is a simple library which allows one to open files from both the native file system and
// archive/package files such as Zip files.
//
//
//
// USAGE
//
// This is a single-file library. To use it, do something like the following in one .c file.
//   #define EASY_VFS_IMPLEMENTATION
//   #include "easy_vfs.h"
//
// You can then #include easy_vfs.h in other parts of the program as you would with any other header file.
//
//
//
// OPTIONS
//
// To use these options, just place the appropriate #define's in the .c file before including this file.
//
// #define EASY_VFS_FORCE_STDIO
//   Force the use the C standard library's IO functions for file handling on all platforms. Note that this currently
//   only affects the Windows platform. All other platforms use stdio regardless of whether or not this is set.
//
// #define EASY_VFS_NO_ZIP
//   Disable built-in support for Zip files.
//
// #define EASY_VFS_NO_PAK
//   Disable support for Quake 2 PAK files.
//
// #define EASY_VFS_NO_MTL
//   Disable support for Wavefront MTL files.
//
//
//
// QUICK NOTES
//
// - easy_vfs is not currently thread-safe. This will be addressed soon. This should only be an issue when adding
//   and removing base directories and back-ends while trying to open/close files.
// - The library works by using the notion of an "archive" to create an abstraction around the file system.
// - Conceptually, and archive is just a grouping of files and folders. An archive can be a directory on
//   the native file system or an actual archive file such as a .zip file.
// - When iterating over files and folder, the order is undefined. Do not assume alphabetical.
// - When specifying an absolute path, it is assumed to be verbos. When specifying a relative path, it does not need
//   to be verbose.
// - Archive backends are selected based on their extension.
// - Archive backends cannot currently share the same extension. For example, many package file formats use the .pak
//   extension, however only one backend can use that .pak extension.
// - For safety, if you want to overwrite a file you must explicitly call easyvfs_open() with the EASYVFS_TRUNCATE flag.
//
//
//
// TODO:
//
// - Make thread-safe.
// - Proper error codes.
// - Change easyvfs_rename_file() to easyvfs_move_file() to enable better flexibility.
// - Document performance issues.
// - Consider making it so persistent constant strings (such as base paths) use dynamically allocated strings rather
//   than fixed size arrays of EASYVFS_MAX_PATH.

#ifndef easy_vfs_h
#define easy_vfs_h

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


// The maximum length of a path in bytes, including the null terminator. If a path exceeds this amount, it will be set to an empty
// string. When this is changed the source file will need to be recompiled. Most of the time leaving this at 256 is fine, but it's
// not a problem to increase the size if you are encountering issues. Note that increasing this value will increase memory usage
// on both the heap and the stack.
#ifndef EASYVFS_MAX_PATH
//#define EASYVFS_MAX_PATH    256
#define EASYVFS_MAX_PATH    1024
//#define EASYVFS_MAX_PATH    4096
#endif

#define EASYVFS_READ        (1 << 0)
#define EASYVFS_WRITE       (1 << 1)
#define EASYVFS_EXISTING    (1 << 2)
#define EASYVFS_TRUNCATE    (1 << 3)
#define EASYVFS_CREATE_DIRS (1 << 4)    // Creates the directory structure if required.

#define EASYVFS_FILE_ATTRIBUTE_DIRECTORY    0x00000001
#define EASYVFS_FILE_ATTRIBUTE_READONLY     0x00000002


// The allowable seeking origins.
typedef enum
{
    easyvfs_origin_current,
    easyvfs_origin_start,
    easyvfs_origin_end
}easyvfs_seek_origin;


typedef long long          easyvfs_int64;
typedef unsigned long long easyvfs_uint64;

typedef void* easyvfs_handle;

typedef struct easyvfs_context   easyvfs_context;
typedef struct easyvfs_archive   easyvfs_archive;
typedef struct easyvfs_file      easyvfs_file;
typedef struct easyvfs_file_info easyvfs_file_info;
typedef struct easyvfs_iterator  easyvfs_iterator;

typedef bool           (* easyvfs_is_valid_extension_proc)(const char* extension);
typedef easyvfs_handle (* easyvfs_open_archive_proc)      (easyvfs_file* pArchiveFile, unsigned int accessMode);
typedef void           (* easyvfs_close_archive_proc)     (easyvfs_handle archive);
typedef bool           (* easyvfs_get_file_info_proc)     (easyvfs_handle archive, const char* relativePath, easyvfs_file_info* fi);
typedef easyvfs_handle (* easyvfs_begin_iteration_proc)   (easyvfs_handle archive, const char* relativePath);
typedef void           (* easyvfs_end_iteration_proc)     (easyvfs_handle archive, easyvfs_handle iterator);
typedef bool           (* easyvfs_next_iteration_proc)    (easyvfs_handle archive, easyvfs_handle iterator, easyvfs_file_info* fi);
typedef bool           (* easyvfs_delete_file_proc)       (easyvfs_handle archive, const char* relativePath);
typedef bool           (* easyvfs_rename_file_proc)       (easyvfs_handle archive, const char* relativePathOld, const char* relativePathNew);
typedef bool           (* easyvfs_create_directory_proc)  (easyvfs_handle archive, const char* relativePath);
typedef bool           (* easyvfs_copy_file_proc)         (easyvfs_handle archive, const char* relativePathSrc, const char* relativePathDst, bool failIfExists);
typedef easyvfs_handle (* easyvfs_open_file_proc)         (easyvfs_handle archive, const char* relativePath, unsigned int accessMode);
typedef void           (* easyvfs_close_file_proc)        (easyvfs_handle archive, easyvfs_handle file);
typedef bool           (* easyvfs_read_file_proc)         (easyvfs_handle archive, easyvfs_handle file, void* pDataOut, size_t bytesToRead, size_t* pBytesReadOut);
typedef bool           (* easyvfs_write_file_proc)        (easyvfs_handle archive, easyvfs_handle file, const void* pData, size_t bytesToWrite, size_t* pBytesWrittenOut);
typedef bool           (* easyvfs_seek_file_proc)         (easyvfs_handle archive, easyvfs_handle file, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin);
typedef easyvfs_uint64 (* easyvfs_tell_file_proc)         (easyvfs_handle archive, easyvfs_handle file);
typedef easyvfs_uint64 (* easyvfs_file_size_proc)         (easyvfs_handle archive, easyvfs_handle file);
typedef void           (* easyvfs_flush_file_proc)        (easyvfs_handle archive, easyvfs_handle file);

typedef struct
{
    easyvfs_is_valid_extension_proc is_valid_extension;
    easyvfs_open_archive_proc       open_archive;
    easyvfs_close_archive_proc      close_archive;
    easyvfs_get_file_info_proc      get_file_info;
    easyvfs_begin_iteration_proc    begin_iteration;
    easyvfs_end_iteration_proc      end_iteration;
    easyvfs_next_iteration_proc     next_iteration;
    easyvfs_delete_file_proc        delete_file;
    easyvfs_rename_file_proc        rename_file;
    easyvfs_create_directory_proc   create_directory;
    easyvfs_copy_file_proc          copy_file;
    easyvfs_open_file_proc          open_file;
    easyvfs_close_file_proc         close_file;
    easyvfs_read_file_proc          read_file;
    easyvfs_write_file_proc         write_file;
    easyvfs_seek_file_proc          seek_file;
    easyvfs_tell_file_proc          tell_file;
    easyvfs_file_size_proc          file_size;
    easyvfs_flush_file_proc         flush_file;

} easyvfs_archive_callbacks;


struct easyvfs_file_info
{
    // The absolute path of the file.
    char absolutePath[EASYVFS_MAX_PATH];

    // The size of the file, in bytes.
    easyvfs_uint64 sizeInBytes;

    // The time the file was last modified.
    easyvfs_uint64 lastModifiedTime;

    // File attributes. 
    unsigned int attributes;

    // Padding. Unused.
    unsigned int padding4;
};

struct easyvfs_iterator
{
    // A pointer to the archive that contains the folder being iterated.
    easyvfs_archive* pArchive;

    // A pointer to the iterator's internal handle that was returned with begin_iteration().
    easyvfs_handle internalIteratorHandle;

    // The file info.
    easyvfs_file_info info;
};



// Creates an empty context.
easyvfs_context* easyvfs_create_context();

// Deletes the given context.
//
// This does not close any files or archives - it is up to the application to ensure those are tidied up.
void easyvfs_delete_context(easyvfs_context* pContext);


// Registers an archive back-end.
void easyvfs_register_archive_backend(easyvfs_context* pContext, easyvfs_archive_callbacks callbacks);


// Inserts a base directory at a specific priority position.
//
// A lower value index means a higher priority. This must be in the range of [0, easyvfs_get_base_directory_count()].
void easyvfs_insert_base_directory(easyvfs_context* pContext, const char* absolutePath, unsigned int index);

// Adds a base directory to the end of the list.
//
// The further down the list the base directory, the lower priority is will receive. This adds it to the end which
// means it it given a lower priority to those that are already in the list. Use easyvfs_insert_base_directory() to
// insert the base directory at a specific position.
//
// Base directories must be an absolute path to a real directory.
void easyvfs_add_base_directory(easyvfs_context* pContext, const char* absolutePath);

// Removes the given base directory.
void easyvfs_remove_base_directory(easyvfs_context* pContext, const char* absolutePath);

// Removes the directory at the given index.
//
// If you need to remove every base directory, use easyvfs_remove_all_base_directories() since that is more efficient.
void easyvfs_remove_base_directory_by_index(easyvfs_context* pContext, unsigned int index);

// Removes every base directory from the given context.
void easyvfs_remove_all_base_directories(easyvfs_context* pContext);

// Retrieves the number of base directories attached to the given context.
unsigned int easyvfs_get_base_directory_count(easyvfs_context* pContext);

// Retrieves the base directory at the given index.
const char* easyvfs_get_base_directory_by_index(easyvfs_context* pContext, unsigned int index);


// Sets the base directory for write operations (including delete).
//
// When doing a write operation using a relative path, the full path will be resolved using this directory as the base.
//
// If the base write directory is not set, and absolute path must be used for all write operations.
//
// If the write directory guard is enabled, all write operations that are attempted at a higher level than this directory
// will fail.
void easyvfs_set_base_write_directory(easyvfs_context* pContext, const char* absolutePath);

// Retrieves the base write directory.
bool easyvfs_get_base_write_directory(easyvfs_context* pContext, char* absolutePathOut, unsigned int absolutePathOutSize);

// Enables the write directory guard.
void easyvfs_enable_write_directory_guard(easyvfs_context* pContext);

// Disables the write directory guard.
void easyvfs_disable_write_directory_guard(easyvfs_context* pContext);

// Determines whether or not the base directory guard is enabled.
bool easyvfs_is_write_directory_guard_enabled(easyvfs_context* pContext);


// Opens an archive at the given path.
//
// If the given path points to a directory on the native file system an archive will be created at that
// directory. If the path points to an archive file such as a .zip file, easy_vfs will hold a handle to
// that file until the archive is closed with easyvfs_close_archive(). Keep this in mind if you are keeping
// many archives open at a time on platforms that limit the number of files one can have open at any given
// time.
//
// The given path must be either absolute, or relative to one of the base directories.
//
// The path can be nested, such as "C:/my_zip_file.zip/my_inner_zip_file.zip".
easyvfs_archive* easyvfs_open_archive(easyvfs_context* pContext, const char* absoluteOrRelativePath, unsigned int accessMode);

// Opens the archive that owns the given file.
//
// This is different to easyvfs_open_archive() in that it can accept non-archive files. It will open the
// archive that directly owns the file. In addition, it will output the path of the file, relative to the
// archive.
//
// If the given file is an archive itself, the archive that owns that archive will be opened. If the file
// is a file on the native file system, the returned archive will represent the folder it's directly
// contained in.
easyvfs_archive* easyvfs_open_owner_archive(easyvfs_context* pContext, const char* absoluteOrRelativePath, unsigned int accessMode, char* relativePathOut, size_t relativePathOutSize);

// Closes the given archive.
void easyvfs_close_archive(easyvfs_archive* pArchive);

// Opens a file relative to the given archive.
easyvfs_file* easyvfs_open_file_from_archive(easyvfs_archive* pArchive, const char* relativePath, unsigned int accessMode, size_t extraDataSize);



// Opens a file.
//
// When opening the file in write mode, the write pointer will always be sitting at the start of the file.
easyvfs_file* easyvfs_open(easyvfs_context* pContext, const char* absoluteOrRelativePath, unsigned int accessMode, size_t extraDataSize);

// Closes the given file.
void easyvfs_close(easyvfs_file* pFile);

// Reads data from the given file.
//
// Returns true if successful; false otherwise. If the value output to <pBytesReadOut> is less than <bytesToRead> it means the file is at the end. In this case,
// true is still returned.
bool easyvfs_read(easyvfs_file* pFile, void* pDataOut, size_t bytesToRead, size_t* pBytesReadOut);

// Writes data to the given file.
bool easyvfs_write(easyvfs_file* pFile, const void* pData, size_t bytesToWrite, size_t* pBytesWrittenOut);

// Seeks the file pointer by the given number of bytes, relative to the specified origin.
bool easyvfs_seek(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin);

// Retrieves the current position of the file pointer.
easyvfs_uint64 easyvfs_tell(easyvfs_file* pFile);

// Retrieves the size of the given file.
easyvfs_uint64 easyvfs_size(easyvfs_file* pFile);

// Flushes the given file.
void easyvfs_flush(easyvfs_file* pFile);

// Retrieves the size of the extra data for the given file.
size_t easyvfs_get_extra_data_size(easyvfs_file* pFile);

// Retrieves a pointer to the extra data for the given file.
void* easyvfs_get_extra_data(easyvfs_file* pFile);


// Retrieves information about the file at the given path.
//
// <fi> is allowed to be null, in which case the call is equivalent to simply checking if the file exists.
bool easyvfs_get_file_info(easyvfs_context* pContext, const char* absoluteOrRelativePath, easyvfs_file_info* fi);


// Creates an iterator for iterating over the files and folders in the given directory.
bool easyvfs_begin(easyvfs_context* pContext, const char* path, easyvfs_iterator* pIteratorOut);

// Goes to the next file or folder based on the given iterator.
bool easyvfs_next(easyvfs_context* pContext, easyvfs_iterator* pIterator);

// Closes the given iterator.
//
// This is not needed if easyvfs_next_iteration() returns false naturally. If iteration is terminated early, however, this
// needs to be called on the iterator to ensure internal resources are freed.
void easyvfs_end(easyvfs_context* pContext, easyvfs_iterator* pIterator);


// Deletes the file at the given path.
//
// The path must be a absolute, or relative to the write directory.
bool easyvfs_delete_file(easyvfs_context* pContext, const char* path);

// Renames the given file.
//
// The path must be a absolute, or relative to the write directory. This will fail if the file already exists. This will
// fail if the old and new paths are across different archives. Consider using easyvfs_copy_file() for more flexibility
// with moving files to a different location.
bool easyvfs_rename_file(easyvfs_context* pContext, const char* pathOld, const char* pathNew);

// Creates a directory.
//
// The path must be a absolute, or relative to the write directory.
bool easyvfs_create_directory(easyvfs_context* pContext, const char* path);

// Copies a file.
//
// The destination path must be a absolute, or relative to the write directory.
bool easyvfs_copy_file(easyvfs_context* pContext, const char* srcPath, const char* dstPath, bool failIfExists);


// Determines whether or not the given path refers to an archive file.
//
// This does not validate that the archive file exists or is valid. This will also return false if the path refers
// to a folder on the normal file system.
//
// Use easyvfs_open_archive() to check that the archive file actually exists.
bool easyvfs_is_archive_path(easyvfs_context* pContext, const char* path);



///////////////////////////////////////////////////////////////////////////////
//
// High Level API
//
///////////////////////////////////////////////////////////////////////////////

// Free's memory that was allocated internally by easy_vfs. This is used when easy_vfs allocates memory via a high-level helper API
// and the application is done with that memory.
void easyvfs_free(void* p);

// Finds the absolute, verbose path of the given path.
bool easyvfs_find_absolute_path(easyvfs_context* pContext, const char* relativePath, char* absolutePathOut, unsigned int absolutePathOutSize);

// Finds the absolute, verbose path of the given path, using the given path as the higest priority base path.
bool easyvfs_find_absolute_path_explicit_base(easyvfs_context* pContext, const char* relativePath, const char* highestPriorityBasePath, char* absolutePathOut, unsigned int absolutePathOutSize);

// Helper function for determining whether or not the given path refers to a base directory.
bool easyvfs_is_base_directory(easyvfs_context* pContext, const char* baseDir);

// Helper function for writing a string.
bool easyvfs_write_string(easyvfs_file* pFile, const char* str);

// Helper function for writing a string, and then inserting a new line right after it.
//
// The new line character is "\n" and NOT "\r\n".
bool easyvfs_write_line(easyvfs_file* pFile, const char* str);


// Helper function for opening a binary file and retrieving it's data in one go.
//
// Free the returned pointer with easyvfs_free()
void* easyvfs_open_and_read_binary_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, size_t* pSizeInBytesOut);

// Helper function for opening a text file and retrieving it's data in one go.
//
// Free the returned pointer with easyvfs_free().
//
// The returned string is null terminated. The size returned by pSizeInBytesOut does not include the null terminator.
char* easyvfs_open_and_read_text_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, size_t* pSizeInBytesOut);

// Helper function for opening a file, writing the given data, and then closing it. This deletes the contents of the existing file, if any.
bool easyvfs_open_and_write_binary_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, const void* pData, size_t dataSize);

// Helper function for opening a file, writing the given textual data, and then closing it. This deletes the contents of the existing file, if any.
bool easyvfs_open_and_write_text_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, const char* pTextData);


// Helper function for determining whether or not the given path refers to an existing file or directory.
bool easyvfs_exists(easyvfs_context* pContext, const char* absoluteOrRelativePath);

// Determines if the given path refers to an existing file (not a directory).
//
// This will return false for directories. Use easyvfs_exists() to check for either a file or directory.
bool easyvfs_is_existing_file(easyvfs_context* pContext, const char* absoluteOrRelativePath);

// Determines if the given path refers to an existing directory.
bool easyvfs_is_existing_directory(easyvfs_context* pContext, const char* absoluteOrRelativePath);

// Same as easyvfs_mkdir(), except creates the entire directory structure recursively.
bool easyvfs_create_directory_recursive(easyvfs_context* pContext, const char* path);

// Determines whether or not the given file is at the end.
//
// This is just a high-level helper function equivalent to easyvfs_tell(pFile) == easyvfs_file_size(pFile).
bool easyvfs_eof(easyvfs_file* pFile);



#ifdef __cplusplus
}
#endif

#endif  //easy_vfs_h



///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////

#ifdef EASY_VFS_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define PRIVATE static

// Whether or not the file owns the archive object it's part of.
#define EASY_VFS_OWNS_PARENT_ARCHIVE       0x00000001



typedef struct
{
    // The absolute path of the base path.
    char absolutePath[EASYVFS_MAX_PATH];
} easyvfs_basepath;

typedef struct
{
    // A pointer to the buffer containing the list of base paths.
    easyvfs_basepath* pBuffer;

    // The size of the buffer, in easyvfs_basepath's.
    unsigned int bufferSize;

    // The number of items in the list.
    unsigned int count;

} easyvfs_basedirs;

PRIVATE bool easyvfs_basedirs_init(easyvfs_basedirs* pBasePaths)
{
    if (pBasePaths == NULL) {
        return false;
    }

    pBasePaths->pBuffer    = 0;
    pBasePaths->bufferSize = 0;
    pBasePaths->count      = 0;
    
    return true;
}

PRIVATE void easyvfs_basedirs_uninit(easyvfs_basedirs* pBasePaths)
{
    if (pBasePaths == NULL) {
        return;
    }

    free(pBasePaths->pBuffer);
}

PRIVATE bool easyvfs_basedirs_inflateandinsert(easyvfs_basedirs* pBasePaths, const char* absolutePath, unsigned int index)
{
    if (pBasePaths == NULL) {
        return false;
    }

    unsigned int newBufferSize = (pBasePaths->bufferSize == 0) ? 2 : pBasePaths->bufferSize*2;

    easyvfs_basepath* pOldBuffer = pBasePaths->pBuffer;
    easyvfs_basepath* pNewBuffer = malloc(newBufferSize * sizeof(easyvfs_basepath));
    if (pNewBuffer == NULL) {
        return false;
    }

    for (unsigned int iDst = 0; iDst < index; ++iDst) {
        memcpy(pNewBuffer + iDst, pOldBuffer + iDst, sizeof(easyvfs_basepath));
    }

    strcpy_s((pNewBuffer + index)->absolutePath, EASYVFS_MAX_PATH, absolutePath);

    for (unsigned int iDst = index; iDst < pBasePaths->count; ++iDst) {
        memcpy(pNewBuffer + iDst + 1, pOldBuffer + iDst, sizeof(easyvfs_basepath));
    }


    pBasePaths->pBuffer    = pNewBuffer;
    pBasePaths->bufferSize = newBufferSize;
    pBasePaths->count     += 1;

    free(pOldBuffer);
    return true;
}

PRIVATE bool easyvfs_basedirs_movedown1slot(easyvfs_basedirs* pBasePaths, unsigned int index)
{
    if (pBasePaths == NULL || pBasePaths->count >= pBasePaths->bufferSize) {
        return false;
    }

    for (unsigned int iDst = pBasePaths->count; iDst > index; --iDst) {
        memcpy(pBasePaths->pBuffer + iDst, pBasePaths->pBuffer + iDst - 1, sizeof(easyvfs_basepath));
    }

    return true;
}

PRIVATE bool easyvfs_basedirs_insert(easyvfs_basedirs* pBasePaths, const char* absolutePath, unsigned int index)
{
    if (pBasePaths == NULL || index > pBasePaths->count) {
        return false;
    }

    if (pBasePaths->count == pBasePaths->bufferSize) {
        return easyvfs_basedirs_inflateandinsert(pBasePaths, absolutePath, index);
    } else {
        if (!easyvfs_basedirs_movedown1slot(pBasePaths, index)) {
            return false;
        }

        strcpy_s((pBasePaths->pBuffer + index)->absolutePath, EASYVFS_MAX_PATH, absolutePath);
        pBasePaths->count += 1;

        return true;
    }
}

PRIVATE bool easyvfs_basedirs_remove(easyvfs_basedirs* pBasePaths, unsigned int index)
{
    if (pBasePaths == NULL || index >= pBasePaths->count) {
        return false;
    }

    assert(pBasePaths->count > 0);

    for (unsigned int iDst = index; iDst < pBasePaths->count - 1; ++iDst) {
        memcpy(pBasePaths->pBuffer + iDst, pBasePaths->pBuffer + iDst + 1, sizeof(easyvfs_basepath));
    }

    pBasePaths->count -= 1;

    return true;
}

PRIVATE void easyvfs_basedirs_clear(easyvfs_basedirs* pBasePaths)
{
    if (pBasePaths == NULL) {
        return;
    }

    easyvfs_basedirs_uninit(pBasePaths);
    easyvfs_basedirs_init(pBasePaths);
}



typedef struct
{
    // A pointer to the buffer containing the list of base paths.
    easyvfs_archive_callbacks* pBuffer;

    // The number of items in the list and buffer. This is a tightly packed list so the size of the buffer is always the
    // same as the count.
    unsigned int count;

} easyvfs_callbacklist;

PRIVATE bool easyvfs_callbacklist_init(easyvfs_callbacklist* pList)
{
    if (pList == NULL) {
        return false;
    }

    pList->pBuffer = 0;
    pList->count   = 0;

    return true;
}

PRIVATE void easyvfs_callbacklist_uninit(easyvfs_callbacklist* pList)
{
    if (pList == NULL) {
        return;
    }

    free(pList->pBuffer);
}

PRIVATE bool easyvfs_callbacklist_inflate(easyvfs_callbacklist* pList)
{
    if (pList == NULL) {
        return false;
    }

    easyvfs_archive_callbacks* pOldBuffer = pList->pBuffer;
    easyvfs_archive_callbacks* pNewBuffer = malloc((pList->count + 1) * sizeof(easyvfs_archive_callbacks));
    if (pNewBuffer == NULL) {
        return false;
    }

    for (unsigned int iDst = 0; iDst < pList->count; ++iDst) {
        memcpy(pNewBuffer + iDst, pOldBuffer + iDst, sizeof(easyvfs_archive_callbacks));
    }

    pList->pBuffer = pNewBuffer;

    free(pOldBuffer);
    return true;
}

PRIVATE bool easyvfs_callbacklist_pushback(easyvfs_callbacklist* pList, easyvfs_archive_callbacks callbacks)
{
    if (pList == NULL) {
        return false;
    }

    if (!easyvfs_callbacklist_inflate(pList)) {
        return false;
    }

    pList->pBuffer[pList->count] = callbacks;
    pList->count  += 1;

    return true;
}

PRIVATE void easyvfs_callbacklist_clear(easyvfs_callbacklist* pList)
{
    if (pList == NULL) {
        return;
    }

    easyvfs_callbacklist_uninit(pList);
    easyvfs_callbacklist_init(pList);
}


struct easyvfs_context
{
    // The list of archive callbacks which are used for loading non-native archives. This does not include the native callbacks.
    easyvfs_callbacklist archiveCallbacks;

    // The list of base directories.
    easyvfs_basedirs baseDirectories;

    // The write base directory.
    char writeBaseDirectory[EASYVFS_MAX_PATH];

    // Keeps track of whether or not write directory guard is enabled.
    bool isWriteGuardEnabled;
};

struct easyvfs_archive
{
    // A pointer to the context that owns this archive.
    easyvfs_context* pContext;

    // A pointer to the archive that contains this archive. This can be null in which case it is the top level archive (which is always native).
    easyvfs_archive* pParentArchive;

    // A pointer to the file containing the data of the archive file.
    easyvfs_file* pFile;

    // The internal handle that was returned when the archive was opened by the archive definition.
    easyvfs_handle internalArchiveHandle;

    // Flags. Can be a combination of the following:
    //   EASY_VFS_OWNS_PARENT_ARCHIVE
    int flags;

    // The callbacks to use when working with on the archive. This contains all of the functions for opening files, reading
    // files, etc.
    easyvfs_archive_callbacks callbacks;

    // The absolute, verbose path of the archive. For native archives, this will be the name of the folder on the native file
    // system. For non-native archives (zip, etc.) this is the the path of the archive file.
    char absolutePath[EASYVFS_MAX_PATH];    // Change this to char absolutePath[1] and have it sized exactly as needed.
};

struct easyvfs_file
{
    // A pointer to the archive that contains the file. This should never be null. Retrieve a pointer to the contex from this
    // by doing pArchive->pContext. The file containing the archive's raw data can be retrieved with pArchive->pFile.
    easyvfs_archive* pArchive;

    // The internal file handle for use by the archive that owns it.
    easyvfs_handle internalFileHandle;


    // Flags. Can be a combination of the following:
    //   EASY_VFS_OWNS_PARENT_ARCHIVE
    int flags;


    // The size of the extra data.
    size_t extraDataSize;

    // A pointer to the extra data.
    char pExtraData[1];
};


//// Path Manipulation ////
typedef struct
{
    int offset;
    int length;
} easyvfs_pathsegment;

typedef struct
{
    const char* path;
    easyvfs_pathsegment segment;
}easyvfs_pathiterator;

PRIVATE bool easyvfs_next_path_segment(easyvfs_pathiterator* i)
{
    if (i != 0 && i->path != 0)
    {
        i->segment.offset = i->segment.offset + i->segment.length;
        i->segment.length = 0;

        while (i->path[i->segment.offset] != '\0' && (i->path[i->segment.offset] == '/' || i->path[i->segment.offset] == '\\')) {
            i->segment.offset += 1;
        }

        if (i->path[i->segment.offset] == '\0') {
            return false;
        }


        while (i->path[i->segment.offset + i->segment.length] != '\0' && (i->path[i->segment.offset + i->segment.length] != '/' && i->path[i->segment.offset + i->segment.length] != '\\')) {
            i->segment.length += 1;
        }

        return true;
    }

    return false;
}

bool easyvfs_prev_path_segment(easyvfs_pathiterator* i)
{
    if (i != 0 && i->path != 0 && i->segment.offset > 0)
    {
        i->segment.length = 0;

        do
        {
            i->segment.offset -= 1;
        } while (i->segment.offset > 0 && (i->path[i->segment.offset] == '/' || i->path[i->segment.offset] == '\\'));

        if (i->segment.offset == 0)
        {
            return 0;
        }


        unsigned int offsetEnd = i->segment.offset + 1;
        while (i->segment.offset > 0 && (i->path[i->segment.offset] != '/' && i->path[i->segment.offset] != '\\'))
        {
            i->segment.offset -= 1;
        }

        if (i->path[i->segment.offset] == '/' || i->path[i->segment.offset] == '\\')
        {
            i->segment.offset += 1;
        }


        i->segment.length = offsetEnd - i->segment.offset;

        return true;
    }

    return false;
}

PRIVATE easyvfs_pathiterator easyvfs_begin_path_iteration(const char* path)
{
    easyvfs_pathiterator i;
    i.path = path;
    i.segment.offset = 0;
    i.segment.length = 0;

    return i;
}

PRIVATE easyvfs_pathiterator easyvfs_last_path_segment(const char* path)
{
    easyvfs_pathiterator i;
    i.path = path;
    i.segment.offset = strlen(path);
    i.segment.length = 0;

    easyvfs_prev_path_segment(&i);
    return i;
}

PRIVATE bool easyvfs_at_end_of_path(easyvfs_pathiterator i)
{
    return !easyvfs_next_path_segment(&i);
}

PRIVATE bool easyvfs_path_segments_equal(const char* s0Path, const easyvfs_pathsegment s0, const char* s1Path, const easyvfs_pathsegment s1)
{
    if (s0Path != 0 && s1Path != 0)
    {
        if (s0.length == s1.length)
        {
            for (int i = 0; i < s0.length; ++i)
            {
                if (s0Path[s0.offset + i] != s1Path[s1.offset + i])
                {
                    return false;
                }
            }

            return true;
        }
    }

    return false;
}

PRIVATE bool easyvfs_pathiterators_equal(const easyvfs_pathiterator i0, const easyvfs_pathiterator i1)
{
    return easyvfs_path_segments_equal(i0.path, i0.segment, i1.path, i1.segment);
}

PRIVATE bool easyvfs_append_path_iterator(char* base, unsigned int baseBufferSizeInBytes, easyvfs_pathiterator i)
{
    if (base != 0)
    {
        size_t path1Length = strlen(base);
        size_t path2Length = (size_t)i.segment.length;

        if (path1Length < baseBufferSizeInBytes)
        {
            // Slash.
            if (path1Length > 0 && base[path1Length - 1] != '/' && base[path1Length - 1] != '\\')
            {
                base[path1Length] = '/';
                path1Length += 1;
            }


            // Other part.
            if (path1Length + path2Length >= baseBufferSizeInBytes)
            {
                path2Length = baseBufferSizeInBytes - path1Length - 1;      // -1 for the null terminator.
            }

            strncpy_s(base + path1Length, baseBufferSizeInBytes - path1Length, i.path + i.segment.offset, path2Length);


            return true;
        }
    }

    return false;
}

PRIVATE bool easyvfs_is_path_child(const char* childAbsolutePath, const char* parentAbsolutePath)
{
    easyvfs_pathiterator iParent = easyvfs_begin_path_iteration(parentAbsolutePath);
    easyvfs_pathiterator iChild  = easyvfs_begin_path_iteration(childAbsolutePath);

    while (easyvfs_next_path_segment(&iParent))
    {
        if (easyvfs_next_path_segment(&iChild))
        {
            // If the segment is different, the paths are different and thus it is not a descendant.
            if (!easyvfs_pathiterators_equal(iParent, iChild))
            {
                return 0;
            }
        }
        else
        {
            // The descendant is shorter which means it's impossible for it to be a descendant.
            return 0;
        }
    }

    // At this point we have finished iteration of the parent, which should be shorter one. We now do a couple of iterations of
    // the child to ensure it is indeed a direct child.
    if (easyvfs_next_path_segment(&iChild))
    {
        // It could be a child. If the next iteration fails, it's a direct child and we want to return true.
        if (!easyvfs_next_path_segment(&iChild))
        {
            return 1;
        }
    }

    return 0;
}

PRIVATE bool easyvfs_is_path_descendant(const char* descendantAbsolutePath, const char* parentAbsolutePath)
{
    easyvfs_pathiterator iParent = easyvfs_begin_path_iteration(parentAbsolutePath);
    easyvfs_pathiterator iChild  = easyvfs_begin_path_iteration(descendantAbsolutePath);

    while (easyvfs_next_path_segment(&iParent))
    {
        if (easyvfs_next_path_segment(&iChild))
        {
            // If the segment is different, the paths are different and thus it is not a descendant.
            if (!easyvfs_pathiterators_equal(iParent, iChild))
            {
                return 0;
            }
        }
        else
        {
            // The descendant is shorter which means it's impossible for it to be a descendant.
            return 0;
        }
    }

    // At this point we have finished iteration of the parent, which should be shorter one. We now do one final iteration of
    // the descendant to ensure it is indeed shorter. If so, it's a descendant.
    return easyvfs_next_path_segment(&iChild);
}

PRIVATE bool easyvfs_copy_base_path(const char* path, char* baseOut, unsigned int baseSizeInBytes)
{
    if (strcpy_s(baseOut, baseSizeInBytes, path) == 0)
    {
        if (baseOut != 0)
        {
            char* baseend = baseOut;

            // We just loop through the path until we find the last slash.
            while (baseOut[0] != '\0')
            {
                if (baseOut[0] == '/' || baseOut[0] == '\\')
                {
                    baseend = baseOut;
                }

                baseOut += 1;
            }


            // Now we just loop backwards until we hit the first non-slash.
            while (baseend > baseOut)
            {
                if (baseend[0] != '/' && baseend[0] != '\\')
                {
                    break;
                }

                baseend -= 1;
            }


            // We just put a null terminator on the end.
            baseend[0] = '\0';

            return 1;
        }
    }

    return 0;
}

PRIVATE const char* easyvfs_file_name(const char* path)
{
    if (path != 0)
    {
        const char* fileName = path;

        // We just loop through the path until we find the last slash.
        while (path[0] != '\0')
        {
            if (path[0] == '/' || path[0] == '\\')
            {
                fileName = path;
            }

            path += 1;
        }


        // At this point the file name is sitting on a slash, so just move forward.
        while (fileName[0] != '\0' && (fileName[0] == '/' || fileName[0] == '\\'))
        {
            fileName += 1;
        }


        return fileName;
    }

    return 0;
}

PRIVATE const char* easyvfs_extension(const char* path)
{
    if (path != 0)
    {
        const char* extension = easyvfs_file_name(path);
        const char* lastoccurance = 0;

        // Just find the last '.' and return.
        while (extension[0] != '\0')
        {
            extension += 1;

            if (extension[0] == '.')
            {
                extension += 1;
                lastoccurance = extension;
            }
        }

        return (lastoccurance != 0) ? lastoccurance : extension;
    }

    return 0;
}

PRIVATE bool easyvfs_paths_equal(const char* path1, const char* path2)
{
    if (path1 != 0 && path2 != 0)
    {
        easyvfs_pathiterator iPath1 = easyvfs_begin_path_iteration(path1);
        easyvfs_pathiterator iPath2 = easyvfs_begin_path_iteration(path2);

        int isPath1Valid = easyvfs_next_path_segment(&iPath1);
        int isPath2Valid = easyvfs_next_path_segment(&iPath2);
        while (isPath1Valid && isPath2Valid)
        {
            if (!easyvfs_pathiterators_equal(iPath1, iPath2))
            {
                return 0;
            }

            isPath1Valid = easyvfs_next_path_segment(&iPath1);
            isPath2Valid = easyvfs_next_path_segment(&iPath2);
        }


        // At this point either iPath1 and/or iPath2 have finished iterating. If both of them are at the end, the two paths are equal.
        return isPath1Valid == isPath2Valid && iPath1.path[iPath1.segment.offset] == '\0' && iPath2.path[iPath2.segment.offset] == '\0';
    }

    return 0;
}

PRIVATE bool easyvfs_is_path_relative(const char* path)
{
    if (path != 0 && path[0] != '\0')
    {
        if (path[0] == '/')
        {
            return 0;
        }

        if (path[1] != '\0')
        {
            if (((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) && path[1] == ':')
            {
                return 0;
            }
        }
    }

    return 1;
}

PRIVATE bool easyvfs_is_path_absolute(const char* path)
{
    return !easyvfs_is_path_relative(path);
}

PRIVATE bool easyvfs_append_path(char* base, unsigned int baseBufferSizeInBytes, const char* other)
{
    if (base != 0 && other != 0)
    {
        size_t path1Length = strlen(base);
        size_t path2Length = strlen(other);

        if (path1Length < baseBufferSizeInBytes)
        {
            // Slash.
            if (path1Length > 0 && base[path1Length - 1] != '/' && base[path1Length - 1] != '\\') {
                base[path1Length] = '/';
                path1Length += 1;
            }


            // Other part.
            if (path1Length + path2Length >= baseBufferSizeInBytes) {
                path2Length = baseBufferSizeInBytes - path1Length - 1;      // -1 for the null terminator.
            }

            strncpy_s(base + path1Length, baseBufferSizeInBytes - path1Length, other, path2Length);


            return 1;
        }
    }

    return 0;
}

PRIVATE bool easyvfs_copy_and_append_path(char* dst, unsigned int dstSizeInBytes, const char* base, const char* other)
{
    if (dst != NULL && dstSizeInBytes > 0)
    {
        strcpy_s(dst, dstSizeInBytes, base);
        return easyvfs_append_path(dst, dstSizeInBytes, other);
    }

    return 0;
}

// This function recursively cleans a path which is defined as a chain of iterators. This function works backwards, which means at the time of calling this
// function, each iterator in the chain should be placed at the end of the path.
//
// This does not write the null terminator.
unsigned int easyvfs_easypath_clean_trywrite(easyvfs_pathiterator* iterators, unsigned int iteratorCount, char* pathOut, unsigned int pathOutSizeInBytes, unsigned int ignoreCounter)
{
    if (iteratorCount == 0) {
        return 0;
    }

    easyvfs_pathiterator isegment = iterators[iteratorCount - 1];


    // If this segment is a ".", we ignore it. If it is a ".." we ignore it and increment "ignoreCount".
    int ignoreThisSegment = ignoreCounter > 0 && isegment.segment.length > 0;

    if (isegment.segment.length == 1 && isegment.path[isegment.segment.offset] == '.')
    {
        // "."
        ignoreThisSegment = 1;
    }
    else
    {
        if (isegment.segment.length == 2 && isegment.path[isegment.segment.offset] == '.' && isegment.path[isegment.segment.offset + 1] == '.')
        {
            // ".."
            ignoreThisSegment = 1;
            ignoreCounter += 1;
        }
        else
        {
            // It's a regular segment, so decrement the ignore counter.
            if (ignoreCounter > 0)
            {
                ignoreCounter -= 1;
            }
        }
    }

    
    // The previous segment needs to be written before we can write this one.
    unsigned int bytesWritten = 0;

    easyvfs_pathiterator prev = isegment;
    if (!easyvfs_prev_path_segment(&prev))
    {
        if (iteratorCount > 1)
        {
            iteratorCount -= 1;
            prev = iterators[iteratorCount - 1];
        }
        else
        {
            prev.path           = NULL;
            prev.segment.offset = 0;
            prev.segment.length = 0;
        }
    }

    if (prev.segment.length > 0)
    {
        iterators[iteratorCount - 1] = prev;
        bytesWritten = easyvfs_easypath_clean_trywrite(iterators, iteratorCount, pathOut, pathOutSizeInBytes, ignoreCounter);
    }


    if (!ignoreThisSegment)
    {
        if (pathOutSizeInBytes > 0)
        {
            pathOut            += bytesWritten;
            pathOutSizeInBytes -= bytesWritten;

            if (bytesWritten > 0)
            {
                pathOut[0] = '/';
                bytesWritten += 1;

                pathOut            += 1;
                pathOutSizeInBytes -= 1;
            }

            if (pathOutSizeInBytes >= (unsigned int)isegment.segment.length)
            {
                strncpy_s(pathOut, pathOutSizeInBytes, isegment.path + isegment.segment.offset, isegment.segment.length);
                bytesWritten += isegment.segment.length;
            }
            else
            {
                strncpy_s(pathOut, pathOutSizeInBytes, isegment.path + isegment.segment.offset, pathOutSizeInBytes);
                bytesWritten += pathOutSizeInBytes;
            }
        }
    }

    return bytesWritten;
}

PRIVATE int easyvfs_append_and_clean(char* dst, unsigned int dstSizeInBytes, const char* base, const char* other)
{
    if (base != 0 && other != 0)
    {
        int iLastSeg = -1;

        easyvfs_pathiterator last[2];
        last[0] = easyvfs_last_path_segment(base);
        last[1] = easyvfs_last_path_segment(other);

        if (last[1].segment.length > 0) {
            iLastSeg = 1;
        } else if (last[0].segment.length > 0) {
            iLastSeg = 0;
        } else {
            // Both input strings are empty.
            return 0;
        }


        unsigned int bytesWritten = easyvfs_easypath_clean_trywrite(last, 2, dst, dstSizeInBytes - 1, 0);  // -1 to ensure there is enough room for a null terminator later on.
        if (dstSizeInBytes > bytesWritten)
        {
            dst[bytesWritten] = '\0';
        }

        return bytesWritten + 1;
    }

    return 0;
}


//// Private Utiltities ////

// Recursively creates the given directory structure on the native file system.
PRIVATE bool easyvfs_mkdir_recursive_native(const char* absolutePath);

// Determines whether or not the given path is valid for writing based on the base write path and whether or not
// the write guard is enabled.
PRIVATE bool easyvfs_validate_write_path(easyvfs_context* pContext, const char* absoluteOrRelativePath, char* absolutePathOut, unsigned int absolutePathOutSize)
{
    // If the path is relative, we need to convert to absolute. Then, if the write directory guard is enabled, we need to check that it's a descendant of the base path.
    if (easyvfs_is_path_relative(absoluteOrRelativePath)) {
        if (easyvfs_append_and_clean(absolutePathOut, absolutePathOutSize, pContext->writeBaseDirectory, absoluteOrRelativePath)) {
            absoluteOrRelativePath = absolutePathOut;
        } else {
            return false;
        }
    } else {
        if (strcpy_s(absolutePathOut, absolutePathOutSize, absoluteOrRelativePath) != 0) {
            return false;
        }
    }

    assert(easyvfs_is_path_absolute(absoluteOrRelativePath));

    if (easyvfs_is_write_directory_guard_enabled(pContext)) {
        if (easyvfs_is_path_descendant(absoluteOrRelativePath, pContext->writeBaseDirectory)) {
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }
}

// A simple helper function for determining the access mode to use for an archive file based on the access mode
// of a file within that archive.
PRIVATE unsigned int easyvfs_archive_access_mode(unsigned int fileAccessMode)
{
    return (fileAccessMode == EASYVFS_READ) ? EASYVFS_READ : EASYVFS_READ | EASYVFS_WRITE | EASYVFS_EXISTING;
}



//// Platform-Specific Section ////

// The functions in this section implement a common abstraction for working with files on the native file system. When adding
// support for a new platform you'll probably want to either implement a platform-specific back-end or force stdio.

#if defined(EASY_VFS_FORCE_STDIO)
#define EASY_VFS_USE_STDIO
#else
#if defined(_WIN32)
#define EASY_VFS_USE_WIN32
#else
#define EASY_VFS_USE_STDIO
#endif
#endif

// Low-level function for opening a file on the native file system.
//
// This will fail if attempting to open a file that's inside an archive such as a zip file. It can only open
// files that are sitting on the native file system.
//
// The given file path must be absolute.
PRIVATE easyvfs_handle easyvfs_open_native_file(const char* absolutePath, unsigned int accessMode);

// Closes the given native file.
PRIVATE void easyvfs_close_native_file(easyvfs_handle file);

// Determines whether or not the given path refers to a directory on the native file system.
PRIVATE bool easyvfs_is_native_directory(const char* absolutePath);

// Determines whether or not the given path refers to a file on the native file system. This will return false for directories.
PRIVATE bool easyvfs_is_native_directory(const char* absolutePath);

// Deletes a native file.
PRIVATE bool easyvfs_delete_native_file(const char* absolutePath);

// Renames a native file. Fails if the target already exists.
PRIVATE bool easyvfs_rename_native_file(const char* absolutePathOld, const char* absolutePathNew);

// Creates a directory on the native file system.
PRIVATE bool easyvfs_mkdir_native(const char* absolutePath);

// Creates a copy of a native file.
PRIVATE bool easyvfs_copy_native_file(const char* absolutePathSrc, const char* absolutePathDst, bool failIfExists);

// Reads data from the given native file.
PRIVATE bool easyvfs_read_native_file(easyvfs_handle file, void* pDataOut, size_t bytesToRead, size_t* pBytesReadOut);

// Writes data to the given native file.
PRIVATE bool easyvfs_write_native_file(easyvfs_handle file, const void* pData, size_t bytesToWrite, size_t* pBytesWrittenOut);

// Seeks the given native file.
PRIVATE bool easyvfs_seek_native_file(easyvfs_handle file, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin);

// Retrieves the read/write pointer of the given native file.
PRIVATE easyvfs_uint64 easyvfs_tell_native_file(easyvfs_handle file);

// Retrieves the size of the given native file.
PRIVATE easyvfs_uint64 easyvfs_get_native_file_size(easyvfs_handle file);

// Flushes the given native file.
PRIVATE void easyvfs_flush_native_file(easyvfs_handle file);

// Retrieves information about the file OR DIRECTORY at the given path on the native file system.
//
// <fi> is allowed to be null, in which case the call is equivalent to simply checking if the file or directory exists.
PRIVATE bool easyvfs_get_native_file_info(const char* absolutePath, easyvfs_file_info* fi);

// Creates an iterator for iterating over the native files in the given directory.
//
// The iterator will be deleted with easyvfs_end_native_iteration().
PRIVATE easyvfs_handle easyvfs_begin_native_iteration(const char* absolutePath);

// Deletes the iterator that was returned with easyvfs_end_native_iteration().
PRIVATE void easyvfs_end_native_iteration(easyvfs_handle iterator);

// Retrieves information about the next native file based on the given iterator.
PRIVATE bool easyvfs_next_native_iteration(easyvfs_handle iterator, easyvfs_file_info* fi);



#ifdef EASY_VFS_USE_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

PRIVATE easyvfs_handle easyvfs_open_native_file(const char* absolutePath, unsigned int accessMode)
{
    assert(absolutePath != NULL);

    DWORD dwDesiredAccess       = 0;
    DWORD dwShareMode           = 0;
    DWORD dwCreationDisposition = OPEN_EXISTING;

    if ((accessMode & EASYVFS_READ) != 0) {
        dwDesiredAccess |= FILE_GENERIC_READ;
        dwShareMode     |= FILE_SHARE_READ;
    }

    if ((accessMode & EASYVFS_WRITE) != 0) {
        dwDesiredAccess |= FILE_GENERIC_WRITE;

        if ((accessMode & EASYVFS_EXISTING) != 0) {
            if ((accessMode & EASYVFS_TRUNCATE) != 0) {
                dwCreationDisposition = TRUNCATE_EXISTING;
            } else {
                dwCreationDisposition = OPEN_EXISTING;
            }
        } else {
            if ((accessMode & EASYVFS_TRUNCATE) != 0) {
                dwCreationDisposition = CREATE_ALWAYS;
            } else {
                dwCreationDisposition = OPEN_ALWAYS;
            }
        }
    }


    HANDLE hFile = CreateFileA(absolutePath, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        // We failed to create the file, however it may be because we are trying to create the file and the directory structure does not exist yet. In this
        // case we want to try creating the directory structure and try again.
        if ((accessMode & EASYVFS_WRITE) != 0 && (accessMode & EASYVFS_CREATE_DIRS) != 0)
        {
            char dirAbsolutePath[EASYVFS_MAX_PATH];
            if (easyvfs_copy_base_path(absolutePath, dirAbsolutePath, sizeof(dirAbsolutePath))) {
                if (!easyvfs_is_native_directory(dirAbsolutePath) && easyvfs_mkdir_recursive_native(dirAbsolutePath)) {
                    hFile = CreateFileA(absolutePath, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
                }
            }
        }
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        return (easyvfs_handle)hFile;
    }

    return NULL;
}

PRIVATE void easyvfs_close_native_file(easyvfs_handle file)
{
    CloseHandle((HANDLE)file);
}

PRIVATE bool easyvfs_is_native_directory(const char* absolutePath)
{
    DWORD attributes = GetFileAttributesA(absolutePath);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

PRIVATE bool easyvfs_delete_native_file(const char* absolutePath)
{
    DWORD attributes = GetFileAttributesA(absolutePath);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return DeleteFileA(absolutePath);
    } else {
        return RemoveDirectoryA(absolutePath);
    }
}

PRIVATE bool easyvfs_is_native_file(const char* absolutePath)
{
    DWORD attributes = GetFileAttributesA(absolutePath);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

PRIVATE bool easyvfs_rename_native_file(const char* absolutePathOld, const char* absolutePathNew)
{
    return MoveFileExA(absolutePathOld, absolutePathNew, 0);
}

PRIVATE bool easyvfs_mkdir_native(const char* absolutePath)
{
    return CreateDirectoryA(absolutePath, NULL);
}

PRIVATE bool easyvfs_copy_native_file(const char* absolutePathSrc, const char* absolutePathDst, bool failIfExists)
{
    return CopyFileA(absolutePathSrc, absolutePathDst, failIfExists);
}

PRIVATE bool easyvfs_read_native_file(easyvfs_handle file, void* pDataOut, size_t bytesToRead, size_t* pBytesReadOut)
{
    // Unfortunately Win32 expects a DWORD for the number of bytes to read, however we accept size_t. We need to loop to ensure
    // we handle data > 4GB correctly.

    size_t totalBytesRead = 0;

    char* pDst = pDataOut;
    easyvfs_uint64 bytesRemaining = bytesToRead;
    while (bytesRemaining > 0)
    {
        DWORD bytesToProcess;
        if (bytesRemaining > UINT_MAX) {
            bytesToProcess = UINT_MAX;
        } else {
            bytesToProcess = (DWORD)bytesRemaining;
        }


        DWORD bytesRead;
        BOOL result = ReadFile((HANDLE)file, pDst, bytesToProcess, &bytesRead, NULL);
        if (!result)
        {
            // We failed to read the chunk.
            totalBytesRead += bytesRead;

            if (pBytesReadOut) {
                *pBytesReadOut = totalBytesRead;
                return false;
            }
        }

        pDst += bytesToProcess;
        bytesRemaining -= bytesToProcess;
        totalBytesRead += bytesToProcess;
    }


    if (pBytesReadOut != NULL) {
        *pBytesReadOut = totalBytesRead;
    }

    return true;
}

PRIVATE bool easyvfs_write_native_file(easyvfs_handle file, const void* pData, size_t bytesToWrite, size_t* pBytesWrittenOut)
{
    // Unfortunately Win32 expects a DWORD for the number of bytes to write, however we accept size_t. We need to loop to ensure
    // we handle data > 4GB correctly.

    size_t totalBytesWritten = 0;
    const char* pSrc = pData;

    size_t bytesRemaining = bytesToWrite;
    while (bytesRemaining > 0)
    {
        DWORD bytesToProcess;
        if (bytesRemaining > UINT_MAX) {
            bytesToProcess = UINT_MAX;
        } else {
            bytesToProcess = (DWORD)bytesRemaining;
        }

        DWORD bytesWritten;
        BOOL result = WriteFile((HANDLE)file, pSrc, bytesToProcess, &bytesWritten, NULL);
        if (!result)
        {
            // We failed to write the chunk.
            totalBytesWritten += bytesWritten;

            if (pBytesWrittenOut) {
                *pBytesWrittenOut = totalBytesWritten;
                return false;
            }
        }

        pSrc += bytesToProcess;
        bytesRemaining -= bytesToProcess;
        totalBytesWritten += bytesToProcess;
    }

    if (pBytesWrittenOut) {
        *pBytesWrittenOut = totalBytesWritten;
    }

    return true;
}

PRIVATE bool easyvfs_seek_native_file(easyvfs_handle file, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
{
    LARGE_INTEGER lNewFilePointer;
    LARGE_INTEGER lDistanceToMove;
    lDistanceToMove.QuadPart = bytesToSeek;

    DWORD dwMoveMethod = FILE_CURRENT;
    if (origin == easyvfs_origin_start) {
        dwMoveMethod = FILE_BEGIN;
    } else if (origin == easyvfs_origin_end) {
        dwMoveMethod = FILE_END;
    }

    return SetFilePointerEx((HANDLE)file, lDistanceToMove, &lNewFilePointer, dwMoveMethod);
}

PRIVATE easyvfs_uint64 easyvfs_tell_native_file(easyvfs_handle file)
{
    LARGE_INTEGER lNewFilePointer;
    LARGE_INTEGER lDistanceToMove;
    lDistanceToMove.QuadPart = 0;

    if (SetFilePointerEx((HANDLE)file, lDistanceToMove, &lNewFilePointer, FILE_CURRENT)) {
        return (easyvfs_uint64)lNewFilePointer.QuadPart;
    }

    return 0;
}

PRIVATE easyvfs_uint64 easyvfs_get_native_file_size(easyvfs_handle file)
{
    LARGE_INTEGER fileSize;
    if (GetFileSizeEx((HANDLE)file, &fileSize)) {
        return (easyvfs_uint64)fileSize.QuadPart;
    }

    return 0;
}

PRIVATE void easyvfs_flush_native_file(easyvfs_handle file)
{
    FlushFileBuffers((HANDLE)file);
}

PRIVATE bool easyvfs_get_native_file_info(const char* absolutePath, easyvfs_file_info* fi)
{
    assert(absolutePath != NULL);
    assert(fi != NULL);

    // <fi> is allowed to be null, in which case the call is equivalent to simply checking if the file exists.
    if (fi == NULL) {
        return GetFileAttributesA(absolutePath) != INVALID_FILE_ATTRIBUTES;
    }

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(absolutePath, GetFileExInfoStandard, &fad))
    {
        ULARGE_INTEGER liSize;
        liSize.LowPart  = fad.nFileSizeLow;
        liSize.HighPart = fad.nFileSizeHigh;

        ULARGE_INTEGER liTime;
        liTime.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
        liTime.HighPart = fad.ftLastWriteTime.dwHighDateTime;

        strcpy_s(fi->absolutePath, sizeof(fi->absolutePath), absolutePath);
        fi->sizeInBytes      = liSize.QuadPart;
        fi->lastModifiedTime = liTime.QuadPart;

        fi->attributes = 0;
        if ((fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            fi->attributes |= EASYVFS_FILE_ATTRIBUTE_DIRECTORY;
        }
        if ((fad.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0) {
            fi->attributes |= EASYVFS_FILE_ATTRIBUTE_READONLY;
        }

        return true;
    }

    return false;
}

typedef struct
{
    HANDLE hFind;
    WIN32_FIND_DATAA ffd;
    char directoryPath[1];
} easyvfs_iterator_win32;

PRIVATE easyvfs_handle easyvfs_begin_native_iteration(const char* absolutePath)
{
    assert(easyvfs_is_path_absolute(absolutePath));

    char searchQuery[EASYVFS_MAX_PATH];
    if (strcpy_s(searchQuery, sizeof(searchQuery), absolutePath) != 0) {
        return NULL;
    }

    unsigned int searchQueryLength = (unsigned int)strlen(searchQuery);
    if (searchQueryLength >= EASYVFS_MAX_PATH - 3) {
        return NULL;    // Path is too long.
    }

    searchQuery[searchQueryLength + 0] = '\\';
    searchQuery[searchQueryLength + 1] = '*';
    searchQuery[searchQueryLength + 2] = '\0';

    easyvfs_iterator_win32* pIterator = malloc(sizeof(*pIterator) + searchQueryLength);
    if (pIterator == NULL) {
        return NULL;
    }

    ZeroMemory(pIterator, sizeof(*pIterator));

    pIterator->hFind = FindFirstFileA(searchQuery, &pIterator->ffd);
    if (pIterator->hFind == INVALID_HANDLE_VALUE) {
        free(pIterator);
        return NULL;    // Failed to begin search.
    }

    strcpy_s(pIterator->directoryPath, searchQueryLength + 1, absolutePath);     // This won't fail in practice.
    return (easyvfs_handle)pIterator;
}

PRIVATE void easyvfs_end_native_iteration(easyvfs_handle iterator)
{
    easyvfs_iterator_win32* pIterator = iterator;
    assert(pIterator != NULL);

    if (pIterator->hFind) {
        FindClose(pIterator->hFind);
    }
    
    free(pIterator);
}

PRIVATE bool easyvfs_next_native_iteration(easyvfs_handle iterator, easyvfs_file_info* fi)
{
    easyvfs_iterator_win32* pIterator = iterator;
    assert(pIterator != NULL);

    if (pIterator->hFind != INVALID_HANDLE_VALUE && pIterator->hFind != NULL)
    {
        // Skip past "." and ".." directories.
        while (strcmp(pIterator->ffd.cFileName, ".") == 0 || strcmp(pIterator->ffd.cFileName, "..") == 0) {
            if (FindNextFileA(pIterator->hFind, &pIterator->ffd) == 0) {
                return false;
            }
        }

        if (fi != NULL)
        {
            // The absolute path actually needs to be set to the relative path. The higher level APIs are the once responsible for translating
            // it back to an absolute path.
            strcpy_s(fi->absolutePath, sizeof(fi->absolutePath), pIterator->ffd.cFileName);

            ULARGE_INTEGER liSize;
            liSize.LowPart  = pIterator->ffd.nFileSizeLow;
            liSize.HighPart = pIterator->ffd.nFileSizeHigh;
            fi->sizeInBytes = liSize.QuadPart;

            ULARGE_INTEGER liTime;
            liTime.LowPart  = pIterator->ffd.ftLastWriteTime.dwLowDateTime;
            liTime.HighPart = pIterator->ffd.ftLastWriteTime.dwHighDateTime;
            fi->lastModifiedTime = liTime.QuadPart;

            fi->attributes = 0;
            if ((pIterator->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                fi->attributes |= EASYVFS_FILE_ATTRIBUTE_DIRECTORY;
            }
            if ((pIterator->ffd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0) {
                fi->attributes |= EASYVFS_FILE_ATTRIBUTE_READONLY;
            }
        }

        if (!FindNextFileA(pIterator->hFind, &pIterator->ffd)) {
            FindClose(pIterator->hFind);
            pIterator->hFind = NULL;
        }

        return true;
    }

    return false;
}
#endif //EASY_VFS_USE_WIN32


PRIVATE bool easyvfs_mkdir_recursive_native(const char* absolutePath)
{
    char runningPath[EASYVFS_MAX_PATH];
    runningPath[0] = '\0';

    easyvfs_pathiterator iPathSeg = easyvfs_begin_path_iteration(absolutePath);

    // Never check the first segment because we can assume it always exists - it will always be the drive root.
    if (easyvfs_next_path_segment(&iPathSeg) && easyvfs_append_path_iterator(runningPath, sizeof(runningPath), iPathSeg))
    {
        // Loop over every directory until we find one that does not exist.
        while (easyvfs_next_path_segment(&iPathSeg))
        {
            if (!easyvfs_append_path_iterator(runningPath, sizeof(runningPath), iPathSeg)) {
                return false;
            }

            if (!easyvfs_is_native_directory(runningPath)) {
                if (!easyvfs_mkdir_native(runningPath)) {
                    return false;
                }

                break;
            }
        }


        // At this point all we need to do is create the remaining directories - we can assert that the directory does not exist
        // rather than actually checking it which should be a bit more efficient.
        while (easyvfs_next_path_segment(&iPathSeg))
        {
            if (!easyvfs_append_path_iterator(runningPath, sizeof(runningPath), iPathSeg)) {
                return false;
            }

            assert(!easyvfs_is_native_directory(runningPath));

            if (!easyvfs_mkdir_native(runningPath)) {
                return false;
            }
        }

        return true;
    }

    return false;
}



//// Native Archive Implementation ////

typedef struct
{
    // The access mode.
    unsigned int accessMode;

    // The absolute path of the directory.
    char absolutePath[1];

} easyvfs_archive_native;

PRIVATE easyvfs_handle easyvfs_open_archive__native(easyvfs_file* pArchiveFile, unsigned int accessMode)
{
    (void)pArchiveFile;
    (void)accessMode;

    // This function should never be called for native archives. Native archives are a special case because they are just directories
    // on the file system rather than actual files, and as such they are handled just slightly differently. This function is only
    // included here for this assert so we can ensure we don't incorrectly call this function.
    assert(false);

    return 0;
}

PRIVATE easyvfs_handle easyvfs_open_archive__native__special(const char* absolutePath, unsigned int accessMode)
{
    assert(absolutePath != NULL);       // There is no notion of a file for native archives (which are just directories on the file system).

    size_t absolutePathLen = strlen(absolutePath);

    easyvfs_archive_native* pNativeArchive = malloc(sizeof(*pNativeArchive) + absolutePathLen + 1);
    if (pNativeArchive == NULL) {
        return NULL;
    }

    strcpy_s(pNativeArchive->absolutePath, absolutePathLen + 1, absolutePath);
    pNativeArchive->accessMode = accessMode;

    return (easyvfs_handle)pNativeArchive;
}

PRIVATE void easyvfs_close_archive__native(easyvfs_handle archive)
{
    // Nothing to do except free the object.
    free(archive);
}

PRIVATE bool easyvfs_get_file_info__native(easyvfs_handle archive, const char* relativePath, easyvfs_file_info* fi)
{
    easyvfs_archive_native* pNativeArchive = archive;
    assert(pNativeArchive != NULL);

    char absolutePath[EASYVFS_MAX_PATH];
    if (!easyvfs_copy_and_append_path(absolutePath, sizeof(absolutePath), pNativeArchive->absolutePath, relativePath)) {
        return false;
    }

    memset(fi, sizeof(fi), 0);
    return easyvfs_get_native_file_info(absolutePath, fi);
}

PRIVATE easyvfs_handle easyvfs_begin_iteration__native(easyvfs_handle archive, const char* relativePath)
{
    easyvfs_archive_native* pNativeArchive = archive;
    assert(pNativeArchive != NULL);

    char absolutePath[EASYVFS_MAX_PATH];
    if (!easyvfs_copy_and_append_path(absolutePath, sizeof(absolutePath), pNativeArchive->absolutePath, relativePath)) {
        return NULL;
    }

    return easyvfs_begin_native_iteration(absolutePath);
}

PRIVATE void easyvfs_end_iteration__native(easyvfs_handle archive, easyvfs_handle iterator)
{
    (void)archive;
    assert(archive != NULL);
    assert(iterator != NULL);

    easyvfs_end_native_iteration(iterator);
}

PRIVATE bool easyvfs_next_iteration__native(easyvfs_handle archive, easyvfs_handle iterator, easyvfs_file_info* fi)
{
    (void)archive;
    assert(archive != NULL);
    assert(iterator != NULL);

    return easyvfs_next_native_iteration(iterator, fi);
}

PRIVATE bool easyvfs_delete_file__native(easyvfs_handle archive, const char* relativePath)
{
    assert(relativePath != NULL);

    easyvfs_archive_native* pNativeArchive = archive;
    assert(pNativeArchive != NULL);

    char absolutePath[EASYVFS_MAX_PATH];
    if (!easyvfs_copy_and_append_path(absolutePath, sizeof(absolutePath), pNativeArchive->absolutePath, relativePath)) {
        return false;
    }

    return easyvfs_delete_native_file(absolutePath);
}

PRIVATE bool easyvfs_rename_file__native(easyvfs_handle archive, const char* relativePathOld, const char* relativePathNew)
{
    assert(relativePathOld != NULL);
    assert(relativePathNew != NULL);

    easyvfs_archive_native* pNativeArchive = archive;
    assert(pNativeArchive != NULL);

    char absolutePathOld[EASYVFS_MAX_PATH];
    if (!easyvfs_copy_and_append_path(absolutePathOld, sizeof(absolutePathOld), pNativeArchive->absolutePath, relativePathOld)) {
        return false;
    }

    char absolutePathNew[EASYVFS_MAX_PATH];
    if (!easyvfs_copy_and_append_path(absolutePathNew, sizeof(absolutePathNew), pNativeArchive->absolutePath, relativePathNew)) {
        return false;
    }

    return easyvfs_rename_native_file(absolutePathOld, absolutePathNew);
}

PRIVATE bool easyvfs_create_directory__native(easyvfs_handle archive, const char* relativePath)
{
    assert(relativePath != NULL);

    easyvfs_archive_native* pNativeArchive = archive;
    assert(pNativeArchive != NULL);

    char absolutePath[EASYVFS_MAX_PATH];
    if (!easyvfs_copy_and_append_path(absolutePath, sizeof(absolutePath), pNativeArchive->absolutePath, relativePath)) {
        return false;
    }

    return easyvfs_mkdir_native(absolutePath);
}

PRIVATE bool easyvfs_copy_file__native(easyvfs_handle archive, const char* relativePathSrc, const char* relativePathDst, bool failIfExists)
{
    assert(relativePathSrc != NULL);
    assert(relativePathDst != NULL);

    easyvfs_archive_native* pNativeArchive = archive;
    assert(pNativeArchive != NULL);

    char absolutePathSrc[EASYVFS_MAX_PATH];
    if (!easyvfs_copy_and_append_path(absolutePathSrc, sizeof(absolutePathSrc), pNativeArchive->absolutePath, relativePathSrc)) {
        return false;
    }

    char absolutePathDst[EASYVFS_MAX_PATH];
    if (!easyvfs_copy_and_append_path(absolutePathDst, sizeof(absolutePathDst), pNativeArchive->absolutePath, relativePathDst)) {
        return false;
    }

    return easyvfs_copy_native_file(absolutePathSrc, absolutePathDst, failIfExists);
}

PRIVATE easyvfs_handle easyvfs_open_file__native(easyvfs_handle archive, const char* relativePath, unsigned int accessMode)
{
    assert(archive != NULL);
    assert(relativePath != NULL);

    easyvfs_archive_native* pNativeArchive = archive;
    assert(pNativeArchive != NULL);

    char absolutePath[EASYVFS_MAX_PATH];
    if (easyvfs_copy_and_append_path(absolutePath, sizeof(absolutePath), pNativeArchive->absolutePath, relativePath)) {
        return easyvfs_open_native_file(absolutePath, accessMode);
    }

    return NULL;
}

PRIVATE void easyvfs_close_file__native(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;
    assert(archive != NULL);
    assert(file != NULL);

    easyvfs_close_native_file(file);
}

PRIVATE bool easyvfs_read_file__native(easyvfs_handle archive, easyvfs_handle file, void* pDataOut, size_t bytesToRead, size_t* pBytesReadOut)
{
    (void)archive;
    assert(archive != NULL);
    assert(file != NULL);

    return easyvfs_read_native_file(file, pDataOut, bytesToRead, pBytesReadOut);
}

PRIVATE bool easyvfs_write_file__native(easyvfs_handle archive, easyvfs_handle file, const void* pData, size_t bytesToWrite, size_t* pBytesWrittenOut)
{
    (void)archive;
    assert(archive != NULL);
    assert(file != NULL);

    return easyvfs_write_native_file(file, pData, bytesToWrite, pBytesWrittenOut);
}

PRIVATE bool easyvfs_seek_file__native(easyvfs_handle archive, easyvfs_handle file, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
{
    (void)archive;
    assert(archive != NULL);
    assert(file != NULL);

    return easyvfs_seek_native_file(file, bytesToSeek, origin);
}

PRIVATE easyvfs_uint64 easyvfs_tell_file__native(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;
    assert(archive != NULL);
    assert(file != NULL);

    return easyvfs_tell_native_file(file);
}

PRIVATE easyvfs_uint64 easyvfs_file_size__native(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;
    assert(archive != NULL);
    assert(file != NULL);

    return easyvfs_get_native_file_size(file);
}

PRIVATE void easyvfs_flush__native(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;
    assert(archive != NULL);
    assert(file != NULL);

    easyvfs_flush_native_file(file);
}


// Finds the back-end callbacks by the given extension.
PRIVATE bool easyvfs_find_backend_by_extension(easyvfs_context* pContext, const char* extension, easyvfs_archive_callbacks* pCallbacksOut)
{
    if (pContext == NULL || extension == NULL || extension[0] == '\0') {
        return false;
    }

    for (unsigned int i = 0; i < pContext->archiveCallbacks.count; ++i)
    {
        if (pContext->archiveCallbacks.pBuffer[i].is_valid_extension) {
            if (pContext->archiveCallbacks.pBuffer[i].is_valid_extension(extension)) {
                if (pCallbacksOut) {
                    *pCallbacksOut = pContext->archiveCallbacks.pBuffer[i];
                }
                return true;
            }
        }
    }

    return false;
}

// Recursively claims ownership of parent archives so that when the child archive is deleted, so are it's parents.
PRIVATE void easyvfs_recursively_claim_ownership_or_parent_archive(easyvfs_archive* pArchive)
{
    if (pArchive == NULL) {
        return;
    }

    pArchive->flags |= EASY_VFS_OWNS_PARENT_ARCHIVE;
    easyvfs_recursively_claim_ownership_or_parent_archive(pArchive->pParentArchive);
}

// Opens a native archive.
PRIVATE easyvfs_archive* easyvfs_open_native_archive(easyvfs_context* pContext, const char* absolutePath, unsigned int accessMode)
{
    assert(pContext != NULL);
    assert(absolutePath != NULL);

    easyvfs_handle internalArchiveHandle = easyvfs_open_archive__native__special(absolutePath, accessMode);
    if (internalArchiveHandle == NULL) {
        return NULL;
    }

    easyvfs_archive* pArchive = malloc(sizeof(*pArchive));
    if (pArchive == NULL) {
        return NULL;
    }

    pArchive->pContext                     = pContext;
    pArchive->pParentArchive               = NULL;
    pArchive->pFile                        = NULL;
    pArchive->internalArchiveHandle        = internalArchiveHandle;
    pArchive->flags                        = 0;
    pArchive->callbacks.is_valid_extension = NULL;
    pArchive->callbacks.open_archive       = easyvfs_open_archive__native;
    pArchive->callbacks.close_archive      = easyvfs_close_archive__native;
    pArchive->callbacks.get_file_info      = easyvfs_get_file_info__native;
    pArchive->callbacks.begin_iteration    = easyvfs_begin_iteration__native;
    pArchive->callbacks.end_iteration      = easyvfs_end_iteration__native;
    pArchive->callbacks.next_iteration     = easyvfs_next_iteration__native;
    pArchive->callbacks.delete_file        = easyvfs_delete_file__native;
    pArchive->callbacks.rename_file        = easyvfs_rename_file__native;
    pArchive->callbacks.create_directory   = easyvfs_create_directory__native;
    pArchive->callbacks.copy_file          = easyvfs_copy_file__native;
    pArchive->callbacks.open_file          = easyvfs_open_file__native;
    pArchive->callbacks.close_file         = easyvfs_close_file__native;
    pArchive->callbacks.read_file          = easyvfs_read_file__native;
    pArchive->callbacks.write_file         = easyvfs_write_file__native;
    pArchive->callbacks.seek_file          = easyvfs_seek_file__native;
    pArchive->callbacks.tell_file          = easyvfs_tell_file__native;
    pArchive->callbacks.file_size          = easyvfs_file_size__native;
    pArchive->callbacks.flush_file         = easyvfs_flush__native;
    strcpy_s(pArchive->absolutePath, sizeof(pArchive->absolutePath), absolutePath);
    
    return pArchive;
}

// Opens an archive from a file and callbacks.
PRIVATE easyvfs_archive* easyvfs_open_non_native_archive(easyvfs_archive* pParentArchive, easyvfs_file* pArchiveFile, easyvfs_archive_callbacks* pBackEndCallbacks, const char* relativePath, unsigned int accessMode)
{
    assert(pParentArchive != NULL);
    assert(pArchiveFile != NULL);
    assert(pBackEndCallbacks != NULL);

    if (pBackEndCallbacks->open_archive == NULL) {
        return NULL;
    }

    easyvfs_handle internalArchiveHandle = pBackEndCallbacks->open_archive(pArchiveFile, accessMode);
    if (internalArchiveHandle == NULL) {
        return NULL;
    }

    easyvfs_archive* pArchive = malloc(sizeof(*pArchive));
    if (pArchive == NULL) {
        return NULL;
    }

    pArchive->pContext              = pParentArchive->pContext;
    pArchive->pParentArchive        = pParentArchive;
    pArchive->pFile                 = pArchiveFile;
    pArchive->internalArchiveHandle = internalArchiveHandle;
    pArchive->flags                 = 0;
    pArchive->callbacks             = *pBackEndCallbacks;
    easyvfs_copy_and_append_path(pArchive->absolutePath, sizeof(pArchive->absolutePath), pParentArchive->absolutePath, relativePath);

    return pArchive;
}

// Attempts to open an archive from another archive.
PRIVATE easyvfs_archive* easyvfs_open_non_native_archive_from_path(easyvfs_archive* pParentArchive, const char* relativePath, unsigned int accessMode)
{
    assert(pParentArchive != NULL);
    assert(relativePath != NULL);

    easyvfs_archive_callbacks backendCallbacks;
    if (!easyvfs_find_backend_by_extension(pParentArchive->pContext, easyvfs_extension(relativePath), &backendCallbacks)) {
        return NULL;
    }

    easyvfs_file* pArchiveFile = easyvfs_open_file_from_archive(pParentArchive, relativePath, accessMode, 0);
    if (pArchiveFile == NULL) {
        return NULL;
    }

    return easyvfs_open_non_native_archive(pParentArchive, pArchiveFile, &backendCallbacks, relativePath, accessMode);
}


// Recursively opens the archive that owns the file at the given verbose path.
PRIVATE easyvfs_archive* easyvfs_open_owner_archive_recursively_from_verbose_path(easyvfs_archive* pParentArchive, const char* relativePath, unsigned int accessMode, char* relativePathOut, size_t relativePathOutSize)
{
    assert(pParentArchive != NULL);
    assert(relativePath != NULL);

    if (pParentArchive->callbacks.get_file_info == NULL) {
        return NULL;
    }

    easyvfs_file_info fi;
    if (pParentArchive->callbacks.get_file_info(pParentArchive->internalArchiveHandle, relativePath, &fi))
    {
        // The file is in this archive.
        strcpy_s(relativePathOut, relativePathOutSize, relativePath);
        return pParentArchive;
    }
    else
    {
        char runningPath[EASYVFS_MAX_PATH];
        runningPath[0] = '\0';

        easyvfs_pathiterator segment = easyvfs_begin_path_iteration(relativePath);
        while (easyvfs_next_path_segment(&segment))
        {
            if (!easyvfs_append_path_iterator(runningPath, sizeof(runningPath), segment)) {
                return NULL;
            }

            if (pParentArchive->callbacks.get_file_info(pParentArchive->internalArchiveHandle, runningPath, &fi))
            {
                if ((fi.attributes & EASYVFS_FILE_ATTRIBUTE_DIRECTORY) == 0)
                {
                    // The running path points to an actual file. It could be a sub-archive.
                    easyvfs_archive_callbacks backendCallbacks;
                    if (easyvfs_find_backend_by_extension(pParentArchive->pContext, easyvfs_extension(runningPath), &backendCallbacks))
                    {
                        easyvfs_file* pNextArchiveFile = easyvfs_open_file_from_archive(pParentArchive, runningPath, accessMode, 0);
                        if (pNextArchiveFile == NULL) {
                            break;    // Failed to open the archive file.
                        }

                        easyvfs_archive* pNextArchive = easyvfs_open_non_native_archive(pParentArchive, pNextArchiveFile, &backendCallbacks, runningPath, accessMode);
                        if (pNextArchive == NULL) {
                            easyvfs_close(pNextArchiveFile);
                            break;
                        }

                        // At this point we should have an archive. We now need to call this function recursively if there are any segments left.
                        easyvfs_pathiterator nextsegment = segment;
                        if (easyvfs_next_path_segment(&nextsegment))
                        {
                            easyvfs_archive* pOwnerArchive = easyvfs_open_owner_archive_recursively_from_verbose_path(pNextArchive, nextsegment.path + nextsegment.segment.offset, accessMode, relativePathOut, relativePathOutSize);
                            if (pOwnerArchive == NULL) {
                                easyvfs_close_archive(pNextArchive);
                                break;
                            }

                            return pOwnerArchive;
                        }
                        else
                        {
                            // We reached the end of the path. If we get here it means the file doesn't exist, because otherwise we would have caught it in the check right at the top.
                            easyvfs_close_archive(pNextArchive);
                            break;
                        }
                    }
                }
            }
        }

        // The running path is not part of this archive.
        strcpy_s(relativePathOut, relativePathOutSize, relativePath);
        return pParentArchive;
    }
}

// Opens the archive that owns the file at the given verbose path.
PRIVATE easyvfs_archive* easyvfs_open_owner_archive_from_absolute_path(easyvfs_context* pContext, const char* absolutePath, unsigned int accessMode, char* relativePathOut, size_t relativePathOutSize)
{
    assert(pContext != NULL);
    assert(absolutePath != NULL);

    // We are currently assuming absolute path's are verbose. This means we don't need to do any searching on the file system. We just
    // move through the path and look for a segment with an extension matching one of the registered back-ends.

    char runningPath[EASYVFS_MAX_PATH];
    runningPath[0] = '\0';

    easyvfs_pathiterator segment = easyvfs_begin_path_iteration(absolutePath);
    while (easyvfs_next_path_segment(&segment))
    {
        if (!easyvfs_append_path_iterator(runningPath, sizeof(runningPath), segment)) {
            return NULL;
        }

        if (!easyvfs_is_native_directory(runningPath))
        {
            char dirAbsolutePath[EASYVFS_MAX_PATH];
            if (!easyvfs_copy_base_path(runningPath, dirAbsolutePath, sizeof(dirAbsolutePath))) {
                return NULL;
            }

            easyvfs_archive* pNativeArchive = easyvfs_open_native_archive(pContext, dirAbsolutePath, accessMode);
            if (pNativeArchive == NULL) {
                return NULL;    // Failed to open the native archive.
            }


            // The running path is not a native directory. It could be an archive file.
            easyvfs_archive_callbacks backendCallbacks;
            if (easyvfs_find_backend_by_extension(pContext, easyvfs_extension(runningPath), &backendCallbacks))
            {
                // The running path refers to an archive file. We need to try opening the archive here. If this fails, we
                // need to return NULL.
                easyvfs_archive* pArchive = easyvfs_open_owner_archive_recursively_from_verbose_path(pNativeArchive, segment.path + segment.segment.offset, accessMode, relativePathOut, relativePathOutSize);
                if (pArchive == NULL) {
                    easyvfs_close_archive(pNativeArchive);
                    return NULL;
                }

                return pArchive;
            }
            else
            {
                strcpy_s(relativePathOut, relativePathOutSize, segment.path + segment.segment.offset);
                return pNativeArchive;
            }
        }
    }

    return NULL;
}

// Recursively opens the archive that owns the file specified by the given relative path.
PRIVATE easyvfs_archive* easyvfs_open_owner_archive_recursively_from_relative_path(easyvfs_archive* pParentArchive, const char* rootSearchPath, const char* relativePath, unsigned int accessMode, char* relativePathOut, size_t relativePathOutSize)
{
    assert(pParentArchive != NULL);
    assert(relativePath != NULL);

    if (pParentArchive->callbacks.get_file_info == NULL) {
        return NULL;
    }

    // Always try the direct route by checking if the file exists within the archive first.
    easyvfs_file_info fi;
    if (pParentArchive->callbacks.get_file_info(pParentArchive->internalArchiveHandle, relativePath, &fi))
    {
        // The file is in this archive.
        strcpy_s(relativePathOut, relativePathOutSize, relativePath);
        return pParentArchive;
    }
    else
    {
        // The file does not exist within this archive. We need to search the parent archive recursively. We never search above
        // the path specified by rootSearchPath.
        char runningPath[EASYVFS_MAX_PATH];
        strcpy_s(runningPath, sizeof(runningPath), rootSearchPath);

        // Part of rootSearchPath and relativePath will be overlapping. We want to begin searching at the non-overlapping section.
        easyvfs_pathiterator pathSeg0 = easyvfs_begin_path_iteration(rootSearchPath);
        easyvfs_pathiterator pathSeg1 = easyvfs_begin_path_iteration(relativePath);
        while (easyvfs_next_path_segment(&pathSeg0) && easyvfs_next_path_segment(&pathSeg1)) {}
        relativePath = pathSeg1.path + pathSeg1.segment.offset;

        // Searching works as such:
        //   For each segment in "relativePath"
        //       If segment is archive then
        //           Open and search archive
        //       Else
        //           Search each archive file in this directory
        //       Endif
        easyvfs_pathiterator pathseg = easyvfs_begin_path_iteration(relativePath);
        while (easyvfs_next_path_segment(&pathseg))
        {
            char runningPathBase[EASYVFS_MAX_PATH];
            strcpy_s(runningPathBase, sizeof(runningPathBase), runningPath);

            if (!easyvfs_append_path_iterator(runningPath, sizeof(runningPath), pathseg)) {
                return NULL;
            }

            easyvfs_archive* pNextArchive = easyvfs_open_non_native_archive_from_path(pParentArchive, runningPath, accessMode);
            if (pNextArchive != NULL)
            {
                // It's an archive segment. We need to check this archive recursively, starting from the next segment.
                easyvfs_pathiterator nextseg = pathseg;
                if (!easyvfs_next_path_segment(&nextseg)) {
                    // Should never actually get here, but if we do it means we've reached the end of the path. Assume the file could not be found.
                    easyvfs_close_archive(pNextArchive);
                    return NULL;
                }

                easyvfs_archive* pOwnerArchive = easyvfs_open_owner_archive_recursively_from_relative_path(pNextArchive, "", nextseg.path + nextseg.segment.offset, accessMode, relativePathOut, relativePathOutSize);
                if (pOwnerArchive == NULL) {
                    easyvfs_close_archive(pNextArchive);
                    return NULL;
                }

                return pOwnerArchive;
            }
            else
            {
                // It's not an archive segment. We need to search.
                if (pParentArchive->callbacks.begin_iteration == NULL || pParentArchive->callbacks.next_iteration == NULL || pParentArchive->callbacks.end_iteration == NULL) {
                    return NULL;
                }

                easyvfs_handle iterator = pParentArchive->callbacks.begin_iteration(pParentArchive->internalArchiveHandle, runningPathBase);
                if (iterator == NULL) {
                    return NULL;
                }

                while (pParentArchive->callbacks.next_iteration(pParentArchive->internalArchiveHandle, iterator, &fi))
                {
                    if ((fi.attributes & EASYVFS_FILE_ATTRIBUTE_DIRECTORY) == 0)
                    {
                        // It's a file which means it could be an archive. Note that fi.absolutePath is actually relative to the parent archive.
                        pNextArchive = easyvfs_open_non_native_archive_from_path(pParentArchive, fi.absolutePath, accessMode);
                        if (pNextArchive != NULL)
                        {
                            // It's an archive, so check it.
                            easyvfs_archive* pOwnerArchive = easyvfs_open_owner_archive_recursively_from_relative_path(pNextArchive, "", pathseg.path + pathseg.segment.offset, accessMode, relativePathOut, relativePathOutSize);
                            if (pOwnerArchive != NULL) {
                                pParentArchive->callbacks.end_iteration(pParentArchive->internalArchiveHandle, iterator);
                                return pOwnerArchive;
                            } else {
                                easyvfs_close_archive(pNextArchive);
                            }
                        }
                    }
                }

                pParentArchive->callbacks.end_iteration(pParentArchive->internalArchiveHandle, iterator);
            }
        }
    }

    return NULL;
}

// Opens the archive that owns the file at the given path that's relative to the given base path. This supports non-verbose paths and will search
// the file system for the archive.
PRIVATE easyvfs_archive* easyvfs_open_owner_archive_from_relative_path(easyvfs_context* pContext, const char* absoluteBasePath, const char* relativePath, unsigned int accessMode, char* relativePathOut, size_t relativePathOutSize)
{
    assert(pContext != NULL);
    assert(absoluteBasePath != NULL);
    assert(relativePath != NULL);
    assert(easyvfs_is_path_absolute(absoluteBasePath));

    char adjustedRelativePath[EASYVFS_MAX_PATH];
    char relativeBasePath[EASYVFS_MAX_PATH];
    relativeBasePath[0] = '\0';

    // The base path should be absolute and verbose. It does not need to be an actual directory.
    easyvfs_archive* pBaseArchive = NULL;
    if (easyvfs_is_native_directory(absoluteBasePath))
    {
        pBaseArchive = easyvfs_open_native_archive(pContext, absoluteBasePath, accessMode);
        if (pBaseArchive == NULL) {
            return NULL;
        }

        if (strcpy_s(adjustedRelativePath, sizeof(adjustedRelativePath), relativePath) != 0) {
            easyvfs_close_archive(pBaseArchive);
            return NULL;
        }
    }
    else
    {
        pBaseArchive = easyvfs_open_owner_archive(pContext, absoluteBasePath, accessMode, relativeBasePath, sizeof(relativeBasePath));
        if (pBaseArchive == NULL) {
            return NULL;
        }

        if (!easyvfs_copy_and_append_path(adjustedRelativePath, sizeof(adjustedRelativePath), relativeBasePath, relativePath)) {
            easyvfs_close_archive(pBaseArchive);
            return NULL;
        }
    }


    // We couldn't open the archive file from here, so we'll need to search for the owner archive recursively.
    easyvfs_archive* pOwnerArchive = easyvfs_open_owner_archive_recursively_from_relative_path(pBaseArchive, relativeBasePath, adjustedRelativePath, accessMode, relativePathOut, relativePathOutSize);
    if (pOwnerArchive == NULL) {
        return NULL;
    }

    easyvfs_recursively_claim_ownership_or_parent_archive(pOwnerArchive);
    return pOwnerArchive;
}

// Opens an archive from a path that's relative to the given base path. This supports non-verbose paths and will search
// the file system for the archive.
PRIVATE easyvfs_archive* easyvfs_open_archive_from_relative_path(easyvfs_context* pContext, const char* absoluteBasePath, const char* relativePath, unsigned int accessMode)
{
    assert(pContext != NULL);
    assert(absoluteBasePath != NULL);
    assert(relativePath != NULL);
    assert(easyvfs_is_path_absolute(absoluteBasePath));

    char adjustedRelativePath[EASYVFS_MAX_PATH];
    char relativeBasePath[EASYVFS_MAX_PATH];
    relativeBasePath[0] = '\0';

    // The base path should be absolute and verbose. It does not need to be an actual directory.
    easyvfs_archive* pBaseArchive = NULL;
    if (easyvfs_is_native_directory(absoluteBasePath))
    {
        pBaseArchive = easyvfs_open_native_archive(pContext, absoluteBasePath, accessMode);
        if (pBaseArchive == NULL) {
            return NULL;
        }

        if (strcpy_s(adjustedRelativePath, sizeof(adjustedRelativePath), relativePath) != 0) {
            easyvfs_close_archive(pBaseArchive);
            return NULL;
        }
    }
    else
    {
        pBaseArchive = easyvfs_open_owner_archive(pContext, absoluteBasePath, accessMode, relativeBasePath, sizeof(relativeBasePath));
        if (pBaseArchive == NULL) {
            return NULL;
        }

        if (!easyvfs_copy_and_append_path(adjustedRelativePath, sizeof(adjustedRelativePath), relativeBasePath, relativePath)) {
            easyvfs_close_archive(pBaseArchive);
            return NULL;
        }
    }


    // We can try opening the file directly from the base path. If this doesn't work, we can try recursively opening the owner archive
    // an open from there.
    easyvfs_archive* pArchive = easyvfs_open_non_native_archive_from_path(pBaseArchive, adjustedRelativePath, accessMode);
    if (pArchive == NULL)
    {
        // We couldn't open the archive file from here, so we'll need to search for the owner archive recursively.
        char archiveRelativePath[EASYVFS_MAX_PATH];
        easyvfs_archive* pOwnerArchive = easyvfs_open_owner_archive_recursively_from_relative_path(pBaseArchive, relativeBasePath, adjustedRelativePath, accessMode, archiveRelativePath, sizeof(archiveRelativePath));
        if (pOwnerArchive != NULL)
        {
            easyvfs_recursively_claim_ownership_or_parent_archive(pOwnerArchive);

            pArchive = easyvfs_open_non_native_archive_from_path(pOwnerArchive, archiveRelativePath, accessMode);
            if (pArchive == NULL) {
                easyvfs_close_archive(pOwnerArchive);
                return NULL;
            }
        }
    }

    if (pArchive == NULL) {
        easyvfs_close_archive(pBaseArchive);
    }

    return pArchive;
}

//// Back-End Registration ////

#ifndef EASY_VFS_NO_ZIP
// Registers the archive callbacks which enables support for Zip files.
PRIVATE void easyvfs_register_zip_backend(easyvfs_context* pContext);
#endif

#ifndef EASY_VFS_NO_PAK
// Registers the archive callbacks which enables support for Quake 2 pak files.
PRIVATE void easyvfs_register_pak_backend(easyvfs_context* pContext);
#endif

#ifndef EASY_VFS_NO_MTL
// Registers the archive callbacks which enables support for Quake 2 pak files.
PRIVATE void easyvfs_register_mtl_backend(easyvfs_context* pContext);
#endif



//// Public API Implementation ////

easyvfs_context* easyvfs_create_context()
{
    easyvfs_context* pContext = malloc(sizeof(*pContext));
    if (pContext == NULL) {
        return NULL;
    }

    if (!easyvfs_callbacklist_init(&pContext->archiveCallbacks) || !easyvfs_basedirs_init(&pContext->baseDirectories)) {
        free(pContext);
        return NULL;
    }

    memset(pContext->writeBaseDirectory, 0, EASYVFS_MAX_PATH);
    pContext->isWriteGuardEnabled = 0;

#ifndef EASY_VFS_NO_ZIP
    easyvfs_register_zip_backend(pContext);
#endif

#ifndef EASY_VFS_NO_PAK
    easyvfs_register_pak_backend(pContext);
#endif

#ifndef EASY_VFS_NO_MTL
    easyvfs_register_mtl_backend(pContext);
#endif

    return pContext;
}

void easyvfs_delete_context(easyvfs_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    easyvfs_basedirs_uninit(&pContext->baseDirectories);
    easyvfs_callbacklist_uninit(&pContext->archiveCallbacks);
    free(pContext);
}


void easyvfs_register_archive_backend(easyvfs_context* pContext, easyvfs_archive_callbacks callbacks)
{
    if (pContext == NULL) {
        return;
    }

    easyvfs_callbacklist_pushback(&pContext->archiveCallbacks, callbacks);
}


void easyvfs_insert_base_directory(easyvfs_context* pContext, const char* absolutePath, unsigned int index)
{
    if (pContext == NULL) {
        return;
    }

    easyvfs_basedirs_insert(&pContext->baseDirectories, absolutePath, index);
}

void easyvfs_add_base_directory(easyvfs_context* pContext, const char* absolutePath)
{
    if (pContext == NULL) {
        return;
    }

    easyvfs_insert_base_directory(pContext, absolutePath, easyvfs_get_base_directory_count(pContext));
}

void easyvfs_remove_base_directory(easyvfs_context* pContext, const char* absolutePath)
{
    if (pContext == NULL) {
        return;
    }

    for (unsigned int iPath = 0; iPath < pContext->baseDirectories.count; /*DO NOTHING*/) {
        if (easyvfs_paths_equal(pContext->baseDirectories.pBuffer[iPath].absolutePath, absolutePath)) {
            easyvfs_basedirs_remove(&pContext->baseDirectories, iPath);
        } else {
            ++iPath;
        }
    }
}

void easyvfs_remove_base_directory_by_index(easyvfs_context* pContext, unsigned int index)
{
    if (pContext == NULL) {
        return;
    }

    easyvfs_basedirs_remove(&pContext->baseDirectories, index);
}

void easyvfs_remove_all_base_directories(easyvfs_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    easyvfs_basedirs_clear(&pContext->baseDirectories);
}

unsigned int easyvfs_get_base_directory_count(easyvfs_context* pContext)
{
    if (pContext == NULL) {
        return 0;
    }

    return pContext->baseDirectories.count;
}

const char* easyvfs_get_base_directory_by_index(easyvfs_context* pContext, unsigned int index)
{
    if (pContext == NULL || index >= pContext->baseDirectories.count) {
        return NULL;
    }

    return pContext->baseDirectories.pBuffer[index].absolutePath;
}


void easyvfs_set_base_write_directory(easyvfs_context* pContext, const char* absolutePath)
{
    if (pContext == NULL) {
        return;
    }

    if (absolutePath == NULL) {
        memset(pContext->writeBaseDirectory, 0, sizeof(pContext->writeBaseDirectory));
    } else {
        strcpy_s(pContext->writeBaseDirectory, sizeof(pContext->writeBaseDirectory), absolutePath);
    }
}

bool easyvfs_get_base_write_directory(easyvfs_context* pContext, char* absolutePathOut, unsigned int absolutePathOutSize)
{
    if (pContext == NULL || absolutePathOut == NULL || absolutePathOutSize == 0) {
        return false;
    }

    return strcpy_s(absolutePathOut, absolutePathOutSize, pContext->writeBaseDirectory) == 0;
}

void easyvfs_enable_write_directory_guard(easyvfs_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    pContext->isWriteGuardEnabled = true;
}

void easyvfs_disable_write_directory_guard(easyvfs_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    pContext->isWriteGuardEnabled = false;
}

bool easyvfs_is_write_directory_guard_enabled(easyvfs_context* pContext)
{
    if (pContext == NULL) {
        return false;
    }

    return pContext->isWriteGuardEnabled;
}




easyvfs_archive* easyvfs_open_archive(easyvfs_context* pContext, const char* absoluteOrRelativePath, unsigned int accessMode)
{
    if (pContext == NULL || absoluteOrRelativePath == NULL) {
        return NULL;
    }

    if (easyvfs_is_path_absolute(absoluteOrRelativePath))
    {
        if (easyvfs_is_native_directory(absoluteOrRelativePath))
        {
            // It's a native directory.
            return easyvfs_open_native_archive(pContext, absoluteOrRelativePath, accessMode);
        }
        else
        {
            // It's not a native directory. We can just use easyvfs_open_owner_archive() in this case.
            char relativePath[EASYVFS_MAX_PATH];
            easyvfs_archive* pOwnerArchive = easyvfs_open_owner_archive(pContext, absoluteOrRelativePath, accessMode, relativePath, sizeof(relativePath));
            if (pOwnerArchive == NULL) {
                return NULL;
            }

            easyvfs_archive* pArchive = easyvfs_open_non_native_archive_from_path(pOwnerArchive, relativePath, accessMode);
            if (pArchive != NULL) {
                easyvfs_recursively_claim_ownership_or_parent_archive(pArchive);
            }

            return pArchive;
        }
    }
    else
    {
        // The input path is not absolute. We need to check each base directory.

        for (unsigned int iBaseDir = 0; iBaseDir < easyvfs_get_base_directory_count(pContext); ++iBaseDir)
        {
            easyvfs_archive* pArchive = easyvfs_open_archive_from_relative_path(pContext, easyvfs_get_base_directory_by_index(pContext, iBaseDir), absoluteOrRelativePath, accessMode);
            if (pArchive != NULL) {
                easyvfs_recursively_claim_ownership_or_parent_archive(pArchive);
                return pArchive;
            }
        }
    }

    return NULL;
}

easyvfs_archive* easyvfs_open_owner_archive(easyvfs_context* pContext, const char* absoluteOrRelativePath, unsigned int accessMode, char* relativePathOut, size_t relativePathOutSize)
{
    if (pContext == NULL || absoluteOrRelativePath == NULL) {
        return NULL;
    }

    if (easyvfs_is_path_absolute(absoluteOrRelativePath))
    {
        if (easyvfs_is_native_file(absoluteOrRelativePath) || easyvfs_is_native_directory(absoluteOrRelativePath))
        {
            // It's a native file. The owner archive is the directory it's directly sitting in.
            char dirAbsolutePath[EASYVFS_MAX_PATH];
            if (!easyvfs_copy_base_path(absoluteOrRelativePath, dirAbsolutePath, sizeof(dirAbsolutePath))) {
                return NULL;
            }

            easyvfs_archive* pArchive = easyvfs_open_archive(pContext, dirAbsolutePath, accessMode);
            if (pArchive == NULL) {
                return NULL;
            }

            if (relativePathOut) {
                if (strcpy_s(relativePathOut, relativePathOutSize, easyvfs_file_name(absoluteOrRelativePath)) != 0) {
                    easyvfs_close_archive(pArchive);
                    return NULL;
                }
            }

            return pArchive;
        }
        else
        {
            // It's not a native file or directory.
            easyvfs_archive* pArchive = easyvfs_open_owner_archive_from_absolute_path(pContext, absoluteOrRelativePath, accessMode, relativePathOut, relativePathOutSize);
            if (pArchive != NULL) {
                easyvfs_recursively_claim_ownership_or_parent_archive(pArchive);
                return pArchive;
            }
        }
    }
    else
    {
        // The input path is not absolute. We need to loop through each base directory.

        for (unsigned int iBaseDir = 0; iBaseDir < easyvfs_get_base_directory_count(pContext); ++iBaseDir)
        {
            easyvfs_archive* pArchive = easyvfs_open_owner_archive_from_relative_path(pContext, easyvfs_get_base_directory_by_index(pContext, iBaseDir), absoluteOrRelativePath, accessMode, relativePathOut, relativePathOutSize);
            if (pArchive != NULL) {
                easyvfs_recursively_claim_ownership_or_parent_archive(pArchive);
                return pArchive;
            }
        }
    }

    return NULL;
}

void easyvfs_close_archive(easyvfs_archive* pArchive)
{
    if (pArchive == NULL) {
        return;
    }

    // The internal handle needs to be closed.
    if (pArchive->callbacks.close_archive) {
        pArchive->callbacks.close_archive(pArchive->internalArchiveHandle);
    }

    easyvfs_close(pArchive->pFile);


    if ((pArchive->flags & EASY_VFS_OWNS_PARENT_ARCHIVE) != 0) {
        easyvfs_close_archive(pArchive->pParentArchive);
    }

    free(pArchive);
}

easyvfs_file* easyvfs_open_file_from_archive(easyvfs_archive* pArchive, const char* relativePath, unsigned int accessMode, size_t extraDataSize)
{
    if (pArchive == NULL || relativePath == NULL || pArchive->callbacks.open_file == NULL) {
        return NULL;
    }

    easyvfs_handle internalFileHandle = pArchive->callbacks.open_file(pArchive->internalArchiveHandle, relativePath, accessMode);
    if (internalFileHandle == NULL) {
        return NULL;
    }

    // At this point the file is opened and we can create the file object.
    easyvfs_file* pFile = malloc(sizeof(*pFile) - sizeof(pFile->pExtraData) + extraDataSize);
    if (pFile == NULL) {
        return NULL;
    }

    pFile->pArchive           = pArchive;
    pFile->internalFileHandle = internalFileHandle;
    pFile->flags              = 0;
    pFile->extraDataSize      = extraDataSize;

    return pFile;
}


easyvfs_file* easyvfs_open(easyvfs_context* pContext, const char* absoluteOrRelativePath, unsigned int accessMode, size_t extraDataSize)
{
    if (pContext == NULL || absoluteOrRelativePath == NULL) {
        return NULL;
    }

    char absolutePathForWriteMode[EASYVFS_MAX_PATH];
    if ((accessMode & EASYVFS_WRITE) != 0) {
        if (easyvfs_validate_write_path(pContext, absoluteOrRelativePath, absolutePathForWriteMode, sizeof(absolutePathForWriteMode))) {
            absoluteOrRelativePath = absolutePathForWriteMode;
        } else {
            return NULL;
        }
    }

    char relativePath[EASYVFS_MAX_PATH];
    easyvfs_archive* pArchive = easyvfs_open_owner_archive(pContext, absoluteOrRelativePath, easyvfs_archive_access_mode(accessMode), relativePath, sizeof(relativePath));
    if (pArchive == NULL) {
        return NULL;
    }

    easyvfs_file* pFile = easyvfs_open_file_from_archive(pArchive, relativePath, accessMode, extraDataSize);
    if (pFile == NULL) {
        return NULL;
    }

    // When using this API, we want to claim ownership of the archive so that it's closed when we close this file.
    pFile->flags |= EASY_VFS_OWNS_PARENT_ARCHIVE;

    return pFile;
}

void easyvfs_close(easyvfs_file* pFile)
{
    if (pFile == NULL) {
        return;
    }

    if (pFile->pArchive != NULL && pFile->pArchive->callbacks.close_file) {
        pFile->pArchive->callbacks.close_file(pFile->pArchive->internalArchiveHandle, pFile->internalFileHandle);
    }


    if ((pFile->flags & EASY_VFS_OWNS_PARENT_ARCHIVE) != 0) {
        easyvfs_close_archive(pFile->pArchive);
    }

    free(pFile);
}

bool easyvfs_read(easyvfs_file* pFile, void* pDataOut, size_t bytesToRead, size_t* pBytesReadOut)
{
    if (pFile == NULL || pDataOut == NULL || pFile->pArchive == NULL || pFile->pArchive->callbacks.read_file == NULL) {
        return false;
    }

    return pFile->pArchive->callbacks.read_file(pFile->pArchive->internalArchiveHandle, pFile->internalFileHandle, pDataOut, bytesToRead, pBytesReadOut);
}

bool easyvfs_write(easyvfs_file* pFile, const void* pData, size_t bytesToWrite, size_t* pBytesWrittenOut)
{
    if (pFile == NULL || pData == NULL || pFile->pArchive == NULL || pFile->pArchive->callbacks.write_file == NULL) {
        return false;
    }

    return pFile->pArchive->callbacks.write_file(pFile->pArchive->internalArchiveHandle, pFile->internalFileHandle, pData, bytesToWrite, pBytesWrittenOut);
}

bool easyvfs_seek(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
{
    if (pFile == NULL || pFile->pArchive == NULL || pFile->pArchive->callbacks.seek_file == NULL) {
        return false;
    }

    return pFile->pArchive->callbacks.seek_file(pFile->pArchive->internalArchiveHandle, pFile->internalFileHandle, bytesToSeek, origin);
}

easyvfs_uint64 easyvfs_tell(easyvfs_file* pFile)
{
    if (pFile == NULL || pFile->pArchive == NULL || pFile->pArchive->callbacks.tell_file == NULL) {
        return false;
    }

    return pFile->pArchive->callbacks.tell_file(pFile->pArchive->internalArchiveHandle, pFile->internalFileHandle);
}

easyvfs_uint64 easyvfs_size(easyvfs_file* pFile)
{
    if (pFile == NULL || pFile->pArchive == NULL || pFile->pArchive->callbacks.file_size == NULL) {
        return 0;
    }

    return pFile->pArchive->callbacks.file_size(pFile->pArchive->internalArchiveHandle, pFile->internalFileHandle);
}

void easyvfs_flush(easyvfs_file* pFile)
{
    if (pFile == NULL || pFile->pArchive == NULL || pFile->pArchive->callbacks.flush_file == NULL) {
        return;
    }

    pFile->pArchive->callbacks.flush_file(pFile->pArchive->internalArchiveHandle, pFile->internalFileHandle);
}

size_t easyvfs_get_extra_data_size(easyvfs_file* pFile)
{
    if (pFile == NULL) {
        return 0;
    }

    return pFile->extraDataSize;
}

void* easyvfs_get_extra_data(easyvfs_file* pFile)
{
    if (pFile == NULL) {
        return NULL;
    }

    return pFile->pExtraData;
}


bool easyvfs_get_file_info(easyvfs_context* pContext, const char* absoluteOrRelativePath, easyvfs_file_info* fi)
{
    if (pContext == NULL || absoluteOrRelativePath == NULL) {
        return false;
    }
    
    char relativePath[EASYVFS_MAX_PATH];
    easyvfs_archive* pOwnerArchive = easyvfs_open_owner_archive(pContext, absoluteOrRelativePath, EASYVFS_READ, relativePath, sizeof(relativePath));
    if (pOwnerArchive == NULL) {
        return false;
    }

    bool result = false;
    if (pOwnerArchive->callbacks.get_file_info) {
        result = pOwnerArchive->callbacks.get_file_info(pOwnerArchive->internalArchiveHandle, relativePath, fi);
    }

    if (result && fi != NULL) {
        easyvfs_copy_and_append_path(fi->absolutePath, sizeof(fi->absolutePath), pOwnerArchive->absolutePath, relativePath);
    }

    easyvfs_close_archive(pOwnerArchive);
    return result;
}


bool easyvfs_begin(easyvfs_context* pContext, const char* absoluteOrRelativePath, easyvfs_iterator* pIteratorOut)
{
    if (pIteratorOut == NULL) {
        return false;
    }

    memset(pIteratorOut, 0, sizeof(*pIteratorOut));

    if (pContext == NULL || absoluteOrRelativePath == NULL || pIteratorOut == NULL) {
        return false;
    }

    // We need to open the archive that the given folder is in. The path could, however, point to an actual archive which is allowed
    // in which case we just iterate over the root directory within that archive. What we do is first try using the path as an actual
    // archive. If it fails we assume the path is to a folder within an archive (such as a zip file) in which case we just try opening
    // the owner archive. If both fail, we return false.

    char relativePath[EASYVFS_MAX_PATH];
    pIteratorOut->pArchive = easyvfs_open_archive(pContext, absoluteOrRelativePath, EASYVFS_READ);
    if (pIteratorOut->pArchive != NULL) {
        relativePath[0] = '\0';
    } else {
        pIteratorOut->pArchive = easyvfs_open_owner_archive(pContext, absoluteOrRelativePath, EASYVFS_READ, relativePath, sizeof(relativePath));
    }

    if (pIteratorOut->pArchive == NULL) {
        return false;
    }


    if (pIteratorOut->pArchive->callbacks.begin_iteration) {
        pIteratorOut->internalIteratorHandle = pIteratorOut->pArchive->callbacks.begin_iteration(pIteratorOut->pArchive->internalArchiveHandle, relativePath);
    }

    if (pIteratorOut->internalIteratorHandle == NULL) {
        easyvfs_close_archive(pIteratorOut->pArchive);
        pIteratorOut->pArchive = NULL;
        return false;
    }


    bool foundFirst = easyvfs_next(pContext, pIteratorOut);
    if (!foundFirst) {
        easyvfs_end(pContext, pIteratorOut);
    }

    return foundFirst;
}

bool easyvfs_next(easyvfs_context* pContext, easyvfs_iterator* pIterator)
{
    if (pIterator == NULL) {
        return false;
    }

    memset(&pIterator->info, 0, sizeof(pIterator->info));

    if (pContext == NULL || pIterator->pArchive == NULL) {
        return false;
    }

    bool result = false;
    if (pIterator->pArchive->callbacks.next_iteration) {
        result = pIterator->pArchive->callbacks.next_iteration(pIterator->pArchive->internalArchiveHandle, pIterator->internalIteratorHandle, &pIterator->info);
    }

    // At this point the pIterator->info.absolutePath is actually referring to a relative path. We need to convert it to an absolute path.
    if (result) {
        char relativePath[EASYVFS_MAX_PATH];
        strcpy_s(relativePath, sizeof(relativePath), pIterator->info.absolutePath);
        easyvfs_copy_and_append_path(pIterator->info.absolutePath, sizeof(pIterator->info.absolutePath), pIterator->pArchive->absolutePath, relativePath);
    }

    return result;
}

void easyvfs_end(easyvfs_context* pContext, easyvfs_iterator* pIterator)
{
    if (pContext == NULL || pIterator == NULL) {
        return;
    }

    if (pIterator->pArchive != NULL && pIterator->pArchive->callbacks.end_iteration) {
        pIterator->pArchive->callbacks.end_iteration(pIterator->pArchive->internalArchiveHandle, pIterator->internalIteratorHandle);
    }

    easyvfs_close_archive(pIterator->pArchive);
    memset(pIterator, 0, sizeof(*pIterator));
}

bool easyvfs_delete_file(easyvfs_context* pContext, const char* path)
{
    if (pContext == NULL || path == NULL) {
        return false;
    }

    char absolutePath[EASYVFS_MAX_PATH];
    if (!easyvfs_validate_write_path(pContext, path, absolutePath, sizeof(absolutePath))) {
        return false;
    }


    char relativePath[EASYVFS_MAX_PATH];
    easyvfs_archive* pArchive = easyvfs_open_owner_archive(pContext, absolutePath, easyvfs_archive_access_mode(EASYVFS_READ | EASYVFS_WRITE), relativePath, sizeof(relativePath));
    if (pArchive == NULL) {
        return false;
    }

    bool result = false;
    if (pArchive->callbacks.delete_file) {
        result = pArchive->callbacks.delete_file(pArchive->internalArchiveHandle, relativePath);
    }

    easyvfs_close_archive(pArchive);
    return result;
}

bool easyvfs_rename_file(easyvfs_context* pContext, const char* pathOld, const char* pathNew)
{
    // Renaming/moving is not supported across different archives.

    if (pContext == NULL || pathOld == NULL || pathNew == NULL) {
        return false;
    }

    bool result = false;

    char absolutePathOld[EASYVFS_MAX_PATH];
    if (easyvfs_validate_write_path(pContext, pathOld, absolutePathOld, sizeof(absolutePathOld))) {
        pathOld = absolutePathOld;
    } else {
        return 0;
    }

    char absolutePathNew[EASYVFS_MAX_PATH];
    if (easyvfs_validate_write_path(pContext, pathNew, absolutePathNew, sizeof(absolutePathNew))) {
        pathNew = absolutePathNew;
    } else {
        return 0;
    }


    char relativePathOld[EASYVFS_MAX_PATH];
    easyvfs_archive* pArchiveOld = easyvfs_open_owner_archive(pContext, pathOld, easyvfs_archive_access_mode(EASYVFS_READ | EASYVFS_WRITE), relativePathOld, sizeof(relativePathOld));
    if (pArchiveOld != NULL)
    {
        char relativePathNew[EASYVFS_MAX_PATH];
        easyvfs_archive* pArchiveNew = easyvfs_open_owner_archive(pContext, pathNew, easyvfs_archive_access_mode(EASYVFS_READ | EASYVFS_WRITE), relativePathNew, sizeof(relativePathNew));
        if (pArchiveNew != NULL)
        {
            if (easyvfs_paths_equal(pArchiveOld->absolutePath, pArchiveNew->absolutePath) && pArchiveOld->callbacks.rename_file) {
                result = pArchiveOld->callbacks.rename_file(pArchiveOld, relativePathOld, relativePathNew);
            }

            easyvfs_close_archive(pArchiveNew);
        }

        easyvfs_close_archive(pArchiveOld);
    }

    return result;
}

bool easyvfs_create_directory(easyvfs_context* pContext, const char* path)
{
    if (pContext == NULL || path == NULL) {
        return false;
    }

    char absolutePath[EASYVFS_MAX_PATH];
    if (!easyvfs_validate_write_path(pContext, path, absolutePath, sizeof(absolutePath))) {
        return false;
    }

    char relativePath[EASYVFS_MAX_PATH];
    easyvfs_archive* pArchive = easyvfs_open_owner_archive(pContext, absolutePath, easyvfs_archive_access_mode(EASYVFS_READ | EASYVFS_WRITE), relativePath, sizeof(relativePath));
    if (pArchive == NULL) {
        return false;
    }

    bool result = false;
    if (pArchive->callbacks.create_directory) {
        result = pArchive->callbacks.create_directory(pArchive->internalArchiveHandle, relativePath);
    }

    easyvfs_close_archive(pArchive);
    return result;
}

bool easyvfs_copy_file(easyvfs_context* pContext, const char* srcPath, const char* dstPath, bool failIfExists)
{
    if (pContext == NULL || srcPath == NULL || dstPath == NULL) {
        return false;
    }

    char dstPathAbsolute[EASYVFS_MAX_PATH];
    if (!easyvfs_validate_write_path(pContext, dstPath, dstPathAbsolute, sizeof(dstPathAbsolute))) {
        return false;
    }


    // We want to open the archive of both the source and destination. If they are the same archive we'll do an intra-archive copy.
    char srcRelativePath[EASYVFS_MAX_PATH];
    easyvfs_archive* pSrcArchive = easyvfs_open_owner_archive(pContext, srcPath, easyvfs_archive_access_mode(EASYVFS_READ), srcRelativePath, sizeof(srcRelativePath));
    if (pSrcArchive == NULL) {
        return false;
    }

    char dstRelativePath[EASYVFS_MAX_PATH];
    easyvfs_archive* pDstArchive = easyvfs_open_owner_archive(pContext, dstPathAbsolute, easyvfs_archive_access_mode(EASYVFS_READ | EASYVFS_WRITE), dstRelativePath, sizeof(dstRelativePath));
    if (pDstArchive == NULL) {
        easyvfs_close_archive(pSrcArchive);
        return false;
    }


        
    bool result = false;
    if (strcmp(pSrcArchive->absolutePath, pDstArchive->absolutePath) == 0 && pDstArchive->callbacks.copy_file)
    {
        // Intra-archive copy.
        result = pDstArchive->callbacks.copy_file(pDstArchive, srcRelativePath, dstRelativePath, failIfExists);
    }
    else
    {
        bool areBothArchivesNative = (pSrcArchive->callbacks.copy_file == pDstArchive->callbacks.copy_file && pDstArchive->callbacks.copy_file == easyvfs_copy_file__native);
        if (areBothArchivesNative)
        {
            char srcPathAbsolute[EASYVFS_MAX_PATH];
            easyvfs_copy_and_append_path(srcPathAbsolute, sizeof(srcPathAbsolute), pSrcArchive->absolutePath, srcPath);

            result = easyvfs_copy_native_file(srcPathAbsolute, dstPathAbsolute, failIfExists);
        }
        else
        {
            // Inter-archive copy. This is a theoretically slower path because we do everything manually. Probably not much slower in practice, though.
            if (failIfExists && pDstArchive->callbacks.get_file_info(pDstArchive, dstRelativePath, NULL) == true)
            {
                result = false;
            }
            else
            {
                easyvfs_file* pSrcFile = easyvfs_open_file_from_archive(pSrcArchive, srcRelativePath, EASYVFS_READ, 0);
                easyvfs_file* pDstFile = easyvfs_open_file_from_archive(pDstArchive, dstRelativePath, EASYVFS_WRITE | EASYVFS_TRUNCATE, 0);
                if (pSrcFile != NULL && pDstFile != NULL)
                {
                    char chunk[4096];
                    size_t bytesRead;
                    while (easyvfs_read(pSrcFile, chunk, sizeof(chunk), &bytesRead))
                    {
                        easyvfs_write(pDstFile, chunk, bytesRead, NULL);
                    }

                    result = true;
                }
                else
                {
                    result = false;
                }

                easyvfs_close(pSrcFile);
                easyvfs_close(pDstFile);
            }
        }
    }


    easyvfs_close_archive(pSrcArchive);
    easyvfs_close_archive(pDstArchive);

    return result;
}


bool easyvfs_is_archive_path(easyvfs_context* pContext, const char* path)
{
    return easyvfs_find_backend_by_extension(pContext, easyvfs_extension(path), NULL);
}



///////////////////////////////////////////////////////////////////////////////
//
// High Level API
//
///////////////////////////////////////////////////////////////////////////////

void easyvfs_free(void* p)
{
    free(p);
}

bool easyvfs_find_absolute_path(easyvfs_context* pContext, const char* relativePath, char* absolutePathOut, unsigned int absolutePathOutSize)
{
    if (pContext == NULL || relativePath == NULL || absolutePathOut == NULL || absolutePathOutSize == 0) {
        return false;
    }

    easyvfs_file_info fi;
    if (easyvfs_get_file_info(pContext, relativePath, &fi)) {
        return strcpy_s(absolutePathOut, absolutePathOutSize, fi.absolutePath) == 0;
    }

    return false;
}

bool easyvfs_find_absolute_path_explicit_base(easyvfs_context* pContext, const char* relativePath, const char* highestPriorityBasePath, char* absolutePathOut, unsigned int absolutePathOutSize)
{
    if (pContext == NULL || relativePath == NULL || highestPriorityBasePath == NULL || absolutePathOut == NULL || absolutePathOutSize == 0) {
        return false;
    }

    bool result = 0;
    easyvfs_insert_base_directory(pContext, highestPriorityBasePath, 0);
    {
        result = easyvfs_find_absolute_path(pContext, relativePath, absolutePathOut, absolutePathOutSize);
    }
    easyvfs_remove_base_directory_by_index(pContext, 0);

    return result;
}

bool easyvfs_is_base_directory(easyvfs_context* pContext, const char* baseDir)
{
    if (pContext == NULL) {
        return false;
    }

    for (unsigned int i = 0; i < easyvfs_get_base_directory_count(pContext); ++i) {
        if (easyvfs_paths_equal(easyvfs_get_base_directory_by_index(pContext, i), baseDir)) {
            return true;
        }
    }

    return false;
}

bool easyvfs_write_string(easyvfs_file* pFile, const char* str)
{
    return easyvfs_write(pFile, str, (unsigned int)strlen(str), NULL);
}

bool easyvfs_write_line(easyvfs_file* pFile, const char* str)
{
    return easyvfs_write_string(pFile, str) && easyvfs_write_string(pFile, "\n");
}


void* easyvfs_open_and_read_binary_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, size_t* pSizeInBytesOut)
{
    easyvfs_file* pFile = easyvfs_open(pContext, absoluteOrRelativePath, EASYVFS_READ, 0);
    if (pFile == NULL) {
        return NULL;
    }

    easyvfs_uint64 fileSize = easyvfs_size(pFile);
    if (fileSize > SIZE_MAX)
    {
        // File's too big.
        easyvfs_close(pFile);
        return NULL;
    }


    void* pData = malloc((size_t)fileSize);
    if (pData == NULL)
    {
        // Failed to allocate memory.
        easyvfs_close(pFile);
        return NULL;
    }

    
    if (!easyvfs_read(pFile, pData, (size_t)fileSize, NULL))
    {
        free(pData);
        if (pSizeInBytesOut != NULL) {
            *pSizeInBytesOut = 0;
        }

        easyvfs_close(pFile);
        return NULL;
    }


    if (pSizeInBytesOut != NULL) {
        *pSizeInBytesOut = (size_t)fileSize;
    }

    easyvfs_close(pFile);
    return pData;
}

char* easyvfs_open_and_read_text_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, size_t* pSizeInBytesOut)
{
    easyvfs_file* pFile = easyvfs_open(pContext, absoluteOrRelativePath, EASYVFS_READ, 0);
    if (pFile == NULL) {
        return NULL;
    }

    easyvfs_uint64 fileSize = easyvfs_size(pFile);
    if (fileSize > SIZE_MAX)
    {
        // File's too big.
        easyvfs_close(pFile);
        return NULL;
    }


    void* pData = malloc((size_t)fileSize + 1);     // +1 for null terminator.
    if (pData == NULL)
    {
        // Failed to allocate memory.
        easyvfs_close(pFile);
        return NULL;
    }

    
    if (!easyvfs_read(pFile, pData, (size_t)fileSize, NULL))
    {
        free(pData);
        if (pSizeInBytesOut != NULL) {
            *pSizeInBytesOut = 0;
        }

        easyvfs_close(pFile);
        return NULL;
    }

    // Null terminator.
    ((char*)pData)[fileSize] = '\0';


    if (pSizeInBytesOut != NULL) {
        *pSizeInBytesOut = (size_t)fileSize;
    }

    easyvfs_close(pFile);
    return pData;
}

bool easyvfs_open_and_write_binary_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, const void* pData, size_t dataSize)
{
    easyvfs_file* pFile = easyvfs_open(pContext, absoluteOrRelativePath, EASYVFS_WRITE | EASYVFS_TRUNCATE, 0);
    if (pFile == NULL) {
        return false;
    }

    bool result = easyvfs_write(pFile, pData, dataSize, NULL);

    easyvfs_close(pFile);
    return result;
}

bool easyvfs_open_and_write_text_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, const char* pTextData)
{
    return easyvfs_open_and_write_binary_file(pContext, absoluteOrRelativePath, pTextData, strlen(pTextData));
}


bool easyvfs_exists(easyvfs_context* pContext, const char* absoluteOrRelativePath)
{
    easyvfs_file_info fi;
    return easyvfs_get_file_info(pContext, absoluteOrRelativePath, &fi);
}

bool easyvfs_is_existing_file(easyvfs_context* pContext, const char* absoluteOrRelativePath)
{
    easyvfs_file_info fi;
    if (easyvfs_get_file_info(pContext, absoluteOrRelativePath, &fi))
    {
        return (fi.attributes & EASYVFS_FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    return false;
}

bool easyvfs_is_existing_directory(easyvfs_context* pContext, const char* absoluteOrRelativePath)
{
    easyvfs_file_info fi;
    if (easyvfs_get_file_info(pContext, absoluteOrRelativePath, &fi))
    {
        return (fi.attributes & EASYVFS_FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    return false;
}

bool easyvfs_create_directory_recursive(easyvfs_context* pContext, const char* path)
{
    if (pContext == NULL || path == NULL) {
        return false;
    }

    // We just iterate over each segment and try creating each directory if it doesn't exist.
    bool result = false;

    char absolutePath[EASYVFS_MAX_PATH];
    if (easyvfs_validate_write_path(pContext, path, absolutePath, EASYVFS_MAX_PATH)) {
        path = absolutePath;
    } else {
        return false;
    }


    char runningPath[EASYVFS_MAX_PATH];
    runningPath[0] = '\0';

    easyvfs_pathiterator iPathSeg = easyvfs_begin_path_iteration(absolutePath);

    // Never check the first segment because we can assume it always exists - it will always be the drive root.
    if (easyvfs_next_path_segment(&iPathSeg) && easyvfs_append_path_iterator(runningPath, sizeof(runningPath), iPathSeg))
    {
        // Loop over every directory until we find one that does not exist.
        while (easyvfs_next_path_segment(&iPathSeg))
        {
            if (!easyvfs_append_path_iterator(runningPath, sizeof(runningPath), iPathSeg)) {
                return false;
            }

            if (!easyvfs_is_existing_directory(pContext, runningPath)) {
                if (!easyvfs_create_directory(pContext, runningPath)) {
                    return false;
                }

                break;
            }
        }


        // At this point all we need to do is create the remaining directories - we can assert that the directory does not exist
        // rather than actually checking it which should be a bit more efficient.
        while (easyvfs_next_path_segment(&iPathSeg))
        {
            if (!easyvfs_append_path_iterator(runningPath, sizeof(runningPath), iPathSeg)) {
                return false;
            }

            assert(!easyvfs_is_existing_directory(pContext, runningPath));

            if (!easyvfs_create_directory(pContext, runningPath)) {
                return false;
            }
        }


        result = true;
    }

    return result;
}

bool easyvfs_eof(easyvfs_file* pFile)
{
    return easyvfs_tell(pFile) == easyvfs_size(pFile);
}




///////////////////////////////////////////////////////////////////////////////
//
// ZIP
//
///////////////////////////////////////////////////////////////////////////////
#ifndef EASY_VFS_NO_ZIP
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

typedef struct
{
    // The current index of the iterator. When this hits the file count, the iteration is finished.
    unsigned int index;

    // The directory being iterated.
    char directoryPath[EASYVFS_MAX_PATH];

}easyvfs_iterator_zip;

typedef struct
{
    // The file index within the archive.
    mz_uint index;

    // A pointer to the buffer containing the entire uncompressed data of the file. Unfortunately this is the only way I'm aware of for
    // reading file data from miniz.c so we'll just stick with it for now. We use a pointer to an 8-bit type so we can easily calculate
    // offsets.
    mz_uint8* pData;

    // The size of the file in bytes so we can guard against overflowing reads.
    size_t sizeInBytes;

    // The current position of the file's read pointer.
    size_t readPointer;

}easyvfs_openedfile_zip;

PRIVATE size_t easyvfs_mz_file_read_func(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n)
{
    // The opaque type is a pointer to a easyvfs_file object which represents the file of the archive.
    easyvfs_file* pZipFile = pOpaque;
    assert(pZipFile != NULL);

    easyvfs_seek(pZipFile, (easyvfs_int64)file_ofs, easyvfs_origin_start);

    size_t bytesRead;
    int result = easyvfs_read(pZipFile, pBuf, (unsigned int)n, &bytesRead);
    if (result == 0)
    {
        // Failed to read the file.
        bytesRead = 0;
    }

    return (size_t)bytesRead;
}


PRIVATE bool easyvfs_is_valid_extension__zip(const char* extension)
{
    return _stricmp(extension, "zip") == 0;
}

PRIVATE easyvfs_handle easyvfs_open_archive__zip(easyvfs_file* pArchiveFile, unsigned int accessMode)
{
    assert(pArchiveFile != NULL);
    assert(easyvfs_tell(pArchiveFile) == 0);

    // Only support read-only mode at the moment.
    if ((accessMode & EASYVFS_WRITE) != 0) {
        return NULL;
    }


    mz_zip_archive* pZip = malloc(sizeof(mz_zip_archive));
    if (pZip == NULL) {
        return NULL;
    }

    memset(pZip, 0, sizeof(mz_zip_archive));

    pZip->m_pRead = easyvfs_mz_file_read_func;
    pZip->m_pIO_opaque = pArchiveFile;
    if (!mz_zip_reader_init(pZip, easyvfs_size(pArchiveFile), 0)) {
        free(pZip);
        return NULL;
    }

    return pZip;
}

PRIVATE void easyvfs_close_archive__zip(easyvfs_handle archive)
{
    assert(archive != NULL);

    mz_zip_reader_end(archive);
    free(archive);
}

PRIVATE bool easyvfs_get_file_info__zip(easyvfs_handle archive, const char* relativePath, easyvfs_file_info* fi)
{
    assert(archive != NULL);

    mz_zip_archive* pZip = archive;
    int fileIndex = mz_zip_reader_locate_file(pZip, relativePath, NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (fileIndex == -1)
    {
        // We failed to locate the file, but there's a chance it could actually be a folder. Here's the problem - folders
        // can be named such that they include a trailing slash. We'll want to check for that. Another problem is that
        // sometimes the folders won't actually be included in the central directory at all which means we need to do a
        // manual check across every file in the archive.
        char relativePathWithSlash[EASYVFS_MAX_PATH];
        strcpy_s(relativePathWithSlash, sizeof(relativePathWithSlash), relativePath);
        strcat_s(relativePathWithSlash, sizeof(relativePathWithSlash), "/");
        fileIndex = mz_zip_reader_locate_file(pZip, relativePath, NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
        if (fileIndex == -1)
        {
            // We still couldn't find the directory even with the trailing slash. There's a chace it's a folder that's
            // simply not included in the central directory. It's appears the "Send to -> Compressed (zipped) folder" 
            // functionality in Windows does this.
            mz_uint numFiles = mz_zip_reader_get_num_files(pZip);
            for (mz_uint iFile = 0; iFile < numFiles; ++iFile)
            {
                char filePath[EASYVFS_MAX_PATH];
                if (mz_zip_reader_get_filename(pZip, iFile, filePath, EASYVFS_MAX_PATH) > 0)
                {
                    if (easyvfs_is_path_child(filePath, relativePath))
                    {
                        // This file is within a folder with a path of relativePath which means we can imply that relativePath
                        // is a folder.
                        strcpy_s(fi->absolutePath, sizeof(fi->absolutePath), relativePath);
                        fi->sizeInBytes      = 0;
                        fi->lastModifiedTime = 0;
                        fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY | EASYVFS_FILE_ATTRIBUTE_DIRECTORY;

                        return true;
                    }
                }
            }

            return false;
        }
    }

    assert(fileIndex != -1);

    if (fi != NULL)
    {
        mz_zip_archive_file_stat zipStat;
        if (mz_zip_reader_file_stat(pZip, (mz_uint)fileIndex, &zipStat))
        {
            strcpy_s(fi->absolutePath, sizeof(fi->absolutePath), relativePath);
            fi->sizeInBytes      = zipStat.m_uncomp_size;
            fi->lastModifiedTime = (easyvfs_uint64)zipStat.m_time;
            fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;
            if (mz_zip_reader_is_file_a_directory(pZip, (mz_uint)fileIndex)) {
                fi->attributes |= EASYVFS_FILE_ATTRIBUTE_DIRECTORY;
            }

            return true;
        }
    }

    return true;
}

PRIVATE easyvfs_handle easyvfs_begin_iteration__zip(easyvfs_handle archive, const char* relativePath)
{
    assert(relativePath != NULL);

    mz_zip_archive* pZip = archive;
    assert(pZip != NULL);

    int directoryFileIndex = -1;
    if (relativePath[0] == '\0') {
        directoryFileIndex = 0;
    } else {
        directoryFileIndex = mz_zip_reader_locate_file(pZip, relativePath, NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    }

    if (directoryFileIndex == -1)
    {
        // The same issue applies here as documented in easyvfs_get_file_info__zip().
        char relativePathWithSlash[EASYVFS_MAX_PATH];
        strcpy_s(relativePathWithSlash, sizeof(relativePathWithSlash), relativePath);
        strcat_s(relativePathWithSlash, sizeof(relativePathWithSlash), "/");
        directoryFileIndex = mz_zip_reader_locate_file(pZip, relativePath, NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
        if (directoryFileIndex == -1)
        {
            // We still couldn't find the directory even with the trailing slash. There's a chace it's a folder that's
            // simply not included in the central directory. It's appears the "Send to -> Compressed (zipped) folder" 
            // functionality in Windows does this.
            mz_uint numFiles = mz_zip_reader_get_num_files(pZip);
            for (mz_uint iFile = 0; iFile < numFiles; ++iFile)
            {
                char filePath[EASYVFS_MAX_PATH];
                if (mz_zip_reader_get_filename(pZip, iFile, filePath, EASYVFS_MAX_PATH) > 0)
                {
                    if (easyvfs_is_path_child(filePath, relativePath))
                    {
                        // This file is within a folder with a path of relativePath which means we can imply that relativePath
                        // is a folder.
                        goto on_success;
                    }
                }
            }

            return NULL;
        }
    }

    
    
on_success:;
    easyvfs_iterator_zip* pZipIterator = malloc(sizeof(easyvfs_iterator_zip));
    if (pZipIterator != NULL)
    {
        pZipIterator->index = 0;
        strcpy_s(pZipIterator->directoryPath, sizeof(pZipIterator->directoryPath), relativePath);
    }

    return pZipIterator;
}

PRIVATE void easyvfs_end_iteration__zip(easyvfs_handle archive, easyvfs_handle iterator)
{
    (void)archive;
    assert(archive != NULL);
    assert(iterator != NULL);
    
    free(iterator);
}

PRIVATE bool easyvfs_next_iteration__zip(easyvfs_handle archive, easyvfs_handle iterator, easyvfs_file_info* fi)
{
    (void)archive;
    assert(archive != NULL);
    assert(iterator != NULL);

    easyvfs_iterator_zip* pZipIterator = iterator;
    if (pZipIterator == NULL) {
        return false;
    }

    mz_zip_archive* pZip = archive;
    while (pZipIterator->index < mz_zip_reader_get_num_files(pZip))
    {
        unsigned int iFile = pZipIterator->index++;

        char filePath[EASYVFS_MAX_PATH];
        if (mz_zip_reader_get_filename(pZip, iFile, filePath, EASYVFS_MAX_PATH) > 0)
        {
            if (easyvfs_is_path_child(filePath, pZipIterator->directoryPath))
            {
                if (fi != NULL)
                {
                    mz_zip_archive_file_stat zipStat;
                    if (mz_zip_reader_file_stat(pZip, iFile, &zipStat))
                    {
                        strcpy_s(fi->absolutePath, sizeof(fi->absolutePath), filePath);
                        fi->sizeInBytes      = zipStat.m_uncomp_size;
                        fi->lastModifiedTime = (easyvfs_uint64)zipStat.m_time;
                        fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;
                        if (mz_zip_reader_is_file_a_directory(pZip, iFile)) {
                            fi->attributes |= EASYVFS_FILE_ATTRIBUTE_DIRECTORY;
                        }

                        // If we have a directory we need to ensure we don't have a trailing slash.
                        if ((fi->attributes & EASYVFS_FILE_ATTRIBUTE_DIRECTORY) != 0) {
                            size_t absolutePathLen = strlen(fi->absolutePath);
                            if (absolutePathLen > 0 && (fi->absolutePath[absolutePathLen - 1] == '/' || fi->absolutePath[absolutePathLen - 1] == '\\')) {
                                fi->absolutePath[absolutePathLen - 1] = '\0';
                            }
                        }
                    }
                }

                return true;
            }
        }
    }

    return false;
}

PRIVATE bool easyvfs_delete_file__zip(easyvfs_handle archive, const char* relativePath)
{
    (void)archive;
    (void)relativePath;

    assert(archive != NULL);
    assert(relativePath != 0);

    // All files are read-only for now.
    return false;
}

PRIVATE bool easyvfs_rename_file__zip(easyvfs_handle archive, const char* relativePathOld, const char* relativePathNew)
{
    (void)archive;
    (void)relativePathOld;
    (void)relativePathNew;

    assert(archive != 0);
    assert(relativePathOld != 0);
    assert(relativePathNew != 0);

    // All files are read-only for now.
    return false;
}

PRIVATE bool easyvfs_create_directory__zip(easyvfs_handle archive, const char* relativePath)
{
    (void)archive;
    (void)relativePath;

    assert(archive != 0);
    assert(relativePath != 0);

    // All files are read-only for now.
    return false;
}

PRIVATE bool easyvfs_copy_file__zip(easyvfs_handle archive, const char* relativePathSrc, const char* relativePathDst, bool failIfExists)
{
    (void)archive;
    (void)relativePathSrc;
    (void)relativePathDst;
    (void)failIfExists;

    assert(archive != 0);
    assert(relativePathSrc != 0);
    assert(relativePathDst != 0);

    // No support for this at the moment because it's read-only for now.
    return false;
}

PRIVATE easyvfs_handle easyvfs_open_file__zip(easyvfs_handle archive, const char* relativePath, unsigned int accessMode)
{
    assert(archive != NULL);
    assert(relativePath != NULL);

    // Only supporting read-only for now.
    if ((accessMode & EASYVFS_WRITE) != 0) {
        return NULL;
    }


    mz_zip_archive* pZip = archive;
    int fileIndex = mz_zip_reader_locate_file(pZip, relativePath, NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (fileIndex == -1) {
        return NULL;
    }

    easyvfs_openedfile_zip* pOpenedFile = malloc(sizeof(*pOpenedFile));
    if (pOpenedFile == NULL) {
        return NULL;
    }

    pOpenedFile->pData = mz_zip_reader_extract_to_heap(pZip, (mz_uint)fileIndex, &pOpenedFile->sizeInBytes, 0);
    if (pOpenedFile->pData == NULL) {
        free(pOpenedFile);
        return NULL;
    }

    pOpenedFile->index = (mz_uint)fileIndex;
    pOpenedFile->readPointer = 0;

    return pOpenedFile;
}

PRIVATE void easyvfs_close_file__zip(easyvfs_handle archive, easyvfs_handle file)
{
    easyvfs_openedfile_zip* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    mz_zip_archive* pZip = archive;
    assert(pZip != NULL);

    pZip->m_pFree(pZip->m_pAlloc_opaque, pOpenedFile->pData);
    free(pOpenedFile);
}

PRIVATE bool easyvfs_read_file__zip(easyvfs_handle archive, easyvfs_handle file, void* pDataOut, size_t bytesToRead, size_t* pBytesReadOut)
{
    (void)archive;
    assert(archive != NULL);
    assert(file != NULL);
    assert(pDataOut != NULL);
    assert(bytesToRead > 0);

    easyvfs_openedfile_zip* pOpenedFile = file;
    if (pOpenedFile == NULL) {
        return false;
    }

    size_t bytesAvailable = pOpenedFile->sizeInBytes - pOpenedFile->readPointer;
    if (bytesAvailable < bytesToRead) {
        bytesToRead = bytesAvailable;
    }

    if (bytesToRead == 0) {
        return false;   // Nothing left to read.
    }


    memcpy(pDataOut, pOpenedFile->pData + pOpenedFile->readPointer, bytesToRead);
    pOpenedFile->readPointer += bytesToRead;

    if (pBytesReadOut) {
        *pBytesReadOut = bytesToRead;
    }

    return true;
}

PRIVATE bool easyvfs_write_file__zip(easyvfs_handle archive, easyvfs_handle file, const void* pData, size_t bytesToWrite, size_t* pBytesWrittenOut)
{
    (void)archive;
    (void)file;
    (void)pData;
    (void)bytesToWrite;

    assert(archive != NULL);
    assert(file != NULL);
    assert(pData != NULL);
    assert(bytesToWrite > 0);

    // All files are read-only for now.
    if (pBytesWrittenOut) {
        *pBytesWrittenOut = 0;
    }

    return false;
}

PRIVATE bool easyvfs_seek_file__zip(easyvfs_handle archive, easyvfs_handle file, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
{
    (void)archive;

    assert(archive != NULL);
    assert(file != NULL);

    easyvfs_openedfile_zip* pOpenedFile = file;
    if (pOpenedFile == NULL) {
        return false;
    }

    easyvfs_uint64 newPos = pOpenedFile->readPointer;
    if (origin == easyvfs_origin_current)
    {
        if ((easyvfs_int64)newPos + bytesToSeek >= 0)
        {
            newPos = (easyvfs_uint64)((easyvfs_int64)newPos + bytesToSeek);
        }
        else
        {
            // Trying to seek to before the beginning of the file.
            return false;
        }
    }
    else if (origin == easyvfs_origin_start)
    {
        assert(bytesToSeek >= 0);
        newPos = (easyvfs_uint64)bytesToSeek;
    }
    else if (origin == easyvfs_origin_end)
    {
        assert(bytesToSeek >= 0);
        if ((easyvfs_uint64)bytesToSeek <= pOpenedFile->sizeInBytes)
        {
            newPos = pOpenedFile->sizeInBytes - (easyvfs_uint64)bytesToSeek;
        }
        else
        {
            // Trying to seek to before the beginning of the file.
            return false;
        }
    }
    else
    {
        // Should never get here.
        return false;
    }


    if (newPos > pOpenedFile->sizeInBytes) {
        return false;
    }

    pOpenedFile->readPointer = (size_t)newPos;
    return true;
}

PRIVATE easyvfs_uint64 easyvfs_tell_file__zip(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;

    easyvfs_openedfile_zip* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    return pOpenedFile->readPointer;
}

PRIVATE easyvfs_uint64 easyvfs_file_size__zip(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;

    easyvfs_openedfile_zip* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    return pOpenedFile->sizeInBytes;
}

PRIVATE void easyvfs_flush__zip(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;
    (void)file;

    assert(archive != NULL);
    assert(file != NULL);

    // All files are read-only for now.
}


PRIVATE void easyvfs_register_zip_backend(easyvfs_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    easyvfs_archive_callbacks callbacks;
    callbacks.is_valid_extension = easyvfs_is_valid_extension__zip;
    callbacks.open_archive       = easyvfs_open_archive__zip;
    callbacks.close_archive      = easyvfs_close_archive__zip;
    callbacks.get_file_info      = easyvfs_get_file_info__zip;
    callbacks.begin_iteration    = easyvfs_begin_iteration__zip;
    callbacks.end_iteration      = easyvfs_end_iteration__zip;
    callbacks.next_iteration     = easyvfs_next_iteration__zip;
    callbacks.delete_file        = easyvfs_delete_file__zip;
    callbacks.rename_file        = easyvfs_rename_file__zip;
    callbacks.create_directory   = easyvfs_create_directory__zip;
    callbacks.copy_file          = easyvfs_copy_file__zip;
    callbacks.open_file          = easyvfs_open_file__zip;
    callbacks.close_file         = easyvfs_close_file__zip;
    callbacks.read_file          = easyvfs_read_file__zip;
    callbacks.write_file         = easyvfs_write_file__zip;
    callbacks.seek_file          = easyvfs_seek_file__zip;
    callbacks.tell_file          = easyvfs_tell_file__zip;
    callbacks.file_size          = easyvfs_file_size__zip;
    callbacks.flush_file         = easyvfs_flush__zip;
    easyvfs_register_archive_backend(pContext, callbacks);
}
#endif  //EASY_VFS_NO_ZIP



///////////////////////////////////////////////////////////////////////////////
//
// Quake 2 PAK
//
///////////////////////////////////////////////////////////////////////////////
#ifndef EASY_VFS_NO_PAK
typedef struct
{
    char path[64];

}easyvfs_path_pak;

typedef struct
{
    // The file name.
    char name[56];

    // The position within the file of the first byte of the file.
    unsigned int offset;

    // The size of the file, in bytes.
    unsigned int sizeInBytes;

}easyvfs_file_pak;

typedef struct
{
    // A pointer to the archive file for reading data.
    easyvfs_file* pArchiveFile;


    // The 4-byte identifiers: "PACK"
    char id[4];

    // The offset of the directory.
    unsigned int directoryOffset;

    // The size of the directory. This should a multiple of 64.
    unsigned int directoryLength;


    // The access mode.
    unsigned int accessMode;

    // A pointer to the buffer containing the file information. The number of items in this array is equal to directoryLength / 64.
    easyvfs_file_pak* pFiles;

}easyvfs_archive_pak;


typedef struct
{
    // The current index of the iterator. When this hits childCount, the iteration is finished.
    unsigned int index;

    // The directory being iterated.
    char directoryPath[EASYVFS_MAX_PATH];


    // The number of directories that have previously been iterated.
    unsigned int processedDirCount;

    // The directories that were previously iterated.
    easyvfs_path_pak* pProcessedDirs;

}easyvfs_iterator_pak;

PRIVATE bool easyvfs_iterator_pak_append_processed_dir(easyvfs_iterator_pak* pIterator, const char* path)
{
    if (pIterator != NULL && path != NULL)
    {
        easyvfs_path_pak* pOldBuffer = pIterator->pProcessedDirs;
        easyvfs_path_pak* pNewBuffer = malloc(sizeof(easyvfs_path_pak) * (pIterator->processedDirCount + 1));

        if (pNewBuffer != 0)
        {
            for (unsigned int iDst = 0; iDst < pIterator->processedDirCount; ++iDst)
            {
                pNewBuffer[iDst] = pOldBuffer[iDst];
            }

            strcpy_s(pNewBuffer[pIterator->processedDirCount].path, 64, path);


            pIterator->pProcessedDirs     = pNewBuffer;
            pIterator->processedDirCount += 1;

            easyvfs_free(pOldBuffer);

            return 1;
        }
    }

    return 0;
}

PRIVATE bool easyvfs_iterator_pak_has_dir_been_processed(easyvfs_iterator_pak* pIterator, const char* path)
{
    for (unsigned int i = 0; i < pIterator->processedDirCount; ++i)
    {
        if (strcmp(path, pIterator->pProcessedDirs[i].path) == 0)
        {
            return 1;
        }
    }

    return 0;
}


typedef struct
{
    // The offset of the first byte of the file's data.
    size_t offsetInArchive;

    // The size of the file in bytes so we can guard against overflowing reads.
    size_t sizeInBytes;

    // The current position of the file's read pointer.
    size_t readPointer;

}easyvfs_openedfile_pak;



PRIVATE easyvfs_archive_pak* easyvfs_pak_create(easyvfs_file* pArchiveFile, unsigned int accessMode)
{
    easyvfs_archive_pak* pak = malloc(sizeof(*pak));
    if (pak != NULL)
    {
        pak->pArchiveFile    = pArchiveFile;
        pak->directoryOffset = 0;
        pak->directoryLength = 0;
        pak->accessMode      = accessMode;
        pak->pFiles          = NULL;
    }

    return pak;
}

PRIVATE void easyvfs_pak_delete(easyvfs_archive_pak* pArchive)
{
    free(pArchive->pFiles);
    free(pArchive);
}




PRIVATE bool easyvfs_is_valid_extension__pak(const char* extension)
{
    return _stricmp(extension, "pak") == 0;
}


PRIVATE easyvfs_handle easyvfs_open_archive__pak(easyvfs_file* pArchiveFile, unsigned int accessMode)
{
    assert(pArchiveFile != NULL);
    assert(easyvfs_tell(pArchiveFile) == 0);

    easyvfs_archive_pak* pak = easyvfs_pak_create(pArchiveFile, accessMode);
    if (pak != NULL)
    {
        // First 4 bytes should equal "PACK"
        if (easyvfs_read(pArchiveFile, pak->id, 4, NULL))
        {
            if (pak->id[0] == 'P' && pak->id[1] == 'A' && pak->id[2] == 'C' && pak->id[3] == 'K')
            {
                if (easyvfs_read(pArchiveFile, &pak->directoryOffset, 4, NULL))
                {
                    if (easyvfs_read(pArchiveFile, &pak->directoryLength, 4, NULL))
                    {
                        // We loaded the header just fine so now we want to allocate space for each file in the directory and load them. Note that
                        // this does not load the file data itself, just information about the files like their name and size.
                        if (pak->directoryLength % 64 == 0)
                        {
                            unsigned int fileCount = pak->directoryLength / 64;
                            if (fileCount > 0)
                            {
                                assert((sizeof(easyvfs_file_pak) * fileCount) == pak->directoryLength);

                                pak->pFiles = malloc(pak->directoryLength);
                                if (pak->pFiles != NULL)
                                {
                                    // Seek to the directory listing before trying to read it.
                                    if (easyvfs_seek(pArchiveFile, pak->directoryOffset, easyvfs_origin_start))
                                    {
                                        size_t bytesRead;
                                        if (easyvfs_read(pArchiveFile, pak->pFiles, pak->directoryLength, &bytesRead) && bytesRead == pak->directoryLength)
                                        {
                                            // All good!
                                        }
                                        else
                                        {
                                            // Failed to read the directory listing.
                                            easyvfs_pak_delete(pak);
                                            pak = NULL;
                                        }
                                    }
                                    else
                                    {
                                        // Failed to seek to the directory listing.
                                        easyvfs_pak_delete(pak);
                                        pak = NULL;
                                    }
                                }
                                else
                                {
                                    // Failed to allocate memory for the file info buffer.
                                    easyvfs_pak_delete(pak);
                                    pak = NULL;
                                }
                            }
                        }
                        else
                        {
                            // The directory length is not a multiple of 64 - something is wrong with the file.
                            easyvfs_pak_delete(pak);
                            pak = NULL;
                        }
                    }
                    else
                    {
                        // Failed to read the directory length.
                        easyvfs_pak_delete(pak);
                        pak = NULL;
                    }
                }
                else
                {
                    // Failed to read the directory offset.
                    easyvfs_pak_delete(pak);
                    pak = NULL;
                }
            }
            else
            {
                // Not a pak file.
                easyvfs_pak_delete(pak);
                pak = NULL;
            }
        }
        else
        {
            // Failed to read the header.
            easyvfs_pak_delete(pak);
            pak = NULL;
        }
    }

    return pak;
}

PRIVATE void easyvfs_close_archive__pak(easyvfs_handle archive)
{
    easyvfs_archive_pak* pPak = archive;
    assert(pPak != NULL);

    easyvfs_pak_delete(pPak);
}

PRIVATE bool easyvfs_get_file_info__pak(easyvfs_handle archive, const char* relativePath, easyvfs_file_info* fi)
{
    // We can determine whether or not the path refers to a file or folder by checking it the path is parent of any
    // files in the archive. If so, it's a folder, otherwise it's a file (so long as it exists).
    easyvfs_archive_pak* pak = archive;
    assert(pak != NULL);

    unsigned int fileCount = pak->directoryLength / 64;
    for (unsigned int i = 0; i < fileCount; ++i)
    {
        easyvfs_file_pak* pFile = pak->pFiles + i;
        if (strcmp(pFile->name, relativePath) == 0)
        {
            // It's a file.
            strcpy_s(fi->absolutePath, sizeof(fi->absolutePath), relativePath);
            fi->sizeInBytes      = (easyvfs_uint64)pFile->sizeInBytes;
            fi->lastModifiedTime = 0;
            fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;

            return true;
        }
        else if (easyvfs_is_path_descendant(pFile->name, relativePath))
        {
            // It's a directory.
            strcpy_s(fi->absolutePath, sizeof(fi->absolutePath), relativePath);
            fi->sizeInBytes      = 0;
            fi->lastModifiedTime = 0;
            fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY | EASYVFS_FILE_ATTRIBUTE_DIRECTORY;

            return true;
        }
    }

    return false;
}

PRIVATE easyvfs_handle easyvfs_begin_iteration__pak(easyvfs_handle archive, const char* relativePath)
{
    (void)archive;
    assert(relativePath != NULL);

    easyvfs_iterator_pak* pIterator = malloc(sizeof(easyvfs_iterator_pak));
    if (pIterator != NULL)
    {
        pIterator->index = 0;
        strcpy_s(pIterator->directoryPath, sizeof(pIterator->directoryPath), relativePath);
        pIterator->processedDirCount = 0;
        pIterator->pProcessedDirs    = NULL;
    }

    return pIterator;
}

PRIVATE void easyvfs_end_iteration__pak(easyvfs_handle archive, easyvfs_handle iterator)
{
    (void)archive;

    easyvfs_iterator_pak* pIterator = iterator;
    assert(pIterator != NULL);

    free(pIterator);
}

PRIVATE bool easyvfs_next_iteration__pak(easyvfs_handle archive, easyvfs_handle iterator, easyvfs_file_info* fi)
{
    easyvfs_iterator_pak* pIterator = iterator;
    assert(pIterator != NULL);

    easyvfs_archive_pak* pak = archive;
    assert(pak != NULL);

    unsigned int fileCount = pak->directoryLength / 64;
    while (pIterator->index < fileCount)
    {
        unsigned int iFile = pIterator->index++;

        easyvfs_file_pak* pFile = pak->pFiles + iFile;
        if (easyvfs_is_path_child(pFile->name, pIterator->directoryPath))
        {
            // It's a file.
            strcpy_s(fi->absolutePath, EASYVFS_MAX_PATH, pFile->name);
            fi->sizeInBytes      = (easyvfs_uint64)pFile->sizeInBytes;
            fi->lastModifiedTime = 0;
            fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;

            return true;
        }
        else if (easyvfs_is_path_descendant(pFile->name, pIterator->directoryPath))
        {
            // It's a directory. This needs special handling because we don't want to iterate over the same directory multiple times.
            const char* childDirEnd = pFile->name + strlen(pIterator->directoryPath) + 1;    // +1 for the slash.
            while (childDirEnd[0] != '\0' && childDirEnd[0] != '/' && childDirEnd[0] != '\\')
            {
                childDirEnd += 1;
            }

            char childDir[64];
            memcpy(childDir, pFile->name, childDirEnd - pFile->name);
            childDir[childDirEnd - pFile->name] = '\0';

            if (!easyvfs_iterator_pak_has_dir_been_processed(pIterator, childDir))
            {
                // It's a directory.
                strcpy_s(fi->absolutePath, EASYVFS_MAX_PATH, childDir);
                fi->sizeInBytes      = 0;
                fi->lastModifiedTime = 0;
                fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY | EASYVFS_FILE_ATTRIBUTE_DIRECTORY;

                easyvfs_iterator_pak_append_processed_dir(pIterator, childDir);

                return true;
            }
        }
    }

    return false;
}

PRIVATE bool easyvfs_delete_file__pak(easyvfs_handle archive, const char* relativePath)
{
    (void)archive;
    (void)relativePath;

    assert(archive != NULL);
    assert(relativePath != 0);

    // All files are read-only for now.
    return false;
}

PRIVATE bool easyvfs_rename_file__pak(easyvfs_handle archive, const char* relativePathOld, const char* relativePathNew)
{
    (void)archive;
    (void)relativePathOld;
    (void)relativePathNew;

    assert(archive != 0);
    assert(relativePathOld != 0);
    assert(relativePathNew != 0);

    // All files are read-only for now.
    return false;
}

PRIVATE bool easyvfs_create_directory__pak(easyvfs_handle archive, const char* relativePath)
{
    (void)archive;
    (void)relativePath;

    assert(archive != 0);
    assert(relativePath != 0);

    // All files are read-only for now.
    return false;
}

PRIVATE bool easyvfs_copy_file__pak(easyvfs_handle archive, const char* relativePathSrc, const char* relativePathDst, bool failIfExists)
{
    (void)archive;
    (void)relativePathSrc;
    (void)relativePathDst;
    (void)failIfExists;

    assert(archive != 0);
    assert(relativePathSrc != 0);
    assert(relativePathDst != 0);

    // No support for this at the moment because it's read-only for now.
    return false;
}

PRIVATE easyvfs_handle easyvfs_open_file__pak(easyvfs_handle archive, const char* relativePath, unsigned int accessMode)
{
    assert(relativePath != NULL);

    // Only supporting read-only for now.
    if ((accessMode & EASYVFS_WRITE) != 0) {
        return NULL;
    }


    easyvfs_archive_pak* pak = archive;
    assert(pak != NULL);

    for (unsigned int iFile = 0; iFile < (pak->directoryLength / 64); ++iFile)
    {
        if (strcmp(relativePath, pak->pFiles[iFile].name) == 0)
        {
            // We found the file.
            easyvfs_openedfile_pak* pOpenedFile = malloc(sizeof(*pOpenedFile));
            if (pOpenedFile != NULL)
            {
                pOpenedFile->offsetInArchive = pak->pFiles[iFile].offset;
                pOpenedFile->sizeInBytes     = pak->pFiles[iFile].sizeInBytes;
                pOpenedFile->readPointer     = 0;

                return pOpenedFile;
            }
        }
    }

   
    return NULL;
}

PRIVATE void easyvfs_close_file__pak(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;

    easyvfs_openedfile_pak* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    free(pOpenedFile);
}

PRIVATE bool easyvfs_read_file__pak(easyvfs_handle archive, easyvfs_handle file, void* pDataOut, size_t bytesToRead, size_t* pBytesReadOut)
{
    assert(pDataOut != NULL);
    assert(bytesToRead > 0);

    easyvfs_archive_pak* pak = archive;
    assert(pak != NULL);

    easyvfs_openedfile_pak* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    // The read pointer should never go past the file size.
    assert(pOpenedFile->sizeInBytes >= pOpenedFile->readPointer);


    size_t bytesAvailable = pOpenedFile->sizeInBytes - pOpenedFile->readPointer;
    if (bytesAvailable < bytesToRead) {
        bytesToRead = bytesAvailable;     // Safe cast, as per the check above.
    }

    easyvfs_seek(pak->pArchiveFile, (easyvfs_int64)(pOpenedFile->offsetInArchive + pOpenedFile->readPointer), easyvfs_origin_start);
    int result = easyvfs_read(pak->pArchiveFile, pDataOut, bytesToRead, pBytesReadOut);
    if (result != 0) {
        pOpenedFile->readPointer += bytesToRead;
    }

    return result;
}

PRIVATE bool easyvfs_write_file__pak(easyvfs_handle archive, easyvfs_handle file, const void* pData, size_t bytesToWrite, size_t* pBytesWrittenOut)
{
    (void)archive;
    (void)file;
    (void)pData;
    (void)bytesToWrite;

    assert(archive != NULL);
    assert(file != NULL);
    assert(pData != NULL);
    assert(bytesToWrite > 0);

    // All files are read-only for now.
    if (pBytesWrittenOut) {
        *pBytesWrittenOut = 0;
    }

    return false;
}

PRIVATE bool easyvfs_seek_file__pak(easyvfs_handle archive, easyvfs_handle file, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
{
    (void)archive;

    easyvfs_openedfile_pak* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    easyvfs_uint64 newPos = pOpenedFile->readPointer;
    if (origin == easyvfs_origin_current)
    {
        if ((easyvfs_int64)newPos + bytesToSeek >= 0)
        {
            newPos = (easyvfs_uint64)((easyvfs_int64)newPos + bytesToSeek);
        }
        else
        {
            // Trying to seek to before the beginning of the file.
            return false;
        }
    }
    else if (origin == easyvfs_origin_start)
    {
        assert(bytesToSeek >= 0);
        newPos = (easyvfs_uint64)bytesToSeek;
    }
    else if (origin == easyvfs_origin_end)
    {
        assert(bytesToSeek >= 0);
        if ((easyvfs_uint64)bytesToSeek <= pOpenedFile->sizeInBytes)
        {
            newPos = pOpenedFile->sizeInBytes - (easyvfs_uint64)bytesToSeek;
        }
        else
        {
            // Trying to seek to before the beginning of the file.
            return false;
        }
    }
    else
    {
        // Should never get here.
        return false;
    }


    if (newPos > pOpenedFile->sizeInBytes) {
        return false;
    }

    pOpenedFile->readPointer = (size_t)newPos;
    return true;
}

PRIVATE easyvfs_uint64 easyvfs_tell_file__pak(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;

    easyvfs_openedfile_pak* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    return pOpenedFile->readPointer;
}

PRIVATE easyvfs_uint64 easyvfs_file_size__pak(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;

    easyvfs_openedfile_pak* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    return pOpenedFile->sizeInBytes;
}

PRIVATE void easyvfs_flush__pak(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;
    (void)file;

    assert(archive != NULL);
    assert(file != NULL);

    // All files are read-only for now.
}

PRIVATE void easyvfs_register_pak_backend(easyvfs_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    easyvfs_archive_callbacks callbacks;
    callbacks.is_valid_extension = easyvfs_is_valid_extension__pak;
    callbacks.open_archive       = easyvfs_open_archive__pak;
    callbacks.close_archive      = easyvfs_close_archive__pak;
    callbacks.get_file_info      = easyvfs_get_file_info__pak;
    callbacks.begin_iteration    = easyvfs_begin_iteration__pak;
    callbacks.end_iteration      = easyvfs_end_iteration__pak;
    callbacks.next_iteration     = easyvfs_next_iteration__pak;
    callbacks.delete_file        = easyvfs_delete_file__pak;
    callbacks.rename_file        = easyvfs_rename_file__pak;
    callbacks.create_directory   = easyvfs_create_directory__pak;
    callbacks.copy_file          = easyvfs_copy_file__pak;
    callbacks.open_file          = easyvfs_open_file__pak;
    callbacks.close_file         = easyvfs_close_file__pak;
    callbacks.read_file          = easyvfs_read_file__pak;
    callbacks.write_file         = easyvfs_write_file__pak;
    callbacks.seek_file          = easyvfs_seek_file__pak;
    callbacks.tell_file          = easyvfs_tell_file__pak;
    callbacks.file_size          = easyvfs_file_size__pak;
    callbacks.flush_file         = easyvfs_flush__pak;
    easyvfs_register_archive_backend(pContext, callbacks);
}
#endif  //EASY_VFS_NO_PAK



///////////////////////////////////////////////////////////////////////////////
//
// Wavefront MTL
//
///////////////////////////////////////////////////////////////////////////////
#ifndef EASY_VFS_NO_MTL
typedef struct
{
    // The byte offset within the archive 
    easyvfs_uint64 offset;

    // The size of the file, in bytes.
    easyvfs_uint64 sizeInBytes;

    // The name of the material. The specification says this can be any length, but we're going to clamp it to 255 + null terminator which should be fine.
    char name[256];

}easyvfs_file_mtl;

typedef struct
{
    // A pointer to the archive's file so we can read data.
    easyvfs_file* pArchiveFile;

    // The access mode.
    unsigned int accessMode;

    // The buffer containing the list of files.
    easyvfs_file_mtl* pFiles;

    // The number of files in the archive.
    unsigned int fileCount;

}easyvfs_archive_mtl;

typedef struct
{
    // The current index of the iterator. When this hits the file count, the iteration is finished.
    unsigned int index;

}easyvfs_iterator_mtl;

typedef struct
{
    // The offset within the archive file the first byte of the file is located.
    easyvfs_uint64 offsetInArchive;

    // The size of the file in bytes so we can guard against overflowing reads.
    easyvfs_uint64 sizeInBytes;

    // The current position of the file's read pointer.
    easyvfs_uint64 readPointer;

}easyvfs_openedfile_mtl;


PRIVATE easyvfs_archive_mtl* easyvfs_mtl_create(easyvfs_file* pArchiveFile, unsigned int accessMode)
{
    easyvfs_archive_mtl* mtl = malloc(sizeof(easyvfs_archive_mtl));
    if (mtl != NULL)
    {
        mtl->pArchiveFile = pArchiveFile;
        mtl->accessMode   = accessMode;
        mtl->pFiles       = NULL;
        mtl->fileCount    = 0;
    }

    return mtl;
}

PRIVATE void easyvfs_mtl_delete(easyvfs_archive_mtl* pArchive)
{
    free(pArchive->pFiles);
    free(pArchive);
}

PRIVATE void easyvfs_mtl_addfile(easyvfs_archive_mtl* pArchive, easyvfs_file_mtl* pFile)
{
    if (pArchive != NULL && pFile != NULL)
    {
        easyvfs_file_mtl* pOldBuffer = pArchive->pFiles;
        easyvfs_file_mtl* pNewBuffer = malloc(sizeof(easyvfs_file_mtl) * (pArchive->fileCount + 1));

        if (pNewBuffer != 0)
        {
            for (unsigned int iDst = 0; iDst < pArchive->fileCount; ++iDst) {
                pNewBuffer[iDst] = pOldBuffer[iDst];
            }

            pNewBuffer[pArchive->fileCount] = *pFile;

            pArchive->pFiles     = pNewBuffer;
            pArchive->fileCount += 1;

            free(pOldBuffer);
        }
    }
}


typedef struct
{
    easyvfs_uint64 archiveSizeInBytes;
    easyvfs_uint64 bytesRemaining;
    easyvfs_file*  pFile;
    char*          chunkPointer;
    char*          chunkEnd;
    char           chunk[4096];
    unsigned int   chunkSize;

}easyvfs_openarchive_mtl_state;

PRIVATE bool easyvfs_mtl_loadnextchunk(easyvfs_openarchive_mtl_state* pState)
{
    assert(pState != NULL);

    if (pState->bytesRemaining > 0)
    {
        pState->chunkSize = (pState->bytesRemaining > 4096) ? 4096 : (unsigned int)pState->bytesRemaining;
        assert(pState->chunkSize);

        if (easyvfs_read(pState->pFile, pState->chunk, pState->chunkSize, NULL))
        {
            pState->bytesRemaining -= pState->chunkSize;
            pState->chunkPointer = pState->chunk;
            pState->chunkEnd     = pState->chunk + pState->chunkSize;

            return 1;
        }
        else
        {
            // An error occured while reading. Just reset everything to make it look like an error occured.
            pState->bytesRemaining = 0;
            pState->chunkSize      = 0;
            pState->chunkPointer   = pState->chunk;
            pState->chunkEnd       = pState->chunkPointer;
        }
    }

    return 0;
}

PRIVATE bool easyvfs_mtl_loadnewmtl(easyvfs_openarchive_mtl_state* pState)
{
    assert(pState != NULL);

    const char newmtl[7] = "newmtl";
    for (unsigned int i = 0; i < 6; ++i)
    {
        // Check if we need a new chunk.
        if (pState->chunkPointer >= pState->chunkEnd)
        {
            if (!easyvfs_mtl_loadnextchunk(pState))
            {
                return 0;
            }
        }


        if (pState->chunkPointer[0] != newmtl[i])
        {
            return 0;
        }

        pState->chunkPointer += 1;
    }

    // At this point the first 6 characters equal "newmtl".
    return 1;
}

PRIVATE bool easyvfs_mtl_skipline(easyvfs_openarchive_mtl_state* pState)
{
    assert(pState != NULL);

    // Keep looping until we find a new line character.
    while (pState->chunkPointer < pState->chunkEnd)
    {
        if (pState->chunkPointer[0] == '\n')
        {
            // Found the new line. Now move forward by one to get past the new line character.
            pState->chunkPointer += 1;
            if (pState->chunkPointer >= pState->chunkEnd)
            {
                return easyvfs_mtl_loadnextchunk(pState);
            }

            return 1;
        }

        pState->chunkPointer += 1;
    }

    // If we get here it means we got past the end of the chunk. We just read the next chunk and call this recursively.
    if (easyvfs_mtl_loadnextchunk(pState))
    {
        return easyvfs_mtl_skipline(pState);
    }

    return 0;
}

PRIVATE bool easyvfs_mtl_skipwhitespace(easyvfs_openarchive_mtl_state* pState)
{
    assert(pState != NULL);

    while (pState->chunkPointer < pState->chunkEnd)
    {
        const char c = pState->chunkPointer[0];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
        {
            return 1;
        }

        pState->chunkPointer += 1;
    }

    if (easyvfs_mtl_loadnextchunk(pState))
    {
        return easyvfs_mtl_skipwhitespace(pState);
    }

    return 0;
}

PRIVATE bool easyvfs_mtl_loadmtlname(easyvfs_openarchive_mtl_state* pState, void* dst, unsigned int dstSizeInBytes)
{
    assert(pState != NULL);

    // We loop over character by character until we find a whitespace, "#" character or the end of the file.
    char* dst8 = dst;
    while (dstSizeInBytes > 0 && pState->chunkPointer < pState->chunkEnd)
    {
        const char c = pState->chunkPointer[0];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '#')
        {
            // We've found the end of the name. Null terminate and return.
            *dst8 = '\0';
            return 1;
        }
        else
        {
            *dst8++ = c;
            dstSizeInBytes -= 1;
            pState->chunkPointer += 1;
        }
    }

    // At this point we either ran out of space in the destination buffer or the chunk.
    if (dstSizeInBytes > 0)
    {
        // We got to the end of the chunk, so we need to load the next chunk and call this recursively.
        assert(pState->chunkPointer == pState->chunkEnd);

        if (easyvfs_mtl_loadnextchunk(pState))
        {
            return easyvfs_mtl_loadmtlname(pState, dst8, dstSizeInBytes);
        }
        else
        {
            // We reached the end of the file, but the name may be valid.
            return 1;
        }
    }
    else
    {
        // We ran out of room in the buffer.
        return 0;
    }
}


PRIVATE bool easyvfs_is_valid_extension__mtl(const char* extension)
{
    return _stricmp(extension, "mtl") == 0;
}

PRIVATE easyvfs_handle easyvfs_open_archive__mtl(easyvfs_file* pArchiveFile, unsigned int accessMode)
{
    assert(pArchiveFile != NULL);
    assert(easyvfs_tell(pArchiveFile) == 0);

    easyvfs_archive_mtl* mtl = easyvfs_mtl_create(pArchiveFile, accessMode);
    if (mtl == NULL) {
        return NULL;
    }

    // We create a state object that is used to help us with chunk management.
    easyvfs_openarchive_mtl_state state;
    state.pFile              = pArchiveFile;
    state.archiveSizeInBytes = easyvfs_size(pArchiveFile);
    state.bytesRemaining     = state.archiveSizeInBytes;
    state.chunkSize          = 0;
    state.chunkPointer       = state.chunk;
    state.chunkEnd           = state.chunk;
    if (easyvfs_mtl_loadnextchunk(&state))
    {
        while (state.bytesRemaining > 0 || state.chunkPointer < state.chunkEnd)
        {
            ptrdiff_t bytesRemainingInChunk = state.chunkEnd - state.chunkPointer;
            assert(bytesRemainingInChunk > 0);

            easyvfs_uint64 newmtlOffset = state.archiveSizeInBytes - state.bytesRemaining - ((easyvfs_uint64)bytesRemainingInChunk);

            if (easyvfs_mtl_loadnewmtl(&state))
            {
                if (state.chunkPointer[0] == ' ' || state.chunkPointer[0] == '\t')
                {
                    // We found a new material. We need to iterate until we hit the first whitespace, "#", or the end of the file.
                    if (easyvfs_mtl_skipwhitespace(&state))
                    {
                        easyvfs_file_mtl file;
                        if (easyvfs_mtl_loadmtlname(&state, file.name, 256))
                        {
                            // Everything worked out. We now need to create the file and add it to our list. At this point we won't know the size. We determine
                            // the size in a post-processing step later.
                            file.offset = newmtlOffset;
                            easyvfs_mtl_addfile(mtl, &file);
                        }
                    }
                }
            }

            // Move to the next line.
            easyvfs_mtl_skipline(&state);
        }


        // The files will have been read at this point, but we need to do a post-processing step to retrieve the size of each file.
        for (unsigned int iFile = 0; iFile < mtl->fileCount; ++iFile)
        {
            if (iFile < mtl->fileCount - 1)
            {
                // It's not the last item. The size of this item is the offset of the next file minus the offset of this file.
                mtl->pFiles[iFile].sizeInBytes = mtl->pFiles[iFile + 1].offset - mtl->pFiles[iFile].offset;
            }
            else
            {
                // It's the last item. The size of this item is the size of the archive file minus the file's offset.
                mtl->pFiles[iFile].sizeInBytes = state.archiveSizeInBytes - mtl->pFiles[iFile].offset;
            }
        }
    }
    else
    {
        easyvfs_mtl_delete(mtl);
        mtl = NULL;
    }

    return mtl;
}

PRIVATE void easyvfs_close_archive__mtl(easyvfs_handle archive)
{
    easyvfs_archive_mtl* mtl = archive;
    assert(mtl != NULL);

    easyvfs_mtl_delete(mtl);
}

PRIVATE bool easyvfs_get_file_info__mtl(easyvfs_handle archive, const char* relativePath, easyvfs_file_info* fi)
{
    easyvfs_archive_mtl* mtl = archive;
    assert(mtl != NULL);

    for (unsigned int iFile = 0; iFile < mtl->fileCount; ++iFile)
    {
        if (strcmp(relativePath, mtl->pFiles[iFile].name) == 0)
        {
            // We found the file.
            if (fi != NULL)
            {
                strcpy_s(fi->absolutePath, sizeof(fi->absolutePath), relativePath);
                fi->sizeInBytes      = mtl->pFiles[iFile].sizeInBytes;
                fi->lastModifiedTime = 0;
                fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;
            }

            return 1;
        }
    }

    return 0;
}

PRIVATE easyvfs_handle easyvfs_begin_iteration__mtl(easyvfs_handle archive, const char* relativePath)
{
    assert(relativePath != NULL);

    easyvfs_archive_mtl* mtl = archive;
    assert(mtl != NULL);

    if (mtl->fileCount > 0)
    {
        if (relativePath[0] == '\0' || (relativePath[0] == '/' && relativePath[1] == '\0'))     // This is a flat archive, so no sub-folders.
        {
            easyvfs_iterator_mtl* pIterator = malloc(sizeof(*pIterator));
            if (pIterator != NULL)
            {
                pIterator->index = 0;
                return pIterator;
            }
        }
    }

    return NULL;
}

PRIVATE void easyvfs_end_iteration__mtl(easyvfs_handle archive, easyvfs_handle iterator)
{
    (void)archive;

    easyvfs_iterator_mtl* pIterator = iterator;
    free(pIterator);
}

PRIVATE bool easyvfs_next_iteration__mtl(easyvfs_handle archive, easyvfs_handle iterator, easyvfs_file_info* fi)
{
    easyvfs_archive_mtl* mtl = archive;
    assert(mtl != NULL);

    easyvfs_iterator_mtl* pIterator = iterator;
    assert(pIterator != NULL);

    if (pIterator->index < mtl->fileCount)
    {
        if (fi != NULL)
        {
            strcpy_s(fi->absolutePath, EASYVFS_MAX_PATH, mtl->pFiles[pIterator->index].name);
            fi->sizeInBytes      = mtl->pFiles[pIterator->index].sizeInBytes;
            fi->lastModifiedTime = 0;
            fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;
        }

        pIterator->index += 1;
        return true;
    }
    
    return false;
}

PRIVATE bool easyvfs_delete_file__mtl(easyvfs_handle archive, const char* relativePath)
{
    (void)archive;
    (void)relativePath;

    assert(archive != NULL);
    assert(relativePath != 0);

    // All files are read-only for now.
    return false;
}

PRIVATE bool easyvfs_rename_file__mtl(easyvfs_handle archive, const char* relativePathOld, const char* relativePathNew)
{
    (void)archive;
    (void)relativePathOld;
    (void)relativePathNew;

    assert(archive != 0);
    assert(relativePathOld != 0);
    assert(relativePathNew != 0);

    // All files are read-only for now.
    return false;
}

PRIVATE bool easyvfs_create_directory__mtl(easyvfs_handle archive, const char* relativePath)
{
    (void)archive;
    (void)relativePath;

    assert(archive != 0);
    assert(relativePath != 0);

    // All files are read-only for now.
    return false;
}

PRIVATE bool easyvfs_copy_file__mtl(easyvfs_handle archive, const char* relativePathSrc, const char* relativePathDst, bool failIfExists)
{
    (void)archive;
    (void)relativePathSrc;
    (void)relativePathDst;
    (void)failIfExists;

    assert(archive != 0);
    assert(relativePathSrc != 0);
    assert(relativePathDst != 0);

    // No support for this at the moment because it's read-only for now.
    return false;
}

PRIVATE easyvfs_handle easyvfs_open_file__mtl(easyvfs_handle archive, const char* relativePath, unsigned int accessMode)
{
    assert(relativePath != NULL);

    // Only supporting read-only for now.
    if ((accessMode & EASYVFS_WRITE) != 0) {
        return NULL;
    }

    easyvfs_archive_mtl* mtl = archive;
    assert(mtl != NULL);

    for (unsigned int iFile = 0; iFile < mtl->fileCount; ++iFile)
    {
        if (strcmp(relativePath, mtl->pFiles[iFile].name) == 0)
        {
            // We found the file.
            easyvfs_openedfile_mtl* pOpenedFile = malloc(sizeof(easyvfs_openedfile_mtl));
            if (pOpenedFile != NULL)
            {
                pOpenedFile->offsetInArchive = mtl->pFiles[iFile].offset;
                pOpenedFile->sizeInBytes     = mtl->pFiles[iFile].sizeInBytes;
                pOpenedFile->readPointer     = 0;

                return pOpenedFile;
            }
        }
    }

    return NULL;
}

PRIVATE void easyvfs_close_file__mtl(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;

    easyvfs_openedfile_mtl* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    free(pOpenedFile);
}

PRIVATE bool easyvfs_read_file__mtl(easyvfs_handle archive, easyvfs_handle file, void* pDataOut, size_t bytesToRead, size_t* pBytesReadOut)
{
    assert(pDataOut != NULL);
    assert(bytesToRead > 0);

    easyvfs_archive_mtl* mtl = archive;
    assert(mtl != NULL);

    easyvfs_openedfile_mtl* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    // The read pointer should never go past the file size.
    assert(pOpenedFile->sizeInBytes >= pOpenedFile->readPointer);

    easyvfs_uint64 bytesAvailable = pOpenedFile->sizeInBytes - pOpenedFile->readPointer;
    if (bytesAvailable < bytesToRead) {
        bytesToRead = (size_t)bytesAvailable;     // Safe cast, as per the check above.
    }

    easyvfs_seek(mtl->pArchiveFile, (easyvfs_int64)(pOpenedFile->offsetInArchive + pOpenedFile->readPointer), easyvfs_origin_start);
    int result = easyvfs_read(mtl->pArchiveFile, pDataOut, bytesToRead, pBytesReadOut);
    if (result != 0) {
        pOpenedFile->readPointer += bytesToRead;
    }

    return result;
}

PRIVATE bool easyvfs_write_file__mtl(easyvfs_handle archive, easyvfs_handle file, const void* pData, size_t bytesToWrite, size_t* pBytesWrittenOut)
{
    (void)archive;
    (void)file;
    (void)pData;
    (void)bytesToWrite;

    assert(archive != NULL);
    assert(file != NULL);
    assert(pData != NULL);
    assert(bytesToWrite > 0);

    // All files are read-only for now.
    if (pBytesWrittenOut) {
        *pBytesWrittenOut = 0;
    }

    return false;
}

PRIVATE bool easyvfs_seek_file__mtl(easyvfs_handle archive, easyvfs_handle file, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
{
    (void)archive;

    easyvfs_openedfile_mtl* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    easyvfs_uint64 newPos = pOpenedFile->readPointer;
    if (origin == easyvfs_origin_current)
    {
        if ((easyvfs_int64)newPos + bytesToSeek >= 0)
        {
            newPos = (easyvfs_uint64)((easyvfs_int64)newPos + bytesToSeek);
        }
        else
        {
            // Trying to seek to before the beginning of the file.
            return false;
        }
    }
    else if (origin == easyvfs_origin_start)
    {
        assert(bytesToSeek >= 0);
        newPos = (easyvfs_uint64)bytesToSeek;
    }
    else if (origin == easyvfs_origin_end)
    {
        assert(bytesToSeek >= 0);
        if ((easyvfs_uint64)bytesToSeek <= pOpenedFile->sizeInBytes)
        {
            newPos = pOpenedFile->sizeInBytes - (easyvfs_uint64)bytesToSeek;
        }
        else
        {
            // Trying to seek to before the beginning of the file.
            return false;
        }
    }
    else
    {
        // Should never get here.
        return false;
    }


    if (newPos > pOpenedFile->sizeInBytes)
    {
        return false;
    }

    pOpenedFile->readPointer = newPos;
    return true;
}

PRIVATE easyvfs_uint64 easyvfs_tell_file__mtl(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;

    easyvfs_openedfile_mtl* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    return pOpenedFile->readPointer;
}

PRIVATE easyvfs_uint64 easyvfs_file_size__mtl(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;

    easyvfs_openedfile_mtl* pOpenedFile = file;
    assert(pOpenedFile != NULL);

    return pOpenedFile->sizeInBytes;
}

PRIVATE void easyvfs_flush__mtl(easyvfs_handle archive, easyvfs_handle file)
{
    (void)archive;
    (void)file;

    assert(archive != NULL);
    assert(file != NULL);

    // All files are read-only for now.
}


PRIVATE void easyvfs_register_mtl_backend(easyvfs_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    easyvfs_archive_callbacks callbacks;
    callbacks.is_valid_extension = easyvfs_is_valid_extension__mtl;
    callbacks.open_archive       = easyvfs_open_archive__mtl;
    callbacks.close_archive      = easyvfs_close_archive__mtl;
    callbacks.get_file_info      = easyvfs_get_file_info__mtl;
    callbacks.begin_iteration    = easyvfs_begin_iteration__mtl;
    callbacks.end_iteration      = easyvfs_end_iteration__mtl;
    callbacks.next_iteration     = easyvfs_next_iteration__mtl;
    callbacks.delete_file        = easyvfs_delete_file__mtl;
    callbacks.rename_file        = easyvfs_rename_file__mtl;
    callbacks.create_directory   = easyvfs_create_directory__mtl;
    callbacks.copy_file          = easyvfs_copy_file__mtl;
    callbacks.open_file          = easyvfs_open_file__mtl;
    callbacks.close_file         = easyvfs_close_file__mtl;
    callbacks.read_file          = easyvfs_read_file__mtl;
    callbacks.write_file         = easyvfs_write_file__mtl;
    callbacks.seek_file          = easyvfs_seek_file__mtl;
    callbacks.tell_file          = easyvfs_tell_file__mtl;
    callbacks.file_size          = easyvfs_file_size__mtl;
    callbacks.flush_file         = easyvfs_flush__mtl;
    easyvfs_register_archive_backend(pContext, callbacks);
}
#endif  //EASY_VFS_NO_MTL



#endif  //EASY_VFS_IMPLEMENTATION

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
