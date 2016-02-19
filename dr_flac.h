// Public domain. See "unlicense" statement at the end of this file.

// QUICK NOTES
//
// - This has not been tested on big-endian architectures.
// - dr_flac is not thread-safe, but it's APIs can be called from any thread so long as the application does it's own thread-safety.
// - dr_flac does not currently do any CRC checks.

#ifndef dr_flac_h
#define dr_flac_h

#include <stddef.h>

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
    int subframeType;
    int wastedBitsPerSample;

    int lpcOrder;   // Used by both SUBFRAME_FIXED and SUBFRAME_LPC

    // The offset of the first decoded sample in the decoded sample buffer.
    unsigned int firstDecodedSampleOffset;

    // The number of bits per sample for this subframe. This is not always equal to the current frame's bit per sample because
    // side channels for when Interchannel Decorrelation is being used require an extra bit per sample.
    int bitsPerSample;

} drflac_subframe;

typedef struct
{
    // If the stream uses fixed block sizes, this will be set to the frame number. If variable block sizes are used, this will always be 0.
    unsigned long long frameNumber;

    // If the stream uses variable block sizes, this will be set to the index of the first sample. If fixed block sizes are used, this will
    // always be set to 0.
    unsigned long long sampleNumber;

    // The number of samples in each sub-frame within this frame.
    unsigned int blockSize;

    // The sample rate of this frame.
    unsigned int sampleRate;

    // The channel assignment of this frame. This is not always set to the channel count. If interchannel decorrelation is being used this
    // will be set to DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE, DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE or DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE.
    unsigned int channelAssignment;

    // The number of bits per sample within this frame.
    unsigned int bitsPerSample;

    // The frame's CRC. This is set, but unused at the moment.
    unsigned char crc8;

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


    // The temporary byte containing containing the bits that have not yet been read.
    unsigned char leftoverByte;

    // The number of bits remaining in the byte the reader is currently sitting on.
    unsigned char leftoverBitsRemaining;


    // The maximum block size, in samples. This number represents the number of samples in each channel (not combined.)
    unsigned int maxBlockSize;

    // The sample rate. Will be set to something like 44100.
    unsigned int sampleRate;

    // The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc. Maxium 8. This is set based on the
    // value specified in the STREAMINFO block.
    unsigned int channels;

    // The bits per sample. Will be set to somthing like 16, 24, etc.
    unsigned int bitsPerSample;

    // The total number of samples making up the stream. This includes every channel. For example, if the stream has 2 channels,
    // with each channel having a total of 4096, this value will be set to 4096*2 = 8192.
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


    // The number of samples remaining in the current frame. We use this to determine when a new frame needs to be begun.
    long long samplesRemainingInCurrentFrame;

    // The index of the channel that the next sample belongs to. We use this for interleaving, and is never larger than the the channel count as
    // specified by currentFrameHeader.channelAssignment-1.
    int nextSampleChannel;


    // The number of valid decoded samples in <decodedSamples> (below). We use this to keep track of where to place newly decoded samples.
    unsigned int decodedSampleCount;

    // A pointer the decoded sample data. This section is allocated once at initialization time and sized based on the maximum block size.
    int decodedSamples[1];

} drflac;




// Opens a FLAC decoder.
drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_tell_proc onTell, void* userData);

// Closes the given FLAC decoder.
void drflac_close(drflac* flac);

// Reads sample data from the given FLAC decoder, output as signed 32-bit PCM .
unsigned int drflac_read_s32(drflac* flac, size_t samplesToRead, int* bufferOut);




#ifndef DR_FLAC_NO_STDIO
// Opens a flac decoder from the file at the given path.
drflac* drflac_open_file(const char* pFile);
#endif


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

#define drflac_false 0
#define drflac_true  1
typedef int drflac_bool;

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

#ifndef DR_FLAC_NO_STDIO
#include <stdio.h>

static size_t drflac__on_read_stdio(void* userData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)userData);
}

static int drflac__on_seek_stdio(void* userData, int offset)
{
    return fseek((FILE*)userData, offset, SEEK_CUR) == 0;
}

static long long drflac__on_tell_stdio(void* userData)
{
    return ftell((FILE*)userData);
}

drflac* drflac_open_file(const char* filename)
{
    FILE* pFile;
#ifdef _MSC_VER
    if (fopen_s(&pFile, filename, "rb") != 0) {
        return NULL;
    }
#else
    pFile = fopen(filename, "rb");
    if (pFile == NULL) {
        return NULL;
    }
#endif

    return drflac_open(drflac__on_read_stdio, drflac__on_seek_stdio, drflac__on_tell_stdio, pFile);
}
#endif  //DR_FLAC_NO_STDIO


static inline drflac_bool drflac__is_little_endian()
{
    int n = 1;
    return (*(char*)&n) == 1;
}

static inline unsigned short drflac__swap_endian_uint16(unsigned short n)
{
#ifdef _MSC_VER
    return _byteswap_ushort(n);
#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC__ >= 3))
    return __builtin_bswap16(n);
#else
    return ((n & 0xFF00) >> 8) | ((n & 0x00FF) << 8);
#endif
}

static inline unsigned int drflac__swap_endian_uint32(unsigned int n)
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

static inline unsigned long long drflac__swap_endian_uint64(unsigned long long n)
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

static inline unsigned short drflac__be2host_16(unsigned short n)
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

static inline unsigned int drflac__be2host_32(unsigned int n)
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

static inline unsigned long long drflac__be2host_64(unsigned long long n)
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


static inline unsigned char drflac__extract_byte(const unsigned char* pIn, unsigned int bitOffsetIn)
{
    if (bitOffsetIn == 0) {
        return pIn[0];
    }

    unsigned char hiMask = 0xFF << (8 - bitOffsetIn);
    unsigned char loMask = ~hiMask;

    return ((pIn[0] & loMask) << bitOffsetIn) | ((pIn[1] & hiMask) >> (8 - bitOffsetIn));
}

static inline void drflac__copy_byte(unsigned char b, unsigned char* pOut, unsigned int bitOffsetOut)
{
    if (bitOffsetOut == 0) {
        pOut[0] = b;
        return;
    }

    unsigned char hiMaskOut = 0xFF << (8 - bitOffsetOut);
    unsigned char loMaskOut = ~hiMaskOut;

    unsigned char hiMaskIn  = 0xFF << (bitOffsetOut);
    unsigned char loMaskIn  = ~hiMaskIn;
    
    pOut[0] = (pOut[0] & hiMaskOut) | ((b & hiMaskIn) >> bitOffsetOut);
    pOut[1] = (pOut[1] & loMaskOut) | ((b & loMaskIn) << (8 - bitOffsetOut));
}

static inline void drflac__copy_bits(unsigned int bitCount, const unsigned char* pIn, unsigned int bitOffsetIn, unsigned char* pOut, unsigned int bitOffsetOut)
{
    assert(bitCount > 0);
    assert(pIn != NULL);
    assert(bitOffsetIn < 8);
    assert(pOut != NULL);

    int leadingBytes = bitOffsetOut / 8;
    if (leadingBytes > 0) {
        pOut += leadingBytes;
        bitOffsetOut -= (leadingBytes * 8);
    }

    leadingBytes = bitOffsetIn / 8;
    if (leadingBytes > 0) {
        pIn += leadingBytes;
        bitOffsetIn -= (leadingBytes * 8);
    }


    // Whole bytes.
    while (bitCount >= 8)
    {
        drflac__copy_byte(drflac__extract_byte(pIn, bitOffsetIn), pOut, bitOffsetOut);

        pIn  += 1;
        pOut += 1;

        bitCount -= 8;
    }

    // Left over bits.
    if (bitCount > 0) {
        assert(bitCount < 8);

        unsigned char src = (pIn[0] << bitOffsetIn);
        if (bitCount > 8 - bitOffsetIn) {
            int excessBits = bitCount - (8 - bitOffsetIn);
            src |= (pIn[1] & ~(0xFF >> excessBits)) >> (8 - bitOffsetIn);
        }
        
        if (bitCount <= 8 - bitOffsetOut) {
            unsigned char srcMask = (0xFF << (8 - bitCount));
            unsigned char dstMask = ~(0xFF >> bitOffsetOut) | ~(0xFF << (8 - bitCount - bitOffsetOut));
            pOut[0] = (pOut[0] & dstMask) | ((src & srcMask) >> bitOffsetOut);
        } else {
            // Split over 2 bytes.
            unsigned char hiMaskOut = 0xFF << (8 - bitOffsetOut);
            unsigned char loMaskOut = 0xFF >> (bitCount - (8 - bitOffsetOut));

            unsigned char hiMaskIn  = 0xFF << bitOffsetOut;
            unsigned char loMaskIn  = ~hiMaskIn & ~(0xFF >> bitCount);
    
            pOut[0] = (pOut[0] & hiMaskOut) | ((src & hiMaskIn) >> bitOffsetOut);
            pOut[1] = (pOut[1] & loMaskOut) | ((src & loMaskIn) << (8 - bitOffsetOut));
        }
    }
}

static inline size_t drflac__grab_bytes(drflac* pFlac, void* pOut, size_t bytesToRead)
{
    assert(pFlac != NULL);
    assert(pOut != NULL);
    assert(bytesToRead > 0);

    // This function grabs bytes directly from the stream. We should never be calling this while not being aligned to
    // a byte boundary. To help detect potential hard-to-find errors we'll place an assert here to ensure we only ever
    // call this while aligned.
    assert(pFlac->leftoverBitsRemaining == 0);

    return pFlac->onRead(pFlac->userData, pOut, bytesToRead);
}

static inline int drflac__seek_to_byte(drflac* pFlac, long long offsetFromStart)
{
    assert(pFlac != NULL);

    long long bytesToMove = offsetFromStart - pFlac->onTell(pFlac->userData);
    if (bytesToMove == 0) {
        return 1;
    }

    if (bytesToMove > 0x7FFFFFFF) {
        while (bytesToMove > 0x7FFFFFFF) {
            if (!pFlac->onSeek(pFlac->userData, 0x7FFFFFFF)) {
                return 0;
            }

            bytesToMove -= 0x7FFFFFFF;
        }
    } else {
        while (bytesToMove < (int)0x80000000) {
            if (!pFlac->onSeek(pFlac->userData, (int)0x80000000)) {
                return 0;
            }

            bytesToMove -= (int)0x80000000;
        }
    }

    assert(bytesToMove <= 0x7FFFFFFF && bytesToMove >= (int)0x80000000);

    return pFlac->onSeek(pFlac->userData, (int)bytesToMove);    // <-- Safe cast as per the assert above.
}

static unsigned int drflac__read_bits(drflac* pFlac, unsigned int bitsToRead, unsigned char* pOut, unsigned int bitOffsetOut)
{
    assert(pFlac != NULL);
    assert(pOut != NULL);
    assert(pFlac->leftoverBitsRemaining <= 7);

    // Optimization for byte-aligned reads.
    if (pFlac->leftoverBitsRemaining == 0 && bitOffsetOut == 0 && (bitsToRead % 8) == 0) {
        return drflac__grab_bytes(pFlac, pOut, bitsToRead / 8) * 8;
    }

    unsigned int bitsRead = 0;

    // Leftover bits
    if (pFlac->leftoverBitsRemaining > 0) {
        if (bitsToRead >= pFlac->leftoverBitsRemaining) {
            drflac__copy_bits(pFlac->leftoverBitsRemaining, &pFlac->leftoverByte, 8 - pFlac->leftoverBitsRemaining, pOut, bitOffsetOut);
            bitsRead = pFlac->leftoverBitsRemaining;
            pFlac->leftoverBitsRemaining = 0;
        } else {
            // The whole data is entirely contained within the leftover bits.
            drflac__copy_bits(bitsToRead, &pFlac->leftoverByte, 8 - pFlac->leftoverBitsRemaining, pOut, bitOffsetOut);
            pFlac->leftoverBitsRemaining -= bitsToRead;
            return bitsToRead;
        }
    }

    assert(pFlac->leftoverBitsRemaining == 0);

    unsigned int bitOffset = bitsRead + bitOffsetOut;

    // Wholy contained bytes.
    size_t bytesToRead = (bitsToRead - bitsRead) / 8;
    if (bytesToRead > 0)
    {
        if (bitOffset % 8 == 0)
        {
            // Aligned read. Faster.
            pOut += (bitOffset >> 3);

            size_t bytesRead = drflac__grab_bytes(pFlac, pOut, bytesToRead);
            bitsRead += bytesRead * 8;

            if (bytesRead != bytesToRead) { // <-- did we run out of data?
                return bitsRead;
            }

            pOut += bytesRead;
        }
        else
        {
            // Unaligned read. Slower.
            while (bytesToRead > 0)
            {
                unsigned char nextByte;
                if (drflac__grab_bytes(pFlac, &nextByte, 1) != 1) {
                    return bitsRead;
                }

                drflac__copy_bits(8, &nextByte, 0, pOut, bitOffset);

                bytesToRead -= 1;
                bitsRead += 8;
                pOut += 1;
            }
        }
    }

    // Trailing bits.
    unsigned int bitsRemaining = (bitsToRead - bitsRead);
    if (bitsRemaining > 0)
    {
        assert(bitsRemaining < 8);

        if (drflac__grab_bytes(pFlac, &pFlac->leftoverByte, 1) != 1) {
            return bitsRead;    // Ran out of data.
        }

        bitsRead += bitsRemaining;

        drflac__copy_bits(bitsRemaining, &pFlac->leftoverByte, 0, pOut, bitOffset);
        pFlac->leftoverBitsRemaining = 8 - bitsRemaining;
    }

    return bitsRead;
}

static inline int drflac__read_next_bit(drflac* pFlac)
{
    assert(pFlac != NULL);

    if (pFlac->leftoverBitsRemaining == 0) {
        if (drflac__grab_bytes(pFlac, &pFlac->leftoverByte, 1) != 1) {
            return -1;  // Ran out of data.
        }

        pFlac->leftoverBitsRemaining = 8;
    }

    pFlac->leftoverBitsRemaining -= 1;
    return (pFlac->leftoverByte & (1 << pFlac->leftoverBitsRemaining)) >> pFlac->leftoverBitsRemaining;
}

static inline drflac_bool drflac__read_uint16(drflac* pFlac, unsigned int bitCount, unsigned short* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 16);

    unsigned int result = 0;
    if (drflac__read_bits(pFlac, bitCount, (unsigned char*)&result, 16 - bitCount) != bitCount) {
        return drflac_false;
    }

    *pResult = drflac__be2host_16(result);
    return drflac_true;
}

static inline drflac_bool drflac__read_uint32(drflac* pFlac, unsigned int bitCount, unsigned int* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 32);

    unsigned int result = 0;
    if (drflac__read_bits(pFlac, bitCount, (unsigned char*)&result, 32 - bitCount) != bitCount) {
        return drflac_false;
    }

    *pResult = drflac__be2host_32(result);
    return drflac_true;
}

static inline drflac_bool drflac__read_uint64(drflac* pFlac, unsigned int bitCount, unsigned long long* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 64);

    unsigned long long result = 0;
    if (drflac__read_bits(pFlac, bitCount, (unsigned char*)&result, 64 - bitCount) != bitCount) {
        return drflac_false;
    }

    *pResult = drflac__be2host_64(result);
    return drflac_true;
}




static drflac_bool drflac__read_utf8_coded_number(drflac* pFlac, long long* pNumberOut)
{
    assert(pFlac != NULL);
    assert(pNumberOut != NULL);

    // We should never need to read UTF-8 data while not being aligned to a byte boundary. Therefore we can grab the data
    // directly from the input stream rather than using drflac__read_bits().
    assert(pFlac->leftoverBitsRemaining == 0);

    unsigned char utf8[7] = {0};
    if (drflac__grab_bytes(pFlac, utf8, 1) != 1) {
        *pNumberOut = 0;
        return drflac_false;
    }

    if ((utf8[0] & 0x80) == 0) {
        *pNumberOut = utf8[0];
        return drflac_true;
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
        return drflac_false;     // Bad UTF-8 encoding.
    }

    // Read extra bytes.
    assert(byteCount > 1);

    if (drflac__grab_bytes(pFlac, utf8 + 1, byteCount - 1) != byteCount - 1) {
        *pNumberOut = 0;
        return drflac_false;     // Ran out of data.
    }

    // At this point we have the raw UTF-8 data and we just need to decode it.
    long long result = ((long long)(utf8[0] & (0xFF >> (byteCount + 1))));
    if (drflac__is_little_endian())
    {
        switch (byteCount)
        {
        case 7: result = (result << 6) | (utf8[6] & 0x3F);
        case 6: result = (result << 6) | (utf8[5] & 0x3F);
        case 5: result = (result << 6) | (utf8[4] & 0x3F);
        case 4: result = (result << 6) | (utf8[3] & 0x3F);
        case 3: result = (result << 6) | (utf8[2] & 0x3F);
        case 2: result = (result << 6) | (utf8[1] & 0x3F);
        default: break;
        }
    }
    else
    {
        switch (byteCount)
        {
        case 2: result = (result << 6) | (utf8[1] & 0x3F);
        case 3: result = (result << 6) | (utf8[2] & 0x3F);
        case 4: result = (result << 6) | (utf8[3] & 0x3F);
        case 5: result = (result << 6) | (utf8[4] & 0x3F);
        case 6: result = (result << 6) | (utf8[5] & 0x3F);
        case 7: result = (result << 6) | (utf8[6] & 0x3F);
        default: break;
        }
    }

    *pNumberOut = result;
    return drflac_true;
}




// Converts a stream of bits of a variable length to an unsigned integer. Assumes the input data is big-endian and the first bit
// is at an offset of bitOffsetIn.
static inline unsigned int drflac__to_uint16(unsigned char* pIn, unsigned int bitOffsetIn, unsigned int bitCount)
{
    assert(bitCount <= 16);

    unsigned int result = 0;
    drflac__copy_bits(bitCount, pIn, bitOffsetIn, (unsigned char*)&result, 16 - bitCount);

    if (drflac__is_little_endian()) {
        result = drflac__swap_endian_uint16(result);
    }

    return result;
}

static inline unsigned int drflac__to_uint32(unsigned char* pIn, unsigned int bitOffsetIn, unsigned int bitCount)
{
    assert(bitCount <= 32);

    unsigned int result = 0;
    drflac__copy_bits(bitCount, pIn, bitOffsetIn, (unsigned char*)&result, 32 - bitCount);

    if (drflac__is_little_endian()) {
        result = drflac__swap_endian_uint32(result);
    }

    return result;
}

static inline unsigned long long drflac__to_uint64(unsigned char* pIn, unsigned int bitOffsetIn, unsigned int bitCount)
{
    assert(bitCount <= 64);

    unsigned long long result = 0;
    drflac__copy_bits(bitCount, pIn, bitOffsetIn, (unsigned char*)&result, 64 - bitCount);

    if (drflac__is_little_endian()) {
        result = drflac__swap_endian_uint64(result);
    }

    return result;
}

// Converts a stream of bits of a variable length to a signed integer. Assumes the input data is big-endian and the first bit
// is at an offset of bitOffsetIn.
static inline int drflac__to_int32(unsigned char* pIn, unsigned int bitOffsetIn, unsigned int bitCount)
{
    int leadingBytes = bitOffsetIn / 8;
    if (leadingBytes > 0) {
        pIn += leadingBytes;
        bitOffsetIn -= (leadingBytes * 8);
    }

    unsigned int result = drflac__to_uint32(pIn, bitOffsetIn, bitCount);
    if ((pIn[0] & (1 << (8 - bitOffsetIn - 1)))) {
        result |= (0xFFFFFFFF << bitCount);
    }

    return (int)result;
}

static inline long long drflac__to_int64(unsigned char* pIn, unsigned int bitOffsetIn, unsigned int bitCount)
{
    int leadingBytes = bitOffsetIn / 8;
    if (leadingBytes > 0) {
        pIn += leadingBytes;
        bitOffsetIn -= (leadingBytes * 8);
    }

    unsigned long long result = drflac__to_uint64(pIn, bitOffsetIn, bitCount);
    if ((pIn[0] & (1 << (8 - bitOffsetIn - 1)))) {
        result |= (0xFFFFFFFFFFFFFFFFULL << bitCount);
    }

    return (long long)result;
}


static inline int drflac__read_and_decode_rice(drflac* pFlac, unsigned char m)
{
    // TODO: Profile and optimize this. Will probably need to look at decoding Rice codes in groups.

    unsigned int zeroCounter = 0;
    while (drflac__read_next_bit(pFlac) == 0) {
        zeroCounter += 1;
    }

    unsigned int decodedValue = 0;
    drflac__read_uint32(pFlac, m, &decodedValue);


    decodedValue |= (zeroCounter << m);
    if ((decodedValue & 0x01)) {
        decodedValue = ~(decodedValue >> 1);
    } else {
        decodedValue = (decodedValue >> 1);
    }

    return (int)decodedValue;
}


static drflac_bool drflac__read_next_frame_header(drflac* pFlac)
{
    assert(pFlac != NULL);
    assert(pFlac->onRead != NULL);

    // Frame headers should always be aligned to a byte boundary so we use drflac__grab_bytes() directly instead of the
    // less efficient drflac__read_bits().
    //
    // At the moment the sync code is as a form of basic validation. The CRC is stored, but is unused at the moment. This
    // should probably be handled better in the future.

    const int sampleRateTable[12]   = {0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
    const int bitsPerSampleTable[8] = {0, 8, 12, -1, 16, 20, 24, -1};   // -1 = reserved.

    unsigned char headerDataFixed[4];
    if (drflac__grab_bytes(pFlac, headerDataFixed, sizeof(headerDataFixed)) != sizeof(headerDataFixed)) {
        return drflac_false;
    }

    unsigned short syncCode = drflac__to_uint16((unsigned char*)&headerDataFixed, 0, 14);
    if (syncCode != 0x3FFE) {
        // TODO: Try and recover by attempting to seek to and read the next frame?
        return drflac_false;
    }


    unsigned char blockSize         = (headerDataFixed[2] & 0xF0) >> 4;
    unsigned char sampleRate        = (headerDataFixed[2] & 0x0F);
    unsigned char channelAssignment = (headerDataFixed[3] & 0xF0) >> 4;
    unsigned char bitsPerSample     = (headerDataFixed[3] & 0x0E) >> 1;


    unsigned char isVariableBlockSize = (headerDataFixed[1] & 0x01);
    if (isVariableBlockSize) {
        pFlac->currentFrame.frameNumber = 0;
        if (!drflac__read_utf8_coded_number(pFlac, &pFlac->currentFrame.sampleNumber)) {
            return drflac_false;
        }
    } else {
        if (!drflac__read_utf8_coded_number(pFlac, &pFlac->currentFrame.frameNumber)) {
            return drflac_false;
        }
        pFlac->currentFrame.sampleNumber = 0;
    }


    if (blockSize == 1) {
        pFlac->currentFrame.blockSize = 192;
    } else if (blockSize >= 2 && blockSize <= 5) {
        pFlac->currentFrame.blockSize = 576 * (1 << (blockSize - 2));
    } else if (blockSize == 6) {
        unsigned char actualBlockSize;
        if (drflac__grab_bytes(pFlac->userData, &actualBlockSize, 1) != 1) {
            return drflac_false;
        }

        pFlac->currentFrame.blockSize = actualBlockSize + 1;
    } else if (blockSize == 7) {
        unsigned char actualBlockSize[2];
        if (drflac__grab_bytes(pFlac, &actualBlockSize, 2) != 2) {
            return drflac_false;
        }

        pFlac->currentFrame.blockSize = drflac__to_int32(actualBlockSize, 0, 16) + 1;
    } else {
        pFlac->currentFrame.blockSize = 256 * (1 << (blockSize - 8));
    }
    

    if (sampleRate >= 0 && sampleRate <= 11) {
        pFlac->currentFrame.sampleRate = sampleRateTable[sampleRate];
    } else if (sampleRate == 12) {
        unsigned char actualSampleRate_kHz;
        if (pFlac->onRead(pFlac->userData, &actualSampleRate_kHz, 1) != 1) {
            return drflac_false;
        }

        pFlac->currentFrame.sampleRate = (int)actualSampleRate_kHz * 1000;
    } else if (sampleRate == 13) {
        unsigned char actualSampleRate_Hz[2];
        if (drflac__grab_bytes(pFlac, &actualSampleRate_Hz, 2) != 2) {
            return drflac_false;
        }

        pFlac->currentFrame.sampleRate = drflac__to_int32(actualSampleRate_Hz, 0, 24);
    } else if (sampleRate == 14) {
        unsigned char actualSampleRate_tensOfHz[2];
        if (drflac__grab_bytes(pFlac, &actualSampleRate_tensOfHz, 2) != 2) {
            return drflac_false;
        }

        pFlac->currentFrame.sampleRate = drflac__to_int32(actualSampleRate_tensOfHz, 0, 24) * 10;
    } else {
        return drflac_false;  // Invalid.
    }
    

    pFlac->currentFrame.channelAssignment = channelAssignment;
    pFlac->currentFrame.bitsPerSample     = bitsPerSampleTable[bitsPerSample];

    if (drflac__grab_bytes(pFlac, &pFlac->currentFrame.crc8, 1) != 1) {
        return drflac_false;
    }

    return drflac_true;
}

static drflac_bool drflac__read_subframe_header(drflac* pFlac, drflac_subframe* pSubframe)
{
    drflac_bool result = drflac_false;

    unsigned char header;
    if (drflac__read_bits(pFlac, 8, &header, 0) != 8) {
        goto done_reading_subframe_header;
    }

    // First bit should always be 0.
    if ((header & 0x80) != 0) {
        goto done_reading_subframe_header;
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
        goto done_reading_subframe_header;
    }

    // Wasted bits per sample.
    pSubframe->wastedBitsPerSample = 0;
    if ((header & 0x01) == 1) {
        do {
            pSubframe->wastedBitsPerSample += 1;
        } while (drflac__read_next_bit(pFlac) == 0);
    }

    result = drflac_true;


done_reading_subframe_header:
    return result;
}

static drflac_bool drflac__decode_subframe(drflac* pFlac, int subframeIndex)
{
    assert(pFlac != NULL);

    drflac_subframe* pSubframe = pFlac->currentFrame.subframes + subframeIndex;
    if (!drflac__read_subframe_header(pFlac, pSubframe)) {
        return drflac_false;
    }

    pSubframe->firstDecodedSampleOffset = pFlac->decodedSampleCount;

    // Side channels require an extra bit per sample. Took a while to figure that one out...
    pSubframe->bitsPerSample = pFlac->currentFrame.bitsPerSample;
    if ((pFlac->currentFrame.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE || pFlac->currentFrame.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE) && subframeIndex == 1) {
        pSubframe->bitsPerSample += 1;
    } else if (pFlac->currentFrame.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE && subframeIndex == 0) {
        pSubframe->bitsPerSample += 1;
    }

    // Need to handle wasted bits per sample.
    pSubframe->bitsPerSample -= pSubframe->wastedBitsPerSample;


    switch (pSubframe->subframeType)
    {
        case DRFLAC_SUBFRAME_CONSTANT:
        {
            // Only a single sample needs to be decoded here.
            unsigned char originalSample[4];
            if (drflac__read_bits(pFlac, pSubframe->bitsPerSample, originalSample, 0) != pSubframe->bitsPerSample) {
                return drflac_false;
            }

            int sample = drflac__to_int32(originalSample, 0, pSubframe->bitsPerSample);

            // TODO: We don't really need to expand this, but I'm just doing it for now to just get things working.
            for (unsigned int i = 0; i < pFlac->currentFrame.blockSize; ++i) {
                pFlac->decodedSamples[pFlac->decodedSampleCount] = sample;
                pFlac->decodedSampleCount += 1;
            }

        } break;

        case DRFLAC_SUBFRAME_VERBATIM:
        {
            for (unsigned int i = 0; i < pFlac->currentFrame.blockSize; ++i) {
                unsigned char originalSample[4];
                if (drflac__read_bits(pFlac, pSubframe->bitsPerSample, originalSample, 0) != pSubframe->bitsPerSample) {
                    return drflac_false;
                }

                pFlac->decodedSamples[pFlac->decodedSampleCount] = drflac__to_int32(originalSample, 0, pSubframe->bitsPerSample);
                pFlac->decodedSampleCount += 1;
            }
        } break;

        case DRFLAC_SUBFRAME_FIXED:
        {
            // Warm up samples.
            for (int i = 0; i < pSubframe->lpcOrder; ++i) {
                unsigned char originalSample[4] = {0};
                if (drflac__read_bits(pFlac, pSubframe->bitsPerSample, originalSample, 0) != pSubframe->bitsPerSample) {
                    return drflac_false;
                }

                pFlac->decodedSamples[pFlac->decodedSampleCount] = drflac__to_int32(originalSample, 0, pSubframe->bitsPerSample);
                pFlac->decodedSampleCount += 1;
            }

            int lpcCoefficientsTable[5][4] = {
                {0,  0, 0,  0},
                {1,  0, 0,  0},
                {2, -1, 0,  0},
                {3, -3, 1,  0},
                {4, -6, 4, -1}
            };


            unsigned char residualMethod = 0;
            drflac__read_bits(pFlac, 2, &residualMethod, 6);

            unsigned char partitionOrder = 0;
            drflac__read_bits(pFlac, 4, &partitionOrder, 4);

            int samplesRemainingInPartition;
            if (partitionOrder == 0) {
                samplesRemainingInPartition = pFlac->currentFrame.blockSize - pSubframe->lpcOrder;
            } else {
                samplesRemainingInPartition = (pFlac->currentFrame.blockSize / (1 << partitionOrder)) - pSubframe->lpcOrder;
            }

            int partitionsRemaining = (1 << partitionOrder);
            do
            {
                unsigned char riceParam = 0;
                if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
                    drflac__read_bits(pFlac, 4, &riceParam, 4);
                    if (riceParam == 16) {
                        riceParam = 0xFF;
                    }
                } else if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
                    drflac__read_bits(pFlac, 5, &riceParam, 3);
                    if (riceParam == 32) {
                        riceParam = 0xFF;
                    }
                }
                

                if (riceParam != 0xFF)
                {
                    // Rice encoded.
                    while (samplesRemainingInPartition > 0)
                    {
                        // Residual.
                        int residual = drflac__read_and_decode_rice(pFlac, riceParam);

                        // Prediction.
                        long long prediction = 0;
                        for (int i = 0; i < pSubframe->lpcOrder; ++i) {
                            prediction += (long long)lpcCoefficientsTable[pSubframe->lpcOrder][i] * (long long)pFlac->decodedSamples[pFlac->decodedSampleCount - i - 1];
                        }


                        pFlac->decodedSamples[pFlac->decodedSampleCount] = (int)prediction + residual;
                        pFlac->decodedSampleCount += 1;

                        samplesRemainingInPartition -= 1;
                    }
                }
                else
                {
                    // Unencoded with each number being unencodedBitsPerSample in size.
                    unsigned char unencodedBitsPerSample = 0;
                    drflac__read_bits(pFlac, 5, &unencodedBitsPerSample, 3);

                    while (samplesRemainingInPartition > 0)
                    {
                        // Residual.
                        unsigned char residualBytes[4];
                        drflac__read_bits(pFlac, unencodedBitsPerSample, residualBytes, 0);
                        int residual = drflac__to_int32(residualBytes, 0, unencodedBitsPerSample);

                        // Prediction.
                        long long prediction = 0;
                        for (int i = 0; i < pSubframe->lpcOrder; ++i) {
                            prediction += (long long)lpcCoefficientsTable[pSubframe->lpcOrder][i] * (long long)pFlac->decodedSamples[pFlac->decodedSampleCount - i - 1];
                        }


                        pFlac->decodedSamples[pFlac->decodedSampleCount] = (int)prediction + residual;
                        pFlac->decodedSampleCount += 1;

                        samplesRemainingInPartition -= 1;
                    }
                }


                partitionsRemaining -= 1;
                if (partitionsRemaining > 0) {
                    samplesRemainingInPartition = (pFlac->currentFrame.blockSize / (1 << partitionOrder));
                }

            } while (partitionsRemaining > 0);
        } break;

        case DRFLAC_SUBFRAME_LPC:
        {
            // Warm up samples.
            for (int i = 0; i < pSubframe->lpcOrder; ++i) {
                unsigned char originalSample[4] = {0};
                if (drflac__read_bits(pFlac, pSubframe->bitsPerSample, originalSample, 0) != pSubframe->bitsPerSample) {
                    return drflac_false;
                }

                pFlac->decodedSamples[pFlac->decodedSampleCount] = drflac__to_int32(originalSample, 0, pSubframe->bitsPerSample);
                pFlac->decodedSampleCount += 1;
            }

            unsigned char lpcPrecision = 0;
            signed   char lpcShift     = 0;
            drflac__read_bits(pFlac, 4, &lpcPrecision, 4);
            drflac__read_bits(pFlac, 5, &lpcShift, 0);

            if (lpcPrecision == 15) {
                return drflac_false;    // Invalid.
            }

            lpcPrecision += 1;
            lpcShift >>= 3;

            int lpcCoefficients[32];
            for (int i = 0; i < pSubframe->lpcOrder; ++i) {
                unsigned char coefficient[2] = {0};
                if (drflac__read_bits(pFlac, lpcPrecision, coefficient, 0) != lpcPrecision) {
                    return drflac_false;
                }

                lpcCoefficients[i] = drflac__to_int32(coefficient, 0, lpcPrecision);
            }

            unsigned char residualMethod = 0;
            drflac__read_bits(pFlac, 2, &residualMethod, 6);

            unsigned char partitionOrder = 0;
            drflac__read_bits(pFlac, 4, &partitionOrder, 4);

            int samplesRemainingInPartition;
            if (partitionOrder == 0) {
                samplesRemainingInPartition = pFlac->currentFrame.blockSize - pSubframe->lpcOrder;
            } else {
                samplesRemainingInPartition = (pFlac->currentFrame.blockSize / (1 << partitionOrder)) - pSubframe->lpcOrder;
            }

            int partitionsRemaining = (1 << partitionOrder);
            do
            {
                unsigned char riceParam = 0;
                if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
                    drflac__read_bits(pFlac, 4, &riceParam, 4);
                    if (riceParam == 16) {
                        riceParam = 0xFF;
                    }
                } else if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
                    drflac__read_bits(pFlac, 5, &riceParam, 3);
                    if (riceParam == 32) {
                        riceParam = 0xFF;
                    }
                }
                

                if (riceParam != 0xFF)
                {
                    // Rice encoded.
                    while (samplesRemainingInPartition > 0)
                    {
                        // Residual.
                        int residual = drflac__read_and_decode_rice(pFlac, riceParam);

                        // Prediction.
                        long long prediction = 0;
                        for (int i = 0; i < pSubframe->lpcOrder; ++i) {
                            prediction += (long long)lpcCoefficients[i] * (long long)pFlac->decodedSamples[pFlac->decodedSampleCount - i - 1];
                        }

                        prediction >>= lpcShift;


                        pFlac->decodedSamples[pFlac->decodedSampleCount] = (int)prediction + residual;
                        pFlac->decodedSampleCount += 1;

                        samplesRemainingInPartition -= 1;
                    }
                }
                else
                {
                    // Unencoded with each number being unencodedBitsPerSample in size.
                    unsigned char unencodedBitsPerSample = 0;
                    drflac__read_bits(pFlac, 5, &unencodedBitsPerSample, 3);

                    while (samplesRemainingInPartition > 0)
                    {
                        // Residual.
                        unsigned char residualBytes[4];
                        drflac__read_bits(pFlac, unencodedBitsPerSample, residualBytes, 0);
                        int residual = drflac__to_int32(residualBytes, 0, unencodedBitsPerSample);

                        // Prediction.
                        long long prediction = 0;
                        for (int i = 0; i < pSubframe->lpcOrder; ++i) {
                            prediction += (long long)lpcCoefficients[i] * (long long)pFlac->decodedSamples[pFlac->decodedSampleCount - i - 1];
                        }

                        prediction >>= lpcShift;


                        pFlac->decodedSamples[pFlac->decodedSampleCount] = (int)prediction + residual;
                        pFlac->decodedSampleCount += 1;

                        samplesRemainingInPartition -= 1;
                    }
                }


                partitionsRemaining -= 1;
                if (partitionsRemaining > 0) {
                    samplesRemainingInPartition = (pFlac->currentFrame.blockSize / (1 << partitionOrder));
                }

            } while (partitionsRemaining > 0);

        } break;

        default: return drflac_false;
    }

    return drflac_true;
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

static drflac_bool drflac__begin_next_frame(drflac* pFlac)      // TODO: Rename this to drflac__decode_next_frame().
{
    assert(pFlac != NULL);

    if (!drflac__read_next_frame_header(pFlac)) {
        return drflac_false;
    }

    // At this point we have the frame header, but we need to retrieve information about each sub-frame. There is one sub-frame for each channel.
    memset(&pFlac->currentFrame.subframes, 0, sizeof(pFlac->currentFrame.subframes));
    pFlac->decodedSampleCount = 0;

    int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);
    for (int i = 0; i < channelCount; ++i)
    {
        if (!drflac__decode_subframe(pFlac, i)) {
            return drflac_false;
        }
    }

    // Padding to byte alignment.
    unsigned char padding;
    if (pFlac->leftoverBitsRemaining > 0) {
        drflac__read_bits(pFlac, pFlac->leftoverBitsRemaining, &padding, 0);
    }
    

    // CRC.
    unsigned char crc[2];
    drflac__read_bits(pFlac, 16, crc, 0);

    pFlac->samplesRemainingInCurrentFrame = pFlac->currentFrame.blockSize * drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);

    return drflac_true;
}

static unsigned int drflac__read_block_header(drflac_read_proc onRead, void* userData, unsigned int* pBlockSizeOut, drflac_bool* pIsLastBlockOut)
{
    drflac_bool isLastBlock = drflac_true;
    unsigned int blockType = DRFLAC_BLOCK_TYPE_INVALID;
    unsigned int blockSize = 0;

    unsigned char blockHeader[4];
    if (onRead(userData, blockHeader, sizeof(blockHeader)) != sizeof(blockHeader)) {
        goto done_reading_block_header;
    }

    isLastBlock = (blockHeader[0] & 0x80) != 0;
    blockType = drflac__to_uint32(blockHeader,  1, 7);
    blockSize = drflac__to_uint32(blockHeader + 1, 0, 24);


done_reading_block_header:
    if (pBlockSizeOut) {
        *pBlockSizeOut = blockSize;
    }

    if (pIsLastBlockOut) {
        *pIsLastBlockOut = isLastBlock;
    }

    return blockType;
}

drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_tell_proc onTell, void* userData)
{
    if (onRead == NULL || onSeek == NULL) {
        return NULL;
    }

    unsigned char id[4];
    if (onRead(userData, id, 4) != 4 || id[0] != 'f' || id[1] != 'L' || id[2] != 'a' || id[3] != 'C') {
        return NULL;    // Not a FLAC stream.
    }

    // The first metadata block should be the STREAMINFO block. We don't care about everything in here.
    unsigned int blockSize;
    drflac_bool isLastBlock;
    int blockType = drflac__read_block_header(onRead, userData, &blockSize, &isLastBlock);
    if (blockType != DRFLAC_BLOCK_TYPE_STREAMINFO && blockSize != 34) {
        return NULL;
    }
    
    unsigned char blockData[34];    // Total size of the STREAMINFO should be 38 bytes.
    if (onRead(userData, blockData, sizeof(blockData)) != sizeof(blockData)) {
        return drflac_false;
    }

    unsigned int maxBlockSize           = drflac__to_uint32(blockData + 2,  0, 16);
    unsigned int sampleRate             = drflac__to_uint32(blockData + 10, 0, 20);
    unsigned int channels               = drflac__to_uint32(blockData + 12, 4,  3) + 1;
    unsigned int bitsPerSample          = drflac__to_uint32(blockData + 12, 7,  5) + 1;
    unsigned long long totalSampleCount = drflac__to_uint64(blockData + 13, 4, 36) * channels;


    drflac_block applicationBlock;
    drflac_block seektableBlock;
    drflac_block vorbisCommentBlock;
    drflac_block cuesheetBlock;
    drflac_block pictureBlock;

    while (!isLastBlock)
    {
        blockType = drflac__read_block_header(onRead, userData, &blockSize, &isLastBlock);
        switch (blockType)
        {
            case DRFLAC_BLOCK_TYPE_APPLICATION:
            {
                applicationBlock.pos = onTell(userData);
                applicationBlock.sizeInBytes = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_SEEKTABLE:
            {
                seektableBlock.pos = onTell(userData);
                seektableBlock.sizeInBytes = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_VORBIS_COMMENT:
            {
                vorbisCommentBlock.pos = onTell(userData);
                vorbisCommentBlock.sizeInBytes = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_CUESHEET:
            {
                cuesheetBlock.pos = onTell(userData);
                cuesheetBlock.sizeInBytes = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_PICTURE:
            {
                pictureBlock.pos = onTell(userData);
                pictureBlock.sizeInBytes = blockSize;
            } break;

            
            // These blocks we either don't care about or aren't supporting.
            case DRFLAC_BLOCK_TYPE_PADDING:
            case DRFLAC_BLOCK_TYPE_INVALID:
            default: break;
        }

        if (!onSeek(userData, blockSize)) {
            return NULL;    // Failed to seek past this block. 
        }
    }

    
    // At this point we should be sitting right at the start of the very first frame.
    drflac* pFlac = malloc(sizeof(*pFlac) - sizeof(pFlac->decodedSamples) + (maxBlockSize * 4 * channels));     // x4 because a sample is at most 32-bit (4 bytes).
    if (pFlac == NULL) {
        return NULL;
    }

    pFlac->onRead                         = onRead;
    pFlac->onSeek                         = onSeek;
    pFlac->onTell                         = onTell;
    pFlac->userData                       = userData;
    pFlac->leftoverByte                   = 0;
    pFlac->leftoverBitsRemaining          = 0;
    pFlac->maxBlockSize                   = maxBlockSize;
    pFlac->sampleRate                     = sampleRate;
    pFlac->channels                       = channels;
    pFlac->bitsPerSample                  = bitsPerSample;
    pFlac->totalSampleCount               = totalSampleCount;
    pFlac->applicationBlock               = applicationBlock;
    pFlac->seektableBlock                 = seektableBlock;
    pFlac->vorbisCommentBlock             = vorbisCommentBlock;
    pFlac->cuesheetBlock                  = cuesheetBlock;
    pFlac->pictureBlock                   = pictureBlock;
    memset(&pFlac->currentFrame, 0, sizeof(pFlac->currentFrame));
    pFlac->samplesRemainingInCurrentFrame = 0;
    pFlac->nextSampleChannel              = 0;
    pFlac->decodedSampleCount             = 0;

    return pFlac;
}

void drflac_close(drflac* pFlac)
{
    free(pFlac);
}

unsigned int drflac_read_s32(drflac* pFlac, size_t samplesToRead, int* bufferOut)
{
    if (pFlac == NULL || samplesToRead == 0 || bufferOut == NULL) {
        return 0;
    }

    size_t samplesRead = 0;
    while (samplesToRead > 0)
    {
        // If we've run out of samples in this frame, go to the next.
        if (pFlac->samplesRemainingInCurrentFrame == 0)
        {
            if (!drflac__begin_next_frame(pFlac)) {
                break;  // Couldn't read the next frame, so just break from the loop and return.
            }
        }
        else
        {
            unsigned int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);

            // We still have samples remaining in the current frame so we need to retrieve them.
            while (pFlac->samplesRemainingInCurrentFrame > 0 && samplesToRead > 0)
            {
                unsigned long long nextSampleInFrame = ((pFlac->currentFrame.blockSize * channelCount) - pFlac->samplesRemainingInCurrentFrame) / channelCount;
                unsigned long long sampleIndex0 = pFlac->currentFrame.subframes[pFlac->nextSampleChannel].firstDecodedSampleOffset + nextSampleInFrame;

                int decodedSample = 0;
                switch (pFlac->currentFrame.channelAssignment)
                {
                    case DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE:
                    {
                        if (pFlac->nextSampleChannel == 0) {
                            decodedSample = pFlac->decodedSamples[sampleIndex0];
                        } else {
                            int side = pFlac->decodedSamples[pFlac->currentFrame.subframes[pFlac->nextSampleChannel + 0].firstDecodedSampleOffset + nextSampleInFrame];
                            int left = pFlac->decodedSamples[pFlac->currentFrame.subframes[pFlac->nextSampleChannel - 1].firstDecodedSampleOffset + nextSampleInFrame];
                            decodedSample = left - side;
                        }

                    } break;

                    case DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                    {
                        if (pFlac->nextSampleChannel == 0) {
                            int side  = pFlac->decodedSamples[pFlac->currentFrame.subframes[pFlac->nextSampleChannel + 0].firstDecodedSampleOffset + nextSampleInFrame];
                            int right = pFlac->decodedSamples[pFlac->currentFrame.subframes[pFlac->nextSampleChannel + 1].firstDecodedSampleOffset + nextSampleInFrame];
                            decodedSample = side + right;
                        } else {
                            decodedSample = pFlac->decodedSamples[sampleIndex0];
                        }

                    } break;

                    case DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE:
                    {
                        int mid;
                        int side;
                        if (pFlac->nextSampleChannel == 0) {
                            mid  = pFlac->decodedSamples[pFlac->currentFrame.subframes[pFlac->nextSampleChannel + 0].firstDecodedSampleOffset + nextSampleInFrame];
                            side = pFlac->decodedSamples[pFlac->currentFrame.subframes[pFlac->nextSampleChannel + 1].firstDecodedSampleOffset + nextSampleInFrame];

                            mid = (((unsigned int)mid) << 1) | (side & 0x01);
                            decodedSample = (mid + side) >> 1;
                        } else {
                            mid  = pFlac->decodedSamples[pFlac->currentFrame.subframes[pFlac->nextSampleChannel - 1].firstDecodedSampleOffset + nextSampleInFrame];
                            side = pFlac->decodedSamples[pFlac->currentFrame.subframes[pFlac->nextSampleChannel + 0].firstDecodedSampleOffset + nextSampleInFrame];

                            mid = (((unsigned int)mid) << 1) | (side & 0x01);
                            decodedSample = (mid - side) >> 1;
                        }
                        
                    } break;

                    case DRFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT:
                    default:
                    {
                        decodedSample = pFlac->decodedSamples[sampleIndex0];
                    } break;
                }


                decodedSample <<= (32 - pFlac->bitsPerSample);


                *bufferOut++ = decodedSample;
                pFlac->nextSampleChannel = (pFlac->nextSampleChannel + 1) % pFlac->channels;

                samplesRead += 1;
                pFlac->samplesRemainingInCurrentFrame -= 1;
                samplesToRead -= 1;
            }
        }
    }

    return samplesRead;
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
