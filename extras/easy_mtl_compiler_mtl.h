// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#ifndef __easy_mtl_compiler_mtl_h_
#define __easy_mtl_compiler_mtl_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef int easymtl_bool;
typedef struct easymtl_material easymtl_material;

/// Compiles a Wavefront MTL file.
///
/// @param pMaterial [in] A pointer to the destination material.
///
/// @remarks
///     This will compile the material at the first occurance of the "newmtl" statement, and will end at either the next
///     occurance of "newmtl" of when the input buffer has been exhausted.
///     @par
///     This will initialize the material, so ensure that you have not already initialized it before calling this. If this
///     returns successfully, call easymtl_uninit() to uninitialize the material.
easymtl_bool easymtl_compile_wavefront_mtl(easymtl_material* pMaterial, const char* mtlData, unsigned int mtlDataSizeInBytes);


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