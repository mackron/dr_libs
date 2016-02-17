// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - This has not been tested on big-endian architectures.
// - dr_flac is not thread-safe, but it's APIs can be called from any thread so long as the application does it's own thread-safety.
//

//
// OPTIONS
//
//
//

#ifndef dr_flac_h
#define dr_flac_h

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* drflac_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

/// Callback for when data needs to be seeked. Offset is always absolute. Return value is 0 on failure, non-zero success.
typedef int (* drflac_seek_proc)(void* userData, long long offsetFromStart);

/// Callback for when the read position needs to be queried.
typedef long long (* drflac_tell_proc)(void* userData);

typedef struct
{
    int _isLastBlock;  // Used internally.

    int minBlockSize;
    int maxBlockSize;
    int minFrameSize;
    int maxFrameSize;
    int sampleRate;
    int channels;
    int bitsPerSample;
    long long totalSampleCount;
    unsigned char md5[16];

} drflac_STREAMINFO;

typedef struct
{
    unsigned short syncCode;    // '0011111111111110'
    int isVariableBlocksize;

    long long frameNumber;      // Only used if isVariableBlockSize is false. Otherwise set to zero.
    long long sampleNumber;     // Only used if isVariableBlockSize is true. Otherwise set to zero.

    int blockSize;
    int sampleRate;
    int channelAssignment;
    int bitsPerSample;
    unsigned char crc8;

    // The number of samples in each subframe.
    int subframeSampleCount;

} drflac_FRAME_HEADER;

typedef struct
{
    int _isHeaderLoaded;        // Used internally.
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
    /// The function to call when more data needs to be read. This is set by drflac_open().
    drflac_read_proc onRead;

    /// The function to call when the current read position needs to be moved.
    drflac_seek_proc onSeek;

    /// The function to call when the current read position needs to be queried.
    drflac_tell_proc onTell;

    /// The user data to pass around to onRead and onSeek.
    void* userData;


    /// The temporary byte containing containing the bits that have not yet been read.
    unsigned char leftoverByte;

    /// The number of bits remaining in the byte the reader is currently sitting on.
    unsigned char leftoverBitsRemaining;


    /// The STREAMINFO block. Use this to find information like channel counts and sample rate.
    drflac_STREAMINFO info;

    /// The position in the file of the application as defined by the APPLICATION block, if any. A valid of 0 means it doesn't exist.
    long long applicationMetadataPos;

    /// The size of the application data. Does not include the registration ID or block header.
    int applicationMetadataSize;


    /// Information about the current frame.
    drflac_FRAME_HEADER currentFrameHeader;

    /// Information about the subframes of the current frame. There is one of these per channel, and there is a maximum of 8 channels.
    drflac_subframe currentSubframes[8];

    /// The number of samples remaining in the current frame. We use this to determine when a new frame needs to be begun.
    long long samplesRemainingInCurrentFrame;

    /// The index of the channel that the next sample belongs to. We use this for interleaving, and is never larger than the the channel count as
    /// specified by currentFrameHeader.channelAssignment-1.
    int nextSampleChannel;


    /// The number of valid decoded samples in <decodedSamples> (below). We use this to keep track of where to place newly decoded samples.
    unsigned int decodedSampleCount;

    /// A pointer the decoded sample data. This section is allocated once at initialization time and sized based on the maximum block size.
    int decodedSamples[1];

} drflac;




/// Opens a FLAC decoder.
drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_tell_proc onTell, void* userData);

/// Closes the given FLAC decoder.
void drflac_close(drflac* flac);


/// Reads sample data from the given FLAC decoder.
unsigned int drflac_read(drflac* flac, unsigned int samplesToRead, void* bufferOut);


/// Decodes a Rice code.
int drflac_decode_rice(int m, int rice);



#ifndef DR_FLAC_NO_STDIO
/// Opens a flac decoder from the file at the given path.
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

#define drflac_false 0
#define drflac_true  1
typedef int drflac_bool;

#define DRFLAC_BLOCK_TYPE_STREAMINFO        0
#define DRFLAC_BLOCK_TYPE_PADDING           1
#define DRFLAC_BLOCK_TYPE_APPLICATION       2
#define DRFLAC_BLOCK_TYPE_SEEKTABLE         3
#define DRFLAC_BLOCK_TYPE_VORBIS_COMMENT    4
#define DRFLAC_BLOCK_TYPE_CUESHEET          5
#define DRFLAC_BLOCK_TYPE_PICTURE           6
#define DRFLAC_BLOCK_TYPE_INVALID           127

#define DRFLAC_SUBFRAME_CONSTANT            0
#define DRFLAC_SUBFRAME_VERBATIM            1
#define DRFLAC_SUBFRAME_FIXED               8
#define DRFLAC_SUBFRAME_LPC                 32
#define DRFLAC_SUBFRAME_RESERVED            -1

#define DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE  0
#define DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2 1

#ifndef DR_FLAC_NO_STDIO
#include <stdio.h>

static size_t drflac__on_read_stdio(void* userData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)userData);
}

static int drflac__on_seek_stdio(void* userData, long long offsetFromStart)
{
    return fseek((FILE*)userData, offsetFromStart, SEEK_SET) == 0;
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
    assert(bitOffsetOut < 8);

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

static inline size_t drflac__read_bytes(drflac* pFlac, void* pOut, size_t bytesToRead)
{
    assert(pFlac != NULL);
    assert(pOut != NULL);
    assert(bytesToRead > 0);

    size_t bytesRead = pFlac->onRead(pFlac->userData, pOut, bytesToRead);

    return bytesRead;
}

static inline int drflac__seek_to(drflac* pFlac, long long offsetFromStart)
{
    assert(pFlac != NULL);

    int result = pFlac->onSeek(pFlac->userData, offsetFromStart);

    return result;
}

static unsigned int drflac__read_bits(drflac* pFlac, unsigned int bitsToRead, unsigned char* pOut, unsigned int bitOffsetOut)
{
    assert(pFlac != NULL);
    assert(pOut != NULL);
    assert(pFlac->leftoverBitsRemaining <= 7);  

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

            size_t bytesRead = drflac__read_bytes(pFlac, pOut, bytesToRead);
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
                if (drflac__read_bytes(pFlac, &nextByte, 1) != 1) {
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

        if (drflac__read_bytes(pFlac, &pFlac->leftoverByte, 1) != 1) {
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
        if (drflac__read_bytes(pFlac, &pFlac->leftoverByte, 1) != 1) {
            return -1;  // Ran out of data.
        }

        pFlac->leftoverBitsRemaining = 8;
    }

    pFlac->leftoverBitsRemaining -= 1;
    return (pFlac->leftoverByte & (1 << pFlac->leftoverBitsRemaining)) >> pFlac->leftoverBitsRemaining;
}


static drflac_bool drflac__is_little_endian()
{
    int n = 1;
    return (*(char*)&n) == 1;
}

static unsigned int drflac__read_u24(const unsigned char* data)
{
    if (drflac__is_little_endian()) {
        return (data[0] << 16) | (data[1] << 8) | (data[2] << 0);
    } else {
        return (data[2] << 16) | (data[1] << 8) | (data[0] << 0);
    }
}

static unsigned short drflac__read_u16(const unsigned char* data)
{
    if (drflac__is_little_endian()) {
        return (data[0] << 8) | (data[1] << 0);
    } else {
        return (data[1] << 8) | (data[0] << 0);
    }
}

static long long drflac__read_utf8_coded_number(drflac_read_proc onRead, void* userData, size_t* pBytesReadOut)
{
    assert(onRead != NULL);

    long long result = 0;
    size_t bytesRead = 0;

    unsigned char utf8[7] = {0};
    if (onRead(userData, utf8, 1) != 1) {
        goto done_reading_utf8;     // Ran out of data.
    }

    bytesRead = 1;

    if ((utf8[0] & 0x80) == 0) {
        result = utf8[0];
        goto done_reading_utf8;
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
        goto done_reading_utf8;     // Bad UTF-8 encoding.
    }

    // Read extra bytes.
    assert(byteCount > 1);

    size_t extraBytesRead;
    if ((extraBytesRead = onRead(userData, utf8 + 1, byteCount - 1)) != byteCount) {
        goto done_reading_utf8;     // Ran out of data.
    }

    bytesRead += extraBytesRead;

    // At this point we have the raw UTF-8 data and we just need to decode it.
    result = ((long long)(utf8[0] & (0xFF >> (byteCount + 1))));
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
    


done_reading_utf8:
    if (pBytesReadOut) {
        *pBytesReadOut = bytesRead;
    }

    return result;
}

// Converts a stream of bits of a variable length to a signed 32-bit number. Assumes the input data is big-endian.
static inline int drflac__to_signed_32(unsigned char* pIn, int bitOffsetIn, int bitCount)
{
    int result = 0;

    // TODO: Add support for big-endian.
    if (!drflac__is_little_endian()) {
        assert(0);      // No support for big-endian at the moment.
    }

    if (bitCount > 24) {
        result = (pIn[0] << (bitCount - 8)) | (pIn[1] << (bitCount - 16)) | (pIn[2] << (bitCount - 24)) | (pIn[3] >> (8 - (bitCount - 24)));
    } else if (bitCount > 16) {
        result = (pIn[0] << (bitCount - 8)) | (pIn[1] << (bitCount - 16)) | (pIn[2] >> (8 - (bitCount - 16)));
    } else if (bitCount > 8) {
        result = (pIn[0] << (bitCount - 8)) | (pIn[1] >> (8 - (bitCount - 8)));
    } else {
        result = (pIn[0] >> (8 - bitCount));
    }

    if ((pIn[0] & 0x80)) {
        result |= (0xFFFFFFFF << bitCount);
    }

    return result;
}

static inline int drflac__read_and_decode_rice(drflac* pFlac, unsigned char m)
{
    // TODO: Profile and optimize this.

    // TODO: This is completely wrong in the way it calculates the value. Just figuring out the correct number of bits to read.
    int zeroCounter = 0;
    while (drflac__read_next_bit(pFlac) == 0) {
        zeroCounter += 1;
    }

    int decodedValue = 0;
    for (unsigned char i = 0; i < m; ++i) {
        if (drflac__read_next_bit(pFlac) != 0) {
            decodedValue |= (1 << (m - i - 1));
        }
    }

    return decodedValue;
}

static inline int drflac__calculate_prediction(drflac* pFlac, int order)
{
    // TODO: Implement Me.
    return 0;
}

static drflac_bool drflac__read_streaminfo(drflac_read_proc onRead, void* userData, drflac_STREAMINFO* pInfo)
{
    unsigned char blockData[38];    // Total size of the STREAMINFO should be 38 bytes.
    if (onRead(userData, blockData, sizeof(blockData)) != sizeof(blockData)) {
        return drflac_false;
    }

    if ((blockData[0] & 0x7F) != DRFLAC_BLOCK_TYPE_STREAMINFO) {
        return drflac_false;        // The block is not a STREAMINFO block.
    }

    pInfo->_isLastBlock = (blockData[0] & 0x80) != 0;

    unsigned int blockSize = drflac__read_u24(blockData + 1);
    if (blockSize != 34) {
        return drflac_false;        // Expecting a block size of 34 for STREAMINFO.
    }

    pInfo->minBlockSize = (int)drflac__read_u16(blockData + 4);
    pInfo->maxBlockSize = (int)drflac__read_u16(blockData + 6);
    pInfo->minFrameSize = (int)drflac__read_u24(blockData + 8);
    pInfo->maxFrameSize = (int)drflac__read_u24(blockData + 11);

    if (drflac__is_little_endian()) {
        pInfo->sampleRate = (blockData[14] << 12) | (blockData[15] << 4) | (((blockData[16] & 0xF0) >> 4) << 0);
    } else {
        pInfo->sampleRate = (blockData[14] <<  0) | (blockData[15] << 8) | (((blockData[16] & 0xF0) >> 4) << 16);   // <-- This has not been tested!
    }

    pInfo->channels      = ((blockData[16] & 0xE) >> 1) + 1;
    pInfo->bitsPerSample = (((blockData[16] & 0x1) << 4) | ((blockData[17] & 0xF0) >> 4)) + 1;

    if (drflac__is_little_endian()) {
        pInfo->totalSampleCount = ((((long long)blockData[17] & 0x0F) << 32) | (blockData[18] << 24) | (blockData[19] << 16) | (blockData[20] <<  8) | (blockData[21] << 0));
    } else {
        pInfo->totalSampleCount = ((((long long)blockData[17] & 0x0F) <<  0) | (blockData[18] <<  4) | (blockData[19] << 12) | (blockData[20] << 20) | (blockData[21] << 28));    // <-- This has not been tested!
    }
    
    for (int i = 0; i < 16; ++i) {
        pInfo->md5[i] = blockData[22 + i];
    }
    
    return drflac_true;
}

static drflac_bool drflac__read_frame_header(drflac_read_proc onRead, void* userData, drflac_FRAME_HEADER* pFrameHeader, size_t* pBytesReadOut)
{
    if (onRead == NULL || pFrameHeader == NULL) {
        return drflac_false;
    }

    const int sampleRateTable[12]   = {0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
    const int bitsPerSampleTable[8] = {0, 8, 12, -1, 16, 20, 24, -1};   // -1 = reserved.


    size_t totalBytesRead = 0;
    drflac_bool result = drflac_false;

    size_t mostRecentBytesRead = 0;

    unsigned char headerDataFixed[4];
    if ((mostRecentBytesRead = onRead(userData, headerDataFixed, sizeof(headerDataFixed))) != sizeof(headerDataFixed)) {
        totalBytesRead += mostRecentBytesRead;
        result = drflac_false;
        goto done_reading_frame_header;
    }
    
    totalBytesRead += mostRecentBytesRead;


    unsigned char blockSize         = (headerDataFixed[2] & 0xF0) >> 4;
    unsigned char sampleRate        = (headerDataFixed[2] & 0x0F);
    unsigned char channelAssignment = (headerDataFixed[3] & 0xF0) >> 4;
    unsigned char bitsPerSample     = (headerDataFixed[3] & 0x0E) >> 1;

    pFrameHeader->syncCode            = (headerDataFixed[0] << 6) | ((headerDataFixed[1] & 0xFC) >> 2);
    pFrameHeader->isVariableBlocksize = (headerDataFixed[1] & 0x01);

    if (pFrameHeader->isVariableBlocksize) {
        pFrameHeader->frameNumber  = 0;
        pFrameHeader->sampleNumber = drflac__read_utf8_coded_number(onRead, userData, &mostRecentBytesRead);
    } else {
        pFrameHeader->frameNumber  = drflac__read_utf8_coded_number(onRead, userData, &mostRecentBytesRead);
        pFrameHeader->sampleNumber = 0;
    }

    totalBytesRead += mostRecentBytesRead;


    if (blockSize == 1) {
        pFrameHeader->blockSize = 192;
    } else if (blockSize >= 2 && blockSize <= 5) {
        pFrameHeader->blockSize = 576 * (1 << (blockSize - 2));
    } else if (blockSize == 6) {
        unsigned char actualBlockSize;
        if (onRead(userData, &actualBlockSize, 1) != 1) {
            result = drflac_false;
            goto done_reading_frame_header;
        }

        totalBytesRead += mostRecentBytesRead;
        pFrameHeader->blockSize = actualBlockSize + 1;
    } else if (blockSize == 7) {
        unsigned char actualBlockSize[2];
        if ((mostRecentBytesRead = onRead(userData, &actualBlockSize, 2)) != 2) {
            totalBytesRead += mostRecentBytesRead;
            result = drflac_false;
            goto done_reading_frame_header;
        }

        totalBytesRead += mostRecentBytesRead;
        pFrameHeader->blockSize = (int)drflac__read_u24(actualBlockSize) + 1;
    } else {
        pFrameHeader->blockSize = 256 * (1 << (blockSize - 8));
    }
    

    if (sampleRate >= 0 && sampleRate <= 11) {
        pFrameHeader->sampleRate = sampleRateTable[sampleRate];
    } else if (sampleRate == 12) {
        unsigned char actualSampleRate_kHz;
        if ((mostRecentBytesRead = onRead(userData, &actualSampleRate_kHz, 1)) != 1) {
            result = drflac_false;
            goto done_reading_frame_header;
        }

        totalBytesRead += mostRecentBytesRead;
        pFrameHeader->sampleRate = (int)actualSampleRate_kHz * 1000;
    } else if (sampleRate == 13) {
        unsigned char actualSampleRate_Hz[2];
        if ((mostRecentBytesRead = onRead(userData, &actualSampleRate_Hz, 2)) != 2) {
            totalBytesRead += mostRecentBytesRead;
            result = drflac_false;
            goto done_reading_frame_header;
        }

        totalBytesRead += mostRecentBytesRead;
        pFrameHeader->sampleRate = (int)drflac__read_u24(actualSampleRate_Hz);
    } else if (sampleRate == 14) {
        unsigned char actualSampleRate_tensOfHz[2];
        if ((mostRecentBytesRead = onRead(userData, &actualSampleRate_tensOfHz, 2)) != 2) {
            totalBytesRead += mostRecentBytesRead;
            result = drflac_false;
            goto done_reading_frame_header;
        }

        totalBytesRead += mostRecentBytesRead;
        pFrameHeader->sampleRate = (int)drflac__read_u24(actualSampleRate_tensOfHz) * 10;
    } else {
        result = drflac_false;  // Invalid.
        goto done_reading_frame_header;
    }
    

    pFrameHeader->channelAssignment   = channelAssignment;
    pFrameHeader->bitsPerSample       = bitsPerSampleTable[bitsPerSample];
    pFrameHeader->subframeSampleCount = pFrameHeader->blockSize;

    mostRecentBytesRead = onRead(userData, &pFrameHeader->crc8, 1);
    if (mostRecentBytesRead != 1) {
        result = drflac_false;
        goto done_reading_frame_header;
    }

    totalBytesRead += 1;


    result = drflac_true;


done_reading_frame_header:
    if (pBytesReadOut != NULL) {
        *pBytesReadOut = totalBytesRead;
    }

    return result;
}

static drflac_bool drflac__read_next_frame_header(drflac* pFlac)
{
    assert(pFlac != NULL);
    assert(pFlac->onRead != NULL);

    size_t bytesRead;
    drflac_bool result = drflac__read_frame_header(pFlac->onRead, pFlac->userData, &pFlac->currentFrameHeader, &bytesRead);

    return result;
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

    drflac_subframe* pSubframe = pFlac->currentSubframes + subframeIndex;
    if (!drflac__read_subframe_header(pFlac, pSubframe)) {
        return drflac_false;
    }

    pSubframe->firstDecodedSampleOffset = pFlac->decodedSampleCount;

    // Side channels require an extra bit per sample. Took a while to figure that one out...
    pSubframe->bitsPerSample = pFlac->currentFrameHeader.bitsPerSample;
    if ((pFlac->currentFrameHeader.channelAssignment == 8 || pFlac->currentFrameHeader.channelAssignment == 10) && subframeIndex == 1) {
        pSubframe->bitsPerSample += 1;
    } else if (pFlac->currentFrameHeader.channelAssignment == 9 && subframeIndex == 0) {
        pSubframe->bitsPerSample += 1;
    }


    switch (pSubframe->subframeType)
    {
        case DRFLAC_SUBFRAME_CONSTANT:
        {
            // Only a single sample needs to be decoded here.
            unsigned char originalSample[4];
            if (drflac__read_bits(pFlac, pSubframe->bitsPerSample, originalSample, 0) != pSubframe->bitsPerSample) {
                return drflac_false;
            }

            pFlac->decodedSamples[pFlac->decodedSampleCount] = drflac__to_signed_32(originalSample, 0, pSubframe->bitsPerSample);
            pFlac->decodedSampleCount += 1;

        } break;

        case DRFLAC_SUBFRAME_VERBATIM:
        {
            for (int i = 0; i < pFlac->currentFrameHeader.subframeSampleCount; ++i) {
                unsigned char originalSample[4];
                if (drflac__read_bits(pFlac, pSubframe->bitsPerSample, originalSample, 0) != pSubframe->bitsPerSample) {
                    return drflac_false;
                }

                pFlac->decodedSamples[pFlac->decodedSampleCount] = drflac__to_signed_32(originalSample, 0, pSubframe->bitsPerSample);
                pFlac->decodedSampleCount += 1;
            }
        } break;

        case DRFLAC_SUBFRAME_FIXED:
        {
            // Warm up samples.

            // Decode samples.
            for (int i = 0; i < pFlac->currentFrameHeader.subframeSampleCount; ++i) {
                //unsigned char originalSample[4];
                //if (drflac__read_bits(pFlac, pSubframe->bitsPerSample, originalSample, 0) != pSubframe->bitsPerSample) {
                //    return drflac_false;
                //}

                //pFlac->decodedSamples[pFlac->decodeSampleCount] = drflac__to_s32_sample(originalSample, 0, pSubframe->bitsPerSample);
                pFlac->decodedSampleCount += 1;
            }
        } break;

        case DRFLAC_SUBFRAME_LPC:
        {
            // Warm up samples.
            for (int i = 0; i < pSubframe->lpcOrder; ++i) {
                unsigned char originalSample[4] = {0};
                if (drflac__read_bits(pFlac, pSubframe->bitsPerSample, originalSample, 0) != pSubframe->bitsPerSample) {
                    return drflac_false;
                }

                pFlac->decodedSamples[pFlac->decodedSampleCount] = drflac__to_signed_32(originalSample, 0, pSubframe->bitsPerSample);
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

                lpcCoefficients[i] = drflac__to_signed_32(coefficient, 0, lpcPrecision);
            }

            unsigned char residualMethod = 0;
            drflac__read_bits(pFlac, 2, &residualMethod, 6);

            unsigned char partitionOrder = 0;
            drflac__read_bits(pFlac, 4, &partitionOrder, 4);

            int samplesRemainingInPartition;
            if (partitionOrder == 0) {
                samplesRemainingInPartition = pFlac->currentFrameHeader.blockSize - pSubframe->lpcOrder;
            } else {
                samplesRemainingInPartition = (pFlac->currentFrameHeader.blockSize / (1 << partitionOrder)) - pSubframe->lpcOrder;
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
                        int prediction = drflac__calculate_prediction(pFlac, pSubframe->lpcOrder);

                        pFlac->decodedSamples[pFlac->decodedSampleCount] = prediction + residual;
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
                        int residual = drflac__to_signed_32(residualBytes, 0, unencodedBitsPerSample);

                        // Prediction.
                        int prediction = drflac__calculate_prediction(pFlac, pSubframe->lpcOrder);

                        pFlac->decodedSamples[pFlac->decodedSampleCount] = prediction + residual;
                        pFlac->decodedSampleCount += 1;

                        samplesRemainingInPartition -= 1;
                    }
                }


                partitionsRemaining -= 1;
                if (partitionsRemaining > 0) {
                    samplesRemainingInPartition = (pFlac->currentFrameHeader.blockSize / (1 << partitionOrder));
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

    if (channelAssignment >= 8 && channelAssignment <= 11) {
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
    memset(&pFlac->currentSubframes, 0, sizeof(pFlac->currentSubframes));
    pFlac->decodedSampleCount = 0;

    int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrameHeader.channelAssignment);
    for (int i = 0; i < channelCount; ++i)
    {
        if (!drflac__decode_subframe(pFlac, i)) {
            return drflac_false;
        }
    }

    // Padding to byte alignment.
    unsigned char padding = 0xFF;   // Initializing to 0xFF for testing. Delete later.
    drflac__read_bits(pFlac, pFlac->leftoverBitsRemaining, &padding, 0);

    // CRC.
    unsigned char crc[2];
    drflac__read_bits(pFlac, 16, crc, 0);

    return drflac_true;
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

    // The next chunk of data should be the STREAMINFO block.
    drflac_STREAMINFO streaminfo;
    if (!drflac__read_streaminfo(onRead, userData, &streaminfo)) {
        return NULL;    // Failed to read the STREAMINFO block.
    }

    long long currentPos = 42;  // 42 = 4 bytes "fLaC" header + 38 byte STREAMINFO block.

    long long applicationMetadataPos   = 0;
    int applicationMetadataSize        = 0;
    long long seektableMetadataPos     = 0;
    int seektableMetadataSize          = 0;
    long long vorbisCommentMetadataPos = 0;
    int vorbisCommentMetadataSize      = 0;
    long long cuesheetMetadataPos      = 0;
    int cuesheetMetadataSize           = 0;
    long long pictureMetadataPos       = 0;
    int pictureMetadataSize            = 0;

    drflac_bool isLastBlock = streaminfo._isLastBlock;
    while (!isLastBlock)
    {
        unsigned char blockHeader[4];
        if (onRead(userData, blockHeader, sizeof(blockHeader)) != sizeof(blockHeader)) {
            return NULL;    // Unexpected end of file.
        }

        currentPos += sizeof(blockHeader);
        isLastBlock = (blockHeader[0] & 0x80) != 0;

        int blockSize = (int)drflac__read_u24(blockHeader + 1);
        int blockType = (blockHeader[0] & 0x7F);
        switch (blockType)
        {
            case DRFLAC_BLOCK_TYPE_APPLICATION:
            {
                applicationMetadataPos = currentPos;
                applicationMetadataSize = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_SEEKTABLE:
            {
                seektableMetadataPos = currentPos;
                seektableMetadataSize = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_VORBIS_COMMENT:
            {
                vorbisCommentMetadataPos = currentPos;
                vorbisCommentMetadataSize = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_CUESHEET:
            {
                cuesheetMetadataPos = currentPos;
                cuesheetMetadataSize = blockSize;
            } break;

            case DRFLAC_BLOCK_TYPE_PICTURE:
            {
                pictureMetadataPos = currentPos;
                pictureMetadataSize = blockSize;
            } break;

            
            // These blocks we either don't care about or aren't supporting.
            case DRFLAC_BLOCK_TYPE_PADDING:
            case DRFLAC_BLOCK_TYPE_INVALID:
            default: break;
        }

        currentPos += blockSize;

        if (!onSeek(userData, currentPos)) {
            return NULL;    // Failed to seek past this block. 
        }
    }

    
    // At this point we should be sitting right at the start of the very first frame.
    drflac* pFlac = malloc(sizeof(*pFlac) - sizeof(pFlac->decodedSamples) + (streaminfo.maxBlockSize * 4 * streaminfo.channels));     // x4 because a sample is at most 32-bit (4 bytes).
    if (pFlac == NULL) {
        return NULL;
    }

    pFlac->onRead                         = onRead;
    pFlac->onSeek                         = onSeek;
    pFlac->onTell                         = onTell;
    pFlac->userData                       = userData;
    pFlac->leftoverByte                   = 0;
    pFlac->leftoverBitsRemaining          = 0;
    pFlac->info                           = streaminfo;
    pFlac->applicationMetadataPos         = applicationMetadataPos;
    pFlac->applicationMetadataSize        = applicationMetadataSize;
    memset(&pFlac->currentFrameHeader, 0, sizeof(pFlac->currentFrameHeader));
    memset(&pFlac->currentSubframes, 0, sizeof(pFlac->currentSubframes));
    pFlac->samplesRemainingInCurrentFrame = 0;
    pFlac->nextSampleChannel              = 0;
    pFlac->decodedSampleCount             = 0;

    return pFlac;
}

void drflac_close(drflac* pFlac)
{
    free(pFlac);
}

unsigned int drflac_read(drflac* pFlac, unsigned int samplesToRead, void* bufferOut)
{
    if (pFlac == NULL || samplesToRead == 0 || bufferOut == NULL) {
        return 0;
    }

    unsigned int samplesRead = 0;
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
            // We still have samples remaining in the current frame so we need to decode it. This is where the real work is done.
            samplesRead += 1;
            pFlac->samplesRemainingInCurrentFrame -= 1;
        }
    }


    return samplesRead;
}


int drflac_decode_rice(int m, int rice)
{
    // This is quick test #1 - probably terrible.

    unsigned int sigbitCount = 0;
    for (int i = 31 - (m + 1); i >= 0; --i) {
        if ((rice & (1 << i)) == 0) {
            sigbitCount += 1;
        } else {
            break;
        }
    }

    unsigned int result = ~(0xFFFFFFFF << m) & (rice >> (31 - m));
    if (sigbitCount > 0) {
        result |= (1 << (sigbitCount + m - 1));
    }

    if ((rice & 0x80000000) != 0) {
        result = ~result + 1;
    }


    return (int)result;
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
