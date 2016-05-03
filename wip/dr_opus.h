// Public domain. See "unlicense" statement at the end of this file.

// THIS LIBRARY IS NOT COMPLETE

// ABOUT
//
// dr_opus is a simple library for decoding Opus files and streams.
//
//
//
// USAGE
//
// dr_opus is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_OPUS_IMPLEMENTATION
//   #include "dr_opus.h"
//
// You can then #include this file in other parts of the program as you would with any other header file. To decode audio data,
// do something like the following:
//
//     dropus* pOpus = dropus_open_file("MySong.opus");
//     if (pOpus == NULL) {
//         ... Failed to open Opus file ...
//     }
//
//     int32_t* pSamples = malloc(pOpus->totalSampleCount * sizeof(int32_t));
//     uint64_t numberOfSamplesActuallyRead = dropus_read_s32(pOpus, pOpus->totalSampleCount, pSamples);
//
//     ... pSamples now contains the decoded samples as interleaved signed 32-bit PCM ...
//
// The dropus object represents the decoder. It is a transparent type so all the information you need, such as the number of
// channels and the bits per sample, should be directly accessible - just make sure you don't change their values.
//
// You do not need to decode the entire stream in one go - you just specify how many samples you'd like at any given time and
// the decoder will give you as many samples as it can, up to the amount requested. Later on when you need the next batch of
// samples, just call it again. Example:
//
//     while (dropus_read_s32(pOpus, chunkSize, pChunkSamples) > 0) {
//         do_something();
//     }
//
// You can seek to a specific sample with dropus_seek_to_sample(). The given sample is based on interleaving. So for example,
// if you were to seek to the sample at index 0 in a stereo stream, you'll be seeking to the first sample of the left channel.
// The sample at index 1 will be the first sample of the right channel. The sample at index 2 will be the second sample of the
// left channel, etc.
//
//
//
// OPTIONS
// #define these options before including this file.
//
// #define DR_OPUS_NO_STDIO
//   Disable dropus_open_file().
//
// #define DR_OPUS_NO_WIN32_IO
//   Don't use the Win32 API internally for dropus_open_file(). Setting this will force stdio FILE APIs instead. This is
//   mainly for testing, but it's left here in case somebody might find use for it. dr_opus will use the Win32 API by
//   default. Ignored when DR_OPUS_NO_STDIO is #defined.
//
// #define DR_OPUS_BUFFER_SIZE <number>
//   Defines the size of the internal buffer to store data from onRead(). This buffer is used to reduce the number of calls
//   back to the client for more data. Larger values means more memory, but better performance. My tests show diminishing
//   returns after about 4KB (which is the default). Consider reducing this if you have a very efficient implementation of
//   onRead(), or increase it if it's very inefficient.


#ifndef dr_opus_h
#define dr_opus_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef DR_OPUS_BUFFER_SIZE
#define DR_OPUS_BUFFER_SIZE   4096
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Check if we can enable 64-bit optimizations.
#if defined(_WIN64)
#define DROPUS_64BIT
#endif

#if defined(__GNUC__)
#if defined(__x86_64__) || defined(__ppc64__)
#define DROPUS_64BIT
#endif
#endif

#ifdef DROPUS_64BIT
typedef uint64_t dropus_cache_t;
#else
typedef uint32_t dropus_cache_t;
#endif



// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* dropus_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

// Callback for when data needs to be seeked. Offset is always relative to the current position. Return value is false on failure, true success.
typedef bool (* dropus_seek_proc)(void* userData, int offset);


typedef struct
{
    // The function to call when more data needs to be read. This is set by dropus_open().
    dropus_read_proc onRead;

    // The function to call when the current read position needs to be moved.
    dropus_seek_proc onSeek;

    // The user data to pass around to onRead and onSeek.
    void* pUserData;


    // The sample rate. Will be set to something like 44100.
    unsigned int sampleRate;

    // The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc.
    unsigned char channels;

    // The bits per sample. Will be set to somthing like 16, 24, etc.
    unsigned char bitsPerSample;


    // The total number of samples making up the stream. This includes every channel. For example, if the stream has 2 channels,
    // with each channel having a total of 4096, this value will be set to 2*4096 = 8192.
    uint64_t totalSampleCount;


    // The current byte position in the client's data stream.
    uint64_t currentBytePos;

    // The index of the next valid cache line in the "L2" cache.
    size_t nextL2Line;

    // The number of bits that have been consumed by the cache. This is used to determine how many valid bits are remaining.
    size_t consumedBits;

    // Unused L2 lines. This will always be 0 until the end of the stream is hit. Used for correctly calculating the current byte
    // position of the read pointer in the stream.
    size_t unusedL2Lines;

    // The cached data which was most recently read from the client. When data is read from the client, it is placed within this
    // variable. As data is read, it's bit-shifted such that the next valid bit is sitting on the most significant bit.
    dropus_cache_t cache;
    dropus_cache_t cacheL2[DR_OPUS_BUFFER_SIZE/sizeof(dropus_cache_t)];

} dropus;


// dropus_open()
dropus* dropus_open(dropus_read_proc onRead, dropus_seek_proc onSeek, void* pUserData);

// dropus_close()
void dropus_close(dropus* pOpus);

// dropus_read_s32()
uint64_t dropus_read_s32(dropus* pOpus, uint64_t samplesToRead, int32_t* pBufferOut);

// Seeks to the sample at the given index.
bool dropus_seek_to_sample(dropus* pOpus, uint64_t sampleIndex);


#ifndef DR_OPUS_NO_STDIO
// Opens an Opus decoder from the file at the given path.
dropus* dropus_open_file(const char* pFile);
#endif

// Helper for opening a file from a pre-allocated memory buffer.
//
// This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
// the lifetime of the decoder.
dropus* dropus_open_memory(const void* data, size_t dataSize);

#ifdef __cplusplus
}
#endif
#endif  // dr_opus


///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DR_OPUS_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _MSC_VER
#include <intrin.h>     // For _byteswap_ulong and _byteswap_uint64
#endif

#ifdef __linux__
#define _BSD_SOURCE
#include <endian.h>
#endif

#ifdef _MSC_VER
#define DROPUS_INLINE __forceinline
#else
#define DROPUS_INLINE inline
#endif


#ifndef DR_OPUS_NO_STDIO
#if defined(DR_OPUS_NO_WIN32_IO) || !defined(_WIN32)
#include <stdio.h>

static size_t dropus__on_read_stdio(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)pUserData);
}

static bool dropus__on_seek_stdio(void* pUserData, int offset)
{
    return fseek((FILE*)pUserData, offset, SEEK_CUR) == 0;
}

dropus* dropus_open_file(const char* filename)
{
    FILE* pFile;
#ifdef _MSC_VER
    if (fopen_s(&pFile, filename, "rb") != 0) {
        return false;
    }
#else
    pFile = fopen(filename, "rb");
    if (pFile == NULL) {
        return false;
    }
#endif

    return dropus_open(dropus__on_read_stdio, dropus__on_seek_stdio, pFile);
}
#else
#include <windows.h>

static size_t dropus__on_read_stdio(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    assert(bytesToRead < 0xFFFFFFFF);   // dr_opus will never request huge amounts of data at a time. This is a safe assertion.

    DWORD bytesRead;
    ReadFile((HANDLE)pUserData, bufferOut, (DWORD)bytesToRead, &bytesRead, NULL);

    return (size_t)bytesRead;
}

static bool dropus__on_seek_stdio(void* pUserData, int offset)
{
    return SetFilePointer((HANDLE)pUserData, offset, NULL, FILE_CURRENT) != INVALID_SET_FILE_POINTER;
}

dropus* dropus_open_file(const char* filename)
{
    HANDLE hFile = CreateFileA(filename, FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    return dropus_open(dropus__on_read_stdio, dropus__on_seek_stdio, (void*)hFile);
}
#endif
#endif  // DR_OPUS_NO_STDIO


typedef struct
{
    // A pointer to the beginning of the data. We use a char as the type here for easy offsetting.
    const unsigned char* data;

    // The size of the data.
    size_t dataSize;

    // The position we're currently sitting at.
    size_t currentReadPos;

} dropus_memory;

static size_t dropus__on_read_memory(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    dropus_memory* memory = pUserData;
    assert(memory != NULL);
    assert(memory->dataSize >= memory->currentReadPos);

    size_t bytesRemaining = memory->dataSize - memory->currentReadPos;
    if (bytesToRead > bytesRemaining) {
        bytesToRead = bytesRemaining;
    }

    if (bytesToRead > 0) {
        memcpy(bufferOut, memory->data + memory->currentReadPos, bytesToRead);
        memory->currentReadPos += bytesToRead;
    }

    return bytesToRead;
}

static bool dropus__on_seek_memory(void* pUserData, int offset)
{
    dropus_memory* memory = pUserData;
    assert(memory != NULL);

    if (offset > 0) {
        if (memory->currentReadPos + offset > memory->dataSize) {
            offset = (int)(memory->dataSize - memory->currentReadPos);     // Trying to seek too far forward.
        }
    } else {
        if (memory->currentReadPos < (size_t)-offset) {
            offset = -(int)memory->currentReadPos;                  // Trying to seek too far backwards.
        }
    }

    // This will never underflow thanks to the clamps above.
    memory->currentReadPos += offset;

    return 1;
}

dropus* dropus_open_memory(const void* data, size_t dataSize)
{
    dropus_memory* pUserData = malloc(sizeof(*pUserData));
    if (pUserData == NULL) {
        return false;
    }

    pUserData->data = data;
    pUserData->dataSize = dataSize;
    pUserData->currentReadPos = 0;
    return dropus_open(dropus__on_read_memory, dropus__on_seek_memory, pUserData);
}



//// Endian Management ////
static DROPUS_INLINE bool dropus__is_little_endian()
{
    int n = 1;
    return (*(char*)&n) == 1;
}

static DROPUS_INLINE uint32_t dropus__swap_endian_uint32(uint32_t n)
{
#ifdef _MSC_VER
    return _byteswap_ulong(n);
#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC__ >= 3))
    return __builtin_bswap32(n);
#else
    return ((n & 0xFF000000) >> 24) |
           ((n & 0x00FF0000) >>  8) |
           ((n & 0x0000FF00) <<  8) |
           ((n & 0x000000FF) << 24);
#endif
}

static DROPUS_INLINE uint64_t dropus__swap_endian_uint64(uint64_t n)
{
#ifdef _MSC_VER
    return _byteswap_uint64(n);
#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC__ >= 3))
    return __builtin_bswap64(n);
#else
    return ((n & 0xFF00000000000000ULL) >> 56) |
           ((n & 0x00FF000000000000ULL) >> 40) |
           ((n & 0x0000FF0000000000ULL) >> 24) |
           ((n & 0x000000FF00000000ULL) >>  8) |
           ((n & 0x00000000FF000000ULL) <<  8) |
           ((n & 0x0000000000FF0000ULL) << 24) |
           ((n & 0x000000000000FF00ULL) << 40) |
           ((n & 0x00000000000000FFULL) << 56);
#endif
}


static DROPUS_INLINE uint32_t dropus__be2host_32(uint32_t n)
{
#ifdef __linux__
    return be32toh(n);
#else
    if (dropus__is_little_endian()) {
        return dropus__swap_endian_uint32(n);
    }

    return n;
#endif
}

static DROPUS_INLINE uint64_t dropus__be2host_64(uint64_t n)
{
#ifdef __linux__
    return be64toh(n);
#else
    if (dropus__is_little_endian()) {
        return dropus__swap_endian_uint64(n);
    }

    return n;
#endif
}

#ifdef DROPUS_64BIT
#define dropus__be2host__cache_line dropus__be2host_64
#else
#define dropus__be2host__cache_line dropus__be2host_32
#endif


// This uses a 32- or 64-bit bit-shifted cache - as bits are read, the cache is shifted such that the first valid bit is sitting
// on the most significant bit. It uses the notion of an L1 and L2 cache (borrowed from CPU architecture), where the L1 cache
// is a 32- or 64-bit unsigned integer (depending on whether or not a 32- or 64-bit build is being compiled) and the L2 is an
// array of "cache lines", with each cache line being the same size as the L1. The L2 is a buffer of about 4KB and is where data
// from onRead() is read into.
#define DROPUS_CACHE_L1_SIZE_BYTES                  (sizeof(pOpus->cache))
#define DROPUS_CACHE_L1_SIZE_BITS                   (sizeof(pOpus->cache)*8)
#define DROPUS_CACHE_L1_BITS_REMAINING              (DROPUS_CACHE_L1_SIZE_BITS - (pOpus->consumedBits))
#ifdef DROPUS_64BIT
#define DROPUS_CACHE_L1_SELECTION_MASK(_bitCount)   (~(((uint64_t)-1LL) >> (_bitCount)))
#else
#define DROPUS_CACHE_L1_SELECTION_MASK(_bitCount)   (~(((uint32_t)-1) >> (_bitCount)))
#endif
#define DROPUS_CACHE_L1_SELECTION_SHIFT(_bitCount)  (DROPUS_CACHE_L1_SIZE_BITS - (_bitCount))
#define DROPUS_CACHE_L1_SELECT(_bitCount)           ((pOpus->cache) & DROPUS_CACHE_L1_SELECTION_MASK(_bitCount))
#define DROPUS_CACHE_L1_SELECT_AND_SHIFT(_bitCount) (DROPUS_CACHE_L1_SELECT(_bitCount) >> DROPUS_CACHE_L1_SELECTION_SHIFT(_bitCount))
#define DROPUS_CACHE_L2_SIZE_BYTES                  (sizeof(pOpus->cacheL2))
#define DROPUS_CACHE_L2_LINE_COUNT                  (DROPUS_CACHE_L2_SIZE_BYTES / sizeof(pOpus->cacheL2[0]))
#define DROPUS_CACHE_L2_LINES_REMAINING             (DROPUS_CACHE_L2_LINE_COUNT - pOpus->nextL2Line)

static DROPUS_INLINE bool dropus__reload_l1_cache_from_l2(dropus* pOpus)
{
    // Fast path. Try loading straight from L2.
    if (pOpus->nextL2Line < DROPUS_CACHE_L2_LINE_COUNT) {
        pOpus->cache = pOpus->cacheL2[pOpus->nextL2Line++];
        return true;
    }

    // If we get here it means we've run out of data in the L2 cache. We'll need to fetch more from the client.
    size_t bytesRead = pOpus->onRead(pOpus->pUserData, pOpus->cacheL2, DROPUS_CACHE_L2_SIZE_BYTES);
    pOpus->currentBytePos += bytesRead;

    pOpus->nextL2Line = 0;
    if (bytesRead == DROPUS_CACHE_L2_SIZE_BYTES) {
        pOpus->cache = pOpus->cacheL2[pOpus->nextL2Line++];
        return true;
    }


    // If we get here it means we were unable to retrieve enough data to fill the entire L2 cache. It probably
    // means we've just reached the end of the file. We need to move the valid data down to the end of the buffer
    // and adjust the index of the next line accordingly. Also keep in mind that the L2 cache must be aligned to
    // the size of the L1 so we'll need to seek backwards by any misaligned bytes.
    size_t alignedL1LineCount = bytesRead / DROPUS_CACHE_L1_SIZE_BYTES;
    if (alignedL1LineCount > 0)
    {
        size_t offset = DROPUS_CACHE_L2_LINE_COUNT - alignedL1LineCount;
        for (size_t i = alignedL1LineCount; i > 0; --i) {
            pOpus->cacheL2[i-1 + offset] = pOpus->cacheL2[i-1];
        }

        pOpus->nextL2Line = offset;
        pOpus->unusedL2Lines = offset;

        // At this point there may be some leftover unaligned bytes. We need to seek backwards so we don't lose
        // those bytes.
        size_t unalignedBytes = bytesRead - (alignedL1LineCount * DROPUS_CACHE_L1_SIZE_BYTES);
        if (unalignedBytes > 0) {
            pOpus->onSeek(pOpus->pUserData, -(int)unalignedBytes);
            pOpus->currentBytePos -= unalignedBytes;
        }

        pOpus->cache = pOpus->cacheL2[pOpus->nextL2Line++];
        return true;
    }
    else
    {
        // If we get into this branch it means we weren't able to load any L1-aligned data. We just need to seek
        // backwards by the leftover bytes and return false.
        if (bytesRead > 0) {
            pOpus->onSeek(pOpus->pUserData, -(int)bytesRead);
            pOpus->currentBytePos -= bytesRead;
        }

        pOpus->nextL2Line = DROPUS_CACHE_L2_LINE_COUNT;
        return false;
    }
}

static bool dropus__reload_cache(dropus* pOpus)
{
    // Fast path. Try just moving the next value in the L2 cache to the L1 cache.
    if (dropus__reload_l1_cache_from_l2(pOpus)) {
        pOpus->cache = dropus__be2host__cache_line(pOpus->cache);
        pOpus->consumedBits = 0;
        return true;
    }

    // Slow path.

    // If we get here it means we have failed to load the L1 cache from the L2. Likely we've just reached the end of the stream and the last
    // few bytes did not meet the alignment requirements for the L2 cache. In this case we need to fall back to a slower path and read the
    // data straight from the client into the L1 cache. This should only really happen once per stream so efficiency is not important.
    size_t bytesRead = pOpus->onRead(pOpus->pUserData, &pOpus->cache, DROPUS_CACHE_L1_SIZE_BYTES);
    if (bytesRead == 0) {
        return false;
    }

    pOpus->currentBytePos += bytesRead;

    assert(bytesRead < DROPUS_CACHE_L1_SIZE_BYTES);
    pOpus->consumedBits = (DROPUS_CACHE_L1_SIZE_BYTES - bytesRead) * 8;

    pOpus->cache = dropus__be2host__cache_line(pOpus->cache);
    pOpus->cache &= DROPUS_CACHE_L1_SELECTION_MASK(DROPUS_CACHE_L1_SIZE_BITS - pOpus->consumedBits);    // <-- Make sure the consumed bits are always set to zero. Other parts of the library depend on this property.
    return true;
}

static bool dropus__seek_bits(dropus* pOpus, size_t bitsToSeek)
{
    if (bitsToSeek <= DROPUS_CACHE_L1_BITS_REMAINING) {
        pOpus->consumedBits += bitsToSeek;
        pOpus->cache <<= bitsToSeek;
        return true;
    } else {
        // It straddles the cached data. This function isn't called too frequently so I'm favouring simplicity here.
        bitsToSeek -= DROPUS_CACHE_L1_BITS_REMAINING;
        pOpus->consumedBits += DROPUS_CACHE_L1_BITS_REMAINING;
        pOpus->cache = 0;

        size_t wholeBytesRemaining = bitsToSeek/8;
        if (wholeBytesRemaining > 0)
        {
            // The next bytes to seek will be located in the L2 cache. The problem is that the L2 cache is not byte aligned,
            // but rather DROPUS_CACHE_L1_SIZE_BYTES aligned (usually 4 or 8). If, for example, the number of bytes to seek is
            // 3, we'll need to handle it in a special way.
            size_t wholeCacheLinesRemaining = wholeBytesRemaining / DROPUS_CACHE_L1_SIZE_BYTES;
            if (wholeCacheLinesRemaining < DROPUS_CACHE_L2_LINES_REMAINING)
            {
                wholeBytesRemaining -= wholeCacheLinesRemaining * DROPUS_CACHE_L1_SIZE_BYTES;
                bitsToSeek -= wholeCacheLinesRemaining * DROPUS_CACHE_L1_SIZE_BITS;
                pOpus->nextL2Line += wholeCacheLinesRemaining;
            }
            else
            {
                wholeBytesRemaining -= DROPUS_CACHE_L2_LINES_REMAINING * DROPUS_CACHE_L1_SIZE_BYTES;
                bitsToSeek -= DROPUS_CACHE_L2_LINES_REMAINING * DROPUS_CACHE_L1_SIZE_BITS;
                pOpus->nextL2Line += DROPUS_CACHE_L2_LINES_REMAINING;

                pOpus->onSeek(pOpus->pUserData, (int)wholeBytesRemaining);
                pOpus->currentBytePos += wholeBytesRemaining;
                bitsToSeek -= wholeBytesRemaining*8;
            }
        }


        if (bitsToSeek > 0) {
            if (!dropus__reload_cache(pOpus)) {
                return false;
            }

            return dropus__seek_bits(pOpus, bitsToSeek);
        }

        return true;
    }
}

static bool dropus__read_uint32(dropus* pOpus, unsigned int bitCount, uint32_t* pResultOut)
{
    assert(pOpus != NULL);
    assert(pResultOut != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 32);

    if (pOpus->consumedBits == DROPUS_CACHE_L1_SIZE_BITS) {
        if (!dropus__reload_cache(pOpus)) {
            return false;
        }
    }

    if (bitCount <= DROPUS_CACHE_L1_BITS_REMAINING) {
        if (bitCount < DROPUS_CACHE_L1_SIZE_BITS) {
            *pResultOut = DROPUS_CACHE_L1_SELECT_AND_SHIFT(bitCount);
            pOpus->consumedBits += bitCount;
            pOpus->cache <<= bitCount;
        } else {
            *pResultOut = (uint32_t)pOpus->cache;
            pOpus->consumedBits = DROPUS_CACHE_L1_SIZE_BITS;
            pOpus->cache = 0;
        }
        return true;
    } else {
        // It straddles the cached data. It will never cover more than the next chunk. We just read the number in two parts and combine them.
        size_t bitCountHi = DROPUS_CACHE_L1_BITS_REMAINING;
        size_t bitCountLo = bitCount - bitCountHi;
        uint32_t resultHi = DROPUS_CACHE_L1_SELECT_AND_SHIFT(bitCountHi);

        if (!dropus__reload_cache(pOpus)) {
            return false;
        }

        *pResultOut = (resultHi << bitCountLo) | DROPUS_CACHE_L1_SELECT_AND_SHIFT(bitCountLo);
        pOpus->consumedBits += bitCountLo;
        pOpus->cache <<= bitCountLo;
        return true;
    }
}

static bool dropus__read_int32(dropus* pOpus, unsigned int bitCount, int32_t* pResult)
{
    assert(pOpus != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 32);

    uint32_t result;
    if (!dropus__read_uint32(pOpus, bitCount, &result)) {
        return false;
    }

    if ((result & (1 << (bitCount - 1)))) {  // TODO: See if we can get rid of this branch.
        result |= (-1 << bitCount);
    }

    *pResult = (int32_t)result;
    return true;
}

static bool dropus__read_uint64(dropus* pOpus, unsigned int bitCount, uint64_t* pResultOut)
{
    assert(bitCount <= 64);
    assert(bitCount >  32);

    uint32_t resultHi;
    if (!dropus__read_uint32(pOpus, bitCount - 32, &resultHi)) {
        return false;
    }

    uint32_t resultLo;
    if (!dropus__read_uint32(pOpus, 32, &resultLo)) {
        return false;
    }

    *pResultOut = (((uint64_t)resultHi) << 32) | ((uint64_t)resultLo);
    return true;
}

static bool dropus__read_int64(dropus* pOpus, unsigned int bitCount, int64_t* pResultOut)
{
    assert(bitCount <= 64);

    uint64_t result;
    if (!dropus__read_uint64(pOpus, bitCount, &result)) {
        return false;
    }

    if ((result & (1ULL << (bitCount - 1)))) {  // TODO: See if we can get rid of this branch.
        result |= (-1LL << bitCount);
    }

    *pResultOut = (int64_t)result;
    return true;
}

static bool dropus__read_uint16(dropus* pOpus, unsigned int bitCount, uint16_t* pResult)
{
    assert(pOpus != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 16);

    uint32_t result;
    if (!dropus__read_uint32(pOpus, bitCount, &result)) {
        return false;
    }

    *pResult = (uint16_t)result;
    return true;
}

static bool dropus__read_int16(dropus* pOpus, unsigned int bitCount, int16_t* pResult)
{
    assert(pOpus != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 16);

    int32_t result;
    if (!dropus__read_int32(pOpus, bitCount, &result)) {
        return false;
    }

    *pResult = (int16_t)result;
    return true;
}

static bool dropus__read_uint8(dropus* pOpus, unsigned int bitCount, uint8_t* pResult)
{
    assert(pOpus != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 8);

    uint32_t result;
    if (!dropus__read_uint32(pOpus, bitCount, &result)) {
        return false;
    }

    *pResult = (uint8_t)result;
    return true;
}

static bool dropus__read_int8(dropus* pOpus, unsigned int bitCount, int8_t* pResult)
{
    assert(pOpus != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 8);

    int32_t result;
    if (!dropus__read_int32(pOpus, bitCount, &result)) {
        return false;
    }

    *pResult = (int8_t)result;
    return true;
}




dropus* dropus_open(dropus_read_proc onRead, dropus_seek_proc onSeek, void* pUserData)
{
    dropus* pOpus = calloc(1, sizeof(*pOpus));
    if (pOpus == NULL) {
        return NULL;
    }

    pOpus->onRead = onRead;
    pOpus->onSeek = onSeek;
    pOpus->pUserData = pUserData;


    return pOpus;
}

void dropus_close(dropus* pOpus)
{
    if (pOpus == NULL) {
        return;
    }

#ifndef DR_OPUS_NO_STDIO
    // If we opened the file with dropus_open_file() we will want to close the file handle. We can know whether or not dropus_open_file()
    // was used by looking at the callbacks.
    if (pOpus->onRead == dropus__on_read_stdio) {
#if defined(DR_OPUS_NO_WIN32_IO) || !defined(_WIN32)
        fclose((FILE*)pOpus->pUserData);
#else
        CloseHandle((HANDLE)pOpus->pUserData);
#endif
    }
#endif

    // If we opened the file with dropus_open_memory() we will want to free() the user data.
    if (pOpus->onRead == dropus__on_read_memory) {
        free(pOpus->pUserData);
    }
}

uint64_t dropus_read_s32(dropus* pOpus, uint64_t samplesToRead, int32_t* pBufferOut)
{
    if (pOpus == NULL || samplesToRead == 0) {
        return 0;
    }

    // TODO: Implement Me.
    return 0;
}

bool dropus_seek_to_sample(dropus* pOpus, uint64_t sampleIndex)
{
    if (pOpus == NULL) {
        return false;
    }

    if (sampleIndex == 0) {
        // TODO: Efficient seek to the start of the stream.
        return false;
    }


    // Clamp the sample to the end.
    if (sampleIndex >= pOpus->totalSampleCount) {
        sampleIndex  = pOpus->totalSampleCount - 1;
    }



    // TODO: Implement Me.
    return false;
}


#endif  // DR_OPUS_IMPLEMENTATION

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
