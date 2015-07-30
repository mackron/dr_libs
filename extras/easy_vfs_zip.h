// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#ifndef __easy_vfs_zip_h_
#define __easy_vfs_zip_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct easyvfs_context easyvfs_context;

/// Registers the archive callbacks which enables support for Wavefront MTL material files. The .mtl file
/// is treated as a flat archive containing a "file" for each material defined inside the .mtl file. The
/// first byte in each "file" is the very beginning of the "newmtl" statement, with the last byte being the
/// byte just before the next "newmtl" statement, or the end of the file. The name of each file is the word
/// coming after the "newmtl" token.
void easyvfs_registerarchivecallbacks_zip(easyvfs_context* pContext);


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