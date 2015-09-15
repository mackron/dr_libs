// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#include "easy_vfs.h"

#if EASYVFS_USE_EASYPATH
#include EASYPATH_HEADER
#endif

#include <assert.h>

#if defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif


#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)
#include <windows.h>

void* easyvfs_malloc(size_t sizeInBytes)
{
    return HeapAlloc(GetProcessHeap(), 0, sizeInBytes);
}
void easyvfs_free(void* p)
{
    HeapFree(GetProcessHeap(), 0, p);
}
void easyvfs_memcpy(void* dst, const void* src, size_t sizeInBytes)
{
    CopyMemory(dst, src, sizeInBytes);
}
#endif

void easyvfs_strcpy(char* dst, size_t dstSizeInBytes, const char* src)
{
#if defined(_MSC_VER)
    strcpy_s(dst, dstSizeInBytes, src);
#else
    while (dstSizeInBytes > 0 && src[0] != '\0')
    {
        dst[0] = src[0];

        dst += 1;
        src += 1;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes > 0)
    {
        dst[0] = '\0';
    }
#endif
}

void easyvfs_strcpy2(char* dst, unsigned int dstSizeInBytes, const char* src, unsigned int srcSizeInBytes)
{
    while (dstSizeInBytes > 0 && src[0] != '\0' && srcSizeInBytes > 0)
    {
        dst[0] = src[0];

        dst += 1;
        src += 1;
        dstSizeInBytes -= 1;
        srcSizeInBytes -= 1;
    }

    if (dstSizeInBytes > 0)
    {
        dst[0] = '\0';
    }
}


#if !EASYVFS_USE_EASYPATH
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

easyvfs_pathiterator easyvfs_beginpathiteration(const char* path)
{
    easyvfs_pathiterator i;
    i.path = path;
    i.segment.offset = 0;
    i.segment.length = 0;

    return i;
}

int easyvfs_nextpathsegment(easyvfs_pathiterator* i)
{
    if (i != 0 && i->path != 0)
    {
        i->segment.offset = i->segment.offset + i->segment.length;
        i->segment.length = 0;

        while (i->path[i->segment.offset] != '\0' && (i->path[i->segment.offset] == '/' || i->path[i->segment.offset] == '\\'))
        {
            i->segment.offset += 1;
        }

        if (i->path[i->segment.offset] == '\0')
        {
            return 0;
        }



        while (i->path[i->segment.offset + i->segment.length] != '\0' && (i->path[i->segment.offset + i->segment.length] != '/' && i->path[i->segment.offset + i->segment.length] != '\\'))
        {
            i->segment.length += 1;
        }

        return 1;
    }

    return 0;
}

int easyvfs_atendofpath(easyvfs_pathiterator i)
{
    return !easyvfs_nextpathsegment(&i);
}

int easyvfs_pathsegments_equal(const char* s0Path, const easyvfs_pathsegment s0, const char* s1Path, const easyvfs_pathsegment s1)
{
    if (s0Path != 0 && s1Path != 0)
    {
        if (s0.length == s1.length)
        {
            for (int i = 0; i < s0.length; ++i)
            {
                if (s0Path[s0.offset + i] != s1Path[s1.offset + i])
                {
                    return 0;
                }
            }

            return 1;
        }
    }

    return 0;
}

int easyvfs_pathiterators_equal(const easyvfs_pathiterator i0, const easyvfs_pathiterator i1)
{
    return easyvfs_pathsegments_equal(i0.path, i0.segment, i1.path, i1.segment);
}

int easyvfs_appendpathiterator(char* base, unsigned int baseBufferSizeInBytes, easyvfs_pathiterator i)
{
    if (base != 0)
    {
        unsigned int path1Length = (unsigned int)strlen(base);
        unsigned int path2Length = (unsigned int)i.segment.length;

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

            easyvfs_strcpy2(base + path1Length, baseBufferSizeInBytes - path1Length, i.path + i.segment.offset, path2Length);


            return 1;
        }
    }

    return 0;
}
#else
typedef struct easypath_iterator easyvfs_pathiterator;

easyvfs_pathiterator easyvfs_beginpathiteration(const char* path)
{
    return easypath_begin(path);
}

int easyvfs_nextpathsegment(easyvfs_pathiterator* i)
{
    return easypath_next(i);
}

int easyvfs_atendofpath(easyvfs_pathiterator i)
{
    return easypath_atend(i);
}

int easyvfs_pathiterators_equal(const easyvfs_pathiterator i0, const easyvfs_pathiterator i1)
{
    return easypath_iterators_equal(i0, i1);
}

int easyvfs_appendpathiterator(char* base, unsigned int baseBufferSizeInBytes, easyvfs_pathiterator i)
{
    return easypath_appenditerator(base, baseBufferSizeInBytes, i);
}
#endif


typedef struct
{
    /// The absolute path of the base path.
    char absolutePath[EASYVFS_MAX_PATH];

}easyvfs_basepath;

typedef struct
{
    /// A pointer to the buffer containing the list of base paths.
    easyvfs_basepath* pBuffer;

    /// The size of the buffer, in easyvfs_basepath's.
    unsigned int bufferSize;

    /// The number of items in the list.
    unsigned int count;

}easyvfs_basepaths;

int easyvfs_basepaths_init(easyvfs_basepaths* pBasePaths)
{
    if (pBasePaths != 0)
    {
        pBasePaths->pBuffer    = 0;
        pBasePaths->bufferSize = 0;
        pBasePaths->count      = 0;

        return 1;
    }

    return 0;
}

void easyvfs_basepaths_uninit(easyvfs_basepaths* pBasePaths)
{
    if (pBasePaths != 0)
    {
        easyvfs_free(pBasePaths->pBuffer);
    }
}

int easyvfs_basepaths_inflateandinsert(easyvfs_basepaths* pBasePaths, const char* absolutePath, unsigned int index)
{
    if (pBasePaths != 0)
    {
        unsigned int newBufferSize = (pBasePaths->bufferSize == 0) ? 2 : pBasePaths->bufferSize*2;

        easyvfs_basepath* pOldBuffer = pBasePaths->pBuffer;
        easyvfs_basepath* pNewBuffer = easyvfs_malloc(newBufferSize * sizeof(easyvfs_basepath));
        if (pNewBuffer != 0)
        {
            for (unsigned int iDst = 0; iDst < index; ++iDst)
            {
                easyvfs_memcpy(pNewBuffer + iDst, pOldBuffer + iDst, sizeof(easyvfs_basepath));
            }

            easyvfs_strcpy((pNewBuffer + index)->absolutePath, EASYVFS_MAX_PATH, absolutePath);

            for (unsigned int iDst = index; iDst < pBasePaths->count; ++iDst)
            {
                easyvfs_memcpy(pNewBuffer + iDst + 1, pOldBuffer + iDst, sizeof(easyvfs_basepath));
            }


            pBasePaths->pBuffer    = pNewBuffer;
            pBasePaths->bufferSize = newBufferSize;
            pBasePaths->count     += 1;

            easyvfs_free(pOldBuffer);
            return 1;
        }
    }

    return 0;
}

int easyvfs_basepaths_movedown1slot(easyvfs_basepaths* pBasePaths, unsigned int index)
{
    if (pBasePaths != 0)
    {
        if (pBasePaths->count < pBasePaths->bufferSize)
        {
            for (unsigned int iDst = pBasePaths->count; iDst > index; --iDst)
            {
                easyvfs_memcpy(pBasePaths->pBuffer + iDst, pBasePaths->pBuffer + iDst - 1, sizeof(easyvfs_basepath));
            }

            return 1;
        }
    }

    return 0;
}

int easyvfs_basepaths_insert(easyvfs_basepaths* pBasePaths, const char* absolutePath, unsigned int index)
{
    if (pBasePaths != 0)
    {
        if (index <= pBasePaths->count)
        {
            if (pBasePaths->count == pBasePaths->bufferSize)
            {
                return easyvfs_basepaths_inflateandinsert(pBasePaths, absolutePath, index);
            }
            else
            {
                if (easyvfs_basepaths_movedown1slot(pBasePaths, index))
                {
                    easyvfs_strcpy((pBasePaths->pBuffer + index)->absolutePath, EASYVFS_MAX_PATH, absolutePath);
                    pBasePaths->count += 1;

                    return 1;
                }
            }
        }
    }

    return 0;
}

int easyvfs_basepaths_remove(easyvfs_basepaths* pBasePaths, unsigned int index)
{
    if (pBasePaths != 0)
    {
        if (index < pBasePaths->count)
        {
            assert(pBasePaths->count > 0);

            for (unsigned int iDst = index; iDst < pBasePaths->count - 1; ++iDst)
            {
                easyvfs_memcpy(pBasePaths->pBuffer + iDst, pBasePaths->pBuffer + iDst + 1, sizeof(easyvfs_basepath));
            }

            pBasePaths->count -= 1;

            return 1;
        }
    }

    return 0;
}

void easyvfs_basepaths_clear(easyvfs_basepaths* pBasePaths)
{
    if (pBasePaths != 0)
    {
        easyvfs_basepaths_uninit(pBasePaths);
        easyvfs_basepaths_init(pBasePaths);
    }
}




typedef struct
{
    /// A pointer to the buffer containing the list of base paths.
    easyvfs_archive_callbacks* pBuffer;

    /// The number of items in the list and buffer. This is a tightly packed list so the size of the buffer is always the
    /// same as the count.
    unsigned int count;

}easyvfs_callbacklist;

int easyvfs_callbacklist_init(easyvfs_callbacklist* pList)
{
    if (pList != 0)
    {
        pList->pBuffer = 0;
        pList->count   = 0;

        return 1;
    }

    return 0;
}

void easyvfs_callbacklist_uninit(easyvfs_callbacklist* pList)
{
    if (pList != 0)
    {
        easyvfs_free(pList->pBuffer);
    }
}

int easyvfs_callbacklist_inflate(easyvfs_callbacklist* pList)
{
    if (pList != 0)
    {
        easyvfs_archive_callbacks* pOldBuffer = pList->pBuffer;
        easyvfs_archive_callbacks* pNewBuffer = easyvfs_malloc((pList->count + 1) * sizeof(easyvfs_archive_callbacks));
        if (pNewBuffer != 0)
        {
            for (unsigned int iDst = 0; iDst < pList->count; ++iDst)
            {
                easyvfs_memcpy(pNewBuffer + iDst, pOldBuffer + iDst, sizeof(easyvfs_archive_callbacks));
            }


            pList->pBuffer = pNewBuffer;


            return 1;
        }
    }

    return 0;
}

int easyvfs_callbacklist_pushback(easyvfs_callbacklist* pList, easyvfs_archive_callbacks callbacks)
{
    if (pList != 0)
    {
        easyvfs_callbacklist_inflate(pList);

        pList->pBuffer[pList->count] = callbacks;
        pList->count  += 1;
    }

    return 0;
}

void easyvfs_callbacklist_clear(easyvfs_callbacklist* pList)
{
    if (pList != 0)
    {
        easyvfs_callbacklist_uninit(pList);
        easyvfs_callbacklist_init(pList);
    }
}




struct easyvfs_context
{
    /// The list of archive callbacks which are used for loading non-native archives. This does not include the native callbacks.
    easyvfs_callbacklist archiveCallbacks;

    /// The list of base directories.
    easyvfs_basepaths baseDirectories;

    /// The callbacks for native archives. These callbacks are implemented down the bottom of this file, in the platform-specific section.
    easyvfs_archive_callbacks nativeCallbacks;
};


////////////////////////////////////////
// Private API

/// When a native archive is created, the user data will be a pointer to an object of this structure.
typedef struct
{
    /// The access mode.
    easyvfs_accessmode accessMode;

}easyvfs_native_archive;


/// Native implementations for archive callbacks.
int            easyvfs_isvalidarchive_impl_native(easyvfs_context* pContext, const char* path);
void*          easyvfs_openarchive_impl_native   (easyvfs_file* pFile, easyvfs_accessmode accessMode);
void           easyvfs_closearchive_impl_native  (easyvfs_archive* pArchive);
int            easyvfs_getfileinfo_impl_native   (easyvfs_archive* pArchive, const char* path, easyvfs_fileinfo *fi);
void*          easyvfs_beginiteration_impl_native(easyvfs_archive* pArchive, const char* path);
void           easyvfs_enditeration_impl_native  (easyvfs_archive* pArchive, easyvfs_iterator* i);
int            easyvfs_nextiteration_impl_native (easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_fileinfo* fi);
void*          easyvfs_openfile_impl_native      (easyvfs_archive* pArchive, const char* path, easyvfs_accessmode accessMode);
void           easyvfs_closefile_impl_native     (easyvfs_file* pFile);
int            easyvfs_readfile_impl_native      (easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);
int            easyvfs_writefile_impl_native     (easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);
easyvfs_bool   easyvfs_seekfile_impl_native      (easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seekorigin origin);
easyvfs_uint64 easyvfs_tellfile_impl_native      (easyvfs_file* pFile);
easyvfs_uint64 easyvfs_filesize_impl_native      (easyvfs_file* pFile);
int            easyvfs_deletefile_impl_native    (easyvfs_archive* pArchive, const char* path);
int            easyvfs_renamefile_impl_native    (easyvfs_archive* pArchive, const char* pathOld, const char* pathNew);
int            easyvfs_mkdir_impl_native         (easyvfs_archive* pArchive, const char* path);


easyvfs_file* easyvfs_archive_openfile(easyvfs_archive* pArchive, const char* path, easyvfs_accessmode accessMode);
void easyvfs_archive_closefile(easyvfs_archive* pArchive, easyvfs_file* pFile);
void easyvfs_closearchive(easyvfs_archive* pArchive);


int easyvfs_archive_beginiteration(easyvfs_archive* pArchive, const char* path, easyvfs_iterator* iOut)
{
    assert(pArchive != NULL);
    assert(path     != NULL);
    assert(iOut     != NULL);

    void* pUserData = pArchive->callbacks.beginiteration(pArchive, path);
    if (pUserData != NULL)
    {
        iOut->pArchive  = pArchive;
        iOut->pUserData = pUserData;

        return 1;
    }

    return 0;
}

void easyvfs_archive_enditeration(easyvfs_iterator* i)
{
    assert(i           != NULL);
    assert(i->pArchive != NULL);

    i->pArchive->callbacks.enditeration(i->pArchive, i);
}

int easyvfs_archive_nextiteration(easyvfs_iterator* i, easyvfs_fileinfo* fi)
{
    assert(i           != NULL);
    assert(i->pArchive != NULL);

    if (i->pUserData != NULL)
    {
        return i->pArchive->callbacks.nextiteration(i->pArchive, i, fi);
    }

    return 0;
}


/// A simple helper function for determining the access mode to use for an archive file based on the access mode
/// of a file within that archive.
easyvfs_accessmode easyvfs_archiveaccessmode(easyvfs_accessmode accessMode)
{
    return (accessMode == easyvfs_read) ? easyvfs_read : easyvfs_readwrite;
}

/// Opens a native archive. This is not recursive.
easyvfs_archive* easyvfs_openarchive_native(easyvfs_context* pContext, const char* basePath, easyvfs_accessmode accessMode)
{
    if (pContext != NULL && basePath != NULL)
    {
        if (pContext->nativeCallbacks.isvalidarchive(pContext, basePath))
        {
            easyvfs_archive* pNewArchive = easyvfs_malloc(sizeof(easyvfs_archive));
            if (pNewArchive != NULL)
            {
                pNewArchive->pContext       = pContext;
                pNewArchive->pParentArchive = NULL;
                pNewArchive->pFile          = NULL;
                pNewArchive->callbacks      = pContext->nativeCallbacks;
                easyvfs_strcpy(pNewArchive->absolutePath, EASYVFS_MAX_PATH, basePath);
                pNewArchive->pUserData      = pContext->nativeCallbacks.openarchive(NULL, accessMode);
                if (pNewArchive->pUserData == NULL)
                {
                    easyvfs_free(pNewArchive);
                    pNewArchive = NULL;
                }
            }

            return pNewArchive;
        }
    }

    return NULL;
}

/// Opens a non-native archive. This is not recursive.
easyvfs_archive* easyvfs_openarchive_nonnative(easyvfs_archive* pParentArchive, const char* path, easyvfs_accessmode accessMode)
{
    assert(pParentArchive           != NULL);
    assert(pParentArchive->pContext != NULL);
    assert(path                     != NULL);

    for (unsigned int iCallbacks = 0; iCallbacks < pParentArchive->pContext->archiveCallbacks.count; ++iCallbacks)
    {
        easyvfs_archive_callbacks* pCallbacks = &pParentArchive->pContext->archiveCallbacks.pBuffer[iCallbacks];
        assert(pCallbacks != NULL);

        if (pCallbacks->isvalidarchive && pCallbacks->openarchive)
        {
            if (pCallbacks->isvalidarchive(pParentArchive->pContext, path))
            {
                easyvfs_file* pNewArchiveFile = easyvfs_archive_openfile(pParentArchive, path, accessMode); // pParentArchive->callbacks.openfile(pParentArchive, path, accessMode);
                if (pNewArchiveFile != NULL)
                {
                    easyvfs_archive* pNewArchive = easyvfs_malloc(sizeof(easyvfs_archive));
                    if (pNewArchive != NULL)
                    {
                        void* pNewArchiveUserData = pCallbacks->openarchive(pNewArchiveFile, accessMode);
                        if (pNewArchiveUserData != NULL)
                        {
                            pNewArchive->pContext       = pParentArchive->pContext;
                            pNewArchive->pParentArchive = pParentArchive;
                            pNewArchive->pFile          = pNewArchiveFile;
                            pNewArchive->callbacks      = *pCallbacks;
                            easyvfs_copyandappendpath(pNewArchive->absolutePath, EASYVFS_MAX_PATH, pParentArchive->absolutePath, path);
                            pNewArchive->pUserData      = pNewArchiveUserData;
                        }
                        else
                        {
                            pParentArchive->callbacks.closefile(pNewArchiveFile);

                            easyvfs_free(pNewArchive);
                            pNewArchive = NULL;
                        }
                    }
                    else
                    {
                        pParentArchive->callbacks.closefile(pNewArchiveFile);
                    }

                    return pNewArchive;
                }
            }
        }
    }

    return NULL;
}

/// Opens a non-native archive.
///
/// When pParentArchive is null this will create a native archive. This is not recursive.
easyvfs_archive* easyvfs_openarchive(easyvfs_context* pContext, easyvfs_archive* pParentArchive, const char* path, easyvfs_accessmode accessMode)
{
    if (pParentArchive == NULL)
    {
        // Open a native archive.
        return easyvfs_openarchive_native(pContext, path, accessMode);
    }
    else
    {
        // Open a non-native archive.
        return easyvfs_openarchive_nonnative(pParentArchive, path, accessMode);
    }
}


// This will return an archive even when the file doesn't exist.
easyvfs_archive* easyvfs_openarchive_frompath_verbose(easyvfs_archive* pArchive, const char* path, easyvfs_accessmode accessMode, char* relativePathOut, unsigned int relativePathBufferSizeInBytes)
{
    assert(pArchive != NULL);

    easyvfs_strcpy(relativePathOut, relativePathBufferSizeInBytes, path);

    // Check that the file exists first.
    easyvfs_fileinfo fi;
    if (pArchive->callbacks.getfileinfo(pArchive, path, &fi))
    {
        // The file exists in this archive.
        return pArchive;
    }
    else
    {
        // The file is not contained within this archive, so we need to continue iterating.
        char runningPath[EASYVFS_MAX_PATH];
        runningPath[0] = '\0';

        easyvfs_pathiterator iPathSegment = easyvfs_beginpathiteration(path);
        while (easyvfs_nextpathsegment(&iPathSegment))
        {
            if (easyvfs_appendpathiterator(runningPath, EASYVFS_MAX_PATH, iPathSegment))
            {
                if (pArchive->callbacks.getfileinfo(pArchive, runningPath, &fi))
                {
                    if ((fi.attributes & EASYVFS_FILE_ATTRIBUTE_DIRECTORY) == 0)
                    {
                        // The file is not a directory which means it could be an archive. Try opening the archive, and then try loading the file recursively.
                        easyvfs_archive* pChildArchive = easyvfs_openarchive(pArchive->pContext, pArchive, runningPath, easyvfs_archiveaccessmode(accessMode));
                        if (pChildArchive != NULL)
                        {
                            // It's an archive, so check it. The name to check is the beginning of the next path segment.
                            if (easyvfs_nextpathsegment(&iPathSegment))
                            {
                                easyvfs_strcpy(relativePathOut, relativePathBufferSizeInBytes, path);

                                easyvfs_archive* pResultArchive = easyvfs_openarchive_frompath_verbose(pChildArchive, iPathSegment.path + iPathSegment.segment.offset, accessMode, relativePathOut, relativePathBufferSizeInBytes);
                                if (pResultArchive != NULL)
                                {
                                    return pResultArchive;
                                }
                                else
                                {
                                    return pChildArchive;
                                }
                            }
                            else
                            {
                                // We should never get here, but we are at the last segment in the input path already. Just close the child archive and return null.
                                easyvfs_closearchive(pChildArchive);
                                return NULL;
                            }
                        }
                        else
                        {
                            // It's not an archive.
                            return pArchive;
                        }
                    }
                }
            }
            else
            {
                // Should never get here, but if we do it means there was an error appending the path segment to the running path. We'll never find the
                // file in this case, so return null.
                return NULL;
            }
        }
    }

    // If we get here the file is not contained within this archive.
    return pArchive;
}

easyvfs_archive* easyvfs_openarchive_frompath_default(easyvfs_archive* pArchive, const char* path, easyvfs_accessmode accessMode, char* relativePathOut, unsigned int relativePathBufferSizeInBytes)
{
    // Check that the file exists first.
    easyvfs_fileinfo fi;
    if (pArchive->callbacks.getfileinfo(pArchive, path, &fi))
    {
        // The file exists in this archive.
        easyvfs_strcpy(relativePathOut, relativePathBufferSizeInBytes, path);
        return pArchive;
    }
    else
    {
        // The file is not contained within this archive, but may be contained in other archives - we'll search for it.
        //
        // Searching works as such:
        //   For each segment in "path"
        //       If segment is archive then
        //           Open and search archive
        //       Else
        //           Search each archive file in this directory
        //       Endif
        char runningPath[EASYVFS_MAX_PATH];
        runningPath[0] = '\0';

        easyvfs_pathiterator iPathSegment = easyvfs_beginpathiteration(path);
        while (easyvfs_nextpathsegment(&iPathSegment))
        {
            char runningPathBase[EASYVFS_MAX_PATH];
            easyvfs_memcpy(runningPathBase, runningPath, EASYVFS_MAX_PATH);

            if (easyvfs_appendpathiterator(runningPath, EASYVFS_MAX_PATH, iPathSegment))
            {
                // Check if the path segment refers to an archive.
                if (pArchive->callbacks.getfileinfo(pArchive, runningPath, &fi))
                {
                    if ((fi.attributes & EASYVFS_FILE_ATTRIBUTE_DIRECTORY) == 0)
                    {
                        // It's a normal file. This should not be the last segment in the path.
                        if (!easyvfs_atendofpath(iPathSegment))
                        {
                            // The file could be an archive. Try opening the archive, and then try loading the file recursively. If this fails, we need to return null.
                            easyvfs_archive* pChildArchive = easyvfs_openarchive(pArchive->pContext, pArchive, runningPath, easyvfs_archiveaccessmode(accessMode));
                            if (pChildArchive != NULL)
                            {
                                // It's an archive, so check it. The name to check is the beginning of the next path segment.
                                easyvfs_pathiterator iNextSegment = iPathSegment;
                                if (easyvfs_nextpathsegment(&iNextSegment))
                                {
                                    easyvfs_archive* pResultArchive = easyvfs_openarchive_frompath_default(pChildArchive, iNextSegment.path + iNextSegment.segment.offset, accessMode, relativePathOut, relativePathBufferSizeInBytes);
                                    if (pResultArchive != NULL)
                                    {
                                        return pResultArchive;
                                    }
                                    else
                                    {
                                        easyvfs_closearchive(pChildArchive);
                                        return NULL;
                                    }
                                }
                                else
                                {
                                    // We should never get here, but we are at the last segment in the input path already. Just close the child archive and return null.
                                    easyvfs_closearchive(pChildArchive);
                                    return NULL;
                                }
                            }
                            else
                            {
                                return NULL;
                            }
                        }
                    }
                }


                // The path segment is not an archive file, so we need to scan for other archives in the running directory and check them, recursively.
                easyvfs_iterator iFile;
                if (easyvfs_archive_beginiteration(pArchive, runningPathBase, &iFile))
                {
                    while (easyvfs_archive_nextiteration(&iFile, &fi))
                    {
                        if ((fi.attributes & EASYVFS_FILE_ATTRIBUTE_DIRECTORY) == 0)
                        {
                            // It's a file which means it could be an archive.
                            //
                            // TODO: THIS NEEDS A SMALL ADJUSTMENT FOR CLARITY. THE VARIABLE fi.absolutePath IS ACTUALLY ABSOLUTE RELATIVE TO THE ARCHIVE FILE AND IS NOT A FULLY RESOLVED VERBOSE PATH.
                            easyvfs_archive* pChildArchive = easyvfs_openarchive(pArchive->pContext, pArchive, fi.absolutePath, easyvfs_archiveaccessmode(accessMode));
                            if (pChildArchive != NULL)
                            {
                                // It's an archive, so check it.
                                easyvfs_archive* pResultArchive = easyvfs_openarchive_frompath_default(pChildArchive, iPathSegment.path + iPathSegment.segment.offset, accessMode, relativePathOut, relativePathBufferSizeInBytes);
                                if (pResultArchive != NULL)
                                {
                                    easyvfs_archive_enditeration(&iFile);
                                    return pResultArchive;
                                }
                                else
                                {
                                    easyvfs_closearchive(pChildArchive);
                                }
                            }
                        }
                    }

                    easyvfs_archive_enditeration(&iFile);
                }
            }
            else
            {
                // Should never get here, but if we do it means there was an error appending the path segment to the running path. We'll never find the
                // file in this case, so return null.
                return NULL;
            }
        }
    }


    // If we get here we could not find the file.
    return NULL;
}


/// Opens an archive from a path.
///
/// When the archive is no longer needed, delete it recursively.
easyvfs_archive* easyvfs_openarchive_frompath(easyvfs_context* pContext, const char* path, easyvfs_accessmode accessMode, char* relativePathOut, unsigned int relativePathBufferSizeInBytes)
{
    assert(pContext != NULL);
    assert(path     != NULL);

    if (easyvfs_ispathabsolute(path))
    {
        // The path is absolute. We assume it's a verbose path.
        easyvfs_archive* pRootArchive = easyvfs_openarchive_native(pContext, "", easyvfs_read);
        if (pRootArchive != NULL)
        {
            easyvfs_archive* pResultArchive = easyvfs_openarchive_frompath_verbose(pRootArchive, path, accessMode, relativePathOut, relativePathBufferSizeInBytes);
            if (pResultArchive == NULL)
            {
                easyvfs_closearchive(pRootArchive);
            }

            return pResultArchive;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        // The path is relative. The archive is null, so we need to create an archive from each base directory and call this function recursively.
        for (unsigned int iBasePath = 0; iBasePath < pContext->baseDirectories.count; ++iBasePath)
        {
            easyvfs_archive* pRootArchive = easyvfs_openarchive_native(pContext, pContext->baseDirectories.pBuffer[iBasePath].absolutePath, accessMode);
            if (pRootArchive != NULL)
            {
                easyvfs_archive* pResultArchive = easyvfs_openarchive_frompath_default(pRootArchive, path, accessMode, relativePathOut, relativePathBufferSizeInBytes);
                if (pResultArchive == NULL)
                {
                    easyvfs_closearchive(pRootArchive);
                }

                if (pResultArchive != NULL)
                {
                    return pResultArchive;
                }
            }
        }

        return NULL;
    }
}


/// Closes the given archive.
void easyvfs_closearchive(easyvfs_archive* pArchive)
{
    assert(pArchive != NULL);

    if (pArchive->pParentArchive != NULL)
    {
        // It's a non-native archive. We close the archive based on it's own callbacks.
        pArchive->callbacks.closearchive(pArchive);

        // We now need to close the file.
        easyvfs_archive_closefile(pArchive->pParentArchive, pArchive->pFile);
    }
    else
    {
        // It's a native archive which is closed a bit differently to non-native ones.
        if (pArchive->pContext != NULL)
        {
            pArchive->pContext->nativeCallbacks.closearchive(pArchive);
        }

        assert(pArchive->pFile == NULL);
    }

    easyvfs_free(pArchive);
}

/// Recursively closes the given archive and it's parent's.
void easyvfs_closearchive_recursive(easyvfs_archive* pArchive)
{
    assert(pArchive != NULL);

    easyvfs_archive* pParentArchive = pArchive->pParentArchive;


    easyvfs_closearchive(pArchive);

    if (pParentArchive != NULL)
    {
        easyvfs_closearchive_recursive(pParentArchive);
    }
}



/// Opens a file from an archive.
///
/// This is not recursive.
easyvfs_file* easyvfs_archive_openfile(easyvfs_archive* pArchive, const char* path, easyvfs_accessmode accessMode)
{
    assert(pArchive != NULL);
    assert(path     != NULL);

    easyvfs_file* pNewFile = easyvfs_malloc(sizeof(easyvfs_file));
    if (pNewFile != NULL)
    {
        void* pUserData = pArchive->callbacks.openfile(pArchive, path, accessMode);
        if (pUserData != NULL)
        {
            pNewFile->pArchive  = pArchive;
            pNewFile->pUserData = pUserData;

            return pNewFile;
        }
        else
        {
            easyvfs_free(pNewFile);
            pNewFile = NULL;
        }
    }


    return pNewFile;
}

void easyvfs_archive_closefile(easyvfs_archive* pArchive, easyvfs_file* pFile)
{
    assert(pArchive        != NULL);
    assert(pFile           != NULL);
    assert(pFile->pArchive == pArchive);

    pArchive->callbacks.closefile(pFile);
    easyvfs_free(pFile);
}




////////////////////////////////////////
// Public API

easyvfs_context* easyvfs_createcontext()
{
    easyvfs_context* pContext = easyvfs_malloc(sizeof(easyvfs_context));
    if (pContext != NULL)
    {
        if (easyvfs_callbacklist_init(&pContext->archiveCallbacks) && easyvfs_basepaths_init(&pContext->baseDirectories))
        {
            pContext->nativeCallbacks.isvalidarchive = easyvfs_isvalidarchive_impl_native;
            pContext->nativeCallbacks.openarchive    = easyvfs_openarchive_impl_native;
            pContext->nativeCallbacks.closearchive   = easyvfs_closearchive_impl_native;
            pContext->nativeCallbacks.getfileinfo    = easyvfs_getfileinfo_impl_native;
            pContext->nativeCallbacks.beginiteration = easyvfs_beginiteration_impl_native;
            pContext->nativeCallbacks.enditeration   = easyvfs_enditeration_impl_native;
            pContext->nativeCallbacks.nextiteration  = easyvfs_nextiteration_impl_native;
            pContext->nativeCallbacks.openfile       = easyvfs_openfile_impl_native;
            pContext->nativeCallbacks.closefile      = easyvfs_closefile_impl_native;
            pContext->nativeCallbacks.readfile       = easyvfs_readfile_impl_native;
            pContext->nativeCallbacks.writefile      = easyvfs_writefile_impl_native;
            pContext->nativeCallbacks.seekfile       = easyvfs_seekfile_impl_native;
            pContext->nativeCallbacks.tellfile       = easyvfs_tellfile_impl_native;
            pContext->nativeCallbacks.filesize       = easyvfs_filesize_impl_native;
            pContext->nativeCallbacks.deletefile     = easyvfs_deletefile_impl_native;
            pContext->nativeCallbacks.renamefile     = easyvfs_renamefile_impl_native;
            pContext->nativeCallbacks.mkdir          = easyvfs_mkdir_impl_native;
        }
        else
        {
            easyvfs_free(pContext);
            pContext = NULL;
        }
    }

    return pContext;
}

void easyvfs_deletecontext(easyvfs_context* pContext)
{
    if (pContext != NULL)
    {
        easyvfs_basepaths_uninit(&pContext->baseDirectories);
        easyvfs_callbacklist_uninit(&pContext->archiveCallbacks);
        easyvfs_free(pContext);
    }
}


void easyvfs_registerarchivecallbacks(easyvfs_context* pContext, easyvfs_archive_callbacks callbacks)
{
    if (pContext != NULL)
    {
        easyvfs_callbacklist_pushback(&pContext->archiveCallbacks, callbacks);
    }
}


void easyvfs_insertbasedirectory(easyvfs_context* pContext, const char* absolutePath, unsigned int index)
{
    if (pContext != 0)
    {
        easyvfs_basepaths_insert(&pContext->baseDirectories, absolutePath, index);
    }
}

void easyvfs_addbasedirectory(easyvfs_context* pContext, const char* absolutePath)
{
    easyvfs_insertbasedirectory(pContext, absolutePath, easyvfs_basedirectorycount(pContext));
}

void easyvfs_removebasedirectory(easyvfs_context* pContext, const char* absolutePath)
{
    if (pContext != NULL)
    {
        for (unsigned int iPath = 0; iPath < pContext->baseDirectories.count; )
        {
            if (easyvfs_pathsequal(pContext->baseDirectories.pBuffer[iPath].absolutePath, absolutePath))
            {
                easyvfs_basepaths_remove(&pContext->baseDirectories, iPath);
            }
            else
            {
                ++iPath;
            }
        }
    }
}

void easyvfs_removebasedirectorybyindex(easyvfs_context* pContext, unsigned int index)
{
    if (pContext != NULL)
    {
        easyvfs_basepaths_remove(&pContext->baseDirectories, index);
    }
}

void easyvfs_removeallbasedirectories(easyvfs_context* pContext)
{
    if (pContext != NULL)
    {
        easyvfs_basepaths_clear(&pContext->baseDirectories);
    }
}

unsigned int easyvfs_basedirectorycount(easyvfs_context* pContext)
{
    if (pContext != NULL)
    {
        return pContext->baseDirectories.count;
    }

    return 0;
}


int easyvfs_beginiteration(easyvfs_context* pContext, const char* path, easyvfs_iterator* iOut)
{
    if (pContext != NULL && path != NULL && iOut != NULL)
    {
        char relativePath[EASYVFS_MAX_PATH];

        easyvfs_archive* pArchive = easyvfs_openarchive_frompath(pContext, path, easyvfs_read, relativePath, EASYVFS_MAX_PATH);
        if (pArchive != NULL)
        {
            void* pUserData = pArchive->callbacks.beginiteration(pArchive, path);
            if (pUserData != NULL)
            {
                iOut->pArchive  = pArchive;
                iOut->pUserData = pUserData;

                return 1;
            }
            else
            {
                easyvfs_closearchive_recursive(pArchive);
                pArchive = NULL;
            }
        }
    }

    return 0;
}

int easyvfs_nextiteration(easyvfs_context* pContext, easyvfs_iterator* i, easyvfs_fileinfo* fi)
{
    if (pContext != NULL && i != NULL && i->pUserData != NULL)
    {
        assert(i->pArchive != NULL);
        int result = i->pArchive->callbacks.nextiteration(i->pArchive, i, fi);
        if (result)
        {
            // The absolute path returned by the callback will be relative to the archive file. We want it to be absolute.
            if (fi != NULL)
            {
                char tempPath[EASYVFS_MAX_PATH];
                easyvfs_strcpy(tempPath, EASYVFS_MAX_PATH, fi->absolutePath);

                easyvfs_copyandappendpath(fi->absolutePath, EASYVFS_MAX_PATH, i->pArchive->absolutePath, tempPath);
            }

        }

        return result;
    }

    return 0;
}

void easyvfs_enditeration(easyvfs_context* pContext, easyvfs_iterator* i)
{
    if (pContext != NULL && i != NULL)
    {
        assert(i->pArchive != NULL);
        i->pArchive->callbacks.enditeration(i->pArchive, i);
        i->pUserData = NULL;

        easyvfs_closearchive_recursive(i->pArchive);
        i->pArchive = NULL;
    }
}


int easyvfs_getbasedirectorybyindex(easyvfs_context* pContext, unsigned int index, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes)
{
    if (pContext != NULL && index < pContext->baseDirectories.count)
    {
        easyvfs_strcpy(absolutePathOut, absolutePathBufferSizeInBytes, pContext->baseDirectories.pBuffer[index].absolutePath);
    }

    return 0;
}

int easyvfs_getfileinfo(easyvfs_context* pContext, const char* path, easyvfs_fileinfo* fi)
{
    if (pContext != NULL && path != NULL)
    {
        char relativePath[EASYVFS_MAX_PATH];

        easyvfs_archive* pArchive = easyvfs_openarchive_frompath(pContext, path, easyvfs_read, relativePath, EASYVFS_MAX_PATH);
        if (pArchive != NULL)
        {
            int result = pArchive->callbacks.getfileinfo(pArchive, relativePath, fi);


            // The archive is no longer needed, so it needs to be closed.
            easyvfs_closearchive_recursive(pArchive);
            pArchive = NULL;

            return result;
        }
    }

    return 0;
}

int easyvfs_findabsolutepath(easyvfs_context* pContext, const char* path, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes)
{
    if (pContext != NULL && path != NULL && absolutePathOut != NULL && absolutePathBufferSizeInBytes > 0)
    {
        // We retrieve the absolute path by just getting the file info which will contain it.
        easyvfs_fileinfo fi;
        if (easyvfs_getfileinfo(pContext, path, &fi))
        {
            easyvfs_strcpy(absolutePathOut, absolutePathBufferSizeInBytes, fi.absolutePath);
            return 1;
        }
    }

    return 0;
}

int easyvfs_findabsolutepath_explicitbase(easyvfs_context* pContext, const char* path, const char* highestPriorityBasePath, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes)
{
    if (pContext != NULL && path != NULL && highestPriorityBasePath != NULL && absolutePathOut != NULL && absolutePathBufferSizeInBytes > 0)
    {
        int result = 0;
        easyvfs_insertbasedirectory(pContext, highestPriorityBasePath, 0);
        {
            result = easyvfs_findabsolutepath(pContext, path, absolutePathOut, absolutePathBufferSizeInBytes);
        }
        easyvfs_removebasedirectorybyindex(pContext, 0);

        return result;
    }

    return 0;
}


int easyvfs_deletefile(easyvfs_context* pContext, const char* path)
{
    int result = 0;
    if (easyvfs_ispathabsolute(path))
    {
        if (pContext != NULL && path != NULL)
        {
            char relativePath[EASYVFS_MAX_PATH];
            easyvfs_archive* pArchive = easyvfs_openarchive_frompath(pContext, path, easyvfs_readwrite, relativePath, EASYVFS_MAX_PATH);
            if (pArchive != NULL)
            {
                result = pArchive->callbacks.deletefile(pArchive, relativePath);

                easyvfs_closearchive_recursive(pArchive);
            }
        }
    }

    return result;
}

int easyvfs_renamefile(easyvfs_context* pContext, const char* pathOld, const char* pathNew)
{
    // Renaming/moving is not supported across different archives.

    int result = 0;
    if (easyvfs_ispathabsolute(pathOld) && easyvfs_ispathabsolute(pathNew))
    {
        if (pContext != NULL && pathOld != NULL && pathNew != NULL)
        {
            char relativePathOld[EASYVFS_MAX_PATH];
            easyvfs_archive* pArchiveOld = easyvfs_openarchive_frompath(pContext, pathOld, easyvfs_readwrite, relativePathOld, EASYVFS_MAX_PATH);
            if (pArchiveOld != NULL)
            {
                char relativePathNew[EASYVFS_MAX_PATH];
                easyvfs_archive* pArchiveNew = easyvfs_openarchive_frompath(pContext, pathNew, easyvfs_readwrite, relativePathNew, EASYVFS_MAX_PATH);
                if (pArchiveNew != NULL)
                {
                    if (easyvfs_pathsequal(pArchiveOld->absolutePath, pArchiveNew->absolutePath))
                    {
                        result = pArchiveOld->callbacks.renamefile(pArchiveOld, relativePathOld, relativePathNew);
                    }

                    easyvfs_closearchive_recursive(pArchiveNew);
                }

                easyvfs_closearchive_recursive(pArchiveOld);
            }
        }
    }

    return result;
}

int easyvfs_mkdir(easyvfs_context* pContext, const char* path)
{
    int result = 0;
    if (easyvfs_ispathabsolute(path))
    {
        if (pContext != NULL && path != NULL)
        {
            char relativePath[EASYVFS_MAX_PATH];
            easyvfs_archive* pArchive = easyvfs_openarchive_frompath(pContext, path, easyvfs_readwrite, relativePath, EASYVFS_MAX_PATH);
            if (pArchive != NULL)
            {
                result = pArchive->callbacks.mkdir(pArchive, relativePath);

                easyvfs_closearchive_recursive(pArchive);
            }
        }
    }

    return result;
}



easyvfs_file* easyvfs_openfile(easyvfs_context* pContext, const char* absoluteOrRelativePath, easyvfs_accessmode accessMode)
{
    if (pContext != NULL && absoluteOrRelativePath)
    {
        char relativePath[EASYVFS_MAX_PATH];
        easyvfs_archive* pArchive = easyvfs_openarchive_frompath(pContext, absoluteOrRelativePath, easyvfs_archiveaccessmode(accessMode), relativePath, EASYVFS_MAX_PATH);
        if (pArchive != NULL)
        {
            easyvfs_file* pFile = easyvfs_archive_openfile(pArchive, relativePath, accessMode);
            if (pFile == NULL)
            {
                easyvfs_closearchive_recursive(pArchive);
                pArchive = NULL;
            }

            return pFile;
        }
    }

    return NULL;
}

void easyvfs_closefile(easyvfs_file* pFile)
{
    if (pFile != NULL)
    {
        easyvfs_archive* pArchive = pFile->pArchive;
        assert(pArchive != NULL);


        // Have the archive close the file. This will free the file object, but leave the archive alive.
        easyvfs_archive_closefile(pFile->pArchive, pFile);

        // We now want to close the archive, making sure we don't try to access pFile anymore since that has been freed.
        easyvfs_closearchive_recursive(pArchive);
    }
}

int easyvfs_readfile(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.readfile(pFile, dst, bytesToRead, bytesReadOut);
    }

    return 0;
}

int easyvfs_writefile(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.writefile(pFile, src, bytesToWrite, bytesWrittenOut);
    }

    return 0;
}

easyvfs_bool easyvfs_seekfile(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seekorigin origin)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.seekfile(pFile, bytesToSeek, origin);
    }

    return 0;
}

easyvfs_uint64 easyvfs_tellfile(easyvfs_file* pFile)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.tellfile(pFile);
    }

    return 0;
}

easyvfs_uint64 easyvfs_filesize(easyvfs_file* pFile)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.filesize(pFile);
    }

    return 0;
}



///////////////////////////////////////////
// Utilities

int easyvfs_ispathchild(const char* childAbsolutePath, const char* parentAbsolutePath)
{
#if !EASYVFS_USE_EASYPATH
    easyvfs_pathiterator iParent = easyvfs_beginpathiteration(parentAbsolutePath);
    easyvfs_pathiterator iChild  = easyvfs_beginpathiteration(childAbsolutePath);

    while (easyvfs_nextpathsegment(&iParent))
    {
        if (easyvfs_nextpathsegment(&iChild))
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
    if (easyvfs_nextpathsegment(&iChild))
    {
        // It could be a child. If the next iteration fails, it's a direct child and we want to return true.
        if (!easyvfs_nextpathsegment(&iChild))
        {
            return 1;
        }
    }

    return 0;
#else
    return easypath_ischild(childAbsolutePath, parentAbsolutePath);
#endif
}

const char* easyvfs_filename(const char* path)
{
#if !EASYVFS_USE_EASYPATH
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
#else
    return easypath_filename(path);
#endif
}

const char* easyvfs_extension(const char* path)
{
#if !EASYVFS_USE_EASYPATH
    if (path != 0)
    {
        const char* extension     = easyvfs_filename(path);
        const char* lastoccurance = 0;

        // Just find the last '.' and return.
        while (extension[0] != '\0')
        {
            extension += 1;

            if (extension[0] == '.')
            {
                extension    += 1;
                lastoccurance = extension;
            }
        }


        return (lastoccurance != 0) ? lastoccurance : extension;
    }

    return 0;
#else
    return easypath_extension(path);
#endif
}

int easyvfs_extensionequal(const char* path, const char* extension)
{
#if !EASYVFS_USE_EASYPATH
    if (path != 0 && extension != 0)
    {
        const char* ext1 = extension;
        const char* ext2 = easyvfs_extension(path);

        while (ext1[0] != '\0' && ext2[0] != '\0')
        {
            if (tolower(ext1[0]) != tolower(ext2[0]))
            {
                return 0;
            }

            ext1 += 1;
            ext2 += 1;
        }


        return ext1[0] == '\0' && ext2[0] == '\0';
    }

    return 1;
#else
    return easypath_extensionequal(path, extension);
#endif
}

int easyvfs_pathsequal(const char* path1, const char* path2)
{
#if !EASYVFS_USE_EASYPATH
    if (path1 != 0 && path2 != 0)
    {
        easyvfs_pathiterator iPath1 = easyvfs_beginpathiteration(path1);
        easyvfs_pathiterator iPath2 = easyvfs_beginpathiteration(path2);

        while (easyvfs_nextpathsegment(&iPath1) && easyvfs_nextpathsegment(&iPath2))
        {
            if (!easyvfs_pathiterators_equal(iPath1, iPath2))
            {
                return 0;
            }
        }


        // At this point either iPath1 and/or iPath2 have finished iterating. If both of them are at the end, the two paths are equal.
        return iPath1.path[iPath1.segment.offset] == '\0' && iPath2.path[iPath2.segment.offset] == '\0';
    }

    return 0;
#else
    return easypath_equal(path1, path2);
#endif
}

int easyvfs_ispathrelative(const char* path)
{
#if !EASYVFS_USE_EASYPATH
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
#else
    return easypath_isrelative(path);
#endif
}

int easyvfs_ispathabsolute(const char* path)
{
    return !easyvfs_ispathrelative(path);
}

int easyvfs_appendpath(char* base, unsigned int baseBufferSizeInBytes, const char* other)
{
#if !EASYVFS_USE_EASYPATH
    if (base != 0 && other != 0)
    {
        unsigned int path1Length = (unsigned int)strlen(base);
        unsigned int path2Length = (unsigned int)strlen(other);

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

            easyvfs_strcpy2(base + path1Length, baseBufferSizeInBytes - path1Length, other, path2Length);


            return 1;
        }
    }

    return 0;
#else
    return easypath_append(base, baseBufferSizeInBytes, other);
#endif
}

int easyvfs_copyandappendpath(char* dst, unsigned int dstSizeInBytes, const char* base, const char* other)
{
#if !EASYVFS_USE_EASYPATH
    if (dst != NULL && dstSizeInBytes > 0)
    {
        easyvfs_strcpy(dst, dstSizeInBytes, base);
        return easyvfs_appendpath(dst, dstSizeInBytes, other);
    }

    return 0;
#else
    return easypath_copyandappend(dst, dstSizeInBytes, base, other);
#endif
}



///////////////////////////////////////////
// Platform Specific

#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)
typedef struct
{
    HANDLE hFind;
    WIN32_FIND_DATAA ffd;
    char directoryPath[EASYVFS_MAX_PATH];

}easyvfs_iterator_win32;

int easyvfs_isvalidarchive_impl_native(easyvfs_context* pContext, const char* path)
{
    (void)pContext;

    // For native archives, the path must be a folder, or an empty path. An empty path just refers to a root directory.
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0'))
    {
        return 1;
    }

    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        return 0;
    }

    return 1;
}

void* easyvfs_openarchive_impl_native(easyvfs_file* pFile, easyvfs_accessmode accessMode)
{
    (void)pFile;
    assert(pFile == NULL);

    // easyvfs_isvalidarchive_impl_native will have been called before this which is where the check is done to ensure the path refers to a directory.
    easyvfs_native_archive* pUserData = easyvfs_malloc(sizeof(easyvfs_native_archive));
    if (pUserData != NULL)
    {
        pUserData->accessMode = accessMode;
    }

    return pUserData;
}

void easyvfs_closearchive_impl_native(easyvfs_archive* pArchive)
{
    assert(pArchive            != NULL);
    assert(pArchive->pUserData != NULL);

    // Nothing to do except free the object.
    easyvfs_free(pArchive->pUserData);
}

int easyvfs_getfileinfo_impl_native(easyvfs_archive* pArchive, const char* path, easyvfs_fileinfo *fi)
{
    assert(pArchive            != NULL);
    assert(pArchive->pUserData != NULL);
    assert(path                != NULL);

    char fullPath[EASYVFS_MAX_PATH];
    if (easyvfs_copyandappendpath(fullPath, EASYVFS_MAX_PATH, pArchive->absolutePath, path))
    {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(fullPath, GetFileExInfoStandard, &fad))
        {
            if (fi != NULL)
            {
                ULARGE_INTEGER liSize;
                liSize.LowPart  = fad.nFileSizeLow;
                liSize.HighPart = fad.nFileSizeHigh;

                ULARGE_INTEGER liTime;
                liTime.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
                liTime.HighPart = fad.ftLastWriteTime.dwHighDateTime;

                easyvfs_memcpy(fi->absolutePath, fullPath, EASYVFS_MAX_PATH);
                fi->sizeInBytes      = liSize.QuadPart;
                fi->lastModifiedTime = liTime.QuadPart;

                fi->attributes       = 0;
                if ((fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    fi->attributes |= EASYVFS_FILE_ATTRIBUTE_DIRECTORY;
                }
                if ((fad.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
                {
                    fi->attributes |= EASYVFS_FILE_ATTRIBUTE_READONLY;
                }
            }

            return 1;
        }
    }

    return 0;
}

void* easyvfs_beginiteration_impl_native(easyvfs_archive* pArchive, const char* path)
{
    assert(pArchive            != NULL);
    assert(pArchive->pUserData != NULL);
    assert(path                != NULL);


    char searchQuery[EASYVFS_MAX_PATH];
    easyvfs_copyandappendpath(searchQuery, EASYVFS_MAX_PATH, pArchive->absolutePath, path);

    unsigned int searchQueryLength = (unsigned int)strlen(searchQuery);
    if (searchQueryLength < EASYVFS_MAX_PATH - 3)
    {
        searchQuery[searchQueryLength + 0] = '\\';
        searchQuery[searchQueryLength + 1] = '*';
        searchQuery[searchQueryLength + 2] = '\0';

        easyvfs_iterator_win32* pUserData = easyvfs_malloc(sizeof(easyvfs_iterator_win32));
        if (pUserData != NULL)
        {
            pUserData->hFind = FindFirstFileA(searchQuery, &pUserData->ffd);
            if (pUserData->hFind != INVALID_HANDLE_VALUE)
            {
                easyvfs_strcpy(pUserData->directoryPath, EASYVFS_MAX_PATH, path);

                return pUserData;
            }
            else
            {
                // Failed to begin search.
                easyvfs_free(pUserData);
                return NULL;
            }
        }
        else
        {
            // Failed to allocate iterator data.
            return NULL;
        }
    }
    else
    {
        // Path is too long.
        return NULL;
    }
}

void easyvfs_enditeration_impl_native(easyvfs_archive* pArchive, easyvfs_iterator* i)
{
    assert(pArchive            != NULL);
    assert(pArchive->pUserData != NULL);
    assert(i                   != NULL);

    easyvfs_iterator_win32* pUserData = i->pUserData;
    if (pUserData != NULL)
    {
        FindClose(pUserData->hFind);
        easyvfs_free(pUserData);

        i->pUserData = NULL;
    }
}

int easyvfs_nextiteration_impl_native(easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_fileinfo* fi)
{
    assert(pArchive != NULL);
    assert(i        != NULL);

    easyvfs_iterator_win32* pUserData = i->pUserData;
    if (pUserData != NULL && pUserData->hFind != INVALID_HANDLE_VALUE && pUserData->hFind != NULL)
    {
        // Skip past "." and ".." directories.
        while (strcmp(pUserData->ffd.cFileName, ".") == 0 || strcmp(pUserData->ffd.cFileName, "..") == 0)
        {
            if (FindNextFileA(pUserData->hFind, &pUserData->ffd) == 0)
            {
                return 0;
            }
        }

        if (fi != NULL)
        {
            easyvfs_copyandappendpath(fi->absolutePath, EASYVFS_MAX_PATH, pUserData->directoryPath, pUserData->ffd.cFileName);

            ULARGE_INTEGER liSize;
            liSize.LowPart  = pUserData->ffd.nFileSizeLow;
            liSize.HighPart = pUserData->ffd.nFileSizeHigh;
            fi->sizeInBytes = liSize.QuadPart;

            ULARGE_INTEGER liTime;
            liTime.LowPart  = pUserData->ffd.ftLastWriteTime.dwLowDateTime;
            liTime.HighPart = pUserData->ffd.ftLastWriteTime.dwHighDateTime;
            fi->lastModifiedTime = liTime.QuadPart;

            fi->attributes = 0;
            if ((pUserData->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                fi->attributes |= EASYVFS_FILE_ATTRIBUTE_DIRECTORY;
            }
            if ((pUserData->ffd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
            {
                fi->attributes |= EASYVFS_FILE_ATTRIBUTE_READONLY;
            }
        }


        if (FindNextFileA(pUserData->hFind, &pUserData->ffd) == 0)
        {
            easyvfs_enditeration_impl_native(pArchive, i);
        }

        return 1;
    }

    return 0;
}

void* easyvfs_openfile_impl_native(easyvfs_archive* pArchive, const char* path, easyvfs_accessmode accessMode)
{
    assert(pArchive            != NULL);
    assert(pArchive->pUserData != NULL);
    assert(path                != NULL);

    char fullPath[EASYVFS_MAX_PATH];
    if (easyvfs_copyandappendpath(fullPath, EASYVFS_MAX_PATH, pArchive->absolutePath, path))
    {
        DWORD dwDesiredAccess       = 0;
        DWORD dwShareMode           = 0;
        DWORD dwCreationDisposition = OPEN_EXISTING;

        switch (accessMode)
        {
        case easyvfs_read:
            {
                dwDesiredAccess = FILE_GENERIC_READ;
                dwShareMode     = FILE_SHARE_READ;
                break;
            }

        case easyvfs_write:
            {
                dwDesiredAccess       = FILE_GENERIC_WRITE;
                dwCreationDisposition = CREATE_ALWAYS;
                break;
            }

        case easyvfs_readwrite:
            {
                dwDesiredAccess       = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
                dwCreationDisposition = OPEN_ALWAYS;
                break;
            }

        default: return NULL;
        }


        HANDLE hFile = CreateFileA(fullPath, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            return (void*)hFile;
        }
    }

    return NULL;
}

void easyvfs_closefile_impl_native(easyvfs_file* pFile)
{
    assert(pFile            != NULL);
    assert(pFile->pArchive  != NULL);
    assert(pFile->pUserData != NULL);

    CloseHandle((HANDLE)pFile->pUserData);
}

int easyvfs_readfile_impl_native(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut)
{
    assert(pFile != NULL);
    assert(pFile->pUserData != NULL);

    DWORD bytesRead;
    BOOL result = ReadFile((HANDLE)pFile->pUserData, dst, (DWORD)bytesToRead, &bytesRead, NULL);
    if (result && bytesReadOut != NULL)
    {
        *bytesReadOut = bytesRead;
    }

    return result;
}

int easyvfs_writefile_impl_native(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut)
{
    assert(pFile != NULL);
    assert(pFile->pUserData != NULL);

    DWORD bytesWritten;
    BOOL result = WriteFile((HANDLE)pFile->pUserData, src, (DWORD)bytesToWrite, &bytesWritten, NULL);
    if (result && bytesWrittenOut != NULL)
    {
        *bytesWrittenOut = bytesWritten;
    }

    return result;
}

easyvfs_bool easyvfs_seekfile_impl_native(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seekorigin origin)
{
    assert(pFile != NULL);
    assert(pFile->pUserData != NULL);

    LARGE_INTEGER lNewFilePointer;
    LARGE_INTEGER lDistanceToMove;
    lDistanceToMove.QuadPart = bytesToSeek;

    DWORD dwMoveMethod = FILE_CURRENT;
    if (origin == easyvfs_start)
    {
        dwMoveMethod = FILE_BEGIN;
    }
    else if (origin == easyvfs_end)
    {
        dwMoveMethod = FILE_END;
    }


    return SetFilePointerEx((HANDLE)pFile->pUserData, lDistanceToMove, &lNewFilePointer, dwMoveMethod);
}

easyvfs_uint64 easyvfs_tellfile_impl_native(easyvfs_file* pFile)
{
    assert(pFile != NULL);
    assert(pFile->pUserData != NULL);

    LARGE_INTEGER lNewFilePointer;
    LARGE_INTEGER lDistanceToMove;
    lDistanceToMove.QuadPart = 0;

    if (SetFilePointerEx((HANDLE)pFile->pUserData, lDistanceToMove, &lNewFilePointer, FILE_CURRENT))
    {
        return (easyvfs_uint64)lNewFilePointer.QuadPart;
    }

    return 0;
}

easyvfs_uint64 easyvfs_filesize_impl_native(easyvfs_file* pFile)
{
    assert(pFile != NULL);
    assert(pFile->pUserData != NULL);

    LARGE_INTEGER fileSize;
    if (GetFileSizeEx((HANDLE)pFile->pUserData, &fileSize))
    {
        return (easyvfs_uint64)fileSize.QuadPart;
    }

    return 0;
}

int easyvfs_deletefile_impl_native(easyvfs_archive* pArchive, const char* path)
{
    assert(pArchive            != NULL);
    assert(pArchive->pUserData != NULL);
    assert(path                != NULL);

    char fullPath[EASYVFS_MAX_PATH];
    if (easyvfs_copyandappendpath(fullPath, EASYVFS_MAX_PATH, pArchive->absolutePath, path))
    {
        DWORD attributes = GetFileAttributesA(fullPath);
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            // It's a normal file.
            return DeleteFileA(fullPath);
        }
        else
        {
            // It's a directory.
            return RemoveDirectoryA(fullPath);
        }
    }

    return 0;
}

int easyvfs_renamefile_impl_native(easyvfs_archive* pArchive, const char* pathOld, const char* pathNew)
{
    assert(pArchive            != NULL);
    assert(pArchive->pUserData != NULL);
    assert(pathOld             != NULL);
    assert(pathNew             != NULL);

    // We use the "Ex" version here because we want to fail if we would have to copy the file (if the destination and target are on seperate file systems).
    char fullPathOld[EASYVFS_MAX_PATH];
    if (easyvfs_copyandappendpath(fullPathOld, EASYVFS_MAX_PATH, pArchive->absolutePath, pathOld))
    {
        char fullPathNew[EASYVFS_MAX_PATH];
        if (easyvfs_copyandappendpath(fullPathNew, EASYVFS_MAX_PATH, pArchive->absolutePath, pathNew))
        {
            return MoveFileExA(fullPathOld, fullPathNew, 0);
        }
    }

    return 0;
}

int easyvfs_mkdir_impl_native(easyvfs_archive* pArchive, const char* path)
{
    assert(pArchive            != NULL);
    assert(pArchive->pUserData != NULL);
    assert(path                != NULL);

    char fullPath[EASYVFS_MAX_PATH];
    if (easyvfs_copyandappendpath(fullPath, EASYVFS_MAX_PATH, pArchive->absolutePath, path))
    {
        return CreateDirectoryA(fullPath, NULL);
    }

    return 0;
}
#endif

#if defined(__clang__)
    #pragma GCC diagnostic pop
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
