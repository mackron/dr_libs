// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#include "easy_vfs_pak.h"
#include "../easy_vfs.h"

#include <assert.h>
#include <string.h>


typedef struct
{
    char path[64];

}easyvfs_path_pak;

typedef struct
{
    /// The file name.
    char name[56];

    /// The position within the file of the first byte of the file.
    unsigned int offset;

    /// The size of the file, in bytes.
    unsigned int sizeInBytes;

}easyvfs_file_pak;

typedef struct
{
    /// The 4-byte identifiers: "PACK"
    char id[4];

    /// The offset of the directory.
    unsigned int directoryOffset;

    /// The size of the directory. This should a multiple of 64.
    unsigned int directoryLength;


    /// The access mode.
    easyvfs_accessmode accessMode;

    /// A pointer to the buffer containing the file information. The number of items in this array is equal to directoryLength / 64.
    easyvfs_file_pak* pFiles;

}easyvfs_archive_pak;


typedef struct
{
    /// The current index of the iterator. When this hits childCount, the iteration is finished.
    unsigned int index;

    /// The directory being iterated.
    char directoryPath[EASYVFS_MAX_PATH];


    /// The number of directories that have previously been iterated.
    unsigned int processedDirCount;

    /// The directories that were previously iterated.
    easyvfs_path_pak* pProcessedDirs;

}easyvfs_iterator_pak;

easyvfs_bool easyvfs_iterator_pak_append_processed_dir(easyvfs_iterator_pak* pIterator, const char* path)
{
    if (pIterator != NULL && path != NULL)
    {
        easyvfs_path_pak* pOldBuffer = pIterator->pProcessedDirs;
        easyvfs_path_pak* pNewBuffer = easyvfs_malloc(sizeof(easyvfs_path_pak) * (pIterator->processedDirCount + 1));

        if (pNewBuffer != 0)
        {
            for (unsigned int iDst = 0; iDst < pIterator->processedDirCount; ++iDst)
            {
                pNewBuffer[iDst] = pOldBuffer[iDst];
            }

            easyvfs_strcpy(pNewBuffer[pIterator->processedDirCount].path, 64, path);


            pIterator->pProcessedDirs     = pNewBuffer;
            pIterator->processedDirCount += 1;

            easyvfs_free(pOldBuffer);

            return 1;
        }
    }

    return 0;
}

easyvfs_bool easyvfs_iterator_pak_has_dir_been_processed(easyvfs_iterator_pak* pIterator, const char* path)
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
    /// The offset of the first byte of the file's data.
    size_t offsetInArchive;

    /// The size of the file in bytes so we can guard against overflowing reads.
    size_t sizeInBytes;

    /// The current position of the file's read pointer.
    size_t readPointer;

}easyvfs_openedfile_pak;



easyvfs_archive_pak* easyvfs_pak_create(easyvfs_accessmode accessMode)
{
    easyvfs_archive_pak* pak = easyvfs_malloc(sizeof(easyvfs_archive_pak));
    if (pak != NULL)
    {
        pak->directoryOffset = 0;
        pak->directoryLength = 0;
        pak->accessMode      = accessMode;
        pak->pFiles          = NULL;
    }

    return pak;
}

void easyvfs_pak_delete(easyvfs_archive_pak* pArchive)
{
    easyvfs_free(pArchive->pFiles);
    easyvfs_free(pArchive);
}



int            easyvfs_isvalidarchive_pak(easyvfs_context* pContext, const char* path);
void*          easyvfs_openarchive_pak   (easyvfs_file* pFile, easyvfs_accessmode accessMode);
void           easyvfs_closearchive_pak  (easyvfs_archive* pArchive);
int            easyvfs_getfileinfo_pak   (easyvfs_archive* pArchive, const char* path, easyvfs_fileinfo *fi);
void*          easyvfs_beginiteration_pak(easyvfs_archive* pArchive, const char* path);
void           easyvfs_enditeration_pak  (easyvfs_archive* pArchive, easyvfs_iterator* i);
int            easyvfs_nextiteration_pak (easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_fileinfo* fi);
void*          easyvfs_openfile_pak      (easyvfs_archive* pArchive, const char* path, easyvfs_accessmode accessMode);
void           easyvfs_closefile_pak     (easyvfs_file* pFile);
int            easyvfs_readfile_pak      (easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);
int            easyvfs_writefile_pak     (easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);
easyvfs_bool   easyvfs_seekfile_pak      (easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seekorigin origin);
easyvfs_uint64 easyvfs_tellfile_pak      (easyvfs_file* pFile);
easyvfs_uint64 easyvfs_filesize_pak      (easyvfs_file* pFile);
int            easyvfs_deletefile_pak    (easyvfs_archive* pArchive, const char* path);
int            easyvfs_renamefile_pak    (easyvfs_archive* pArchive, const char* pathOld, const char* pathNew);
int            easyvfs_mkdir_pak         (easyvfs_archive* pArchive, const char* path);

void easyvfs_registerarchivecallbacks_pak(easyvfs_context* pContext)
{
    easyvfs_archive_callbacks callbacks;
    callbacks.isvalidarchive = easyvfs_isvalidarchive_pak;
    callbacks.openarchive    = easyvfs_openarchive_pak;
    callbacks.closearchive   = easyvfs_closearchive_pak;
    callbacks.getfileinfo    = easyvfs_getfileinfo_pak;
    callbacks.beginiteration = easyvfs_beginiteration_pak;
    callbacks.enditeration   = easyvfs_enditeration_pak;
    callbacks.nextiteration  = easyvfs_nextiteration_pak;
    callbacks.openfile       = easyvfs_openfile_pak;
    callbacks.closefile      = easyvfs_closefile_pak;
    callbacks.readfile       = easyvfs_readfile_pak;
    callbacks.writefile      = easyvfs_writefile_pak;
    callbacks.seekfile       = easyvfs_seekfile_pak;
    callbacks.tellfile       = easyvfs_tellfile_pak;
    callbacks.filesize       = easyvfs_filesize_pak;
    callbacks.deletefile     = easyvfs_deletefile_pak;
    callbacks.renamefile     = easyvfs_renamefile_pak;
    callbacks.mkdir          = easyvfs_mkdir_pak;
    easyvfs_registerarchivecallbacks(pContext, callbacks);
}


int easyvfs_isvalidarchive_pak(easyvfs_context* pContext, const char* path)
{
    (void)pContext;

    if (easyvfs_extension_equal(path, "pak"))
    {
        return 1;
    }

    return 0;
}


void* easyvfs_openarchive_pak(easyvfs_file* pFile, easyvfs_accessmode accessMode)
{
    assert(pFile != NULL);
    assert(easyvfs_tellfile(pFile) == 0);

    easyvfs_archive_pak* pak = easyvfs_pak_create(accessMode);
    if (pak != NULL)
    {
        // First 4 bytes should equal "PACK"
        if (easyvfs_readfile(pFile, pak->id, 4, NULL))
        {
            if (pak->id[0] == 'P' && pak->id[1] == 'A' && pak->id[2] == 'C' && pak->id[3] == 'K')
            {
                if (easyvfs_readfile(pFile, &pak->directoryOffset, 4, NULL))
                {
                    if (easyvfs_readfile(pFile, &pak->directoryLength, 4, NULL))
                    {
                        // We loaded the header just fine so now we want to allocate space for each file in the directory and load them. Note that
                        // this does not load the file data itself, just information about the files like their name and size.
                        if (pak->directoryLength % 64 == 0)
                        {
                            unsigned int fileCount = pak->directoryLength / 64;
                            if (fileCount > 0)
                            {
                                assert((sizeof(easyvfs_file_pak) * fileCount) == pak->directoryLength);

                                pak->pFiles = easyvfs_malloc(pak->directoryLength);
                                if (pak->pFiles != NULL)
                                {
                                    // Seek to the directory listing before trying to read it.
                                    if (easyvfs_seekfile(pFile, pak->directoryOffset, easyvfs_start))
                                    {
                                        unsigned int bytesRead;
                                        if (easyvfs_readfile(pFile, pak->pFiles, pak->directoryLength, &bytesRead) && bytesRead == pak->directoryLength)
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

void easyvfs_closearchive_pak(easyvfs_archive* pArchive)
{
    assert(pArchive != NULL);
    assert(pArchive->pUserData != NULL);

    easyvfs_archive_pak* pUserData = pArchive->pUserData;
    if (pUserData != NULL)
    {
        easyvfs_pak_delete(pUserData);
    }
}

int easyvfs_getfileinfo_pak(easyvfs_archive* pArchive, const char* path, easyvfs_fileinfo* fi)
{
    assert(pArchive != NULL);
    assert(pArchive->pUserData != NULL);

    // We can determine whether or not the path refers to a file or folder by checking it the path is parent of any
    // files in the archive. If so, it's a folder, otherwise it's a file (so long as it exists).
    easyvfs_archive_pak* pak = pArchive->pUserData;
    if (pak != NULL)
    {
        unsigned int fileCount = pak->directoryLength / 64;
        for (unsigned int i = 0; i < fileCount; ++i)
        {
            easyvfs_file_pak* pFile = pak->pFiles + i;
            if (strcmp(pFile->name, path) == 0)
            {
                // It's a file.
                easyvfs_copy_and_append_path(fi->absolutePath, EASYVFS_MAX_PATH, pArchive->absolutePath, path);
                fi->sizeInBytes      = (easyvfs_uint64)pFile->sizeInBytes;
                fi->lastModifiedTime = 0;
                fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;

                return 1;
            }
            else if (easyvfs_is_path_descendant(pFile->name, path))
            {
                // It's a directory.
                easyvfs_copy_and_append_path(fi->absolutePath, EASYVFS_MAX_PATH, pArchive->absolutePath, path);
                fi->sizeInBytes      = 0;
                fi->lastModifiedTime = 0;
                fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY | EASYVFS_FILE_ATTRIBUTE_DIRECTORY;

                return 1;
            }
        }
    }

    return 0;
}

void* easyvfs_beginiteration_pak(easyvfs_archive* pArchive, const char* path)
{
    assert(pArchive != 0);
    assert(pArchive->pUserData != NULL);
    assert(path != NULL);

    easyvfs_iterator_pak* pIterator = easyvfs_malloc(sizeof(easyvfs_iterator_pak));
    if (pIterator != NULL)
    {
        pIterator->index = 0;
        easyvfs_strcpy(pIterator->directoryPath, EASYVFS_MAX_PATH, path);
        pIterator->processedDirCount = 0;
        pIterator->pProcessedDirs    = NULL;
    }

    return pIterator;
}

void easyvfs_enditeration_pak(easyvfs_archive* pArchive, easyvfs_iterator* i)
{
    assert(pArchive != 0);
    assert(pArchive->pUserData != NULL);
    assert(i != NULL);

    easyvfs_free(i->pUserData);
    i->pUserData = NULL;
}

int easyvfs_nextiteration_pak(easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_fileinfo* fi)
{
    assert(pArchive != 0);
    assert(i != NULL);

    easyvfs_iterator_pak* pIterator = i->pUserData;
    if (pIterator != NULL)
    {
        easyvfs_archive_pak* pak = pArchive->pUserData;
        if (pak != NULL)
        {
            unsigned int fileCount = pak->directoryLength / 64;
            while (pIterator->index < fileCount)
            {
                unsigned int iFile = pIterator->index++;

                easyvfs_file_pak* pFile = pak->pFiles + iFile;
                if (easyvfs_is_path_child(pFile->name, pIterator->directoryPath))
                {
                    // It's a file.
                    easyvfs_strcpy(fi->absolutePath, EASYVFS_MAX_PATH, pFile->name);
                    fi->sizeInBytes      = (easyvfs_uint64)pFile->sizeInBytes;
                    fi->lastModifiedTime = 0;
                    fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;

                    return 1;
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
                        easyvfs_strcpy(fi->absolutePath, EASYVFS_MAX_PATH, childDir);
                        fi->sizeInBytes      = 0;
                        fi->lastModifiedTime = 0;
                        fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY | EASYVFS_FILE_ATTRIBUTE_DIRECTORY;

                        easyvfs_iterator_pak_append_processed_dir(pIterator, childDir);

                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

void* easyvfs_openfile_pak(easyvfs_archive* pArchive, const char* path, easyvfs_accessmode accessMode)
{
    assert(pArchive != 0);
    assert(pArchive->pUserData != NULL);
    assert(path != NULL);

    // Only supporting read-only for now.
    if ((accessMode & easyvfs_write) != 0)
    {
        return NULL;
    }


    easyvfs_archive_pak* pak = pArchive->pUserData;
    if (pak != NULL)
    {
        for (unsigned int iFile = 0; iFile < (pak->directoryLength / 64); ++iFile)
        {
            if (strcmp(path, pak->pFiles[iFile].name) == 0)
            {
                // We found the file.
                easyvfs_openedfile_pak* pOpenedFile = easyvfs_malloc(sizeof(easyvfs_openedfile_pak));
                if (pOpenedFile != NULL)
                {
                    pOpenedFile->offsetInArchive = pak->pFiles[iFile].offset;
                    pOpenedFile->sizeInBytes     = pak->pFiles[iFile].sizeInBytes;
                    pOpenedFile->readPointer     = 0;

                    return pOpenedFile;
                }
            }
        }
    }

   
    return NULL;
}

void easyvfs_closefile_pak(easyvfs_file* pFile)
{
    assert(pFile != 0);

    easyvfs_openedfile_pak* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        easyvfs_free(pOpenedFile);
    }
}

int easyvfs_readfile_pak(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut)
{
    assert(pFile != 0);
    assert(dst != NULL);
    assert(bytesToRead > 0);

    easyvfs_openedfile_pak* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        easyvfs_file* pArchiveFile = pFile->pArchive->pFile;
        assert(pArchiveFile != NULL);

        easyvfs_int64 bytesAvailable = pOpenedFile->sizeInBytes - pOpenedFile->readPointer;
        if (bytesAvailable < bytesToRead) {
            bytesToRead = (unsigned int)bytesAvailable;     // Safe cast, as per the check above.
        }

        easyvfs_seekfile(pArchiveFile, (easyvfs_int64)(pOpenedFile->offsetInArchive + pOpenedFile->readPointer), easyvfs_start);
        int result = easyvfs_readfile(pArchiveFile, dst, bytesToRead, bytesReadOut);
        if (result != 0)
        {
            pOpenedFile->readPointer += bytesToRead;
        }

        return result;
    }

    return 0;
}

int easyvfs_writefile_pak(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut)
{
    assert(pFile != 0);
    assert(src != NULL);
    assert(bytesToWrite > 0);

    // All files are read-only for now.
    if (bytesWrittenOut != NULL)
    {
        *bytesWrittenOut = 0;
    }

    return 0;
}

easyvfs_bool easyvfs_seekfile_pak(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seekorigin origin)
{
    assert(pFile != 0);

    easyvfs_openedfile_pak* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        easyvfs_uint64 newPos = pOpenedFile->readPointer;
        if (origin == easyvfs_current)
        {
            if ((easyvfs_int64)newPos + bytesToSeek >= 0)
            {
                newPos = (easyvfs_uint64)((easyvfs_int64)newPos + bytesToSeek);
            }
            else
            {
                // Trying to seek to before the beginning of the file.
                return 0;
            }
        }
        else if (origin == easyvfs_start)
        {
            assert(bytesToSeek >= 0);
            newPos = (easyvfs_uint64)bytesToSeek;
        }
        else if (origin == easyvfs_end)
        {
            assert(bytesToSeek >= 0);
            if ((easyvfs_uint64)bytesToSeek <= pOpenedFile->sizeInBytes)
            {
                newPos = pOpenedFile->sizeInBytes - (easyvfs_uint64)bytesToSeek;
            }
            else
            {
                // Trying to seek to before the beginning of the file.
                return 0;
            }
        }
        else
        {
            // Should never get here.
            return 0;
        }


        if (newPos > pOpenedFile->sizeInBytes)
        {
            return 0;
        }

        pOpenedFile->readPointer = (size_t)newPos;
        return 1;
    }

    return 0;
}

easyvfs_uint64 easyvfs_tellfile_pak(easyvfs_file* pFile)
{
    assert(pFile != 0);

    easyvfs_openedfile_pak* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        return pOpenedFile->readPointer;
    }

    return 0;
}

easyvfs_uint64 easyvfs_filesize_pak(easyvfs_file* pFile)
{
    assert(pFile != 0);

    easyvfs_openedfile_pak* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        return pOpenedFile->sizeInBytes;
    }

    return 0;
}

int easyvfs_deletefile_pak(easyvfs_archive* pArchive, const char* path)
{
    assert(pArchive != 0);
    assert(path     != 0);

    // All files are read-only for now.
    return 0;
}

int easyvfs_renamefile_pak(easyvfs_archive* pArchive, const char* pathOld, const char* pathNew)
{
    assert(pArchive != 0);
    assert(pathOld  != 0);
    assert(pathNew  != 0);

    // All files are read-only for now.
    return 0;
}

int easyvfs_mkdir_pak(easyvfs_archive* pArchive, const char* path)
{
    assert(pArchive != 0);
    assert(path     != 0);

    // All files are read-only for now.
    return 0;
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
