// FLAC audio decoder. Public domain. See "unlicense" statement at the end of this file.
// dr_flac - v0.1a - 05/05/2016
//
// David Reid - mackron@gmail.com

// USAGE
//
// dr_flac is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_FLAC_IMPLEMENTATION
//   #include "dr_flac.h"
//
// You can then #include this file in other parts of the program as you would with any other header file. To decode audio data,
// do something like the following:
//
//     drflac* pFlac = drflac_open_file("MySong.flac");
//     if (pFlac == NULL) {
//         // Failed to open FLAC file
//     }
//
//     int32_t* pSamples = malloc(pFlac->totalSampleCount * sizeof(int32_t));
//     uint64_t numberOfInterleavedSamplesActuallyRead = drflac_read_s32(pFlac, pFlac->totalSampleCount, pSamples);
//
// The drflac object represents the decoder. It is a transparent type so all the information you need, such as the number of
// channels and the bits per sample, should be directly accessible - just make sure you don't change their values. Samples are
// always output as interleaved signed 32-bit PCM.
//
// You do not need to decode the entire stream in one go - you just specify how many samples you'd like at any given time and
// the decoder will give you as many samples as it can, up to the amount requested. Later on when you need the next batch of
// samples, just call it again. Example:
//
//     while (drflac_read_s32(pFlac, chunkSize, pChunkSamples) > 0) {
//         do_something();
//     }
//
// You can seek to a specific sample with drflac_seek_to_sample(). The given sample is based on interleaving. So for example,
// if you were to seek to the sample at index 0 in a stereo stream, you'll be seeking to the first sample of the left channel.
// The sample at index 1 will be the first sample of the right channel. The sample at index 2 will be the second sample of the
// left channel, etc.
//
//
// If you just want to quickly decode an entire FLAC file in one go you can do something like this:
//
//     unsigned int sampleRate;
//     unsigned int channels;
//     uint64_t totalSampleCount;
//     int32_t* pSampleData = drflac_open_and_decode_file("MySong.flac", &sampleRate, &channels, &totalSampleCount);
//     if (pSampleData == NULL) {
//         // Failed to open and decode FLAC file.
//     }
//
//     ...
//
//     drflac_free(pSampleData);
//
//
// If you need access to metadata (album art, etc.), use drflac_open_with_metadata(), drflac_open_file_with_metdata() or
// drflac_open_memory_with_metadata(). The rationale for keeping these APIs separate is that they're slightly slower than the
// normal versions and also just a little bit harder to use.
//
// dr_flac reports metadata to the application through the use of a callback, and every metadata block is reported before
// drflac_open_with_metdata() returns. See https://github.com/mackron/dr_libs_tests/blob/master/dr_flac/dr_flac_test_5.c for
// an example on how to read metadata.
//
//
//
// OPTIONS
// #define these options before including this file.
//
// #define DR_FLAC_NO_STDIO
//   Disable drflac_open_file().
//
// #define DR_FLAC_NO_WIN32_IO
//   Don't use the Win32 API internally for drflac_open_file(). Setting this will force stdio FILE APIs instead. This is
//   mainly for testing, but it's left here in case somebody might find use for it. dr_flac will use the Win32 API by
//   default. Ignored when DR_FLAC_NO_STDIO is #defined.
//
// #define DR_FLAC_BUFFER_SIZE <number>
//   Defines the size of the internal buffer to store data from onRead(). This buffer is used to reduce the number of calls
//   back to the client for more data. Larger values means more memory, but better performance. My tests show diminishing
//   returns after about 4KB (which is the default). Consider reducing this if you have a very efficient implementation of
//   onRead(), or increase it if it's very inefficient. Must be a multiple of 8.
//
//
//
// QUICK NOTES
// - Based on my tests, the 32-bit build is about about 1.1x-1.25x slower than the reference implementation. The 64-bit build is
//   at about parity.
// - dr_flac should work fine with valid native FLAC files, but for network based streams and whatnot it won't work if the header
//   and STREAMINFO block is unavailable.
// - Audio data is output as signed 32-bit PCM, regardless of the bits per sample the FLAC stream is encoded as.
// - This has not been tested on big-endian architectures.
// - Rice codes in unencoded binary form (see https://xiph.org/flac/format.html#rice_partition) has not been tested. If anybody
//   knows where I can find some test files for this, let me know.
// - Perverse and erroneous files have not been tested. Again, if you know where I can get some test files let me know.
// - dr_flac is not thread-safe, but it's APIs can be called from any thread so long as you do your own synchronization.
// - dr_flac does not currently do any CRC checks.
// - Ogg encapsulation is not supported, but will be added in the future.

#ifndef dr_flac_h
#define dr_flac_h

#include <stdint.h>
#include <stdbool.h>

// As data is read from the client it is placed into an internal buffer for fast access. This controls the
// size of that buffer. Larger values means more speed, but also more memory. In my testing there is diminishing
// returns after about 4KB, but you can fiddle with this to suit your own needs. Must be a multiple of 8.
#ifndef DR_FLAC_BUFFER_SIZE
#define DR_FLAC_BUFFER_SIZE   4096
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Check if we can enable 64-bit optimizations.
#if defined(_WIN64)
#define DRFLAC_64BIT
#endif

#if defined(__GNUC__)
#if defined(__x86_64__) || defined(__ppc64__)
#define DRFLAC_64BIT
#endif
#endif

#ifdef DRFLAC_64BIT
typedef uint64_t drflac_cache_t;
#else
typedef uint32_t drflac_cache_t;
#endif

// The various metadata block types.
#define DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO       0
#define DRFLAC_METADATA_BLOCK_TYPE_PADDING          1
#define DRFLAC_METADATA_BLOCK_TYPE_APPLICATION      2
#define DRFLAC_METADATA_BLOCK_TYPE_SEEKTABLE        3
#define DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT   4
#define DRFLAC_METADATA_BLOCK_TYPE_CUESHEET         5
#define DRFLAC_METADATA_BLOCK_TYPE_PICTURE          6
#define DRFLAC_METADATA_BLOCK_TYPE_INVALID          127

// The various picture types specified in the PICTURE block.
#define DRFLAC_PICTURE_TYPE_OTHER                   0
#define DRFLAC_PICTURE_TYPE_FILE_ICON               1
#define DRFLAC_PICTURE_TYPE_OTHER_FILE_ICON         2
#define DRFLAC_PICTURE_TYPE_COVER_FRONT             3
#define DRFLAC_PICTURE_TYPE_COVER_BACK              4
#define DRFLAC_PICTURE_TYPE_LEAFLET_PAGE            5
#define DRFLAC_PICTURE_TYPE_MEDIA                   6
#define DRFLAC_PICTURE_TYPE_LEAD_ARTIST             7
#define DRFLAC_PICTURE_TYPE_ARTIST                  8
#define DRFLAC_PICTURE_TYPE_CONDUCTOR               9
#define DRFLAC_PICTURE_TYPE_BAND                    10
#define DRFLAC_PICTURE_TYPE_COMPOSER                11
#define DRFLAC_PICTURE_TYPE_LYRICIST                12
#define DRFLAC_PICTURE_TYPE_RECORDING_LOCATION      13
#define DRFLAC_PICTURE_TYPE_DURING_RECORDING        14
#define DRFLAC_PICTURE_TYPE_DURING_PERFORMANCE      15
#define DRFLAC_PICTURE_TYPE_SCREEN_CAPTURE          16
#define DRFLAC_PICTURE_TYPE_BRIGHT_COLORED_FISH     17
#define DRFLAC_PICTURE_TYPE_ILLUSTRATION            18
#define DRFLAC_PICTURE_TYPE_BAND_LOGOTYPE           19
#define DRFLAC_PICTURE_TYPE_PUBLISHER_LOGOTYPE      20

// Packing is important on this structure because we map this directly to the raw data within the SEEKTABLE metadata block.
#pragma pack(2)
typedef struct
{
    uint64_t firstSample;
    uint64_t frameOffset;   // The offset from the first byte of the header of the first frame.
    uint16_t sampleCount;
} drflac_seekpoint;
#pragma pack()

typedef struct
{
    // The metadata type. Use this to know how to interpret the data below.
    uint32_t type;

    // A pointer to the raw data. This points to a temporary buffer so don't hold on to it. It's best to
    // not modify the contents of this buffer. Use the structures below for more meaningful and structured
    // information about the metadata. It's possible for this to be null.
    const void* pRawData;

    // The size in bytes of the block and the buffer pointed to by pRawData if it's non-NULL.
    uint32_t rawDataSize;

    union
    {
        struct
        {
            uint16_t minBlockSize;
            uint16_t maxBlockSize;
            uint32_t minFrameSize;
            uint32_t maxFrameSize;
            uint32_t sampleRate;
            uint8_t  channels;
            uint8_t  bitsPerSample;
            uint64_t totalSampleCount;
            uint8_t  md5[16];
        } streaminfo;

        struct
        {
            int unused;
        } padding;

        struct
        {
            uint32_t id;
            const void* pData;
            uint32_t dataSize;
        } application;

        struct
        {
            uint32_t seekpointCount;
            const drflac_seekpoint* pSeekpoints;
        } seektable;

        struct
        {
            uint32_t vendorLength;
            const char* vendor;
            uint32_t commentCount;
            const char* comments;
        } vorbis_comment;

        struct
        {
            char catalog[128];
            uint64_t leadInSampleCount;
            bool isCD;
            uint8_t trackCount;
            const uint8_t* pTrackData;
        } cuesheet;

        struct
        {
            uint32_t type;
            uint32_t mimeLength;
            const char* mime;
            uint32_t descriptionLength;
            const char* description;
            uint32_t width;
            uint32_t height;
            uint32_t colorDepth;
            uint32_t indexColorCount;
            uint32_t pictureDataSize;
            const uint8_t* pPictureData;
        } picture;
    } data;

} drflac_metadata;


// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* drflac_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

// Callback for when data needs to be seeked. Offset is always relative to the current position. Return value is false on failure, true success.
typedef bool (* drflac_seek_proc)(void* pUserData, int offset);

// Callback for when a metadata block is read.
typedef void (* drflac_meta_proc)(void* pUserData, drflac_metadata* pMetadata);


// Structure for internal use. Only used for decoders opened with drflac_open_memory.
typedef struct
{
    const unsigned char* data;
    size_t dataSize;
    size_t currentReadPos;
} drflac__memory_stream;

typedef struct
{
    // The type of the subframe: SUBFRAME_CONSTANT, SUBFRAME_VERBATIM, SUBFRAME_FIXED or SUBFRAME_LPC.
    uint8_t subframeType;

    // The number of wasted bits per sample as specified by the sub-frame header.
    uint8_t wastedBitsPerSample;

    // The order to use for the prediction stage for SUBFRAME_FIXED and SUBFRAME_LPC.
    uint8_t lpcOrder;

    // The number of bits per sample for this subframe. This is not always equal to the current frame's bit per sample because
    // an extra bit is required for side channels when interchannel decorrelation is being used.
    uint32_t bitsPerSample;

    // A pointer to the buffer containing the decoded samples in the subframe. This pointer is an offset from drflac::pExtraData, or
    // NULL if the heap is not being used. Note that it's a signed 32-bit integer for each value.
    int32_t* pDecodedSamples;

} drflac_subframe;

typedef struct
{
    // If the stream uses variable block sizes, this will be set to the index of the first sample. If fixed block sizes are used, this will
    // always be set to 0.
    uint64_t sampleNumber;

    // If the stream uses fixed block sizes, this will be set to the frame number. If variable block sizes are used, this will always be 0.
    uint32_t frameNumber;

    // The sample rate of this frame.
    uint32_t sampleRate;

    // The number of samples in each sub-frame within this frame.
    uint16_t blockSize;

    // The channel assignment of this frame. This is not always set to the channel count. If interchannel decorrelation is being used this
    // will be set to DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE, DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE or DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE.
    uint8_t channelAssignment;

    // The number of bits per sample within this frame.
    uint8_t bitsPerSample;

    // The frame's CRC. This is set, but unused at the moment.
    uint8_t crc8;

    // The number of samples left to be read in this frame. This is initially set to the block size multiplied by the channel count. As samples
    // are read, this will be decremented. When it reaches 0, the decoder will see this frame as fully consumed and load the next frame.
    uint32_t samplesRemaining;

    // The list of sub-frames within the frame. There is one sub-frame for each channel, and there's a maximum of 8 channels.
    drflac_subframe subframes[8];

} drflac_frame;

typedef struct
{
    // The function to call when more data needs to be read. This is set by drflac_open().
    drflac_read_proc onRead;

    // The function to call when the current read position needs to be moved.
    drflac_seek_proc onSeek;

    // The function to call when a metadata block is read.
    drflac_meta_proc onMeta;

    // The user data to pass around to onRead and onSeek.
    void* pUserData;

    // The user data posted to the metadata callback function. This will often be the same as pUserData, but will be different
    // for decoders opened with drflac_open_file_with_metadata() and drflac_open_memory_with_metadata().
    void* pUserDataMD;


    // The sample rate. Will be set to something like 44100.
    uint32_t sampleRate;

    // The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc. Maximum 8. This is set based on the
    // value specified in the STREAMINFO block.
    uint8_t channels;

    // The bits per sample. Will be set to somthing like 16, 24, etc.
    uint8_t bitsPerSample;

    // The maximum block size, in samples. This number represents the number of samples in each channel (not combined).
    uint16_t maxBlockSize;

    // The total number of samples making up the stream. This includes every channel. For example, if the stream has 2 channels,
    // with each channel having a total of 4096, this value will be set to 2*4096 = 8192.
    uint64_t totalSampleCount;


    // The position of the seektable in the file.
    uint64_t seektablePos;

    // The size of the seektable.
    uint32_t seektableSize;


    // Information about the frame the decoder is currently sitting on.
    drflac_frame currentFrame;

    // The position of the first frame in the stream. This is only ever used for seeking.
    uint64_t firstFramePos;


    // A hack to avoid a malloc() when opening a decoder with drflac_open_memory().
    drflac__memory_stream memoryStream;


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
    drflac_cache_t cache;
    drflac_cache_t cacheL2[DR_FLAC_BUFFER_SIZE/sizeof(drflac_cache_t)];


    // A pointer to the decoded sample data. This is an offset of pExtraData.
    int32_t* pDecodedSamples;

    // Variable length extra data. We attach this to the end of the object so we avoid unnecessary mallocs.
    uint8_t pExtraData[1];

} drflac;


// Opens a FLAC decoder.
//
// This is the lowest level function for opening a FLAC stream. You can also use drflac_open_file() and drflac_open_memory()
// to open the stream from a file or from a block of memory respectively.
//
// At the moment the STREAMINFO block must be present for this to succeed.
//
// The onRead and onSeek callbacks are used to read and seek data provided by the client.
drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData);

// Opens a FLAC decoder and notifies the caller of the metadata chunks (album art, etc.).
//
// This is slower that drflac_open(), so avoid this one if you don't need metadata. Internally, this will do a malloc()
// and free() for every metadata block except for STREAMINFO and PADDING blocks.
//
// The caller is notified of the metadata via the onMeta callback. The metadata will all be handled before the function
// returns.
drflac* drflac_open_with_metadata(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData);

// Closes the given FLAC decoder.
void drflac_close(drflac* pFlac);


// Reads sample data from the given FLAC decoder, output as interleaved signed 32-bit PCM.
//
// Returns the number of samples actually read.
uint64_t drflac_read_s32(drflac* pFlac, uint64_t samplesToRead, int32_t* pBufferOut);

// Seeks to the sample at the given index.
bool drflac_seek_to_sample(drflac* pFlac, uint64_t sampleIndex);



#ifndef DR_FLAC_NO_STDIO
// Opens a FLAC decoder from the file at the given path.
drflac* drflac_open_file(const char* filename);

// Opens a FLAC decoder from the file at the given path and notifies the caller of the metadata chunks (album art, etc.)
//
// Look at the documentation for drflac_open_with_metadata() for more information on how metadata is handled.
drflac* drflac_open_file_with_metadata(const char* filename, drflac_meta_proc onMeta, void* pUserData);
#endif

// Opens a FLAC decoder from a pre-allocated block of memory
//
// This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
// the lifetime of the decoder.
drflac* drflac_open_memory(const void* data, size_t dataSize);

// Opens a FLAC decoder from a pre-allocated block of memory and notifies the caller of the metadata chunks (album art, etc.)
//
// Look at the documentation for drflac_open_with_metadata() for more information on how metadata is handled.
drflac* drflac_open_memory_with_metadata(const void* data, size_t dataSize, drflac_meta_proc onMeta, void* pUserData);



//// High Level APIs ////

// Opens a FLAC stream from the given callbacks and fully decodes it in a single operation. The return value is a
// pointer to the sample data as interleaved signed 32-bit PCM. The returned data must be freed with drflac_free().
int32_t* drflac_open_and_decode(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount);

#ifndef DR_FLAC_NO_STDIO
// Opens a FLAC file and fully decodes it in a single operation. The return value is a pointer to the sample data
// as interleaved signed 32-bit PCM. The returned data must be freed with drflac_free().
int32_t* drflac_open_and_decode_file(const char* filename, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount);
#endif

// Opens a FLAC file from a block of memory and fully decodes it in a single operation. The return value is a pointer
// to the sample data as interleaved signed 32-bit PCM. The returned data must be freed with drflac_free().
int32_t* drflac_open_and_decode_memory(const void* data, size_t dataSize, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount);

// Frees data returned by drflac_open_and_decode_*().
void drflac_free(void* pSampleDataReturnedByOpenAndDecode);


// Structure representing an iterator for vorbis comments in a VORBIS_COMMENT metadata block.
typedef struct
{
    uint32_t countRemaining;
    const char* pRunningData;
} drflac_vorbis_comment_iterator;

// Initializes a vorbis comment iterator. This can be used for iterating over the vorbis comments in a VORBIS_COMMENT
// metadata block.
void drflac_init_vorbis_comment_iterator(drflac_vorbis_comment_iterator* pIter, uint32_t commentCount, const char* pComments);

// Goes to the next vorbis comment in the given iterator. If null is returned it means there are no more comments. The
// returned string is NOT null terminated.
const char* drflac_next_vorbis_comment(drflac_vorbis_comment_iterator* pIter, uint32_t* pCommentLengthOut);



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

#ifdef _MSC_VER
#define DRFLAC_INLINE __forceinline
#else
#define DRFLAC_INLINE inline
#endif

#define DRFLAC_SUBFRAME_CONSTANT                        0
#define DRFLAC_SUBFRAME_VERBATIM                        1
#define DRFLAC_SUBFRAME_FIXED                           8
#define DRFLAC_SUBFRAME_LPC                             32
#define DRFLAC_SUBFRAME_RESERVED                        255

#define DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE  0
#define DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2 1

#define DRFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT           0
#define DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE             8
#define DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE            9
#define DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE              10


//// Endian Management ////
static DRFLAC_INLINE bool drflac__is_little_endian()
{
    int n = 1;
    return (*(char*)&n) == 1;
}

static DRFLAC_INLINE uint16_t drflac__swap_endian_uint16(uint16_t n)
{
#ifdef _MSC_VER
    return _byteswap_ushort(n);
#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC__ >= 3))
    return __builtin_bswap16(n);
#else
    return ((n & 0xFF00) >> 8) |
           ((n & 0x00FF) << 8);
#endif
}

static DRFLAC_INLINE uint32_t drflac__swap_endian_uint32(uint32_t n)
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

static DRFLAC_INLINE uint64_t drflac__swap_endian_uint64(uint64_t n)
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

static DRFLAC_INLINE uint16_t drflac__be2host_16(uint16_t n)
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

static DRFLAC_INLINE uint32_t drflac__be2host_32(uint32_t n)
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

static DRFLAC_INLINE uint64_t drflac__be2host_64(uint64_t n)
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


static DRFLAC_INLINE uint32_t drflac__le2host_32(uint32_t n)
{
#ifdef __linux__
    return le32toh(n);
#else
    if (!drflac__is_little_endian()) {
        return drflac__swap_endian_uint32(n);
    }

    return n;
#endif
}


#ifdef DRFLAC_64BIT
#define drflac__be2host__cache_line drflac__be2host_64
#else
#define drflac__be2host__cache_line drflac__be2host_32
#endif


// BIT READING ATTEMPT #2
//
// This uses a 32- or 64-bit bit-shifted cache - as bits are read, the cache is shifted such that the first valid bit is sitting
// on the most significant bit. It uses the notion of an L1 and L2 cache (borrowed from CPU architecture), where the L1 cache
// is a 32- or 64-bit unsigned integer (depending on whether or not a 32- or 64-bit build is being compiled) and the L2 is an
// array of "cache lines", with each cache line being the same size as the L1. The L2 is a buffer of about 4KB and is where data
// from onRead() is read into.
#define DRFLAC_CACHE_L1_SIZE_BYTES                  (sizeof(pFlac->cache))
#define DRFLAC_CACHE_L1_SIZE_BITS                   (sizeof(pFlac->cache)*8)
#define DRFLAC_CACHE_L1_BITS_REMAINING              (DRFLAC_CACHE_L1_SIZE_BITS - (pFlac->consumedBits))
#ifdef DRFLAC_64BIT
#define DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount)   (~(((uint64_t)-1LL) >> (_bitCount)))
#else
#define DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount)   (~(((uint32_t)-1) >> (_bitCount)))
#endif
#define DRFLAC_CACHE_L1_SELECTION_SHIFT(_bitCount)  (DRFLAC_CACHE_L1_SIZE_BITS - (_bitCount))
#define DRFLAC_CACHE_L1_SELECT(_bitCount)           ((pFlac->cache) & DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount))
#define DRFLAC_CACHE_L1_SELECT_AND_SHIFT(_bitCount) (DRFLAC_CACHE_L1_SELECT(_bitCount) >> DRFLAC_CACHE_L1_SELECTION_SHIFT(_bitCount))
#define DRFLAC_CACHE_L2_SIZE_BYTES                  (sizeof(pFlac->cacheL2))
#define DRFLAC_CACHE_L2_LINE_COUNT                  (DRFLAC_CACHE_L2_SIZE_BYTES / sizeof(pFlac->cacheL2[0]))
#define DRFLAC_CACHE_L2_LINES_REMAINING             (DRFLAC_CACHE_L2_LINE_COUNT - pFlac->nextL2Line)

static DRFLAC_INLINE bool drflac__reload_l1_cache_from_l2(drflac* pFlac)
{
    // Fast path. Try loading straight from L2.
    if (pFlac->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT) {
        pFlac->cache = pFlac->cacheL2[pFlac->nextL2Line++];
        return true;
    }

    // If we get here it means we've run out of data in the L2 cache. We'll need to fetch more from the client.
    size_t bytesRead = pFlac->onRead(pFlac->pUserData, pFlac->cacheL2, DRFLAC_CACHE_L2_SIZE_BYTES);
    pFlac->currentBytePos += bytesRead;

    pFlac->nextL2Line = 0;
    if (bytesRead == DRFLAC_CACHE_L2_SIZE_BYTES) {
        pFlac->cache = pFlac->cacheL2[pFlac->nextL2Line++];
        return true;
    }


    // If we get here it means we were unable to retrieve enough data to fill the entire L2 cache. It probably
    // means we've just reached the end of the file. We need to move the valid data down to the end of the buffer
    // and adjust the index of the next line accordingly. Also keep in mind that the L2 cache must be aligned to
    // the size of the L1 so we'll need to seek backwards by any misaligned bytes.
    size_t alignedL1LineCount = bytesRead / DRFLAC_CACHE_L1_SIZE_BYTES;
    if (alignedL1LineCount > 0)
    {
        size_t offset = DRFLAC_CACHE_L2_LINE_COUNT - alignedL1LineCount;
        for (size_t i = alignedL1LineCount; i > 0; --i) {
            pFlac->cacheL2[i-1 + offset] = pFlac->cacheL2[i-1];
        }

        pFlac->nextL2Line = offset;
        pFlac->unusedL2Lines = offset;

        // At this point there may be some leftover unaligned bytes. We need to seek backwards so we don't lose
        // those bytes.
        size_t unalignedBytes = bytesRead - (alignedL1LineCount * DRFLAC_CACHE_L1_SIZE_BYTES);
        if (unalignedBytes > 0) {
            pFlac->onSeek(pFlac->pUserData, -(int)unalignedBytes);
            pFlac->currentBytePos -= unalignedBytes;
        }

        pFlac->cache = pFlac->cacheL2[pFlac->nextL2Line++];
        return true;
    }
    else
    {
        // If we get into this branch it means we weren't able to load any L1-aligned data. We just need to seek
        // backwards by the leftover bytes and return false.
        if (bytesRead > 0) {
            pFlac->onSeek(pFlac->pUserData, -(int)bytesRead);
            pFlac->currentBytePos -= bytesRead;
        }

        pFlac->nextL2Line = DRFLAC_CACHE_L2_LINE_COUNT;
        return false;
    }
}

static bool drflac__reload_cache(drflac* pFlac)
{
    // Fast path. Try just moving the next value in the L2 cache to the L1 cache.
    if (drflac__reload_l1_cache_from_l2(pFlac)) {
        pFlac->cache = drflac__be2host__cache_line(pFlac->cache);
        pFlac->consumedBits = 0;
        return true;
    }

    // Slow path.

    // If we get here it means we have failed to load the L1 cache from the L2. Likely we've just reached the end of the stream and the last
    // few bytes did not meet the alignment requirements for the L2 cache. In this case we need to fall back to a slower path and read the
    // data straight from the client into the L1 cache. This should only really happen once per stream so efficiency is not important.
    size_t bytesRead = pFlac->onRead(pFlac->pUserData, &pFlac->cache, DRFLAC_CACHE_L1_SIZE_BYTES);
    if (bytesRead == 0) {
        return false;
    }

    pFlac->currentBytePos += bytesRead;

    assert(bytesRead < DRFLAC_CACHE_L1_SIZE_BYTES);
    pFlac->consumedBits = (DRFLAC_CACHE_L1_SIZE_BYTES - bytesRead) * 8;

    pFlac->cache = drflac__be2host__cache_line(pFlac->cache);
    pFlac->cache &= DRFLAC_CACHE_L1_SELECTION_MASK(DRFLAC_CACHE_L1_SIZE_BITS - pFlac->consumedBits);    // <-- Make sure the consumed bits are always set to zero. Other parts of the library depend on this property.
    return true;
}

static bool drflac__seek_bits(drflac* pFlac, size_t bitsToSeek)
{
    if (bitsToSeek <= DRFLAC_CACHE_L1_BITS_REMAINING) {
        pFlac->consumedBits += bitsToSeek;
        pFlac->cache <<= bitsToSeek;
        return true;
    } else {
        // It straddles the cached data. This function isn't called too frequently so I'm favouring simplicity here.
        bitsToSeek -= DRFLAC_CACHE_L1_BITS_REMAINING;
        pFlac->consumedBits += DRFLAC_CACHE_L1_BITS_REMAINING;
        pFlac->cache = 0;

        size_t wholeBytesRemaining = bitsToSeek/8;
        if (wholeBytesRemaining > 0)
        {
            // The next bytes to seek will be located in the L2 cache. The problem is that the L2 cache is not byte aligned,
            // but rather DRFLAC_CACHE_L1_SIZE_BYTES aligned (usually 4 or 8). If, for example, the number of bytes to seek is
            // 3, we'll need to handle it in a special way.
            size_t wholeCacheLinesRemaining = wholeBytesRemaining / DRFLAC_CACHE_L1_SIZE_BYTES;
            if (wholeCacheLinesRemaining < DRFLAC_CACHE_L2_LINES_REMAINING)
            {
                wholeBytesRemaining -= wholeCacheLinesRemaining * DRFLAC_CACHE_L1_SIZE_BYTES;
                bitsToSeek -= wholeCacheLinesRemaining * DRFLAC_CACHE_L1_SIZE_BITS;
                pFlac->nextL2Line += wholeCacheLinesRemaining;
            }
            else
            {
                wholeBytesRemaining -= DRFLAC_CACHE_L2_LINES_REMAINING * DRFLAC_CACHE_L1_SIZE_BYTES;
                bitsToSeek -= DRFLAC_CACHE_L2_LINES_REMAINING * DRFLAC_CACHE_L1_SIZE_BITS;
                pFlac->nextL2Line += DRFLAC_CACHE_L2_LINES_REMAINING;

                pFlac->onSeek(pFlac->pUserData, (int)wholeBytesRemaining);
                pFlac->currentBytePos += wholeBytesRemaining;
                bitsToSeek -= wholeBytesRemaining*8;
            }
        }


        if (bitsToSeek > 0) {
            if (!drflac__reload_cache(pFlac)) {
                return false;
            }

            return drflac__seek_bits(pFlac, bitsToSeek);
        }

        return true;
    }
}

static bool drflac__read_uint32(drflac* pFlac, unsigned int bitCount, uint32_t* pResultOut)
{
    assert(pFlac != NULL);
    assert(pResultOut != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 32);

    if (pFlac->consumedBits == DRFLAC_CACHE_L1_SIZE_BITS) {
        if (!drflac__reload_cache(pFlac)) {
            return false;
        }
    }

    if (bitCount <= DRFLAC_CACHE_L1_BITS_REMAINING) {
        if (bitCount < DRFLAC_CACHE_L1_SIZE_BITS) {
            *pResultOut = DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bitCount);
            pFlac->consumedBits += bitCount;
            pFlac->cache <<= bitCount;
        } else {
            *pResultOut = (uint32_t)pFlac->cache;
            pFlac->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS;
            pFlac->cache = 0;
        }
        return true;
    } else {
        // It straddles the cached data. It will never cover more than the next chunk. We just read the number in two parts and combine them.
        size_t bitCountHi = DRFLAC_CACHE_L1_BITS_REMAINING;
        size_t bitCountLo = bitCount - bitCountHi;
        uint32_t resultHi = DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bitCountHi);

        if (!drflac__reload_cache(pFlac)) {
            return false;
        }

        *pResultOut = (resultHi << bitCountLo) | DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bitCountLo);
        pFlac->consumedBits += bitCountLo;
        pFlac->cache <<= bitCountLo;
        return true;
    }
}

static bool drflac__read_int32(drflac* pFlac, unsigned int bitCount, int32_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 32);

    uint32_t result;
    if (!drflac__read_uint32(pFlac, bitCount, &result)) {
        return false;
    }

    if ((result & (1 << (bitCount - 1)))) {  // TODO: See if we can get rid of this branch.
        result |= (-1 << bitCount);
    }

    *pResult = (int32_t)result;
    return true;
}

static bool drflac__read_uint64(drflac* pFlac, unsigned int bitCount, uint64_t* pResultOut)
{
    assert(bitCount <= 64);
    assert(bitCount >  32);

    uint32_t resultHi;
    if (!drflac__read_uint32(pFlac, bitCount - 32, &resultHi)) {
        return false;
    }

    uint32_t resultLo;
    if (!drflac__read_uint32(pFlac, 32, &resultLo)) {
        return false;
    }

    *pResultOut = (((uint64_t)resultHi) << 32) | ((uint64_t)resultLo);
    return true;
}

// Function below is unused, but leaving it here in case I need to quickly add it again.
#if 0
static bool drflac__read_int64(drflac* pFlac, unsigned int bitCount, int64_t* pResultOut)
{
    assert(bitCount <= 64);

    uint64_t result;
    if (!drflac__read_uint64(pFlac, bitCount, &result)) {
        return false;
    }

    if ((result & (1ULL << (bitCount - 1)))) {  // TODO: See if we can get rid of this branch.
        result |= (-1LL << bitCount);
    }

    *pResultOut = (int64_t)result;
    return true;
}
#endif

static bool drflac__read_uint16(drflac* pFlac, unsigned int bitCount, uint16_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 16);

    uint32_t result;
    if (!drflac__read_uint32(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (uint16_t)result;
    return true;
}

static bool drflac__read_int16(drflac* pFlac, unsigned int bitCount, int16_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 16);

    int32_t result;
    if (!drflac__read_int32(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (int16_t)result;
    return true;
}

static bool drflac__read_uint8(drflac* pFlac, unsigned int bitCount, uint8_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 8);

    uint32_t result;
    if (!drflac__read_uint32(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (uint8_t)result;
    return true;
}

static bool drflac__read_int8(drflac* pFlac, unsigned int bitCount, int8_t* pResult)
{
    assert(pFlac != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 8);

    int32_t result;
    if (!drflac__read_int32(pFlac, bitCount, &result)) {
        return false;
    }

    *pResult = (int8_t)result;
    return true;
}


static inline bool drflac__seek_past_next_set_bit(drflac* pFlac, unsigned int* pOffsetOut)
{
    unsigned int zeroCounter = 0;
    while (pFlac->cache == 0) {
        zeroCounter += (unsigned int)DRFLAC_CACHE_L1_BITS_REMAINING;
        if (!drflac__reload_cache(pFlac)) {
            return false;
        }
    }

    // At this point the cache should not be zero, in which case we know the first set bit should be somewhere in here. There is
    // no need for us to perform any cache reloading logic here which should make things much faster.
    assert(pFlac->cache != 0);

    unsigned int bitOffsetTable[] = {
        0,
        4,
        3, 3,
        2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1
    };

    unsigned int setBitOffsetPlus1 = bitOffsetTable[DRFLAC_CACHE_L1_SELECT_AND_SHIFT(4)];
    if (setBitOffsetPlus1 == 0) {
        if (pFlac->cache == 1) {
            setBitOffsetPlus1 = DRFLAC_CACHE_L1_SIZE_BITS;
        } else {
            setBitOffsetPlus1 = 5;
            for (;;)
            {
                if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(setBitOffsetPlus1))) {
                    break;
                }

                setBitOffsetPlus1 += 1;
            }
        }
    }

    pFlac->consumedBits += setBitOffsetPlus1;
    pFlac->cache <<= setBitOffsetPlus1;

    *pOffsetOut = zeroCounter + setBitOffsetPlus1 - 1;
    return true;
}



static bool drflac__seek_to_byte(drflac* pFlac, long long offsetFromStart)
{
    assert(pFlac != NULL);

    long long bytesToMove = offsetFromStart - pFlac->currentBytePos;
    if (bytesToMove == 0) {
        return 1;
    }

    if (bytesToMove > 0x7FFFFFFF) {
        while (bytesToMove > 0x7FFFFFFF) {
            if (!pFlac->onSeek(pFlac->pUserData, 0x7FFFFFFF)) {
                return 0;
            }

            pFlac->currentBytePos += 0x7FFFFFFF;
            bytesToMove -= 0x7FFFFFFF;
        }
    } else {
        while (bytesToMove < (int)0x80000000) {
            if (!pFlac->onSeek(pFlac->pUserData, (int)0x80000000)) {
                return 0;
            }

            pFlac->currentBytePos += (int)0x80000000;
            bytesToMove -= (int)0x80000000;
        }
    }

    assert(bytesToMove <= 0x7FFFFFFF && bytesToMove >= (int)0x80000000);

    bool result = pFlac->onSeek(pFlac->pUserData, (int)bytesToMove);    // <-- Safe cast as per the assert above.
    pFlac->currentBytePos += (int)bytesToMove;

    pFlac->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS;
    pFlac->cache = 0;
    pFlac->nextL2Line = DRFLAC_CACHE_L2_LINE_COUNT; // <-- This clears the L2 cache.

    return result;
}


static bool drflac__read_utf8_coded_number(drflac* pFlac, uint64_t* pNumberOut)
{
    assert(pFlac != NULL);
    assert(pNumberOut != NULL);

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



static DRFLAC_INLINE bool drflac__read_and_seek_rice(drflac* pFlac, unsigned char m)
{
    unsigned int unused;
    if (!drflac__seek_past_next_set_bit(pFlac, &unused)) {
        return false;
    }

    if (m > 0) {
        if (!drflac__seek_bits(pFlac, m)) {
            return false;
        }
    }

    return true;
}


// The next two functions are responsible for calculating the prediction.
//
// When the bits per sample is >16 we need to use 64-bit integer arithmetic because otherwise we'll run out of precision. It's
// safe to assume this will be slower on 32-bit platforms so we use a more optimal solution when the bits per sample is <=16.
//
//
// Optimization Experiment #1
//
// The first optimization experiment I'm trying here is a loop unroll for the most common LPC orders. I've done a little test
// and the results are as follows, in order of most common:
// 1)  order = 8  : 93.1M
// 2)  order = 7  : 36.6M
// 3)  order = 3  : 33.2M
// 4)  order = 6  : 20.9M
// 5)  order = 5  : 18.1M
// 6)  order = 4  : 15.8M
// 7)  order = 12 : 10.8M
// 8)  order = 2  :  9.8M
// 9)  order = 1  :  1.6M
// 10) order = 10 :  1.0M
// 11) order = 9  :  0.8M
// 12) order = 11 :  0.8M
//
// We'll experiment with unrolling the top 8 most common ones. We'll ignore the least common ones since there seems to be a
// large drop off there.
//
// Result: There's a tiny improvement in some cases, but it could just be within margin of error so unsure if it's worthwhile
// just yet.
static DRFLAC_INLINE int32_t drflac__calculate_prediction_32(unsigned int order, int shift, const short* coefficients, int32_t* pDecodedSamples)
{
    assert(order <= 32);

    // 32-bit version.

    // This method is slower on both 32- and 64-bit builds with VC++. Leaving this here for now just in case we need it later
    // for whatever reason.
#if 0
    int prediction;
    if (order == 8)
    {
        prediction  = coefficients[0] * pDecodedSamples[-1];
        prediction += coefficients[1] * pDecodedSamples[-2];
        prediction += coefficients[2] * pDecodedSamples[-3];
        prediction += coefficients[3] * pDecodedSamples[-4];
        prediction += coefficients[4] * pDecodedSamples[-5];
        prediction += coefficients[5] * pDecodedSamples[-6];
        prediction += coefficients[6] * pDecodedSamples[-7];
        prediction += coefficients[7] * pDecodedSamples[-8];
    }
    else if (order == 7)
    {
        prediction  = coefficients[0] * pDecodedSamples[-1];
        prediction += coefficients[1] * pDecodedSamples[-2];
        prediction += coefficients[2] * pDecodedSamples[-3];
        prediction += coefficients[3] * pDecodedSamples[-4];
        prediction += coefficients[4] * pDecodedSamples[-5];
        prediction += coefficients[5] * pDecodedSamples[-6];
        prediction += coefficients[6] * pDecodedSamples[-7];
    }
    else if (order == 3)
    {
        prediction  = coefficients[0] * pDecodedSamples[-1];
        prediction += coefficients[1] * pDecodedSamples[-2];
        prediction += coefficients[2] * pDecodedSamples[-3];
    }
    else if (order == 6)
    {
        prediction  = coefficients[0] * pDecodedSamples[-1];
        prediction += coefficients[1] * pDecodedSamples[-2];
        prediction += coefficients[2] * pDecodedSamples[-3];
        prediction += coefficients[3] * pDecodedSamples[-4];
        prediction += coefficients[4] * pDecodedSamples[-5];
        prediction += coefficients[5] * pDecodedSamples[-6];
    }
    else if (order == 5)
    {
        prediction  = coefficients[0] * pDecodedSamples[-1];
        prediction += coefficients[1] * pDecodedSamples[-2];
        prediction += coefficients[2] * pDecodedSamples[-3];
        prediction += coefficients[3] * pDecodedSamples[-4];
        prediction += coefficients[4] * pDecodedSamples[-5];
    }
    else if (order == 4)
    {
        prediction  = coefficients[0] * pDecodedSamples[-1];
        prediction += coefficients[1] * pDecodedSamples[-2];
        prediction += coefficients[2] * pDecodedSamples[-3];
        prediction += coefficients[3] * pDecodedSamples[-4];
    }
    else if (order == 12)
    {
        prediction  = coefficients[0]  * pDecodedSamples[-1];
        prediction += coefficients[1]  * pDecodedSamples[-2];
        prediction += coefficients[2]  * pDecodedSamples[-3];
        prediction += coefficients[3]  * pDecodedSamples[-4];
        prediction += coefficients[4]  * pDecodedSamples[-5];
        prediction += coefficients[5]  * pDecodedSamples[-6];
        prediction += coefficients[6]  * pDecodedSamples[-7];
        prediction += coefficients[7]  * pDecodedSamples[-8];
        prediction += coefficients[8]  * pDecodedSamples[-9];
        prediction += coefficients[9]  * pDecodedSamples[-10];
        prediction += coefficients[10] * pDecodedSamples[-11];
        prediction += coefficients[11] * pDecodedSamples[-12];
    }
    else if (order == 2)
    {
        prediction  = coefficients[0] * pDecodedSamples[-1];
        prediction += coefficients[1] * pDecodedSamples[-2];
    }
    else if (order == 1)
    {
        prediction = (long long)coefficients[0] * (long long)pDecodedSamples[-1];
    }
    else if (order == 10)
    {
        prediction  = coefficients[0]  * pDecodedSamples[-1];
        prediction += coefficients[1]  * pDecodedSamples[-2];
        prediction += coefficients[2]  * pDecodedSamples[-3];
        prediction += coefficients[3]  * pDecodedSamples[-4];
        prediction += coefficients[4]  * pDecodedSamples[-5];
        prediction += coefficients[5]  * pDecodedSamples[-6];
        prediction += coefficients[6]  * pDecodedSamples[-7];
        prediction += coefficients[7]  * pDecodedSamples[-8];
        prediction += coefficients[8]  * pDecodedSamples[-9];
        prediction += coefficients[9]  * pDecodedSamples[-10];
    }
    else if (order == 9)
    {
        prediction  = coefficients[0]  * pDecodedSamples[-1];
        prediction += coefficients[1]  * pDecodedSamples[-2];
        prediction += coefficients[2]  * pDecodedSamples[-3];
        prediction += coefficients[3]  * pDecodedSamples[-4];
        prediction += coefficients[4]  * pDecodedSamples[-5];
        prediction += coefficients[5]  * pDecodedSamples[-6];
        prediction += coefficients[6]  * pDecodedSamples[-7];
        prediction += coefficients[7]  * pDecodedSamples[-8];
        prediction += coefficients[8]  * pDecodedSamples[-9];
    }
    else if (order == 11)
    {
        prediction  = coefficients[0]  * pDecodedSamples[-1];
        prediction += coefficients[1]  * pDecodedSamples[-2];
        prediction += coefficients[2]  * pDecodedSamples[-3];
        prediction += coefficients[3]  * pDecodedSamples[-4];
        prediction += coefficients[4]  * pDecodedSamples[-5];
        prediction += coefficients[5]  * pDecodedSamples[-6];
        prediction += coefficients[6]  * pDecodedSamples[-7];
        prediction += coefficients[7]  * pDecodedSamples[-8];
        prediction += coefficients[8]  * pDecodedSamples[-9];
        prediction += coefficients[9]  * pDecodedSamples[-10];
        prediction += coefficients[10] * pDecodedSamples[-11];
    }
    else
    {
        prediction = 0;
        for (int j = 0; j < (int)order; ++j) {
            prediction += coefficients[j] * pDecodedSamples[-j-1];
        }
    }
#endif

    // Experiment #2. See if we can use a switch and let the compiler optimize it to a jump table.
    // Result: VC++ definitely optimizes this to a single jmp as expected. I expect other compilers should do the same, but I've
    // not verified yet.
#if 1
    int prediction = 0;

    switch (order)
    {
    case 32: prediction += coefficients[31] * pDecodedSamples[-32];
    case 31: prediction += coefficients[30] * pDecodedSamples[-31];
    case 30: prediction += coefficients[29] * pDecodedSamples[-30];
    case 29: prediction += coefficients[28] * pDecodedSamples[-29];
    case 28: prediction += coefficients[27] * pDecodedSamples[-28];
    case 27: prediction += coefficients[26] * pDecodedSamples[-27];
    case 26: prediction += coefficients[25] * pDecodedSamples[-26];
    case 25: prediction += coefficients[24] * pDecodedSamples[-25];
    case 24: prediction += coefficients[23] * pDecodedSamples[-24];
    case 23: prediction += coefficients[22] * pDecodedSamples[-23];
    case 22: prediction += coefficients[21] * pDecodedSamples[-22];
    case 21: prediction += coefficients[20] * pDecodedSamples[-21];
    case 20: prediction += coefficients[19] * pDecodedSamples[-20];
    case 19: prediction += coefficients[18] * pDecodedSamples[-19];
    case 18: prediction += coefficients[17] * pDecodedSamples[-18];
    case 17: prediction += coefficients[16] * pDecodedSamples[-17];
    case 16: prediction += coefficients[15] * pDecodedSamples[-16];
    case 15: prediction += coefficients[14] * pDecodedSamples[-15];
    case 14: prediction += coefficients[13] * pDecodedSamples[-14];
    case 13: prediction += coefficients[12] * pDecodedSamples[-13];
    case 12: prediction += coefficients[11] * pDecodedSamples[-12];
    case 11: prediction += coefficients[10] * pDecodedSamples[-11];
    case 10: prediction += coefficients[ 9] * pDecodedSamples[-10];
    case  9: prediction += coefficients[ 8] * pDecodedSamples[- 9];
    case  8: prediction += coefficients[ 7] * pDecodedSamples[- 8];
    case  7: prediction += coefficients[ 6] * pDecodedSamples[- 7];
    case  6: prediction += coefficients[ 5] * pDecodedSamples[- 6];
    case  5: prediction += coefficients[ 4] * pDecodedSamples[- 5];
    case  4: prediction += coefficients[ 3] * pDecodedSamples[- 4];
    case  3: prediction += coefficients[ 2] * pDecodedSamples[- 3];
    case  2: prediction += coefficients[ 1] * pDecodedSamples[- 2];
    case  1: prediction += coefficients[ 0] * pDecodedSamples[- 1];
    }
#endif

    return (int32_t)(prediction >> shift);
}

static DRFLAC_INLINE int32_t drflac__calculate_prediction(unsigned int order, int shift, const short* coefficients, int32_t* pDecodedSamples)
{
    assert(order <= 32);

    // 64-bit version.

    // This method is faster on the 32-bit build when compiling with VC++. See note below.
#ifndef DRFLAC_64BIT
    long long prediction;
    if (order == 8)
    {
        prediction  = (long long)coefficients[0] * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1] * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2] * (long long)pDecodedSamples[-3];
        prediction += (long long)coefficients[3] * (long long)pDecodedSamples[-4];
        prediction += (long long)coefficients[4] * (long long)pDecodedSamples[-5];
        prediction += (long long)coefficients[5] * (long long)pDecodedSamples[-6];
        prediction += (long long)coefficients[6] * (long long)pDecodedSamples[-7];
        prediction += (long long)coefficients[7] * (long long)pDecodedSamples[-8];
    }
    else if (order == 7)
    {
        prediction  = (long long)coefficients[0] * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1] * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2] * (long long)pDecodedSamples[-3];
        prediction += (long long)coefficients[3] * (long long)pDecodedSamples[-4];
        prediction += (long long)coefficients[4] * (long long)pDecodedSamples[-5];
        prediction += (long long)coefficients[5] * (long long)pDecodedSamples[-6];
        prediction += (long long)coefficients[6] * (long long)pDecodedSamples[-7];
    }
    else if (order == 3)
    {
        prediction  = (long long)coefficients[0] * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1] * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2] * (long long)pDecodedSamples[-3];
    }
    else if (order == 6)
    {
        prediction  = (long long)coefficients[0] * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1] * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2] * (long long)pDecodedSamples[-3];
        prediction += (long long)coefficients[3] * (long long)pDecodedSamples[-4];
        prediction += (long long)coefficients[4] * (long long)pDecodedSamples[-5];
        prediction += (long long)coefficients[5] * (long long)pDecodedSamples[-6];
    }
    else if (order == 5)
    {
        prediction  = (long long)coefficients[0] * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1] * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2] * (long long)pDecodedSamples[-3];
        prediction += (long long)coefficients[3] * (long long)pDecodedSamples[-4];
        prediction += (long long)coefficients[4] * (long long)pDecodedSamples[-5];
    }
    else if (order == 4)
    {
        prediction  = (long long)coefficients[0] * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1] * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2] * (long long)pDecodedSamples[-3];
        prediction += (long long)coefficients[3] * (long long)pDecodedSamples[-4];
    }
    else if (order == 12)
    {
        prediction  = (long long)coefficients[0]  * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1]  * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2]  * (long long)pDecodedSamples[-3];
        prediction += (long long)coefficients[3]  * (long long)pDecodedSamples[-4];
        prediction += (long long)coefficients[4]  * (long long)pDecodedSamples[-5];
        prediction += (long long)coefficients[5]  * (long long)pDecodedSamples[-6];
        prediction += (long long)coefficients[6]  * (long long)pDecodedSamples[-7];
        prediction += (long long)coefficients[7]  * (long long)pDecodedSamples[-8];
        prediction += (long long)coefficients[8]  * (long long)pDecodedSamples[-9];
        prediction += (long long)coefficients[9]  * (long long)pDecodedSamples[-10];
        prediction += (long long)coefficients[10] * (long long)pDecodedSamples[-11];
        prediction += (long long)coefficients[11] * (long long)pDecodedSamples[-12];
    }
    else if (order == 2)
    {
        prediction  = (long long)coefficients[0] * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1] * (long long)pDecodedSamples[-2];
    }
    else if (order == 1)
    {
        prediction = (long long)coefficients[0] * (long long)pDecodedSamples[-1];
    }
    else if (order == 10)
    {
        prediction  = (long long)coefficients[0]  * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1]  * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2]  * (long long)pDecodedSamples[-3];
        prediction += (long long)coefficients[3]  * (long long)pDecodedSamples[-4];
        prediction += (long long)coefficients[4]  * (long long)pDecodedSamples[-5];
        prediction += (long long)coefficients[5]  * (long long)pDecodedSamples[-6];
        prediction += (long long)coefficients[6]  * (long long)pDecodedSamples[-7];
        prediction += (long long)coefficients[7]  * (long long)pDecodedSamples[-8];
        prediction += (long long)coefficients[8]  * (long long)pDecodedSamples[-9];
        prediction += (long long)coefficients[9]  * (long long)pDecodedSamples[-10];
    }
    else if (order == 9)
    {
        prediction  = (long long)coefficients[0]  * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1]  * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2]  * (long long)pDecodedSamples[-3];
        prediction += (long long)coefficients[3]  * (long long)pDecodedSamples[-4];
        prediction += (long long)coefficients[4]  * (long long)pDecodedSamples[-5];
        prediction += (long long)coefficients[5]  * (long long)pDecodedSamples[-6];
        prediction += (long long)coefficients[6]  * (long long)pDecodedSamples[-7];
        prediction += (long long)coefficients[7]  * (long long)pDecodedSamples[-8];
        prediction += (long long)coefficients[8]  * (long long)pDecodedSamples[-9];
    }
    else if (order == 11)
    {
        prediction  = (long long)coefficients[0]  * (long long)pDecodedSamples[-1];
        prediction += (long long)coefficients[1]  * (long long)pDecodedSamples[-2];
        prediction += (long long)coefficients[2]  * (long long)pDecodedSamples[-3];
        prediction += (long long)coefficients[3]  * (long long)pDecodedSamples[-4];
        prediction += (long long)coefficients[4]  * (long long)pDecodedSamples[-5];
        prediction += (long long)coefficients[5]  * (long long)pDecodedSamples[-6];
        prediction += (long long)coefficients[6]  * (long long)pDecodedSamples[-7];
        prediction += (long long)coefficients[7]  * (long long)pDecodedSamples[-8];
        prediction += (long long)coefficients[8]  * (long long)pDecodedSamples[-9];
        prediction += (long long)coefficients[9]  * (long long)pDecodedSamples[-10];
        prediction += (long long)coefficients[10] * (long long)pDecodedSamples[-11];
    }
    else
    {
        prediction = 0;
        for (int j = 0; j < (int)order; ++j) {
            prediction += (long long)coefficients[j] * (long long)pDecodedSamples[-j-1];
        }
    }
#endif

    // Experiment #2. See if we can use a switch and let the compiler optimize it to a single jmp instruction.
    // Result: VC++ optimizes this to a single jmp on the 64-bit build, but for some reason the 32-bit version compiles to less efficient
    // code. Thus, we use this version on the 64-bit build and the uglier version above for the 32-bit build. If anyone has an idea on how
    // I can get VC++ to generate an efficient jump table for the 32-bit build let me know.
#ifdef DRFLAC_64BIT
    long long prediction = 0;

    switch (order)
    {
    case 32: prediction += (long long)coefficients[31] * (long long)pDecodedSamples[-32];
    case 31: prediction += (long long)coefficients[30] * (long long)pDecodedSamples[-31];
    case 30: prediction += (long long)coefficients[29] * (long long)pDecodedSamples[-30];
    case 29: prediction += (long long)coefficients[28] * (long long)pDecodedSamples[-29];
    case 28: prediction += (long long)coefficients[27] * (long long)pDecodedSamples[-28];
    case 27: prediction += (long long)coefficients[26] * (long long)pDecodedSamples[-27];
    case 26: prediction += (long long)coefficients[25] * (long long)pDecodedSamples[-26];
    case 25: prediction += (long long)coefficients[24] * (long long)pDecodedSamples[-25];
    case 24: prediction += (long long)coefficients[23] * (long long)pDecodedSamples[-24];
    case 23: prediction += (long long)coefficients[22] * (long long)pDecodedSamples[-23];
    case 22: prediction += (long long)coefficients[21] * (long long)pDecodedSamples[-22];
    case 21: prediction += (long long)coefficients[20] * (long long)pDecodedSamples[-21];
    case 20: prediction += (long long)coefficients[19] * (long long)pDecodedSamples[-20];
    case 19: prediction += (long long)coefficients[18] * (long long)pDecodedSamples[-19];
    case 18: prediction += (long long)coefficients[17] * (long long)pDecodedSamples[-18];
    case 17: prediction += (long long)coefficients[16] * (long long)pDecodedSamples[-17];
    case 16: prediction += (long long)coefficients[15] * (long long)pDecodedSamples[-16];
    case 15: prediction += (long long)coefficients[14] * (long long)pDecodedSamples[-15];
    case 14: prediction += (long long)coefficients[13] * (long long)pDecodedSamples[-14];
    case 13: prediction += (long long)coefficients[12] * (long long)pDecodedSamples[-13];
    case 12: prediction += (long long)coefficients[11] * (long long)pDecodedSamples[-12];
    case 11: prediction += (long long)coefficients[10] * (long long)pDecodedSamples[-11];
    case 10: prediction += (long long)coefficients[ 9] * (long long)pDecodedSamples[-10];
    case  9: prediction += (long long)coefficients[ 8] * (long long)pDecodedSamples[- 9];
    case  8: prediction += (long long)coefficients[ 7] * (long long)pDecodedSamples[- 8];
    case  7: prediction += (long long)coefficients[ 6] * (long long)pDecodedSamples[- 7];
    case  6: prediction += (long long)coefficients[ 5] * (long long)pDecodedSamples[- 6];
    case  5: prediction += (long long)coefficients[ 4] * (long long)pDecodedSamples[- 5];
    case  4: prediction += (long long)coefficients[ 3] * (long long)pDecodedSamples[- 4];
    case  3: prediction += (long long)coefficients[ 2] * (long long)pDecodedSamples[- 3];
    case  2: prediction += (long long)coefficients[ 1] * (long long)pDecodedSamples[- 2];
    case  1: prediction += (long long)coefficients[ 0] * (long long)pDecodedSamples[- 1];
    }
#endif

    return (int32_t)(prediction >> shift);
}


// Reads and decodes a string of residual values as Rice codes. The decoder should be sitting on the first bit of the Rice codes.
//
// This is the most frequently called function in the library. It does both the Rice decoding and the prediction in a single loop
// iteration.
static bool drflac__decode_samples_with_residual__rice(drflac* pFlac, unsigned int count, unsigned char riceParam, unsigned int order, int shift, const short* coefficients, int* pSamplesOut)
{
    assert(pFlac != NULL);
    assert(count > 0);
    assert(pSamplesOut != NULL);

    static unsigned int bitOffsetTable[] = {
        0,
        4,
        3, 3,
        2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1
    };

    drflac_cache_t riceParamMask = DRFLAC_CACHE_L1_SELECTION_MASK(riceParam);
    drflac_cache_t resultHiShift = DRFLAC_CACHE_L1_SIZE_BITS - riceParam;

    for (int i = 0; i < (int)count; ++i)
    {
        unsigned int zeroCounter = 0;
        while (pFlac->cache == 0) {
            zeroCounter += (unsigned int)DRFLAC_CACHE_L1_BITS_REMAINING;
            if (!drflac__reload_cache(pFlac)) {
                return false;
            }
        }

        // At this point the cache should not be zero, in which case we know the first set bit should be somewhere in here. There is
        // no need for us to perform any cache reloading logic here which should make things much faster.
        assert(pFlac->cache != 0);
        unsigned int decodedRice;

        unsigned int setBitOffsetPlus1 = bitOffsetTable[DRFLAC_CACHE_L1_SELECT_AND_SHIFT(4)];
        if (setBitOffsetPlus1 > 0) {
            decodedRice = (zeroCounter + (setBitOffsetPlus1-1)) << riceParam;
        } else {
            if (pFlac->cache == 1) {
                setBitOffsetPlus1 = DRFLAC_CACHE_L1_SIZE_BITS;
                decodedRice = (zeroCounter + (DRFLAC_CACHE_L1_SIZE_BITS-1)) << riceParam;
            } else {
                setBitOffsetPlus1 = 5;
                for (;;)
                {
                    if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(setBitOffsetPlus1))) {
                        decodedRice = (zeroCounter + (setBitOffsetPlus1-1)) << riceParam;
                        break;
                    }

                    setBitOffsetPlus1 += 1;
                }
            }
        }


        unsigned int bitsLo = 0;
        unsigned int riceLength = setBitOffsetPlus1 + riceParam;
        if (riceLength < DRFLAC_CACHE_L1_BITS_REMAINING)
        {
            bitsLo = (unsigned int)((pFlac->cache & (riceParamMask >> setBitOffsetPlus1)) >> (DRFLAC_CACHE_L1_SIZE_BITS - riceLength));

            pFlac->consumedBits += riceLength;
            pFlac->cache <<= riceLength;
        }
        else
        {
            pFlac->consumedBits += riceLength;
            pFlac->cache <<= setBitOffsetPlus1;

            // It straddles the cached data. It will never cover more than the next chunk. We just read the number in two parts and combine them.
            size_t bitCountLo = pFlac->consumedBits - DRFLAC_CACHE_L1_SIZE_BITS;
            drflac_cache_t resultHi = pFlac->cache & riceParamMask;    // <-- This mask is OK because all bits after the first bits are always zero.


            if (pFlac->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT) {
                pFlac->cache = drflac__be2host__cache_line(pFlac->cacheL2[pFlac->nextL2Line++]);
            } else {
                // Slow path. We need to fetch more data from the client.
                if (!drflac__reload_cache(pFlac)) {
                    return false;
                }
            }

            bitsLo = (unsigned int)((resultHi >> resultHiShift) | DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bitCountLo));
            pFlac->consumedBits = bitCountLo;
            pFlac->cache <<= bitCountLo;
        }


        decodedRice |= bitsLo;
        if ((decodedRice & 0x01)) {
            decodedRice = ~(decodedRice >> 1);
        } else {
            decodedRice = (decodedRice >> 1);
        }


        // In order to properly calculate the prediction when the bits per sample is >16 we need to do it using 64-bit arithmetic. We can assume this
        // is probably going to be slower on 32-bit systems so we'll do a more optimized 32-bit version when the bits per sample is low enough.
        if (pFlac->currentFrame.bitsPerSample > 16) {
            pSamplesOut[i] = ((int)decodedRice + drflac__calculate_prediction(order, shift, coefficients, pSamplesOut + i));
        } else {
            pSamplesOut[i] = ((int)decodedRice + drflac__calculate_prediction_32(order, shift, coefficients, pSamplesOut + i));
        }
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

static bool drflac__decode_samples_with_residual__unencoded(drflac* pFlac, unsigned int count, unsigned char unencodedBitsPerSample, unsigned int order, int shift, const short* coefficients, int* pSamplesOut)
{
    assert(pFlac != NULL);
    assert(count > 0);
    assert(unencodedBitsPerSample > 0 && unencodedBitsPerSample <= 32);
    assert(pSamplesOut != NULL);

    for (unsigned int i = 0; i < count; ++i)
    {
        if (!drflac__read_int32(pFlac, unencodedBitsPerSample, pSamplesOut + i)) {
            return false;
        }

        pSamplesOut[i] += drflac__calculate_prediction(order, shift, coefficients, pSamplesOut + i);
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

        pSubframe->pDecodedSamples[i] = sample;
    }


    if (!drflac__decode_samples_with_residual(pFlac, pFlac->currentFrame.blockSize, pSubframe->lpcOrder, 0, lpcCoefficientsTable[pSubframe->lpcOrder], pSubframe->pDecodedSamples)) {
        return false;
    }

    return true;
}

static bool drflac__decode_samples__lpc(drflac* pFlac, drflac_subframe* pSubframe)
{
    // Warm up samples.
    for (unsigned int i = 0; i < pSubframe->lpcOrder; ++i) {
        int sample;
        if (!drflac__read_int32(pFlac, pSubframe->bitsPerSample, &sample)) {
            return false;
        }

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


    short coefficients[32];
    for (unsigned int i = 0; i < pSubframe->lpcOrder; ++i) {
        if (!drflac__read_int16(pFlac, lpcPrecision, coefficients + i)) {
            return false;
        }
    }

    if (!drflac__decode_samples_with_residual(pFlac, pFlac->currentFrame.blockSize, pSubframe->lpcOrder, lpcShift, coefficients, pSubframe->pDecodedSamples)) {
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

    const int sampleRateTable[12]       = {0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
    const uint8_t bitsPerSampleTable[8] = {0, 8, 12, (uint8_t)-1, 16, 20, 24, (uint8_t)-1};   // -1 = reserved.

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
        uint64_t sampleNumber;
        if (!drflac__read_utf8_coded_number(pFlac, &sampleNumber)) {
            return false;
        }
        pFlac->currentFrame.frameNumber  = 0;
        pFlac->currentFrame.sampleNumber = sampleNumber;

    } else {
        uint64_t frameNumber = 0;
        if (!drflac__read_utf8_coded_number(pFlac, &frameNumber)) {
            return false;
        }
        pFlac->currentFrame.frameNumber  = (uint32_t)frameNumber;   // <-- Safe cast.
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


    if (sampleRate <= 11) {
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

    pFlac->currentFrame.bitsPerSample = bitsPerSampleTable[bitsPerSample];
    if (pFlac->currentFrame.bitsPerSample == 0) {
        pFlac->currentFrame.bitsPerSample = pFlac->bitsPerSample;
    }

    if (drflac__read_uint8(pFlac, 8, &pFlac->currentFrame.crc8) != 1) {
        return false;
    }

    memset(pFlac->currentFrame.subframes, 0, sizeof(pFlac->currentFrame.subframes));

    return true;
}

static bool drflac__read_subframe_header(drflac* pFlac, drflac_subframe* pSubframe)
{
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
        unsigned int wastedBitsPerSample;
        if (!drflac__seek_past_next_set_bit(pFlac, &wastedBitsPerSample)) {
            return false;
        }
        pSubframe->wastedBitsPerSample = (unsigned char)wastedBitsPerSample + 1;
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
    pSubframe->pDecodedSamples = pFlac->pDecodedSamples + (pFlac->currentFrame.blockSize * subframeIndex);

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
            drflac__decode_samples__lpc(pFlac, pSubframe);
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
    pSubframe->pDecodedSamples = pFlac->pDecodedSamples + (pFlac->currentFrame.blockSize * subframeIndex);

    switch (pSubframe->subframeType)
    {
        case DRFLAC_SUBFRAME_CONSTANT:
        {
            if (!drflac__seek_bits(pFlac, pSubframe->bitsPerSample)) {
                return false;
            }
        } break;

        case DRFLAC_SUBFRAME_VERBATIM:
        {
            unsigned int bitsToSeek = pFlac->currentFrame.blockSize * pSubframe->bitsPerSample;
            if (!drflac__seek_bits(pFlac, bitsToSeek)) {
                return false;
            }
        } break;

        case DRFLAC_SUBFRAME_FIXED:
        {
            unsigned int bitsToSeek = pSubframe->lpcOrder * pSubframe->bitsPerSample;
            if (!drflac__seek_bits(pFlac, bitsToSeek)) {
                return false;
            }

            if (!drflac__read_and_seek_residual(pFlac, pFlac->currentFrame.blockSize, pSubframe->lpcOrder)) {
                return false;
            }
        } break;

        case DRFLAC_SUBFRAME_LPC:
        {
            unsigned int bitsToSeek = pSubframe->lpcOrder * pSubframe->bitsPerSample;
            if (!drflac__seek_bits(pFlac, bitsToSeek)) {
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
            if (!drflac__seek_bits(pFlac, bitsToSeek)) {
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


static DRFLAC_INLINE int drflac__get_channel_count_from_channel_assignment(int channelAssignment)
{
    assert(channelAssignment <= 10);

    int lookup[] = {1, 2, 3, 4, 5, 6, 7, 8, 2, 2, 2};
    return lookup[channelAssignment];
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
    if (!drflac__seek_bits(pFlac, (DRFLAC_CACHE_L1_BITS_REMAINING & 7) + 16)) {
        return false;
    }


    pFlac->currentFrame.samplesRemaining = pFlac->currentFrame.blockSize * channelCount;

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
    return drflac__seek_bits(pFlac, (DRFLAC_CACHE_L1_BITS_REMAINING & 7) + 16);
}

static bool drflac__read_and_decode_next_frame(drflac* pFlac)
{
    assert(pFlac != NULL);

    if (!drflac__read_next_frame_header(pFlac)) {
        return false;
    }

    return drflac__decode_frame(pFlac);
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
    pFlac->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS;
    pFlac->cache = 0;

    memset(&pFlac->currentFrame, 0, sizeof(pFlac->currentFrame));


    return result;
}

static DRFLAC_INLINE bool drflac__seek_to_next_frame(drflac* pFlac)
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

    return drflac_read_s32(pFlac, samplesToDecode, NULL) != 0;
}

static bool drflac__seek_to_sample__seek_table(drflac* pFlac, uint64_t sampleIndex)
{
    assert(pFlac != NULL);

    if (pFlac->seektablePos == 0) {
        return false;
    }

    if (!drflac__seek_to_byte(pFlac, pFlac->seektablePos)) {
        return false;
    }

    // The number of seek points is derived from the size of the SEEKTABLE block.
    unsigned int seekpointCount = pFlac->seektableSize / 18;   // 18 = the size of each seek point.
    if (seekpointCount == 0) {
        return false;   // Would this ever happen?
    }


    drflac_seekpoint closestSeekpoint = {0};

    unsigned int seekpointsRemaining = seekpointCount;
    while (seekpointsRemaining > 0)
    {
        drflac_seekpoint seekpoint;
        if (!drflac__read_uint64(pFlac, 64, &seekpoint.firstSample)) {
            break;
        }
        if (!drflac__read_uint64(pFlac, 64, &seekpoint.frameOffset)) {
            break;
        }
        if (!drflac__read_uint16(pFlac, 16, &seekpoint.sampleCount)) {
            break;
        }

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

    assert(firstSampleInFrame <= sampleIndex);

    // At this point we are just sitting on the byte after the frame header. We need to decode the frame before reading anything from it.
    if (!drflac__decode_frame(pFlac)) {
        return false;
    }

    size_t samplesToDecode = (size_t)(sampleIndex - firstSampleInFrame);    // <-- Safe cast because the maximum number of samples in a frame is 65535.
    return drflac_read_s32(pFlac, samplesToDecode, NULL) == samplesToDecode;
}



typedef struct
{
    drflac_read_proc onRead;
    drflac_seek_proc onSeek;
    drflac_meta_proc onMeta;
    void* pUserData;
    void* pUserDataMD;
    uint32_t sampleRate;
    uint8_t  channels;
    uint8_t  bitsPerSample;
    uint64_t totalSampleCount;
    uint16_t maxBlockSize;
    uint64_t runningFilePos;
    uint64_t seektablePos;
    uint32_t seektableSize;
} drflac_init_info;

void drflac__decode_block_header(uint32_t blockHeader, uint8_t* isLastBlock, uint8_t* blockType, uint32_t* blockSize)
{
    blockHeader = drflac__be2host_32(blockHeader);
    *isLastBlock = (blockHeader & (0x01 << 31)) >> 31;
    *blockType   = (blockHeader & (0x7F << 24)) >> 24;
    *blockSize   = (blockHeader & 0xFFFFFF);
}

bool drflac__init_private(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD)
{
    if (pInit == NULL || onRead == NULL || onSeek == NULL) {
        return false;
    }

    pInit->onRead      = onRead;
    pInit->onSeek      = onSeek;
    pInit->onMeta      = onMeta;
    pInit->pUserData   = pUserData;
    pInit->pUserDataMD = pUserDataMD;

    unsigned char id[4];
    if (onRead(pUserData, id, 4) != 4 || id[0] != 'f' || id[1] != 'L' || id[2] != 'a' || id[3] != 'C') {
        return false;    // Not a FLAC stream.
    }

    // The first metadata block should be the STREAMINFO block. We don't care about everything in here.
    uint32_t blockHeader;
    if (onRead(pUserData, &blockHeader, 4) != 4) {
        return false;    // Ran out of data.
    }

    uint8_t  isLastBlock = (blockHeader & (0x01 << 31)) >> 31;
    uint8_t  blockType   = (blockHeader & (0x7F << 24)) >> 24;
    uint32_t blockSize   = (blockHeader & 0xFFFFFF);
    drflac__decode_block_header(blockHeader, &isLastBlock, &blockType, &blockSize);
    if (blockType != DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO || blockSize != 34) {
        return false;    // Invalid block type. First block must be the STREAMINFO block.
    }

    // min/max block size.
    uint32_t blockSizes;
    if (onRead(pUserData, &blockSizes, 4) != 4) {
        return false;
    }

    // min/max frame size.
    uint64_t frameSizes = 0;
    if (onRead(pUserData, &frameSizes, 6) != 6) {
        return false;
    }

    // Sample rate, channels, bits per sample and total sample count.
    uint64_t importantProps;
    if (onRead(pUserData, &importantProps, 8) != 8) {
        return false;
    }

    // MD5
    uint8_t md5[16];
    if (onRead(pUserData, md5, sizeof(md5)) != sizeof(md5)) {
        return false;
    }

    blockSizes     = drflac__be2host_32(blockSizes);
    frameSizes     = drflac__be2host_64(frameSizes);
    importantProps = drflac__be2host_64(importantProps);

    pInit->sampleRate       = (uint32_t)((importantProps & 0xFFFFF00000000000ULL) >> 44ULL);
    pInit->channels         = (uint8_t )((importantProps & 0x00000E0000000000ULL) >> 41ULL) + 1;
    pInit->bitsPerSample    = (uint8_t )((importantProps & 0x000001F000000000ULL) >> 36ULL) + 1;
    pInit->totalSampleCount = (importantProps & 0x0000000FFFFFFFFFULL) * pInit->channels;
    pInit->maxBlockSize     = blockSizes & 0x0000FFFF;    // Don't care about the min block size - only the max (used for determining the size of the memory allocation).

    if (onMeta) {
        drflac_metadata metadata;
        metadata.type = DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO;
        metadata.pRawData = NULL;
        metadata.rawDataSize = 0;

        metadata.data.streaminfo.minBlockSize     = (blockSizes & 0xFFFF0000) >> 16;
        metadata.data.streaminfo.maxBlockSize     = pInit->maxBlockSize;
        metadata.data.streaminfo.minFrameSize     = (uint32_t)((frameSizes & 0xFFFFFF0000000000ULL) >> 40ULL);
        metadata.data.streaminfo.maxFrameSize     = (uint32_t)((frameSizes & 0x000000FFFFFF0000ULL) >> 16ULL);
        metadata.data.streaminfo.sampleRate       = pInit->sampleRate;
        metadata.data.streaminfo.channels         = pInit->channels;
        metadata.data.streaminfo.bitsPerSample    = pInit->bitsPerSample;
        metadata.data.streaminfo.totalSampleCount = pInit->totalSampleCount;
        memcpy(metadata.data.streaminfo.md5, md5, sizeof(md5));
        onMeta(pUserDataMD, &metadata);
    }


    // The next blocks are metadata which are all optional. We want to keep track of the seektable so we can do efficient seeking.
    pInit->runningFilePos = 42;
    pInit->seektablePos = 0;
    pInit->seektableSize = 0;
    while (!isLastBlock)
    {
        if (onRead(pUserData, &blockHeader, 4) != 4) {
            return false;
        }
        pInit->runningFilePos += 4;

        drflac__decode_block_header(blockHeader, &isLastBlock, &blockType, &blockSize);

        drflac_metadata metadata;
        metadata.type = blockType;
        metadata.pRawData = NULL;
        metadata.rawDataSize = 0;

        switch (blockType)
        {
            case DRFLAC_METADATA_BLOCK_TYPE_APPLICATION:
            {
                if (onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return false;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return false;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;
                    metadata.data.application.id       = drflac__be2host_32(*(uint32_t*)pRawData);
                    metadata.data.application.pData    = (const void*)((char*)pRawData + sizeof(uint32_t));
                    metadata.data.application.dataSize = blockSize - sizeof(uint32_t);
                    onMeta(pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_SEEKTABLE:
            {
                pInit->seektablePos  = pInit->runningFilePos;
                pInit->seektableSize = blockSize;

                if (onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return false;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return false;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;
                    metadata.data.seektable.seekpointCount = blockSize/sizeof(drflac_seekpoint);
                    metadata.data.seektable.pSeekpoints = (const drflac_seekpoint*)pRawData;

                    // Endian swap.
                    for (uint32_t iSeekpoint = 0; iSeekpoint < metadata.data.seektable.seekpointCount; ++iSeekpoint) {
                        drflac_seekpoint* pSeekpoint = (drflac_seekpoint*)pRawData + iSeekpoint;
                        pSeekpoint->firstSample = drflac__be2host_64(pSeekpoint->firstSample);
                        pSeekpoint->frameOffset = drflac__be2host_64(pSeekpoint->frameOffset);
                        pSeekpoint->sampleCount = drflac__be2host_16(pSeekpoint->sampleCount);
                    }

                    onMeta(pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT:
            {
                if (onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return false;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return false;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;

                    const char* pRunningData = (const char*)pRawData;
                    metadata.data.vorbis_comment.vendorLength = drflac__le2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.vorbis_comment.vendor       = pRunningData;                                 pRunningData += metadata.data.vorbis_comment.vendorLength;
                    metadata.data.vorbis_comment.commentCount = drflac__le2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.vorbis_comment.comments     = pRunningData;
                    onMeta(pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_CUESHEET:
            {
                if (onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return false;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return false;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;

                    const char* pRunningData = (const char*)pRawData;
                    memcpy(metadata.data.cuesheet.catalog, pRunningData, 128);                               pRunningData += 128;
                    metadata.data.cuesheet.leadInSampleCount = drflac__be2host_64(*(uint64_t*)pRunningData); pRunningData += 4;
                    metadata.data.cuesheet.isCD              = ((pRunningData[0] & 0x80) >> 7) != 0;         pRunningData += 259;
                    metadata.data.cuesheet.trackCount        = pRunningData[0];                              pRunningData += 1;
                    metadata.data.cuesheet.pTrackData        = (const uint8_t*)pRunningData;
                    onMeta(pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_PICTURE:
            {
                if (onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return false;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return false;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;

                    const char* pRunningData = (const char*)pRawData;
                    metadata.data.picture.type              = drflac__be2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.picture.mimeLength        = drflac__be2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.picture.mime              = pRunningData;                                 pRunningData += metadata.data.picture.mimeLength;
                    metadata.data.picture.descriptionLength = drflac__be2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.picture.description       = pRunningData;
                    metadata.data.picture.width             = drflac__be2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.picture.height            = drflac__be2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.picture.colorDepth        = drflac__be2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.picture.indexColorCount   = drflac__be2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.picture.pictureDataSize   = drflac__be2host_32(*(uint32_t*)pRunningData); pRunningData += 4;
                    metadata.data.picture.pPictureData      = (const uint8_t*)pRunningData;
                    onMeta(pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_PADDING:
            {
                if (onMeta) {
                    metadata.data.padding.unused = 0;

                    // Padding doesn't have anything meaningful in it, so just skip over it.
                    if (!onSeek(pUserData, blockSize)) {
                        return false;
                    }

                    onMeta(pUserDataMD, &metadata);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_INVALID:
            {
                // Invalid chunk. Just skip over this one.
                if (onMeta) {
                    if (!onSeek(pUserData, blockSize)) {
                        return false;
                    }
                }
            }

            default:
            {
                // It's an unknown chunk, but not necessarily invalid. There's a chance more metadata blocks might be defined later on, so we
                // can at the very least report the chunk to the application and let it look at the raw data.
                if (onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return false;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return false;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;
                    onMeta(pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;
        }

        // If we're not handling metadata, just skip over the block. If we are, it will have been handled earlier in the switch statement above.
        if (onMeta == NULL) {
            if (!onSeek(pUserData, blockSize)) {
                return false;
            }
        }

        pInit->runningFilePos += blockSize;
    }

    return true;
}

void drflac__init_from_info(drflac* pFlac, drflac_init_info* pInit)
{
    assert(pFlac != NULL);
    assert(pInit != NULL);

    memset(pFlac, 0, sizeof(*pFlac));
    pFlac->onRead           = pInit->onRead;
    pFlac->onSeek           = pInit->onSeek;
    pFlac->onMeta           = pInit->onMeta;
    pFlac->pUserData        = pInit->pUserData;
    pFlac->pUserDataMD      = pInit->pUserDataMD;
    pFlac->currentBytePos   = pInit->runningFilePos;
    pFlac->nextL2Line       = sizeof(pFlac->cacheL2) / sizeof(pFlac->cacheL2[0]); // <-- Initialize to this to force a client-side data retrieval right from the start.
    pFlac->consumedBits     = sizeof(pFlac->cache)*8;
    pFlac->maxBlockSize     = pInit->maxBlockSize;
    pFlac->sampleRate       = pInit->sampleRate;
    pFlac->channels         = (uint8_t)pInit->channels;
    pFlac->bitsPerSample    = (uint8_t)pInit->bitsPerSample;
    pFlac->totalSampleCount = pInit->totalSampleCount;
    pFlac->seektablePos     = pInit->seektablePos;
    pFlac->seektableSize    = pInit->seektableSize;
    pFlac->firstFramePos    = pInit->runningFilePos;
}

drflac* drflac_open_with_metadata_private(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD)
{
    drflac_init_info init;
    if (!drflac__init_private(&init, onRead, onSeek, onMeta, pUserData, pUserDataMD)) {
        return false;
    }

    size_t allocationSize = sizeof(drflac) - sizeof(char);
    allocationSize += init.maxBlockSize * init.channels * sizeof(int32_t);
    //allocationSize += init.seektableSize;

    drflac* pFlac = (drflac*)malloc(allocationSize);
    drflac__init_from_info(pFlac, &init);
    pFlac->pDecodedSamples = (int32_t*)pFlac->pExtraData;

    return pFlac;
}



#ifndef DR_FLAC_NO_STDIO
typedef void* drflac_file;

#if defined(DR_FLAC_NO_WIN32_IO) || !defined(_WIN32)
#include <stdio.h>

static size_t drflac__on_read_stdio(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)pUserData);
}

static bool drflac__on_seek_stdio(void* pUserData, int offset)
{
    return fseek((FILE*)pUserData, offset, SEEK_CUR) == 0;
}

static drflac_file drflac__open_file_handle(const char* filename)
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

    return (drflac_file)pFile;
}

static void drflac__close_file_handle(drflac_file file)
{
    fclose((FILE*)file);
}
#else
#include <windows.h>

static size_t drflac__on_read_stdio(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    assert(bytesToRead < 0xFFFFFFFF);   // dr_flac will never request huge amounts of data at a time. This is a safe assertion.

    DWORD bytesRead;
    ReadFile((HANDLE)pUserData, bufferOut, (DWORD)bytesToRead, &bytesRead, NULL);

    return (size_t)bytesRead;
}

static bool drflac__on_seek_stdio(void* pUserData, int offset)
{
    return SetFilePointer((HANDLE)pUserData, offset, NULL, FILE_CURRENT) != INVALID_SET_FILE_POINTER;
}

static drflac_file drflac__open_file_handle(const char* filename)
{
    HANDLE hFile = CreateFileA(filename, FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    return (drflac_file)hFile;
}

static void drflac__close_file_handle(drflac_file file)
{
    CloseHandle((HANDLE)file);
}
#endif


drflac* drflac_open_file(const char* filename)
{
    drflac_file file = drflac__open_file_handle(filename);
    if (file == NULL) {
        return NULL;
    }

    return drflac_open(drflac__on_read_stdio, drflac__on_seek_stdio, (void*)file);
}

drflac* drflac_open_file_with_metadata(const char* filename, drflac_meta_proc onMeta, void* pUserData)
{
    drflac_file file = drflac__open_file_handle(filename);
    if (file == NULL) {
        return NULL;
    }

    return drflac_open_with_metadata_private(drflac__on_read_stdio, drflac__on_seek_stdio, onMeta, (void*)file, pUserData);
}
#endif  //DR_FLAC_NO_STDIO

static size_t drflac__on_read_memory(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    drflac__memory_stream* memoryStream = (drflac__memory_stream*)pUserData;
    assert(memoryStream != NULL);
    assert(memoryStream->dataSize >= memoryStream->currentReadPos);

    size_t bytesRemaining = memoryStream->dataSize - memoryStream->currentReadPos;
    if (bytesToRead > bytesRemaining) {
        bytesToRead = bytesRemaining;
    }

    if (bytesToRead > 0) {
        memcpy(bufferOut, memoryStream->data + memoryStream->currentReadPos, bytesToRead);
        memoryStream->currentReadPos += bytesToRead;
    }

    return bytesToRead;
}

static bool drflac__on_seek_memory(void* pUserData, int offset)
{
    drflac__memory_stream* memoryStream = (drflac__memory_stream*)pUserData;
    assert(memoryStream != NULL);

    if (offset > 0) {
        if (memoryStream->currentReadPos + offset > memoryStream->dataSize) {
            offset = (int)(memoryStream->dataSize - memoryStream->currentReadPos);     // Trying to seek too far forward.
        }
    } else {
        if (memoryStream->currentReadPos < (size_t)-offset) {
            offset = -(int)memoryStream->currentReadPos;                  // Trying to seek too far backwards.
        }
    }

    // This will never underflow thanks to the clamps above.
    memoryStream->currentReadPos += offset;
    return true;
}

drflac* drflac_open_memory(const void* data, size_t dataSize)
{
    drflac__memory_stream memoryStream;
    memoryStream.data = (const unsigned char*)data;
    memoryStream.dataSize = dataSize;
    memoryStream.currentReadPos = 0;
    drflac* pFlac = drflac_open(drflac__on_read_memory, drflac__on_seek_memory, &memoryStream);
    if (pFlac == NULL) {
        return NULL;
    }

    pFlac->memoryStream = memoryStream;
    pFlac->pUserData = &pFlac->memoryStream;
    return pFlac;
}

drflac* drflac_open_memory_with_metadata(const void* data, size_t dataSize, drflac_meta_proc onMeta, void* pUserData)
{
    drflac__memory_stream memoryStream;
    memoryStream.data = (const unsigned char*)data;
    memoryStream.dataSize = dataSize;
    memoryStream.currentReadPos = 0;
    drflac* pFlac = drflac_open_with_metadata_private(drflac__on_read_memory, drflac__on_seek_memory, onMeta, &memoryStream, pUserData);
    if (pFlac == NULL) {
        return NULL;
    }

    pFlac->memoryStream = memoryStream;
    pFlac->pUserData = &pFlac->memoryStream;
    return pFlac;
}



drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData)
{
    return drflac_open_with_metadata_private(onRead, onSeek, NULL, pUserData, pUserData);
}

drflac* drflac_open_with_metadata(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData)
{
    return drflac_open_with_metadata_private(onRead, onSeek, onMeta, pUserData, pUserData);
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
        drflac__close_file_handle((drflac_file)pFlac->pUserData);
    }
#endif

    free(pFlac);
}

uint64_t drflac__read_s32__misaligned(drflac* pFlac, uint64_t samplesToRead, int32_t* bufferOut)
{
    unsigned int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.channelAssignment);

    // We should never be calling this when the number of samples to read is >= the sample count.
    assert(samplesToRead < channelCount);
    assert(pFlac->currentFrame.samplesRemaining > 0 && samplesToRead <= pFlac->currentFrame.samplesRemaining);


    uint64_t samplesRead = 0;
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


        decodedSample <<= (32 - pFlac->bitsPerSample) << pFlac->currentFrame.subframes[channelIndex].wastedBitsPerSample;

        if (bufferOut) {
            *bufferOut++ = decodedSample;
        }

        samplesRead += 1;
        pFlac->currentFrame.samplesRemaining -= 1;
        samplesToRead -= 1;
    }

    return samplesRead;
}

uint64_t drflac__seek_forward_by_samples(drflac* pFlac, uint64_t samplesToRead)
{
    uint64_t samplesRead = 0;
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

uint64_t drflac_read_s32(drflac* pFlac, uint64_t samplesToRead, int32_t* bufferOut)
{
    // Note that <bufferOut> is allowed to be null, in which case this will be treated as something like a seek.
    if (pFlac == NULL || samplesToRead == 0) {
        return 0;
    }

    if (bufferOut == NULL) {
        return drflac__seek_forward_by_samples(pFlac, samplesToRead);
    }


    uint64_t samplesRead = 0;
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
                uint64_t misalignedSamplesRead = drflac__read_s32__misaligned(pFlac, misalignedSampleCount, bufferOut);
                samplesRead   += misalignedSamplesRead;
                samplesReadFromFrameSoFar += misalignedSamplesRead;
                bufferOut     += misalignedSamplesRead;
                samplesToRead -= misalignedSamplesRead;
            }


            uint64_t alignedSampleCountPerChannel = samplesToRead / channelCount;
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

                    for (uint64_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int left  = pDecodedSamples0[i];
                        int side  = pDecodedSamples1[i];
                        int right = left - side;

                        bufferOut[i*2+0] = left  << (unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample);
                        bufferOut[i*2+1] = right << (unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample);
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                {
                    const int* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const int* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    for (uint64_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int side  = pDecodedSamples0[i];
                        int right = pDecodedSamples1[i];
                        int left  = right + side;

                        bufferOut[i*2+0] = left  << (unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample);
                        bufferOut[i*2+1] = right << (unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample);
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE:
                {
                    const int* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const int* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    for (uint64_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int side = pDecodedSamples1[i];
                        int mid  = (((uint32_t)pDecodedSamples0[i]) << 1) | (side & 0x01);

                        bufferOut[i*2+0] = ((mid + side) >> 1) << (unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample);
                        bufferOut[i*2+1] = ((mid - side) >> 1) << (unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample);
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

                        for (uint64_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                            bufferOut[i*2+0] = pDecodedSamples0[i] << (unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample);
                            bufferOut[i*2+1] = pDecodedSamples1[i] << (unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample);
                        }
                    }
                    else
                    {
                        // Generic interleaving.
                        for (uint64_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                            for (unsigned int j = 0; j < channelCount; ++j) {
                                bufferOut[(i*channelCount)+j] = (pFlac->currentFrame.subframes[j].pDecodedSamples[firstAlignedSampleInFrame + i]) << (unusedBitsPerSample + pFlac->currentFrame.subframes[j].wastedBitsPerSample);
                            }
                        }
                    }
                } break;
            }

            uint64_t alignedSamplesRead = alignedSampleCountPerChannel * channelCount;
            samplesRead   += alignedSamplesRead;
            samplesReadFromFrameSoFar += alignedSamplesRead;
            bufferOut     += alignedSamplesRead;
            samplesToRead -= alignedSamplesRead;
            pFlac->currentFrame.samplesRemaining -= (unsigned int)alignedSamplesRead;



            // At this point we may still have some excess samples left to read.
            if (samplesToRead > 0 && pFlac->currentFrame.samplesRemaining > 0)
            {
                uint64_t excessSamplesRead = 0;
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



//// High Level APIs ////

int32_t* drflac__full_decode_and_close(drflac* pFlac, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    assert(pFlac != NULL);

    uint64_t dataSize = pFlac->totalSampleCount * sizeof(int32_t);
    if (dataSize > SIZE_MAX) {
        drflac_close(pFlac);
        return NULL;    // The decoded data is too big.
    }

    int32_t* pSampleData = (int32_t*)malloc((size_t)dataSize);    // <-- Safe cast as per the check above.
    if (pSampleData == NULL) {
        drflac_close(pFlac);
        return NULL;
    }

    uint64_t samplesDecoded = drflac_read_s32(pFlac, pFlac->totalSampleCount, pSampleData);
    if (samplesDecoded != pFlac->totalSampleCount) {
        drflac_close(pFlac);
        free(pSampleData);
        return NULL;    // Something went wrong when decoding the FLAC stream.
    }


    if (sampleRate) *sampleRate = pFlac->sampleRate;
    if (channels) *channels = pFlac->channels;
    if (totalSampleCount) *totalSampleCount = pFlac->totalSampleCount;

    drflac_close(pFlac);
    return pSampleData;
}

int32_t* drflac_open_and_decode(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    // Safety.
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open(onRead, onSeek, pUserData);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close(pFlac, sampleRate, channels, totalSampleCount);
}

#ifndef DR_FLAC_NO_STDIO
int32_t* drflac_open_and_decode_file(const char* filename, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open_file(filename);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close(pFlac, sampleRate, channels, totalSampleCount);
}
#endif

int32_t* drflac_open_and_decode_memory(const void* data, size_t dataSize, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open_memory(data, dataSize);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close(pFlac, sampleRate, channels, totalSampleCount);
}

void drflac_free(void* pSampleDataReturnedByOpenAndDecode)
{
    free(pSampleDataReturnedByOpenAndDecode);
}




void drflac_init_vorbis_comment_iterator(drflac_vorbis_comment_iterator* pIter, uint32_t commentCount, const char* pComments)
{
    if (pIter == NULL) {
        return;
    }

    pIter->countRemaining = commentCount;
    pIter->pRunningData   = pComments;
}

const char* drflac_next_vorbis_comment(drflac_vorbis_comment_iterator* pIter, uint32_t* pCommentLengthOut)
{
    // Safety.
    if (pCommentLengthOut) *pCommentLengthOut = 0;

    if (pIter == NULL || pIter->countRemaining == 0 || pIter->pRunningData == NULL) {
        return NULL;
    }

    uint32_t length = drflac__le2host_32(*(uint32_t*)pIter->pRunningData);
    pIter->pRunningData += 4;

    const char* pComment = pIter->pRunningData;
    pIter->pRunningData += length;
    pIter->countRemaining -= 1;

    if (pCommentLengthOut) *pCommentLengthOut = length;
    return pComment;
}
#endif  //DR_FLAC_IMPLEMENTATION


// REVISION HISTORY
//
// v0.1a - 05/05/2016
//   - Minor formatting changes.
//   - Fixed a warning on the GCC build.
//
// v0.1 - 03/05/2016
//   - Initial versioned release.


// TODO
// - Add support for initializing the decoder without a header STREAMINFO block.
// - Test CUESHEET metadata blocks.
// - Add support for Ogg encapsulation.


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
