/*
Opus audio decoder. Choice of public domain or MIT. See license statements at the end of this file.
dr_opus - v0.0.0 (Unreleased) - xxxx-xx-xx

David Reid - mackron@gmail.com
*/

/* ====== WORK-IN-PROGRESSS ====== */

#ifndef dr_opus_h
#define dr_opus_h

#include <stddef.h> /* For size_t. */

#ifndef DROPUS_HAS_STDINT
    #if defined(_MSC_VER)
        #if _MSC_VER >= 1600
            #define DROPUS_HAS_STDINT
        #endif
    #else
        #if defined(__has_include)
            #if __has_include(<stdint.h>)
                #define DROPUS_HAS_STDINT
            #endif
        #endif
    #endif
#endif

#if !defined(DROPUS_HAS_STDINT) && (defined(__GNUC__) || defined(__clang__))   /* Assume support for stdint.h on GCC and Clang. */
    #define DROPUS_HAS_STDINT
#endif

#ifndef DROPUS_HAS_STDINT
typedef   signed char               dropus_int8;
typedef unsigned char               dropus_uint8;
typedef   signed short              dropus_int16;
typedef unsigned short              dropus_uint16;
typedef   signed int                dropus_int32;
typedef unsigned int                dropus_uint32;
    #if defined(_MSC_VER)
    typedef   signed __int64        dropus_int64;
    typedef unsigned __int64        dropus_uint64;
    #else
    typedef   signed long long int  dropus_int64;
    typedef unsigned long long int  dropus_uint64;
    #endif
    #if defined(_WIN32)
        #if defined(_WIN64)
        typedef dropus_uint64       dropus_uintptr;
        #else
        typedef dropus_uint32       dropus_uintptr;
        #endif
    #elif defined(__GNUC__)
        #if defined(__LP64__)
        typedef dropus_uint64       dropus_uintptr;
        #else
        typedef dropus_uint32       dropus_uintptr;
        #endif
    #else
        typedef dropus_uint64       dropus_uintptr;    /* Fallback. */
    #endif
#else
#include <stdint.h>
typedef int8_t                      dropus_int8;
typedef uint8_t                     dropus_uint8;
typedef int16_t                     dropus_int16;
typedef uint16_t                    dropus_uint16;
typedef int32_t                     dropus_int32;
typedef uint32_t                    dropus_uint32;
typedef int64_t                     dropus_int64;
typedef uint64_t                    dropus_uint64;
typedef uintptr_t                   dropus_uintptr;
#endif
typedef dropus_uint8                dropus_bool8;
typedef dropus_uint32               dropus_bool32;
#define DROPUS_TRUE                 1
#define DROPUS_FALSE                0

typedef void* dropus_handle;
typedef void* dropus_ptr;
typedef void (* dropus_proc)(void);

#if defined(_MSC_VER) && !defined(_WCHAR_T_DEFINED)
typedef dropus_uint16 wchar_t;
#endif

#ifndef NULL
#define NULL 0
#endif

#if defined(SIZE_MAX)
    #define DROPUS_SIZE_MAX SIZE_MAX
#else
    #define DROPUS_SIZE_MAX 0xFFFFFFFF  /* When SIZE_MAX is not defined by the standard library just default to the maximum 32-bit unsigned integer. */
#endif


#ifdef _MSC_VER
#define DROPUS_INLINE __forceinline
#else
#ifdef __GNUC__
#define DROPUS_INLINE inline __attribute__((always_inline))
#else
#define DROPUS_INLINE inline
#endif
#endif

typedef int dropus_result;
#define DROPUS_SUCCESS           0
#define DROPUS_ERROR            -1  /* Generic or unknown error. */
#define DROPUS_INVALID_ARGS     -2
#define DROPUS_BAD_DATA         -100

/***********************************************************************************************************************************************************

Low-Level Opus Stream API

This API is where the low-level decoding takes place. In order to use this, you must know the packet structure of the Opus stream. This is usually the job of
encapsulations such as Ogg and Matroska.

************************************************************************************************************************************************************/
#define DROPUS_MAX_OPUS_FRAMES_PER_PACKET   48      /* RFC 6716 - Section 3.2.5 */
#define DROPUS_MAX_PCM_FRAMES_PER_PACKET    6144    /* RFC 6716 - Section 3.2.5. Maximum of 120ms. Maximum rate is 48kHz. 6144 = 120*48. */

typedef enum
{
    dropus_mode_silk,
    dropus_mode_celt,
    dropus_mode_hybrid
} dropus_mode;

typedef struct
{
    dropus_uint16 sizeInBytes;
} dropus_stream_frame;

typedef struct
{
    dropus_uint8 toc;   /* TOC byte. RFC 6716 - Section 3.1 */
    dropus_stream_frame frames[DROPUS_MAX_OPUS_FRAMES_PER_PACKET];
} dropus_stream_packet;

typedef struct
{
    dropus_stream_packet packet;   /* The current packet. */
} dropus_stream;

/*
Initializes a new low-level Opus stream object.
*/
dropus_result dropus_stream_init(dropus_stream* pOpusStream);

/*
Decodes a packet from the given compressed data.
*/
dropus_result dropus_stream_decode_packet(dropus_stream* pOpusStream, const void* pData, size_t dataSize);



/***********************************************************************************************************************************************************

High-Level Opus Decoding API

************************************************************************************************************************************************************/

typedef enum
{
    dropus_seek_origin_start,
    dropus_seek_origin_current
} dropus_seek_origin;

typedef size_t (* dropus_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);
typedef dropus_bool32 (* dropus_seek_proc)(void* pUserData, int offset, dropus_seek_origin origin);

typedef struct
{
    dropus_read_proc onRead;
    dropus_seek_proc onSeek;
    void* pUserData;
    void* pFile;    /* Only used for decoders that were opened against a file. */
    struct
    {
        const dropus_uint8* pData;
        size_t dataSize;
        size_t currentReadPos;
    } memory;       /* Only used for decoders that were opened against a block of memory. */
} dropus;

/*
Initializes a pre-allocated decoder object from callbacks.
*/
dropus_bool32 dropus_init(dropus* pOpus, dropus_read_proc onRead, dropus_seek_proc onSeek, void* pUserData);

#ifndef DR_OPUS_NO_STDIO
/*
Initializes a pre-allocated decoder object from a file.

This keeps hold of the file handle throughout the lifetime of the decoder and closes it in dropus_uninit().
*/
dropus_bool32 dropus_init_file(dropus* pOpus, const char* pFilePath);
#endif

/*
Initializes a pre-allocated decoder object from a block of memory.

This does not make a copy of the memory.
*/
dropus_bool32 dropus_init_memory(dropus* pOpus, const void* pData, size_t dataSize);

/*
Uninitializes an Opus decoder.
*/
void dropus_uninit(dropus* pOpus);


#endif  /* dr_opus_h */

/************************************************************************************************************************************************************
 ************************************************************************************************************************************************************

 IMPLEMENTATION

 ************************************************************************************************************************************************************
 ************************************************************************************************************************************************************/
#ifdef DR_OPUS_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>
#ifndef DR_OPUS_NO_STDIO
#include <stdio.h>
#endif

#ifndef DROPUS_ASSERT
#include <assert.h>
#define DROPUS_ASSERT(expression)           assert(expression)
#endif
#ifndef DROPUS_COPY_MEMORY
#define DROPUS_COPY_MEMORY(dst, src, sz)    memcpy((dst), (src), (sz))
#endif
#ifndef DROPUS_ZERO_MEMORY
#define DROPUS_ZERO_MEMORY(p, sz)           memset((p), 0, (sz))
#endif
#ifndef DROPUS_ZERO_OBJECT
#define DROPUS_ZERO_OBJECT(p)               DROPUS_ZERO_MEMORY((p), sizeof(*(p)))
#endif



/***********************************************************************************************************************************************************

Low-Level Opus Stream API

************************************************************************************************************************************************************/

/*********************************** 
RFC 6716 - Section 3.1 The TOC Byte
************************************/
DROPUS_INLINE dropus_uint8 dropus_toc_config(dropus_uint8 toc)
{
    return (toc & 0xF8) >> 3;
}

DROPUS_INLINE dropus_uint8 dropus_toc_s(dropus_uint8 toc)
{
    return (toc & 0x04) >> 2;
}

DROPUS_INLINE dropus_uint8 dropus_toc_c(dropus_uint8 toc)
{
    return (toc & 0x03);
}

DROPUS_INLINE dropus_mode dropus_toc_config_mode(dropus_uint8 config)
{
    /* Table 2 in RFC 6716 */
    static dropus_mode modes[32] = {
        dropus_mode_silk,   dropus_mode_silk,   dropus_mode_silk, dropus_mode_silk, /*  0...3  */
        dropus_mode_silk,   dropus_mode_silk,   dropus_mode_silk, dropus_mode_silk, /*  4...7  */
        dropus_mode_silk,   dropus_mode_silk,   dropus_mode_silk, dropus_mode_silk, /*  8...11 */
        dropus_mode_hybrid, dropus_mode_hybrid,                                     /* 12...13 */
        dropus_mode_hybrid, dropus_mode_hybrid,                                     /* 14...15 */
        dropus_mode_celt,   dropus_mode_celt,   dropus_mode_celt, dropus_mode_celt, /* 16...19 */
        dropus_mode_celt,   dropus_mode_celt,   dropus_mode_celt, dropus_mode_celt, /* 20...23 */
        dropus_mode_celt,   dropus_mode_celt,   dropus_mode_celt, dropus_mode_celt, /* 24...27 */
        dropus_mode_celt,   dropus_mode_celt,   dropus_mode_celt, dropus_mode_celt  /* 28...31 */
    };

    DROPUS_ASSERT(config < 32);
    return modes[config];
}

DROPUS_INLINE dropus_mode dropus_toc_mode(dropus_uint8 toc)
{
    return dropus_toc_config_mode(dropus_toc_config(toc));
}

DROPUS_INLINE dropus_uint32 dropus_toc_config_sample_rate(dropus_uint8 config)
{
    /* Table 2 with Table 1 in RFC 6716 */
    static dropus_uint32 rates[32] = {
        8000,  8000,  8000,  8000,  /*  0...3  */
        12000, 12000, 12000, 12000, /*  4...7  */
        16000, 16000, 16000, 16000, /*  8...11 */
        24000, 24000,               /* 12...13 */
        48000, 48000,               /* 14...15 */
        8000,  8000,  8000,  8000,  /* 16...19 */
        16000, 16000, 16000, 16000, /* 20...23 */
        24000, 24000, 24000, 24000, /* 24...27 */
        48000, 48000, 48000, 48000  /* 28...31 */
    };

    DROPUS_ASSERT(config < 32);
    return rates[config];
}

DROPUS_INLINE dropus_uint32 dropus_toc_sample_rate(dropus_uint8 toc)
{
    return dropus_toc_config_sample_rate(dropus_toc_config(toc));
}

DROPUS_INLINE dropus_uint32 dropus_toc_config_frame_size_in_pcm_frames(dropus_uint8 config)
{
    /* Table 2 with Table 1 in RFC 6716 */
    static dropus_uint32 sizes[32] = {
        80,  160, 320, 480, /*  0...3  */
        120, 240, 480, 720, /*  4...7  */
        160, 320, 640, 960, /*  8...11 */
        240, 480,           /* 12...13 */
        480, 960,           /* 14...15 */
        20,  40,  80,  160, /* 16...19 */
        40,  80,  160, 320, /* 20...23 */
        60,  120, 240, 480, /* 24...27 */
        120, 240, 480, 960  /* 28...31 */
    };

    DROPUS_ASSERT(config < 32);
    return sizes[config];
}

DROPUS_INLINE dropus_uint32 dropus_toc_frame_size_in_pcm_frames(dropus_uint8 toc)
{
    return dropus_toc_config_frame_size_in_pcm_frames(dropus_toc_config(toc));
}


dropus_result dropus_stream_init(dropus_stream* pOpusStream)
{
    if (pOpusStream == NULL) {
        return DROPUS_INVALID_ARGS;
    }

    DROPUS_ZERO_OBJECT(pOpusStream);

    return DROPUS_SUCCESS;
}

dropus_result dropus_stream_decode_packet(dropus_stream* pOpusStream, const void* pData, size_t dataSize)
{
    if (pOpusStream == NULL || pData == NULL) {
        return DROPUS_INVALID_ARGS;
    }

    /* RFC 6716 - Section 3.4 [R1] Packets are at least one byte. */
    if (dataSize < 1) {
        return DROPUS_BAD_DATA;
    }

    const dropus_uint8* pRunningData8 = (const dropus_uint8*)pData;

    /* The table of contents byte specifies the structure of the packet. */
    dropus_uint8 toc = pRunningData8[0];
    pRunningData8 += 1;
    




    /* TODO: Implement me. */
    return DROPUS_SUCCESS;
}



/***********************************************************************************************************************************************************

High-Level Opus Decoding API

************************************************************************************************************************************************************/

dropus_bool32 dropus_init_internal(dropus* pOpus, dropus_read_proc onRead, dropus_seek_proc onSeek, void* pUserData)
{
    DROPUS_ASSERT(pOpus != NULL);
    DROPUS_ASSERT(onRead != NULL);

    /* Must always have an onRead callback. */
    if (onRead == NULL) {
        return DROPUS_FALSE;
    }

    pOpus->onRead = onRead;
    pOpus->onSeek = onSeek;
    pOpus->pUserData = pUserData;

    /* TODO: Implement me. */
    
    return DROPUS_TRUE;
}

dropus_bool32 dropus_init(dropus* pOpus, dropus_read_proc onRead, dropus_seek_proc onSeek, void* pUserData)
{
    if (pOpus == NULL) {
        return DROPUS_FALSE;    /* Invalid args. */
    }

    DROPUS_ZERO_OBJECT(pOpus);

    return dropus_init_internal(pOpus, onRead, onSeek, pUserData);
}

#ifndef DR_OPUS_NO_STDIO
FILE* dropus_fopen(const char* filename, const char* mode)
{
    FILE* pFile;
#ifdef _MSC_VER
    if (fopen_s(&pFile, filename, mode) != 0) {
        return NULL;
    }
#else
    pFile = fopen(filename, mode);
    if (pFile == NULL) {
        return NULL;
    }
#endif

    return pFile;
}

int dropus_fclose(FILE* pFile)
{
    return fclose(pFile);
}


size_t dropus_on_read_stdio(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    return fread(pBufferOut, 1, bytesToRead, (FILE*)pUserData);
}

dropus_bool32 dropus_on_seek_stdio(void* pUserData, int offset, dropus_seek_origin origin)
{
    return fseek((FILE*)pUserData, offset, (origin == dropus_seek_origin_current) ? SEEK_CUR : SEEK_SET) == 0;
}

dropus_bool32 dropus_init_file(dropus* pOpus, const char* pFilePath)
{
    if (pOpus == NULL) {
        return DROPUS_FALSE;    /* Invalid args. */
    }

    DROPUS_ZERO_OBJECT(pOpus);

    if (pFilePath == NULL || pFilePath[0] == '\0') {
        return DROPUS_FALSE;    /* Invalid args. */
    }

    FILE* pFile = dropus_fopen(pFilePath, "rb");
    if (pFile == NULL) {
        return DROPUS_FALSE;    /* Failed to open file. */
    }

    pOpus->pFile = (void*)pFile;
    dropus_bool32 successful = dropus_init_internal(pOpus, dropus_on_read_stdio, dropus_on_seek_stdio, NULL);
    if (!successful) {
        dropus_fclose(pFile);
        return DROPUS_FALSE;
    }
    
    return DROPUS_TRUE;
}
#endif

static size_t dropus_on_read_memory(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    dropus* pOpus = (dropus*)pUserData;
    DROPUS_ASSERT(pOpus != NULL);
    DROPUS_ASSERT(pOpus->memory.dataSize >= pOpus->memory.currentReadPos);

    size_t bytesRemaining = pOpus->memory.dataSize - pOpus->memory.currentReadPos;
    if (bytesToRead > bytesRemaining) {
        bytesToRead = bytesRemaining;
    }

    if (bytesToRead > 0) {
        DROPUS_COPY_MEMORY(pBufferOut, pOpus->memory.pData + pOpus->memory.currentReadPos, bytesToRead);
        pOpus->memory.currentReadPos += bytesToRead;
    }

    return bytesToRead;
}

static dropus_bool32 dropus_on_seek_memory(void* pUserData, int byteOffset, dropus_seek_origin origin)
{
    dropus* pOpus = (dropus*)pUserData;
    DROPUS_ASSERT(pOpus != NULL);

    if (origin == dropus_seek_origin_current) {
        if (byteOffset > 0) {
            if (pOpus->memory.currentReadPos + byteOffset > pOpus->memory.dataSize) {
                byteOffset = (int)(pOpus->memory.dataSize - pOpus->memory.currentReadPos);  /* Trying to seek too far forward. */
            }
        } else {
            if (pOpus->memory.currentReadPos < (size_t)-byteOffset) {
                byteOffset = -(int)pOpus->memory.currentReadPos;  /* Trying to seek too far backwards. */
            }
        }

        /* This will never underflow thanks to the clamps above. */
        pOpus->memory.currentReadPos += byteOffset;
    } else {
        if ((dropus_uint32)byteOffset <= pOpus->memory.dataSize) {
            pOpus->memory.currentReadPos = byteOffset;
        } else {
            pOpus->memory.currentReadPos = pOpus->memory.dataSize;  /* Trying to seek too far forward. */
        }
    }

    return DROPUS_TRUE;
}

dropus_bool32 dropus_init_memory(dropus* pOpus, const void* pData, size_t dataSize)
{
    if (pOpus == NULL) {
        return DROPUS_FALSE;    /* Invalid args. */
    }

    DROPUS_ZERO_OBJECT(pOpus);

    if (pData == NULL || dataSize == 0) {
        return DROPUS_FALSE;    /* Invalid args. */
    }

    pOpus->memory.pData = pData;
    pOpus->memory.dataSize = dataSize;
    pOpus->memory.currentReadPos = 0;
    return dropus_init_internal(pOpus, dropus_on_read_memory, dropus_on_seek_memory, NULL);
}


void dropus_uninit(dropus* pOpus)
{
    if (pOpus == NULL) {
        return;
    }

#ifndef DR_OPUS_NO_STDIO
    /* Since dr_opus manages the stdio FILE object make sure it's closed on uninitialization. */
    if (pOpus->pFile != NULL) {
        dropus_fclose((FILE*)pOpus->pFile);
    }
#endif
}

#endif  /* DR_OPUS_IMPLEMENTATION */

/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT
===============================================================================
Copyright 2018 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
