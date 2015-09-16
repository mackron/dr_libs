// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#ifndef easy_util
#define easy_util


/////////////////////////////////////////////////////////
// Options

#ifdef EASYUTIL_ONLY_ANNOTATIONS
    #define EASYUTIL_NO_MSVC_COMPAT
    #define EASYUTIL_NO_ALIGNED_MALLOC
#endif

#ifdef EASYUTIL_ONLY_MSVC_COMPAT
    #define EASYUTIL_NO_ANNOTATIONS
    #define EASYUTIL_NO_ALIGNED_MALLOC
#endif

#ifdef EASYUTIL_ONLY_ALIGNED_MALLOC
    #define EASYUTIL_NO_ANNOTATIONS
    #define EASYUTIL_NO_MSVC_COMPAT
#endif


// Disable MSVC compatibility if we're compiling with it.
#if !defined(EASYUTIL_NO_MSVC_COMPAT) && !defined(_MSC_VER)
    #define EASYUTIL_NO_MSVC_COMPAT
#endif



#include <stdlib.h>

#if !defined(_MSC_VER)
#include <errno.h>
#endif


/////////////////////////////////////////////////////////
// Annotations

#ifndef EASYUTIL_NO_ANNOTATIONS
    #ifndef IN
    #define IN
    #endif

    #ifndef OUT
    #define OUT
    #endif

    #ifndef UNUSED
    #define UNUSED(x) ((void)x)
    #endif
#endif


/////////////////////////////////////////////////////////
// MSVC Compatibility

#ifndef EASYUTIL_NO_MSVC_COMPAT
/// A basic implementation of MSVC's strcpy_s().
inline int strcpy_s(char* dst, size_t dstSizeInBytes, const char* src)
{
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
}
#endif


/////////////////////////////////////////////////////////
// Aligned Allocations

#ifndef EASYUTIL_NO_ALIGNED_MALLOC
inline void* aligned_malloc(size_t alignment, size_t size)
{
#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)
    return _aligned_malloc(size, alignment);
#else
    void* pResult;
    if (posix_memalign(&pResult, alignment, size) == 0) {
        return pResult;
    }
    
    return 0;
#endif
}

inline void aligned_free(void* ptr)
{
#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}
#endif  // !EASYUTIL_NO_ALIGNED_MALLOC




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
