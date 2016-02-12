// Public domain. See "unlicense" statement at the end of this file.

// ABOUT
//
// dr_codegen is a simple library for doing basic code generation. This is only updated as I need it.
//
//
//
// USAGE
//
// This is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_CODEGEN_IMPLEMENTATION
//   #include "dr_codegen.h"
//
// You can then #include this file in other parts of the program as you would with any other header file.

#ifndef dr_codegen_h
#define dr_codegen_h

#ifdef __cplusplus
extern "C" {
#endif

// Converts the given buffer to a C-style static const unsigned char array.
//
// Free the returned pointer with free().
char* dr_codegen_buffer_to_c_array(const unsigned char* buffer, unsigned int size, const char* variableName);

// Frees a buffer that was created by dr_codegen.
void dr_codegen_free(void* buffer);

// memcpy()
void dr_codegen_memcpy(void* dst, const void* src, unsigned int size);

// strlen() implementation for dr_codegen().
unsigned int dr_codegen_strlen(const char* str);


#ifdef __cplusplus
}
#endif

#endif  //dr_codegen_h


///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DR_CODEGEN_IMPLEMENTATION
#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <string.h>
#endif

char* dr_codegen_buffer_to_c_array(const unsigned char* buffer, unsigned int size, const char* variableName)
{
    const unsigned int bytesPerLine = 16;
    const char* header = "static const unsigned char ";
    const char* declarationTail = "[] = {\n";

    size_t headerLen          = dr_codegen_strlen(header);
    size_t variableNameLen    = dr_codegen_strlen(variableName);
    size_t declarationTailLen = dr_codegen_strlen(declarationTail);

    size_t totalLen = headerLen + variableNameLen + declarationTailLen;
    totalLen += size * 6;                                                // x6 because we store 6 character's per byte.
    totalLen += (size / bytesPerLine + 1) * 4;                           // Indentation.
    totalLen += 2;                                                       // +2 for the "};" at the end.

#ifdef _WIN32
    char* output = HeapAlloc(GetProcessHeap(), 0, totalLen);
#else
    char* output = malloc(totalLen);                                     // No need for +1 for the null terminator because the last byte will not have a trailing "," which leaves room.
#endif

    char* runningOutput = output;
    dr_codegen_memcpy(runningOutput, header, headerLen);
    runningOutput += headerLen;

    dr_codegen_memcpy(runningOutput, variableName, variableNameLen);
    runningOutput += variableNameLen;

    dr_codegen_memcpy(runningOutput, declarationTail, declarationTailLen);
    runningOutput += declarationTailLen;

    for (unsigned int i = 0; i < size; ++i)
    {
        const unsigned char byte = buffer[i];

        if ((i % bytesPerLine) == 0) {
            runningOutput[0] = ' ';
            runningOutput[1] = ' ';
            runningOutput[2] = ' ';
            runningOutput[3] = ' ';
            runningOutput += 4;
        }

        runningOutput[0] = '0';
        runningOutput[1] = 'x';
        runningOutput[2] = ((byte >>  4) + '0'); if (runningOutput[2] > '9') runningOutput[2] += 7;
        runningOutput[3] = ((byte & 0xF) + '0'); if (runningOutput[3] > '9') runningOutput[3] += 7;
        runningOutput += 4;

        if (i + 1 < size) {
            *runningOutput++ = ',';
        }
        
        if ((i % bytesPerLine)+1 == bytesPerLine || i + 1 == size) {
            *runningOutput++ = '\n';
        } else {
            *runningOutput++ = ' ';
        }
    }

    runningOutput[0] = '}';
    runningOutput[1] = ';';
    runningOutput[2] = '\0';
    return output;
}

void dr_codegen_free(void* buffer)
{
#ifdef _WIN32
    HeapFree(GetProcessHeap(), 0, buffer);
#else
    free(buffer);
#endif
}

void dr_codegen_memcpy(void* dst, const void* src, unsigned int sizeInBytes)
{
    unsigned char* dst8 = dst;
    const unsigned char* src8 = src;

    for (unsigned int i = 0; i < sizeInBytes; ++i) {
        *dst8++ = *src8++;
    }
}

unsigned int dr_codegen_strlen(const char* str)
{
    unsigned int i = 0;
    while (*str++ != '\0') {
        i += 1;
    }

    return i;
}
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
