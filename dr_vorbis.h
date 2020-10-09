/*
Vorbis audio decoder. Choice of public domain or MIT-0. See license statements at the end of this file.
dr_vorbis - v0.0.0 - WIP

David Reid - mackron@gmail.com

GitHub: https://github.com/mackron/dr_libs
*/

/* WORK IN PROGRESS */

/*
Notes
=====
dr_vorbis is experimenting with a few API ideas that differ from other dr_* libraries. Eventually I want to make all dr_* libraries consistent
with each other, so I'd be interested in feedback on some of these API changes:

  * Everything is now namespaced consistently regarding the placement of the underscore. In dr_flac, etc., the underscore is inconsistent where,
    for example, there would be DR_FLAC_IMPLEMENTATION, but then no underscore like DRFLAC_TRUE. This is just messy. In dr_vorbis, all namesspaces
    are in underscored format at all times: DR_VORBIS_IMPLEMENTATION, DR_VORBIS_TRUE, etc. The same applies for lower case symbols: dr_vorbis_init(),
    dr_vorbis_uint32, etc.

  * Result codes are now returned instead of booleans. I'm experimenting with using error codes in errno.h (EINVAL, EILSEQ, etc.). I'm not sure if
    this is better than dr_vorbis-specific result codes so this may change. The advantage of using errno.h is that error codes from file handling can
    be passed straight through. Invalid arguments will result in EINVAL; invalid data within the bitstream will result in EILSEQ; out of memory will
    result in ENOMEM; a CRC mismatch will result in a custom error code called DR_VORBIS_ECRC; success will result in DR_VORBIS_SUCCESS (0).

  * When initializing an object, the pointer to the object being initialized is now the last parameter of the initialization function. This is
    consistent with my work in miniaudio and is likely here to stay.

  * Allocation callbacks are simplified. Previously, allocation callbacks would allow realloc to be left as NULL in which case it was emulated in
    terms of malloc and free. This is no longer the case with dr_vorbis - the application must now ensure the realloc is specified if they want to
    use custom memory allocation routines.

  * The file APIs (dr_vorbis_init_file*()) are now always declared, regardless of the presence of DR_VORBIS_NO_STDIO. When DR_VORBIS_NO_STDIO is
    defined, these functions will still be implemented, but they will do nothing and return an error. The reason for this setup is to avoid putting
    references to DR_VORBIS_NO_STDIO in the header section. I'm not sure if this change is a good change or not, so this may be changed later.

  * The reading APIs now return a result code and output the frames read via an output parameter.

*/

#ifndef dr_vorbis_h
#define dr_vorbis_h

#ifdef __cplusplus
extern "C" {
#endif

#define DR_VORBIS_STRINGIFY(x)      #x
#define DR_VORBIS_XSTRINGIFY(x)     DR_VORBIS_STRINGIFY(x)

#define DR_VORBIS_VERSION_MAJOR     0
#define DR_VORBIS_VERSION_MINOR     0
#define DR_VORBIS_VERSION_REVISION  0
#define DR_VORBIS_VERSION_STRING    DR_VORBIS_XSTRINGIFY(DR_VORBIS_VERSION_MAJOR) "." DR_VORBIS_XSTRINGIFY(DR_VORBIS_VERSION_MINOR) "." DR_VORBIS_XSTRINGIFY(DR_VORBIS_VERSION_REVISION)

#include <errno.h>
#include <stddef.h> /* For size_t. */

/* Sized types. */
typedef   signed char           dr_vorbis_int8;
typedef unsigned char           dr_vorbis_uint8;
typedef   signed short          dr_vorbis_int16;
typedef unsigned short          dr_vorbis_uint16;
typedef   signed int            dr_vorbis_int32;
typedef unsigned int            dr_vorbis_uint32;
#if defined(_MSC_VER)
    typedef   signed __int64    dr_vorbis_int64;
    typedef unsigned __int64    dr_vorbis_uint64;
#else
    #if defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wlong-long"
        #if defined(__clang__)
            #pragma GCC diagnostic ignored "-Wc++11-long-long"
        #endif
    #endif
    typedef   signed long long  dr_vorbis_int64;
    typedef unsigned long long  dr_vorbis_uint64;
    #if defined(__GNUC__)
        #pragma GCC diagnostic pop
    #endif
#endif
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
    typedef dr_vorbis_uint64    dr_vorbis_uintptr;
#else
    typedef dr_vorbis_uint32    dr_vorbis_uintptr;
#endif
typedef dr_vorbis_uint8         dr_vorbis_bool8;
typedef dr_vorbis_uint32        dr_vorbis_bool32;
#define DR_VORBIS_TRUE          1
#define DR_VORBIS_FALSE         0

#if !defined(DR_VORBIS_API)
    #if defined(DR_VORBIS_DLL)
        #if defined(_WIN32)
            #define DR_VORBIS_DLL_IMPORT  __declspec(dllimport)
            #define DR_VORBIS_DLL_EXPORT  __declspec(dllexport)
            #define DR_VORBIS_DLL_PRIVATE static
        #else
            #if defined(__GNUC__) && __GNUC__ >= 4
                #define DR_VORBIS_DLL_IMPORT  __attribute__((visibility("default")))
                #define DR_VORBIS_DLL_EXPORT  __attribute__((visibility("default")))
                #define DR_VORBIS_DLL_PRIVATE __attribute__((visibility("hidden")))
            #else
                #define DR_VORBIS_DLL_IMPORT
                #define DR_VORBIS_DLL_EXPORT
                #define DR_VORBIS_DLL_PRIVATE static
            #endif
        #endif

        #if defined(DR_VORBIS_IMPLEMENTATION) || defined(DR_VORBIS_IMPLEMENTATION)
            #define DR_VORBIS_API  DR_VORBIS_DLL_EXPORT
        #else
            #define DR_VORBIS_API  DR_VORBIS_DLL_IMPORT
        #endif
        #define DR_VORBIS_PRIVATE DR_VORBIS_DLL_PRIVATE
    #else
        #define DR_VORBIS_API extern
        #define DR_VORBIS_PRIVATE static
    #endif
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1700   /* Visual Studio 2012 */
    #define DR_VORBIS_DEPRECATED       __declspec(deprecated)
#elif (defined(__GNUC__) && __GNUC__ >= 4)  /* GCC 4 */
    #define DR_VORBIS_DEPRECATED       __attribute__((deprecated))
#elif defined(__has_feature)                /* Clang */
    #if __has_feature(attribute_deprecated)
        #define DR_VORBIS_DEPRECATED   __attribute__((deprecated))
    #else
        #define DR_VORBIS_DEPRECATED
    #endif
#else
    #define DR_VORBIS_DEPRECATED
#endif

/*
As data is read from the client it is placed into an internal buffer for fast access. This controls the size of that buffer. Larger values means more speed,
but also more memory. In my testing there is diminishing returns after about 4KB, but you can fiddle with this to suit your own needs. Must be a multiple of 8.
*/
#ifndef DR_VORBIS_BUFFER_SIZE
#define DR_VORBIS_BUFFER_SIZE   4096
#endif

/* Check if we can enable 64-bit optimizations. */
#if defined(_WIN64) || defined(_LP64) || defined(__LP64__)
#define DR_VORBIS_64BIT
#endif

#ifdef DR_VORBIS_64BIT
typedef dr_vorbis_uint64 dr_vorbis_cache_t;
#else
typedef dr_vorbis_uint32 dr_vorbis_cache_t;
#endif


/* Some custom result codes. */
typedef int dr_vorbis_result;
#define DR_VORBIS_SUCCESS   0
#define DR_VORBIS_ECRC      -16384  /* CRC mismatch. */


typedef enum
{
    dr_vorbis_metadata_type_vendor, /* Contained in the comment header, reported separately to the other comments. */
    dr_vorbis_metadata_type_comment
} dr_vorbis_metadata_type;

typedef struct
{
    dr_vorbis_metadata_type type;
    union
    {
        struct
        {
            dr_vorbis_uint32 length;
            const char* pData;  /* Null terminated. */
        } vendor, comment;
    } data;
} dr_vorbis_metadata;

typedef enum
{
    dr_vorbis_seek_origin_start,
    dr_vorbis_seek_origin_current
} dr_vorbis_seek_origin;

typedef int (* dr_vorbis_read_data_proc)(void* pUserData, void* pOutput, size_t bytesToRead, size_t* pBytesRead);
typedef int (* dr_vorbis_seek_data_proc)(void* pUserData, dr_vorbis_int64 offset, dr_vorbis_seek_origin origin);
typedef int (* dr_vorbis_meta_data_proc)(void* pUserData, const dr_vorbis_metadata* pMetadata);

typedef struct
{
    void* pUserData;
    void* (* onMalloc)(size_t sz, void* pUserData);
    void* (* onRealloc)(void* p, size_t sz, void* pUserData);
    void  (* onFree)(void* p, void* pUserData);
} dr_vorbis_allocation_callbacks;



/************************************************************************************************************************************************************

Version API

************************************************************************************************************************************************************/
DR_VORBIS_API void dr_vorbis_version(dr_vorbis_uint32* pMajor, dr_vorbis_uint32* pMinor, dr_vorbis_uint32* pRevision);
DR_VORBIS_API const char* dr_vorbis_version_string(void);


/************************************************************************************************************************************************************

Bitstream API

This is a low-level API for reading bits from a Vorbis bitstream. I've made this generic and public in case others might find it useful and to make it easier
to extract in case we want to use it for other decoders in the future.

The bitstream API has two levels of cache. The first is an element of either 32 or 64 bits, depending on the target architecture. The second is a buffer of
dr_vorbis_cache_t items which is filled from the read callback. Data flows like so:

  onRead -> L2 -> L1 -> [Output]

When all of the bits in L1 have been consumed, it's filled with fresh data from L2. When all of the items in L2 have been consumed, it'll be refilled with
data from the read callback. The reason for this design is to ensure the read callback is not called too frequently thereby causing too much inefficiency.

************************************************************************************************************************************************************/
typedef struct
{
    
    dr_vorbis_cache_t l1;               /* Data flows from onRead -> l2 -> l1 -> dr_vorbis_bs_read_bits(). */
    dr_vorbis_cache_t l2[DR_VORBIS_BUFFER_SIZE / sizeof(dr_vorbis_cache_t)];    /* Data is moved from here into l1. */
    dr_vorbis_uint32 l1RemainingBits;   /* Number of bits remaining in l1. */
    dr_vorbis_uint32 l2RemainingLines;  /* Number of dr_vorbis_cache_t items remaining in l2. */
    dr_vorbis_uint32 l2RemainingBytes;  /* Number of bytes remaining in l2. */
    dr_vorbis_read_data_proc onRead;    /* Used for filling the L2 buffer with fresh data. */
    void* pReadUserData;
} dr_vorbis_bs;

DR_VORBIS_API dr_vorbis_result dr_vorbis_bs_init(void* pReadUserData, dr_vorbis_read_data_proc onRead, dr_vorbis_bs* pBS);
DR_VORBIS_API dr_vorbis_result dr_vorbis_bs_read_bits(dr_vorbis_bs* pBS, dr_vorbis_uint32 bitsToRead, dr_vorbis_uint32* pValue);
DR_VORBIS_API dr_vorbis_result dr_vorbis_bs_read_bytes(dr_vorbis_bs* pBS, void* pOutput, size_t bytesToRead, size_t* pBytesRead);


/************************************************************************************************************************************************************

Vorbis Stream API

This is the low-level decoding API. As input it takes a raw stream of Vorbis audio data. It does *not* include container information. If a container such
as Ogg is being used, the data needs to be extracted from the container first. To do this, the dr_vorbis_ogg API can be used. Otherwise an application can
do their own container management to extract the Vorbis stream.

************************************************************************************************************************************************************/
typedef struct
{
    dr_vorbis_bs bs;                    /* All data is read through the bitstream. */
    void* pMetaUserData;
    dr_vorbis_meta_data_proc onMeta;    /* Called during processing of the comment header in dr_vorbis_stream_init(). */
    dr_vorbis_allocation_callbacks allocationCallbacks;
    dr_vorbis_uint8 channels;
    dr_vorbis_uint32 sampleRate;
    dr_vorbis_uint16 blockSize0;
    dr_vorbis_uint16 blockSize1;
} dr_vorbis_stream;

DR_VORBIS_API int dr_vorbis_stream_init(void* pReadUserData, dr_vorbis_read_data_proc onRead, void* pMetaUserData, dr_vorbis_meta_data_proc onMeta, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis_stream* pStream);
DR_VORBIS_API int dr_vorbis_stream_uninit(dr_vorbis_stream* pStream);
DR_VORBIS_API int dr_vorbis_stream_reset(dr_vorbis_stream* pStream);    /* Call this to reset internal buffers. Useful for seeking. */
DR_VORBIS_API int dr_vorbis_stream_read_pcm_frames_s16(dr_vorbis_stream* pStream, dr_vorbis_int16* pFramesOut, dr_vorbis_uint64 frameCount, dr_vorbis_uint64* pFramesRead);
DR_VORBIS_API int dr_vorbis_stream_read_pcm_frames_f32(dr_vorbis_stream* pStream, float* pFramesOut, dr_vorbis_uint64 frameCount, dr_vorbis_uint64* pFramesRead);


/************************************************************************************************************************************************************

Generic Container API

Vorbis streams are typically contained within a container format, most often Ogg. The container abstraction allows different containers to be plugged in
with relative ease.

************************************************************************************************************************************************************/
typedef void dr_vorbis_container;

typedef struct
{
    void* pUserData;
    dr_vorbis_read_data_proc onRead;  /* Returned data must be native Vorbis data and *not* container data such as Ogg. */
} dr_vorbis_container_callbacks;

DR_VORBIS_API int dr_vorbis_container_read_vorbis_data(dr_vorbis_container* pContainer, void* pOutput, size_t bytesToRead, size_t* pBytesRead);


/************************************************************************************************************************************************************

Ogg/Vorbis Container API

Use the dr_vorbis_ogg API to extract a Vorbis stream out of an Ogg container. The idea is that you read data from the dr_vorbis_ogg API, and then pass that
as input into the dr_vorbis_stream API.

The dr_vorbis_ogg object is a dr_vorbis_container object, which means it can be plugged directly into dr_vorbis_container_*() APIs.

************************************************************************************************************************************************************/
#define DR_VORBIS_OGG_MAX_PAGE_SIZE 65307

typedef struct
{
    dr_vorbis_uint8 capturePattern[4];  /* Should be "OggS" */
    dr_vorbis_uint8 structureVersion;   /* Always 0. */
    dr_vorbis_uint8 headerType;
    dr_vorbis_uint64 granulePosition;
    dr_vorbis_uint32 serialNumber;
    dr_vorbis_uint32 sequenceNumber;
    dr_vorbis_uint32 checksum;
    dr_vorbis_uint8 segmentCount;
    dr_vorbis_uint8 segmentTable[255];
} dr_vorbis_ogg_page_header;

typedef struct
{
    dr_vorbis_container_callbacks cb;       /* Must be the first member. */
    void* pUserData;                        /* Passed into the onRead and onSeek callbacks. */
    dr_vorbis_read_data_proc onRead;        /* Returned data will be Ogg data and *not* the raw Vorbis stream. */
    dr_vorbis_seek_data_proc onSeek;        /* Seeks based on the Ogg stream, *not* the raw Vorbis stream. */
    dr_vorbis_uint32 vorbisSerialNumber;    /* The serial number assigned to Ogg pages which relate to our Vorbis stream. */
    dr_vorbis_uint16 pageDataRead;          /* The amount of data that's been read from the page. Used with pageDataSize to calculate how much data remains. */
    dr_vorbis_uint16 pageDataSize;          /* The size of the current page. */
#ifndef DR_VORBIS_NO_CRC
    dr_vorbis_uint8 pageData[DR_VORBIS_OGG_MAX_PAGE_SIZE];  /* Not used when CRC is disabled. */
#endif
} dr_vorbis_ogg;

DR_VORBIS_API int dr_vorbis_ogg_init(void* pUserData, dr_vorbis_read_data_proc onRead, dr_vorbis_seek_data_proc onSeek, dr_vorbis_ogg* pOgg);
DR_VORBIS_API int dr_vorbis_ogg_uninit(dr_vorbis_ogg* pOgg);
DR_VORBIS_API int dr_vorbis_ogg_read_vorbis_data(dr_vorbis_ogg* pOgg, void* pOutput, size_t bytesToRead, size_t* pBytesRead);


/************************************************************************************************************************************************************

Main API

This is the primary API that most applications will be using to decode Vorbis files.

************************************************************************************************************************************************************/
typedef struct
{
    union
    {
        dr_vorbis_ogg ogg;
    } container;                /* The container. Data is read from this and then passed as input into the stream. */
    dr_vorbis_stream stream;    /* The low-level stream object. Data is read from the container and then passed as input into the stream. */

    union
    {
        /*FILE**/void* pFile;
        struct
        {
            const dr_vorbis_uint8* pData;
            size_t dataSize;
            size_t currentReadPos;
        } memory;               /* Only used for decoders that were opened against a block of memory. */
    } backend;                  /* Private data for the file and memory initialization APIs. */
} dr_vorbis;

DR_VORBIS_API int dr_vorbis_init_ex(void* pReadSeekUserData, dr_vorbis_read_data_proc onRead, dr_vorbis_seek_data_proc onSeek, void* pMetaUserData, dr_vorbis_meta_data_proc onMeta, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis);
DR_VORBIS_API int dr_vorbis_init(void* pReadSeekUserData, dr_vorbis_read_data_proc onRead, dr_vorbis_seek_data_proc onSeek, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis);
DR_VORBIS_API int dr_vorbis_init_file_ex(const char* pFilePath, void* pMetaUserData, dr_vorbis_meta_data_proc onMeta, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis);
DR_VORBIS_API int dr_vorbis_init_file_w_ex(const wchar_t* pFilePath, void* pMetaUserData, dr_vorbis_meta_data_proc onMeta, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis);
DR_VORBIS_API int dr_vorbis_init_file(const char* pFilePath, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis);
DR_VORBIS_API int dr_vorbis_init_file_w(const wchar_t* pFilePath, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis);
DR_VORBIS_API int dr_vorbis_uninit(dr_vorbis* pVorbis);
DR_VORBIS_API int dr_vorbis_read_pcm_frames_s16(dr_vorbis* pVorbis, dr_vorbis_int16* pFramesOut, dr_vorbis_uint64 framesCount, dr_vorbis_uint64* pFramesRead);
DR_VORBIS_API int dr_vorbis_read_pcm_frames_f32(dr_vorbis* pVorbis, float* pFramesOut, dr_vorbis_uint64 frameCount, dr_vorbis_uint64* pFramesRead);
DR_VORBIS_API int dr_vorbis_seek_to_pcm_frame(dr_vorbis* pVorbis, dr_vorbis_uint64 frameIndex);


/************************************************************************************************************************************************************

Miscellaneous APIs

************************************************************************************************************************************************************/
DR_VORBIS_API void* dr_vorbis_malloc(size_t sz, const dr_vorbis_allocation_callbacks* pAllocationCallbacks);
DR_VORBIS_API void* dr_vorbis_realloc(void* p, size_t sz, const dr_vorbis_allocation_callbacks* pAllocationCallbacks);
DR_VORBIS_API void dr_vorbis_free(void* p, const dr_vorbis_allocation_callbacks* pAllocationCallbacks);


#ifdef __cplusplus
}
#endif
#endif  /* dr_vorbis_h */

/************************************************************************************************************************************************************
*************************************************************************************************************************************************************

IMPLEMENTATION

*************************************************************************************************************************************************************
************************************************************************************************************************************************************/
#if defined(DR_VORBIS_IMPLEMENTATION)
#ifndef dr_vorbis_c
#define dr_vorbis_c

#include <stdlib.h> /* malloc(), realloc(), free() */
#include <string.h> /* memset(), memcpy() */
#include <assert.h> /* assert() */

#ifndef DR_VORBIS_NO_STDIO
#include <stdio.h>  /* FILE */
#endif

#ifdef _MSC_VER
    #define DR_VORBIS_INLINE __forceinline
#elif defined(__GNUC__)
    /*
    I've had a bug report where GCC is emitting warnings about functions possibly not being inlineable. This warning happens when
    the __attribute__((always_inline)) attribute is defined without an "inline" statement. I think therefore there must be some
    case where "__inline__" is not always defined, thus the compiler emitting these warnings. When using -std=c89 or -ansi on the
    command line, we cannot use the "inline" keyword and instead need to use "__inline__". In an attempt to work around this issue
    I am using "__inline__" only when we're compiling in strict ANSI mode.
    */
    #if defined(__STRICT_ANSI__)
        #define DR_VORBIS_INLINE __inline__ __attribute__((always_inline))
    #else
        #define DR_VORBIS_INLINE inline __attribute__((always_inline))
    #endif
#else
    #define DR_VORBIS_INLINE
#endif

/* CPU architecture. */
#if defined(__x86_64__) || defined(_M_X64)
    #define DR_VORBIS_X64
#elif defined(__i386) || defined(_M_IX86)
    #define DR_VORBIS_X86
#elif defined(__arm__) || defined(_M_ARM)
    #define DR_VORBIS_ARM
#endif


#ifndef DR_VORBIS_MALLOC
#define DR_VORBIS_MALLOC(sz)                malloc((sz))
#endif
#ifndef DR_VORBIS_REALLOC
#define DR_VORBIS_REALLOC(p, sz)            realloc((p), (sz))
#endif
#ifndef DR_VORBIS_FREE
#define DR_VORBIS_FREE(p)                   free((p))
#endif
#ifndef DR_VORBIS_ZERO_MEMORY
#define DR_VORBIS_ZERO_MEMORY(p, sz)        memset((p), 0, (sz))
#endif
#ifndef DR_VORBIS_COPY_MEMORY
#define DR_VORBIS_COPY_MEMORY(dst, src, sz) memcpy((dst), (src), (sz))
#endif
#ifndef DR_VORBIS_MOVE_MEMORY
#define DR_VORBIS_MOVE_MEMORY(dst, src, sz) memmove((dst), (src), (sz))
#endif
#ifndef DR_VORBIS_ASSERT
#define DR_VORBIS_ASSERT(condition)         assert(condition)
#endif
#define DR_VORBIS_ZERO_OBJECT(p)            DR_VORBIS_ZERO_MEMORY((p), sizeof(*(p)))
#define DR_VORBIS_COUNTOF(x)                (sizeof(x) / sizeof(x[0]))
#define DR_VORBIS_OFFSET_PTR(p, offset)     (((dr_vorbis_uint8*)(p)) + (offset))


/* The size of the identification header is always 30 bytes. */
#define DR_VORBIS_IDENTIFICATION_HEADER_SIZE    30

/* Packet types. */
#define DR_VORBIS_PACKET_TYPE_AUDIO             0
#define DR_VORBIS_PACKET_TYPE_IDENTIFICATION    1
#define DR_VORBIS_PACKET_TYPE_COMMENT           3
#define DR_VORBIS_PACKET_TYPE_SETUP             5



/************************************************************************************************************************************************************

Private Helpers

************************************************************************************************************************************************************/
static dr_vorbis_uint32 dr_vorbis_bytes_to_uint32(const dr_vorbis_uint8* data)
{
    return (data[0] << 0) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}


static void* dr_vorbis_malloc_default(size_t sz, void* pUserData)
{
    (void)pUserData;
    return DR_VORBIS_MALLOC(sz);
}

static void* dr_vorbis_realloc_default(void* p, size_t sz, void* pUserData)
{
    (void)pUserData;
    return DR_VORBIS_REALLOC(p, sz);
}

static void dr_vorbis_free_default(void* p, void* pUserData)
{
    (void)pUserData;
    DR_VORBIS_FREE(p);
}

static dr_vorbis_allocation_callbacks dr_vorbis_allocation_callbacks_init_default()
{
    dr_vorbis_allocation_callbacks callbacks;

    callbacks.pUserData = NULL;
    callbacks.onMalloc  = dr_vorbis_malloc_default;
    callbacks.onRealloc = dr_vorbis_realloc_default;
    callbacks.onFree    = dr_vorbis_free_default;

    return callbacks;
}

static dr_vorbis_allocation_callbacks dr_vorbis_allocation_callbacks_init_copy_or_default(const dr_vorbis_allocation_callbacks* pSrc)
{
    if (pSrc == NULL) {
        return dr_vorbis_allocation_callbacks_init_default();
    } else {
        dr_vorbis_allocation_callbacks callbacks;

        callbacks.pUserData = pSrc->pUserData;
        callbacks.onMalloc  = pSrc->onMalloc;
        callbacks.onRealloc = pSrc->onRealloc;
        callbacks.onFree    = pSrc->onFree;

        return callbacks;
    }
}


/************************************************************************************************************************************************************

Version API

************************************************************************************************************************************************************/
DR_VORBIS_API void dr_vorbis_version(dr_vorbis_uint32* pMajor, dr_vorbis_uint32* pMinor, dr_vorbis_uint32* pRevision)
{
    if (pMajor) {
        *pMajor = DR_VORBIS_VERSION_MAJOR;
    }

    if (pMinor) {
        *pMinor = DR_VORBIS_VERSION_MINOR;
    }

    if (pRevision) {
        *pRevision = DR_VORBIS_VERSION_REVISION;
    }
}

DR_VORBIS_API const char* dr_vorbis_version_string(void)
{
    return DR_VORBIS_VERSION_STRING;
}



/************************************************************************************************************************************************************

Bitstream API

************************************************************************************************************************************************************/
#define DR_VORBIS_L1_MASK(bitCount) ((1 << bitCount) - 1)

DR_VORBIS_API dr_vorbis_result dr_vorbis_bs_init(void* pReadUserData, dr_vorbis_read_data_proc onRead, dr_vorbis_bs* pBS)
{
    if (pBS == NULL) {
        return EINVAL;
    }

    DR_VORBIS_ZERO_OBJECT(pBS);

    if (onRead == NULL) {
        return EINVAL;  /* A read callback is required. */
    }

    pBS->onRead        = onRead;
    pBS->pReadUserData = pReadUserData;

    return DR_VORBIS_SUCCESS;
}

static DR_VORBIS_INLINE dr_vorbis_result dr_vorbis_bs_reload_l1(dr_vorbis_bs* pBS)
{
    dr_vorbis_result result;

    DR_VORBIS_ASSERT(pBS != NULL);

    /*
    If there's nothing in the L2 cache we'll need to reload it first. Note that if we aren't able to load the entire L2 cache, we'll just need to
    slide it down a bit to account for the pBS->l2RemainingLines variable which is used to determine the next cache line to load into L1.
    */
    if (pBS->l2RemainingBytes == 0) {
        size_t bytesRead;
        result = pBS->onRead(pBS->pReadUserData, pBS->l2, sizeof(pBS->l2), &bytesRead);
        if (result != DR_VORBIS_SUCCESS) {
            return result;
        }

        /* If we didn't read any data, we were unable to reload the L1 cache. */
        if (bytesRead == 0) {
            return ERANGE;
        }

        /*
        It's OK if we didn't read enough for the entire L2 cache. We just need to make sure the data is positioned correctly in the cache and
        that the bytes and lines remaining is initialized correctly.
        */
        if (bytesRead != sizeof(pBS->l2)) {
            /* Slide the buffer down so that l2RemainigBytes lines up with the data in the buffer. */
            DR_VORBIS_MOVE_MEMORY(pBS->l2 + (sizeof(pBS->l2) - bytesRead), pBS->l2, bytesRead);
        }

        pBS->l2RemainingBytes = (dr_vorbis_uint32)bytesRead;
        pBS->l2RemainingLines = (dr_vorbis_uint32)bytesRead / sizeof(dr_vorbis_cache_t);
            
        /*
        Make sure we handle the remainder. Use of the mod operator doesn't really matter here in terms of performance because this will only
        get hit right at the very end of the stream, once.
        */
        if ((pBS->l2RemainingLines % sizeof(dr_vorbis_cache_t)) != 0) {
            pBS->l2RemainingLines += 1;
        }
    }

    if (pBS->l2RemainingLines > 0) {
        pBS->l1 = pBS->l2[DR_VORBIS_COUNTOF(pBS->l2) - pBS->l2RemainingLines];
        if (pBS->l2RemainingBytes >= sizeof(dr_vorbis_cache_t)) {
            pBS->l1RemainingBits   = sizeof(dr_vorbis_cache_t)*8;
            pBS->l2RemainingBytes -= sizeof(dr_vorbis_cache_t);
        } else {
            pBS->l1RemainingBits   = pBS->l2RemainingBytes*8;
            pBS->l2RemainingBytes  = 0;
        }

        pBS->l2RemainingLines -= 1;
    } else {
        /* There's nothing available in the L2 cache. */
        return ERANGE;
    }

    return DR_VORBIS_SUCCESS;
}

DR_VORBIS_API dr_vorbis_result dr_vorbis_bs_read_bits(dr_vorbis_bs* pBS, dr_vorbis_uint32 bitsToRead, dr_vorbis_uint32* pValue)
{
    /* Exception: We're checking inputs with an assert rather than a runtime check because this function may be called in high-performance scenarios. */
    DR_VORBIS_ASSERT(pBS != NULL);
    DR_VORBIS_ASSERT(bitsToRead <= 32); /* bitsToRead *can* be zero, and is well defined by the spec. */

    /*
    Bits are read starting from LSB. We use a sort of sliding system where we read bits, and then shift everything down so that the next bits to be read
    are sitting in the LSB position. There is a case where we may be straddling the L1 cache where part of the data is in L1, but then a reload from L2
    is required to read the rest. This is done in a slightly slower path which constructs the data in two steps.
    */
    if (pBS->l1RemainingBits >= bitsToRead) {
        /*
        Fast path. Read directly from the L1 and slide. Unfortunately it's possible for 32-bits to be requested (according to the spec), but 32-bit
        shifts are not well defined for 32-bit integers (*grumble*). We'll run a slightly less efficient path on 32-bit to work around this (one
        extra branch)
        */
    #if defined(DR_VORBIS_32BIT)
        if (bitsToRead == 32) {
            *pValue = l1;
            pBS->l1 = 0;
        } else
    #endif
        {
            *pValue = (pBS->l1 & DR_VORBIS_L1_MASK(bitsToRead));    /* Select. */
            pBS->l1 = pBS->l1 >> bitsToRead;                        /* Consume. */
        }

        pBS->l1RemainingBits -= bitsToRead;
    } else {
        /* Not enough data in L1 to read everything. Requires a two step construction. */
        dr_vorbis_result result;
        dr_vorbis_uint32 step1BitCount = pBS->l1RemainingBits;
        dr_vorbis_uint32 step2BitCount = bitsToRead - pBS->l1RemainingBits;

        /* Relevant asserts because shifting a 32-bit integer by 32 bits is not well defined, which will happen when compiling for 32-bit environments. */
        DR_VORBIS_ASSERT(step1BitCount < 32);
        DR_VORBIS_ASSERT(step2BitCount < 32);

        /* Step 1: Extract the remaining data from L1. */
        *pValue = (pBS->l1 & DR_VORBIS_L1_MASK(step1BitCount));

        /* Step 2: Reload L1 and read the remainder. */
        result = dr_vorbis_bs_reload_l1(pBS);
        if (result != DR_VORBIS_SUCCESS) {
            pBS->l1 = 0;
            pBS->l1RemainingBits = 0;
            return result;
        }

        if (pBS->l1RemainingBits >= step2BitCount) {
            *pValue |= (pBS->l1 & DR_VORBIS_L1_MASK(step2BitCount)) << step1BitCount;
            pBS->l1 >>= step2BitCount;
            pBS->l1RemainingBits -= step2BitCount;
        } else {
            return EILSEQ;  /* Not enough data available to read the entire value. */
        }
    }

    return DR_VORBIS_SUCCESS;
}

DR_VORBIS_API dr_vorbis_result dr_vorbis_bs_read_bytes(dr_vorbis_bs* pBS, void* pOutput, size_t bytesToRead, size_t* pBytesRead)
{
    dr_vorbis_result result = DR_VORBIS_SUCCESS;
    size_t totalBytesRead;

    /*
    Reading an array of bytes should only ever happen in byte-clean aligned scenarios. We can exploit this property to make this more efficient than reading
    individual bits. For now, however, we're just going to wrap this around dr_vorbis_bs_read_bits() just to keep it simple while we're developing.
    */
    totalBytesRead = 0;
    while (totalBytesRead < bytesToRead) {
        dr_vorbis_uint32 byte;
        result = dr_vorbis_bs_read_bits(pBS, 8, &byte);
        if (result != DR_VORBIS_SUCCESS) {
            break;  /* Failed to read data. Abort. */
        }

        ((dr_vorbis_uint8*)pOutput)[totalBytesRead] = (dr_vorbis_uint8)(byte & 0xFF);
        totalBytesRead += 1;
    }

    if (pBytesRead != NULL) {
        *pBytesRead = totalBytesRead;
    }

    return result;
}



/************************************************************************************************************************************************************

Vorbis Stream API

************************************************************************************************************************************************************/
#define DR_VORBIS_UNUSED_CODEWORD   0

/*
Used by dr_vorbis_float32_unpack() to raise 2 to an integer power. I'm just doing this myself to avoid a math.h dependency, but may change this
later to just use ldexp().
*/
static double dr_vorbis_ldexp(double x, int exp)
{
    /* Manual implementation to void dependency on math.h, and more importantly, the "-lm" linker option on GCC. Probably temporary. */
    int i;
    int y = 1;

    for (i = 0; i < exp; i += 1) {
        y *= 2;
    }

    return x * y;
}

/* Used in dr_vorbis_lookup1_values() to raise an unsigned integer to a power. */
static dr_vorbis_uint32 dr_vorbis_pow_ui(dr_vorbis_uint32 x, dr_vorbis_uint32 exp)
{
    /* Manual implementation to void dependency on math.h, and more importantly, the "-lm" linker option on GCC. Probably temporary. */
    dr_vorbis_uint32 i;
    dr_vorbis_uint32 y = 1;

    for (i = 0; i < exp; i += 1) {
        y *= x;
    }

    return y;
}

/* ilog() implementation from spec. */
static dr_vorbis_int32 dr_vorbis_ilog(dr_vorbis_int32 x)
{
#if 1   /* Spec implementation. I don't like this implementation, but left here for reference and debugging. */
    dr_vorbis_int32 result = 0;
    while (x > 0) {
        result += 1;
        x >>= 1;
        x &= ~0x80000000;   /* Explicitly set MSB to 0 as mentioned in spec, but shouldn't be necessary in practice. */
    }

    return result;
#endif
}

static dr_vorbis_uint32 dr_vorbis_ilog_u(dr_vorbis_uint32 x)
{
    return (dr_vorbis_int32)dr_vorbis_ilog((dr_vorbis_int32)x);
}

/* float32_unpack() implementation from spec. */
static float dr_vorbis_float32_unpack(dr_vorbis_uint32 x)
{
    dr_vorbis_uint32 mantissa =  x & 0x001FFFFF;
    dr_vorbis_uint32 sign     =  x & 0x80000000;
    dr_vorbis_uint32 exponent = (x & 0x7FE00000) >> 21;
    double result = (double)mantissa;

    if (sign) {
        result = -result;
    }

    return (float)dr_vorbis_ldexp(result, (int)(exponent - 788));
}

/* lookup1_values(). Spec does not specify an exact implementation for this, but is implied. */
static dr_vorbis_uint32 dr_vorbis_lookup1_values(dr_vorbis_uint32 entries, dr_vorbis_uint32 dimensions)
{
#if 1 /* Naive solution is to run in a loop. Leaving this here for reference and debugging. */
    dr_vorbis_uint32 result = 0;
    for (;;) {
        dr_vorbis_uint32 p = dr_vorbis_pow_ui(result + 1, dimensions);
        if (p > entries) {
            break;
        }

        result += 1;
    }

    return result;
#endif
}


static int dr_vorbis_stream_read_bits(dr_vorbis_stream* pStream, dr_vorbis_uint32 bitsToRead, dr_vorbis_uint32* pValue)
{
    DR_VORBIS_ASSERT(pStream != NULL);

    return dr_vorbis_bs_read_bits(&pStream->bs, bitsToRead, pValue);
}

static int dr_vorbis_stream_read_bytes(dr_vorbis_stream* pStream, void* pOutput, size_t bytesToRead, size_t* pBytesRead)
{
    DR_VORBIS_ASSERT(pStream != NULL);

    return dr_vorbis_bs_read_bytes(&pStream->bs, pOutput, bytesToRead, pBytesRead);
}

static int dr_vorbis_stream_skip_bytes(dr_vorbis_stream* pStream, size_t bytesToSkip, size_t* pBytesSkipped)
{
    int result = 0;                 /* <-- Must be initialized to 0. */
    size_t totalBytesSkipped = 0;   /* <-- Must be initialized to 0. */

    DR_VORBIS_ASSERT(pStream != NULL);

    /*
    We're just reading and discarding for now. We may want to implement a proper seeking callback for this, but for now we're only using
    this to skip over comments for when no metadata callback is specified which means there's no real performance pressure to justify
    optimizing this right now.
    */
    while (totalBytesSkipped < bytesToSkip) {
        dr_vorbis_uint8 buffer[4096];
        size_t bytesSkipped;
        size_t bytesRemaining = bytesToSkip - totalBytesSkipped;
        size_t bytesToSkipNow = bytesRemaining;
        if (bytesToSkipNow > sizeof(buffer)) {
            bytesToSkipNow = sizeof(buffer);
        }

        result = dr_vorbis_stream_read_bytes(pStream, buffer, bytesToSkipNow, &bytesSkipped);
        totalBytesSkipped += bytesSkipped;

        if (result != 0) {
            break;
        }

        if (bytesSkipped != bytesToSkipNow) {
            result = EILSEQ;    /* Premature EOF. */
            break;
        }
    }

    if (pBytesSkipped != NULL) {
        *pBytesSkipped = totalBytesSkipped;
    }

    return result;
}

static int dr_vorbis_stream_read_uint32(dr_vorbis_stream* pStream, dr_vorbis_uint32* pValue)
{
    int result;
    size_t bytesRead;
    dr_vorbis_uint8 data[4];

    DR_VORBIS_ASSERT(pStream != NULL);
    DR_VORBIS_ASSERT(pValue  != NULL);

    result = dr_vorbis_stream_read_bytes(pStream, data, sizeof(data), &bytesRead);
    if (result != 0) {
        return result;
    }

    if (bytesRead != sizeof(data)) {
        return EILSEQ;  /* Premature EOF. */
    }

    *pValue = dr_vorbis_bytes_to_uint32(data);

    return 0;
}

static int dr_vorbis_stream_read_uint8(dr_vorbis_stream* pStream, dr_vorbis_uint8* pValue)
{
    int result;
    size_t bytesRead;
    
    DR_VORBIS_ASSERT(pStream != NULL);
    DR_VORBIS_ASSERT(pValue  != NULL);

    result = dr_vorbis_stream_read_bytes(pStream, pValue, 1, &bytesRead);
    if (result != 0) {
        return result;
    }

    if (bytesRead != 1) {
        return EILSEQ;  /* Premature EOF. */
    }

    return 0;
}

static int dr_vorbis_stream_read_common_header(dr_vorbis_stream* pStream, dr_vorbis_uint8 expectedPacketType)
{
    int result;
    dr_vorbis_uint8 data[7];
    size_t bytesRead;

    DR_VORBIS_ASSERT(pStream != NULL);
    DR_VORBIS_ASSERT(expectedPacketType == 1 || expectedPacketType == 3 || expectedPacketType == 5);

    result = dr_vorbis_stream_read_bytes(pStream, data, 7, &bytesRead);
    if (result != 0) {
        return result;  /* Failed to read data. */
    }

    if (bytesRead != 7) {
        return EILSEQ;  /* Premature EOF. */
    }

    /* [packet_type] (1 byte). */
    if (data[0] != expectedPacketType) {
        return EILSEQ;  /* Unexpected packet type. */
    }

    /* "vorbis". */
    if (data[1] != 'v' || data[2] != 'o' || data[3] != 'r' || data[4] != 'b' || data[5] != 'i' || data[6] != 's') {
        return EILSEQ;  /* Not a vorbis header packet. */
    }

    return 0;
}

static int dr_vorbis_stream_load_identification_header(dr_vorbis_stream* pStream)
{
    int result;
    dr_vorbis_uint8 data[23];
    size_t bytesRead;
    dr_vorbis_uint32 vorbisVersion;
    dr_vorbis_uint16 blockSize0;
    dr_vorbis_uint16 blockSize1;

    /* Common header. */
    result = dr_vorbis_stream_read_common_header(pStream, DR_VORBIS_PACKET_TYPE_IDENTIFICATION);
    if (result != 0) {
        return result;
    }


    /* Identification-specific data. */
    result = dr_vorbis_stream_read_bytes(pStream, data, sizeof(data), &bytesRead);
    if (result != 0) {
        return result;  /* Don't bother about any kind of CRC recovery at this point. If that fails, we can't be sure that it's a valid Vorbis file so just abort. */
    }

    if (bytesRead != sizeof(data)) {
        return EILSEQ;
    }


    /* [vorbis_version] (4 bytes). Spec: [vorbis_version] is to read '0' ... */
    vorbisVersion = dr_vorbis_bytes_to_uint32(data + 0);
    if (vorbisVersion != 0) {
        return EILSEQ;
    }

    /* [audio_channels] (1 byte). Spec: Both [audio_channels] and [audio_sample_rate] must read greater than zero. */
    pStream->channels = data[4];
    if (pStream->channels <= 0) {
        return EILSEQ;
    }

    /* [audio_sample_rate] (4 bytes). Spec: Both [audio_channels] and [audio_sample_rate] must read greater than zero. */
    pStream->sampleRate = dr_vorbis_bytes_to_uint32(data + 5);
    if (pStream->channels <= 0) {
        return EILSEQ;
    }

    /* [bitrate_maximum], [bitrate_nominal], [bitrate_minimum]. Ignored. Spec does not mention validation rules for these fields. */

    /* [blocksize_0], [blocksize_1] (4 bits each, total 1 byte). Spec: [blocksize_0] must be less than or equal to [blocksize_1]. */
    blockSize0 = (data[21] & 0x0F);
    blockSize1 = (data[21] & 0xF0) >> 4;
    if (blockSize0 > blockSize1) {
        return EILSEQ;
    }

    pStream->blockSize0 = 1 << blockSize0;
    pStream->blockSize1 = 1 << blockSize1;

    /* Spec: Allowed final blocksize values are 64, 128, 256, 512, 1024, 2048, 4096 and 8192 ... */
    if ((pStream->blockSize0 < 64 || pStream->blockSize0 > 8192) || (pStream->blockSize1 < 64 || pStream->blockSize1 > 8192)) {
        return EILSEQ;
    }

    /* [framing_flag] (1 bit). Spec: The framing bit must be nonzero. */
    if ((data[22] & 0x01) == 0) {
        return EILSEQ;
    }
    
    return 0;
}

static int dr_vorbis_stream_load_comment_header(dr_vorbis_stream* pStream)
{
    int result;
    size_t bytesRead;
    dr_vorbis_metadata metadata;
    dr_vorbis_uint32 commentCount;
    dr_vorbis_uint32 iComment;
    dr_vorbis_uint8 framingBit;
    dr_vorbis_uint32 length;
    char* pData = NULL;             /* <-- Must be initialized to NULL. */
    size_t dataCap = 0;             /* <-- Must be initialized to 0. */

    DR_VORBIS_ASSERT(pStream != NULL);

    /* Common header. */
    result = dr_vorbis_stream_read_common_header(pStream, DR_VORBIS_PACKET_TYPE_COMMENT);
    if (result != 0) {
        return result;
    }

    /* Comment-specific data. */

    /* Vendor. */
    result = dr_vorbis_stream_read_uint32(pStream, &length);
    if (result != 0) {
        return result;
    }

    if (length > 0) {
        if (pStream->onMeta != NULL) {
            dataCap = length + 1;

            pData = dr_vorbis_malloc(dataCap, &pStream->allocationCallbacks);
            if (pData == NULL) {
                return ENOMEM;
            }

            result = dr_vorbis_stream_read_bytes(pStream, pData, length, &bytesRead);
            if (result != 0) {
                dr_vorbis_free(pData, &pStream->allocationCallbacks);
                return result;
            }

            if (bytesRead != length) {
                dr_vorbis_free(pData, &pStream->allocationCallbacks);
                return EILSEQ;  /* Premature EOF. */
            }

            pData[length] = '\0';

            metadata.type = dr_vorbis_metadata_type_vendor;
            metadata.data.vendor.length = length;
            metadata.data.vendor.pData  = pData;
            pStream->onMeta(pStream->pMetaUserData, &metadata);
        } else {
            result = dr_vorbis_stream_skip_bytes(pStream, length, &bytesRead);
            if (result != 0) {
                return result;  /* Failed to seek past the length. */
            }

            if (bytesRead != length) {
                return EILSEQ;  /* Premature EOF. */
            }
        }
    }


    /* Next 32 bits is the comment count. */
    result = dr_vorbis_stream_read_uint32(pStream, &commentCount);
    if (result != 0) {
        dr_vorbis_free(pData, &pStream->allocationCallbacks);
        return result;
    }

    for (iComment = 0; iComment < commentCount; iComment += 1) {
        result = dr_vorbis_stream_read_uint32(pStream, &length);
        if (result != 0) {
            return result;
        }

        if (pStream->onMeta != NULL) {
            /* Allocate more memory, only if required. */
            dr_vorbis_uint32 newDataCap = length + 1;

            if (newDataCap > dataCap) {
                char* pNewData;

                pNewData = dr_vorbis_realloc(pData, newDataCap, &pStream->allocationCallbacks);
                if (pNewData == NULL) {
                    dr_vorbis_free(pData, &pStream->allocationCallbacks);
                    return ENOMEM;
                }

                pData   = pNewData;
                dataCap = newDataCap;
            }

            result = dr_vorbis_stream_read_bytes(pStream, pData, length, &bytesRead);
            if (result != 0) {
                dr_vorbis_free(pData, &pStream->allocationCallbacks);
                return result;
            }

            if (bytesRead != length) {
                dr_vorbis_free(pData, &pStream->allocationCallbacks);
                return EILSEQ;  /* Premature EOF. */
            }

            pData[length] = '\0';

            metadata.type = dr_vorbis_metadata_type_comment;
            metadata.data.vendor.length = length;
            metadata.data.vendor.pData  = pData;
            pStream->onMeta(pStream->pMetaUserData, &metadata);
        } else {
            result = dr_vorbis_stream_skip_bytes(pStream, length, &bytesRead);
            if (result != 0) {
                return result;  /* Failed to seek past the length. */
            }

            if (bytesRead != length) {
                return EILSEQ;  /* Premature EOF. */
            }
        }
    }
    
    /* Framing bit. */
    result = dr_vorbis_stream_read_uint8(pStream, &framingBit);
    if (result != 0) {
        dr_vorbis_free(pData, &pStream->allocationCallbacks);
        return result;
    }

    if ((framingBit & 0x01) == 0) {
        dr_vorbis_free(pData, &pStream->allocationCallbacks);
        return EILSEQ;  /* Framing bit must be set. */
    }

    /* Done. Make sure we free the data pointer. */
    dr_vorbis_free(pData, &pStream->allocationCallbacks);
    return 0;
}

static int dr_vorbis_stream_load_setup_header_codebooks(dr_vorbis_stream* pStream)
{
    int result;
    dr_vorbis_uint32 codebookCount;
    dr_vorbis_uint32 iCodebook;

    DR_VORBIS_ASSERT(pStream != NULL);

    result = dr_vorbis_stream_read_bits(pStream, 8, &codebookCount);
    if (result != 0) {
        return result;
    }
    codebookCount += 1; /* Spec: read eight bits as unsigned integer and add one */

    for (iCodebook = 0; iCodebook < codebookCount; iCodebook += 1) {
        dr_vorbis_uint32 sync;
        dr_vorbis_uint32 codebookDimensions;
        dr_vorbis_uint32 codebookEntries;
        dr_vorbis_uint32 codebookLookupType;
        dr_vorbis_uint32 ordered;

        result = dr_vorbis_stream_read_bits(pStream, 24, &sync);
        if (result != DR_VORBIS_SUCCESS) {
            return result;  /* Failed to load sync pattern. */
        }

        if (sync != 0x564342) {
            return EILSEQ;  /* Expecting sync pattern. */
        }

        result = dr_vorbis_stream_read_bits(pStream, 16, &codebookDimensions);
        if (result != DR_VORBIS_SUCCESS) {
            return result;  /* Failed to load dimensions. */
        }

        result = dr_vorbis_stream_read_bits(pStream, 24, &codebookEntries);
        if (result != DR_VORBIS_SUCCESS) {
            return result;
        }

        result = dr_vorbis_stream_read_bits(pStream, 1, &ordered);
        if (result != DR_VORBIS_SUCCESS) {
            return result;
        }

        if (ordered == 0) {
            /* Ordered flag is unset. */
            dr_vorbis_uint32 sparse;
            dr_vorbis_uint32 iEntry;

            result = dr_vorbis_stream_read_bits(pStream, 1, &sparse);
            if (result != DR_VORBIS_SUCCESS) {
                return result;
            }

            for (iEntry = 0; iEntry < codebookEntries; iEntry += 1) {
                dr_vorbis_uint32 length;

                if (sparse) {
                    dr_vorbis_uint32 flag;

                    result = dr_vorbis_stream_read_bits(pStream, 1, &flag);
                    if (result != DR_VORBIS_SUCCESS) {
                        return result;
                    }

                    if (flag) {
                        result = dr_vorbis_stream_read_bits(pStream, 5, &length);
                        if (result != DR_VORBIS_SUCCESS) {
                            return result;
                        }

                        length += 1;
                    } else {
                        length = DR_VORBIS_UNUSED_CODEWORD;
                    }
                } else {
                    result = dr_vorbis_stream_read_bits(pStream, 5, &length);
                    if (result != DR_VORBIS_SUCCESS) {
                        return result;
                    }

                    length += 1;
                }

                /* TODO: Store the length. */
                /*pCodebookCodewordLengths[iEntry] = length;*/
            }
        } else {
            /* Ordered flag is set. */
            dr_vorbis_uint32 currentEntry = 0;
            dr_vorbis_uint32 currentLength;

            result = dr_vorbis_stream_read_bits(pStream, 5, &currentLength);
            if (result != DR_VORBIS_SUCCESS) {
                return result;
            }
            currentLength += 1;
            
            while (currentEntry < codebookEntries) {
                dr_vorbis_uint32 number;
                dr_vorbis_uint32 iEntry;

                result = dr_vorbis_stream_read_bits(pStream, dr_vorbis_ilog_u(codebookEntries - currentEntry), &number);
                if (result != DR_VORBIS_SUCCESS) {
                    return result;
                }

                for (iEntry = currentEntry; iEntry < currentEntry + number; iEntry += 1) {
                    /* TODO: Assign entry. */
                    /*pCodebookCodewordLengths[iEntry] = currentLength;*/
                }

                currentEntry += number;

                /* Validation. */
                if (currentEntry > codebookEntries) {
                    return EILSEQ;  /* Error. */
                }
            }
        }

        /* Vector lookup table. */
        result = dr_vorbis_stream_read_bits(pStream, 4, &codebookLookupType);
        if (result != DR_VORBIS_SUCCESS) {
            return result;
        }

        if (codebookLookupType == 0) {
            /* Do nothing. */
        } else if (codebookLookupType == 1 || codebookLookupType == 2) {
            dr_vorbis_uint32 codebookMinimumValueRaw;
            dr_vorbis_uint32 codebookMaximumValueRaw;
            float codebookMinimumValue;
            float codebookMaximumValue;
            dr_vorbis_uint32 valueBits;
            dr_vorbis_uint32 sequenceP;
            dr_vorbis_uint32 lookupValues;
            dr_vorbis_uint32 iValue;

            result = dr_vorbis_stream_read_bits(pStream, 32, &codebookMinimumValueRaw);
            if (result != DR_VORBIS_SUCCESS) {
                /* TODO: Free memory. */
                return result;
            }

            result = dr_vorbis_stream_read_bits(pStream, 32, &codebookMaximumValueRaw);
            if (result != DR_VORBIS_SUCCESS) {
                /* TODO: Free memory. */
                return result;
            }

            codebookMinimumValue = dr_vorbis_float32_unpack(codebookMinimumValueRaw);
            codebookMaximumValue = dr_vorbis_float32_unpack(codebookMaximumValueRaw);

            result = dr_vorbis_stream_read_bits(pStream, 4, &valueBits);
            if (result != DR_VORBIS_SUCCESS) {
                /* TODO: Free memory. */
                return result;
            }
            valueBits += 1;

            result = dr_vorbis_stream_read_bits(pStream, 1, &sequenceP);
            if (result != DR_VORBIS_SUCCESS) {
                /* TODO: Free memory. */
                return result;
            }

            if (codebookLookupType == 1) {
                lookupValues = dr_vorbis_lookup1_values(codebookEntries, codebookDimensions);
            } else {
                lookupValues = codebookEntries * codebookDimensions;
            }

            for (iValue = 0; iValue < lookupValues; iValue += 1) {
                dr_vorbis_uint32 value;

                result = dr_vorbis_stream_read_bits(pStream, valueBits, &value);
                if (result != DR_VORBIS_SUCCESS) {
                    /* TODO: Free memory. */
                    return result;
                }

                /* TODO: Assign value. */
                /*pCookbookMultiplicands[iValue] = (dr_vorbis_uint16)value;*/
            }
        } else {
            return EILSEQ;  /* Unsupported lookup type. */
        }
    }

    return 0;
}

static int dr_vorbis_stream_load_setup_header(dr_vorbis_stream* pStream)
{
    int result;
    dr_vorbis_uint8 framingBit;

    DR_VORBIS_ASSERT(pStream != NULL);

    /* Common header. */
    result = dr_vorbis_stream_read_common_header(pStream, DR_VORBIS_PACKET_TYPE_SETUP);
    if (result != 0) {
        return result;
    }

    /* Setup-specific data. */

    /* Codebooks. */
    result = dr_vorbis_stream_load_setup_header_codebooks(pStream);
    if (result != 0) {
        return result;
    }

    /* Time domain transforms. Not used, but needs to be skipped over. */

    /* Floors. */

    /* Residues. */

    /* Mappings. */

    /* Modes. */

    /* Framing bit. */
    result = dr_vorbis_stream_read_uint8(pStream, &framingBit);
    if (result != 0) {
        /* TODO: Need to free memory from steps above. */
        return result;
    }

    if ((framingBit & 0x01) == 0) {
        /* TODO: Need to free memory from steps above. */
        return EILSEQ;  /* Framing bit must be set. */
    }

    return 0;
}

static int dr_vorbis_stream_load_headers(dr_vorbis_stream* pStream)
{
    int result;

    /* Note that we're not doing any kind of CRC recovery here. If anything in the header is corrupt, the entire stream is corrupt. */

    /* Identification. */
    result = dr_vorbis_stream_load_identification_header(pStream);
    if (result != 0) {
        return result;
    }

    /* Comment. */
    result = dr_vorbis_stream_load_comment_header(pStream);
    if (result != 0) {
        return result;
    }

    /* Setup. */
    result = dr_vorbis_stream_load_setup_header(pStream);
    if (result != 0) {
        return result;
    }

    /* Done. */
    return 0;
}

DR_VORBIS_API int dr_vorbis_stream_init(void* pReadUserData, dr_vorbis_read_data_proc onRead, void* pMetaUserData, dr_vorbis_meta_data_proc onMeta, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis_stream* pStream)
{
    dr_vorbis_result result;

    if (pStream == NULL) {
        return EINVAL;
    }

    DR_VORBIS_ZERO_OBJECT(pStream);

    if (onRead == NULL) {
        return EINVAL;  /* Must have a read callback or else there's no way to read data. */
    }

    result = dr_vorbis_bs_init(pReadUserData, onRead, &pStream->bs);
    if (result != DR_VORBIS_SUCCESS) {
        return result;  /* Failed to initialize bitstream. */
    }

    pStream->pMetaUserData       = pMetaUserData;
    pStream->onMeta              = onMeta;
    pStream->allocationCallbacks = dr_vorbis_allocation_callbacks_init_copy_or_default(pAllocationCallbacks);

    /* Headers are loaded first, and always at initialization time. */
    result = dr_vorbis_stream_load_headers(pStream);
    if (result != 0) {
        return result;
    }

    /* From here on out it's just audio data. This is loaded on demand. */

    return 0;
}

DR_VORBIS_API int dr_vorbis_stream_uninit(dr_vorbis_stream* pStream)
{
    if (pStream == NULL) {
        return EINVAL;
    }

    /* TODO: Implement me. */
    return 0;
}

DR_VORBIS_API int dr_vorbis_stream_reset(dr_vorbis_stream* pStream)
{
    if (pStream == NULL) {
        return EINVAL;
    }

    /* TODO: Implement me. */
    return 0;
}

DR_VORBIS_API int dr_vorbis_stream_read_pcm_frames_s16(dr_vorbis_stream* pStream, dr_vorbis_int16* pFramesOut, dr_vorbis_uint64 frameCount, dr_vorbis_uint64* pFramesRead)
{
    if (pStream == NULL || pFramesOut == NULL) {
        return EINVAL;
    }

    /* TODO: Implement me. */
    return 0;
}

DR_VORBIS_API int dr_vorbis_stream_read_pcm_frames_f32(dr_vorbis_stream* pStream, float* pFramesOut, dr_vorbis_uint64 frameCount, dr_vorbis_uint64* pFramesRead)
{
    if (pStream == NULL || pFramesOut == NULL) {
        return EINVAL;
    }

    /* TODO: Implement me. */
    return 0;
}



/************************************************************************************************************************************************************

Generic Container API

************************************************************************************************************************************************************/
DR_VORBIS_API int dr_vorbis_container_read_vorbis_data(dr_vorbis_container* pContainer, void* pOutput, size_t bytesToRead, size_t* pBytesRead)
{
    dr_vorbis_container_callbacks* pCallbacks = (dr_vorbis_container_callbacks*)pContainer;

    if (pContainer == NULL || pOutput == NULL) {
        return EINVAL;
    }

    return pCallbacks->onRead(pCallbacks->pUserData, pOutput, bytesToRead, pBytesRead);
}


/************************************************************************************************************************************************************

Ogg/Vorbis Container API

************************************************************************************************************************************************************/
#define DR_VORBIS_OGG_CAPTURE_PATTERN_CRC32 1605413199  /* CRC-32 of "OggS". */

static dr_vorbis_uint32 dr_vorbis_ogg__crc32_table[] = {
    0x00000000L, 0x04C11DB7L, 0x09823B6EL, 0x0D4326D9L,
    0x130476DCL, 0x17C56B6BL, 0x1A864DB2L, 0x1E475005L,
    0x2608EDB8L, 0x22C9F00FL, 0x2F8AD6D6L, 0x2B4BCB61L,
    0x350C9B64L, 0x31CD86D3L, 0x3C8EA00AL, 0x384FBDBDL,
    0x4C11DB70L, 0x48D0C6C7L, 0x4593E01EL, 0x4152FDA9L,
    0x5F15ADACL, 0x5BD4B01BL, 0x569796C2L, 0x52568B75L,
    0x6A1936C8L, 0x6ED82B7FL, 0x639B0DA6L, 0x675A1011L,
    0x791D4014L, 0x7DDC5DA3L, 0x709F7B7AL, 0x745E66CDL,
    0x9823B6E0L, 0x9CE2AB57L, 0x91A18D8EL, 0x95609039L,
    0x8B27C03CL, 0x8FE6DD8BL, 0x82A5FB52L, 0x8664E6E5L,
    0xBE2B5B58L, 0xBAEA46EFL, 0xB7A96036L, 0xB3687D81L,
    0xAD2F2D84L, 0xA9EE3033L, 0xA4AD16EAL, 0xA06C0B5DL,
    0xD4326D90L, 0xD0F37027L, 0xDDB056FEL, 0xD9714B49L,
    0xC7361B4CL, 0xC3F706FBL, 0xCEB42022L, 0xCA753D95L,
    0xF23A8028L, 0xF6FB9D9FL, 0xFBB8BB46L, 0xFF79A6F1L,
    0xE13EF6F4L, 0xE5FFEB43L, 0xE8BCCD9AL, 0xEC7DD02DL,
    0x34867077L, 0x30476DC0L, 0x3D044B19L, 0x39C556AEL,
    0x278206ABL, 0x23431B1CL, 0x2E003DC5L, 0x2AC12072L,
    0x128E9DCFL, 0x164F8078L, 0x1B0CA6A1L, 0x1FCDBB16L,
    0x018AEB13L, 0x054BF6A4L, 0x0808D07DL, 0x0CC9CDCAL,
    0x7897AB07L, 0x7C56B6B0L, 0x71159069L, 0x75D48DDEL,
    0x6B93DDDBL, 0x6F52C06CL, 0x6211E6B5L, 0x66D0FB02L,
    0x5E9F46BFL, 0x5A5E5B08L, 0x571D7DD1L, 0x53DC6066L,
    0x4D9B3063L, 0x495A2DD4L, 0x44190B0DL, 0x40D816BAL,
    0xACA5C697L, 0xA864DB20L, 0xA527FDF9L, 0xA1E6E04EL,
    0xBFA1B04BL, 0xBB60ADFCL, 0xB6238B25L, 0xB2E29692L,
    0x8AAD2B2FL, 0x8E6C3698L, 0x832F1041L, 0x87EE0DF6L,
    0x99A95DF3L, 0x9D684044L, 0x902B669DL, 0x94EA7B2AL,
    0xE0B41DE7L, 0xE4750050L, 0xE9362689L, 0xEDF73B3EL,
    0xF3B06B3BL, 0xF771768CL, 0xFA325055L, 0xFEF34DE2L,
    0xC6BCF05FL, 0xC27DEDE8L, 0xCF3ECB31L, 0xCBFFD686L,
    0xD5B88683L, 0xD1799B34L, 0xDC3ABDEDL, 0xD8FBA05AL,
    0x690CE0EEL, 0x6DCDFD59L, 0x608EDB80L, 0x644FC637L,
    0x7A089632L, 0x7EC98B85L, 0x738AAD5CL, 0x774BB0EBL,
    0x4F040D56L, 0x4BC510E1L, 0x46863638L, 0x42472B8FL,
    0x5C007B8AL, 0x58C1663DL, 0x558240E4L, 0x51435D53L,
    0x251D3B9EL, 0x21DC2629L, 0x2C9F00F0L, 0x285E1D47L,
    0x36194D42L, 0x32D850F5L, 0x3F9B762CL, 0x3B5A6B9BL,
    0x0315D626L, 0x07D4CB91L, 0x0A97ED48L, 0x0E56F0FFL,
    0x1011A0FAL, 0x14D0BD4DL, 0x19939B94L, 0x1D528623L,
    0xF12F560EL, 0xF5EE4BB9L, 0xF8AD6D60L, 0xFC6C70D7L,
    0xE22B20D2L, 0xE6EA3D65L, 0xEBA91BBCL, 0xEF68060BL,
    0xD727BBB6L, 0xD3E6A601L, 0xDEA580D8L, 0xDA649D6FL,
    0xC423CD6AL, 0xC0E2D0DDL, 0xCDA1F604L, 0xC960EBB3L,
    0xBD3E8D7EL, 0xB9FF90C9L, 0xB4BCB610L, 0xB07DABA7L,
    0xAE3AFBA2L, 0xAAFBE615L, 0xA7B8C0CCL, 0xA379DD7BL,
    0x9B3660C6L, 0x9FF77D71L, 0x92B45BA8L, 0x9675461FL,
    0x8832161AL, 0x8CF30BADL, 0x81B02D74L, 0x857130C3L,
    0x5D8A9099L, 0x594B8D2EL, 0x5408ABF7L, 0x50C9B640L,
    0x4E8EE645L, 0x4A4FFBF2L, 0x470CDD2BL, 0x43CDC09CL,
    0x7B827D21L, 0x7F436096L, 0x7200464FL, 0x76C15BF8L,
    0x68860BFDL, 0x6C47164AL, 0x61043093L, 0x65C52D24L,
    0x119B4BE9L, 0x155A565EL, 0x18197087L, 0x1CD86D30L,
    0x029F3D35L, 0x065E2082L, 0x0B1D065BL, 0x0FDC1BECL,
    0x3793A651L, 0x3352BBE6L, 0x3E119D3FL, 0x3AD08088L,
    0x2497D08DL, 0x2056CD3AL, 0x2D15EBE3L, 0x29D4F654L,
    0xC5A92679L, 0xC1683BCEL, 0xCC2B1D17L, 0xC8EA00A0L,
    0xD6AD50A5L, 0xD26C4D12L, 0xDF2F6BCBL, 0xDBEE767CL,
    0xE3A1CBC1L, 0xE760D676L, 0xEA23F0AFL, 0xEEE2ED18L,
    0xF0A5BD1DL, 0xF464A0AAL, 0xF9278673L, 0xFDE69BC4L,
    0x89B8FD09L, 0x8D79E0BEL, 0x803AC667L, 0x84FBDBD0L,
    0x9ABC8BD5L, 0x9E7D9662L, 0x933EB0BBL, 0x97FFAD0CL,
    0xAFB010B1L, 0xAB710D06L, 0xA6322BDFL, 0xA2F33668L,
    0xBCB4666DL, 0xB8757BDAL, 0xB5365D03L, 0xB1F740B4L
};

static DR_VORBIS_INLINE dr_vorbis_uint32 dr_vorbis_ogg_crc32_byte(dr_vorbis_uint32 crc32, dr_vorbis_uint8 data)
{
    return (crc32 << 8) ^ dr_vorbis_ogg__crc32_table[(dr_vorbis_uint8)((crc32 >> 24) & 0xFF) ^ data];
}

#if 0
static DR_VORBIS_INLINE dr_vorbis_uint32 dr_vorbis_ogg_crc32_uint32(dr_vorbis_uint32 crc32, dr_vorbis_uint32 data)
{
    crc32 = dr_vorbis_crc32_byte(crc32, (dr_vorbis_uint8)((data >> 24) & 0xFF));
    crc32 = dr_vorbis_crc32_byte(crc32, (dr_vorbis_uint8)((data >> 16) & 0xFF));
    crc32 = dr_vorbis_crc32_byte(crc32, (dr_vorbis_uint8)((data >>  8) & 0xFF));
    crc32 = dr_vorbis_crc32_byte(crc32, (dr_vorbis_uint8)((data >>  0) & 0xFF));
    return crc32;
}

static DR_VORBIS_INLINE dr_vorbis_uint32 dr_vorbis_ogg_crc32_uint64(dr_vorbis_uint32 crc32, dr_vorbis_uint64 data)
{
    crc32 = dr_vorbis_crc32_uint32(crc32, (dr_vorbis_uint32)((data >> 32) & 0xFFFFFFFF));
    crc32 = dr_vorbis_crc32_uint32(crc32, (dr_vorbis_uint32)((data >>  0) & 0xFFFFFFFF));
    return crc32;
}
#endif

static DR_VORBIS_INLINE dr_vorbis_uint32 dr_vorbis_ogg_crc32_buffer(dr_vorbis_uint32 crc32, dr_vorbis_uint8* pData, size_t dataSize)
{
    /* This can be optimized. */
    size_t i;
    for (i = 0; i < dataSize; ++i) {
        crc32 = dr_vorbis_ogg_crc32_byte(crc32, pData[i]);
    }
    return crc32;
}


static int dr_vorbis_ogg_cb__on_read_vorbis_data(void* pUserData, void* pOutput, size_t bytesToRead, size_t* pBytesRead)
{
    return dr_vorbis_ogg_read_vorbis_data((dr_vorbis_ogg*)pUserData, pOutput, bytesToRead, pBytesRead);
}

static int dr_vorbis_ogg_read(dr_vorbis_ogg* pOgg, void* pOutput, size_t bytesToRead, size_t* pBytesRead, dr_vorbis_uint32* pCRC)
{
    int result;
    size_t bytesRead;

    result = pOgg->onRead(pOgg->pUserData, pOutput, bytesToRead, &bytesRead);

    if (pBytesRead != NULL) {
        *pBytesRead = bytesRead;
    }

    if (result != 0) {
        return result;
    }

    if (pCRC != NULL) {
        *pCRC = dr_vorbis_ogg_crc32_buffer(*pCRC, pOutput, bytesRead);
    }

    return 0;
}

static int dr_vorbis_ogg_seek(dr_vorbis_ogg* pOgg, dr_vorbis_int64 offset, dr_vorbis_seek_origin origin)
{
    int result;

    result = pOgg->onSeek(pOgg->pUserData, offset, origin);
    if (result != 0) {
        return result;
    }

    return 0;
}

static dr_vorbis_bool32 dr_vorbis_ogg__is_capture_pattern(dr_vorbis_uint8 pattern[4])
{
    return pattern[0] == 'O' && pattern[1] == 'g' && pattern[2] == 'g' && pattern[3] == 'S';
}

static dr_vorbis_uint16 dr_vorbis_ogg__calculate_page_body_size(dr_vorbis_ogg_page_header* pHeader)
{
    dr_vorbis_uint32 pageBodySize;
    dr_vorbis_uint8 i;

    pageBodySize = 0;
    for (i = 0; i < pHeader->segmentCount; ++i) {
        pageBodySize += pHeader->segmentTable[i];
    }

    DR_VORBIS_ASSERT(pageBodySize <= DR_VORBIS_OGG_MAX_PAGE_SIZE);
    return (dr_vorbis_uint16)pageBodySize;
}

static int dr_vorbis_ogg__read_page_header_after_capture_pattern(dr_vorbis_ogg* pOgg, dr_vorbis_ogg_page_header* pHeader, dr_vorbis_uint32* pCRC)
{
    int result;
    size_t bytesRead;
    dr_vorbis_uint8 data[23];   /* Page header data after the catpure pattern, but before the segment table. */

    DR_VORBIS_ASSERT(pOgg    != NULL);
    DR_VORBIS_ASSERT(pHeader != NULL);
    DR_VORBIS_ASSERT(pCRC    != NULL);

    result = dr_vorbis_ogg_read(pOgg, data, sizeof(data), &bytesRead, /*pCRC*/NULL);    /* Note: Intentionally setting the pCRC parameter to NULL here. See below. */
    if (result != 0) {
        return result;  /* Failed to read data. */
    }

    if (bytesRead != sizeof(data)) {
        return EILSEQ;  /* Premature EOF. */
    }

    pHeader->structureVersion = data[0];
    pHeader->headerType       = data[1];
    DR_VORBIS_COPY_MEMORY(&pHeader->granulePosition, &data[ 2], 8);
    DR_VORBIS_COPY_MEMORY(&pHeader->serialNumber,    &data[10], 4);
    DR_VORBIS_COPY_MEMORY(&pHeader->sequenceNumber,  &data[14], 4);
    DR_VORBIS_COPY_MEMORY(&pHeader->checksum,        &data[18], 4);
    pHeader->segmentCount     = data[22];


    /* Note that before we can update the CRC for this part of the header, we need to set the bytes for the CRC portion to 0. */
    data[18] = 0;
    data[19] = 0;
    data[20] = 0;
    data[21] = 0;
    *pCRC = dr_vorbis_ogg_crc32_buffer(*pCRC, data, sizeof(data));
    

    /* The only thing remaining now is the segment table. */
    result = dr_vorbis_ogg_read(pOgg, pHeader->segmentTable, pHeader->segmentCount, &bytesRead, pCRC);
    if (result != 0) {
        return result;  /* Failed to read data. */
    }

    if (bytesRead != pHeader->segmentCount) {
        return EILSEQ;  /* Premature EOF. */
    }

    return 0;
}

static int dr_vorbis_ogg__read_page_header(dr_vorbis_ogg* pOgg, dr_vorbis_ogg_page_header* pHeader, dr_vorbis_uint32* pCRC)
{
    int result;
    size_t bytesRead;

    DR_VORBIS_ASSERT(pOgg    != NULL);
    DR_VORBIS_ASSERT(pHeader != NULL);
    DR_VORBIS_ASSERT(pCRC    != NULL);

    /* The CRC is on a per-page basis. Need to make sure it's reset at the start of a new page. */
    *pCRC = 0;

    /* Capture pattern is always first. */
    result = dr_vorbis_ogg_read(pOgg, pHeader->capturePattern, 4, &bytesRead, pCRC);
    if (result != 0) {
        return result;  /* Failed to read data. */
    }

    if (bytesRead != 4) {
        return EILSEQ;  /* Premature EOF. */
    }

    if (!dr_vorbis_ogg__is_capture_pattern(pHeader->capturePattern)) {
        return EILSEQ;  /* Not an Ogg page. */
    }


    /*
    At this point we have the capture pattern which means it's most likely an Ogg page. We'll now continue reading the rest of the header. We won't know
    if it's a valid page until the entire page has been read and compared against the CRC. For this purpose of this function, knowing that it *looks*
    like an Ogg page is enough.
    */
    result = dr_vorbis_ogg__read_page_header_after_capture_pattern(pOgg, pHeader, pCRC);
    if (result != 0) {
        return result;
    }


    /* Done. Getting here means we were successful, however it may still not be a valid Ogg page - that will be checked against the CRC at a higher level. */
    return 0;
}

static int dr_vorbis_ogg__goto_and_read_next_page_header(dr_vorbis_ogg* pOgg, dr_vorbis_ogg_page_header* pHeader, dr_vorbis_uint32* pCRC)
{
    int result;
    size_t bytesRead;

    DR_VORBIS_ASSERT(pOgg    != NULL);
    DR_VORBIS_ASSERT(pHeader != NULL);
    DR_VORBIS_ASSERT(pCRC    != NULL);

    result = dr_vorbis_ogg_read(pOgg, pHeader->capturePattern, 4, &bytesRead, NULL);
    if (result != 0) {
        return result;  /* Failed to read data. */
    }

    if (bytesRead != 4) {
        return EILSEQ;  /* Premature EOF. */
    }

    for (;;) {
        if (dr_vorbis_ogg__is_capture_pattern(pHeader->capturePattern)) {
            *pCRC = DR_VORBIS_OGG_CAPTURE_PATTERN_CRC32;

            /* We found the capture pattern. Try reading the rest of the page header. */
            result = dr_vorbis_ogg__read_page_header_after_capture_pattern(pOgg, pHeader, pCRC);
            if (result != 0) {
                return result;
            }

            return 0;
        } else {
            /* The first 4 bytes did not equal the capture pattern. Read the next byte and try again. */
            pHeader->capturePattern[0] = pHeader->capturePattern[1];
            pHeader->capturePattern[1] = pHeader->capturePattern[2];
            pHeader->capturePattern[2] = pHeader->capturePattern[3];
            result = dr_vorbis_ogg_read(pOgg, &pHeader->capturePattern[3], 1, &bytesRead, NULL);
            if (result != 0) {
                return result;  /* Failed to read data. */
            }

            if (bytesRead != 1) {
                return EILSEQ;  /* Premature EOF. */
            }
        }
    }
}

static int dr_vorbis_ogg__goto_and_read_next_vorbis_page_header(dr_vorbis_ogg* pOgg, dr_vorbis_ogg_page_header* pHeader, dr_vorbis_uint32* pCRC)
{
    DR_VORBIS_ASSERT(pOgg    != NULL);
    DR_VORBIS_ASSERT(pHeader != NULL);
    DR_VORBIS_ASSERT(pCRC    != NULL);

    for (;;) {
        int result = dr_vorbis_ogg__goto_and_read_next_page_header(pOgg, pHeader, pCRC);
        if (result != 0) {
            return result;  /* Could not find the next page. Probably at the end. */
        }

        if (pHeader->serialNumber == pOgg->vorbisSerialNumber) {
            return 0;
        }
    }

    /* Will not get here. */
}

DR_VORBIS_API int dr_vorbis_ogg_init(void* pUserData, dr_vorbis_read_data_proc onRead, dr_vorbis_seek_data_proc onSeek, dr_vorbis_ogg* pOgg)
{
    int result;
    size_t bytesRead;
    dr_vorbis_ogg_page_header pageHeader;
    dr_vorbis_uint32 crc32;

    if (pOgg == NULL) {
        return EINVAL;
    }

    DR_VORBIS_ZERO_OBJECT(pOgg);

    if (onRead == NULL || onSeek == NULL) {
        return EINVAL;
    }

    pOgg->cb.onRead    = dr_vorbis_ogg_cb__on_read_vorbis_data;
    pOgg->cb.pUserData = pOgg;
    pOgg->pUserData    = pUserData;
    pOgg->onRead       = onRead;
    pOgg->onSeek       = onSeek;

    /*
    We need to go to the first Vorbis page. This is where we identify the serial number of Vorbis stream. Once this is set up we can use the
    dr_vorbis_ogg__next_page() API. Note that we need to run this in a loop in order to support multiplexed streams.
    */
    for (;;) {
        dr_vorbis_uint32 pageBodySize;

        result = dr_vorbis_ogg__read_page_header(pOgg, &pageHeader, &crc32);
        if (result != 0) {
            return result;
        }

        /* If the page is not a begging-of-stream page we need to abort. */
        if ((pageHeader.headerType & 0x02) == 0) {
            return EINVAL;  /* Could not find a Vorbis stream. */
        }

        /* We have an Ogg page. We now need to check if this page refers to the Vorbis stream. If not, we just continue and try the next page. */
        pageBodySize = dr_vorbis_ogg__calculate_page_body_size(&pageHeader);
        if (pageBodySize == DR_VORBIS_IDENTIFICATION_HEADER_SIZE) {
            /*
            Increasingly more likely to be a Vorbis stream. We're going to read the entire identification header and validate it against the CRC just to be
            sure. We *could* instead just read and compare the common header bytes (the first 7 bytes), but I don't think it's a big deal to just check the
            whole 58 bytes real quick.
            */
            dr_vorbis_uint8 headerData[DR_VORBIS_IDENTIFICATION_HEADER_SIZE];

            result = dr_vorbis_ogg_read(pOgg, headerData, DR_VORBIS_IDENTIFICATION_HEADER_SIZE, &bytesRead, &crc32);
            if (result != 0) {
                return result;  /* Failed to read header data. Something wrong with the underlying data. */
            }

            if (bytesRead != DR_VORBIS_IDENTIFICATION_HEADER_SIZE) {
                return EILSEQ;  /* Premature EOF. */
            }

            /* The identification header should start with a byte containing "1". */
            if (headerData[0] == 1) {
                /* The next 6 bytes should be "vorbis". */
                if (headerData[1] == 'v' && headerData[2] == 'o' && headerData[3] == 'r' && headerData[4] == 'b' && headerData[5] == 'i' && headerData[6] == 's') {
                    /*
                    Getting here means this page header is almost certainly a Vorbis identification header. From this point on, if anything is invalid we
                    abort rather than skip. Validation of Vorbis-specific data will be done by the dr_vorbis_stream API.
                    */

                    /* CRC. */
                    if (pageHeader.checksum != crc32) {
                        return DR_VORBIS_ECRC;  /* Checksum failed. Abort. */
                    }

                    /* Getting here means we're valid. We need to make sure the serial number is set so we can easily identify Vorbis pages in a multiplexed stream. */
                    pOgg->vorbisSerialNumber = pageHeader.serialNumber;

                    /*
                    When CRC is disabled we read straight from the Ogg page without an intermediary buffer. When CRC is enabled, we need to store the data into
                    an intermediary buffer so we can perform a CRC check *before* returning the data. Since we've already read the data, we'll need to either
                    seek back to the start of the identification header, or copy the data into the intermediary buffer depending on whether or not CRC is enabled.
                    Not doing this will result in the dr_vorbis_stream API not recieving the identification header.
                    */
                    pOgg->pageDataRead = 0;
                    pOgg->pageDataSize = DR_VORBIS_IDENTIFICATION_HEADER_SIZE;
                #ifndef DR_VORBIS_NO_CRC
                    /* CRC is enabled. Copy the data into our intermediary buffer. */
                    DR_VORBIS_COPY_MEMORY(pOgg->pageData, headerData, DR_VORBIS_IDENTIFICATION_HEADER_SIZE);
                #else
                    /* CRC is disabled. Seek back to the start of the identification header. */
                    result = dr_vorbis_ogg_seek(pOgg, -DR_VORBIS_IDENTIFICATION_HEADER_SIZE, dr_vorbis_seek_origin_current);
                    if (result != 0) {
                        return result;  /* Failed to seek back to the start of the identification header. */
                    }
                #endif

                    /*
                    Success. There's still a chance the Vorbis stream is invalid, but that's now the responsibility of the dr_vorbis_stream API. The dr_vorbis_ogg
                    API is only concerned about extracting the Vorbis stream from the container so it can be passed to dr_vorbis_stream as input.
                    */
                    return 0;
                } else {
                    continue;   /* Not a Vorbis header. Skip. */
                }
            } else {
                continue;       /* Not a Vorbis identification header. Skip. */
            }
        } else {
            /* Not a Vorbis stream. Skip. */
            result = dr_vorbis_ogg_seek(pOgg, pageBodySize, dr_vorbis_seek_origin_current);
            if (result != 0) {
                return result;
            }
        }
    }
}

DR_VORBIS_API int dr_vorbis_ogg_uninit(dr_vorbis_ogg* pOgg)
{
    if (pOgg == NULL) {
        return EINVAL;
    }

    return 0;
}

DR_VORBIS_API int dr_vorbis_ogg_read_vorbis_data(dr_vorbis_ogg* pOgg, void* pOutput, size_t bytesToRead, size_t* pBytesRead)
{
    int result = 0;
    size_t totalBytesRead;
    size_t bytesReadFromPage;

    if (pOgg == NULL || pOutput == NULL) {
        return EINVAL;
    }

    /*
    Where we read data from is different depending on whether or not CRC is enabled. If CRC is enabled we need to read
    the *entire* page before we return anything which means we'll need to store it in an intermediary buffer. If CRC is
    disabled we can avoid this overhead and read straight from the backing data and into the output buffer.
    */

    /* We may be requesting more data than is available in the current page which means we need to run this in a loop. */
    totalBytesRead = 0;
    while (totalBytesRead < bytesToRead) {
        size_t bytesRemainingToRead = bytesToRead - totalBytesRead;
        size_t bytesRemainingInPage = pOgg->pageDataSize - pOgg->pageDataRead;
        size_t bytesToReadFromPage = bytesRemainingToRead;
        if (bytesToReadFromPage > bytesRemainingInPage) {
            bytesToReadFromPage = bytesRemainingInPage;
        }

        DR_VORBIS_ASSERT(bytesToReadFromPage <= DR_VORBIS_OGG_MAX_PAGE_SIZE);

        /* If there's no more data in the page we'll need to move to the next. */
        if (bytesRemainingInPage > 0) {
            /* There's more data in the current page. */
        #ifndef DR_VORBIS_NO_CRC
            /* CRC is enabled. Read from the intermediary buffer. */
            DR_VORBIS_COPY_MEMORY(DR_VORBIS_OFFSET_PTR(pOutput, totalBytesRead), DR_VORBIS_OFFSET_PTR(pOgg->pageData, pOgg->pageDataRead), bytesToReadFromPage);
            totalBytesRead     +=                   bytesToReadFromPage;
            pOgg->pageDataRead += (dr_vorbis_uint16)bytesToReadFromPage;
        #else
            /* CRC is disabled. Read straight from the backing data. */
            result = dr_vorbis_ogg_read(pOgg, DR_VORBIS_OFFSET_PTR(pOutput, totalBytesRead), bytesToReadFromPage, &bytesReadFromPage, NULL);
            totalBytesRead     += bytesReadFromPage;
            pOgg->pageDataRead += bytesReadFromPage;

            if (result != 0) {
                break;  /* Something went wrong with reading from the page. We need to abort. */
            }

            if (bytesReadFromPage != bytesToReadFromPage) {
                result = EILSEQ;
                break;  /* Premature EOF. */
            }
        #endif
        } else {
            /* There's no more data in the page. Move to the next one. */
            dr_vorbis_ogg_page_header pageHeader;
            dr_vorbis_uint32 crc32;
            dr_vorbis_uint16 pageDataSize;

            result = dr_vorbis_ogg__goto_and_read_next_vorbis_page_header(pOgg, &pageHeader, &crc32);
            if (result != 0) {
                break;
            }

            pageDataSize = dr_vorbis_ogg__calculate_page_body_size(&pageHeader);
            DR_VORBIS_ASSERT(pageDataSize <= DR_VORBIS_OGG_MAX_PAGE_SIZE);

            pOgg->pageDataRead = 0; /* Fresh page. No data has been read yet, so reset to 0. */
            pOgg->pageDataSize = 0; /* Set to 0 by default in case of an error, but later set to it's true value once everything completes successfully. */

            /*
            At this point the page data will not yet have been read. What we do from here depends on whether or not CRC is enabled. If so, we need
            to read the entire page into an intermediary buffer and then do a CRC check. If the checksums do not match, we need to discard the page
            and return an error so that the caller can know whether or not they themselves need to discard any data and perform any kind of recovery
            operation.

            When CRC is disabled, we don't need to do anything more as we'll just read data straight from the source without any kind of
            intermediary data movement.
            */
        #ifndef DR_VORBIS_NO_CRC
            /* CRC is enabled. Read into the intermediary buffer and do a CRC check. If the CRC check fails, the page needs to be discarded. */
            result = dr_vorbis_ogg_read(pOgg, pOgg->pageData, pageDataSize, &bytesReadFromPage, &crc32);
            if (result != 0) {
                break;              /* There was an error reading data page. */
            }

            if (bytesReadFromPage != pageDataSize) {
                result = EILSEQ;    /* Premature EOF. */
                break;
            }

            if (crc32 != pageHeader.checksum) {
                result = DR_VORBIS_ECRC;    /* CRC failed. */
                break;
            }
        #else
            /* CRC is disabled. No need to do anything here as we'll just read straight from the backing data. */
        #endif

            pOgg->pageDataSize = pageDataSize;
        }
    }
    
    if (pBytesRead != NULL) {
        *pBytesRead = totalBytesRead;
    }

    return result;
}



/************************************************************************************************************************************************************

Main API

************************************************************************************************************************************************************/
static int dr_vorbis_cb__stream_read_data(void* pUserData, void* pOutput, size_t bytesToRead, size_t* pBytesRead)
{
    dr_vorbis* pVorbis = (dr_vorbis*)pUserData;

    DR_VORBIS_ASSERT(pVorbis != NULL);

    /* We just read directly from the container. The container will output the raw Vorbis stream data without any container data. */
    return dr_vorbis_container_read_vorbis_data(&pVorbis->container, pOutput, bytesToRead, pBytesRead);
}

DR_VORBIS_API int dr_vorbis_init_ex(void* pReadSeekUserData, dr_vorbis_read_data_proc onRead, dr_vorbis_seek_data_proc onSeek, void* pMetaUserData, dr_vorbis_meta_data_proc onMeta, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis)
{
    int result;

    if (pVorbis == NULL) {
        return EINVAL;
    }

    DR_VORBIS_ZERO_OBJECT(pVorbis);

    if (onRead == NULL || onSeek == NULL) {
        return EINVAL;
    }

    /* We're going to use trial-and-error for the container. */
    result = EINVAL;

    /* Ogg. */
    if (result != 0) {
        result = dr_vorbis_ogg_init(pReadSeekUserData, onRead, onSeek, &pVorbis->container.ogg);
        if (result != 0) {
            onSeek(pReadSeekUserData, 0, dr_vorbis_seek_origin_start);  /* Seek back to the start for the benefit of the next attempt. */
        }
    }
    
    /* If at this point we still don't have a container we need to abort. */
    if (result != 0) {
        return EINVAL;
    }


    /*
    At this point we have a container initialized. From this point on, we must never read from the callbacks directly. For this reason, I'm setting these to NULL so that
    any attempt to use those callbacks will be caught at development time.
    */
    onRead = NULL;
    onSeek = NULL;


    /*
    Now that we have a container we need to initialize the stream. Make sure we don't bother with a metadata callback for the stream if onMeta is NULL. This causes
    metadata to be skipped for a minor optimization and some saving of memory allocations.
    */
    result = dr_vorbis_stream_init(pVorbis, dr_vorbis_cb__stream_read_data, pMetaUserData, onMeta, pAllocationCallbacks, &pVorbis->stream);
    if (result != 0) {
        return result;
    }


    return 0;
}

DR_VORBIS_API int dr_vorbis_init(void* pReadSeekUserData, dr_vorbis_read_data_proc onRead, dr_vorbis_seek_data_proc onSeek, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis)
{
    return dr_vorbis_init_ex(pReadSeekUserData, onRead, onSeek, NULL, NULL, pAllocationCallbacks, pVorbis);
}


#ifndef DR_VORBIS_NO_STDIO
static int dr_vorbis_fopen(FILE** ppFile, const char* pFilePath, const char* pOpenMode)
{
#if _MSC_VER && _MSC_VER >= 1400
    errno_t err;
#endif

    if (ppFile != NULL) {
        *ppFile = NULL;  /* Safety. */
    }

    if (pFilePath == NULL || pOpenMode == NULL || ppFile == NULL) {
        return EINVAL;
    }

#if _MSC_VER && _MSC_VER >= 1400
    err = fopen_s(ppFile, pFilePath, pOpenMode);
    if (err != 0) {
        return err;
    }
#else
#if defined(_WIN32) || defined(__APPLE__)
    *ppFile = fopen(pFilePath, pOpenMode);
#else
    #if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64 && defined(_LARGEFILE64_SOURCE)
        *ppFile = fopen64(pFilePath, pOpenMode);
    #else
        *ppFile = fopen(pFilePath, pOpenMode);
    #endif
#endif
    if (*ppFile == NULL) {
        int result = errno;
        if (result == 0) {
            result = EINVAL;   /* Just a safety check to make sure we never ever return success when pFile == NULL. */
        }

        return result;
    }
#endif

    return 0;
}

/*
_wfopen() isn't always available in all compilation environments.

    * Windows only.
    * MSVC seems to support it universally as far back as VC6 from what I can tell (haven't checked further back).
    * MinGW-64 (both 32- and 64-bit) seems to support it.
    * MinGW wraps it in !defined(__STRICT_ANSI__).

This can be reviewed as compatibility issues arise. The preference is to use _wfopen_s() and _wfopen() as opposed to the wcsrtombs()
fallback, so if you notice your compiler not detecting this properly I'm happy to look at adding support.
*/
#if defined(_WIN32)
    #if defined(_MSC_VER) || defined(__MINGW64__) || !defined(__STRICT_ANSI__)
        #define DRFLAC_HAS_WFOPEN
    #endif
#endif

static int dr_vorbis_wfopen(FILE** ppFile, const wchar_t* pFilePath, const wchar_t* pOpenMode, const dr_vorbis_allocation_callbacks* pAllocationCallbacks)
{
    if (ppFile != NULL) {
        *ppFile = NULL;  /* Safety. */
    }

    if (pFilePath == NULL || pOpenMode == NULL || ppFile == NULL) {
        return EINVAL;
    }

#if defined(DRFLAC_HAS_WFOPEN)
    {
        /* Use _wfopen() on Windows. */
    #if defined(_MSC_VER) && _MSC_VER >= 1400
        errno_t err = _wfopen_s(ppFile, pFilePath, pOpenMode);
        if (err != 0) {
            return err;
        }
    #else
        *ppFile = _wfopen(pFilePath, pOpenMode);
        if (*ppFile == NULL) {
            return errno;
        }
    #endif
        (void)pAllocationCallbacks;
    }
#else
    /*
    Use fopen() on anything other than Windows. Requires a conversion. This is annoying because fopen() is locale specific. The only real way I can
    think of to do this is with wcsrtombs(). Note that wcstombs() is apparently not thread-safe because it uses a static global mbstate_t object for
    maintaining state. I've checked this with -std=c89 and it works, but if somebody get's a compiler error I'll look into improving compatibility.
    */
    {
        mbstate_t mbs;
        size_t lenMB;
        const wchar_t* pFilePathTemp = pFilePath;
        char* pFilePathMB = NULL;
        char pOpenModeMB[32] = {0};

        /* Get the length first. */
        DR_VORBIS_ZERO_OBJECT(&mbs);
        lenMB = wcsrtombs(NULL, &pFilePathTemp, 0, &mbs);
        if (lenMB == (size_t)-1) {
            return errno;
        }

        pFilePathMB = (char*)dr_vorbis_malloc(lenMB + 1, pAllocationCallbacks);
        if (pFilePathMB == NULL) {
            return ENOMEM;
        }

        pFilePathTemp = pFilePath;
        DR_VORBIS_ZERO_OBJECT(&mbs);
        wcsrtombs(pFilePathMB, &pFilePathTemp, lenMB + 1, &mbs);

        /* The open mode should always consist of ASCII characters so we should be able to do a trivial conversion. */
        {
            size_t i = 0;
            for (;;) {
                if (pOpenMode[i] == 0) {
                    pOpenModeMB[i] = '\0';
                    break;
                }

                pOpenModeMB[i] = (char)pOpenMode[i];
                i += 1;
            }
        }

        *ppFile = fopen(pFilePathMB, pOpenModeMB);

        dr_vorbis_free(pFilePathMB, pAllocationCallbacks);
    }

    if (*ppFile == NULL) {
        return EINVAL;
    }
#endif

    return 0;
}

static int dr_vorbis_cb__on_read_stdio(void* pUserData, void* pOutput, size_t bytesToRead, size_t* pBytesRead)
{
    size_t bytesRead = fread(pOutput, 1, bytesToRead, (FILE*)pUserData);
    
    if (pBytesRead != NULL) {
        *pBytesRead = bytesRead;
    }

    /*
    If we didn't read exactly the number of bytes requested we either hit the end of the file or there was an actual error. Hitting the end of
    the file is *not* considered an error.
    */
    if (bytesRead != bytesToRead) {
        if (feof((FILE*)pUserData)) {
            return 0;   /* EOF is not an error. */
        } else {
            return ferror((FILE*)pUserData);
        }
    }

    return 0;
}

static int dr_vorbis_cb__on_seek_stdio(void* pUserData, dr_vorbis_int64 offset, dr_vorbis_seek_origin origin)
{
    int result;

    DR_VORBIS_ASSERT(pUserData != NULL);

#if defined(_WIN32)
    #if defined(_MSC_VER) && _MSC_VER > 1200
        result = _fseeki64((FILE*)pUserData, offset, origin);
    #else
        /* No _fseeki64() so restrict to 31 bits. */
        if (origin > 0x7FFFFFFF) {
            return ERANGE;
        }

        result = fseek((FILE*)pUserData, (int)offset, origin);
    #endif
#else
    result = fseek((FILE*)pUserData, (long int)offset, origin);
#endif
    if (result != 0) {
        return result;
    }

    return 0;
}
#endif

DR_VORBIS_API int dr_vorbis_init_file_ex(const char* pFilePath, void* pMetaUserData, dr_vorbis_meta_data_proc onMeta, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis)
{
#ifndef DR_VORBIS_NO_STDIO
    int result;
    FILE* pFile;

    result = dr_vorbis_fopen(&pFile, pFilePath, "rb");
    if (result != 0) {
        return result;  /* Failed to open file. */
    }

    result = dr_vorbis_init_ex(pFile, dr_vorbis_cb__on_read_stdio, dr_vorbis_cb__on_seek_stdio, pMetaUserData, onMeta, pAllocationCallbacks, pVorbis);
    if (result != 0) {
        fclose(pFile);
        return result;
    }

    return 0;
#else
    /* No stdio. */
    (void)pFilePath;
    (void)pMetaUserData;
    (void)onMeta;
    (void)pAllocationCallbacks;
    (void)pVorbis;
    return EINVAL;
#endif
}

DR_VORBIS_API int dr_vorbis_init_file_w_ex(const wchar_t* pFilePath, void* pMetaUserData, dr_vorbis_meta_data_proc onMeta, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis)
{
#ifndef DR_VORBIS_NO_STDIO
    int result;
    FILE* pFile;

    result = dr_vorbis_wfopen(&pFile, pFilePath, L"rb", pAllocationCallbacks);
    if (result != 0) {
        return result;  /* Failed to open file. */
    }

    result = dr_vorbis_init_ex(pFile, dr_vorbis_cb__on_read_stdio, dr_vorbis_cb__on_seek_stdio, pMetaUserData, onMeta, pAllocationCallbacks, pVorbis);
    if (result != 0) {
        fclose(pFile);
        return result;
    }

    return 0;
#else
    /* No stdio. */
    (void)pFilePath;
    (void)pMetaUserData;
    (void)onMeta;
    (void)pAllocationCallbacks;
    (void)pVorbis;
    return EINVAL;
#endif
}

DR_VORBIS_API int dr_vorbis_init_file(const char* pFilePath, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis)
{
    return dr_vorbis_init_file_ex(pFilePath, NULL, NULL, pAllocationCallbacks, pVorbis);
}

DR_VORBIS_API int dr_vorbis_init_file_w(const wchar_t* pFilePath, const dr_vorbis_allocation_callbacks* pAllocationCallbacks, dr_vorbis* pVorbis)
{
    return dr_vorbis_init_file_w_ex(pFilePath, NULL, NULL, pAllocationCallbacks, pVorbis);
}

DR_VORBIS_API int dr_vorbis_uninit(dr_vorbis* pVorbis)
{
    if (pVorbis == NULL) {
        return EINVAL;
    }

    dr_vorbis_stream_uninit(&pVorbis->stream);

#ifndef DR_VORBIS_NO_STDIO
    if (((dr_vorbis_container_callbacks*)&pVorbis->container)->onRead == dr_vorbis_cb__on_read_stdio) {
        fclose((FILE*)pVorbis->backend.pFile);
        pVorbis->backend.pFile = NULL;
    }
#endif

    return 0;
}

DR_VORBIS_API int dr_vorbis_read_pcm_frames_s16(dr_vorbis* pVorbis, dr_vorbis_int16* pFramesOut, dr_vorbis_uint64 frameCount, dr_vorbis_uint64* pFramesRead)
{
    if (pVorbis == NULL) {
        return EINVAL;
    }

    return dr_vorbis_stream_read_pcm_frames_s16(&pVorbis->stream, pFramesOut, frameCount, pFramesRead);
}

DR_VORBIS_API int dr_vorbis_read_pcm_frames_f32(dr_vorbis* pVorbis, float* pFramesOut, dr_vorbis_uint64 frameCount, dr_vorbis_uint64* pFramesRead)
{
    if (pVorbis == NULL) {
        return EINVAL;
    }

    return dr_vorbis_stream_read_pcm_frames_f32(&pVorbis->stream, pFramesOut, frameCount, pFramesRead);
}

DR_VORBIS_API int dr_vorbis_seek_to_pcm_frame(dr_vorbis* pVorbis, dr_vorbis_uint64 frameIndex)
{
    if (pVorbis == NULL) {
        return EINVAL;
    }

    /*
    We want to use the container to accelerate the search for the seek target. What makes seeking annoying in Vorbis is that it's possible for the target
    sample to depend on the previous frame due to Vorbis' lapping system. We'll need to seek to the prior Vorbis packet, and then read-and-discard any
    leftovers.
    */
    
    /* TODO: Implement me. */
    return 0;
}



/************************************************************************************************************************************************************

Miscellaneous APIs

************************************************************************************************************************************************************/
DR_VORBIS_API void* dr_vorbis_malloc(size_t sz, const dr_vorbis_allocation_callbacks* pAllocationCallbacks)
{
    if (pAllocationCallbacks == NULL) {
        return DR_VORBIS_MALLOC(sz);
    }

    if (pAllocationCallbacks->onMalloc == NULL) {
        return NULL;    /* No malloc() defined. Do not fall back to default implementation - let the program know about the error. */
    }

    return pAllocationCallbacks->onMalloc(sz, pAllocationCallbacks->pUserData);
}

DR_VORBIS_API void* dr_vorbis_realloc(void* p, size_t sz, const dr_vorbis_allocation_callbacks* pAllocationCallbacks)
{
    if (pAllocationCallbacks == NULL) {
        return DR_VORBIS_REALLOC(p, sz);
    }

    if (pAllocationCallbacks->onRealloc == NULL) {
        return NULL;    /* No realloc() defined. Do not fall back to default implementation - let the program know about the error. */
    }

    return pAllocationCallbacks->onRealloc(p, sz, pAllocationCallbacks->pUserData);
}

DR_VORBIS_API void dr_vorbis_free(void* p, const dr_vorbis_allocation_callbacks* pAllocationCallbacks)
{
    if (pAllocationCallbacks == NULL) {
        DR_VORBIS_FREE(p);
        return;
    }

    if (pAllocationCallbacks->onFree == NULL) {
        return;    /* No free() defined. Do not fall back to default implementation. */
    }

    pAllocationCallbacks->onFree(p, pAllocationCallbacks->pUserData);
}


#endif  /* dr_vorbis_c */
#endif  /* DR_VORBIS_IMPLEMENTATION */

/*
REVISION HISTORY
================

*/

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
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2020 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
