// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#ifndef __easy_vfs_h_
#define __easy_vfs_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// Change this to the location of the easy_path header, relative to the source file. The source file will include this path with
// #include EASYPATH_HEADER. This dependency will be removed later once this library becomes a little bit more stable.
#define EASYPATH_HEADER    "../easy_path/easy_path.h"

// The maximum length of a path in bytes, including the null terminator. If a path exceeds this amount, it will be truncated and thus
// won't contain a meaningful value. When this is changed the source file will need to be recompiled. Most of the time leaving this
// at 256 is fine, but it's not a problem to increase the size if you are encountering truncation issues. Note that increasing this
// value will increase memory usage. You should not need make this any higher than 4096.
//#define EASYVFS_MAX_PATH    256
//#define EASYVFS_MAX_PATH    1024
#define EASYVFS_MAX_PATH    4096



/// The allowable access modes.
typedef enum
{
    easyvfs_read = 1,
    easyvfs_write,
    easyvfs_readwrite

}easyvfs_accessmode;

/// The allowable seeking origins.
typedef enum
{
    easyvfs_current,
    easyvfs_start,
    easyvfs_end

}easyvfs_seekorigin;

#define EASYVFS_FILE_ATTRIBUTE_DIRECTORY    0x00000001
#define EASYVFS_FILE_ATTRIBUTE_READONLY     0x00000002


typedef long long easyvfs_int64;

typedef struct easyvfs_context  easyvfs_context;
typedef struct easyvfs_archive  easyvfs_archive;
typedef struct easyvfs_file     easyvfs_file;
typedef struct easyvfs_fileinfo easyvfs_fileinfo;
typedef struct easyvfs_iterator easyvfs_iterator;


typedef int           (* easyvfs_isvalidarchive_proc) (easyvfs_context* pContext, const char* path);
typedef void*         (* easyvfs_openarchive_proc)    (easyvfs_file* pFile, easyvfs_accessmode accessMode);
typedef void          (* easyvfs_closearchive_proc)   (easyvfs_archive* pArchive);
typedef int           (* easyvfs_getfileinfo_proc)    (easyvfs_archive* pArchive, const char* path, easyvfs_fileinfo* fi);
typedef void*         (* easyvfs_beginiteration_proc) (easyvfs_archive* pArchive, const char* path);
typedef void          (* easyvfs_enditeration_proc)   (easyvfs_archive* pArchive, easyvfs_iterator* i);
typedef int           (* easyvfs_nextiteration_proc)  (easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_fileinfo* fi);
typedef void*         (* easyvfs_openfile_proc)       (easyvfs_archive* pArchive, const char* path, easyvfs_accessmode accessMode);
typedef void          (* easyvfs_closefile_proc)      (easyvfs_file* pFile);
typedef int           (* easyvfs_readfile_proc)       (easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);
typedef int           (* easyvfs_writefile_proc)      (easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);
typedef easyvfs_int64 (* easyvfs_seekfile_proc)       (easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seekorigin origin);
typedef easyvfs_int64 (* easyvfs_tellfile_proc)       (easyvfs_file* pFile);
typedef easyvfs_int64 (* easyvfs_filesize_proc)       (easyvfs_file* pFile);
typedef int           (* easyvfs_deletefile_proc)     (easyvfs_archive* pArchive, const char* path);
typedef int           (* easyvfs_renamefile_proc)     (easyvfs_archive* pArchive, const char* pathOld, const char* pathNew);
typedef int           (* easyvfs_mkdir_proc)          (easyvfs_archive* pArchive, const char* path);

typedef struct
{
    easyvfs_isvalidarchive_proc isvalidarchive;
    easyvfs_openarchive_proc    openarchive;
    easyvfs_closearchive_proc   closearchive;
    easyvfs_getfileinfo_proc    getfileinfo;
    easyvfs_beginiteration_proc beginiteration;
    easyvfs_enditeration_proc   enditeration;
    easyvfs_nextiteration_proc  nextiteration;
    easyvfs_openfile_proc       openfile;
    easyvfs_closefile_proc      closefile;
    easyvfs_readfile_proc       readfile;
    easyvfs_writefile_proc      writefile;
    easyvfs_seekfile_proc       seekfile;
    easyvfs_tellfile_proc       tellfile;
    easyvfs_filesize_proc       filesize;
    easyvfs_deletefile_proc     deletefile;
    easyvfs_renamefile_proc     renamefile;
    easyvfs_mkdir_proc          mkdir;

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
};

struct easyvfs_fileinfo
{
    /// The absolute path of the file.
    char absolutePath[EASYVFS_MAX_PATH];

    /// The size of the file, in bytes.
    easyvfs_int64 sizeInBytes;

    /// The time the file was last modified.
    easyvfs_int64 lastModifiedTime;

    /// File attributes. 
    unsigned int attributes;
};

struct easyvfs_iterator
{
    /// A pointer to the archive that contains the folder being iterated.
    easyvfs_archive* pArchive;

    /// A pointer to the iterator's user data that was created when the iterator was creted.
    void* pUserData;
};


/// createcontext()
easyvfs_context* easyvfs_createcontext();

/// deletecontext()
void easyvfs_deletecontext(easyvfs_context* pContext);


/// registerarchivecallbacks()
void easyvfs_registerarchivecallbacks(easyvfs_context* pContext, easyvfs_archive_callbacks callbacks);


/// Inserts a base directory at a specific priority position.
///
/// @param pContext     [in] The relevant context.
/// @param absolutePath [in] The absolute path of the base directory.
/// @param index        [in] The index to insert the path at.
///
/// @remarks
///     A lower value index means a higher priority. This must be in the range of [0, easyvfs_basedirectorycount()].
void easyvfs_insertbasedirectory(easyvfs_context* pContext, const char* absolutePath, unsigned int index);

/// Adds a base directory to the end of the list.
///
/// @remarks
///     The further down the list the base directory, the lower priority is will receive. This adds it to the end
///     which means it it given a lower priority to those that are already in the list.
///     @par
///     Use easyvfs_insertbasedirectory() to insert the base directory at a specific position.
///     @par
///     Base directories must be an absolute path to a real directory.
void easyvfs_addbasedirectory(easyvfs_context* pContext, const char* absolutePath);

/// Removes the given base directory.
///
/// @param pContext     [in] The context whose base directory is being removed.
/// @param absolutePath [in] The absolute path to remove.
void easyvfs_removebasedirectory(easyvfs_context* pContext, const char* absolutePath);

/// Removes the directory at the given index.
///
/// @param pContext [in] The context whose base directory is being removed.
/// @param index    [in] The index of the base directory that should be removed.
///
/// @remarks
///     If you need to remove every base directory, use easyvfs_removeallbasedirectories() since that is more efficient.
void easyvfs_removebasedirectorybyindex(easyvfs_context* pContext, unsigned int index);

/// Removes every base directory from the given context.
///
/// @param pContext [in] The context whose base directories are being removed.
void easyvfs_removeallbasedirectories(easyvfs_context* pContext);

/// Retrieves the number of base directories attached to the given context.
///
/// @param pContext [in] The context in question.
unsigned int easyvfs_basedirectorycount(easyvfs_context* pContext);

/// Retrieves the base directory at the given index.
int easyvfs_getbasedirectorybyindex(easyvfs_context* pContext, unsigned int index, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes);


/// beginiteration()
int easyvfs_beginiteration(easyvfs_context* pContext, const char* path, easyvfs_iterator* iOut);

/// nextiteration()
int easyvfs_nextiteration(easyvfs_context* pContext, easyvfs_iterator* i, easyvfs_fileinfo* fi);

/// enditeration()
void easyvfs_enditeration(easyvfs_context* pContext, easyvfs_iterator* i);


/// Retrieves information about the given file.
int easyvfs_getfileinfo(easyvfs_context* pContext, const char* absolutePath, easyvfs_fileinfo* fi);

/// Finds the absolute, verbose path of the given path.
int easyvfs_findabsolutepath(easyvfs_context* pContext, const char* path, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes);


/// deletefile()
///
/// Must be an absolute, verbose path in order to avoid ambiguity.
int easyvfs_deletefile(easyvfs_context* pContext, const char* path);

/// renamefile()
///
/// Must be an absolute, verbose path in order to avoid ambiguity.
int easyvfs_renamefile(easyvfs_context* pContext, const char* pathOld, const char* pathNew);

/// mkdir()
///
/// Must be an absolute, verbose path in order to avoid ambiguity.
int easyvfs_mkdir(easyvfs_context* pContext, const char* path);



/// openfile()
easyvfs_file* easyvfs_openfile(easyvfs_context* pContext, const char* absoluteOrRelativePath, easyvfs_accessmode accessMode);

/// closefile()
void easyvfs_closefile(easyvfs_file* pFile);

/// Reads data from the given file.
int easyvfs_readfile(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);

/// Writes data to the given file.
int easyvfs_writefile(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);

/// Seeks the file pointer by the given number of bytes, relative to the specified origin.
easyvfs_int64 easyvfs_seekfile(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seekorigin origin);

/// Retrieves the current position of the file pointer.
easyvfs_int64 easyvfs_tellfile(easyvfs_file* pFile);

/// Retrieves the size of the given file.
easyvfs_int64 easyvfs_filesize(easyvfs_file* pFile);



//////////////////////////////////////
// Utilities

// malloc()
void* easyvfs_malloc(size_t sizeInBytes);

// free()
void easyvfs_free(void* p);

// memcpy()
void easyvfs_memcpy(void* dst, const void* src, size_t sizeInBytes);

// strcpy()
void easyvfs_strcpy(char* dst, size_t dstSizeInBytes, const char* src);

// filename()
const char* easyvfs_filename(const char* path);

// extension()
const char* easyvfs_extension(const char* path);

// extensionequal()
int easyvfs_extensionequal(const char* path, const char* extension);

// copyandappendpath()
int easyvfs_copyandappendpath(char* dst, unsigned int dstSizeInBytes, const char* base, const char* other);


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