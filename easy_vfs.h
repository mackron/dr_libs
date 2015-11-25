//Public Domain. See "unlicense" statement at the end of this file.

//
// OPTIONS
//
// #define EASYVFS_NO_ZIP
//   Disables support for Zip files.
//
// #define EASYVFS_NO_PAK
//   Disables support for Quake 2 PAK files.
//
// #define EASYVFS_NO_MTL
//   Disables support for Wavefront MTL files.
//

#ifndef easy_vfs
#define easy_vfs

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>


// If you're project is using easy_path for path manipulation, you can change this value to 1 and set the header location
// below. This will allow the compiler to strip away a little bit of duplicate code.
#ifndef EASYVFS_USE_EASYPATH
#define EASYVFS_USE_EASYPATH    0
#endif

#if EASYVFS_USE_EASYPATH
// If you're using easy_path, change this to the location of the easy_path header, relative to the source file. The source
// file will include this path as #include EASYPATH_HEADER.
#define EASYPATH_HEADER    "../easy_path/easy_path.h"
#endif


// The maximum length of a path in bytes, including the null terminator. If a path exceeds this amount, it will be truncated and thus
// won't contain a meaningful value. When this is changed the source file will need to be recompiled. Most of the time leaving this
// at 256 is fine, but it's not a problem to increase the size if you are encountering truncation issues. Note that increasing this
// value will increase memory usage. You should not need make this any higher than 4096.
//#define EASYVFS_MAX_PATH    256
#define EASYVFS_MAX_PATH    1024
//#define EASYVFS_MAX_PATH    4096



/// The allowable access modes.
typedef unsigned int easyvfs_access_mode;

#define EASYVFS_READ        (1 << 0)
#define EASYVFS_WRITE       (1 << 1)
#define EASYVFS_EXISTING    (1 << 2)
#define EASYVFS_APPEND      (1 << 3)
#define EASYVFS_CREATE_DIRS (1 << 4)    // Creates the directory structure if required.

#define EASYVFS_FILE_ATTRIBUTE_DIRECTORY    0x00000001
#define EASYVFS_FILE_ATTRIBUTE_READONLY     0x00000002


/// The allowable seeking origins.
typedef enum
{
    easyvfs_current,
    easyvfs_start,
    easyvfs_end

}easyvfs_seek_origin;


typedef long long          easyvfs_int64;
typedef unsigned long long easyvfs_uint64;

typedef struct easyvfs_context   easyvfs_context;
typedef struct easyvfs_archive   easyvfs_archive;
typedef struct easyvfs_file      easyvfs_file;
typedef struct easyvfs_file_info easyvfs_file_info;
typedef struct easyvfs_iterator  easyvfs_iterator;


typedef bool           (* easyvfs_is_valid_archive_proc) (easyvfs_context* pContext, const char* path);
typedef void*          (* easyvfs_open_archive_proc)     (easyvfs_file* pFile, easyvfs_access_mode accessMode);
typedef void           (* easyvfs_close_archive_proc)    (easyvfs_archive* pArchive);
typedef bool           (* easyvfs_get_file_info_proc)    (easyvfs_archive* pArchive, const char* path, easyvfs_file_info* fi);
typedef void*          (* easyvfs_begin_iteration_proc)  (easyvfs_archive* pArchive, const char* path);
typedef void           (* easyvfs_end_iteration_proc)    (easyvfs_archive* pArchive, easyvfs_iterator* i);
typedef bool           (* easyvfs_next_iteration_proc)   (easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_file_info* fi);
typedef void*          (* easyvfs_open_file_proc)        (easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode);
typedef void           (* easyvfs_close_file_proc)       (easyvfs_file* pFile);
typedef bool           (* easyvfs_read_file_proc)        (easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);
typedef bool           (* easyvfs_write_file_proc)       (easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);
typedef bool           (* easyvfs_seek_file_proc)        (easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin);
typedef easyvfs_uint64 (* easyvfs_tell_file_proc)        (easyvfs_file* pFile);
typedef easyvfs_uint64 (* easyvfs_file_size_proc)        (easyvfs_file* pFile);
typedef void           (* easyvfs_flush_file_proc)       (easyvfs_file* pFile);
typedef bool           (* easyvfs_deletefile_proc)       (easyvfs_archive* pArchive, const char* path);
typedef bool           (* easyvfs_rename_file_proc)      (easyvfs_archive* pArchive, const char* pathOld, const char* pathNew);
typedef bool           (* easyvfs_mkdir_proc)            (easyvfs_archive* pArchive, const char* path);
typedef bool           (* easyvfs_copy_file_proc)        (easyvfs_archive* pArchive, const char* srcPath, const char* dstPath, bool failIfExists);

typedef struct
{
    easyvfs_is_valid_archive_proc isvalidarchive;
    easyvfs_open_archive_proc     openarchive;
    easyvfs_close_archive_proc    closearchive;
    easyvfs_get_file_info_proc    getfileinfo;
    easyvfs_begin_iteration_proc  beginiteration;
    easyvfs_end_iteration_proc    enditeration;
    easyvfs_next_iteration_proc   nextiteration;
    easyvfs_open_file_proc        openfile;
    easyvfs_close_file_proc       closefile;
    easyvfs_read_file_proc        readfile;
    easyvfs_write_file_proc       writefile;
    easyvfs_seek_file_proc        seekfile;
    easyvfs_tell_file_proc        tellfile;
    easyvfs_file_size_proc        filesize;
    easyvfs_flush_file_proc       flushfile;
    easyvfs_deletefile_proc       deletefile;
    easyvfs_rename_file_proc      renamefile;
    easyvfs_mkdir_proc            mkdir;
    easyvfs_copy_file_proc        copyfile;     // <-- This is only used for intra-archive copies.

}easyvfs_archive_callbacks;


struct easyvfs_archive
{
    /// A pointer to the context that created the archive. This should never be null.
    easyvfs_context* pContext;

    /// A pointer to the archive that contains this archive. This can be null in which case it is the top level archive.
    easyvfs_archive* pParentArchive;

    /// A pointer to the file containing the data of the archive file.
    easyvfs_file* pFile;

    /// The callbacks to use when working with on the archive. This contains all of the functions for opening files, reading
    /// files, etc.
    easyvfs_archive_callbacks callbacks;

    /// The absolute, verbose path of the archive. For native archives, this will be the name of the folder on the native file
    /// system. For non-native archives (zip, etc.) this is the the path of the archive file.
    char absolutePath[EASYVFS_MAX_PATH];

    /// The user data that was returned when the archive was opened by the archive definition.
    void* pUserData;
};

struct easyvfs_file
{
    /// A pointer to the archive that contains the file. This should never be null. Retrieve a pointer to the contex from this
    /// by doing pArchive->pContext. The file containing the archives raw data can be retrieved with pArchive->pFile.
    easyvfs_archive* pArchive;

    /// The user data that was returned when the file was opened by the callback routines.
    void* pUserData;


    /// The size of the extra data.
    unsigned int extraDataSize;

    /// A pointer to the extra data.
    unsigned char pExtraData[1];
};

struct easyvfs_file_info
{
    /// The absolute path of the file.
    char absolutePath[EASYVFS_MAX_PATH];

    /// The size of the file, in bytes.
    easyvfs_uint64 sizeInBytes;

    /// The time the file was last modified.
    easyvfs_uint64 lastModifiedTime;

    /// File attributes. 
    unsigned int attributes;

    /// Padding. Unused.
    unsigned int padding4;
};

struct easyvfs_iterator
{
    /// A pointer to the archive that contains the folder being iterated.
    easyvfs_archive* pArchive;

    /// A pointer to the iterator's user data that was created when the iterator was creted.
    void* pUserData;
};


/// createcontext()
easyvfs_context* easyvfs_create_context(void);

/// deletecontext()
void easyvfs_delete_context(easyvfs_context* pContext);


/// registerarchivecallbacks()
void easyvfs_register_archive_callbacks(easyvfs_context* pContext, easyvfs_archive_callbacks callbacks);


/// Inserts a base directory at a specific priority position.
///
/// @param pContext     [in] The relevant context.
/// @param absolutePath [in] The absolute path of the base directory.
/// @param index        [in] The index to insert the path at.
///
/// @remarks
///     A lower value index means a higher priority. This must be in the range of [0, easyvfs_get_base_directory_count()].
void easyvfs_insert_base_directory(easyvfs_context* pContext, const char* absolutePath, unsigned int index);

/// Adds a base directory to the end of the list.
///
/// @remarks
///     The further down the list the base directory, the lower priority is will receive. This adds it to the end
///     which means it it given a lower priority to those that are already in the list.
///     @par
///     Use easyvfs_insert_base_directory() to insert the base directory at a specific position.
///     @par
///     Base directories must be an absolute path to a real directory.
void easyvfs_add_base_directory(easyvfs_context* pContext, const char* absolutePath);

/// Removes the given base directory.
///
/// @param pContext     [in] The context whose base directory is being removed.
/// @param absolutePath [in] The absolute path to remove.
void easyvfs_remove_base_directory(easyvfs_context* pContext, const char* absolutePath);

/// Removes the directory at the given index.
///
/// @param pContext [in] The context whose base directory is being removed.
/// @param index    [in] The index of the base directory that should be removed.
///
/// @remarks
///     If you need to remove every base directory, use easyvfs_remove_all_base_directories() since that is more efficient.
void easyvfs_remove_base_directory_by_index(easyvfs_context* pContext, unsigned int index);

/// Removes every base directory from the given context.
///
/// @param pContext [in] The context whose base directories are being removed.
void easyvfs_remove_all_base_directories(easyvfs_context* pContext);

/// Retrieves the number of base directories attached to the given context.
///
/// @param pContext [in] The context in question.
unsigned int easyvfs_get_base_directory_count(easyvfs_context* pContext);

/// Retrieves the base directory at the given index.
bool easyvfs_get_base_directory_by_index(easyvfs_context* pContext, unsigned int index, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes);


/// Sets the base directory for write operations (including delete).
///
/// @remarks
///     When doing a write operation using a relative path, the full path will be resolved using this directory as the base.
///     @par
///     If the base write directory is not set, and absolute path must be used for all write operations.
///     @par
///     If the write directory guard is enabled, all write operations that are attempted at a higher level than this directory
///     will fail.
void easyvfs_set_base_write_directory(easyvfs_context* pContext, const char* absolutePath);

/// Retrieves the base write directory.
bool easyvfs_get_base_write_directory(easyvfs_context* pContext, char* absolutePathOut, unsigned int absolutePathOutSize);

/// Enables the write directory guard.
void easyvfs_enable_write_directory_guard(easyvfs_context* pContext);

/// Disables the write directory guard.
void easyvfs_disable_write_directory_guard(easyvfs_context* pContext);

/// Determines whether or not the base directory guard is enabled.
bool easyvfs_is_write_directory_guard_enabled(easyvfs_context* pContext);


/// beginiteration()
bool easyvfs_begin_iteration(easyvfs_context* pContext, const char* path, easyvfs_iterator* iOut);

/// nextiteration()
bool easyvfs_next_iteration(easyvfs_context* pContext, easyvfs_iterator* i, easyvfs_file_info* fi);

/// enditeration()
void easyvfs_end_iteration(easyvfs_context* pContext, easyvfs_iterator* i);


/// Retrieves information about the given file.
bool easyvfs_get_file_info(easyvfs_context* pContext, const char* absoluteOrRelativePath, easyvfs_file_info* fi);

/// Finds the absolute, verbose path of the given path.
bool easyvfs_find_absolute_path(easyvfs_context* pContext, const char* path, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes);

/// Finds the absolute, verbose path of the given path, using the given path as the higest priority base path.
bool easyvfs_find_absolute_path_explicit_base(easyvfs_context* pContext, const char* path, const char* highestPriorityBasePath, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes);


/// Determines whether or not the given path refers to an archive file.
///
/// @remarks
///     This will return false if the path refers to a folder on the normal file system.
bool easyvfs_is_archive(easyvfs_context* pContext, const char* path);


/// deletefile()
///
/// Must be an absolute, verbose path in order to avoid ambiguity.
bool easyvfs_delete_file(easyvfs_context* pContext, const char* path);

/// renamefile()
///
/// Must be an absolute, verbose path in order to avoid ambiguity.
bool easyvfs_rename_file(easyvfs_context* pContext, const char* pathOld, const char* pathNew);

/// mkdir()
///
/// Must be an absolute, verbose path in order to avoid ambiguity.
bool easyvfs_mkdir(easyvfs_context* pContext, const char* path);


/// Copies a file.
bool easyvfs_copy_file(easyvfs_context* pContext, const char* srcPath, const char* dstPath, bool failIfExists);



/// Opens a file.
///
/// @param pContext               [in] A pointer ot the virtual file system context.
/// @param absoluteOrRelativePath [in] The absolute or relative path of the file to open.
/// @param accessModes            [in] Flags specifying how the file should be open. See remarks.
/// @param extraDataSize          [in] The number of extra bytes to allocate with the file for use by the application.
///
/// @remarks
///     The allowable access modes can be a combination of the following:
///      - EASYVFS_READ     - Opens the file for reading
///      - EASYVFS_WRITE    - Opens the file for writing.
///      - EASYVFS_APPEND   - When the file is opened with EASYVFS_WRITE, do not delete the original contents of the file and set the write pointer to the end of the file.
///      - EASYVFS_EXISTING - Only open the file if it exists.
///     @par
///     A file can have extra data associated with it to make it possible for the host application to associate
///     custom data with the file. The size of this extra data is specified with \c extraDataSize. A pointer to
///     the the buffer containing the extra data can be retrieved with easyvfs_get_extra_data().
easyvfs_file* easyvfs_open(easyvfs_context* pContext, const char* absoluteOrRelativePath, easyvfs_access_mode accessMode, unsigned int extraDataSize);

/// closefile()
void easyvfs_close(easyvfs_file* pFile);

/// Reads data from the given file.
bool easyvfs_read(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);

/// Writes data to the given file.
bool easyvfs_write(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);

/// Seeks the file pointer by the given number of bytes, relative to the specified origin.
bool easyvfs_seek(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin);

/// Retrieves the current position of the file pointer.
easyvfs_uint64 easyvfs_tell(easyvfs_file* pFile);

/// Retrieves the size of the given file.
easyvfs_uint64 easyvfs_file_size(easyvfs_file* pFile);

/// Flushes the given file.
void easyvfs_flush(easyvfs_file* pFile);


/// Retrieves the size of the extra data for the given file.
unsigned int easyvfs_get_extra_data_size(easyvfs_file* pFile);

/// Retrieves a pointer to the extra data for the given file.
void* easyvfs_get_extra_data(easyvfs_file* pFile);


//////////////////////////////////////
// High Level API

/// Helper function for writing a string.
bool easyvfs_write_string(easyvfs_file* pFile, const char* str);

/// Helper function for writing a string, and then inserting a new line right after it.
///
/// @remarks
///     The new line character is "\n" and NOT "\r\n".
bool easyvfs_write_line(easyvfs_file* pFile, const char* str);


/// Helper function for opening a binary file and retrieving it's data in one go.
///
/// @remarks
///     Free the returned pointer with easyvfs_free()
void* easyvfs_open_and_read_binary_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, size_t* pSizeInBytesOut);

/// Helper function for opening a text file and retrieving it's data in one go.
///
/// @remarks
///     Free the returned pointer with easyvfs_free()
///     @par
///     The returned string is null terminated. The size returned by pSizeInBytesOut does not include the null terminator.
char* easyvfs_open_and_read_text_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, size_t* pSizeInBytesOut);

/// Helper function for opening a file, writing the given data, and then closing it.
bool easyvfs_open_and_write_binary_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, const void* pData, size_t dataSize);

/// Helper function for opening a file, writing the given textual data, and then closing it.
bool easyvfs_open_and_write_text_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, const char* pTextData);


/// Helper function for determining whether or not the given path refers to an existing file or directory.
bool easyvfs_exists(easyvfs_context* pContext, const char* absoluteOrRelativePath);

/// Determines if the given path refers to an existing file (not a directory).
///
/// @remarks
///     This will return false for directories. Use easyvfs_exists() to check for either a file or directory.
bool easyvfs_is_existing_file(easyvfs_context* pContext, const char* absoluteOrRelativePath);

/// Determines if the given path refers to an existing directory.
bool easyvfs_is_existing_directory(easyvfs_context* pContext, const char* absoluteOrRelativePath);

/// Same as easyvfs_mkdir(), except creates the entire directory structure recursively.
bool easyvfs_mkdir_recursive(easyvfs_context* pContext, const char* path);

/// Determines whether or not the given file is at the end.
///
/// @remarks
///     This is just a high-level helper function equivalent to easyvfs_tell(pFile) == easyvfs_file_size(pFile).
bool easyvfs_eof(easyvfs_file* pFile);


//////////////////////////////////////
// Utilities

// malloc()
void* easyvfs_malloc(size_t sizeInBytes);

// free()
void easyvfs_free(void* p);

// memcpy()
void easyvfs_memcpy(void* dst, const void* src, size_t sizeInBytes);

// strcpy()
int easyvfs_strcpy(char* dst, size_t dstSizeInBytes, const char* src);


// ispathchild()
bool easyvfs_is_path_child(const char* childAbsolutePath, const char* parentAbsolutePath);

// is_path_descendant()
bool easyvfs_is_path_descendant(const char* descendantAbsolutePath, const char* parentAbsolutePath);

// easyvfs_copy_base_path()
bool easyvfs_copy_base_path(const char* path, char* baseOut, unsigned int baseSizeInBytes);

// easyvfs_file_name()
const char* easyvfs_file_name(const char* path);

// easyvfs_extension()
const char* easyvfs_extension(const char* path);

// easyvfs_extension_equal()
bool easyvfs_extension_equal(const char* path, const char* extension);

// easyvfs_paths_equal()
bool easyvfs_paths_equal(const char* path1, const char* path2);


// easyvfs_is_path_relative()
bool easyvfs_is_path_relative(const char* path);

// easyvfs_is_path_absolute()
bool easyvfs_is_path_absolute(const char* path);


// easyvfs_copy_and_append_path()
bool easyvfs_copy_and_append_path(char* dst, unsigned int dstSizeInBytes, const char* base, const char* other);




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
