// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#include "easy_path.h"

#if EASYPATH_USE_STDLIB
#include <string.h>
#include <ctype.h>
#endif

unsigned int easypath_strlen(const char* str)
{
#if EASYPATH_USE_STDLIB
    return (unsigned int)strlen(str);
#else
    const char* pathEnd = path;
    while (pathEnd[0] != '\0')
    {
        pathEnd += 1;
    }

    return pathEnd - path;
#endif
}

void easypath_strcpy(char* dst, unsigned int dstSizeInBytes, const char* src)
{
#if EASYPATH_USE_STDLIB && (defined(_MSC_VER))
    strcpy_s(dst, dstSizeInBytes, src);
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

void easypath_strncpy(char* dst, unsigned int dstSizeInBytes, const char* src, unsigned int srcSizeInBytes)
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


easypath_iterator easypath_first(const char* path)
{
    easypath_iterator i;
    i.path = path;
    i.segment.offset = 0;
    i.segment.length = 0;

    return i;
}

easypath_iterator easypath_last(const char* path)
{
    easypath_iterator i;
    i.path = path;
    i.segment.offset = easypath_strlen(path);
    i.segment.length = 0;

    easypath_prev(&i);
    return i;
}

int easypath_next(easypath_iterator* i)
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

int easypath_prev(easypath_iterator* i)
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

        return 1;
    }

    return 0;
}

int easypath_atend(easypath_iterator i)
{
    // Note that the input argument is a copy of the iterator. Thus, we can just call easypath_next() to determine whether or not it's at the end at
    // it won't affect the caller in any way.
    return !easypath_next(&i);
}

int easypath_atstart(easypath_iterator i)
{
    return !easypath_prev(&i);
}

int easypath_iterators_equal(const easypath_iterator i0, const easypath_iterator i1)
{
    return easypath_segments_equal(i0.path, i0.segment, i1.path, i1.segment);
}

int easypath_segments_equal(const char* s0Path, const easypath_segment s0, const char* s1Path, const easypath_segment s1)
{
    if (s0Path != 0 && s1Path != 0)
    {
        if (s0.length == s1.length)
        {
            for (unsigned int i = 0; i < s0.length; ++i)
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


void easypath_toforwardslashes(char* path)
{
    if (path != 0)
    {
        while (path[0] != '\0')
        {
            if (path[0] == '\\')
            {
                path[0] = '/';
            }

            path += 1;
        }
    }
}

void easypath_tobackslashes(char* path)
{
    if (path != 0)
    {
        while (path[0] != '\0')
        {
            if (path[0] == '/')
            {
                path[0] = '\\';
            }

            path += 1;
        }
    }
}


int easypath_isdescendant(const char* descendantAbsolutePath, const char* parentAbsolutePath)
{
    easypath_iterator iParent = easypath_first(parentAbsolutePath);
    easypath_iterator iChild  = easypath_first(descendantAbsolutePath);

    while (easypath_next(&iParent))
    {
        if (easypath_next(&iChild))
        {
            // If the segment is different, the paths are different and thus it is not a descendant.
            if (!easypath_iterators_equal(iParent, iChild))
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
    return easypath_next(&iChild);
}

int easypath_ischild(const char* childAbsolutePath, const char* parentAbsolutePath)
{
    easypath_iterator iParent = easypath_first(parentAbsolutePath);
    easypath_iterator iChild  = easypath_first(childAbsolutePath);

    while (easypath_next(&iParent))
    {
        if (easypath_next(&iChild))
        {
            // If the segment is different, the paths are different and thus it is not a descendant.
            if (!easypath_iterators_equal(iParent, iChild))
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
    if (easypath_next(&iChild))
    {
        // It could be a child. If the next iteration fails, it's a direct child and we want to return true.
        if (!easypath_next(&iChild))
        {
            return 1;
        }
    }

    return 0;
}

void easypath_basepath(char* path)
{
    if (path != 0)
    {
        char* baseend = path;

        // We just loop through the path until we find the last slash.
        while (path[0] != '\0')
        {
            if (path[0] == '/' || path[0] == '\\')
            {
                baseend = path;
            }

            path += 1;
        }


        // Now we just loop backwards until we hit the first non-slash.
        while (baseend > path)
        {
            if (baseend[0] != '/' && baseend[0] != '\\')
            {
                break;
            }

            baseend -= 1;
        }


        // We just put a null terminator on the end.
        baseend[0] = '\0';
    }
}

void easypath_copybasepath(const char* path, char* baseOut, unsigned int baseSizeInBytes)
{
    if (path != 0 && baseOut != 0 && baseSizeInBytes > 0)
    {
        easypath_strcpy(baseOut, baseSizeInBytes, path);
        easypath_basepath(baseOut);
    }
}

const char* easypath_filename(const char* path)
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

const char* easypath_extension(const char* path)
{
    if (path != 0)
    {
        const char* extension     = easypath_filename(path);
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
}


int easypath_equal(const char* path1, const char* path2)
{
    if (path1 != 0 && path2 != 0)
    {
        easypath_iterator iPath1 = easypath_first(path1);
        easypath_iterator iPath2 = easypath_first(path2);

        while (easypath_next(&iPath1) && easypath_next(&iPath2))
        {
            if (!easypath_iterators_equal(iPath1, iPath2))
            {
                return 0;
            }
        }


        // At this point either iPath1 and/or iPath2 have finished iterating. If both of them are at the end, the two paths are equal.
        return iPath1.path[iPath1.segment.offset] == '\0' && iPath2.path[iPath2.segment.offset] == '\0';
    }

    return 0;
}

int easypath_extensionequal(const char* path, const char* extension)
{
    if (path != 0 && extension != 0)
    {
        const char* ext1 = extension;
        const char* ext2 = easypath_extension(path);

        while (ext1[0] != '\0' && ext2[0] != '\0')
        {
#if EASYPATH_USE_STDLIB
            if (tolower(ext1[0]) != tolower(ext2[0]))
#else
            if (ext1[0] != ext2[0])
#endif
            {
                return 0;
            }

            ext1 += 1;
            ext2 += 1;
        }


        return ext1[0] == '\0' && ext2[0] == '\0';
    }

    return 1;
}



int easypath_isrelative(const char* path)
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

int easypath_isabsolute(const char* path)
{
    return !easypath_isrelative(path);
}


int easypath_append(char* base, unsigned int baseBufferSizeInBytes, const char* other)
{
    if (base != 0 && other != 0)
    {
        unsigned int path1Length = easypath_strlen(base);
        unsigned int path2Length = easypath_strlen(other);

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

            easypath_strncpy(base + path1Length, baseBufferSizeInBytes - path1Length, other, path2Length);


            return 1;
        }
    }

    return 0;
}

int easypath_appenditerator(char* base, unsigned int baseBufferSizeInBytes, easypath_iterator i)
{
    if (base != 0)
    {
        unsigned int path1Length = easypath_strlen(base);
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

            easypath_strncpy(base + path1Length, baseBufferSizeInBytes - path1Length, i.path + i.segment.offset, path2Length);


            return 1;
        }
    }

    return 0;
}

int easypath_appendextension(char* base, unsigned int baseBufferSizeInBytes, const char* extension)
{
    if (base != 0 && extension != 0)
    {
        unsigned int baseLength = easypath_strlen(base);
        unsigned int extLength  = easypath_strlen(extension);

        if (baseLength < baseBufferSizeInBytes)
        {
            base[baseLength] = '.';
            baseLength += 1;

            if (baseLength + extLength >= baseBufferSizeInBytes)
            {
                extLength = baseBufferSizeInBytes - baseLength - 1;      // -1 for the null terminator.
            }

            easypath_strncpy(base + baseLength, baseBufferSizeInBytes - baseLength, extension, extLength);


            return 1;
        }
    }

    return 0;
}

int easypath_copyandappend(char* dst, unsigned int dstSizeInBytes, const char* base, const char* other)
{
    if (dst != 0 && dstSizeInBytes > 0)
    {
        easypath_strcpy(dst, dstSizeInBytes, base);
        return easypath_append(dst, dstSizeInBytes, other);
    }

    return 0;
}

int easypath_copyandappenditerator(char* dst, unsigned int dstSizeInBytes, const char* base, easypath_iterator i)
{
    if (dst != 0 && dstSizeInBytes > 0)
    {
        easypath_strcpy(dst, dstSizeInBytes, base);
        return easypath_appenditerator(dst, dstSizeInBytes, i);
    }

    return 0;
}

int easypath_copyandappendextension(char* dst, unsigned int dstSizeInBytes, const char* base, const char* extension)
{
    if (dst != 0 && dstSizeInBytes > 0)
    {
        easypath_strcpy(dst, dstSizeInBytes, base);
        return easypath_appendextension(dst, dstSizeInBytes, extension);
    }

    return 0;
}



// This does not write the null terminator.
unsigned int _easypath_clean_trywrite(easypath_iterator isegment, char* pathOut, unsigned int pathOutSizeInBytes, unsigned int ignoreCounter);
unsigned int _easypath_clean_trywrite(easypath_iterator isegment, char* pathOut, unsigned int pathOutSizeInBytes, unsigned int ignoreCounter)
{
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

    easypath_iterator prev = isegment;
    if (easypath_prev(&prev))
    {
        bytesWritten = _easypath_clean_trywrite(prev, pathOut, pathOutSizeInBytes, ignoreCounter);
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

            if (pathOutSizeInBytes >= isegment.segment.length)
            {
                easypath_strncpy(pathOut, pathOutSizeInBytes, isegment.path + isegment.segment.offset, isegment.segment.length);
                bytesWritten += isegment.segment.length;
            }
            else
            {
                easypath_strncpy(pathOut, pathOutSizeInBytes, isegment.path + isegment.segment.offset, pathOutSizeInBytes);
                bytesWritten += pathOutSizeInBytes;
            }
        }
    }

    return bytesWritten;
}


unsigned int easypath_clean(const char* path, char* pathOut, unsigned int pathOutSizeInBytes)
{
    if (path != 0)
    {
        easypath_iterator last = easypath_last(path);
        if (last.segment.length > 0)
        {
            unsigned int bytesWritten = _easypath_clean_trywrite(last, pathOut, pathOutSizeInBytes - 1, 0);  // -1 to ensure there is enough room for a null terminator later on.
            if (pathOutSizeInBytes > bytesWritten)
            {
                pathOut[bytesWritten] = '\0';
            }

            return bytesWritten + 1;
        }
    }

    return 0;
}


int easypath_removeextension(char* path)
{
    if (path != 0)
    {
        const char* extension = easypath_extension(path);
        if (extension != 0)
        {
            extension -= 1;
            path[(extension - path)] = '\0';
        }

        return 1;
    }

    return 0;
}

int easypath_copyandremoveextension(char* dst, unsigned int dstSizeInBytes, const char* path)
{
    if (dst != 0 && dstSizeInBytes > 0 && path != 0)
    {
        const char* extension = easypath_extension(path);
        if (extension != 0)
        {
            extension -= 1;
        }

        easypath_strncpy(dst, dstSizeInBytes, path, (unsigned int)(extension - path));
        return 1;
    }

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
