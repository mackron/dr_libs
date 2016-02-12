// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - This has not been tested on big-endian architectures.
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

    int blockSize;
    int sampleRate;
    int channelAssignment;
    int bitsPerSample;
    unsigned char crc8[8];

} drflac_FRAME_HEADER;

typedef struct
{
    /// The STREAMINFO block. Use this to find information like channel counts and sample rate.
    drflac_STREAMINFO info;

    /// The position in the file of the application as defined by the APPLICATION block, if any. A valid of 0 means it doesn't exist.
    long long applicationMetadataPos;

    /// The size of the application data. Does not include the registration ID or block header.
    int applicationMetadataSize;


    /// Information about the current frame.
    drflac_FRAME_HEADER currentFrameHeader;

    /// The number of samples remaining in the current frame. We use this to determine when a new frame needs to be begun.
    long long samplesRemainingInCurrentFrame;


    /// The current position of the read pointer. We need to keep track of this so we can calculate seek offsets.

} drflac;

/// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* drflac_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

/// Callback for when data needs to be seeked. Offset is always relative to the current position. Return value
/// is 0 on failure, non-zero success.
typedef int (* drflac_seek_proc)(void* userData, long long offsetFromStart);


/// Opens a flac decoder.
drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* userData);

/// Closes the given flac decoder.
void drflac_close(drflac* flac);



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

    return drflac_open(drflac__on_read_stdio, drflac__on_seek_stdio, pFile);
}
#endif  //DR_FLAC_NO_STDIO


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

static int drflac__read_streaminfo(drflac_read_proc onRead, void* userData, drflac_STREAMINFO* pInfo)
{
    unsigned char blockData[38];    // Total size of the STREAMINFO should be 38 bytes.
    if (onRead(userData, blockData, sizeof(blockData)) != sizeof(blockData)) {
        return drflac_false;
    }

    if ((blockData[0] & 0x7F) != DRFLAC_BLOCK_TYPE_STREAMINFO) {
        return drflac_false;            // The block is not a STREAMINFO block.
    }

    pInfo->_isLastBlock = (blockData[0] & 0x80) != 0;

    unsigned int blockSize = drflac__read_u24(blockData + 1);
    if (blockSize != 34) {
        return drflac_false;     // Expecting a block size of 34 for STREAMINFO.
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


drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* userData)
{
    if (onRead == NULL || onSeek == NULL) {
        return NULL;
    }

    unsigned char id[4];
    if (onRead(userData, id, 4) != 4 || id[0] != 'f' || id[1] != 'L' || id[2] != 'a' || id[3] != 'C') {
        return NULL;    // Not a flac file.
    }

    // The next chunk of data should be the STREAMINFO block.
    drflac_STREAMINFO streaminfo;
    if (!drflac__read_streaminfo(onRead, userData, &streaminfo)) {
        return NULL;    // Failed to read the STREAMINFO block.
    }

    long long currentPos = 42;  // No joke. 42 = 4 bytes "fLaC" header + 38 byte STREAMINFO block.

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

        if (!onSeek(userData, blockSize)) {
            return NULL;    // Failed to seek past this block. 
        }

        currentPos += blockSize;
    }

    
    // At this point we should be sitting right at the start of the very first frame.
    drflac* pFlac = malloc(sizeof(*pFlac));
    if (pFlac == NULL) {
        return NULL;
    }

    pFlac->info                    = streaminfo;
    pFlac->applicationMetadataPos  = applicationMetadataPos;
    pFlac->applicationMetadataSize = applicationMetadataSize;
    memset(&pFlac->currentFrameHeader, 0, sizeof(pFlac->currentFrameHeader));
    pFlac->samplesRemainingInCurrentFrame = 0;
    
    return NULL;
}

void drflac_close(drflac* flac)
{
    free(flac);
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
