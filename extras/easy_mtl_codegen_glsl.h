// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#ifndef easy_mtl_codegen_glsl
#define easy_mtl_codegen_glsl

#ifdef __cplusplus
extern "C" {
#endif

typedef int easymtl_bool;
typedef struct easymtl_material easymtl_material;


/// Generates GLSL code for the channel with the given name.
easymtl_bool easymtl_codegen_glsl_channel(easymtl_material* pMaterial, const char* channelName, char* codeOut, unsigned int codeOutSizeInBytes, unsigned int* pBytesWrittenOut);

/// Generates GLSL code for the uniform variables as defined by the material's public input variables.
easymtl_bool easymtl_codegen_glsl_uniforms(easymtl_material* pMaterial, char* codeOut, unsigned int codeOutSizeInBytes, unsigned int* pBytesWritteOut);


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
