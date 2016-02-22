// Public domain. See "unlicense" statement at the end of this file.

// QUICK NOTES
//
// - This has not been tested on big-endian architectures.
// - dr_flac is not thread-safe, but it's APIs can be called from any thread so long as the application does it's own thread-safety.
// - dr_flac does not currently do any CRC checks.
//
//
//
// TODO
// - Lots of optimizations:
//   - Bit reading functionality has a LOT of room for improvement. Need to look at reading words at a time rather than bytes.
// - Lots of testing:
//   - Big endian is untested.
//   - Unencoded Rice residual is untested.
// - Add an API to retrieve samples in 32-bit floating point format for consistency with dr_wav. Make it optional through the use
//   of a #define, just like dr_wav does it.
// - Look at removing the onTell callback function to make using the library just that bit easier. Also consider making onSeek
//   optional, in which case seeking and metadata block retrieval will be disabled.
// - Get GCC and Linux builds working.

#ifndef dr_flac_h
#define dr_flac_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* drflac_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

// Callback for when data needs to be seeked. Offset is always relative to the current position. Return value is 0 on failure, non-zero success.
typedef int (* drflac_seek_proc)(void* userData, int offset);

// Callback for when the read position needs to be queried.
typedef long long (* drflac_tell_proc)(void* userData);


typedef struct
{
    // The absolute position of the first byte of the data of the block. This is just past the block's header.
    long long pos;

    // The size in bytes of the block's data.
    unsigned int sizeInBytes;

} drflac_block;

typedef struct
{
    // The type of the subframe: SUBFRAME_CONSTANT, SUBFRAME_VERBATIM, SUBFRAME_FIXED or SUBFRAME_LPC.
    unsigned char subframeType;

    // The number of wasted bits per sample as specified by the sub-frame header.
    unsigned char wastedBitsPerSample;


    // The order to use for the prediction stage for SUBFRAME_FIXED and SUBFRAME_LPC.
    unsigned char lpcOrder;

    // The bit shift to apply at the end of the prediction stage. Only used with SUBFRAME_LPC. Note how this is signed.
    unsigned char lpcShift;

    // The coefficients to use for the prediction stage. For SUBFRAME_FIXED this is set to a pre-defined set of values based on lpcOrder. There
    // is a maximum of 32 coefficients. The number of valid values in this array is equal to lpcOrder.
    short lpcCoefficients[32];

    // The previous samples to use for the prediction stage. Only used with SUBFRAME_FIXED and SUBFRAME_LPC. The number of items in this array
    // is equal to lpcOrder.
    int lpcPrevSamples[32];
    

    // The number of bits per sample for this subframe. This is not always equal to the current frame's bit per sample because
    // an extra bit is required for side channels when interchannel decorrelation is being used.
    int bitsPerSample;


    // A pointer to the buffer containing the decoded samples in the subframe. This pointer is an offset from drflac::pHeap, or
    // NULL if the heap is not being used. Note that it's a signed 32-bit integer for each value.
    int32_t* pDecodedSamples;

} drflac_subframe;

typedef struct
{
    // If the stream uses variable block sizes, this will be set to the index of the first sample. If fixed block sizes are used, this will
    // always be set to 0.
    unsigned long long sampleNumber;

    // If the stream uses fixed block sizes, this will be set to the frame number. If variable block sizes are used, this will always be 0.
    unsigned int frameNumber;

    // The sample rate of this frame.
    unsigned int sampleRate;

    // The number of samples in each sub-frame within this frame.
    unsigned short blockSize;

    // The channel assignment of this frame. This is not always set to the channel count. If interchannel decorrelation is being used this
    // will be set to DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE, DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE or DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE.
    unsigned char channelAssignment;

    // The number of bits per sample within this frame.
    unsigned char bitsPerSample;

    // The frame's CRC. This is set, but unused at the moment.
    unsigned char crc8;


    // The number of samples left to be read in this frame. This is initially set to the block size multiplied by the channel count. As samples
    // are read, this will be decremented. When it reaches 0, the decoder will see this frame as fully consumed and load the next frame.
    unsigned int samplesRemaining;


    // The list of sub-frames within the frame. There is one sub-frame for each channel, and there's a maximum of 8 channels.
    drflac_subframe subframes[8];

} drflac_frame;

typedef struct
{
    // The function to call when more data needs to be read. This is set by drflac_open().
    drflac_read_proc onRead;

    // The function to call when the current read position needs to be moved.
    drflac_seek_proc onSeek;

    // The function to call when the current read position needs to be queried.
    drflac_tell_proc onTell;

    // The user data to pass around to onRead and onSeek.
    void* userData;


    // The current byte position in the client's data stream.
    uint64_t currentBytePos;

    // The temporary byte containing containing the bits that have not yet been read.
    uint64_t leftoverBytes;

    // The number of bits remaining in the byte the reader is currently sitting on.
    unsigned char leftoverBitsRemaining;


    // The sample rate. Will be set to something like 44100.
    unsigned int sampleRate;

    // The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc. Maximum 8. This is set based on the
    // value specified in the STREAMINFO block.
    unsigned char channels;

    // The bits per sample. Will be set to somthing like 16, 24, etc.
    unsigned char bitsPerSample;

    // The maximum block size, in samples. This number represents the number of samples in each channel (not combined.)
    unsigned short maxBlockSize;

    // The total number of samples making up the stream. This includes every channel. For example, if the stream has 2 channels,
    // with each channel having a total of 4096, this value will be set to 2*4096 = 8192.
    unsigned long long totalSampleCount;
    

    // The location and size of the APPLICATION block.
    drflac_block applicationBlock;

    // The location and size of the SEEKTABLE block.
    drflac_block seektableBlock;

    // The location and size of the VORBIS_COMMENT block.
    drflac_block vorbisCommentBlock;

    // The location and size of the CUESHEET block.
    drflac_block cuesheetBlock;

    // The location and size of the PICTURE block.
    drflac_block pictureBlock;


    // Information about the frame the decoder is currently sitting on.
    drflac_frame currentFrame;

    // The position of the first frame in the stream. This is only ever used for seeking.
    unsigned long long firstFramePos;

    // A pointer to a block of memory sitting on the heap. This will be set to NULL if the heap is not being used. Currently, this stores
    // the decoded Rice codes for each channel in the current frame.
    void* pHeap;

} drflac;




// Opens a FLAC decoder.
bool drflac_open(drflac* pFlac, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_tell_proc onTell, void* pUserData);

// Closes the given FLAC decoder.
void drflac_close(drflac* pFlac);

// Reads sample data from the given FLAC decoder, output as signed 32-bit PCM.
size_t drflac_read_s32(drflac* pFlac, size_t samplesToRead, int* pBufferOut);

// Seeks to the sample at the given index.
bool drflac_seek_to_sample(drflac* pFlac, uint64_t sampleIndex);



#ifndef DR_FLAC_NO_STDIO
// Opens a flac decoder from the file at the given path.
bool drflac_open_file(drflac* pFlac, const char* pFile);
#endif

// Helper for opening a file from a pre-allocated memory buffer.
//
// This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
// the lifetime of the drwav object.
bool drflac_open_memory(drflac* pFlac, const void* data, size_t dataSize);


#ifdef __cplusplus
}
#endif

#endif  //dr_flac_h


///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DR_FLAC_IMPLEMENTATION
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

#define DRFLAC_BLOCK_TYPE_STREAMINFO                    0
#define DRFLAC_BLOCK_TYPE_PADDING                       1
#define DRFLAC_BLOCK_TYPE_APPLICATION                   2
#define DRFLAC_BLOCK_TYPE_SEEKTABLE                     3
#define DRFLAC_BLOCK_TYPE_VORBIS_COMMENT                4
#define DRFLAC_BLOCK_TYPE_CUESHEET                      5
#define DRFLAC_BLOCK_TYPE_PICTURE                       6
#define DRFLAC_BLOCK_TYPE_INVALID                       127

#define DRFLAC_SUBFRAME_CONSTANT                        0
#define DRFLAC_SUBFRAME_VERBATIM                        1
#define DRFLAC_SUBFRAME_FIXED                           8
#define DRFLAC_SUBFRAME_LPC                             32
#define DRFLAC_SUBFRAME_RESERVED                        -1

#define DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE  0
#define DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2 1

#define DRFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT           0
#define DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE             8
#define DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE            9
#define DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE              10

typedef struct
{
    uint64_t firstSample;
    uint64_t frameOffset;   // The offset from the first byte of the header of the first frame.
    uint16_t sampleCount;
} drflac_seekpoint;

#ifndef DR_FLAC_NO_STDIO
#include <stdio.h>

static size_t drflac__on_read_stdio(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)pUserData);
}

static int drflac__on_seek_stdio(void* pUserData, int offset)
{
    return fseek((FILE*)pUserData, offset, SEEK_CUR) == 0;
}

static long long drflac__on_tell_stdio(void* pUserData)
{
    return ftell((FILE*)pUserData);
}

bool drflac_open_file(drflac* pFlac, const char* filename)
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

    return drflac_open(pFlac, drflac__on_read_stdio, drflac__on_seek_stdio, drflac__on_tell_stdio, pFile);
}
#endif  //DR_FLAC_NO_STDIO


typedef struct
{
    /// A pointer to the beginning of the data. We use a char as the type here for easy offsetting.
    const unsigned char* data;

    /// The size of the data.
    size_t dataSize;

    /// The position we're currently sitting at.
    size_t currentReadPos;

} drflac_memory;

static size_t drflac__on_read_memory(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    drflac_memory* memory = pUserData;
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

static int drflac__on_seek_memory(void* pUserData, int offset)
{
    drflac_memory* memory = pUserData;
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

static long long drflac__on_tell_memory(void* pUserData)
{
    drflac_memory* memory = pUserData;
    assert(memory != NULL);

    return (long long)memory->currentReadPos;
}

bool drflac_open_memory(drflac* pFlac, const void* data, size_t dataSize)
{
    drflac_memory* pUserData = malloc(sizeof(*pUserData));
    if (pUserData == NULL) {
        return false;
    }

    pUserData->data = data;
    pUserData->dataSize = dataSize;
    pUserData->currentReadPos = 0;
    return drflac_open(pFlac, drflac__on_read_memory, drflac__on_seek_memory, drflac__on_tell_memory, pUserData);
}


//// Endian Management ////
static inline bool drflac__is_little_endian()
{
    int n = 1;
    return (*(char*)&n) == 1;
}

static inline uint16_t drflac__swap_endian_uint16(uint16_t n)
{
#ifdef _MSC_VER
    return _byteswap_ushort(n);
#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC__ >= 3))
    return __builtin_bswap16(n);
#else
    return ((n & 0xFF00) >> 8) | ((n & 0x00FF) << 8);
#endif
}

static inline uint32_t drflac__swap_endian_uint32(uint32_t n)
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

static inline uint64_t drflac__swap_endian_uint64(uint64_t n)
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

static inline uint16_t drflac__be2host_16(uint16_t n)
{
#ifdef __linux__
    return be16toh(n);
#else
    if (drflac__is_little_endian()) {
        return drflac__swap_endian_uint16(n);
    }

    return n;
#endif
}

static inline uint32_t drflac__be2host_32(uint32_t n)
{
#ifdef __linux__
    return be32toh(n);
#else
    if (drflac__is_little_endian()) {
        return drflac__swap_endian_uint32(n);
    }

    return n;
#endif
}

static inline uint64_t drflac__be2host_64(uint64_t n)
{
#ifdef __linux__
    return be64toh(n);
#else
    if (drflac__is_little_endian()) {
        return drflac__swap_endian_uint64(n);
    }

    return n;
#endif
}


//// Bit Reading ////
static inline size_t drflac__grab_leftover_bytes(drflac* pFlac)
{
    assert(pFlac != NULL);

    size_t bytesRead = pFlac->onRead(pFlac->userData, &pFlac->leftoverBytes, sizeof(pFlac->leftoverBytes));
    pFlac->currentBytePos += bytesRead;

    pFlac->leftoverBitsRemaining = (uint8_t)(bytesRead << 3);

    if (drflac__is_little_endian()) {
        pFlac->leftoverBytes = drflac__swap_endian_uint64(pFlac->leftoverBytes);
    }
    

    if (bytesRead < sizeof(pFlac->leftoverBytes))
    {
        // If we get here it means we've run out of data. We just make a small adjustment to the leftover to ensure we don't try
        // reading any invalid data.
        pFlac->leftoverBytes >>= (sizeof(pFlac->leftoverBytes) - bytesRead) * 8;

        // TODO: We may need to set a flag here to let other parts know that we're sitting at the end of the file.
    }

    

    return bytesRead;
}

static inline bool drflac__seek_to_byte(drflac* pFlac, long long offsetFromStart)
{
    assert(pFlac != NULL);

    long long bytesToMove = offsetFromStart - pFlac->currentBytePos;
    if (bytesToMove == 0) {
        return 1;
    }

    if (bytesToMove > 0x7FFFFFFF) {
        while (bytesToMove > 0x7FFFFFFF) {
            if (!pFlac->onSeek(pFlac->userData, 0x7FFFFFFF)) {
                return 0;
            }

            pFlac->currentBytePos += 0x7FFFFFFF;
            bytesToMove -= 0x7FFFFFFF;
        }
    } else {
        while (bytesToMove < (int)0x80000000) {
            if (!pFlac->onSeek(pFlac->userData, (int)0x80000000)) {
                return 0;
            }

            pFlac->currentBytePos += (int)0x80000000;
            bytesToMove -= (int)0x80000000;
        }
    }

    assert(bytesToMove <= 0x7FFFFFFF && bytesToMove >= (int)0x80000000);

    bool result = pFlac->onSeek(pFlac->userData, (int)bytesToMove);    // <-- Safe cast as per the assert above.
    pFlac->currentBytePos += (int)bytesToMove;
    pFlac->leftoverBitsRemaining = 0;

    return result;
}

static inline long long drflac__tell(drflac* pFlac)
{
    assert(pFlac != NULL);
    return pFlac->currentBytePos - (pFlac->leftoverBitsRemaining / 8);
}

static unsigned int drflac__seek_bits(drflac* pFlac, unsigned int bitsToSeek)
{
    assert(pFlac != NULL);

    unsigned int bitsSeeked = 0;

    // Leftover bits
    if (pFlac->leftoverBitsRemaining > 0) {
        if (bitsToSeek >= pFlac->leftoverBitsRemaining) {
            bitsSeeked = pFlac->leftoverBitsRemaining;
            pFlac->leftoverBitsRemaining = 0;
        } else {
            // The whole data is entirely contained within the leftover bits.
            pFlac->leftoverBitsRemaining -= bitsToSeek;
            return bitsToSeek;
        }
    }

    assert(pFlac->leftoverBitsRemaining == 0);

    // Wholy contained bytes.
    int bytesToSeek = (int)(bitsToSeek - bitsSeeked) / 8;
    if (bytesToSeek > 0) {
        if (!pFlac->onSeek(pFlac->userData, bytesToSeek)) {
            return false;
        }

        pFlac->currentBytePos += bytesToSeek;
        bitsSeeked += bytesToSeek * 8;
    }

    // Trailing bits.
    unsigned int bitsRemaining = (bitsToSeek - bitsSeeked);
    if (bitsRemaining > 0)
    {
        assert(bitsRemaining < 8);

        if (drflac__grab_leftover_bytes(pFlac) != sizeof(pFlac->leftoverBytes)) {
            return bitsSeeked;    // Ran out of data.
        }

        bitsSeeked += bitsRemaining;
        pFlac->leftoverBitsRemaining -= bitsRemaining;
    }


    return bitsSeeked;
}

static inline int drflac__read_next_bit(drflac* pFlac)
{
    assert(pFlac != NULL);

    if (pFlac->leftoverBitsRemaining == 0) {
        if (drflac__grab_leftover_bytes(pFlac) == 0) {
            return -1;  // Ran out of data.
        }
    }

    pFlac->leftoverBitsRemaining -= 1;
    return (pFlac->leftoverBytes & (1ULL << pFlac->leftoverBitsRemaining)) >> pFlac->leftoverBitsRemaining;
}

static inline bool drflac__read_uint64(drflac* pFlac, unsigned int bitCount, uint64_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 64);

    // TODO: This could do with some cleanup and clarification... I'm sure it's also more complicated than it needs to be.

    uint64_t result = 0;
    if (pFlac->leftoverBitsRemaining > 0) {
        uint64_t leftoverMask = (((uint64_t)-1LL) >> ((uint64_t)((sizeof(pFlac->leftoverBytes)*8) - pFlac->leftoverBitsRemaining)));
        if (pFlac->leftoverBitsRemaining > bitCount) {
            // The whole value is contained within the leftover.
            leftoverMask ^= (((uint64_t)-1LL) >> ((uint64_t)(((sizeof(pFlac->leftoverBytes)*8) - pFlac->leftoverBitsRemaining) + bitCount)));
            pFlac->leftoverBitsRemaining -= bitCount;
            result = (pFlac->leftoverBytes & leftoverMask) >> pFlac->leftoverBitsRemaining;
            goto done_reading_uint64;
        } else if (pFlac->leftoverBitsRemaining == bitCount) {
            pFlac->leftoverBitsRemaining -= bitCount;
            result = (pFlac->leftoverBytes & leftoverMask) >> pFlac->leftoverBitsRemaining;
            goto done_reading_uint64;
        } else {
            // Only part of the value is contained within the leftover.
            result = (pFlac->leftoverBytes & leftoverMask) << (bitCount - pFlac->leftoverBitsRemaining);
            bitCount -= pFlac->leftoverBitsRemaining;

            drflac__grab_leftover_bytes(pFlac);

            uint64_t leftoverMask = ~(((uint64_t)-1LL) >> bitCount);
            if (pFlac->leftoverBitsRemaining < sizeof(pFlac->leftoverBytes)*8) {
                leftoverMask >>= (sizeof(pFlac->leftoverBytes)*8 - pFlac->leftoverBitsRemaining);
            }

            result |= ((pFlac->leftoverBytes & leftoverMask) << ((sizeof(pFlac->leftoverBytes)*8 - pFlac->leftoverBitsRemaining))) >> ((sizeof(pFlac->leftoverBytes)*8) - bitCount);

            pFlac->leftoverBitsRemaining -= bitCount;
            goto done_reading_uint64;
        }
    }

    // At this point there should be no leftover bits remaining and we'll need to grab the bytes directly from the client.
    assert(pFlac->leftoverBitsRemaining == 0);

    // We need to read in the leftover and just copy over the first <bitCount> bits, which will always be fully contained within the leftover.
    drflac__grab_leftover_bytes(pFlac);

    if (bitCount < sizeof(pFlac->leftoverBytes)*8) {
        uint64_t leftoverMask = ~(((uint64_t)-1LL) >> bitCount);
        result = (pFlac->leftoverBytes & leftoverMask) >> ((sizeof(pFlac->leftoverBytes)*8) - bitCount);
        pFlac->leftoverBitsRemaining -= bitCount;
    } else {
        result = pFlac->leftoverBytes;
        pFlac->leftoverBitsRemaining = 0;
    }


done_reading_uint64:
    *pResult = result;
    return true;
}

static inline bool drflac__read_int64(drflac* pFlac, unsigned int bitCount, int64_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 64);

    uint64_t result;
    if (!drflac__read_uint64(pFlac, bitCount, &result)) {
        return false;
    }

    if ((result & (1ULL << (bitCount - 1)))) {
        result |= (-1LL << bitCount);
    }

    *pResult = (int64_t)result;
    return true;
}

static inline bool drflac__read_uint32(drflac* pFlac, unsigned int bitCount, uint32_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 32);

    uint64_t result;
    if (!drflac__read_uint64(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (uint32_t)result;
    return true;
}

static inline bool drflac__read_int32(drflac* pFlac, unsigned int bitCount, int32_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 32);

    int64_t result;
    if (!drflac__read_int64(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (int32_t)result;
    return true;
}

static inline bool drflac__read_uint16(drflac* pFlac, unsigned int bitCount, uint16_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 16);

    uint64_t result;
    if (!drflac__read_uint64(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (uint16_t)result;
    return true;
}

static inline bool drflac__read_int16(drflac* pFlac, unsigned int bitCount, int16_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 16);

    int64_t result;
    if (!drflac__read_int64(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (int16_t)result;
    return true;
}

static inline bool drflac__read_uint8(drflac* pFlac, unsigned int bitCount, uint8_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 8);

    uint64_t result;
    if (!drflac__read_uint64(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (uint8_t)result;
    return true;
}

static inline bool drflac__read_int8(drflac* pFlac, unsigned int bitCount, int8_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 8);

    int64_t result;
    if (!drflac__read_int64(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (int8_t)result;
    return true;
}






static bool drflac__read_utf8_coded_number(drflac* pFlac, unsigned long long* pNumberOut)
{
    assert(pFlac != NULL);
    assert(pNumberOut != NULL);

    // We should never need to read UTF-8 data while not being aligned to a byte boundary. Therefore we can grab the data
    // directly from the input stream rather than using drflac__read_uint8().
    assert((pFlac->leftoverBitsRemaining % 8) == 0);

    unsigned char utf8[7] = {0};
    if (!drflac__read_uint8(pFlac, 8, utf8)) {
        *pNumberOut = 0;
        return false;
    }

    if ((utf8[0] & 0x80) == 0) {
        *pNumberOut = utf8[0];
        return true;
    }

    int byteCount = 1;
    if ((utf8[0] & 0xE0) == 0xC0) {
        byteCount = 2;
    } else if ((utf8[0] & 0xF0) == 0xE0) {
        byteCount = 3;
    } else if ((utf8[0] & 0xF8) == 0xF0) {
        byteCount = 4;
    } else if ((utf8[0] & 0xFC) == 0xF8) {
        byteCount = 5;
    } else if ((utf8[0] & 0xFE) == 0xFC) {
        byteCount = 6;
    } else if ((utf8[0] & 0xFF) == 0xFE) {
        byteCount = 7;
    } else {
        *pNumberOut = 0;
        return false;     // Bad UTF-8 encoding.
    }

    // Read extra bytes.
    assert(byteCount > 1);

    unsigned long long result = ((long long)(utf8[0] & (0xFF >> (byteCount + 1))));
    for (int i = 1; i < byteCount; ++i) {
        if (!drflac__read_uint8(pFlac, 8, utf8 + i)) {
            *pNumberOut = 0;
            return false;
        }

        result = (result << 6) | (utf8[i] & 0x3F);
    }

    *pNumberOut = result;
    return true;
}



#if 0
static inline bool drflac__read_and_decode_rice(drflac* pFlac, unsigned char m, int* pValueOut)
{
    // TODO: Profile and optimize this. Will probably need to look at decoding Rice codes in groups.

    unsigned int zeroCounter = 0;
    while (drflac__read_next_bit(pFlac) == 0) {
        zeroCounter += 1;
    }

    unsigned int decodedValue = 0;
    if (m > 0) {
        drflac__read_uint32(pFlac, m, &decodedValue);
    }
    


    decodedValue |= (zeroCounter << m);
    if ((decodedValue & 0x01)) {
        decodedValue = ~(decodedValue >> 1);
    } else {
        decodedValue = (decodedValue >> 1);
    }

    *pValueOut = (int)decodedValue;
    return true;
}
#endif

static inline bool drflac__read_and_seek_rice(drflac* pFlac, unsigned char m)
{
    // TODO: Profile and optimize this. Will probably need to look at decoding Rice codes in groups.

    while (drflac__read_next_bit(pFlac) == 0) {
    }

    if (m > 0) {
        drflac__seek_bits(pFlac, m);
    }
    
    return true;
}

// Seeks to the next set bit, and returns it's offset.
static inline bool drflac__seek_to_next_set_bit(drflac* pFlac, unsigned int* pOffsetOut)
{
    // Slow naive method.
#if 0
    unsigned int zeroCounter = 0;
    while (drflac__read_next_bit(pFlac) == 0) {
        zeroCounter += 1;
    }
    
    *pOffsetOut = zeroCounter;
    return true;
#endif


    // Experiment #1: Not-so-slow-but-still-slow naive method.
    // Result: A tiny bit faster, but nothing special.
#if 1
    unsigned int prevLeftoverBitsRemaining = pFlac->leftoverBitsRemaining;
    while (pFlac->leftoverBitsRemaining > 0)
    {
        if (pFlac->leftoverBytes & (1ULL << (pFlac->leftoverBitsRemaining - 1))) {
            *pOffsetOut = (prevLeftoverBitsRemaining - pFlac->leftoverBitsRemaining);
            pFlac->leftoverBitsRemaining -= 1;
            return true;
        }

        pFlac->leftoverBitsRemaining -= 1;
    }

    // If we get here it means we've ran out of leftover. We just need to load more and keep looking. If it's not
    // in this chunk there is no hope in finding it.
    drflac__grab_leftover_bytes(pFlac);
    if (pFlac->leftoverBitsRemaining == 0) {
        return false;   // No more data avaialable.
    }

    while (pFlac->leftoverBitsRemaining > 0)
    {
        if (pFlac->leftoverBytes & (1ULL << (pFlac->leftoverBitsRemaining - 1))) {
            *pOffsetOut = (prevLeftoverBitsRemaining + ((sizeof(pFlac->leftoverBytes)*8) - pFlac->leftoverBitsRemaining));
            pFlac->leftoverBitsRemaining -= 1;
            return true;
        }

        pFlac->leftoverBitsRemaining -= 1;
    }

    return false;
#endif

    // Experiment #2: A divide and conquer type thing where we take a 32-bit mask and check if any bits are set in it. If
    // not, just move to the next 32-bits.
#if 0
    uint64_t mask1  = 0x1;
    uint64_t mask2  = 0x3;
    uint64_t mask4  = 0xF;
    uint64_t mask8  = 0xFF;
    uint64_t mask16 = 0xFFFF;
    uint64_t mask32 = 0xFFFFFFFF;
#endif
}


static inline bool drflac__read_and_decode_rice(drflac* pFlac, unsigned char riceParam, int* pValueOut)
{
    unsigned int zeroCounter;
    if (!drflac__seek_to_next_set_bit(pFlac, &zeroCounter)) {
        return false;
    }

    unsigned int decodedValue = 0;
    if (riceParam > 0) {
        drflac__read_uint32(pFlac, riceParam, &decodedValue);
    }
    

    decodedValue |= (zeroCounter << riceParam);
    if ((decodedValue & 0x01)) {
        decodedValue = ~(decodedValue >> 1);
    } else {
        decodedValue = (decodedValue >> 1);
    }

    *pValueOut = (int)decodedValue;
    return true;
}


// Reads and decodes a string of residual values as Rice codes. The decoder should be sitting on the first bit of the Rice codes.
static bool drflac__decode_samples_with_residual__rice(drflac* pFlac, unsigned int count, unsigned char riceParam, unsigned int order, int shift, const short* coefficients, int* pResidualOut)
{
    assert(pFlac != NULL);
    assert(count > 0);
    assert(pResidualOut != NULL);

    for (int i = 0; i < (int)count; ++i)
    {
        // We need to find the first set bit from the current position.
        if (!drflac__read_and_decode_rice(pFlac, riceParam, pResidualOut + i)) {
            return false;
        }
        
        
        long long prediction = 0;
        for (int j = 0; j < (int)order; ++j) {
            prediction += (long long)coefficients[j] * (long long)pResidualOut[i - j - 1];
        }
        prediction >>= shift;

        pResidualOut[i] += (int)prediction;
    }

    return true;

#if 0
    for (unsigned int i = 0; i < count; ++i) {
        if (!drflac__read_and_decode_rice(pFlac, riceParam, pResidualOut)) {
            return false;
        }

        pResidualOut += 1;
    }

    return true;
#endif
}

static bool drflac__decode_samples_with_residual__fixed_rice(drflac* pFlac, unsigned int count, unsigned char riceParam, unsigned int order, int* pDecodedSamples)
{
    assert(pFlac != NULL);
    assert(count > 0);

    /*short lpcCoefficientsTable[5][4] = {
        {0,  0, 0,  0},
        {1,  0, 0,  0},
        {2, -1, 0,  0},
        {3, -3, 1,  0},
        {4, -6, 4, -1}
    };*/

    switch (order)
    {
        case 0:
        {
        } break;

        case 1:
        {
            for (int i = 0; i < (int)count; ++i) {
                if (!drflac__read_and_decode_rice(pFlac, riceParam, pDecodedSamples + i)) {
                    return false;
                }

                pDecodedSamples[i] += (int)(1*pDecodedSamples[i-1]);
            }
        } break;

        case 2:
        {
            for (int i = 0; i < (int)count; ++i) {
                if (!drflac__read_and_decode_rice(pFlac, riceParam, pDecodedSamples + i)) {
                    return false;
                }

                pDecodedSamples[i] += (int)(2*pDecodedSamples[i-1] + -1*pDecodedSamples[i-2]);
            }
        } break;

        case 3:
        {
            for (int i = 0; i < (int)count; ++i) {
                if (!drflac__read_and_decode_rice(pFlac, riceParam, pDecodedSamples + i)) {
                    return false;
                }

                pDecodedSamples[i] += (int)(3*pDecodedSamples[i-1] + -3*pDecodedSamples[i-2] + 1*pDecodedSamples[i-3]);
            }
        } break;

        case 4:
        {
            for (int i = 0; i < (int)count; ++i) {
                if (!drflac__read_and_decode_rice(pFlac, riceParam, pDecodedSamples + i)) {
                    return false;
                }

                pDecodedSamples[i] += (int)(4*pDecodedSamples[i-1] + -6*pDecodedSamples[i-2] + 4*pDecodedSamples[i-3] + -1*pDecodedSamples[i-4]);
            }
        } break;

        default: return false;
    }

    return true;
}

static bool drflac__decode_samples_with_residual__fixed_unencoded(drflac* pFlac, unsigned int count, unsigned char unencodedBitsPerSample, unsigned int order, int* pDecodedSamples)
{
    assert(pFlac != NULL);
    assert(count > 0);
    assert(unencodedBitsPerSample > 0 && unencodedBitsPerSample <= 32);
    assert(pDecodedSamples != NULL);

    switch (order)
    {
        case 0:
        {
        } break;

        case 1:
        {
            for (int i = 0; i < (int)count; ++i) {
                if (!drflac__read_int32(pFlac, unencodedBitsPerSample, pDecodedSamples)) {
                    return false;
                }

                pDecodedSamples[i] += (int)(1*pDecodedSamples[i-1]);
            }
        } break;

        case 2:
        {
            for (int i = 0; i < (int)count; ++i) {
                if (!drflac__read_int32(pFlac, unencodedBitsPerSample, pDecodedSamples)) {
                    return false;
                }

                pDecodedSamples[i] += (int)(2*pDecodedSamples[i-1] + -1*pDecodedSamples[i-2]);
            }
        } break;

        case 3:
        {
            for (int i = 0; i < (int)count; ++i) {
                if (!drflac__read_int32(pFlac, unencodedBitsPerSample, pDecodedSamples)) {
                    return false;
                }

                pDecodedSamples[i] += (int)(3*pDecodedSamples[i-1] + -3*pDecodedSamples[i-2] + 1*pDecodedSamples[i-3]);
            }
        } break;

        case 4:
        {
            for (int i = 0; i < (int)count; ++i) {
                if (!drflac__read_int32(pFlac, unencodedBitsPerSample, pDecodedSamples)) {
                    return false;
                }

                pDecodedSamples[i] += (int)(4*pDecodedSamples[i-1] + -6*pDecodedSamples[i-2] + 4*pDecodedSamples[i-3] + -1*pDecodedSamples[i-4]);
            }
        } break;

        default: return false;
    }

    return true;
}


// Reads and seeks past a string of residual values as Rice codes. The decoder should be sitting on the first bit of the Rice codes.
static bool drflac__read_and_seek_residual__rice(drflac* pFlac, unsigned int count, unsigned char riceParam)
{
    assert(pFlac != NULL);
    assert(count > 0);

    for (unsigned int i = 0; i < count; ++i) {
        if (!drflac__read_and_seek_rice(pFlac, riceParam)) {
            return false;
        }
    }

    return true;
}

static bool drflac__decode_samples_with_residual__unencoded(drflac* pFlac, unsigned int count, unsigned char unencodedBitsPerSample, unsigned int order, int shift, const short* coefficients, int* pResidualOut)
{
    assert(pFlac != NULL);
    assert(count > 0);
    assert(unencodedBitsPerSample > 0 && unencodedBitsPerSample <= 32);
    assert(pResidualOut != NULL);

    for (unsigned int i = 0; i < count; ++i)
    {
        if (!drflac__read_int32(pFlac, unencodedBitsPerSample, pResidualOut)) {
            return false;
        }

        // TODO: Prediction.

        pResidualOut += 1;
    }

    return true;
}


static bool drflac__decode_samples_with_residual__fixed(drflac* pFlac, unsigned int blockSize, unsigned int order, int* pDecodedSamples)
{
    assert(pFlac != NULL);
    assert(blockSize != 0);
    assert(pDecodedSamples != NULL);       // <-- Should we allow NULL, in which case we just seek past the residual rather than do a full decode?

    unsigned char residualMethod;
    if (!drflac__read_uint8(pFlac, 2, &residualMethod)) {
        return false;
    }

    if (residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE && residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
        return false;    // Unknown or unsupported residual coding method.
    }

    // Ignore the first <order> values.
    pDecodedSamples += order;


    unsigned char partitionOrder;
    if (!drflac__read_uint8(pFlac, 4, &partitionOrder)) {
        return false;
    }


    unsigned int samplesInPartition = (blockSize / (1 << partitionOrder)) - order;
    unsigned int partitionsRemaining = (1 << partitionOrder);
    for (;;)
    {
        unsigned char riceParam = 0;
        if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
            if (!drflac__read_uint8(pFlac, 4, &riceParam)) {
                return false;
            }
            if (riceParam == 16) {
                riceParam = 0xFF;
            }
        } else if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
            if (!drflac__read_uint8(pFlac, 5, &riceParam)) {
                return false;
            }
            if (riceParam == 32) {
                riceParam = 0xFF;
            }
        }

        if (riceParam != 0xFF) {
            if (!drflac__decode_samples_with_residual__fixed_rice(pFlac, samplesInPartition, riceParam, order, pDecodedSamples)) {
                return false;
            }
        } else {
            unsigned char unencodedBitsPerSample = 0;
            if (!drflac__read_uint8(pFlac, 5, &unencodedBitsPerSample)) {
                return false;
            }

            if (!drflac__decode_samples_with_residual__fixed_unencoded(pFlac, samplesInPartition, unencodedBitsPerSample, order, pDecodedSamples)) {
                return false;
            }
        }

        pDecodedSamples += samplesInPartition;

        
        if (partitionsRemaining == 1) {
            break;
        }

        partitionsRemaining -= 1;
        samplesInPartition = blockSize / (1 << partitionOrder);
    }

    return true;
}



// Reads and decodes the residual for the sub-frame the decoder is currently sitting on. This function should be called
// when the decoder is sitting at the very start of the RESIDUAL block. The first <order> residuals will be ignored. The
// <blockSize> and <order> parameters are used to determine how many residual values need to be decoded.
static bool drflac__decode_samples_with_residual(drflac* pFlac, unsigned int blockSize, unsigned int order, int shift, const short* coefficients, int* pDecodedSamples)
{
    assert(pFlac != NULL);
    assert(blockSize != 0);
    assert(pDecodedSamples != NULL);       // <-- Should we allow NULL, in which case we just seek past the residual rather than do a full decode?

    unsigned char residualMethod;
    if (!drflac__read_uint8(pFlac, 2, &residualMethod)) {
        return false;
    }

    if (residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE && residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
        return false;    // Unknown or unsupported residual coding method.
    }

    // Ignore the first <order> values.
    pDecodedSamples += order;


    unsigned char partitionOrder;
    if (!drflac__read_uint8(pFlac, 4, &partitionOrder)) {
        return false;
    }


    unsigned int samplesInPartition = (blockSize / (1 << partitionOrder)) - order;
    unsigned int partitionsRemaining = (1 << partitionOrder);
    for (;;)
    {
        unsigned char riceParam = 0;
        if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
            if (!drflac__read_uint8(pFlac, 4, &riceParam)) {
                return false;
            }
            if (riceParam == 16) {
                riceParam = 0xFF;
            }
        } else if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
            if (!drflac__read_uint8(pFlac, 5, &riceParam)) {
                return false;
            }
            if (riceParam == 32) {
                riceParam = 0xFF;
            }
        }

        if (riceParam != 0xFF) {
            if (!drflac__decode_samples_with_residual__rice(pFlac, samplesInPartition, riceParam, order, shift, coefficients, pDecodedSamples)) {
                return false;
            }
        } else {
            unsigned char unencodedBitsPerSample = 0;
            if (!drflac__read_uint8(pFlac, 5, &unencodedBitsPerSample)) {
                return false;
            }

            if (!drflac__decode_samples_with_residual__unencoded(pFlac, samplesInPartition, unencodedBitsPerSample, order, shift, coefficients, pDecodedSamples)) {
                return false;
            }
        }

        pDecodedSamples += samplesInPartition;

        
        if (partitionsRemaining == 1) {
            break;
        }

        partitionsRemaining -= 1;
        samplesInPartition = blockSize / (1 << partitionOrder);
    }

    return true;
}

// Reads and seeks past the residual for the sub-frame the decoder is currently sitting on. This function should be called
// when the decoder is sitting at the very start of the RESIDUAL block. The first <order> residuals will be set to 0. The
// <blockSize> and <order> parameters are used to determine how many residual values need to be decoded.
static bool drflac__read_and_seek_residual(drflac* pFlac, unsigned int blockSize, unsigned int order)
{
    assert(pFlac != NULL);
    assert(blockSize != 0);

    unsigned char residualMethod;
    if (!drflac__read_uint8(pFlac, 2, &residualMethod)) {
        return false;
    }

    if (residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE && residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
        return false;    // Unknown or unsupported residual coding method.
    }

    unsigned char partitionOrder;
    if (!drflac__read_uint8(pFlac, 4, &partitionOrder)) {
        return false;
    }

    unsigned int samplesInPartition = (blockSize / (1 << partitionOrder)) - order;
    unsigned int partitionsRemaining = (1 << partitionOrder);
    for (;;)
    {
        unsigned char riceParam = 0;
        if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
            if (!drflac__read_uint8(pFlac, 4, &riceParam)) {
                return false;
            }
            if (riceParam == 16) {
                riceParam = 0xFF;
            }
        } else if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
            if (!drflac__read_uint8(pFlac, 5, &riceParam)) {
                return false;
            }
            if (riceParam == 32) {
                riceParam = 0xFF;
            }
        }

        if (riceParam != 0xFF) {
            if (!drflac__read_and_seek_residual__rice(pFlac, samplesInPartition, riceParam)) {
                return false;
            }
        } else {
            unsigned char unencodedBitsPerSample = 0;
            if (!drflac__read_uint8(pFlac, 5, &unencodedBitsPerSample)) {
                return false;
            }

            if (!drflac__seek_bits(pFlac, unencodedBitsPerSample * samplesInPartition)) {
                return false;
            }
        }

        
        if (partitionsRemaining == 1) {
            break;
        }

        partitionsRemaining -= 1;
        samplesInPartition = blockSize / (1 << partitionOrder);
    }

    return true;
}


static bool drflac__decode_samples__constant(drflac* pFlac, drflac_subframe* pSubframe)
{
    // Only a single sample needs to be decoded here.
    int sample;
    if (!drflac__read_int32(pFlac, pSubframe->bitsPerSample, &sample)) {
        return false;
    }

    // We don't really need to expand this, but it does simplify the process of reading samples. If this becomes a performance issue (unlikely)
    // we'll want to look at a more efficient way.
    for (unsigned int i = 0; i < pFlac->currentFrame.blockSize; ++i) {
        pSubframe->pDecodedSamples[i] = sample;
    }

    return true;
}

static bool drflac__decode_samples__verbatim(drflac* pFlac, drflac_subframe* pSubframe)
{
    for (unsigned int i = 0; i < pFlac->currentFrame.blockSize; ++i) {
        int sample;
        if (!drflac__read_int32(pFlac, pSubframe->bitsPerSample, &sample)) {
            return false;
        }

        pSubframe->pDecodedSamples[i] = sample;
    }

    return true;
}

static bool drflac__decode_samples__fixed(drflac* pFlac, drflac_subframe* pSubframe)
{
    short lpcCoefficientsTable[5][4] = {
        {0,  0, 0,  0},
        {1,  0, 0,  0},
        {2, -1, 0,  0},
        {3, -3, 1,  0},
        {4, -6, 4, -1}
    };

    // Warm up samples and coefficients.
    for (unsigned int i = 0; i < pSubframe->lpcOrder; ++i) {
        int sample;
        if (!drflac__read_int32(pFlac, pSubframe->bitsPerSample, &sample)) {
            return false;
        }

        pSubframe->lpcPrevSamples[i]  = sample;
        pSubframe->lpcCoefficients[i] = lpcCoefficientsTable[pSubframe->lpcOrder][i];
        pSubframe->pDecodedSamples[i] = sample;
    }

            
    if (!drflac__decode_samples_with_residual__fixed(pFlac, pFlac->currentFrame.blockSize, pSubframe->lpcOrder, /*0, lpcCoefficientsTable[pSubframe->lpcOrder],*/ pSubframe->pDecodedSamples)) {
        return false;
    }

    return true;
}

static bool drflac__decode_samples__lcp(drflac* pFlac, drflac_subframe* pSubframe)
{
    // Warm up samples.
    for (unsigned int i = 0; i < pSubframe->lpcOrder; ++i) {
        int sample;
        if (!drflac__read_int32(pFlac, pSubframe->bitsPerSample, &sample)) {
            return false;
        }

        pSubframe->lpcPrevSamples[i]  = sample;
        pSubframe->pDecodedSamples[i] = sample;
    }

    unsigned char lpcPrecision;
    if (!drflac__read_uint8(pFlac, 4, &lpcPrecision)) {
        return false;
    }
    if (lpcPrecision == 15) {
        return false;    // Invalid.
    }
    lpcPrecision += 1;


    signed char lpcShift;
    if (!drflac__read_int8(pFlac, 5, &lpcShift)) {
        return false;
    }


    for (unsigned int i = 0; i < pSubframe->lpcOrder; ++i) {
        if (!drflac__read_int16(pFlac, lpcPrecision, pSubframe->lpcCoefficients + i)) {
            return false;
        }
    }

    if (!drflac__decode_samples_with_residual(pFlac, pFlac->currentFrame.blockSize, pSubframe->lpcOrder, lpcShift, pSubframe->lpcCoefficients, pSubframe->pDecodedSamples)) {
        return false;
    }

    return true;
}


static bool drflac__read_next_frame_header(drflac* pFlac)
{
    assert(pFlac != NULL);
    assert(pFlac->onRead != NULL);

    // At the moment the sync code is as a form of basic validation. The CRC is stored, but is unused at the moment. This
    // should probably be handled better in the future.

    const int sampleRateTable[12]   = {0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
    const int bitsPerSampleTable[8] = {0, 8, 12, -1, 16, 20, 24, -1};   // -1 = reserved.

    unsigned short syncCode = 0;
    if (!drflac__read_uint16(pFlac, 14, &syncCode)) {
        return false;
    }

    if (syncCode != 0x3FFE) {
        // TODO: Try and recover by attempting to seek to and read the next frame?
        return false;
    }

    unsigned char reserved;
    if (!drflac__read_uint8(pFlac, 1, &reserved)) {
        return false;
    }

    unsigned char blockingStrategy = 0;
    if (!drflac__read_uint8(pFlac, 1, &blockingStrategy)) {
        return false;
    }



    unsigned char blockSize = 0;
    if (!drflac__read_uint8(pFlac, 4, &blockSize)) {
        return false;
    }

    unsigned char sampleRate = 0;
    if (!drflac__read_uint8(pFlac, 4, &sampleRate)) {
        return false;
    }

    unsigned char channelAssignment = 0;
    if (!drflac__read_uint8(pFlac, 4, &channelAssignment)) {
        return false;
    }

    unsigned char bitsPerSample = 0;
    if (!drflac__read_uint8(pFlac, 3, &bitsPerSample)) {
        return false;
    }

    if (!drflac__read_uint8(pFlac, 1, &reserved)) {
        return false;
    }


    unsigned char isVariableBlockSize = blockingStrategy == 1;
    if (isVariableBlockSize) {
        pFlac->currentFrame.frameNumber = 0;
        if (!drflac__read_utf8_coded_number(pFlac, &pFlac->currentFrame.sampleNumber)) {
            return false;
        }
    } else {
        unsigned long long frameNumber = 0;
        if (!drflac__read_utf8_coded_number(pFlac, &frameNumber)) {
            return false;
        }
        pFlac->currentFrame.frameNumber  = (unsigned int)frameNumber;   // <-- Safe cast.
        pFlac->currentFrame.sampleNumber = 0;
    }


    if (blockSize == 1) {
        pFlac->currentFrame.blockSize = 192;
    } else if (blockSize >= 2 && blockSize <= 5) {
        pFlac->currentFrame.blockSize = 576 * (1 << (blockSize - 2));
    } else if (blockSize == 6) {
        if (!drflac__read_uint16(pFlac, 8, &pFlac->currentFrame.blockSize)) {
            return false;
        }
        pFlac->currentFrame.blockSize += 1;
    } else if (blockSize == 7) {
        if (!drflac__read_uint16(pFlac, 16, &pFlac->currentFrame.blockSize)) {
            return false;
        }
        pFlac->currentFrame.blockSize += 1;
    } else {
        pFlac->currentFrame.blockSize = 256 * (1 << (blockSize - 8));
    }
    

    if (sampleRate >= 0 && sampleRate <= 11) {
        pFlac->currentFrame.sampleRate = sampleRateTable[sampleRate];
    } else if (sampleRate == 12) {
        if (!drflac__read_uint32(pFlac, 8, &pFlac->currentFrame.sampleRate)) {
            return false;
        }
        pFlac->currentFrame.sampleRate *= 1000;
    } else if (sampleRate == 13) {
        if (!drflac__read_uint32(pFlac, 16, &pFlac->currentFrame.sampleRate)) {
            return false;
        }
    } else if (sampleRate == 14) {
        if (!drflac__read_uint32(pFlac, 16, &pFlac->currentFrame.sampleRate)) {
            return false;
        }
        pFlac->currentFrame.sampleRate *= 10;
    } else {
        return false;  // Invalid.
    }
    

    pFlac->currentFrame.channelAssignment = channelAssignment;
    pFlac->currentFrame.bitsPerSample     = bitsPerSampleTable[bitsPerSample];

    if (drflac__read_uint8(pFlac, 8, &pFlac->currentFrame.crc8) != 1) {
        return false;
    }

    memset(pFlac->currentFrame.subframes, 0, sizeof(pFlac->currentFrame.subframes));

    return true;
}

static bool drflac__read_subframe_header(drflac* pFlac, drflac_subframe* pSubframe)
{
    bool result = false;

    unsigned char header;
    if (!drflac__read_uint8(pFlac, 8, &header)) {
        return false;
    }

    // First bit should always be 0.
    if ((header & 0x80) != 0) {
        return false;
    }

    int type = (header & 0x7E) >> 1;
    if (type == 0) {
        pSubframe->subframeType = DRFLAC_SUBFRAME_CONSTANT;
    } else if (type == 1) {
        pSubframe->subframeType = DRFLAC_SUBFRAME_VERBATIM;
    } else {
        if ((type & 0x20) != 0) {
            pSubframe->subframeType = DRFLAC_SUBFRAME_LPC;
            pSubframe->lpcOrder = (type & 0x1F) + 1;
        } else if ((type & 0x08) != 0) {
            pSubframe->subframeType = DRFLAC_SUBFRAME_FIXED;
            pSubframe->lpcOrder = (type & 0x07);
            if (pSubframe->lpcOrder > 4) {
                pSubframe->subframeType = DRFLAC_SUBFRAME_RESERVED;
                pSubframe->lpcOrder = 0;
            }
        } else {
            pSubframe->subframeType = DRFLAC_SUBFRAME_RESERVED;
        }
    }

    if (pSubframe->subframeType == DRFLAC_SUBFRAME_RESERVED) {
        return false;
    }

    // Wasted bits per sample.
    pSubframe->wastedBitsPerSample = 0;
    if ((header & 0x01) == 1) {
        do {
            pSubframe->wastedBitsPerSample += 1;
        } while (drflac__read_next_bit(pFlac) == 0);
    }

    return true;
}

static bool drflac__decode_subframe(drflac* pFlac, int subframeIndex)
{
    assert(pFlac != NULL);

    drflac_subframe* pSubframe = pFlac->currentFrame.subframes + subframeIndex;
    if (!drflac__read_subframe_header(pFlac, pSubframe)) {
        return false;
    }

    // Side channels require an extra bit per sample. Took a while to figure that one out...
    pSubframe->bitsPerSample = pFlac->currentFrame.bitsPerSample;
    if ((pFlac->currentFrame.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE || pFlac->currentFrame.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE) && subframeIndex == 1) {
        pSubframe->bitsPerSample += 1;
    } else if (pFlac->currentFrame.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE && subframeIndex == 0) {
        pSubframe->bitsPerSample += 1;
    }

    // Need to handle wasted bits per sample.
    pSubframe->bitsPerSample -= pSubframe->wastedBitsPerSample;
    pSubframe->pDecodedSamples = ((int*)pFlac->pHeap) + (pFlac->currentFrame.blockSize * subframeIndex);

    switch (pSubframe->subframeType)
    {
        case DRFLAC_SUBFRAME_CONSTANT:
        {
            drflac__decode_samples__constant(pFlac, pSubframe);
        } break;

        case DRFLAC_SUBFRAME_VERBATIM:
        {
            drflac__decode_samples__verbatim(pFlac, pSubframe);
        } break;

        case DRFLAC_SUBFRAME_FIXED:
        {
            drflac__decode_samples__fixed(pFlac, pSubframe);
        } break;

        case DRFLAC_SUBFRAME_LPC:
        {
            drflac__decode_samples__lcp(pFlac, pSubframe);
        } break;

        default: return false;
    }

    return true;
}

static bool drflac__seek_subframe(drflac* pFlac, int subframeIndex)
{
    assert(pFlac != NULL);

    drflac_subframe* pSubframe = pFlac->currentFrame.subframes + subframeIndex;
    if (!drflac__read_subframe_header(pFlac, pSubframe)) {
        return false;
    }

    // Side channels require an extra bit per sample. Took a while to figure that one out...
    pSubframe->bitsPerSample = pFlac->currentFrame.bitsPerSample;
    if ((pFlac->currentFrame.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE || pFlac->currentFrame.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE) && subframeIndex == 1) {
        pSubframe->bitsPerSample += 1;
    } else if (pFlac->currentFrame.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE && subframeIndex == 0) {
        pSubframe->bitsPerSample += 1;
    }

    // Need to handle wasted bits per sample.
    pSubframe->bitsPerSample -= pSubframe->wastedBitsPerSample;
    pSubframe->pDecodedSamples = ((int*)pFlac->pHeap) + (pFlac->currentFrame.blockSize * subframeIndex);

    switch (pSubframe->subframeType)
    {
        case DRFLAC_SUBFRAME_CONSTANT:
        {
            if (drflac__seek_bits(pFlac, pSubframe->bitsPerSample) != pSubframe->bitsPerSample) {
                return false;
            }
        } break;

        case DRFLAC_SUBFRAME_VERBATIM:
        {
            unsigned int bitsToSeek = pFlac->currentFrame.blockSize * pSubframe->bitsPerSample;
            if (drflac__seek_bits(pFlac, bitsToSeek) != pFlac->currentFrame.blockSize) {
                return false;
            }
        } break;

        case DRFLAC_SUBFRAME_FIXED:
        {
            unsigned int bitsToSeek = pSubframe->lpcOrder * pSubframe->bitsPerSample;
            if (drflac__seek_bits(pFlac, bitsToSeek) != bitsToSeek) {
                return false;
            }

            if (!drflac__read_and_seek_residual(pFlac, pFlac->currentFrame.blockSize, pSubframe->lpcOrder)) {
                return false;
            }
        } break;

        case DRFLAC_SUBFRAME_LPC:
        {
            unsigned int bitsToSeek = pSubframe->lpcOrder * pSubframe->bitsPerSample;
            if (drflac__seek_bits(pFlac, bitsToSeek) != bitsToSeek) {
                return false;
            }

            unsigned char lpcPrecision;
            if (!drflac__read_uint8(pFlac, 4, &lpcPrecision)) {
                return false;
            }
            if (lpcPrecision == 15) {
                return false;    // Invalid.
            }
            lpcPrecision += 1;
            

            bitsToSeek = (pSubframe->lpcOrder * lpcPrecision) + 5;    // +5 for shift.
            if (drflac__seek_bits(pFlac, bitsToSeek) != bitsToSeek) {
                return false;
            }

            if (!drflac__read_and_seek_residual(pFlac, pFlac->currentFrame.blockSize, pSubframe->lpcOrder)) {
                return false;
            }
        } break;

        default: return false;
    }

    return true;
}


static int drflac__get_channel_count_from_channel_assignment(int channelAssignment)
{
    if (channelAssignment >= 0 && channelAssignment <= 7) {
        return channelAssignment + 1;
    }

    if (channelAssignment >= 8 && channelAssignment <= 10) {
        return 2;
    }

    return 0;
}

static bool drflac__decode_frame(drflac* pFlac)
{
    // This function should be called while the stream is sitting on the first byte after the frame header.

    int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);
    for (int i = 0; i < channelCount; ++i)
    {
        if (!drflac__decode_subframe(pFlac, i)) {
            return false;
        }
    }

    // At the end of the frame sits the padding and CRC. We don't use these so we can just seek past.
    drflac__seek_bits(pFlac, (pFlac->leftoverBitsRemaining % 8) + 16);


    pFlac->currentFrame.samplesRemaining = pFlac->currentFrame.blockSize * drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);

    return true;
}

static bool drflac__seek_frame(drflac* pFlac)
{
    int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);
    for (int i = 0; i < channelCount; ++i)
    {
        if (!drflac__seek_subframe(pFlac, i)) {
            return false;
        }
    }

    // Padding and CRC.
    drflac__seek_bits(pFlac, (pFlac->leftoverBitsRemaining % 8) + 16);

    return true;
}

static bool drflac__read_and_decode_next_frame(drflac* pFlac)      // TODO: Rename this to drflac__decode_next_frame().
{
    assert(pFlac != NULL);

    if (!drflac__read_next_frame_header(pFlac)) {
        return false;
    }

    return drflac__decode_frame(pFlac);
}

static unsigned int drflac__read_block_header(drflac* pFlac, unsigned int* pBlockSizeOut, bool* pIsLastBlockOut)
{
    assert(pFlac != NULL);

    unsigned char isLastBlock = 1;
    unsigned char blockType = DRFLAC_BLOCK_TYPE_INVALID;
    unsigned int blockSize = 0;

    if (!drflac__read_uint8(pFlac, 1, &isLastBlock)) {
        goto done_reading_block_header;
    }

    if (!drflac__read_uint8(pFlac, 7, &blockType)) {
        goto done_reading_block_header;
    }

    if (!drflac__read_uint32(pFlac, 24, &blockSize)) {
        goto done_reading_block_header;
    }


done_reading_block_header:
    if (pBlockSizeOut) {
        *pBlockSizeOut = blockSize;
    }

    if (pIsLastBlockOut) {
        *pIsLastBlockOut = isLastBlock;
    }

    return blockType;
}


static void drflac__get_current_frame_sample_range(drflac* pFlac, uint64_t* pFirstSampleInFrameOut, uint64_t* pLastSampleInFrameOut)
{
    assert(pFlac != NULL);

    unsigned int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);

    uint64_t firstSampleInFrame = pFlac->currentFrame.sampleNumber;
    if (firstSampleInFrame == 0) {
        firstSampleInFrame = pFlac->currentFrame.frameNumber * pFlac->maxBlockSize*channelCount;
    }

    uint64_t lastSampleInFrame = firstSampleInFrame + (pFlac->currentFrame.blockSize*channelCount);
    if (lastSampleInFrame > 0) {
        lastSampleInFrame -= 1; // Needs to be zero based.
    }


    if (pFirstSampleInFrameOut) {
        *pFirstSampleInFrameOut = firstSampleInFrame;
    }
    if (pLastSampleInFrameOut) {
        *pLastSampleInFrameOut = lastSampleInFrame;
    }
}

static bool drflac__seek_to_first_frame(drflac* pFlac)
{
    assert(pFlac != NULL);
    
    bool result = drflac__seek_to_byte(pFlac, (long long)pFlac->firstFramePos);
    pFlac->leftoverBitsRemaining = 0;
    pFlac->leftoverBytes = 0;

    memset(&pFlac->currentFrame, 0, sizeof(pFlac->currentFrame));

    return result;
}

static bool drflac__seek_to_next_frame(drflac* pFlac)
{
    // This function should only ever be called while the decoder is sitting on the first byte past the FRAME_HEADER section.
    assert(pFlac != NULL);
    return drflac__seek_frame(pFlac);
}

static bool drflac__seek_to_frame_containing_sample(drflac* pFlac, uint64_t sampleIndex)
{
    assert(pFlac != NULL);

    if (!drflac__seek_to_first_frame(pFlac)) {
        return false;
    }

    uint64_t firstSampleInFrame = 0;
    uint64_t lastSampleInFrame = 0;
    for (;;)
    {
        // We need to read the frame's header in order to determine the range of samples it contains.
        if (!drflac__read_next_frame_header(pFlac)) {
            return false;
        }

        drflac__get_current_frame_sample_range(pFlac, &firstSampleInFrame, &lastSampleInFrame);
        if (sampleIndex >= firstSampleInFrame && sampleIndex <= lastSampleInFrame) {
            break;  // The sample is in this frame.
        }

        if (!drflac__seek_to_next_frame(pFlac)) {
            return false;
        }
    }

    // If we get here we should be right at the start of the frame containing the sample.
    return true;
}

static bool drflac__seek_to_sample__brute_force(drflac* pFlac, uint64_t sampleIndex)
{
    if (!drflac__seek_to_frame_containing_sample(pFlac, sampleIndex)) {
        return false;
    }

    // At this point we should be sitting on the first byte of the frame containing the sample. We need to decode every sample up to (but
    // not including) the sample we're seeking to.
    uint64_t firstSampleInFrame = 0;
    drflac__get_current_frame_sample_range(pFlac, &firstSampleInFrame, NULL);

    assert(firstSampleInFrame <= sampleIndex);
    size_t samplesToDecode = (size_t)(sampleIndex - firstSampleInFrame);    // <-- Safe cast because the maximum number of samples in a frame is 65535.
    if (samplesToDecode == 0) {
        return true;
    }

    // At this point we are just sitting on the byte after the frame header. We need to decode the frame before reading anything from it.
    if (!drflac__decode_frame(pFlac)) {
        return false;
    }

    return drflac_read_s32(pFlac, samplesToDecode, NULL);
}

static bool drflac__seek_to_sample__seek_table(drflac* pFlac, uint64_t sampleIndex)
{
    assert(pFlac != NULL);

    if (pFlac->seektableBlock.pos == 0) {
        return false;
    }

    if (!drflac__seek_to_byte(pFlac, pFlac->seektableBlock.pos)) {
        return false;
    }

    // The number of seek points is derived from the size of the SEEKTABLE block.
    unsigned int seekpointCount = pFlac->seektableBlock.sizeInBytes / 18;   // 18 = the size of each seek point.
    if (seekpointCount == 0) {
        return false;   // Would this ever happen?
    }


    drflac_seekpoint closestSeekpoint = {0};

    unsigned int seekpointsRemaining = seekpointCount;
    while (seekpointsRemaining > 0)
    {
        drflac_seekpoint seekpoint;
        drflac__read_uint64(pFlac, 64, &seekpoint.firstSample);
        drflac__read_uint64(pFlac, 64, &seekpoint.frameOffset);
        drflac__read_uint16(pFlac, 16, &seekpoint.sampleCount);

        if (seekpoint.firstSample * pFlac->channels > sampleIndex) {
            break;
        }

        closestSeekpoint = seekpoint;
        seekpointsRemaining -= 1;
    }

    // At this point we should have found the seekpoint closest to our sample. We need to seek to it using basically the same
    // technique as we use with the brute force method.
    drflac__seek_to_byte(pFlac, pFlac->firstFramePos + closestSeekpoint.frameOffset);

    uint64_t firstSampleInFrame = 0;
    uint64_t lastSampleInFrame = 0;
    for (;;)
    {
        // We need to read the frame's header in order to determine the range of samples it contains.
        if (!drflac__read_next_frame_header(pFlac)) {
            return false;
        }

        drflac__get_current_frame_sample_range(pFlac, &firstSampleInFrame, &lastSampleInFrame);
        if (sampleIndex >= firstSampleInFrame && sampleIndex <= lastSampleInFrame) {
            break;  // The sample is in this frame.
        }

        if (!drflac__seek_to_next_frame(pFlac)) {
            return false;
        }
    }

    // At this point we should be sitting on the first byte of the frame containing the sample. We need to decode every sample up to (but
    // not including) the sample we're seeking to.
    assert(firstSampleInFrame <= sampleIndex);
    size_t samplesToDecode = (size_t)(sampleIndex - firstSampleInFrame);    // <-- Safe cast because the maximum number of samples in a frame is 65535.
    if (samplesToDecode == 0) {
        return true;
    }

    // At this point we are just sitting on the byte after the frame header. We need to decode the frame before reading anything from it.
    if (!drflac__decode_frame(pFlac)) {
        return false;
    }

    return drflac_read_s32(pFlac, samplesToDecode, NULL);
}


bool drflac_open(drflac* pFlac, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_tell_proc onTell, void* userData)
{
    if (pFlac == NULL || onRead == NULL || onSeek == NULL || onTell == NULL) {
        return false;
    }

    unsigned char id[4];
    if (onRead(userData, id, 4) != 4 || id[0] != 'f' || id[1] != 'L' || id[2] != 'a' || id[3] != 'C') {
        return false;    // Not a FLAC stream.
    }

    memset(pFlac, 0, sizeof(*pFlac));
    pFlac->onRead         = onRead;
    pFlac->onSeek         = onSeek;
    pFlac->onTell         = onTell;
    pFlac->userData       = userData;
    pFlac->currentBytePos = 4;   // Set to 4 because we just read the ID which is 4 bytes.
    
    
    // The first metadata block should be the STREAMINFO block. We don't care about everything in here.
    unsigned int blockSize;
    bool isLastBlock;
    int blockType = drflac__read_block_header(pFlac, &blockSize, &isLastBlock);
    if (blockType != DRFLAC_BLOCK_TYPE_STREAMINFO && blockSize != 34) {
        return false;
    }


    drflac__seek_bits(pFlac, 16);   // minBlockSize
    drflac__read_uint16(pFlac, 16, &pFlac->maxBlockSize);
    drflac__seek_bits(pFlac, 48);   // minFrameSize + maxFrameSize
    drflac__read_uint32(pFlac, 20, &pFlac->sampleRate);
    drflac__read_uint8(pFlac, 3, &pFlac->channels);
    drflac__read_uint8(pFlac, 5, &pFlac->bitsPerSample);
    drflac__read_uint64(pFlac, 36, &pFlac->totalSampleCount);
    drflac__seek_bits(pFlac, 128);  // MD5

    pFlac->channels += 1;
    pFlac->bitsPerSample += 1;
    pFlac->totalSampleCount *= pFlac->channels;

    while (!isLastBlock)
    {
        blockType = drflac__read_block_header(pFlac, &blockSize, &isLastBlock);

        switch (blockType)
        {
            case DRFLAC_BLOCK_TYPE_APPLICATION:
            {
                pFlac->applicationBlock.pos = drflac__tell(pFlac);
                pFlac->applicationBlock.sizeInBytes = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_SEEKTABLE:
            {
                pFlac->seektableBlock.pos = drflac__tell(pFlac);
                pFlac->seektableBlock.sizeInBytes = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_VORBIS_COMMENT:
            {
                pFlac->vorbisCommentBlock.pos = drflac__tell(pFlac);
                pFlac->vorbisCommentBlock.sizeInBytes = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_CUESHEET:
            {
                pFlac->cuesheetBlock.pos = drflac__tell(pFlac);
                pFlac->cuesheetBlock.sizeInBytes = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_PICTURE:
            {
                pFlac->pictureBlock.pos = drflac__tell(pFlac);
                pFlac->pictureBlock.sizeInBytes = blockSize;
            } break;

            
            // These blocks we either don't care about or aren't supporting.
            case DRFLAC_BLOCK_TYPE_PADDING:
            case DRFLAC_BLOCK_TYPE_INVALID:
            default: break;
        }

        drflac__seek_bits(pFlac, blockSize*8);
    }

    
    // At this point we should be sitting right at the start of the very first frame.
    pFlac->firstFramePos = drflac__tell(pFlac);

    // TODO: Make the heap optional.
    //if (isUsingHeap)
    {
        // The size of the heap is determined by the maximum number of samples in each frame. This is calculated from the maximum block
        // size multiplied by the channel count. We need a single signed 32-bit integer for each sample which is used to stored the
        // decoded residual of each sample.
        pFlac->pHeap = malloc(pFlac->maxBlockSize * pFlac->channels * sizeof(int32_t));
    }

    return true;
}

void drflac_close(drflac* pFlac)
{
    if (pFlac == NULL) {
        return;
    }

#ifndef DR_FLAC_NO_STDIO
    // If we opened the file with drflac_open_file() we will want to close the file handle. We can know whether or not drflac_open_file()
    // was used by looking at the callbacks.
    if (pFlac->onRead == drflac__on_read_stdio) {
        fclose((FILE*)pFlac->userData);
    }
#endif

    // If we opened the file with drflac_open_memory() we will want to free() the user data.
    if (pFlac->onRead == drflac__on_read_memory) {
        free(pFlac->userData);
    }

    free(pFlac->pHeap);
}

size_t drflac__read_s32__misaligned(drflac* pFlac, size_t samplesToRead, int* bufferOut)
{
    unsigned int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);
    
    // We should never be calling this when the number of samples to read is >= the sample count.
    assert(samplesToRead < channelCount);
    assert(pFlac->currentFrame.samplesRemaining > 0 && samplesToRead <= pFlac->currentFrame.samplesRemaining);


    size_t samplesRead = 0;
    while (samplesToRead > 0)
    {
        uint64_t totalSamplesInFrame = pFlac->currentFrame.blockSize * channelCount;
        uint64_t samplesReadFromFrameSoFar = totalSamplesInFrame - pFlac->currentFrame.samplesRemaining;
        unsigned int channelIndex = samplesReadFromFrameSoFar % channelCount;

        unsigned long long nextSampleInFrame = samplesReadFromFrameSoFar / channelCount;

        int decodedSample = 0;
        switch (pFlac->currentFrame.channelAssignment)
        {
            case DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE:
            {
                if (channelIndex == 0) {
                    decodedSample = pFlac->currentFrame.subframes[channelIndex].pDecodedSamples[nextSampleInFrame];
                } else {
                    int side = pFlac->currentFrame.subframes[channelIndex + 0].pDecodedSamples[nextSampleInFrame];
                    int left = pFlac->currentFrame.subframes[channelIndex - 1].pDecodedSamples[nextSampleInFrame];
                    decodedSample = left - side;
                }

            } break;

            case DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE:
            {
                if (channelIndex == 0) {
                    int side  = pFlac->currentFrame.subframes[channelIndex + 0].pDecodedSamples[nextSampleInFrame];
                    int right = pFlac->currentFrame.subframes[channelIndex + 1].pDecodedSamples[nextSampleInFrame];
                    decodedSample = side + right;
                } else {
                    decodedSample = pFlac->currentFrame.subframes[channelIndex].pDecodedSamples[nextSampleInFrame];
                }

            } break;

            case DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE:
            {
                int mid;
                int side;
                if (channelIndex == 0) {
                    mid  = pFlac->currentFrame.subframes[channelIndex + 0].pDecodedSamples[nextSampleInFrame];
                    side = pFlac->currentFrame.subframes[channelIndex + 1].pDecodedSamples[nextSampleInFrame];

                    mid = (((unsigned int)mid) << 1) | (side & 0x01);
                    decodedSample = (mid + side) >> 1;
                } else {
                    mid  = pFlac->currentFrame.subframes[channelIndex - 1].pDecodedSamples[nextSampleInFrame];
                    side = pFlac->currentFrame.subframes[channelIndex + 0].pDecodedSamples[nextSampleInFrame];

                    mid = (((unsigned int)mid) << 1) | (side & 0x01);
                    decodedSample = (mid - side) >> 1;
                }
                        
            } break;

            case DRFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT:
            default:
            {
                decodedSample = pFlac->currentFrame.subframes[channelIndex].pDecodedSamples[nextSampleInFrame];
            } break;
        }


        decodedSample <<= (32 - pFlac->bitsPerSample);

        if (bufferOut) {
            *bufferOut++ = decodedSample;
        }

        samplesRead += 1;
        pFlac->currentFrame.samplesRemaining -= 1;
        samplesToRead -= 1;
    }

    return samplesRead;
}

size_t drflac__seek_forward_by_samples(drflac* pFlac, size_t samplesToRead)
{
    size_t samplesRead = 0;
    while (samplesToRead > 0)
    {
        if (pFlac->currentFrame.samplesRemaining == 0)
        {
            if (!drflac__read_and_decode_next_frame(pFlac)) {
                break;  // Couldn't read the next frame, so just break from the loop and return.
            }
        }
        else
        {
            samplesRead += 1;
            pFlac->currentFrame.samplesRemaining -= 1;
            samplesToRead -= 1;
        }
    }

    return samplesRead;
}

size_t drflac_read_s32(drflac* pFlac, size_t samplesToRead, int* bufferOut)
{
    // Note that <bufferOut> is allowed to be null, in which case this will be treated as something like a seek.
    if (pFlac == NULL || samplesToRead == 0) {
        return 0;
    }

    if (bufferOut == NULL) {
        return drflac__seek_forward_by_samples(pFlac, samplesToRead);
    }


    size_t samplesRead = 0;
    while (samplesToRead > 0)
    {
        // If we've run out of samples in this frame, go to the next.
        if (pFlac->currentFrame.samplesRemaining == 0)
        {
            if (!drflac__read_and_decode_next_frame(pFlac)) {
                break;  // Couldn't read the next frame, so just break from the loop and return.
            }
        }
        else
        {
            // Here is where we grab the samples and interleave them.

            unsigned int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);
            uint64_t totalSamplesInFrame = pFlac->currentFrame.blockSize * channelCount;
            uint64_t samplesReadFromFrameSoFar = totalSamplesInFrame - pFlac->currentFrame.samplesRemaining;

            int misalignedSampleCount = samplesReadFromFrameSoFar % channelCount;
            if (misalignedSampleCount > 0) {
                size_t misalignedSamplesRead = drflac__read_s32__misaligned(pFlac, misalignedSampleCount, bufferOut);
                samplesRead   += misalignedSamplesRead;
                samplesReadFromFrameSoFar += misalignedSamplesRead;
                bufferOut     += misalignedSamplesRead;
                samplesToRead -= misalignedSamplesRead;
            }


            size_t alignedSampleCountPerChannel = samplesToRead / channelCount;
            if (alignedSampleCountPerChannel > pFlac->currentFrame.samplesRemaining / channelCount) {
                alignedSampleCountPerChannel = pFlac->currentFrame.samplesRemaining / channelCount;
            }

            uint64_t firstAlignedSampleInFrame = samplesReadFromFrameSoFar / channelCount;
            unsigned int unusedBitsPerSample = 32 - pFlac->bitsPerSample;

            switch (pFlac->currentFrame.channelAssignment)
            {
                case DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE:
                {
                    const int* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const int* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    unsigned int unusedBitsPerSample0 = unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample;
                    unsigned int unusedBitsPerSample1 = unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample;

                    for (size_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int left  = pDecodedSamples0[i];
                        int side  = pDecodedSamples1[i];
                        int right = left - side;

                        bufferOut[i*2+0] = left  << unusedBitsPerSample0;
                        bufferOut[i*2+1] = right << unusedBitsPerSample1;
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                {
                    const int* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const int* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    unsigned int unusedBitsPerSample0 = unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample;
                    unsigned int unusedBitsPerSample1 = unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample;

                    for (size_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int side  = pDecodedSamples0[i];
                        int right = pDecodedSamples1[i];
                        int left  = right + side;

                        bufferOut[i*2+0] = left  << unusedBitsPerSample0;
                        bufferOut[i*2+1] = right << unusedBitsPerSample1;
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE:
                {
                    const int* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const int* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    unsigned int unusedBitsPerSample0 = unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample;
                    unsigned int unusedBitsPerSample1 = unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample;

                    for (size_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int side = pDecodedSamples1[i];
                        int mid  = (((uint32_t)pDecodedSamples0[i]) << 1) | (side & 0x01);

                        bufferOut[i*2+0] = ((mid + side) >> 1) << unusedBitsPerSample0;
                        bufferOut[i*2+1] = ((mid - side) >> 1) << unusedBitsPerSample1;
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT:
                default:
                {
                    if (pFlac->currentFrame.channelAssignment == 1) // 1 = Stereo
                    {
                        // Stereo optimized inner loop unroll.
                        const int* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                        const int* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                        unsigned int unusedBitsPerSample0 = unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample;
                        unsigned int unusedBitsPerSample1 = unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample;

                        for (size_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                            bufferOut[i*2+0] = pDecodedSamples0[i] << unusedBitsPerSample0;
                            bufferOut[i*2+1] = pDecodedSamples1[i] << unusedBitsPerSample1;
                        }
                    }
                    else
                    {
                        // Generic interleaving.
                        for (size_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                            for (unsigned int j = 0; j < channelCount; ++j) {
                                bufferOut[(i*channelCount)+j] = (pFlac->currentFrame.subframes[j].pDecodedSamples[firstAlignedSampleInFrame + i]) << unusedBitsPerSample;
                            }
                        }
                    }
                } break;
            }

            size_t alignedSamplesRead = alignedSampleCountPerChannel * channelCount;
            samplesRead   += alignedSamplesRead;
            samplesReadFromFrameSoFar += alignedSamplesRead;
            bufferOut     += alignedSamplesRead;
            samplesToRead -= alignedSamplesRead;
            pFlac->currentFrame.samplesRemaining -= (unsigned int)alignedSamplesRead;
            


            // At this point we may still have some excess samples left to read.
            if (samplesToRead > 0 && pFlac->currentFrame.samplesRemaining > 0)
            {
                size_t excessSamplesRead = 0;
                if (samplesToRead < pFlac->currentFrame.samplesRemaining) {
                    excessSamplesRead = drflac__read_s32__misaligned(pFlac, samplesToRead, bufferOut);
                } else {
                    excessSamplesRead = drflac__read_s32__misaligned(pFlac, pFlac->currentFrame.samplesRemaining, bufferOut);
                }

                samplesRead   += excessSamplesRead;
                samplesReadFromFrameSoFar += excessSamplesRead;
                bufferOut     += excessSamplesRead;
                samplesToRead -= excessSamplesRead; 
            }
        }
    }

    return samplesRead;
}

bool drflac_seek_to_sample(drflac* pFlac, uint64_t sampleIndex)
{
    if (pFlac == NULL) {
        return false;
    }

    if (sampleIndex == 0) {
        return drflac__seek_to_first_frame(pFlac);
    }

    // Clamp the sample to the end.
    if (sampleIndex >= pFlac->totalSampleCount) {
        sampleIndex  = pFlac->totalSampleCount - 1;
    }


    // First try seeking via the seek table. If this fails, fall back to a brute force seek which is much slower.
    if (!drflac__seek_to_sample__seek_table(pFlac, sampleIndex)) {
        return drflac__seek_to_sample__brute_force(pFlac, sampleIndex);
    }

    return true;
}


#endif  //DR_FLAC_IMPLEMENTATION


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
