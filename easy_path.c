// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#include "easy_path.h"

easypath_iterator easypath_begin(const char* path)
{
    easypath_iterator i;
    i.path = path;
    i.segment.offset = 0;
    i.segment.length = 0;

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
    easypath_iterator iParent = easypath_begin(parentAbsolutePath);
    easypath_iterator iChild  = easypath_begin(descendantAbsolutePath);

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
        const char* extension = easypath_filename(path);

        // Just find the first '.' and return.
        while (extension[0] != '\0')
        {
            extension += 1;

            if (extension[0] == '.')
            {
                extension += 1;
                break;
            }
        }


        return extension;
    }

    return 0;
}


int easypath_equal(const char* path1, const char* path2)
{
    if (path1 != 0 && path2 != 0)
    {
        easypath_iterator iPath1 = easypath_begin(path1);
        easypath_iterator iPath2 = easypath_begin(path2);

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