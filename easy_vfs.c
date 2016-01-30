// Public Domain. See "unlicense" statement at the end of this file.

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

int easyvfs_strcpy(char* dst, size_t dstSizeInBytes, const char* src)
{
#if defined(_MSC_VER)
    return strcpy_s(dst, dstSizeInBytes, src);
#else
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
    
    char* iDst = dst;
    const char* iSrc = src;
    size_t remainingSizeInBytes = dstSizeInBytes;
    while (remainingSizeInBytes > 0 && iSrc[0] != '\0')
    {
        iDst[0] = iSrc[0];

        iDst += 1;
        iSrc += 1;
        remainingSizeInBytes -= 1;
    }

    if (remainingSizeInBytes > 0) {
        iDst[0] = '\0';
    } else {
        dst[0] = '\0';
        return ERANGE;
    }

    return 0;
#endif
}

void easyvfs_strncpy(char* dst, unsigned int dstSizeInBytes, const char* src, unsigned int srcSizeInBytes)
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

easyvfs_pathiterator easyvfs_begin_path_iteration(const char* path)
{
    easyvfs_pathiterator i;
    i.path = path;
    i.segment.offset = 0;
    i.segment.length = 0;

    return i;
}

bool easyvfs_next_path_segment(easyvfs_pathiterator* i)
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
            return false;
        }



        while (i->path[i->segment.offset + i->segment.length] != '\0' && (i->path[i->segment.offset + i->segment.length] != '/' && i->path[i->segment.offset + i->segment.length] != '\\'))
        {
            i->segment.length += 1;
        }

        return true;
    }

    return false;
}

bool easyvfs_at_end_of_path(easyvfs_pathiterator i)
{
    return !easyvfs_next_path_segment(&i);
}

bool easyvfs_path_segments_equal(const char* s0Path, const easyvfs_pathsegment s0, const char* s1Path, const easyvfs_pathsegment s1)
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

bool easyvfs_pathiterators_equal(const easyvfs_pathiterator i0, const easyvfs_pathiterator i1)
{
    return easyvfs_path_segments_equal(i0.path, i0.segment, i1.path, i1.segment);
}

bool easyvfs_append_path_iterator(char* base, unsigned int baseBufferSizeInBytes, easyvfs_pathiterator i)
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

            easyvfs_strncpy(base + path1Length, baseBufferSizeInBytes - path1Length, i.path + i.segment.offset, path2Length);


            return true;
        }
    }

    return false;
}
#else
typedef struct easypath_iterator easyvfs_pathiterator;

easyvfs_pathiterator easyvfs_begin_path_iteration(const char* path)
{
    return easypath_begin(path);
}

bool easyvfs_next_path_segment(easyvfs_pathiterator* i)
{
    return easypath_next(i);
}

bool easyvfs_at_end_of_path(easyvfs_pathiterator i)
{
    return easypath_at_end(i);
}

bool easyvfs_pathiterators_equal(const easyvfs_pathiterator i0, const easyvfs_pathiterator i1)
{
    return easypath_iterators_equal(i0, i1);
}

bool easyvfs_append_path_iterator(char* base, unsigned int baseBufferSizeInBytes, easyvfs_pathiterator i)
{
    return easypath_append_iterator(base, baseBufferSizeInBytes, i);
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

    /// The write base directory.
    char writeBaseDirectory[EASYVFS_MAX_PATH];

    /// Keeps track of whether or not write directory guard is enabled.
    bool isWriteGuardEnabled;

    /// The callbacks for native archives. These callbacks are implemented down the bottom of this file, in the platform-specific section.
    easyvfs_archive_callbacks nativeCallbacks;
};


////////////////////////////////////////
// Private API

/// When a native archive is created, the user data will be a pointer to an object of this structure.
typedef struct
{
    /// The access mode.
    easyvfs_access_mode accessMode;

}easyvfs_native_archive;


/// Native implementations for archive callbacks.
bool           easyvfs_isvalidarchive_impl_native(easyvfs_context* pContext, const char* path);
void*          easyvfs_openarchive_impl_native   (easyvfs_file* pFile, easyvfs_access_mode accessMode);
void           easyvfs_closearchive_impl_native  (easyvfs_archive* pArchive);
bool           easyvfs_getfileinfo_impl_native   (easyvfs_archive* pArchive, const char* path, easyvfs_file_info *fi);
void*          easyvfs_beginiteration_impl_native(easyvfs_archive* pArchive, const char* path);
void           easyvfs_enditeration_impl_native  (easyvfs_archive* pArchive, easyvfs_iterator* i);
bool           easyvfs_nextiteration_impl_native (easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_file_info* fi);
void*          easyvfs_openfile_impl_native      (easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode);
void           easyvfs_closefile_impl_native     (easyvfs_file* pFile);
bool           easyvfs_readfile_impl_native      (easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);
bool           easyvfs_writefile_impl_native     (easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);
bool           easyvfs_seekfile_impl_native      (easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin);
easyvfs_uint64 easyvfs_tellfile_impl_native      (easyvfs_file* pFile);
easyvfs_uint64 easyvfs_filesize_impl_native      (easyvfs_file* pFile);
void           easyvfs_flushfile_impl_native     (easyvfs_file* pFile);
bool           easyvfs_deletefile_impl_native    (easyvfs_archive* pArchive, const char* path);
bool           easyvfs_renamefile_impl_native    (easyvfs_archive* pArchive, const char* pathOld, const char* pathNew);
bool           easyvfs_mkdir_impl_native         (easyvfs_archive* pArchive, const char* path);
bool           easyvfs_copyfile_impl_native      (easyvfs_archive* pArchive, const char* srcRelativePath, const char* dstRelativePath, bool failIfExists);


easyvfs_file* easyvfs_archive_openfile(easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode, unsigned int extraDataSize);
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

int easyvfs_archive_nextiteration(easyvfs_iterator* i, easyvfs_file_info* fi)
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
easyvfs_access_mode easyvfs_archiveaccessmode(easyvfs_access_mode accessMode)
{
    return (accessMode == EASYVFS_READ) ? EASYVFS_READ : EASYVFS_READ | EASYVFS_WRITE;
}

/// Opens a native archive. This is not recursive.
easyvfs_archive* easyvfs_openarchive_native(easyvfs_context* pContext, const char* basePath, easyvfs_access_mode accessMode)
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
                memset(pNewArchive->basePath, 0, sizeof(pNewArchive->basePath));
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
easyvfs_archive* easyvfs_openarchive_nonnative(easyvfs_archive* pParentArchive, const char* path, easyvfs_access_mode accessMode)
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
                easyvfs_file* pNewArchiveFile = easyvfs_archive_openfile(pParentArchive, path, accessMode, 0); // pParentArchive->callbacks.openfile(pParentArchive, path, accessMode);
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
                            easyvfs_copy_and_append_path(pNewArchive->absolutePath, EASYVFS_MAX_PATH, pParentArchive->absolutePath, path);
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
easyvfs_archive* easyvfs_openarchive(easyvfs_context* pContext, easyvfs_archive* pParentArchive, const char* path, easyvfs_access_mode accessMode)
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
easyvfs_archive* easyvfs_openarchive_frompath_verbose(easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode, char* relativePathOut, unsigned int relativePathBufferSizeInBytes)
{
    assert(pArchive != NULL);

    easyvfs_strcpy(relativePathOut, relativePathBufferSizeInBytes, path);

    // Check that the file exists first.
    easyvfs_file_info fi;
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

        easyvfs_pathiterator iPathSegment = easyvfs_begin_path_iteration(path);
        while (easyvfs_next_path_segment(&iPathSegment))
        {
            if (easyvfs_append_path_iterator(runningPath, EASYVFS_MAX_PATH, iPathSegment))
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
                            if (easyvfs_next_path_segment(&iPathSegment))
                            {
                                easyvfs_strcpy(pChildArchive->basePath, sizeof(pChildArchive->basePath), pArchive->basePath);

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

easyvfs_archive* easyvfs_openarchive_frompath_default(easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode, char* relativePathOut, unsigned int relativePathBufferSizeInBytes)
{
    // Check that the file exists first.
    easyvfs_file_info fi;
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

        easyvfs_pathiterator iPathSegment = easyvfs_begin_path_iteration(path);
        while (easyvfs_next_path_segment(&iPathSegment))
        {
            char runningPathBase[EASYVFS_MAX_PATH];
            easyvfs_memcpy(runningPathBase, runningPath, EASYVFS_MAX_PATH);

            if (easyvfs_append_path_iterator(runningPath, EASYVFS_MAX_PATH, iPathSegment))
            {
                // Check if the path segment refers to an archive.
                if (pArchive->callbacks.getfileinfo(pArchive, runningPath, &fi))
                {
                    if ((fi.attributes & EASYVFS_FILE_ATTRIBUTE_DIRECTORY) == 0)
                    {
                        // It's a normal file. This should not be the last segment in the path.
                        if (!easyvfs_at_end_of_path(iPathSegment))
                        {
                            // The file could be an archive. Try opening the archive, and then try loading the file recursively. If this fails, we need to return null.
                            easyvfs_archive* pChildArchive = easyvfs_openarchive(pArchive->pContext, pArchive, runningPath, easyvfs_archiveaccessmode(accessMode));
                            if (pChildArchive != NULL)
                            {
                                // It's an archive, so check it. The name to check is the beginning of the next path segment.
                                easyvfs_pathiterator iNextSegment = iPathSegment;
                                if (easyvfs_next_path_segment(&iNextSegment))
                                {
                                    easyvfs_strcpy(pChildArchive->basePath, sizeof(pChildArchive->basePath), pArchive->basePath);

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
                                easyvfs_strcpy(pChildArchive->basePath, sizeof(pChildArchive->basePath), pArchive->basePath);

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
easyvfs_archive* easyvfs_openarchive_frompath(easyvfs_context* pContext, const char* path, easyvfs_access_mode accessMode, char* relativePathOut, unsigned int relativePathBufferSizeInBytes)
{
    assert(pContext != NULL);
    assert(path     != NULL);

    if (easyvfs_is_path_absolute(path))
    {
        // The path is absolute. We assume it's a verbose path.
        easyvfs_archive* pRootArchive = easyvfs_openarchive_native(pContext, "", EASYVFS_READ);
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
                easyvfs_strcpy(pRootArchive->basePath, sizeof(pRootArchive->basePath), pContext->baseDirectories.pBuffer[iBasePath].absolutePath);

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


/// Opens an archive file.
///
/// @remarks
///     This will work if the path refers to an archive directly, such as "my/file.zip". It will also work for directories on the file native
///     file system such as "my/directory". It will fail, however, if the path is something like "my/file.zip/compressed.txt".
///     @par
///     Close the archive with easyvfs_closearchive_recursive().
easyvfs_archive* easyvfs_openarchivefile(easyvfs_context* pContext, const char* path, easyvfs_access_mode accessMode)
{
    if (pContext != NULL && path != NULL)
    {
        char relativePath[EASYVFS_MAX_PATH];
        easyvfs_archive* pParentArchive = easyvfs_openarchive_frompath(pContext, path, accessMode, relativePath, EASYVFS_MAX_PATH);
        if (pParentArchive != NULL)
        {
            easyvfs_archive* pActualArchive = easyvfs_openarchive(pContext, pParentArchive, relativePath, accessMode);
            if (pActualArchive == NULL)
            {
                // If the parent archive is a native archive and the absolute path is blank, it means the path could be absolute and pointing to a native folder.
                if (pParentArchive->pParentArchive == NULL && pParentArchive->absolutePath[0] == '\0')
                {
                    pActualArchive = pParentArchive;
                    strcpy_s(pActualArchive->absolutePath, EASYVFS_MAX_PATH, path);
                }
                else
                {
                    easyvfs_closearchive_recursive(pParentArchive);
                }
            }
            

            return pActualArchive;
        }
    }

    return NULL;
}


/// Opens a file from an archive.
///
/// This is not recursive.
easyvfs_file* easyvfs_archive_openfile(easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode, unsigned int extraDataSize)
{
    assert(pArchive != NULL);
    assert(path     != NULL);

    easyvfs_file* pNewFile = easyvfs_malloc(sizeof(easyvfs_file) - sizeof(pNewFile->pExtraData) + extraDataSize);
    if (pNewFile != NULL)
    {
        void* pUserData = pArchive->callbacks.openfile(pArchive, path, accessMode);
        if (pUserData != NULL)
        {
            pNewFile->pArchive      = pArchive;
            pNewFile->pUserData     = pUserData;
            pNewFile->extraDataSize = extraDataSize;
            memset(pNewFile->pExtraData, 0, extraDataSize);

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


bool easyvfs_validate_write_path(easyvfs_context* pContext, const char* absoluteOrRelativePath, char* absolutePathOut, unsigned int absolutePathOutSize)
{
    // If the path is relative, we need to convert to absolute. Then, if the write directory guard is enabled, we need to check that it's a descendant of the base path.
    if (easyvfs_is_path_relative(absoluteOrRelativePath)) {
        if (easyvfs_copy_and_append_path(absolutePathOut, absolutePathOutSize, pContext->writeBaseDirectory, absoluteOrRelativePath)) {
            absoluteOrRelativePath = absolutePathOut;
        } else {
            return 0;
        }
    } else {
        easyvfs_strcpy(absolutePathOut, absolutePathOutSize, absoluteOrRelativePath);
    }

    assert(easyvfs_is_path_absolute(absoluteOrRelativePath));

    if (easyvfs_is_write_directory_guard_enabled(pContext)) {
        if (easyvfs_is_path_descendant(absoluteOrRelativePath, pContext->writeBaseDirectory)) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return 1;
    }
}




#ifndef EASYVFS_NO_ZIP
/// Registers the archive callbacks which enables support for ZIP files.
void easyvfs_registerarchivecallbacks_zip(easyvfs_context* pContext);
#endif

#ifndef EASYVFS_NO_PAK
/// Registers the archive callbacks which enables support for Quake 2 pak files.
void easyvfs_registerarchivecallbacks_pak(easyvfs_context* pContext);
#endif

#ifndef EASYVFS_NO_MTL
/// Registers the archive callbacks which enables support for Wavefront MTL material files. The .mtl file
/// is treated as a flat archive containing a "file" for each material defined inside the .mtl file. The
/// first byte in each "file" is the very beginning of the "newmtl" statement, with the last byte being the
/// byte just before the next "newmtl" statement, or the end of the file. The name of each file is the word
/// coming after the "newmtl" token.
void easyvfs_registerarchivecallbacks_mtl(easyvfs_context* pContext);
#endif



////////////////////////////////////////
// Public API

easyvfs_context* easyvfs_create_context()
{
    easyvfs_context* pContext = easyvfs_malloc(sizeof(easyvfs_context));
    if (pContext != NULL)
    {
        if (easyvfs_callbacklist_init(&pContext->archiveCallbacks) && easyvfs_basepaths_init(&pContext->baseDirectories))
        {
            memset(pContext->writeBaseDirectory, 0, EASYVFS_MAX_PATH);
            pContext->isWriteGuardEnabled = 0;

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
            pContext->nativeCallbacks.flushfile      = easyvfs_flushfile_impl_native;
            pContext->nativeCallbacks.deletefile     = easyvfs_deletefile_impl_native;
            pContext->nativeCallbacks.renamefile     = easyvfs_renamefile_impl_native;
            pContext->nativeCallbacks.mkdir          = easyvfs_mkdir_impl_native;
            pContext->nativeCallbacks.copyfile       = easyvfs_copyfile_impl_native;

#ifndef EASYVFS_NO_ZIP
            easyvfs_registerarchivecallbacks_zip(pContext);
#endif
#ifndef EASYVFS_NO_PAK
            easyvfs_registerarchivecallbacks_pak(pContext);
#endif
#ifndef EASYVFS_NO_MTL
            easyvfs_registerarchivecallbacks_mtl(pContext);
#endif
        }
        else
        {
            easyvfs_free(pContext);
            pContext = NULL;
        }
    }

    return pContext;
}

void easyvfs_delete_context(easyvfs_context* pContext)
{
    if (pContext != NULL)
    {
        easyvfs_basepaths_uninit(&pContext->baseDirectories);
        easyvfs_callbacklist_uninit(&pContext->archiveCallbacks);
        easyvfs_free(pContext);
    }
}


void easyvfs_register_archive_callbacks(easyvfs_context* pContext, easyvfs_archive_callbacks callbacks)
{
    if (pContext != NULL)
    {
        easyvfs_callbacklist_pushback(&pContext->archiveCallbacks, callbacks);
    }
}


void easyvfs_insert_base_directory(easyvfs_context* pContext, const char* absolutePath, unsigned int index)
{
    if (pContext != 0)
    {
        easyvfs_basepaths_insert(&pContext->baseDirectories, absolutePath, index);
    }
}

void easyvfs_add_base_directory(easyvfs_context* pContext, const char* absolutePath)
{
    easyvfs_insert_base_directory(pContext, absolutePath, easyvfs_get_base_directory_count(pContext));
}

void easyvfs_remove_base_directory(easyvfs_context* pContext, const char* absolutePath)
{
    if (pContext != NULL)
    {
        for (unsigned int iPath = 0; iPath < pContext->baseDirectories.count; )
        {
            if (easyvfs_paths_equal(pContext->baseDirectories.pBuffer[iPath].absolutePath, absolutePath))
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

void easyvfs_remove_base_directory_by_index(easyvfs_context* pContext, unsigned int index)
{
    if (pContext != NULL)
    {
        easyvfs_basepaths_remove(&pContext->baseDirectories, index);
    }
}

void easyvfs_remove_all_base_directories(easyvfs_context* pContext)
{
    if (pContext != NULL)
    {
        easyvfs_basepaths_clear(&pContext->baseDirectories);
    }
}

unsigned int easyvfs_get_base_directory_count(easyvfs_context* pContext)
{
    if (pContext != NULL)
    {
        return pContext->baseDirectories.count;
    }

    return 0;
}

//bool easyvfs_get_base_directory_by_index(easyvfs_context* pContext, unsigned int index, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes)
//{
//    if (pContext != NULL && index < pContext->baseDirectories.count)
//    {
//        return easyvfs_strcpy(absolutePathOut, absolutePathBufferSizeInBytes, pContext->baseDirectories.pBuffer[index].absolutePath) == 0;
//    }
//
//    return 0;
//}

const char* easyvfs_get_base_directory_by_index(easyvfs_context* pContext, unsigned int index)
{
    if (pContext != NULL && index < pContext->baseDirectories.count)
    {
        return pContext->baseDirectories.pBuffer[index].absolutePath;
    }

    return NULL;
}

void easyvfs_set_base_write_directory(easyvfs_context* pContext, const char* absolutePath)
{
    if (pContext != NULL) {
        if (absolutePath == NULL) {
            memset(pContext->writeBaseDirectory, 0, EASYVFS_MAX_PATH);
        } else {
            easyvfs_strcpy(pContext->writeBaseDirectory, EASYVFS_MAX_PATH, absolutePath);
        }
    }
}

bool easyvfs_get_base_write_directory(easyvfs_context* pContext, char* absolutePathOut, unsigned int absolutePathOutSize)
{
    if (pContext != NULL && absolutePathOut != NULL && absolutePathOutSize != 0)
    {
        easyvfs_strcpy(absolutePathOut, absolutePathOutSize, pContext->writeBaseDirectory);
        return 1;
    }

    return 0;
}

void easyvfs_enable_write_directory_guard(easyvfs_context * pContext)
{
    if (pContext != NULL) {
        pContext->isWriteGuardEnabled = 1;
    }
}

void easyvfs_disable_write_directory_guard(easyvfs_context * pContext)
{
    if (pContext != NULL) {
        pContext->isWriteGuardEnabled = 0;
    }
}

bool easyvfs_is_write_directory_guard_enabled(easyvfs_context* pContext)
{
    if (pContext != NULL) {
        return pContext->isWriteGuardEnabled;
    }

    return 0;
}


bool easyvfs_begin_iteration(easyvfs_context* pContext, const char* path, easyvfs_iterator* iOut)
{
    if (pContext != NULL && path != NULL && iOut != NULL)
    {
        char relativePath[EASYVFS_MAX_PATH];
        easyvfs_archive* pArchive = easyvfs_openarchivefile(pContext, path, EASYVFS_READ);
        if (pArchive != NULL)
        {
            relativePath[0] = '\0';
        }
        else
        {
            // The given path is not an archive file, but it might be a directory.
            pArchive = easyvfs_openarchive_frompath(pContext, path, EASYVFS_READ, relativePath, EASYVFS_MAX_PATH);
        }

        if (pArchive != NULL)
        {
            void* pUserData = pArchive->callbacks.beginiteration(pArchive, relativePath);
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

bool easyvfs_next_iteration(easyvfs_context* pContext, easyvfs_iterator* i, easyvfs_file_info* fi)
{
    if (pContext != NULL && i != NULL && i->pUserData != NULL)
    {
        assert(i->pArchive != NULL);
        int result = i->pArchive->callbacks.nextiteration(i->pArchive, i, fi);
#if 0
        if (result)
        {
            // The absolute path returned by the callback will be relative to the archive file. We want it to be absolute.
            if (fi != NULL)
            {
                char tempPath[EASYVFS_MAX_PATH];
                easyvfs_strcpy(tempPath, EASYVFS_MAX_PATH, fi->absolutePath);

                easyvfs_copy_and_append_path(fi->absolutePath, EASYVFS_MAX_PATH, i->pArchive->absolutePath, tempPath);
            }
        }
#endif

        return result;
    }

    return 0;
}

void easyvfs_end_iteration(easyvfs_context* pContext, easyvfs_iterator* i)
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


bool easyvfs_get_file_info(easyvfs_context* pContext, const char* path, easyvfs_file_info* fi)
{
    if (pContext != NULL && path != NULL)
    {
        char relativePath[EASYVFS_MAX_PATH];

        easyvfs_archive* pArchive = easyvfs_openarchive_frompath(pContext, path, EASYVFS_READ, relativePath, EASYVFS_MAX_PATH);
        if (pArchive != NULL)
        {
            bool result = pArchive->callbacks.getfileinfo(pArchive, relativePath, fi);


            // The archive is no longer needed, so it needs to be closed.
            easyvfs_closearchive_recursive(pArchive);
            pArchive = NULL;

            return result;
        }
    }

    return 0;
}

bool easyvfs_find_absolute_path(easyvfs_context* pContext, const char* path, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes)
{
    if (pContext != NULL && path != NULL && absolutePathOut != NULL && absolutePathBufferSizeInBytes > 0)
    {
        // We retrieve the absolute path by just getting the file info which will contain it.
        easyvfs_file_info fi;
        if (easyvfs_get_file_info(pContext, path, &fi))
        {
            easyvfs_strcpy(absolutePathOut, absolutePathBufferSizeInBytes, fi.absolutePath);
            return 1;
        }
    }

    return 0;
}

bool easyvfs_find_absolute_path_explicit_base(easyvfs_context* pContext, const char* path, const char* highestPriorityBasePath, char* absolutePathOut, unsigned int absolutePathBufferSizeInBytes)
{
    if (pContext != NULL && path != NULL && highestPriorityBasePath != NULL && absolutePathOut != NULL && absolutePathBufferSizeInBytes > 0)
    {
        bool result = 0;
        easyvfs_insert_base_directory(pContext, highestPriorityBasePath, 0);
        {
            result = easyvfs_find_absolute_path(pContext, path, absolutePathOut, absolutePathBufferSizeInBytes);
        }
        easyvfs_remove_base_directory_by_index(pContext, 0);

        return result;
    }

    return 0;
}


bool easyvfs_is_archive(easyvfs_context* pContext, const char* path)
{
    if (pContext != NULL && path != NULL)
    {
        for (unsigned int i = 0; i < pContext->archiveCallbacks.count; ++i)
        {
            easyvfs_archive_callbacks* pCallbacks = pContext->archiveCallbacks.pBuffer + i;
            if (pCallbacks->isvalidarchive(pContext, path))
            {
                return 1;
            }
        }
    }

    return 0;
}


bool easyvfs_delete_file(easyvfs_context* pContext, const char* path)
{
    int result = 0;
    if (pContext != NULL && path != NULL)
    {
        char absolutePath[EASYVFS_MAX_PATH];
        if (easyvfs_validate_write_path(pContext, path, absolutePath, EASYVFS_MAX_PATH)) {
            path = absolutePath;
        } else {
            return 0;
        }


        char relativePath[EASYVFS_MAX_PATH];
        easyvfs_archive* pArchive = easyvfs_openarchive_frompath(pContext, path, EASYVFS_READ | EASYVFS_WRITE, relativePath, EASYVFS_MAX_PATH);
        if (pArchive != NULL)
        {
            result = pArchive->callbacks.deletefile(pArchive, relativePath);

            easyvfs_closearchive_recursive(pArchive);
        }
    }

    return result;
}

bool easyvfs_rename_file(easyvfs_context* pContext, const char* pathOld, const char* pathNew)
{
    // Renaming/moving is not supported across different archives.

    int result = 0;
    if (pContext != NULL && pathOld != NULL && pathNew != NULL)
    {
        char absolutePathOld[EASYVFS_MAX_PATH];
        if (easyvfs_validate_write_path(pContext, pathOld, absolutePathOld, EASYVFS_MAX_PATH)) {
            pathOld = absolutePathOld;
        } else {
            return 0;
        }

        char absolutePathNew[EASYVFS_MAX_PATH];
        if (easyvfs_validate_write_path(pContext, pathNew, absolutePathNew, EASYVFS_MAX_PATH)) {
            pathNew = absolutePathNew;
        } else {
            return 0;
        }


        char relativePathOld[EASYVFS_MAX_PATH];
        easyvfs_archive* pArchiveOld = easyvfs_openarchive_frompath(pContext, pathOld, EASYVFS_READ | EASYVFS_WRITE, relativePathOld, EASYVFS_MAX_PATH);
        if (pArchiveOld != NULL)
        {
            char relativePathNew[EASYVFS_MAX_PATH];
            easyvfs_archive* pArchiveNew = easyvfs_openarchive_frompath(pContext, pathNew, EASYVFS_READ | EASYVFS_WRITE, relativePathNew, EASYVFS_MAX_PATH);
            if (pArchiveNew != NULL)
            {
                if (easyvfs_paths_equal(pArchiveOld->absolutePath, pArchiveNew->absolutePath))
                {
                    result = pArchiveOld->callbacks.renamefile(pArchiveOld, relativePathOld, relativePathNew);
                }

                easyvfs_closearchive_recursive(pArchiveNew);
            }

            easyvfs_closearchive_recursive(pArchiveOld);
        }
    }

    return result;
}

bool easyvfs_mkdir(easyvfs_context* pContext, const char* path)
{
    bool result = false;
    if (pContext != NULL && path != NULL)
    {
        char absolutePath[EASYVFS_MAX_PATH];
        if (easyvfs_validate_write_path(pContext, path, absolutePath, EASYVFS_MAX_PATH)) {
            path = absolutePath;
        } else {
            return false;
        }

        char relativePath[EASYVFS_MAX_PATH];
        easyvfs_archive* pArchive = easyvfs_openarchive_frompath(pContext, path, EASYVFS_READ | EASYVFS_WRITE, relativePath, EASYVFS_MAX_PATH);
        if (pArchive != NULL)
        {
            result = pArchive->callbacks.mkdir(pArchive, relativePath);

            easyvfs_closearchive_recursive(pArchive);
        }
    }

    return result;
}


bool easyvfs_copy_file(easyvfs_context* pContext, const char* srcPath, const char* dstPath, bool failIfExists)
{
    bool result = false;
    if (pContext != NULL && srcPath != NULL && dstPath != NULL)
    {
        char dstPathAbsolute[EASYVFS_MAX_PATH];
        if (easyvfs_validate_write_path(pContext, dstPath, dstPathAbsolute, sizeof(dstPathAbsolute))) {
            dstPath = dstPathAbsolute;
        } else {
            return false;
        }


        // We want to open the archive of both the source and destination. If they are the same archive we'll do an intra-archive copy.
        char srcRelativePath[EASYVFS_MAX_PATH];
        easyvfs_archive* pSrcArchive = easyvfs_openarchive_frompath(pContext, srcPath, EASYVFS_READ, srcRelativePath, sizeof(srcRelativePath));
        if (pSrcArchive == NULL) {
            return false;
        }

        char dstRelativePath[EASYVFS_MAX_PATH];
        easyvfs_archive* pDstArchive = easyvfs_openarchive_frompath(pContext, dstPath, EASYVFS_READ | EASYVFS_WRITE, dstRelativePath, sizeof(dstRelativePath));
        if (pDstArchive == NULL) {
            easyvfs_closearchive_recursive(pSrcArchive);
            return false;
        }


        

        if (strcmp(pSrcArchive->absolutePath, pDstArchive->absolutePath) == 0)
        {
            // Intra-archive copy.
            result = pDstArchive->callbacks.copyfile(pDstArchive, srcRelativePath, dstRelativePath, failIfExists);
        }
        else
        {
            bool areBothArchivesNative = (pSrcArchive->callbacks.copyfile == pDstArchive->callbacks.copyfile && pDstArchive->callbacks.copyfile == easyvfs_copyfile_impl_native);
            if (areBothArchivesNative)
            {
                char srcPathAbsolute[EASYVFS_MAX_PATH];
                easyvfs_copy_and_append_path(srcPathAbsolute, sizeof(srcPathAbsolute), pSrcArchive->absolutePath, srcPath);

                result = easyvfs_copyfile_impl_native(NULL, srcPathAbsolute, dstPathAbsolute, failIfExists);
            }
            else
            {
                // Inter-archive copy. This is a theoretically slower path because we do everything manually. Probably not much slower in practice, though.
                if (failIfExists && pDstArchive->callbacks.getfileinfo(pDstArchive, dstRelativePath, NULL) == true)
                {
                    result = false;
                }
                else
                {
                    easyvfs_file* pSrcFile = easyvfs_archive_openfile(pSrcArchive, srcRelativePath, EASYVFS_READ, 0);
                    easyvfs_file* pDstFile = easyvfs_archive_openfile(pDstArchive, dstRelativePath, EASYVFS_WRITE, 0);
                    if (pSrcFile != NULL && pDstFile != NULL)
                    {
                        char chunk[4096];
                        unsigned int bytesRead;
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

                    easyvfs_archive_closefile(pSrcArchive, pSrcFile);
                    easyvfs_archive_closefile(pDstArchive, pDstFile);
                }
            }
        }


        easyvfs_closearchive_recursive(pSrcArchive);
        easyvfs_closearchive_recursive(pDstArchive);
    }

    return result;
}



easyvfs_file* easyvfs_open(easyvfs_context* pContext, const char* absoluteOrRelativePath, easyvfs_access_mode accessMode, unsigned int extraDataSize)
{
    if (pContext != NULL && absoluteOrRelativePath)
    {
        char absolutePathForWriteMode[EASYVFS_MAX_PATH];
        if ((accessMode & EASYVFS_WRITE) != 0) {
            if (easyvfs_validate_write_path(pContext, absoluteOrRelativePath, absolutePathForWriteMode, EASYVFS_MAX_PATH)) {
                absoluteOrRelativePath = absolutePathForWriteMode;
            } else {
                return NULL;
            }
        }


        char relativePath[EASYVFS_MAX_PATH];
        easyvfs_archive* pArchive = easyvfs_openarchive_frompath(pContext, absoluteOrRelativePath, easyvfs_archiveaccessmode(accessMode), relativePath, EASYVFS_MAX_PATH);
        if (pArchive != NULL)
        {
            easyvfs_file* pFile = easyvfs_archive_openfile(pArchive, relativePath, accessMode, extraDataSize);
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

void easyvfs_close(easyvfs_file* pFile)
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

bool easyvfs_read(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.readfile(pFile, dst, bytesToRead, bytesReadOut);
    }

    return false;
}

bool easyvfs_write(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.writefile(pFile, src, bytesToWrite, bytesWrittenOut);
    }

    return false;
}

bool easyvfs_seek(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.seekfile(pFile, bytesToSeek, origin);
    }

    return false;
}

easyvfs_uint64 easyvfs_tell(easyvfs_file* pFile)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.tellfile(pFile);
    }

    return 0;
}

easyvfs_uint64 easyvfs_file_size(easyvfs_file* pFile)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        return pFile->pArchive->callbacks.filesize(pFile);
    }

    return 0;
}

void easyvfs_flush(easyvfs_file* pFile)
{
    if (pFile != NULL)
    {
        assert(pFile->pArchive  != NULL);
        assert(pFile->pUserData != NULL);

        pFile->pArchive->callbacks.flushfile(pFile);
    }
}


unsigned int easyvfs_get_extra_data_size(easyvfs_file* pFile)
{
    if (pFile != NULL) {
        return pFile->extraDataSize;
    }

    return 0;
}

void* easyvfs_get_extra_data(easyvfs_file* pFile)
{
    if (pFile != NULL) {
        return pFile->pExtraData;
    }

    return NULL;
}



//////////////////////////////////////
// High Level API

bool easyvfs_is_base_directory(easyvfs_context * pContext, const char * baseDir)
{
    if (pContext != NULL)
    {
        for (unsigned int i = 0; i < easyvfs_get_base_directory_count(pContext); ++i)
        {
            if (easyvfs_paths_equal(easyvfs_get_base_directory_by_index(pContext, i), baseDir))
            {
                return true;
            }
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

    easyvfs_uint64 fileSize = easyvfs_file_size(pFile);
    if (fileSize > SIZE_MAX)
    {
        // File's too big.
        easyvfs_close(pFile);
        return NULL;
    }


    void* pData = easyvfs_malloc((size_t)fileSize);
    if (pData == NULL)
    {
        // Failed to allocate memory.
        easyvfs_close(pFile);
        return NULL;
    }


    char* pDst = pData;
    easyvfs_uint64 bytesRemaining = fileSize;
    while (bytesRemaining > 0)
    {
        unsigned int bytesToProcess;
        if (bytesRemaining > UINT_MAX) {
            bytesToProcess = UINT_MAX;
        } else {
            bytesToProcess = (unsigned int)bytesRemaining;
        }

        if (!easyvfs_read(pFile, pDst, bytesToProcess, NULL))
        {
            easyvfs_free(pData);
            easyvfs_close(pFile);
            return NULL;
        }

        pDst += bytesToProcess;
        bytesRemaining -= bytesToProcess;
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

    easyvfs_uint64 fileSize = easyvfs_file_size(pFile);
    if (fileSize > SIZE_MAX)
    {
        // File's too big.
        easyvfs_close(pFile);
        return NULL;
    }


    void* pData = easyvfs_malloc((size_t)fileSize + 1);     // +1 for null terminator.
    if (pData == NULL)
    {
        // Failed to allocate memory.
        easyvfs_close(pFile);
        return NULL;
    }


    char* pDst = pData;
    easyvfs_uint64 bytesRemaining = fileSize;
    while (bytesRemaining > 0)
    {
        unsigned int bytesToProcess;
        if (bytesRemaining > UINT_MAX) {
            bytesToProcess = UINT_MAX;
        } else {
            bytesToProcess = (unsigned int)bytesRemaining;
        }

        if (!easyvfs_read(pFile, pDst, bytesToProcess, NULL))
        {
            easyvfs_free(pData);
            easyvfs_close(pFile);
            return NULL;
        }

        pDst += bytesToProcess;
        bytesRemaining -= bytesToProcess;
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
    easyvfs_file* pFile = easyvfs_open(pContext, absoluteOrRelativePath, EASYVFS_WRITE, 0);
    if (pFile == NULL) {
        return false;
    }

    const char* pSrc = pData;
    
    size_t bytesRemaining = dataSize;
    while (bytesRemaining > 0)
    {
        unsigned int bytesToProcess;
        if (bytesRemaining > UINT_MAX) {
            bytesToProcess = UINT_MAX;
        } else {
            bytesToProcess = (unsigned int)bytesRemaining;
        }

        if (!easyvfs_write(pFile, pSrc, bytesToProcess, NULL))
        {
            easyvfs_close(pFile);
            return false;
        }

        pSrc += bytesToProcess;
        bytesRemaining -= bytesToProcess;
    }
    
    easyvfs_close(pFile);
    return true;
}

bool easyvfs_open_and_write_text_file(easyvfs_context* pContext, const char* absoluteOrRelativePath, const char* pTextData)
{
    return easyvfs_open_and_write_binary_file(pContext, absoluteOrRelativePath, pTextData, strlen(pTextData));
}


bool easyvfs_exists(easyvfs_context * pContext, const char * absoluteOrRelativePath)
{
    easyvfs_file_info fi;
    return easyvfs_get_file_info(pContext, absoluteOrRelativePath, &fi);
}

bool easyvfs_is_existing_file(easyvfs_context * pContext, const char * absoluteOrRelativePath)
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


bool easyvfs_mkdir_recursive(easyvfs_context* pContext, const char* path)
{
    // We just iterate over each segment and try creating each directory if it doesn't exist.
    bool result = false;
    if (pContext != NULL && path != NULL)
    {
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

                if (!easyvfs_is_existing_directory(pContext, runningPath))
                {
                    if (!easyvfs_mkdir(pContext, runningPath)) {
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

                if (!easyvfs_mkdir(pContext, runningPath)) {
                    return false;
                }
            }


            result = true;
        }
    }

    return result;
}

bool easyvfs_eof(easyvfs_file* pFile)
{
    return easyvfs_tell(pFile) == easyvfs_file_size(pFile);
}




///////////////////////////////////////////
// Utilities

bool easyvfs_is_path_child(const char* childAbsolutePath, const char* parentAbsolutePath)
{
#if !EASYVFS_USE_EASYPATH
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
#else
    return easypath_is_child(childAbsolutePath, parentAbsolutePath);
#endif
}

bool easyvfs_is_path_descendant(const char* descendantAbsolutePath, const char* parentAbsolutePath)
{
#if !EASYVFS_USE_EASYPATH
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
#else
    return easypath_is_descendant(descendantAbsolutePath, parentAbsolutePath);
#endif
}

bool easyvfs_copy_base_path(const char* path, char* baseOut, unsigned int baseSizeInBytes)
{
#if !EASYVFS_USE_EASYPATH
    if (easyvfs_strcpy(baseOut, baseSizeInBytes, path) == 0)
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
#else
    easypath_copy_base_path(path, baseOut, baseSizeInBytes);
    return 1;
#endif
}

const char* easyvfs_file_name(const char* path)
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
    return easypath_file_name(path);
#endif
}

const char* easyvfs_extension(const char* path)
{
#if !EASYVFS_USE_EASYPATH
    if (path != 0)
    {
        const char* extension     = easyvfs_file_name(path);
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

bool easyvfs_extension_equal(const char* path, const char* extension)
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
    return easypath_extension_equal(path, extension);
#endif
}

bool easyvfs_paths_equal(const char* path1, const char* path2)
{
#if !EASYVFS_USE_EASYPATH
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
#else
    return easypath_equal(path1, path2);
#endif
}

bool easyvfs_is_path_relative(const char* path)
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
    return easypath_is_relative(path);
#endif
}

bool easyvfs_is_path_absolute(const char* path)
{
    return !easyvfs_is_path_relative(path);
}

bool easyvfs_appendpath(char* base, unsigned int baseBufferSizeInBytes, const char* other)
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

            easyvfs_strncpy(base + path1Length, baseBufferSizeInBytes - path1Length, other, path2Length);


            return 1;
        }
    }

    return 0;
#else
    return easypath_append(base, baseBufferSizeInBytes, other);
#endif
}

bool easyvfs_copy_and_append_path(char* dst, unsigned int dstSizeInBytes, const char* base, const char* other)
{
#if !EASYVFS_USE_EASYPATH
    if (dst != NULL && dstSizeInBytes > 0)
    {
        easyvfs_strcpy(dst, dstSizeInBytes, base);
        return easyvfs_appendpath(dst, dstSizeInBytes, other);
    }

    return 0;
#else
    return easypath_copy_and_append(dst, dstSizeInBytes, base, other);
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

bool easyvfs_isvalidarchive_impl_native(easyvfs_context* pContext, const char* path)
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

void* easyvfs_openarchive_impl_native(easyvfs_file* pFile, easyvfs_access_mode accessMode)
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

bool easyvfs_getfileinfo_impl_native(easyvfs_archive* pArchive, const char* path, easyvfs_file_info *fi)
{
    assert(path != NULL);

    if (pArchive != NULL)
    {
        // Assume "path" is relative. Convert to absolute and recursively call this function with a null archive.

        char absolutePath[EASYVFS_MAX_PATH];
        if (easyvfs_copy_and_append_path(absolutePath, sizeof(absolutePath), pArchive->absolutePath, path))
        {
            return easyvfs_getfileinfo_impl_native(NULL, absolutePath, fi);
        }
    }
    else
    {
        // Assume "path" is absolute.

        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
        {
            if (fi != NULL)
            {
                ULARGE_INTEGER liSize;
                liSize.LowPart  = fad.nFileSizeLow;
                liSize.HighPart = fad.nFileSizeHigh;

                ULARGE_INTEGER liTime;
                liTime.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
                liTime.HighPart = fad.ftLastWriteTime.dwHighDateTime;

                easyvfs_memcpy(fi->absolutePath, path, EASYVFS_MAX_PATH);
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

            return true;
        }
    }

    return false;
}

void* easyvfs_beginiteration_impl_native(easyvfs_archive* pArchive, const char* path)
{
    assert(path != NULL);

    if (pArchive != NULL)
    {
        // Assume "path" is relative. Convert to absolute and recursively call this function with a null archive.

        char absolutePath[EASYVFS_MAX_PATH];
        if (easyvfs_copy_and_append_path(absolutePath, sizeof(absolutePath), pArchive->absolutePath, path))
        {
            return easyvfs_beginiteration_impl_native(NULL, absolutePath);
        }
    }
    else
    {
        // Assume "path" is absolute.

        char searchQuery[EASYVFS_MAX_PATH];
        easyvfs_strcpy(searchQuery, sizeof(searchQuery), path);

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


    return NULL;
}

void easyvfs_enditeration_impl_native(easyvfs_archive* pArchive, easyvfs_iterator* i)
{
    assert(i != NULL);

    (void)pArchive;

    easyvfs_iterator_win32* pUserData = i->pUserData;
    if (pUserData != NULL)
    {
        FindClose(pUserData->hFind);
        easyvfs_free(pUserData);

        i->pUserData = NULL;
    }
}

bool easyvfs_nextiteration_impl_native(easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_file_info* fi)
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
            easyvfs_copy_and_append_path(fi->absolutePath, EASYVFS_MAX_PATH, pUserData->directoryPath, pUserData->ffd.cFileName);

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

void* easyvfs_openfile_impl_native(easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode)
{
    assert(pArchive            != NULL);
    assert(pArchive->pUserData != NULL);
    assert(path                != NULL);

    char fullPath[EASYVFS_MAX_PATH];
    if (easyvfs_copy_and_append_path(fullPath, EASYVFS_MAX_PATH, pArchive->absolutePath, path))
    {
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
                if ((accessMode & EASYVFS_APPEND) != 0) {
                    dwCreationDisposition = OPEN_EXISTING;
                } else {
                    dwCreationDisposition = TRUNCATE_EXISTING;
                }
            } else {
                if ((accessMode & EASYVFS_APPEND) != 0) {
                    dwCreationDisposition = OPEN_ALWAYS;
                } else {
                    dwCreationDisposition = CREATE_ALWAYS;
                }
            }
        }


        HANDLE hFile = CreateFileA(fullPath, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            // We failed to create the file, however it may be because we are trying to create the file and the directory structure does not exist yet. In this
            // case we want to try creating the directory structure and try again.
            if ((accessMode & EASYVFS_WRITE) != 0 && (accessMode & EASYVFS_CREATE_DIRS) != 0)
            {
                char dirPath[EASYVFS_MAX_PATH];
                if (easyvfs_copy_base_path(fullPath, dirPath, sizeof(dirPath)))
                {
                    if (!easyvfs_is_existing_directory(pArchive->pContext, dirPath))    // Don't try creating the directory if it already exists.
                    {
                        if (easyvfs_mkdir_recursive(pArchive->pContext, dirPath))
                        {
                            // At this point we should have the directory structure in place and we can try creating the file again.
                            hFile = CreateFileA(fullPath, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
                        }
                    }
                }
            }
        }

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
    assert(pFile->pUserData != NULL);

    CloseHandle((HANDLE)pFile->pUserData);
}

bool easyvfs_readfile_impl_native(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut)
{
    assert(pFile            != NULL);
    assert(pFile->pUserData != NULL);

    DWORD bytesRead;
    BOOL result = ReadFile((HANDLE)pFile->pUserData, dst, (DWORD)bytesToRead, &bytesRead, NULL);
    if (result && bytesReadOut != NULL)
    {
        *bytesReadOut = bytesRead;
    }

    return result != 0;
}

bool easyvfs_writefile_impl_native(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut)
{
    assert(pFile            != NULL);
    assert(pFile->pUserData != NULL);

    DWORD bytesWritten;
    BOOL result = WriteFile((HANDLE)pFile->pUserData, src, (DWORD)bytesToWrite, &bytesWritten, NULL);
    if (result && bytesWrittenOut != NULL)
    {
        *bytesWrittenOut = bytesWritten;
    }

    return result != 0;
}

bool easyvfs_seekfile_impl_native(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
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
    assert(pFile            != NULL);
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
    assert(pFile            != NULL);
    assert(pFile->pUserData != NULL);

    LARGE_INTEGER fileSize;
    if (GetFileSizeEx((HANDLE)pFile->pUserData, &fileSize))
    {
        return (easyvfs_uint64)fileSize.QuadPart;
    }

    return 0;
}

void easyvfs_flushfile_impl_native(easyvfs_file* pFile)
{
    assert(pFile            != NULL);
    assert(pFile->pUserData != NULL);

    FlushFileBuffers((HANDLE)pFile->pUserData);
}

bool easyvfs_deletefile_impl_native(easyvfs_archive* pArchive, const char* path)
{
    assert(path != NULL);

    if (pArchive != NULL)
    {
        // Assume "path" is relative. Convert to absolute and recursively call this function with a null archive.

        char absolutePath[EASYVFS_MAX_PATH];
        if (easyvfs_copy_and_append_path(absolutePath, sizeof(absolutePath), pArchive->absolutePath, path))
        {
            return easyvfs_deletefile_impl_native(NULL, absolutePath);
        }
    }
    else
    {
        // Assume "path" is absolute.

        DWORD attributes = GetFileAttributesA(path);
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            // It's a normal file.
            return DeleteFileA(path);
        }
        else
        {
            // It's a directory.
            return RemoveDirectoryA(path);
        }
    }

    return false;
}

bool easyvfs_renamefile_impl_native(easyvfs_archive* pArchive, const char* pathOld, const char* pathNew)
{
    assert(pathOld != NULL);
    assert(pathNew != NULL);

    if (pArchive != NULL)
    {
        char absolutePathOld[EASYVFS_MAX_PATH];
        char absolutePathNew[EASYVFS_MAX_PATH];
        easyvfs_copy_and_append_path(absolutePathOld, sizeof(absolutePathOld), pArchive->absolutePath, pathOld);
        easyvfs_copy_and_append_path(absolutePathNew, sizeof(absolutePathNew), pArchive->absolutePath, pathNew);

        return MoveFileExA(absolutePathOld, absolutePathNew, 0);
    }
    else
    {
        return MoveFileExA(pathOld, pathNew, 0);
    }
}

bool easyvfs_mkdir_impl_native(easyvfs_archive* pArchive, const char* path)
{
    assert(path != NULL);

    if (pArchive != NULL)
    {
        char absolutePath[EASYVFS_MAX_PATH];
        if (easyvfs_copy_and_append_path(absolutePath, sizeof(absolutePath), pArchive->absolutePath, path))
        {
            return CreateDirectoryA(absolutePath, NULL);
        }
    }
    else
    {
        return CreateDirectoryA(path, NULL);
    }

    return false;
}

bool easyvfs_copyfile_impl_native(easyvfs_archive* pArchive, const char* srcPath, const char* dstPath, bool failIfExists)
{
    char srcPathAbsolute[EASYVFS_MAX_PATH];
    char dstPathAbsolute[EASYVFS_MAX_PATH];
    if (pArchive != NULL)
    {
        easyvfs_copy_and_append_path(srcPathAbsolute, sizeof(srcPathAbsolute), pArchive->absolutePath, srcPath);
        easyvfs_copy_and_append_path(dstPathAbsolute, sizeof(dstPathAbsolute), pArchive->absolutePath, dstPath);

        return CopyFileA(srcPathAbsolute, dstPathAbsolute, failIfExists);
    }
    else
    {
        return CopyFileA(srcPath, dstPath, failIfExists);
    }
}
#endif






///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// Archive Formats
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// ZIP
//
///////////////////////////////////////////////////////////////////////////////
#ifndef EASYVFS_NO_ZIP
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

typedef struct
{
    /// The current index of the iterator. When this hits the file count, the iteration is finished.
    unsigned int index;

    /// The directory being iterated.
    char directoryPath[EASYVFS_MAX_PATH];

}easyvfs_iterator_zip;

typedef struct
{
    /// The file index within the archive.
    mz_uint index;

    /// A pointer to the buffer containing the entire uncompressed data of the file. Unfortunately this is the only way I'm aware of for
    /// reading file data from miniz.c so we'll just stick with it for now. We use a pointer to an 8-bit type so we can easily calculate
    /// offsets.
    mz_uint8* pData;

    /// The size of the file in bytes so we can guard against overflowing reads.
    size_t sizeInBytes;

    /// The current position of the file's read pointer.
    size_t readPointer;

}easyvfs_openedfile_zip;



bool           easyvfs_isvalidarchive_zip(easyvfs_context* pContext, const char* path);
void*          easyvfs_openarchive_zip   (easyvfs_file* pFile, easyvfs_access_mode accessMode);
void           easyvfs_closearchive_zip  (easyvfs_archive* pArchive);
bool           easyvfs_getfileinfo_zip   (easyvfs_archive* pArchive, const char* path, easyvfs_file_info *fi);
void*          easyvfs_beginiteration_zip(easyvfs_archive* pArchive, const char* path);
void           easyvfs_enditeration_zip  (easyvfs_archive* pArchive, easyvfs_iterator* i);
bool           easyvfs_nextiteration_zip (easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_file_info* fi);
void*          easyvfs_openfile_zip      (easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode);
void           easyvfs_closefile_zip     (easyvfs_file* pFile);
bool           easyvfs_readfile_zip      (easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);
bool           easyvfs_writefile_zip     (easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);
bool           easyvfs_seekfile_zip      (easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin);
easyvfs_uint64 easyvfs_tellfile_zip      (easyvfs_file* pFile);
easyvfs_uint64 easyvfs_filesize_zip      (easyvfs_file* pFile);
void           easyvfs_flushfile_zip     (easyvfs_file* pFile);
bool           easyvfs_deletefile_zip    (easyvfs_archive* pArchive, const char* path);
bool           easyvfs_renamefile_zip    (easyvfs_archive* pArchive, const char* pathOld, const char* pathNew);
bool           easyvfs_mkdir_zip         (easyvfs_archive* pArchive, const char* path);
bool           easyvfs_copy_file_zip     (easyvfs_archive* pArchive, const char* srcPath, const char* dstPath, bool failIfExists);

void easyvfs_registerarchivecallbacks_zip(easyvfs_context* pContext)
{
    easyvfs_archive_callbacks callbacks;
    callbacks.isvalidarchive = easyvfs_isvalidarchive_zip;
    callbacks.openarchive    = easyvfs_openarchive_zip;
    callbacks.closearchive   = easyvfs_closearchive_zip;
    callbacks.getfileinfo    = easyvfs_getfileinfo_zip;
    callbacks.beginiteration = easyvfs_beginiteration_zip;
    callbacks.enditeration   = easyvfs_enditeration_zip;
    callbacks.nextiteration  = easyvfs_nextiteration_zip;
    callbacks.openfile       = easyvfs_openfile_zip;
    callbacks.closefile      = easyvfs_closefile_zip;
    callbacks.readfile       = easyvfs_readfile_zip;
    callbacks.writefile      = easyvfs_writefile_zip;
    callbacks.seekfile       = easyvfs_seekfile_zip;
    callbacks.tellfile       = easyvfs_tellfile_zip;
    callbacks.filesize       = easyvfs_filesize_zip;
    callbacks.flushfile      = easyvfs_flushfile_zip;
    callbacks.deletefile     = easyvfs_deletefile_zip;
    callbacks.renamefile     = easyvfs_renamefile_zip;
    callbacks.mkdir          = easyvfs_mkdir_zip;
    callbacks.copyfile       = easyvfs_copy_file_zip;
    easyvfs_register_archive_callbacks(pContext, callbacks);
}


bool easyvfs_isvalidarchive_zip(easyvfs_context* pContext, const char* path)
{
    (void)pContext;

    if (easyvfs_extension_equal(path, "zip"))
    {
        return 1;
    }

    return 0;
}


size_t easyvfs_mz_file_read_func(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n)
{
    // The opaque type is a pointer to a easyvfs_file object which represents the file of the archive.
    easyvfs_file* pZipFile = pOpaque;
    assert(pZipFile != NULL);

    easyvfs_seek(pZipFile, (easyvfs_int64)file_ofs, easyvfs_start);

    unsigned int bytesRead;
    int result = easyvfs_read(pZipFile, pBuf, (unsigned int)n, &bytesRead);
    if (result == 0)
    {
        // Failed to read the file.
        bytesRead = 0;
    }

    return (size_t)bytesRead;
}


void* easyvfs_openarchive_zip(easyvfs_file* pFile, easyvfs_access_mode accessMode)
{
    assert(pFile != NULL);
    assert(easyvfs_tell(pFile) == 0);

    // Only support read-only mode at the moment.
    if ((accessMode & EASYVFS_WRITE) != 0)
    {
        return NULL;
    }


    mz_zip_archive* pZip = easyvfs_malloc(sizeof(mz_zip_archive));
    if (pZip != NULL)
    {
        memset(pZip, 0, sizeof(mz_zip_archive));

        pZip->m_pRead        = easyvfs_mz_file_read_func;
        pZip->m_pIO_opaque   = pFile;
        if (mz_zip_reader_init(pZip, easyvfs_file_size(pFile), 0))
        {
            // Everything is good so far...
        }
        else
        {
            easyvfs_free(pZip);
            pZip = NULL;
        }
    }

    return pZip;
}

void easyvfs_closearchive_zip(easyvfs_archive* pArchive)
{
    assert(pArchive != NULL);
    assert(pArchive->pUserData != NULL);

    mz_zip_reader_end(pArchive->pUserData);
    easyvfs_free(pArchive->pUserData);
}

bool easyvfs_getfileinfo_zip(easyvfs_archive* pArchive, const char* path, easyvfs_file_info *fi)
{
    assert(pArchive != NULL);
    assert(pArchive->pUserData != NULL);

    mz_zip_archive* pZip = pArchive->pUserData;
    int fileIndex = mz_zip_reader_locate_file(pZip, path, NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (fileIndex != -1)
    {
        if (fi != NULL)
        {
            mz_zip_archive_file_stat zipStat;
            if (mz_zip_reader_file_stat(pZip, (mz_uint)fileIndex, &zipStat))
            {
                easyvfs_copy_and_append_path(fi->absolutePath, EASYVFS_MAX_PATH, pArchive->absolutePath, path);
                fi->sizeInBytes      = zipStat.m_uncomp_size;
                fi->lastModifiedTime = (easyvfs_uint64)zipStat.m_time;
                fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;
                if (mz_zip_reader_is_file_a_directory(pZip, (mz_uint)fileIndex))
                {
                    fi->attributes |= EASYVFS_FILE_ATTRIBUTE_DIRECTORY;
                }

                return 1;
            }
        }

        return 1;
    }
    else
    {
        // There was an error finding the file. It probably doesn't exist.
    }

    return 0;
}

void* easyvfs_beginiteration_zip(easyvfs_archive* pArchive, const char* path)
{
    assert(pArchive != 0);
    assert(pArchive->pUserData != NULL);
    assert(path != NULL);

    mz_zip_archive* pZip = pArchive->pUserData;

    int directoryFileIndex = -1;
    if (path[0] == '\0') {
        directoryFileIndex = 0;
    } else {
        directoryFileIndex = mz_zip_reader_locate_file(pZip, path, NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    }
    
    if (directoryFileIndex != -1)
    {
        easyvfs_iterator_zip* pZipIterator = easyvfs_malloc(sizeof(easyvfs_iterator_zip));
        if (pZipIterator != NULL)
        {
            pZipIterator->index = 0;
            easyvfs_strcpy(pZipIterator->directoryPath, EASYVFS_MAX_PATH, path);
        }

        return pZipIterator;
    }

    return NULL;
}

void easyvfs_enditeration_zip(easyvfs_archive* pArchive, easyvfs_iterator* i)
{
    (void)pArchive;

    assert(pArchive != 0);
    assert(pArchive->pUserData != NULL);
    assert(i != NULL);

    easyvfs_free(i->pUserData);
    i->pUserData = NULL;
}

bool easyvfs_nextiteration_zip(easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_file_info* fi)
{
    assert(pArchive != 0);
    assert(i != NULL);

    easyvfs_iterator_zip* pZipIterator = i->pUserData;
    if (pZipIterator != NULL)
    {
        mz_zip_archive* pZip = pArchive->pUserData;

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
                            easyvfs_strcpy(fi->absolutePath, EASYVFS_MAX_PATH, filePath);
                            fi->sizeInBytes      = zipStat.m_uncomp_size;
                            fi->lastModifiedTime = (easyvfs_uint64)zipStat.m_time;
                            fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;
                            if (mz_zip_reader_is_file_a_directory(pZip, iFile))
                            {
                                fi->attributes |= EASYVFS_FILE_ATTRIBUTE_DIRECTORY;
                            }
                        }
                    }

                    return 1;
                }
            }
        }
    }

    return 0;
}

void* easyvfs_openfile_zip(easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode)
{
    assert(pArchive != 0);
    assert(pArchive->pUserData != NULL);
    assert(path != NULL);

    // Only supporting read-only for now.
    if ((accessMode & EASYVFS_WRITE) != 0)
    {
        return NULL;
    }


    mz_zip_archive* pZip = pArchive->pUserData;
    int fileIndex = mz_zip_reader_locate_file(pZip, path, NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (fileIndex != -1)
    {
        easyvfs_openedfile_zip* pOpenedFile = easyvfs_malloc(sizeof(easyvfs_openedfile_zip));
        if (pOpenedFile != NULL)
        {
            pOpenedFile->pData = mz_zip_reader_extract_to_heap(pZip, (mz_uint)fileIndex, &pOpenedFile->sizeInBytes, 0);
            if (pOpenedFile->pData != NULL)
            {
                pOpenedFile->index       = (mz_uint)fileIndex;
                pOpenedFile->readPointer = 0;
            }
            else
            {
                // Failed to read the data.
                easyvfs_free(pOpenedFile);
                pOpenedFile = NULL;
            }
        }

        return pOpenedFile;
    }
    else
    {
        // Couldn't find the file.
    }

    return NULL;
}

void easyvfs_closefile_zip(easyvfs_file* pFile)
{
    assert(pFile != 0);

    easyvfs_openedfile_zip* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        mz_zip_archive* pZip = pFile->pArchive->pUserData;
        assert(pZip != NULL);

        pZip->m_pFree(pZip->m_pAlloc_opaque, pOpenedFile->pData);
        easyvfs_free(pOpenedFile);
    }
}

bool easyvfs_readfile_zip(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut)
{
    assert(pFile != 0);
    assert(dst != NULL);
    assert(bytesToRead > 0);

    easyvfs_openedfile_zip* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        size_t bytesAvailable = pOpenedFile->sizeInBytes - pOpenedFile->readPointer;
        if (bytesAvailable < bytesToRead) {
            bytesToRead = (unsigned int)bytesAvailable;
        }

        memcpy(dst, pOpenedFile->pData + pOpenedFile->readPointer, bytesToRead);
        pOpenedFile->readPointer += bytesToRead;

        if (bytesReadOut != NULL)
        {
            *bytesReadOut = bytesToRead;
        }

        return 1;
    }

    return 0;
}

bool easyvfs_writefile_zip(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut)
{
    (void)pFile;
    (void)src;
    (void)bytesToWrite;

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

bool easyvfs_seekfile_zip(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
{
    assert(pFile != 0);

    easyvfs_openedfile_zip* pOpenedFile = pFile->pUserData;
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

easyvfs_uint64 easyvfs_tellfile_zip(easyvfs_file* pFile)
{
    assert(pFile != 0);

    easyvfs_openedfile_zip* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        return pOpenedFile->readPointer;
    }

    return 0;
}

easyvfs_uint64 easyvfs_filesize_zip(easyvfs_file* pFile)
{
    assert(pFile != 0);

    easyvfs_openedfile_zip* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        return pOpenedFile->sizeInBytes;
    }

    return 0;
}

void easyvfs_flushfile_zip(easyvfs_file* pFile)
{
    (void)pFile;

    assert(pFile != 0);

    // All files are read-only for now.
}

bool easyvfs_deletefile_zip(easyvfs_archive* pArchive, const char* path)
{
    (void)pArchive;
    (void)path;

    assert(pArchive != 0);
    assert(path     != 0);

    // All files are read-only for now.
    return 0;
}

bool easyvfs_renamefile_zip(easyvfs_archive* pArchive, const char* pathOld, const char* pathNew)
{
    (void)pArchive;
    (void)pathOld;
    (void)pathNew;

    assert(pArchive != 0);
    assert(pathOld  != 0);
    assert(pathNew  != 0);

    // All files are read-only for now.
    return 0;
}

bool easyvfs_mkdir_zip(easyvfs_archive* pArchive, const char* path)
{
    (void)pArchive;
    (void)path;

    assert(pArchive != 0);
    assert(path     != 0);

    // All files are read-only for now.
    return 0;
}

bool easyvfs_copy_file_zip(easyvfs_archive* pArchive, const char* srcPath, const char* dstPath, bool failIfExists)
{
    (void)pArchive;
    (void)srcPath;
    (void)dstPath;
    (void)failIfExists;

    assert(pArchive != 0);
    assert(srcPath  != 0);
    assert(dstPath  != 0);

    // No support for this at the moment because it's read-only for now.
    return 0;
}

#endif // !EASYVFS_NO_ZIP



///////////////////////////////////////////////////////////////////////////////
//
// Quake 2 PAK
//
///////////////////////////////////////////////////////////////////////////////
#ifndef EASYVFS_NO_PAK
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
    easyvfs_access_mode accessMode;

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

bool easyvfs_iterator_pak_append_processed_dir(easyvfs_iterator_pak* pIterator, const char* path)
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

bool easyvfs_iterator_pak_has_dir_been_processed(easyvfs_iterator_pak* pIterator, const char* path)
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



easyvfs_archive_pak* easyvfs_pak_create(easyvfs_access_mode accessMode)
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



bool           easyvfs_isvalidarchive_pak(easyvfs_context* pContext, const char* path);
void*          easyvfs_openarchive_pak   (easyvfs_file* pFile, easyvfs_access_mode accessMode);
void           easyvfs_closearchive_pak  (easyvfs_archive* pArchive);
bool           easyvfs_getfileinfo_pak   (easyvfs_archive* pArchive, const char* path, easyvfs_file_info *fi);
void*          easyvfs_beginiteration_pak(easyvfs_archive* pArchive, const char* path);
void           easyvfs_enditeration_pak  (easyvfs_archive* pArchive, easyvfs_iterator* i);
bool           easyvfs_nextiteration_pak (easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_file_info* fi);
void*          easyvfs_openfile_pak      (easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode);
void           easyvfs_closefile_pak     (easyvfs_file* pFile);
bool           easyvfs_readfile_pak      (easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);
bool           easyvfs_writefile_pak     (easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);
bool           easyvfs_seekfile_pak      (easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin);
easyvfs_uint64 easyvfs_tellfile_pak      (easyvfs_file* pFile);
easyvfs_uint64 easyvfs_filesize_pak      (easyvfs_file* pFile);
void           easyvfs_flushfile_pak     (easyvfs_file* pFile);
bool           easyvfs_deletefile_pak    (easyvfs_archive* pArchive, const char* path);
bool           easyvfs_renamefile_pak    (easyvfs_archive* pArchive, const char* pathOld, const char* pathNew);
bool           easyvfs_mkdir_pak         (easyvfs_archive* pArchive, const char* path);
bool           easyvfs_copy_file_pak     (easyvfs_archive* pArchive, const char* srcPath, const char* dstPath, bool failIfExists);

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
    callbacks.flushfile      = easyvfs_flushfile_pak;
    callbacks.deletefile     = easyvfs_deletefile_pak;
    callbacks.renamefile     = easyvfs_renamefile_pak;
    callbacks.mkdir          = easyvfs_mkdir_pak;
    callbacks.copyfile       = easyvfs_copy_file_pak;
    easyvfs_register_archive_callbacks(pContext, callbacks);
}


bool easyvfs_isvalidarchive_pak(easyvfs_context* pContext, const char* path)
{
    (void)pContext;

    if (easyvfs_extension_equal(path, "pak"))
    {
        return 1;
    }

    return 0;
}


void* easyvfs_openarchive_pak(easyvfs_file* pFile, easyvfs_access_mode accessMode)
{
    assert(pFile != NULL);
    assert(easyvfs_tell(pFile) == 0);

    easyvfs_archive_pak* pak = easyvfs_pak_create(accessMode);
    if (pak != NULL)
    {
        // First 4 bytes should equal "PACK"
        if (easyvfs_read(pFile, pak->id, 4, NULL))
        {
            if (pak->id[0] == 'P' && pak->id[1] == 'A' && pak->id[2] == 'C' && pak->id[3] == 'K')
            {
                if (easyvfs_read(pFile, &pak->directoryOffset, 4, NULL))
                {
                    if (easyvfs_read(pFile, &pak->directoryLength, 4, NULL))
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
                                    if (easyvfs_seek(pFile, pak->directoryOffset, easyvfs_start))
                                    {
                                        unsigned int bytesRead;
                                        if (easyvfs_read(pFile, pak->pFiles, pak->directoryLength, &bytesRead) && bytesRead == pak->directoryLength)
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

bool easyvfs_getfileinfo_pak(easyvfs_archive* pArchive, const char* path, easyvfs_file_info* fi)
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
    (void)pArchive;

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
    (void)pArchive;

    assert(pArchive != 0);
    assert(pArchive->pUserData != NULL);
    assert(i != NULL);

    easyvfs_free(i->pUserData);
    i->pUserData = NULL;
}

bool easyvfs_nextiteration_pak(easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_file_info* fi)
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

void* easyvfs_openfile_pak(easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode)
{
    assert(pArchive != 0);
    assert(pArchive->pUserData != NULL);
    assert(path != NULL);

    // Only supporting read-only for now.
    if ((accessMode & EASYVFS_WRITE) != 0)
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

bool easyvfs_readfile_pak(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut)
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

        easyvfs_seek(pArchiveFile, (easyvfs_int64)(pOpenedFile->offsetInArchive + pOpenedFile->readPointer), easyvfs_start);
        int result = easyvfs_read(pArchiveFile, dst, bytesToRead, bytesReadOut);
        if (result != 0)
        {
            pOpenedFile->readPointer += bytesToRead;
        }

        return result;
    }

    return 0;
}

bool easyvfs_writefile_pak(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut)
{
    (void)pFile;
    (void)src;
    (void)bytesToWrite;

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

bool easyvfs_seekfile_pak(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
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

void easyvfs_flushfile_pak(easyvfs_file* pFile)
{
    (void)pFile;

    assert(pFile != 0);

    // All files are read-only for now.
}

bool easyvfs_deletefile_pak(easyvfs_archive* pArchive, const char* path)
{
    (void)pArchive;
    (void)path;

    assert(pArchive != 0);
    assert(path     != 0);

    // All files are read-only for now.
    return 0;
}

bool easyvfs_renamefile_pak(easyvfs_archive* pArchive, const char* pathOld, const char* pathNew)
{
    (void)pArchive;
    (void)pathOld;
    (void)pathNew;

    assert(pArchive != 0);
    assert(pathOld  != 0);
    assert(pathNew  != 0);

    // All files are read-only for now.
    return 0;
}

bool easyvfs_mkdir_pak(easyvfs_archive* pArchive, const char* path)
{
    (void)pArchive;
    (void)path;

    assert(pArchive != 0);
    assert(path     != 0);

    // All files are read-only for now.
    return 0;
}

bool easyvfs_copy_file_pak(easyvfs_archive* pArchive, const char* srcPath, const char* dstPath, bool failIfExists)
{
    (void)pArchive;
    (void)srcPath;
    (void)dstPath;
    (void)failIfExists;

    assert(pArchive != 0);
    assert(srcPath  != 0);
    assert(dstPath  != 0);

    // No support for this at the moment because it's read-only for now.
    return 0;
}
#endif // !EASYVFS_NO_PAK




///////////////////////////////////////////////////////////////////////////////
//
// Wavefront MTL
//
///////////////////////////////////////////////////////////////////////////////
#ifndef EASYVFS_NO_MTL
typedef struct
{
    /// The byte offset within the archive 
    easyvfs_uint64 offset;

    /// The size of the file, in bytes.
    easyvfs_uint64 sizeInBytes;

    /// The name of the material. The specification says this can be any length, but we're going to clamp it to 255 + null terminator which should be fine.
    char name[256];

}easyvfs_file_mtl;

typedef struct
{
    /// The access mode.
    easyvfs_access_mode accessMode;

    /// The buffer containing the list of files.
    easyvfs_file_mtl* pFiles;

    /// The number of files in the archive.
    unsigned int fileCount;

}easyvfs_archive_mtl;

typedef struct
{
    /// The current index of the iterator. When this hits the file count, the iteration is finished.
    unsigned int index;

}easyvfs_iterator_mtl;

typedef struct
{
    /// The offset within the archive file the first byte of the file is located.
    easyvfs_uint64 offsetInArchive;

    /// The size of the file in bytes so we can guard against overflowing reads.
    easyvfs_uint64 sizeInBytes;

    /// The current position of the file's read pointer.
    easyvfs_uint64 readPointer;

}easyvfs_openedfile_mtl;


easyvfs_archive_mtl* easyvfs_mtl_create(easyvfs_access_mode accessMode)
{
    easyvfs_archive_mtl* mtl = easyvfs_malloc(sizeof(easyvfs_archive_mtl));
    if (mtl != NULL)
    {
        mtl->accessMode = accessMode;
        mtl->pFiles     = NULL;
        mtl->fileCount  = 0;
    }

    return mtl;
}

void easyvfs_mtl_delete(easyvfs_archive_mtl* pArchive)
{
    easyvfs_free(pArchive->pFiles);
    easyvfs_free(pArchive);
}

void easyvfs_mtl_addfile(easyvfs_archive_mtl* pArchive, easyvfs_file_mtl* pFile)
{
    if (pArchive != NULL && pFile != NULL)
    {
        easyvfs_file_mtl* pOldBuffer = pArchive->pFiles;
        easyvfs_file_mtl* pNewBuffer = easyvfs_malloc(sizeof(easyvfs_file_mtl) * (pArchive->fileCount + 1));

        if (pNewBuffer != 0)
        {
            for (unsigned int iDst = 0; iDst < pArchive->fileCount; ++iDst)
            {
                pNewBuffer[iDst] = pOldBuffer[iDst];
            }

            pNewBuffer[pArchive->fileCount] = *pFile;


            pArchive->pFiles     = pNewBuffer;
            pArchive->fileCount += 1;

            easyvfs_free(pOldBuffer);
        }
    }
}


bool           easyvfs_isvalidarchive_mtl(easyvfs_context* pContext, const char* path);
void*          easyvfs_openarchive_mtl   (easyvfs_file* pFile, easyvfs_access_mode accessMode);
void           easyvfs_closearchive_mtl  (easyvfs_archive* pArchive);
bool           easyvfs_getfileinfo_mtl   (easyvfs_archive* pArchive, const char* path, easyvfs_file_info *fi);
void*          easyvfs_beginiteration_mtl(easyvfs_archive* pArchive, const char* path);
void           easyvfs_enditeration_mtl  (easyvfs_archive* pArchive, easyvfs_iterator* i);
bool           easyvfs_nextiteration_mtl (easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_file_info* fi);
void*          easyvfs_openfile_mtl      (easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode);
void           easyvfs_closefile_mtl     (easyvfs_file* pFile);
bool           easyvfs_readfile_mtl      (easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut);
bool           easyvfs_writefile_mtl     (easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut);
bool           easyvfs_seekfile_mtl      (easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin);
easyvfs_uint64 easyvfs_tellfile_mtl      (easyvfs_file* pFile);
easyvfs_uint64 easyvfs_filesize_mtl      (easyvfs_file* pFile);
void           easyvfs_flushfile_mtl     (easyvfs_file* pFile);
bool           easyvfs_deletefile_mtl    (easyvfs_archive* pArchive, const char* path);
bool           easyvfs_renamefile_mtl    (easyvfs_archive* pArchive, const char* pathOld, const char* pathNew);
bool           easyvfs_mkdir_mtl         (easyvfs_archive* pArchive, const char* path);
bool           easyvfs_copy_file_mtl     (easyvfs_archive* pArchive, const char* srcPath, const char* dstPath, bool failIfExists);

void easyvfs_registerarchivecallbacks_mtl(easyvfs_context* pContext)
{
    easyvfs_archive_callbacks callbacks;
    callbacks.isvalidarchive = easyvfs_isvalidarchive_mtl;
    callbacks.openarchive    = easyvfs_openarchive_mtl;
    callbacks.closearchive   = easyvfs_closearchive_mtl;
    callbacks.getfileinfo    = easyvfs_getfileinfo_mtl;
    callbacks.beginiteration = easyvfs_beginiteration_mtl;
    callbacks.enditeration   = easyvfs_enditeration_mtl;
    callbacks.nextiteration  = easyvfs_nextiteration_mtl;
    callbacks.openfile       = easyvfs_openfile_mtl;
    callbacks.closefile      = easyvfs_closefile_mtl;
    callbacks.readfile       = easyvfs_readfile_mtl;
    callbacks.writefile      = easyvfs_writefile_mtl;
    callbacks.seekfile       = easyvfs_seekfile_mtl;
    callbacks.tellfile       = easyvfs_tellfile_mtl;
    callbacks.filesize       = easyvfs_filesize_mtl;
    callbacks.flushfile      = easyvfs_flushfile_mtl;
    callbacks.deletefile     = easyvfs_deletefile_mtl;
    callbacks.renamefile     = easyvfs_renamefile_mtl;
    callbacks.mkdir          = easyvfs_mkdir_mtl;
    callbacks.copyfile       = easyvfs_copy_file_mtl;
    easyvfs_register_archive_callbacks(pContext, callbacks);
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

bool easyvfs_mtl_loadnextchunk(easyvfs_openarchive_mtl_state* pState)
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

bool easyvfs_mtl_loadnewmtl(easyvfs_openarchive_mtl_state* pState)
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

bool easyvfs_mtl_skipline(easyvfs_openarchive_mtl_state* pState)
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

bool easyvfs_mtl_skipwhitespace(easyvfs_openarchive_mtl_state* pState)
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

bool easyvfs_mtl_loadmtlname(easyvfs_openarchive_mtl_state* pState, void* dst, unsigned int dstSizeInBytes)
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


bool easyvfs_isvalidarchive_mtl(easyvfs_context* pContext, const char* path)
{
    (void)pContext;

    if (easyvfs_extension_equal(path, "mtl"))
    {
        return 1;
    }

    return 0;
}

void* easyvfs_openarchive_mtl(easyvfs_file* pFile, easyvfs_access_mode accessMode)
{
    assert(pFile != NULL);
    assert(easyvfs_tell(pFile) == 0);

    easyvfs_archive_mtl* mtl = easyvfs_mtl_create(accessMode);
    if (mtl != NULL)
    {
        mtl->accessMode = accessMode;


        // We create a state object that is used to help us with chunk management.
        easyvfs_openarchive_mtl_state state;
        state.pFile              = pFile;
        state.archiveSizeInBytes = easyvfs_file_size(pFile);
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
    }

    return mtl;
}

void easyvfs_closearchive_mtl(easyvfs_archive* pArchive)
{
    assert(pArchive != 0);

    easyvfs_archive_mtl* pUserData = pArchive->pUserData;
    if (pUserData != NULL)
    {
        easyvfs_mtl_delete(pUserData);
    }
}

bool easyvfs_getfileinfo_mtl(easyvfs_archive* pArchive, const char* path, easyvfs_file_info *fi)
{
    assert(pArchive != 0);

    easyvfs_archive_mtl* mtl = pArchive->pUserData;
    if (mtl != NULL)
    {
        for (unsigned int iFile = 0; iFile < mtl->fileCount; ++iFile)
        {
            if (strcmp(path, mtl->pFiles[iFile].name) == 0)
            {
                // We found the file.
                if (fi != NULL)
                {
                    easyvfs_copy_and_append_path(fi->absolutePath, EASYVFS_MAX_PATH, pArchive->absolutePath, path);
                    fi->sizeInBytes      = mtl->pFiles[iFile].sizeInBytes;
                    fi->lastModifiedTime = 0;
                    fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;
                }

                return 1;
            }
        }
    }

    return 0;
}

void* easyvfs_beginiteration_mtl(easyvfs_archive* pArchive, const char* path)
{
    assert(pArchive != 0);
    assert(path != NULL);

    easyvfs_archive_mtl* mtl = pArchive->pUserData;
    if (mtl != NULL)
    {
        if (mtl->fileCount > 0)
        {
            if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0'))     // This is a flat archive, so no sub-folders.
            {
                easyvfs_iterator_mtl* i = easyvfs_malloc(sizeof(easyvfs_iterator_mtl));
                if (i != NULL)
                {
                    i->index = 0;
                    return i;
                }
            }
        }
    }

    return NULL;
}

void easyvfs_enditeration_mtl(easyvfs_archive* pArchive, easyvfs_iterator* i)
{
    (void)pArchive;

    assert(pArchive != 0);
    assert(i != NULL);

    easyvfs_free(i->pUserData);
    i->pUserData = NULL;
}

bool easyvfs_nextiteration_mtl(easyvfs_archive* pArchive, easyvfs_iterator* i, easyvfs_file_info* fi)
{
    assert(pArchive != 0);
    assert(i != NULL);

    easyvfs_archive_mtl* mtl = pArchive->pUserData;
    if (mtl != NULL)
    {
        easyvfs_iterator_mtl* imtl = i->pUserData;
        if (imtl != NULL)
        {
            if (imtl->index < mtl->fileCount)
            {
                if (fi != NULL)
                {
                    easyvfs_strcpy(fi->absolutePath, EASYVFS_MAX_PATH, mtl->pFiles[imtl->index].name);
                    fi->sizeInBytes      = mtl->pFiles[imtl->index].sizeInBytes;
                    fi->lastModifiedTime = 0;
                    fi->attributes       = EASYVFS_FILE_ATTRIBUTE_READONLY;
                }

                imtl->index += 1;
                return 1;
            }
        }
    }
    

    return 0;
}

void* easyvfs_openfile_mtl(easyvfs_archive* pArchive, const char* path, easyvfs_access_mode accessMode)
{
    assert(pArchive != 0);
    assert(path != NULL);

    // Only supporting read-only for now.
    if ((accessMode & EASYVFS_WRITE) != 0)
    {
        return NULL;
    }

    easyvfs_archive_mtl* mtl = pArchive->pUserData;
    if (mtl != NULL)
    {
        for (unsigned int iFile = 0; iFile < mtl->fileCount; ++iFile)
        {
            if (strcmp(path, mtl->pFiles[iFile].name) == 0)
            {
                // We found the file.
                easyvfs_openedfile_mtl* pOpenedFile = easyvfs_malloc(sizeof(easyvfs_openedfile_mtl));
                if (pOpenedFile != NULL)
                {
                    pOpenedFile->offsetInArchive = mtl->pFiles[iFile].offset;
                    pOpenedFile->sizeInBytes     = mtl->pFiles[iFile].sizeInBytes;
                    pOpenedFile->readPointer     = 0;

                    return pOpenedFile;
                }
            }
        }
    }

    return NULL;
}

void easyvfs_closefile_mtl(easyvfs_file* pFile)
{
    assert(pFile != 0);

    easyvfs_openedfile_mtl* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        easyvfs_free(pOpenedFile);
    }
}

bool easyvfs_readfile_mtl(easyvfs_file* pFile, void* dst, unsigned int bytesToRead, unsigned int* bytesReadOut)
{
    assert(pFile != 0);
    assert(dst != NULL);
    assert(bytesToRead > 0);

    easyvfs_openedfile_mtl* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        easyvfs_file* pArchiveFile = pFile->pArchive->pFile;
        assert(pArchiveFile != NULL);

        easyvfs_int64 bytesAvailable = pOpenedFile->sizeInBytes - pOpenedFile->readPointer;
        if (bytesAvailable < bytesToRead) {
            bytesToRead = (unsigned int)bytesAvailable;     // Safe cast, as per the check above.
        }

        easyvfs_seek(pArchiveFile, (easyvfs_int64)(pOpenedFile->offsetInArchive + pOpenedFile->readPointer), easyvfs_start);
        int result = easyvfs_read(pArchiveFile, dst, bytesToRead, bytesReadOut);
        if (result != 0)
        {
            pOpenedFile->readPointer += bytesToRead;
        }

        return result;
    }

    return 0;
}

bool easyvfs_writefile_mtl(easyvfs_file* pFile, const void* src, unsigned int bytesToWrite, unsigned int* bytesWrittenOut)
{
    (void)pFile;
    (void)src;
    (void)bytesToWrite;

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

bool easyvfs_seekfile_mtl(easyvfs_file* pFile, easyvfs_int64 bytesToSeek, easyvfs_seek_origin origin)
{
    assert(pFile != 0);

    easyvfs_openedfile_mtl* pOpenedFile = pFile->pUserData;
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

        pOpenedFile->readPointer = newPos;
        return 1;
    }

    return 0;
}

easyvfs_uint64 easyvfs_tellfile_mtl(easyvfs_file* pFile)
{
    assert(pFile != 0);

    easyvfs_openedfile_mtl* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        return pOpenedFile->readPointer;
    }

    return 0;
}

easyvfs_uint64 easyvfs_filesize_mtl(easyvfs_file* pFile)
{
    assert(pFile != 0);

    easyvfs_openedfile_mtl* pOpenedFile = pFile->pUserData;
    if (pOpenedFile != NULL)
    {
        return pOpenedFile->sizeInBytes;
    }

    return 0;
}

void easyvfs_flushfile_mtl(easyvfs_file* pFile)
{
    (void)pFile;

    assert(pFile != 0);

    // No support for this at the moment because it's read-only for now.
}

bool easyvfs_deletefile_mtl(easyvfs_archive* pArchive, const char* path)
{
    (void)pArchive;
    (void)path;

    assert(pArchive != 0);
    assert(path     != 0);

    // No support for this at the moment because it's read-only for now.
    return 0;
}

bool easyvfs_renamefile_mtl(easyvfs_archive* pArchive, const char* pathOld, const char* pathNew)
{
    (void)pArchive;
    (void)pathOld;
    (void)pathNew;

    assert(pArchive != 0);
    assert(pathOld  != 0);
    assert(pathNew  != 0);

    // No support for this at the moment because it's read-only for now.
    return 0;
}

bool easyvfs_mkdir_mtl(easyvfs_archive* pArchive, const char* path)
{
    (void)pArchive;
    (void)path;

    assert(pArchive != 0);
    assert(path     != 0);

    // MTL archives do not have the notion of folders.
    return 0;
}

bool easyvfs_copy_file_mtl(easyvfs_archive* pArchive, const char* srcPath, const char* dstPath, bool failIfExists)
{
    (void)pArchive;
    (void)srcPath;
    (void)dstPath;
    (void)failIfExists;

    assert(pArchive != 0);
    assert(srcPath  != 0);
    assert(dstPath  != 0);

    // No support for this at the moment because it's read-only for now.
    return 0;
}
#endif // !EASYVFS_NO_MTL



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
