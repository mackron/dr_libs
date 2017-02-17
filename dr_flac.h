// FLAC audio decoder. Public domain. See "unlicense" statement at the end of this file.
// dr_flac - v0.4d - 2016-12-26
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
//     dr_int32* pSamples = malloc(pFlac->totalSampleCount * sizeof(dr_int32));
//     dr_uint64 numberOfInterleavedSamplesActuallyRead = drflac_read_s32(pFlac, pFlac->totalSampleCount, pSamples);
//
// The drflac object represents the decoder. It is a transparent type so all the information you need, such as the number of
// channels and the bits per sample, should be directly accessible - just make sure you don't change their values. Samples are
// always output as interleaved signed 32-bit PCM. In the example above a native FLAC stream was opened, however dr_flac has
// seamless support for Ogg encapsulated FLAC streams as well.
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
//     unsigned int channels;
//     unsigned int sampleRate;
//     dr_uint64 totalSampleCount;
//     dr_int32* pSampleData = drflac_open_and_decode_file_s32("MySong.flac", &channels, &sampleRate, &totalSampleCount);
//     if (pSampleData == NULL) {
//         // Failed to open and decode FLAC file.
//     }
//
//     ...
//
//     drflac_free(pSampleData);
//
//
// You can read samples as signed 16-bit integer and 32-bit floating-point PCM with the *_s16() and *_f32() family of APIs
// respectively, but note that this is lossy.
//
//
// If you need access to metadata (album art, etc.), use drflac_open_with_metadata(), drflac_open_file_with_metdata() or
// drflac_open_memory_with_metadata(). The rationale for keeping these APIs separate is that they're slightly slower than the
// normal versions and also just a little bit harder to use.
//
// dr_flac reports metadata to the application through the use of a callback, and every metadata block is reported before
// drflac_open_with_metdata() returns. See https://github.com/mackron/dr_libs_tests/blob/master/dr_flac/dr_flac_test_2.c for
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
// #define DR_FLAC_NO_OGG
//   Disables support for Ogg/FLAC streams.
//
// #define DR_FLAC_NO_WIN32_IO
//   In the Win32 build, dr_flac uses the Win32 IO APIs for drflac_open_file() by default. This setting will make it use the
//   standard FILE APIs instead. Ignored when DR_FLAC_NO_STDIO is #defined. (The rationale for this configuration is that
//   there's a bug in one compiler's Win32 implementation of the FILE APIs which is not present in the Win32 IO APIs.)
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
// - Based on my tests, the performance of the 32-bit build is at about parity with the reference implementation. The 64-bit build
//   is slightly faster.
// - dr_flac does not currently do any CRC checks.
// - dr_flac should work fine with valid native FLAC files, but for broadcast streams it won't work if the header and STREAMINFO
//   block is unavailable.
// - Audio data is output as signed 32-bit PCM, regardless of the bits per sample the FLAC stream is encoded as.
// - This has not been tested on big-endian architectures.
// - Rice codes in unencoded binary form (see https://xiph.org/flac/format.html#rice_partition) has not been tested. If anybody
//   knows where I can find some test files for this, let me know.
// - Perverse and erroneous files have not been tested. Again, if you know where I can get some test files let me know.
// - dr_flac is not thread-safe, but it's APIs can be called from any thread so long as you do your own synchronization.

#ifndef dr_flac_h
#define dr_flac_h

#include <stddef.h>

#ifndef DR_SIZED_TYPES_DEFINED
#define DR_SIZED_TYPES_DEFINED
#if defined(_MSC_VER) && _MSC_VER < 1600
typedef   signed char    dr_int8;
typedef unsigned char    dr_uint8;
typedef   signed short   dr_int16;
typedef unsigned short   dr_uint16;
typedef   signed int     dr_int32;
typedef unsigned int     dr_uint32;
typedef   signed __int64 dr_int64;
typedef unsigned __int64 dr_uint64;
#else
#include <stdint.h>
typedef int8_t           dr_int8;
typedef uint8_t          dr_uint8;
typedef int16_t          dr_int16;
typedef uint16_t         dr_uint16;
typedef int32_t          dr_int32;
typedef uint32_t         dr_uint32;
typedef int64_t          dr_int64;
typedef uint64_t         dr_uint64;
#endif
typedef dr_int8          dr_bool8;
typedef dr_int32         dr_bool32;
#define DR_TRUE          1
#define DR_FALSE         0
#endif

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
typedef dr_uint64 drflac_cache_t;
#else
typedef dr_uint32 drflac_cache_t;
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

typedef enum
{
    drflac_container_native,
    drflac_container_ogg
} drflac_container;

typedef enum
{
    drflac_seek_origin_start,
    drflac_seek_origin_current
} drflac_seek_origin;

// Packing is important on this structure because we map this directly to the raw data within the SEEKTABLE metadata block.
#pragma pack(2)
typedef struct
{
    dr_uint64 firstSample;
    dr_uint64 frameOffset;   // The offset from the first byte of the header of the first frame.
    dr_uint16 sampleCount;
} drflac_seekpoint;
#pragma pack()

typedef struct
{
    dr_uint16 minBlockSize;
    dr_uint16 maxBlockSize;
    dr_uint32 minFrameSize;
    dr_uint32 maxFrameSize;
    dr_uint32 sampleRate;
    dr_uint8  channels;
    dr_uint8  bitsPerSample;
    dr_uint64 totalSampleCount;
    dr_uint8  md5[16];
} drflac_streaminfo;

typedef struct
{
    // The metadata type. Use this to know how to interpret the data below.
    dr_uint32 type;

    // A pointer to the raw data. This points to a temporary buffer so don't hold on to it. It's best to
    // not modify the contents of this buffer. Use the structures below for more meaningful and structured
    // information about the metadata. It's possible for this to be null.
    const void* pRawData;

    // The size in bytes of the block and the buffer pointed to by pRawData if it's non-NULL.
    dr_uint32 rawDataSize;

    union
    {
        drflac_streaminfo streaminfo;

        struct
        {
            int unused;
        } padding;

        struct
        {
            dr_uint32 id;
            const void* pData;
            dr_uint32 dataSize;
        } application;

        struct
        {
            dr_uint32 seekpointCount;
            const drflac_seekpoint* pSeekpoints;
        } seektable;

        struct
        {
            dr_uint32 vendorLength;
            const char* vendor;
            dr_uint32 commentCount;
            const char* comments;
        } vorbis_comment;

        struct
        {
            char catalog[128];
            dr_uint64 leadInSampleCount;
            dr_bool32 isCD;
            dr_uint8 trackCount;
            const dr_uint8* pTrackData;
        } cuesheet;

        struct
        {
            dr_uint32 type;
            dr_uint32 mimeLength;
            const char* mime;
            dr_uint32 descriptionLength;
            const char* description;
            dr_uint32 width;
            dr_uint32 height;
            dr_uint32 colorDepth;
            dr_uint32 indexColorCount;
            dr_uint32 pictureDataSize;
            const dr_uint8* pPictureData;
        } picture;
    } data;

} drflac_metadata;


// Callback for when data needs to be read from the client.
//
// pUserData   [in]  The user data that was passed to drflac_open() and family.
// pBufferOut  [out] The output buffer.
// bytesToRead [in]  The number of bytes to read.
//
// Returns the number of bytes actually read.
typedef size_t (* drflac_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

// Callback for when data needs to be seeked.
//
// pUserData [in] The user data that was passed to drflac_open() and family.
// offset    [in] The number of bytes to move, relative to the origin. Will never be negative.
// origin    [in] The origin of the seek - the current position or the start of the stream.
//
// Returns whether or not the seek was successful.
//
// The offset will never be negative. Whether or not it is relative to the beginning or current position is determined
// by the "origin" parameter which will be either drflac_seek_origin_start or drflac_seek_origin_current.
typedef dr_bool32 (* drflac_seek_proc)(void* pUserData, int offset, drflac_seek_origin origin);

// Callback for when a metadata block is read.
//
// pUserData [in] The user data that was passed to drflac_open() and family.
// pMetadata [in] A pointer to a structure containing the data of the metadata block.
//
// Use pMetadata->type to determine which metadata block is being handled and how to read the data.
typedef void (* drflac_meta_proc)(void* pUserData, drflac_metadata* pMetadata);


// Structure for internal use. Only used for decoders opened with drflac_open_memory.
typedef struct
{
    const dr_uint8* data;
    size_t dataSize;
    size_t currentReadPos;
} drflac__memory_stream;

// Structure for internal use. Used for bit streaming.
typedef struct
{
    // The function to call when more data needs to be read.
    drflac_read_proc onRead;

    // The function to call when the current read position needs to be moved.
    drflac_seek_proc onSeek;

    // The user data to pass around to onRead and onSeek.
    void* pUserData;


    // The number of unaligned bytes in the L2 cache. This will always be 0 until the end of the stream is hit. At the end of the
    // stream there will be a number of bytes that don't cleanly fit in an L1 cache line, so we use this variable to know whether
    // or not the bistreamer needs to run on a slower path to read those last bytes. This will never be more than sizeof(drflac_cache_t).
    size_t unalignedByteCount;

    // The content of the unaligned bytes.
    drflac_cache_t unalignedCache;

    // The index of the next valid cache line in the "L2" cache.
    size_t nextL2Line;

    // The number of bits that have been consumed by the cache. This is used to determine how many valid bits are remaining.
    size_t consumedBits;

    // The cached data which was most recently read from the client. There are two levels of cache. Data flows as such:
    // Client -> L2 -> L1. The L2 -> L1 movement is aligned and runs on a fast path in just a few instructions.
    drflac_cache_t cacheL2[DR_FLAC_BUFFER_SIZE/sizeof(drflac_cache_t)];
    drflac_cache_t cache;

} drflac_bs;

typedef struct
{
    // The type of the subframe: SUBFRAME_CONSTANT, SUBFRAME_VERBATIM, SUBFRAME_FIXED or SUBFRAME_LPC.
    dr_uint8 subframeType;

    // The number of wasted bits per sample as specified by the sub-frame header.
    dr_uint8 wastedBitsPerSample;

    // The order to use for the prediction stage for SUBFRAME_FIXED and SUBFRAME_LPC.
    dr_uint8 lpcOrder;

    // The number of bits per sample for this subframe. This is not always equal to the current frame's bit per sample because
    // an extra bit is required for side channels when interchannel decorrelation is being used.
    dr_uint32 bitsPerSample;

    // A pointer to the buffer containing the decoded samples in the subframe. This pointer is an offset from drflac::pExtraData, or
    // NULL if the heap is not being used. Note that it's a signed 32-bit integer for each value.
    dr_int32* pDecodedSamples;

} drflac_subframe;

typedef struct
{
    // If the stream uses variable block sizes, this will be set to the index of the first sample. If fixed block sizes are used, this will
    // always be set to 0.
    dr_uint64 sampleNumber;

    // If the stream uses fixed block sizes, this will be set to the frame number. If variable block sizes are used, this will always be 0.
    dr_uint32 frameNumber;

    // The sample rate of this frame.
    dr_uint32 sampleRate;

    // The number of samples in each sub-frame within this frame.
    dr_uint16 blockSize;

    // The channel assignment of this frame. This is not always set to the channel count. If interchannel decorrelation is being used this
    // will be set to DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE, DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE or DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE.
    dr_uint8 channelAssignment;

    // The number of bits per sample within this frame.
    dr_uint8 bitsPerSample;

    // The frame's CRC. This is set, but unused at the moment.
    dr_uint8 crc8;

} drflac_frame_header;

typedef struct
{
    // The header.
    drflac_frame_header header;

    // The number of samples left to be read in this frame. This is initially set to the block size multiplied by the channel count. As samples
    // are read, this will be decremented. When it reaches 0, the decoder will see this frame as fully consumed and load the next frame.
    dr_uint32 samplesRemaining;

    // The list of sub-frames within the frame. There is one sub-frame for each channel, and there's a maximum of 8 channels.
    drflac_subframe subframes[8];

} drflac_frame;

typedef struct
{
    // The function to call when a metadata block is read.
    drflac_meta_proc onMeta;

    // The user data posted to the metadata callback function.
    void* pUserDataMD;


    // The sample rate. Will be set to something like 44100.
    dr_uint32 sampleRate;

    // The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc. Maximum 8. This is set based on the
    // value specified in the STREAMINFO block.
    dr_uint8 channels;

    // The bits per sample. Will be set to somthing like 16, 24, etc.
    dr_uint8 bitsPerSample;

    // The maximum block size, in samples. This number represents the number of samples in each channel (not combined).
    dr_uint16 maxBlockSize;

    // The total number of samples making up the stream. This includes every channel. For example, if the stream has 2 channels,
    // with each channel having a total of 4096, this value will be set to 2*4096 = 8192. Can be 0 in which case it's still a
    // valid stream, but just means the total sample count is unknown. Likely the case with streams like internet radio.
    dr_uint64 totalSampleCount;


    // The container type. This is set based on whether or not the decoder was opened from a native or Ogg stream.
    drflac_container container;


    // The position of the seektable in the file.
    dr_uint64 seektablePos;

    // The size of the seektable.
    dr_uint32 seektableSize;


    // Information about the frame the decoder is currently sitting on.
    drflac_frame currentFrame;

    // The position of the first frame in the stream. This is only ever used for seeking.
    dr_uint64 firstFramePos;


    // A hack to avoid a malloc() when opening a decoder with drflac_open_memory().
    drflac__memory_stream memoryStream;



    // A pointer to the decoded sample data. This is an offset of pExtraData.
    dr_int32* pDecodedSamples;


    // The bit streamer. The raw FLAC data is fed through this object.
    drflac_bs bs;

    // Variable length extra data. We attach this to the end of the object so we avoid unnecessary mallocs.
    dr_uint8 pExtraData[1];

} drflac;


// Opens a FLAC decoder.
//
// onRead    [in]           The function to call when data needs to be read from the client.
// onSeek    [in]           The function to call when the read position of the client data needs to move.
// pUserData [in, optional] A pointer to application defined data that will be passed to onRead and onSeek.
//
// Returns a pointer to an object representing the decoder.
//
// Close the decoder with drflac_close().
//
// This function will automatically detect whether or not you are attempting to open a native or Ogg encapsulated
// FLAC, both of which should work seamlessly without any manual intervention. Ogg encapsulation also works with
// multiplexed streams which basically means it can play FLAC encoded audio tracks in videos.
//
// This is the lowest level function for opening a FLAC stream. You can also use drflac_open_file() and drflac_open_memory()
// to open the stream from a file or from a block of memory respectively.
//
// The STREAMINFO block must be present for this to succeed.
//
// See also: drflac_open_file(), drflac_open_memory(), drflac_open_with_metadata(), drflac_close()
drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData);

// Opens a FLAC decoder and notifies the caller of the metadata chunks (album art, etc.).
//
// onRead    [in]           The function to call when data needs to be read from the client.
// onSeek    [in]           The function to call when the read position of the client data needs to move.
// onMeta    [in]           The function to call for every metadata block.
// pUserData [in, optional] A pointer to application defined data that will be passed to onRead, onSeek and onMeta.
//
// Returns a pointer to an object representing the decoder.
//
// Close the decoder with drflac_close().
//
// This is slower than drflac_open(), so avoid this one if you don't need metadata. Internally, this will do a malloc()
// and free() for every metadata block except for STREAMINFO and PADDING blocks.
//
// The caller is notified of the metadata via the onMeta callback. All metadata blocks with be handled before the function
// returns.
//
// See also: drflac_open_file_with_metadata(), drflac_open_memory_with_metadata(), drflac_open(), drflac_close()
drflac* drflac_open_with_metadata(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData);

// Closes the given FLAC decoder.
//
// pFlac [in] The decoder to close.
//
// This will destroy the decoder object.
void drflac_close(drflac* pFlac);


// Reads sample data from the given FLAC decoder, output as interleaved signed 32-bit PCM.
//
// pFlac         [in]            The decoder.
// samplesToRead [in]            The number of samples to read.
// pBufferOut    [out, optional] A pointer to the buffer that will receive the decoded samples.
//
// Returns the number of samples actually read.
//
// pBufferOut can be null, in which case the call will act as a seek, and the return value will be the number of samples
// seeked.
dr_uint64 drflac_read_s32(drflac* pFlac, dr_uint64 samplesToRead, dr_int32* pBufferOut);

// Same as drflac_read_s32(), except outputs samples as 16-bit integer PCM rather than 32-bit.
//
// pFlac         [in]            The decoder.
// samplesToRead [in]            The number of samples to read.
// pBufferOut    [out, optional] A pointer to the buffer that will receive the decoded samples.
//
// Returns the number of samples actually read.
//
// pBufferOut can be null, in which case the call will act as a seek, and the return value will be the number of samples
// seeked.
//
// Note that this is lossey.
dr_uint64 drflac_read_s16(drflac* pFlac, dr_uint64 samplesToRead, dr_int16* pBufferOut);

// Same as drflac_read_s32(), except outputs samples as 32-bit floating-point PCM.
//
// pFlac         [in]            The decoder.
// samplesToRead [in]            The number of samples to read.
// pBufferOut    [out, optional] A pointer to the buffer that will receive the decoded samples.
//
// Returns the number of samples actually read.
//
// pBufferOut can be null, in which case the call will act as a seek, and the return value will be the number of samples
// seeked.
//
// Note that this is lossey.
dr_uint64 drflac_read_f32(drflac* pFlac, dr_uint64 samplesToRead, float* pBufferOut);

// Seeks to the sample at the given index.
//
// pFlac       [in] The decoder.
// sampleIndex [in] The index of the sample to seek to. See notes below.
//
// Returns DR_TRUE if successful; DR_FALSE otherwise.
//
// The sample index is based on interleaving. In a stereo stream, for example, the sample at index 0 is the first sample
// in the left channel; the sample at index 1 is the first sample on the right channel, and so on.
//
// When seeking, you will likely want to ensure it's rounded to a multiple of the channel count. You can do this with
// something like drflac_seek_to_sample(pFlac, (mySampleIndex + (mySampleIndex % pFlac->channels)))
dr_bool32 drflac_seek_to_sample(drflac* pFlac, dr_uint64 sampleIndex);



#ifndef DR_FLAC_NO_STDIO
// Opens a FLAC decoder from the file at the given path.
//
// filename [in] The path of the file to open, either absolute or relative to the current directory.
//
// Returns a pointer to an object representing the decoder.
//
// Close the decoder with drflac_close().
//
// This will hold a handle to the file until the decoder is closed with drflac_close(). Some platforms will restrict the
// number of files a process can have open at any given time, so keep this mind if you have many decoders open at the
// same time.
//
// See also: drflac_open(), drflac_open_file_with_metadata(), drflac_close()
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
//
// Sometimes a FLAC file won't keep track of the total sample count. In this situation the function will continuously
// read samples into a dynamically sized buffer on the heap until no samples are left.
//
// Do not call this function on a broadcast type of stream (like internet radio streams and whatnot).
dr_int32* drflac_open_and_decode_s32(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount);

// Same as drflac_open_and_decode_s32(), except returns signed 16-bit integer samples.
dr_int16* drflac_open_and_decode_s16(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount);

// Same as drflac_open_and_decode_s32(), except returns 32-bit floating-point samples.
float* drflac_open_and_decode_f32(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount);

#ifndef DR_FLAC_NO_STDIO
// Same as drflac_open_and_decode_s32() except opens the decoder from a file.
dr_int32* drflac_open_and_decode_file_s32(const char* filename, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount);

// Same as drflac_open_and_decode_file_s32(), except returns signed 16-bit integer samples.
dr_int16* drflac_open_and_decode_file_s16(const char* filename, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount);

// Same as drflac_open_and_decode_file_f32(), except returns 32-bit floating-point samples.
float* drflac_open_and_decode_file_f32(const char* filename, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount);
#endif

// Same as drflac_open_and_decode_s32() except opens the decoder from a block of memory.
dr_int32* drflac_open_and_decode_memory_s32(const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount);

// Same as drflac_open_and_decode_memory_s32(), except returns signed 16-bit integer samples.
dr_int16* drflac_open_and_decode_memory_s16(const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount);

// Same as drflac_open_and_decode_memory_s32(), except returns 32-bit floating-point samples.
float* drflac_open_and_decode_memory_f32(const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount);

// Frees data returned by drflac_open_and_decode_*().
void drflac_free(void* pSampleDataReturnedByOpenAndDecode);


// Structure representing an iterator for vorbis comments in a VORBIS_COMMENT metadata block.
typedef struct
{
    dr_uint32 countRemaining;
    const char* pRunningData;
} drflac_vorbis_comment_iterator;

// Initializes a vorbis comment iterator. This can be used for iterating over the vorbis comments in a VORBIS_COMMENT
// metadata block.
void drflac_init_vorbis_comment_iterator(drflac_vorbis_comment_iterator* pIter, dr_uint32 commentCount, const char* pComments);

// Goes to the next vorbis comment in the given iterator. If null is returned it means there are no more comments. The
// returned string is NOT null terminated.
const char* drflac_next_vorbis_comment(drflac_vorbis_comment_iterator* pIter, dr_uint32* pCommentLengthOut);



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
static DRFLAC_INLINE dr_bool32 drflac__is_little_endian()
{
    int n = 1;
    return (*(char*)&n) == 1;
}

static DRFLAC_INLINE dr_uint16 drflac__swap_endian_uint16(dr_uint16 n)
{
#ifdef _MSC_VER
    return _byteswap_ushort(n);
#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
    return __builtin_bswap16(n);
#else
    return ((n & 0xFF00) >> 8) |
           ((n & 0x00FF) << 8);
#endif
}

static DRFLAC_INLINE dr_uint32 drflac__swap_endian_uint32(dr_uint32 n)
{
#ifdef _MSC_VER
    return _byteswap_ulong(n);
#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
    return __builtin_bswap32(n);
#else
    return ((n & 0xFF000000) >> 24) |
           ((n & 0x00FF0000) >>  8) |
           ((n & 0x0000FF00) <<  8) |
           ((n & 0x000000FF) << 24);
#endif
}

static DRFLAC_INLINE dr_uint64 drflac__swap_endian_uint64(dr_uint64 n)
{
#ifdef _MSC_VER
    return _byteswap_uint64(n);
#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
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

static DRFLAC_INLINE dr_uint16 drflac__be2host_16(dr_uint16 n)
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

static DRFLAC_INLINE dr_uint32 drflac__be2host_32(dr_uint32 n)
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

static DRFLAC_INLINE dr_uint64 drflac__be2host_64(dr_uint64 n)
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


static DRFLAC_INLINE dr_uint32 drflac__le2host_32(dr_uint32 n)
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
#define DRFLAC_CACHE_L1_SIZE_BYTES(bs)                  (sizeof((bs)->cache))
#define DRFLAC_CACHE_L1_SIZE_BITS(bs)                   (sizeof((bs)->cache)*8)
#define DRFLAC_CACHE_L1_BITS_REMAINING(bs)              (DRFLAC_CACHE_L1_SIZE_BITS(bs) - ((bs)->consumedBits))
#ifdef DRFLAC_64BIT
#define DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount)       (~(((dr_uint64)-1LL) >> (_bitCount)))
#else
#define DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount)       (~(((dr_uint32)-1) >> (_bitCount)))
#endif
#define DRFLAC_CACHE_L1_SELECTION_SHIFT(bs, _bitCount)  (DRFLAC_CACHE_L1_SIZE_BITS(bs) - (_bitCount))
#define DRFLAC_CACHE_L1_SELECT(bs, _bitCount)           (((bs)->cache) & DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount))
#define DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, _bitCount) (DRFLAC_CACHE_L1_SELECT((bs), _bitCount) >> DRFLAC_CACHE_L1_SELECTION_SHIFT((bs), _bitCount))
#define DRFLAC_CACHE_L2_SIZE_BYTES(bs)                  (sizeof((bs)->cacheL2))
#define DRFLAC_CACHE_L2_LINE_COUNT(bs)                  (DRFLAC_CACHE_L2_SIZE_BYTES(bs) / sizeof((bs)->cacheL2[0]))
#define DRFLAC_CACHE_L2_LINES_REMAINING(bs)             (DRFLAC_CACHE_L2_LINE_COUNT(bs) - (bs)->nextL2Line)

static DRFLAC_INLINE dr_bool32 drflac__reload_l1_cache_from_l2(drflac_bs* bs)
{
    // Fast path. Try loading straight from L2.
    if (bs->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT(bs)) {
        bs->cache = bs->cacheL2[bs->nextL2Line++];
        return DR_TRUE;
    }

    // If we get here it means we've run out of data in the L2 cache. We'll need to fetch more from the client, if there's
    // any left.
    if (bs->unalignedByteCount > 0) {
        return DR_FALSE;   // If we have any unaligned bytes it means there's not more aligned bytes left in the client.
    }

    size_t bytesRead = bs->onRead(bs->pUserData, bs->cacheL2, DRFLAC_CACHE_L2_SIZE_BYTES(bs));

    bs->nextL2Line = 0;
    if (bytesRead == DRFLAC_CACHE_L2_SIZE_BYTES(bs)) {
        bs->cache = bs->cacheL2[bs->nextL2Line++];
        return DR_TRUE;
    }


    // If we get here it means we were unable to retrieve enough data to fill the entire L2 cache. It probably
    // means we've just reached the end of the file. We need to move the valid data down to the end of the buffer
    // and adjust the index of the next line accordingly. Also keep in mind that the L2 cache must be aligned to
    // the size of the L1 so we'll need to seek backwards by any misaligned bytes.
    size_t alignedL1LineCount = bytesRead / DRFLAC_CACHE_L1_SIZE_BYTES(bs);

    // We need to keep track of any unaligned bytes for later use.
    bs->unalignedByteCount = bytesRead - (alignedL1LineCount * DRFLAC_CACHE_L1_SIZE_BYTES(bs));
    if (bs->unalignedByteCount > 0) {
        bs->unalignedCache  = bs->cacheL2[alignedL1LineCount];
    }

    if (alignedL1LineCount > 0)
    {
        size_t offset = DRFLAC_CACHE_L2_LINE_COUNT(bs) - alignedL1LineCount;
        for (size_t i = alignedL1LineCount; i > 0; --i) {
            bs->cacheL2[i-1 + offset] = bs->cacheL2[i-1];
        }

        bs->nextL2Line = offset;
        bs->cache = bs->cacheL2[bs->nextL2Line++];
        return DR_TRUE;
    }
    else
    {
        // If we get into this branch it means we weren't able to load any L1-aligned data.
        bs->nextL2Line = DRFLAC_CACHE_L2_LINE_COUNT(bs);
        return DR_FALSE;
    }
}

static dr_bool32 drflac__reload_cache(drflac_bs* bs)
{
    // Fast path. Try just moving the next value in the L2 cache to the L1 cache.
    if (drflac__reload_l1_cache_from_l2(bs)) {
        bs->cache = drflac__be2host__cache_line(bs->cache);
        bs->consumedBits = 0;
        return DR_TRUE;
    }

    // Slow path.

    // If we get here it means we have failed to load the L1 cache from the L2. Likely we've just reached the end of the stream and the last
    // few bytes did not meet the alignment requirements for the L2 cache. In this case we need to fall back to a slower path and read the
    // data from the unaligned cache.
    size_t bytesRead = bs->unalignedByteCount;
    if (bytesRead == 0) {
        return DR_FALSE;
    }

    assert(bytesRead < DRFLAC_CACHE_L1_SIZE_BYTES(bs));
    bs->consumedBits = (DRFLAC_CACHE_L1_SIZE_BYTES(bs) - bytesRead) * 8;

    bs->cache = drflac__be2host__cache_line(bs->unalignedCache);
    bs->cache &= DRFLAC_CACHE_L1_SELECTION_MASK(DRFLAC_CACHE_L1_SIZE_BITS(bs) - bs->consumedBits);    // <-- Make sure the consumed bits are always set to zero. Other parts of the library depend on this property.
    return DR_TRUE;
}

static void drflac__reset_cache(drflac_bs* bs)
{
    bs->nextL2Line   = DRFLAC_CACHE_L2_LINE_COUNT(bs);  // <-- This clears the L2 cache.
    bs->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS(bs);   // <-- This clears the L1 cache.
    bs->cache = 0;
    bs->unalignedByteCount = 0;                         // <-- This clears the trailing unaligned bytes.
    bs->unalignedCache = 0;
}

static dr_bool32 drflac__seek_bits(drflac_bs* bs, size_t bitsToSeek)
{
    if (bitsToSeek <= DRFLAC_CACHE_L1_BITS_REMAINING(bs)) {
        bs->consumedBits += bitsToSeek;
        bs->cache <<= bitsToSeek;
        return DR_TRUE;
    } else {
        // It straddles the cached data. This function isn't called too frequently so I'm favouring simplicity here.
        bitsToSeek -= DRFLAC_CACHE_L1_BITS_REMAINING(bs);
        bs->consumedBits += DRFLAC_CACHE_L1_BITS_REMAINING(bs);
        bs->cache = 0;

        size_t wholeBytesRemaining = bitsToSeek/8;
        if (wholeBytesRemaining > 0)
        {
            // The next bytes to seek will be located in the L2 cache. The problem is that the L2 cache is not byte aligned,
            // but rather DRFLAC_CACHE_L1_SIZE_BYTES aligned (usually 4 or 8). If, for example, the number of bytes to seek is
            // 3, we'll need to handle it in a special way.
            size_t wholeCacheLinesRemaining = wholeBytesRemaining / DRFLAC_CACHE_L1_SIZE_BYTES(bs);
            if (wholeCacheLinesRemaining < DRFLAC_CACHE_L2_LINES_REMAINING(bs))
            {
                wholeBytesRemaining -= wholeCacheLinesRemaining * DRFLAC_CACHE_L1_SIZE_BYTES(bs);
                bitsToSeek -= wholeCacheLinesRemaining * DRFLAC_CACHE_L1_SIZE_BITS(bs);
                bs->nextL2Line += wholeCacheLinesRemaining;
            }
            else
            {
                wholeBytesRemaining -= DRFLAC_CACHE_L2_LINES_REMAINING(bs) * DRFLAC_CACHE_L1_SIZE_BYTES(bs);
                bitsToSeek -= DRFLAC_CACHE_L2_LINES_REMAINING(bs) * DRFLAC_CACHE_L1_SIZE_BITS(bs);
                bs->nextL2Line += DRFLAC_CACHE_L2_LINES_REMAINING(bs);

                if (wholeBytesRemaining > 0) {
                    bs->onSeek(bs->pUserData, (int)wholeBytesRemaining, drflac_seek_origin_current);
                    bitsToSeek -= wholeBytesRemaining*8;
                }
            }
        }


        if (bitsToSeek > 0) {
            if (!drflac__reload_cache(bs)) {
                return DR_FALSE;
            }

            return drflac__seek_bits(bs, bitsToSeek);
        }

        return DR_TRUE;
    }
}

static dr_bool32 drflac__read_uint32(drflac_bs* bs, unsigned int bitCount, dr_uint32* pResultOut)
{
    assert(bs != NULL);
    assert(pResultOut != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 32);

    if (bs->consumedBits == DRFLAC_CACHE_L1_SIZE_BITS(bs)) {
        if (!drflac__reload_cache(bs)) {
            return DR_FALSE;
        }
    }

    if (bitCount <= DRFLAC_CACHE_L1_BITS_REMAINING(bs)) {
        if (bitCount < DRFLAC_CACHE_L1_SIZE_BITS(bs)) {
            *pResultOut = DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCount);
            bs->consumedBits += bitCount;
            bs->cache <<= bitCount;
        } else {
            *pResultOut = (dr_uint32)bs->cache;
            bs->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS(bs);
            bs->cache = 0;
        }
        return DR_TRUE;
    } else {
        // It straddles the cached data. It will never cover more than the next chunk. We just read the number in two parts and combine them.
        size_t bitCountHi = DRFLAC_CACHE_L1_BITS_REMAINING(bs);
        size_t bitCountLo = bitCount - bitCountHi;
        dr_uint32 resultHi = DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCountHi);

        if (!drflac__reload_cache(bs)) {
            return DR_FALSE;
        }

        *pResultOut = (resultHi << bitCountLo) | DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCountLo);
        bs->consumedBits += bitCountLo;
        bs->cache <<= bitCountLo;
        return DR_TRUE;
    }
}

static dr_bool32 drflac__read_int32(drflac_bs* bs, unsigned int bitCount, dr_int32* pResult)
{
    assert(bs != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 32);

    dr_uint32 result;
    if (!drflac__read_uint32(bs, bitCount, &result)) {
        return DR_FALSE;
    }

    dr_uint32 signbit = ((result >> (bitCount-1)) & 0x01);
    result |= (~signbit + 1) << bitCount;

    *pResult = (dr_int32)result;
    return DR_TRUE;
}

static dr_bool32 drflac__read_uint64(drflac_bs* bs, unsigned int bitCount, dr_uint64* pResultOut)
{
    assert(bitCount <= 64);
    assert(bitCount >  32);

    dr_uint32 resultHi;
    if (!drflac__read_uint32(bs, bitCount - 32, &resultHi)) {
        return DR_FALSE;
    }

    dr_uint32 resultLo;
    if (!drflac__read_uint32(bs, 32, &resultLo)) {
        return DR_FALSE;
    }

    *pResultOut = (((dr_uint64)resultHi) << 32) | ((dr_uint64)resultLo);
    return DR_TRUE;
}

// Function below is unused, but leaving it here in case I need to quickly add it again.
#if 0
static dr_bool32 drflac__read_int64(drflac_bs* bs, unsigned int bitCount, dr_int64* pResultOut)
{
    assert(bitCount <= 64);

    dr_uint64 result;
    if (!drflac__read_uint64(bs, bitCount, &result)) {
        return DR_FALSE;
    }

    dr_uint64 signbit = ((result >> (bitCount-1)) & 0x01);
    result |= (~signbit + 1) << bitCount;

    *pResultOut = (dr_int64)result;
    return DR_TRUE;
}
#endif

static dr_bool32 drflac__read_uint16(drflac_bs* bs, unsigned int bitCount, dr_uint16* pResult)
{
    assert(bs != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 16);

    dr_uint32 result;
    if (!drflac__read_uint32(bs, bitCount, &result)) {
        return DR_FALSE;
    }

    *pResult = (dr_uint16)result;
    return DR_TRUE;
}

static dr_bool32 drflac__read_int16(drflac_bs* bs, unsigned int bitCount, dr_int16* pResult)
{
    assert(bs != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 16);

    dr_int32 result;
    if (!drflac__read_int32(bs, bitCount, &result)) {
        return DR_FALSE;
    }

    *pResult = (dr_int16)result;
    return DR_TRUE;
}

static dr_bool32 drflac__read_uint8(drflac_bs* bs, unsigned int bitCount, dr_uint8* pResult)
{
    assert(bs != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 8);

    dr_uint32 result;
    if (!drflac__read_uint32(bs, bitCount, &result)) {
        return DR_FALSE;
    }

    *pResult = (dr_uint8)result;
    return DR_TRUE;
}

static dr_bool32 drflac__read_int8(drflac_bs* bs, unsigned int bitCount, dr_int8* pResult)
{
    assert(bs != NULL);
    assert(pResult != NULL);
    assert(bitCount > 0);
    assert(bitCount <= 8);

    dr_int32 result;
    if (!drflac__read_int32(bs, bitCount, &result)) {
        return DR_FALSE;
    }

    *pResult = (dr_int8)result;
    return DR_TRUE;
}


static inline dr_bool32 drflac__seek_past_next_set_bit(drflac_bs* bs, unsigned int* pOffsetOut)
{
    unsigned int zeroCounter = 0;
    while (bs->cache == 0) {
        zeroCounter += (unsigned int)DRFLAC_CACHE_L1_BITS_REMAINING(bs);
        if (!drflac__reload_cache(bs)) {
            return DR_FALSE;
        }
    }

    // At this point the cache should not be zero, in which case we know the first set bit should be somewhere in here. There is
    // no need for us to perform any cache reloading logic here which should make things much faster.
    assert(bs->cache != 0);

    unsigned int bitOffsetTable[] = {
        0,
        4,
        3, 3,
        2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1
    };

    unsigned int setBitOffsetPlus1 = bitOffsetTable[DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, 4)];
    if (setBitOffsetPlus1 == 0) {
        if (bs->cache == 1) {
            setBitOffsetPlus1 = DRFLAC_CACHE_L1_SIZE_BITS(bs);
        } else {
            setBitOffsetPlus1 = 5;
            for (;;)
            {
                if ((bs->cache & DRFLAC_CACHE_L1_SELECT(bs, setBitOffsetPlus1))) {
                    break;
                }

                setBitOffsetPlus1 += 1;
            }
        }
    }

    bs->consumedBits += setBitOffsetPlus1;
    bs->cache <<= setBitOffsetPlus1;

    *pOffsetOut = zeroCounter + setBitOffsetPlus1 - 1;
    return DR_TRUE;
}



static dr_bool32 drflac__seek_to_byte(drflac_bs* bs, dr_uint64 offsetFromStart)
{
    assert(bs != NULL);
    assert(offsetFromStart > 0);

    // Seeking from the start is not quite as trivial as it sounds because the onSeek callback takes a signed 32-bit integer (which
    // is intentional because it simplifies the implementation of the onSeek callbacks), however offsetFromStart is unsigned 64-bit.
    // To resolve we just need to do an initial seek from the start, and then a series of offset seeks to make up the remainder.
    if (offsetFromStart > 0x7FFFFFFF)
    {
        dr_uint64 bytesRemaining = offsetFromStart;
        if (!bs->onSeek(bs->pUserData, 0x7FFFFFFF, drflac_seek_origin_start)) {
            return DR_FALSE;
        }
        bytesRemaining -= 0x7FFFFFFF;


        while (bytesRemaining > 0x7FFFFFFF) {
            if (!bs->onSeek(bs->pUserData, 0x7FFFFFFF, drflac_seek_origin_current)) {
                return DR_FALSE;
            }
            bytesRemaining -= 0x7FFFFFFF;
        }


        if (bytesRemaining > 0) {
            if (!bs->onSeek(bs->pUserData, (int)bytesRemaining, drflac_seek_origin_current)) {
                return DR_FALSE;
            }
        }
    }
    else
    {
        if (!bs->onSeek(bs->pUserData, (int)offsetFromStart, drflac_seek_origin_start)) {
            return DR_FALSE;
        }
    }


    // The cache should be reset to force a reload of fresh data from the client.
    drflac__reset_cache(bs);
    return DR_TRUE;
}


static dr_bool32 drflac__read_utf8_coded_number(drflac_bs* bs, dr_uint64* pNumberOut)
{
    assert(bs != NULL);
    assert(pNumberOut != NULL);

    unsigned char utf8[7] = {0};
    if (!drflac__read_uint8(bs, 8, utf8)) {
        *pNumberOut = 0;
        return DR_FALSE;
    }

    if ((utf8[0] & 0x80) == 0) {
        *pNumberOut = utf8[0];
        return DR_TRUE;
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
        return DR_FALSE;     // Bad UTF-8 encoding.
    }

    // Read extra bytes.
    assert(byteCount > 1);

    dr_uint64 result = (dr_uint64)(utf8[0] & (0xFF >> (byteCount + 1)));
    for (int i = 1; i < byteCount; ++i) {
        if (!drflac__read_uint8(bs, 8, utf8 + i)) {
            *pNumberOut = 0;
            return DR_FALSE;
        }

        result = (result << 6) | (utf8[i] & 0x3F);
    }

    *pNumberOut = result;
    return DR_TRUE;
}



static DRFLAC_INLINE dr_bool32 drflac__read_and_seek_rice(drflac_bs* bs, dr_uint8 m)
{
    unsigned int unused;
    if (!drflac__seek_past_next_set_bit(bs, &unused)) {
        return DR_FALSE;
    }

    if (m > 0) {
        if (!drflac__seek_bits(bs, m)) {
            return DR_FALSE;
        }
    }

    return DR_TRUE;
}


// The next two functions are responsible for calculating the prediction.
//
// When the bits per sample is >16 we need to use 64-bit integer arithmetic because otherwise we'll run out of precision. It's
// safe to assume this will be slower on 32-bit platforms so we use a more optimal solution when the bits per sample is <=16.
static DRFLAC_INLINE dr_int32 drflac__calculate_prediction_32(dr_uint32 order, dr_int32 shift, const dr_int16* coefficients, dr_int32* pDecodedSamples)
{
    assert(order <= 32);

    // 32-bit version.

    // VC++ optimizes this to a single jmp. I've not yet verified this for other compilers.
    dr_int32 prediction = 0;

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

    return (dr_int32)(prediction >> shift);
}

static DRFLAC_INLINE dr_int32 drflac__calculate_prediction_64(dr_uint32 order, dr_int32 shift, const dr_int16* coefficients, dr_int32* pDecodedSamples)
{
    assert(order <= 32);

    // 64-bit version.

    // This method is faster on the 32-bit build when compiling with VC++. See note below.
#ifndef DRFLAC_64BIT
    dr_int64 prediction;
    if (order == 8)
    {
        prediction  = coefficients[0] * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (dr_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (dr_int64)pDecodedSamples[-4];
        prediction += coefficients[4] * (dr_int64)pDecodedSamples[-5];
        prediction += coefficients[5] * (dr_int64)pDecodedSamples[-6];
        prediction += coefficients[6] * (dr_int64)pDecodedSamples[-7];
        prediction += coefficients[7] * (dr_int64)pDecodedSamples[-8];
    }
    else if (order == 7)
    {
        prediction  = coefficients[0] * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (dr_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (dr_int64)pDecodedSamples[-4];
        prediction += coefficients[4] * (dr_int64)pDecodedSamples[-5];
        prediction += coefficients[5] * (dr_int64)pDecodedSamples[-6];
        prediction += coefficients[6] * (dr_int64)pDecodedSamples[-7];
    }
    else if (order == 3)
    {
        prediction  = coefficients[0] * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (dr_int64)pDecodedSamples[-3];
    }
    else if (order == 6)
    {
        prediction  = coefficients[0] * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (dr_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (dr_int64)pDecodedSamples[-4];
        prediction += coefficients[4] * (dr_int64)pDecodedSamples[-5];
        prediction += coefficients[5] * (dr_int64)pDecodedSamples[-6];
    }
    else if (order == 5)
    {
        prediction  = coefficients[0] * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (dr_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (dr_int64)pDecodedSamples[-4];
        prediction += coefficients[4] * (dr_int64)pDecodedSamples[-5];
    }
    else if (order == 4)
    {
        prediction  = coefficients[0] * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (dr_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (dr_int64)pDecodedSamples[-4];
    }
    else if (order == 12)
    {
        prediction  = coefficients[0]  * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1]  * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2]  * (dr_int64)pDecodedSamples[-3];
        prediction += coefficients[3]  * (dr_int64)pDecodedSamples[-4];
        prediction += coefficients[4]  * (dr_int64)pDecodedSamples[-5];
        prediction += coefficients[5]  * (dr_int64)pDecodedSamples[-6];
        prediction += coefficients[6]  * (dr_int64)pDecodedSamples[-7];
        prediction += coefficients[7]  * (dr_int64)pDecodedSamples[-8];
        prediction += coefficients[8]  * (dr_int64)pDecodedSamples[-9];
        prediction += coefficients[9]  * (dr_int64)pDecodedSamples[-10];
        prediction += coefficients[10] * (dr_int64)pDecodedSamples[-11];
        prediction += coefficients[11] * (dr_int64)pDecodedSamples[-12];
    }
    else if (order == 2)
    {
        prediction  = coefficients[0] * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (dr_int64)pDecodedSamples[-2];
    }
    else if (order == 1)
    {
        prediction = coefficients[0] * (dr_int64)pDecodedSamples[-1];
    }
    else if (order == 10)
    {
        prediction  = coefficients[0]  * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1]  * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2]  * (dr_int64)pDecodedSamples[-3];
        prediction += coefficients[3]  * (dr_int64)pDecodedSamples[-4];
        prediction += coefficients[4]  * (dr_int64)pDecodedSamples[-5];
        prediction += coefficients[5]  * (dr_int64)pDecodedSamples[-6];
        prediction += coefficients[6]  * (dr_int64)pDecodedSamples[-7];
        prediction += coefficients[7]  * (dr_int64)pDecodedSamples[-8];
        prediction += coefficients[8]  * (dr_int64)pDecodedSamples[-9];
        prediction += coefficients[9]  * (dr_int64)pDecodedSamples[-10];
    }
    else if (order == 9)
    {
        prediction  = coefficients[0]  * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1]  * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2]  * (dr_int64)pDecodedSamples[-3];
        prediction += coefficients[3]  * (dr_int64)pDecodedSamples[-4];
        prediction += coefficients[4]  * (dr_int64)pDecodedSamples[-5];
        prediction += coefficients[5]  * (dr_int64)pDecodedSamples[-6];
        prediction += coefficients[6]  * (dr_int64)pDecodedSamples[-7];
        prediction += coefficients[7]  * (dr_int64)pDecodedSamples[-8];
        prediction += coefficients[8]  * (dr_int64)pDecodedSamples[-9];
    }
    else if (order == 11)
    {
        prediction  = coefficients[0]  * (dr_int64)pDecodedSamples[-1];
        prediction += coefficients[1]  * (dr_int64)pDecodedSamples[-2];
        prediction += coefficients[2]  * (dr_int64)pDecodedSamples[-3];
        prediction += coefficients[3]  * (dr_int64)pDecodedSamples[-4];
        prediction += coefficients[4]  * (dr_int64)pDecodedSamples[-5];
        prediction += coefficients[5]  * (dr_int64)pDecodedSamples[-6];
        prediction += coefficients[6]  * (dr_int64)pDecodedSamples[-7];
        prediction += coefficients[7]  * (dr_int64)pDecodedSamples[-8];
        prediction += coefficients[8]  * (dr_int64)pDecodedSamples[-9];
        prediction += coefficients[9]  * (dr_int64)pDecodedSamples[-10];
        prediction += coefficients[10] * (dr_int64)pDecodedSamples[-11];
    }
    else
    {
        prediction = 0;
        for (int j = 0; j < (int)order; ++j) {
            prediction += coefficients[j] * (dr_int64)pDecodedSamples[-j-1];
        }
    }
#endif

    // VC++ optimizes this to a single jmp instruction, but only the 64-bit build. The 32-bit build generates less efficient code for some
    // reason. The ugly version above is faster so we'll just switch between the two depending on the target platform.
#ifdef DRFLAC_64BIT
    dr_int64 prediction = 0;

    switch (order)
    {
    case 32: prediction += coefficients[31] * (dr_int64)pDecodedSamples[-32];
    case 31: prediction += coefficients[30] * (dr_int64)pDecodedSamples[-31];
    case 30: prediction += coefficients[29] * (dr_int64)pDecodedSamples[-30];
    case 29: prediction += coefficients[28] * (dr_int64)pDecodedSamples[-29];
    case 28: prediction += coefficients[27] * (dr_int64)pDecodedSamples[-28];
    case 27: prediction += coefficients[26] * (dr_int64)pDecodedSamples[-27];
    case 26: prediction += coefficients[25] * (dr_int64)pDecodedSamples[-26];
    case 25: prediction += coefficients[24] * (dr_int64)pDecodedSamples[-25];
    case 24: prediction += coefficients[23] * (dr_int64)pDecodedSamples[-24];
    case 23: prediction += coefficients[22] * (dr_int64)pDecodedSamples[-23];
    case 22: prediction += coefficients[21] * (dr_int64)pDecodedSamples[-22];
    case 21: prediction += coefficients[20] * (dr_int64)pDecodedSamples[-21];
    case 20: prediction += coefficients[19] * (dr_int64)pDecodedSamples[-20];
    case 19: prediction += coefficients[18] * (dr_int64)pDecodedSamples[-19];
    case 18: prediction += coefficients[17] * (dr_int64)pDecodedSamples[-18];
    case 17: prediction += coefficients[16] * (dr_int64)pDecodedSamples[-17];
    case 16: prediction += coefficients[15] * (dr_int64)pDecodedSamples[-16];
    case 15: prediction += coefficients[14] * (dr_int64)pDecodedSamples[-15];
    case 14: prediction += coefficients[13] * (dr_int64)pDecodedSamples[-14];
    case 13: prediction += coefficients[12] * (dr_int64)pDecodedSamples[-13];
    case 12: prediction += coefficients[11] * (dr_int64)pDecodedSamples[-12];
    case 11: prediction += coefficients[10] * (dr_int64)pDecodedSamples[-11];
    case 10: prediction += coefficients[ 9] * (dr_int64)pDecodedSamples[-10];
    case  9: prediction += coefficients[ 8] * (dr_int64)pDecodedSamples[- 9];
    case  8: prediction += coefficients[ 7] * (dr_int64)pDecodedSamples[- 8];
    case  7: prediction += coefficients[ 6] * (dr_int64)pDecodedSamples[- 7];
    case  6: prediction += coefficients[ 5] * (dr_int64)pDecodedSamples[- 6];
    case  5: prediction += coefficients[ 4] * (dr_int64)pDecodedSamples[- 5];
    case  4: prediction += coefficients[ 3] * (dr_int64)pDecodedSamples[- 4];
    case  3: prediction += coefficients[ 2] * (dr_int64)pDecodedSamples[- 3];
    case  2: prediction += coefficients[ 1] * (dr_int64)pDecodedSamples[- 2];
    case  1: prediction += coefficients[ 0] * (dr_int64)pDecodedSamples[- 1];
    }
#endif

    return (dr_int32)(prediction >> shift);
}


// Reads and decodes a string of residual values as Rice codes. The decoder should be sitting on the first bit of the Rice codes.
//
// This is the most frequently called function in the library. It does both the Rice decoding and the prediction in a single loop
// iteration. The prediction is done at the end, and there's an annoying branch I'd like to avoid so the main function is defined
// as a #define - sue me!
#define DRFLAC__DECODE_SAMPLES_WITH_RESIDULE__RICE__PROC(funcName, predictionFunc)                                                                                  \
static dr_bool32 funcName (drflac_bs* bs, dr_uint32 count, dr_uint8 riceParam, dr_uint32 order, dr_int32 shift, const dr_int16* coefficients, dr_int32* pSamplesOut)\
{                                                                                                                                                                   \
    assert(bs != NULL);                                                                                                                                             \
    assert(count > 0);                                                                                                                                              \
    assert(pSamplesOut != NULL);                                                                                                                                    \
                                                                                                                                                                    \
    static unsigned int bitOffsetTable[] = {                                                                                                                        \
        0,                                                                                                                                                          \
        4,                                                                                                                                                          \
        3, 3,                                                                                                                                                       \
        2, 2, 2, 2,                                                                                                                                                 \
        1, 1, 1, 1, 1, 1, 1, 1                                                                                                                                      \
    };                                                                                                                                                              \
                                                                                                                                                                    \
    drflac_cache_t riceParamMask = DRFLAC_CACHE_L1_SELECTION_MASK(riceParam);                                                                                       \
    drflac_cache_t resultHiShift = DRFLAC_CACHE_L1_SIZE_BITS(bs) - riceParam;                                                                                       \
                                                                                                                                                                    \
    for (int i = 0; i < (int)count; ++i)                                                                                                                            \
    {                                                                                                                                                               \
        unsigned int zeroCounter = 0;                                                                                                                               \
        while (bs->cache == 0) {                                                                                                                                    \
            zeroCounter += (unsigned int)DRFLAC_CACHE_L1_BITS_REMAINING(bs);                                                                                        \
            if (!drflac__reload_cache(bs)) {                                                                                                                        \
                return DR_FALSE;                                                                                                                                    \
            }                                                                                                                                                       \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        /* At this point the cache should not be zero, in which case we know the first set bit should be somewhere in here. There is                                \
           no need for us to perform any cache reloading logic here which should make things much faster. */                                                        \
        assert(bs->cache != 0);                                                                                                                                     \
        unsigned int decodedRice;                                                                                                                                   \
                                                                                                                                                                    \
        unsigned int setBitOffsetPlus1 = bitOffsetTable[DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, 4)];                                                                   \
        if (setBitOffsetPlus1 > 0) {                                                                                                                                \
            decodedRice = (zeroCounter + (setBitOffsetPlus1-1)) << riceParam;                                                                                       \
        } else {                                                                                                                                                    \
            if (bs->cache == 1) {                                                                                                                                   \
                setBitOffsetPlus1 = DRFLAC_CACHE_L1_SIZE_BITS(bs);                                                                                                  \
                decodedRice = (zeroCounter + (DRFLAC_CACHE_L1_SIZE_BITS(bs)-1)) << riceParam;                                                                       \
            } else {                                                                                                                                                \
                setBitOffsetPlus1 = 5;                                                                                                                              \
                for (;;)                                                                                                                                            \
                {                                                                                                                                                   \
                    if ((bs->cache & DRFLAC_CACHE_L1_SELECT(bs, setBitOffsetPlus1))) {                                                                              \
                        decodedRice = (zeroCounter + (setBitOffsetPlus1-1)) << riceParam;                                                                           \
                        break;                                                                                                                                      \
                    }                                                                                                                                               \
                                                                                                                                                                    \
                    setBitOffsetPlus1 += 1;                                                                                                                         \
                }                                                                                                                                                   \
            }                                                                                                                                                       \
        }                                                                                                                                                           \
                                                                                                                                                                    \
                                                                                                                                                                    \
        unsigned int bitsLo = 0;                                                                                                                                    \
        unsigned int riceLength = setBitOffsetPlus1 + riceParam;                                                                                                    \
        if (riceLength < DRFLAC_CACHE_L1_BITS_REMAINING(bs))                                                                                                        \
        {                                                                                                                                                           \
            bitsLo = (unsigned int)((bs->cache & (riceParamMask >> setBitOffsetPlus1)) >> (DRFLAC_CACHE_L1_SIZE_BITS(bs) - riceLength));                            \
                                                                                                                                                                    \
            bs->consumedBits += riceLength;                                                                                                                         \
            bs->cache <<= riceLength;                                                                                                                               \
        }                                                                                                                                                           \
        else                                                                                                                                                        \
        {                                                                                                                                                           \
            bs->consumedBits += riceLength;                                                                                                                         \
            bs->cache <<= setBitOffsetPlus1;                                                                                                                        \
                                                                                                                                                                    \
            /* It straddles the cached data. It will never cover more than the next chunk. We just read the number in two parts and combine them. */                \
            size_t bitCountLo = bs->consumedBits - DRFLAC_CACHE_L1_SIZE_BITS(bs);                                                                                   \
            drflac_cache_t resultHi = bs->cache & riceParamMask;    /* <-- This mask is OK because all bits after the first bits are always zero. */                \
                                                                                                                                                                    \
                                                                                                                                                                    \
            if (bs->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT(bs)) {                                                                                                  \
                bs->cache = drflac__be2host__cache_line(bs->cacheL2[bs->nextL2Line++]);                                                                             \
            } else {                                                                                                                                                \
                /* Slow path. We need to fetch more data from the client. */                                                                                        \
                if (!drflac__reload_cache(bs)) {                                                                                                                    \
                    return DR_FALSE;                                                                                                                                \
                }                                                                                                                                                   \
            }                                                                                                                                                       \
                                                                                                                                                                    \
            bitsLo = (unsigned int)((resultHi >> resultHiShift) | DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCountLo));                                                \
            bs->consumedBits = bitCountLo;                                                                                                                          \
            bs->cache <<= bitCountLo;                                                                                                                               \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        decodedRice |= bitsLo;                                                                                                                                      \
        decodedRice = (decodedRice >> 1) ^ (~(decodedRice & 0x01) + 1);   /* <-- Ah, much faster! :) */                                                             \
        /*                                                                                                                                                          \
        if ((decodedRice & 0x01)) {                                                                                                                                 \
            decodedRice = ~(decodedRice >> 1);                                                                                                                      \
        } else {                                                                                                                                                    \
            decodedRice = (decodedRice >> 1);                                                                                                                       \
        }                                                                                                                                                           \
        */                                                                                                                                                          \
                                                                                                                                                                    \
        /* In order to properly calculate the prediction when the bits per sample is >16 we need to do it using 64-bit arithmetic. We can assume this               \
           is probably going to be slower on 32-bit systems so we'll do a more optimized 32-bit version when the bits per sample is low enough.*/                   \
        pSamplesOut[i] = ((int)decodedRice + predictionFunc(order, shift, coefficients, pSamplesOut + i));                                                          \
    }                                                                                                                                                               \
                                                                                                                                                                    \
    return DR_TRUE;                                                                                                                                                 \
}                                                                                                                                                                   \

DRFLAC__DECODE_SAMPLES_WITH_RESIDULE__RICE__PROC(drflac__decode_samples_with_residual__rice_64, drflac__calculate_prediction_64)
DRFLAC__DECODE_SAMPLES_WITH_RESIDULE__RICE__PROC(drflac__decode_samples_with_residual__rice_32, drflac__calculate_prediction_32)


// Reads and seeks past a string of residual values as Rice codes. The decoder should be sitting on the first bit of the Rice codes.
static dr_bool32 drflac__read_and_seek_residual__rice(drflac_bs* bs, dr_uint32 count, dr_uint8 riceParam)
{
    assert(bs != NULL);
    assert(count > 0);

    for (dr_uint32 i = 0; i < count; ++i) {
        if (!drflac__read_and_seek_rice(bs, riceParam)) {
            return DR_FALSE;
        }
    }

    return DR_TRUE;
}

static dr_bool32 drflac__decode_samples_with_residual__unencoded(drflac_bs* bs, dr_uint32 bitsPerSample, dr_uint32 count, dr_uint8 unencodedBitsPerSample, dr_uint32 order, dr_int32 shift, const dr_int16* coefficients, dr_int32* pSamplesOut)
{
    assert(bs != NULL);
    assert(count > 0);
    assert(unencodedBitsPerSample > 0 && unencodedBitsPerSample <= 32);
    assert(pSamplesOut != NULL);

    for (unsigned int i = 0; i < count; ++i)
    {
        if (!drflac__read_int32(bs, unencodedBitsPerSample, pSamplesOut + i)) {
            return DR_FALSE;
        }

        if (bitsPerSample > 16) {
            pSamplesOut[i] += drflac__calculate_prediction_64(order, shift, coefficients, pSamplesOut + i);
        } else {
            pSamplesOut[i] += drflac__calculate_prediction_32(order, shift, coefficients, pSamplesOut + i);
        }
    }

    return DR_TRUE;
}


// Reads and decodes the residual for the sub-frame the decoder is currently sitting on. This function should be called
// when the decoder is sitting at the very start of the RESIDUAL block. The first <order> residuals will be ignored. The
// <blockSize> and <order> parameters are used to determine how many residual values need to be decoded.
static dr_bool32 drflac__decode_samples_with_residual(drflac_bs* bs, dr_uint32 bitsPerSample, dr_uint32 blockSize, dr_uint32 order, dr_int32 shift, const dr_int16* coefficients, dr_int32* pDecodedSamples)
{
    assert(bs != NULL);
    assert(blockSize != 0);
    assert(pDecodedSamples != NULL);       // <-- Should we allow NULL, in which case we just seek past the residual rather than do a full decode?

    dr_uint8 residualMethod;
    if (!drflac__read_uint8(bs, 2, &residualMethod)) {
        return DR_FALSE;
    }

    if (residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE && residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
        return DR_FALSE;    // Unknown or unsupported residual coding method.
    }

    // Ignore the first <order> values.
    pDecodedSamples += order;


    dr_uint8 partitionOrder;
    if (!drflac__read_uint8(bs, 4, &partitionOrder)) {
        return DR_FALSE;
    }


    dr_uint32 samplesInPartition = (blockSize / (1 << partitionOrder)) - order;
    dr_uint32 partitionsRemaining = (1 << partitionOrder);
    for (;;)
    {
        dr_uint8 riceParam = 0;
        if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
            if (!drflac__read_uint8(bs, 4, &riceParam)) {
                return DR_FALSE;
            }
            if (riceParam == 16) {
                riceParam = 0xFF;
            }
        } else if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
            if (!drflac__read_uint8(bs, 5, &riceParam)) {
                return DR_FALSE;
            }
            if (riceParam == 32) {
                riceParam = 0xFF;
            }
        }

        if (riceParam != 0xFF) {
            if (bitsPerSample > 16) {
                if (!drflac__decode_samples_with_residual__rice_64(bs, samplesInPartition, riceParam, order, shift, coefficients, pDecodedSamples)) {
                    return DR_FALSE;
                }
            } else {
                if (!drflac__decode_samples_with_residual__rice_32(bs, samplesInPartition, riceParam, order, shift, coefficients, pDecodedSamples)) {
                    return DR_FALSE;
                }
            }
        } else {
            unsigned char unencodedBitsPerSample = 0;
            if (!drflac__read_uint8(bs, 5, &unencodedBitsPerSample)) {
                return DR_FALSE;
            }

            if (!drflac__decode_samples_with_residual__unencoded(bs, bitsPerSample, samplesInPartition, unencodedBitsPerSample, order, shift, coefficients, pDecodedSamples)) {
                return DR_FALSE;
            }
        }

        pDecodedSamples += samplesInPartition;


        if (partitionsRemaining == 1) {
            break;
        }

        partitionsRemaining -= 1;
        samplesInPartition = blockSize / (1 << partitionOrder);
    }

    return DR_TRUE;
}

// Reads and seeks past the residual for the sub-frame the decoder is currently sitting on. This function should be called
// when the decoder is sitting at the very start of the RESIDUAL block. The first <order> residuals will be set to 0. The
// <blockSize> and <order> parameters are used to determine how many residual values need to be decoded.
static dr_bool32 drflac__read_and_seek_residual(drflac_bs* bs, dr_uint32 blockSize, dr_uint32 order)
{
    assert(bs != NULL);
    assert(blockSize != 0);

    dr_uint8 residualMethod;
    if (!drflac__read_uint8(bs, 2, &residualMethod)) {
        return DR_FALSE;
    }

    if (residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE && residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
        return DR_FALSE;    // Unknown or unsupported residual coding method.
    }

    dr_uint8 partitionOrder;
    if (!drflac__read_uint8(bs, 4, &partitionOrder)) {
        return DR_FALSE;
    }

    dr_uint32 samplesInPartition = (blockSize / (1 << partitionOrder)) - order;
    dr_uint32 partitionsRemaining = (1 << partitionOrder);
    for (;;)
    {
        dr_uint8 riceParam = 0;
        if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
            if (!drflac__read_uint8(bs, 4, &riceParam)) {
                return DR_FALSE;
            }
            if (riceParam == 16) {
                riceParam = 0xFF;
            }
        } else if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
            if (!drflac__read_uint8(bs, 5, &riceParam)) {
                return DR_FALSE;
            }
            if (riceParam == 32) {
                riceParam = 0xFF;
            }
        }

        if (riceParam != 0xFF) {
            if (!drflac__read_and_seek_residual__rice(bs, samplesInPartition, riceParam)) {
                return DR_FALSE;
            }
        } else {
            unsigned char unencodedBitsPerSample = 0;
            if (!drflac__read_uint8(bs, 5, &unencodedBitsPerSample)) {
                return DR_FALSE;
            }

            if (!drflac__seek_bits(bs, unencodedBitsPerSample * samplesInPartition)) {
                return DR_FALSE;
            }
        }


        if (partitionsRemaining == 1) {
            break;
        }

        partitionsRemaining -= 1;
        samplesInPartition = blockSize / (1 << partitionOrder);
    }

    return DR_TRUE;
}


static dr_bool32 drflac__decode_samples__constant(drflac_bs* bs, dr_uint32 blockSize, dr_uint32 bitsPerSample, dr_int32* pDecodedSamples)
{
    // Only a single sample needs to be decoded here.
    dr_int32 sample;
    if (!drflac__read_int32(bs, bitsPerSample, &sample)) {
        return DR_FALSE;
    }

    // We don't really need to expand this, but it does simplify the process of reading samples. If this becomes a performance issue (unlikely)
    // we'll want to look at a more efficient way.
    for (dr_uint32 i = 0; i < blockSize; ++i) {
        pDecodedSamples[i] = sample;
    }

    return DR_TRUE;
}

static dr_bool32 drflac__decode_samples__verbatim(drflac_bs* bs, dr_uint32 blockSize, dr_uint32 bitsPerSample, dr_int32* pDecodedSamples)
{
    for (dr_uint32 i = 0; i < blockSize; ++i) {
        dr_int32 sample;
        if (!drflac__read_int32(bs, bitsPerSample, &sample)) {
            return DR_FALSE;
        }

        pDecodedSamples[i] = sample;
    }

    return DR_TRUE;
}

static dr_bool32 drflac__decode_samples__fixed(drflac_bs* bs, dr_uint32 blockSize, dr_uint32 bitsPerSample, dr_uint8 lpcOrder, dr_int32* pDecodedSamples)
{
    short lpcCoefficientsTable[5][4] = {
        {0,  0, 0,  0},
        {1,  0, 0,  0},
        {2, -1, 0,  0},
        {3, -3, 1,  0},
        {4, -6, 4, -1}
    };

    // Warm up samples and coefficients.
    for (dr_uint32 i = 0; i < lpcOrder; ++i) {
        dr_int32 sample;
        if (!drflac__read_int32(bs, bitsPerSample, &sample)) {
            return DR_FALSE;
        }

        pDecodedSamples[i] = sample;
    }


    if (!drflac__decode_samples_with_residual(bs, bitsPerSample, blockSize, lpcOrder, 0, lpcCoefficientsTable[lpcOrder], pDecodedSamples)) {
        return DR_FALSE;
    }

    return DR_TRUE;
}

static dr_bool32 drflac__decode_samples__lpc(drflac_bs* bs, dr_uint32 blockSize, dr_uint32 bitsPerSample, dr_uint8 lpcOrder, dr_int32* pDecodedSamples)
{
    // Warm up samples.
    for (dr_uint8 i = 0; i < lpcOrder; ++i) {
        dr_int32 sample;
        if (!drflac__read_int32(bs, bitsPerSample, &sample)) {
            return DR_FALSE;
        }

        pDecodedSamples[i] = sample;
    }

    dr_uint8 lpcPrecision;
    if (!drflac__read_uint8(bs, 4, &lpcPrecision)) {
        return DR_FALSE;
    }
    if (lpcPrecision == 15) {
        return DR_FALSE;    // Invalid.
    }
    lpcPrecision += 1;


    dr_int8 lpcShift;
    if (!drflac__read_int8(bs, 5, &lpcShift)) {
        return DR_FALSE;
    }


    dr_int16 coefficients[32];
    for (dr_uint8 i = 0; i < lpcOrder; ++i) {
        if (!drflac__read_int16(bs, lpcPrecision, coefficients + i)) {
            return DR_FALSE;
        }
    }

    if (!drflac__decode_samples_with_residual(bs, bitsPerSample, blockSize, lpcOrder, lpcShift, coefficients, pDecodedSamples)) {
        return DR_FALSE;
    }

    return DR_TRUE;
}


static dr_bool32 drflac__read_next_frame_header(drflac_bs* bs, dr_uint8 streaminfoBitsPerSample, drflac_frame_header* header)
{
    assert(bs != NULL);
    assert(header != NULL);

    // At the moment the sync code is as a form of basic validation. The CRC is stored, but is unused at the moment. This
    // should probably be handled better in the future.

    const dr_uint32 sampleRateTable[12]  = {0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
    const dr_uint8 bitsPerSampleTable[8] = {0, 8, 12, (dr_uint8)-1, 16, 20, 24, (dr_uint8)-1};   // -1 = reserved.

    dr_uint16 syncCode = 0;
    if (!drflac__read_uint16(bs, 14, &syncCode)) {
        return DR_FALSE;
    }

    if (syncCode != 0x3FFE) {
        // TODO: Try and recover by attempting to seek to and read the next frame?
        return DR_FALSE;
    }

    dr_uint8 reserved;
    if (!drflac__read_uint8(bs, 1, &reserved)) {
        return DR_FALSE;
    }

    dr_uint8 blockingStrategy = 0;
    if (!drflac__read_uint8(bs, 1, &blockingStrategy)) {
        return DR_FALSE;
    }



    dr_uint8 blockSize = 0;
    if (!drflac__read_uint8(bs, 4, &blockSize)) {
        return DR_FALSE;
    }

    dr_uint8 sampleRate = 0;
    if (!drflac__read_uint8(bs, 4, &sampleRate)) {
        return DR_FALSE;
    }

    dr_uint8 channelAssignment = 0;
    if (!drflac__read_uint8(bs, 4, &channelAssignment)) {
        return DR_FALSE;
    }

    dr_uint8 bitsPerSample = 0;
    if (!drflac__read_uint8(bs, 3, &bitsPerSample)) {
        return DR_FALSE;
    }

    if (!drflac__read_uint8(bs, 1, &reserved)) {
        return DR_FALSE;
    }


    dr_bool32 isVariableBlockSize = blockingStrategy == 1;
    if (isVariableBlockSize) {
        dr_uint64 sampleNumber;
        if (!drflac__read_utf8_coded_number(bs, &sampleNumber)) {
            return DR_FALSE;
        }
        header->frameNumber  = 0;
        header->sampleNumber = sampleNumber;
    } else {
        dr_uint64 frameNumber = 0;
        if (!drflac__read_utf8_coded_number(bs, &frameNumber)) {
            return DR_FALSE;
        }
        header->frameNumber  = (dr_uint32)frameNumber;   // <-- Safe cast.
        header->sampleNumber = 0;
    }


    if (blockSize == 1) {
        header->blockSize = 192;
    } else if (blockSize >= 2 && blockSize <= 5) {
        header->blockSize = 576 * (1 << (blockSize - 2));
    } else if (blockSize == 6) {
        if (!drflac__read_uint16(bs, 8, &header->blockSize)) {
            return DR_FALSE;
        }
        header->blockSize += 1;
    } else if (blockSize == 7) {
        if (!drflac__read_uint16(bs, 16, &header->blockSize)) {
            return DR_FALSE;
        }
        header->blockSize += 1;
    } else {
        header->blockSize = 256 * (1 << (blockSize - 8));
    }


    if (sampleRate <= 11) {
        header->sampleRate = sampleRateTable[sampleRate];
    } else if (sampleRate == 12) {
        if (!drflac__read_uint32(bs, 8, &header->sampleRate)) {
            return DR_FALSE;
        }
        header->sampleRate *= 1000;
    } else if (sampleRate == 13) {
        if (!drflac__read_uint32(bs, 16, &header->sampleRate)) {
            return DR_FALSE;
        }
    } else if (sampleRate == 14) {
        if (!drflac__read_uint32(bs, 16, &header->sampleRate)) {
            return DR_FALSE;
        }
        header->sampleRate *= 10;
    } else {
        return DR_FALSE;  // Invalid.
    }


    header->channelAssignment = channelAssignment;

    header->bitsPerSample = bitsPerSampleTable[bitsPerSample];
    if (header->bitsPerSample == 0) {
        header->bitsPerSample = streaminfoBitsPerSample;
    }

    if (drflac__read_uint8(bs, 8, &header->crc8) != 1) {
        return DR_FALSE;
    }

    return DR_TRUE;
}

static dr_bool32 drflac__read_subframe_header(drflac_bs* bs, drflac_subframe* pSubframe)
{
    dr_uint8 header;
    if (!drflac__read_uint8(bs, 8, &header)) {
        return DR_FALSE;
    }

    // First bit should always be 0.
    if ((header & 0x80) != 0) {
        return DR_FALSE;
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
        return DR_FALSE;
    }

    // Wasted bits per sample.
    pSubframe->wastedBitsPerSample = 0;
    if ((header & 0x01) == 1) {
        unsigned int wastedBitsPerSample;
        if (!drflac__seek_past_next_set_bit(bs, &wastedBitsPerSample)) {
            return DR_FALSE;
        }
        pSubframe->wastedBitsPerSample = (unsigned char)wastedBitsPerSample + 1;
    }

    return DR_TRUE;
}

static dr_bool32 drflac__decode_subframe(drflac_bs* bs, drflac_frame* frame, int subframeIndex, dr_int32* pDecodedSamplesOut)
{
    assert(bs != NULL);
    assert(frame != NULL);

    drflac_subframe* pSubframe = frame->subframes + subframeIndex;
    if (!drflac__read_subframe_header(bs, pSubframe)) {
        return DR_FALSE;
    }

    // Side channels require an extra bit per sample. Took a while to figure that one out...
    pSubframe->bitsPerSample = frame->header.bitsPerSample;
    if ((frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE || frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE) && subframeIndex == 1) {
        pSubframe->bitsPerSample += 1;
    } else if (frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE && subframeIndex == 0) {
        pSubframe->bitsPerSample += 1;
    }

    // Need to handle wasted bits per sample.
    pSubframe->bitsPerSample -= pSubframe->wastedBitsPerSample;
    pSubframe->pDecodedSamples = pDecodedSamplesOut;

    switch (pSubframe->subframeType)
    {
        case DRFLAC_SUBFRAME_CONSTANT:
        {
            drflac__decode_samples__constant(bs, frame->header.blockSize, pSubframe->bitsPerSample, pSubframe->pDecodedSamples);
        } break;

        case DRFLAC_SUBFRAME_VERBATIM:
        {
            drflac__decode_samples__verbatim(bs, frame->header.blockSize, pSubframe->bitsPerSample, pSubframe->pDecodedSamples);
        } break;

        case DRFLAC_SUBFRAME_FIXED:
        {
            drflac__decode_samples__fixed(bs, frame->header.blockSize, pSubframe->bitsPerSample, pSubframe->lpcOrder, pSubframe->pDecodedSamples);
        } break;

        case DRFLAC_SUBFRAME_LPC:
        {
            drflac__decode_samples__lpc(bs, frame->header.blockSize, pSubframe->bitsPerSample, pSubframe->lpcOrder, pSubframe->pDecodedSamples);
        } break;

        default: return DR_FALSE;
    }

    return DR_TRUE;
}

static dr_bool32 drflac__seek_subframe(drflac_bs* bs, drflac_frame* frame, int subframeIndex)
{
    assert(bs != NULL);
    assert(frame != NULL);

    drflac_subframe* pSubframe = frame->subframes + subframeIndex;
    if (!drflac__read_subframe_header(bs, pSubframe)) {
        return DR_FALSE;
    }

    // Side channels require an extra bit per sample. Took a while to figure that one out...
    pSubframe->bitsPerSample = frame->header.bitsPerSample;
    if ((frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE || frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE) && subframeIndex == 1) {
        pSubframe->bitsPerSample += 1;
    } else if (frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE && subframeIndex == 0) {
        pSubframe->bitsPerSample += 1;
    }

    // Need to handle wasted bits per sample.
    pSubframe->bitsPerSample -= pSubframe->wastedBitsPerSample;
    pSubframe->pDecodedSamples = NULL;
    //pSubframe->pDecodedSamples = pFlac->pDecodedSamples + (pFlac->currentFrame.header.blockSize * subframeIndex);

    switch (pSubframe->subframeType)
    {
        case DRFLAC_SUBFRAME_CONSTANT:
        {
            if (!drflac__seek_bits(bs, pSubframe->bitsPerSample)) {
                return DR_FALSE;
            }
        } break;

        case DRFLAC_SUBFRAME_VERBATIM:
        {
            unsigned int bitsToSeek = frame->header.blockSize * pSubframe->bitsPerSample;
            if (!drflac__seek_bits(bs, bitsToSeek)) {
                return DR_FALSE;
            }
        } break;

        case DRFLAC_SUBFRAME_FIXED:
        {
            unsigned int bitsToSeek = pSubframe->lpcOrder * pSubframe->bitsPerSample;
            if (!drflac__seek_bits(bs, bitsToSeek)) {
                return DR_FALSE;
            }

            if (!drflac__read_and_seek_residual(bs, frame->header.blockSize, pSubframe->lpcOrder)) {
                return DR_FALSE;
            }
        } break;

        case DRFLAC_SUBFRAME_LPC:
        {
            unsigned int bitsToSeek = pSubframe->lpcOrder * pSubframe->bitsPerSample;
            if (!drflac__seek_bits(bs, bitsToSeek)) {
                return DR_FALSE;
            }

            unsigned char lpcPrecision;
            if (!drflac__read_uint8(bs, 4, &lpcPrecision)) {
                return DR_FALSE;
            }
            if (lpcPrecision == 15) {
                return DR_FALSE;    // Invalid.
            }
            lpcPrecision += 1;


            bitsToSeek = (pSubframe->lpcOrder * lpcPrecision) + 5;    // +5 for shift.
            if (!drflac__seek_bits(bs, bitsToSeek)) {
                return DR_FALSE;
            }

            if (!drflac__read_and_seek_residual(bs, frame->header.blockSize, pSubframe->lpcOrder)) {
                return DR_FALSE;
            }
        } break;

        default: return DR_FALSE;
    }

    return DR_TRUE;
}


static DRFLAC_INLINE dr_uint8 drflac__get_channel_count_from_channel_assignment(dr_int8 channelAssignment)
{
    assert(channelAssignment <= 10);

    dr_uint8 lookup[] = {1, 2, 3, 4, 5, 6, 7, 8, 2, 2, 2};
    return lookup[channelAssignment];
}

static dr_bool32 drflac__decode_frame(drflac* pFlac)
{
    // This function should be called while the stream is sitting on the first byte after the frame header.
    memset(pFlac->currentFrame.subframes, 0, sizeof(pFlac->currentFrame.subframes));

    int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.header.channelAssignment);
    for (int i = 0; i < channelCount; ++i)
    {
        if (!drflac__decode_subframe(&pFlac->bs, &pFlac->currentFrame, i, pFlac->pDecodedSamples + (pFlac->currentFrame.header.blockSize * i))) {
            return DR_FALSE;
        }
    }

    // At the end of the frame sits the padding and CRC. We don't use these so we can just seek past.
    if (!drflac__seek_bits(&pFlac->bs, (DRFLAC_CACHE_L1_BITS_REMAINING(&pFlac->bs) & 7) + 16)) {
        return DR_FALSE;
    }


    pFlac->currentFrame.samplesRemaining = pFlac->currentFrame.header.blockSize * channelCount;

    return DR_TRUE;
}

static dr_bool32 drflac__seek_frame(drflac* pFlac)
{
    int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.header.channelAssignment);
    for (int i = 0; i < channelCount; ++i)
    {
        if (!drflac__seek_subframe(&pFlac->bs, &pFlac->currentFrame, i)) {
            return DR_FALSE;
        }
    }

    // Padding and CRC.
    return drflac__seek_bits(&pFlac->bs, (DRFLAC_CACHE_L1_BITS_REMAINING(&pFlac->bs) & 7) + 16);
}

static dr_bool32 drflac__read_and_decode_next_frame(drflac* pFlac)
{
    assert(pFlac != NULL);

    if (!drflac__read_next_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFrame.header)) {
        return DR_FALSE;
    }

    return drflac__decode_frame(pFlac);
}


static void drflac__get_current_frame_sample_range(drflac* pFlac, dr_uint64* pFirstSampleInFrameOut, dr_uint64* pLastSampleInFrameOut)
{
    assert(pFlac != NULL);

    unsigned int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.header.channelAssignment);

    dr_uint64 firstSampleInFrame = pFlac->currentFrame.header.sampleNumber;
    if (firstSampleInFrame == 0) {
        firstSampleInFrame = pFlac->currentFrame.header.frameNumber * pFlac->maxBlockSize*channelCount;
    }

    dr_uint64 lastSampleInFrame = firstSampleInFrame + (pFlac->currentFrame.header.blockSize*channelCount);
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

static dr_bool32 drflac__seek_to_first_frame(drflac* pFlac)
{
    assert(pFlac != NULL);

    dr_bool32 result = drflac__seek_to_byte(&pFlac->bs, pFlac->firstFramePos);

    memset(&pFlac->currentFrame, 0, sizeof(pFlac->currentFrame));
    return result;
}

static DRFLAC_INLINE dr_bool32 drflac__seek_to_next_frame(drflac* pFlac)
{
    // This function should only ever be called while the decoder is sitting on the first byte past the FRAME_HEADER section.
    assert(pFlac != NULL);
    return drflac__seek_frame(pFlac);
}

static dr_bool32 drflac__seek_to_frame_containing_sample(drflac* pFlac, dr_uint64 sampleIndex)
{
    assert(pFlac != NULL);

    if (!drflac__seek_to_first_frame(pFlac)) {
        return DR_FALSE;
    }

    dr_uint64 firstSampleInFrame = 0;
    dr_uint64 lastSampleInFrame = 0;
    for (;;)
    {
        // We need to read the frame's header in order to determine the range of samples it contains.
        if (!drflac__read_next_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFrame.header)) {
            return DR_FALSE;
        }

        drflac__get_current_frame_sample_range(pFlac, &firstSampleInFrame, &lastSampleInFrame);
        if (sampleIndex >= firstSampleInFrame && sampleIndex <= lastSampleInFrame) {
            break;  // The sample is in this frame.
        }

        if (!drflac__seek_to_next_frame(pFlac)) {
            return DR_FALSE;
        }
    }

    // If we get here we should be right at the start of the frame containing the sample.
    return DR_TRUE;
}

static dr_bool32 drflac__seek_to_sample__brute_force(drflac* pFlac, dr_uint64 sampleIndex)
{
    if (!drflac__seek_to_frame_containing_sample(pFlac, sampleIndex)) {
        return DR_FALSE;
    }

    // At this point we should be sitting on the first byte of the frame containing the sample. We need to decode every sample up to (but
    // not including) the sample we're seeking to.
    dr_uint64 firstSampleInFrame = 0;
    drflac__get_current_frame_sample_range(pFlac, &firstSampleInFrame, NULL);

    assert(firstSampleInFrame <= sampleIndex);
    size_t samplesToDecode = (size_t)(sampleIndex - firstSampleInFrame);    // <-- Safe cast because the maximum number of samples in a frame is 65535.
    if (samplesToDecode == 0) {
        return DR_TRUE;
    }

    // At this point we are just sitting on the byte after the frame header. We need to decode the frame before reading anything from it.
    if (!drflac__decode_frame(pFlac)) {
        return DR_FALSE;
    }

    return drflac_read_s32(pFlac, samplesToDecode, NULL) != 0;
}


static dr_bool32 drflac__seek_to_sample__seek_table(drflac* pFlac, dr_uint64 sampleIndex)
{
    assert(pFlac != NULL);

    if (pFlac->seektablePos == 0) {
        return DR_FALSE;
    }

    if (!drflac__seek_to_byte(&pFlac->bs, pFlac->seektablePos)) {
        return DR_FALSE;
    }

    // The number of seek points is derived from the size of the SEEKTABLE block.
    dr_uint32 seekpointCount = pFlac->seektableSize / 18;   // 18 = the size of each seek point.
    if (seekpointCount == 0) {
        return DR_FALSE;   // Would this ever happen?
    }


    drflac_seekpoint closestSeekpoint = {0, 0, 0};

    dr_uint32 seekpointsRemaining = seekpointCount;
    while (seekpointsRemaining > 0)
    {
        drflac_seekpoint seekpoint;
        if (!drflac__read_uint64(&pFlac->bs, 64, &seekpoint.firstSample)) {
            break;
        }
        if (!drflac__read_uint64(&pFlac->bs, 64, &seekpoint.frameOffset)) {
            break;
        }
        if (!drflac__read_uint16(&pFlac->bs, 16, &seekpoint.sampleCount)) {
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
    if (!drflac__seek_to_byte(&pFlac->bs, pFlac->firstFramePos + closestSeekpoint.frameOffset)) {
        return DR_FALSE;
    }


    dr_uint64 firstSampleInFrame = 0;
    dr_uint64 lastSampleInFrame = 0;
    for (;;)
    {
        // We need to read the frame's header in order to determine the range of samples it contains.
        if (!drflac__read_next_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFrame.header)) {
            return DR_FALSE;
        }

        drflac__get_current_frame_sample_range(pFlac, &firstSampleInFrame, &lastSampleInFrame);
        if (sampleIndex >= firstSampleInFrame && sampleIndex <= lastSampleInFrame) {
            break;  // The sample is in this frame.
        }

        if (!drflac__seek_to_next_frame(pFlac)) {
            return DR_FALSE;
        }
    }

    assert(firstSampleInFrame <= sampleIndex);

    // At this point we are just sitting on the byte after the frame header. We need to decode the frame before reading anything from it.
    if (!drflac__decode_frame(pFlac)) {
        return DR_FALSE;
    }

    size_t samplesToDecode = (size_t)(sampleIndex - firstSampleInFrame);    // <-- Safe cast because the maximum number of samples in a frame is 65535.
    return drflac_read_s32(pFlac, samplesToDecode, NULL) == samplesToDecode;
}


#ifndef DR_FLAC_NO_OGG
typedef struct
{
    dr_uint8 capturePattern[4];  // Should be "OggS"
    dr_uint8 structureVersion;   // Always 0.
    dr_uint8 headerType;
    dr_uint64 granulePosition;
    dr_uint32 serialNumber;
    dr_uint32 sequenceNumber;
    dr_uint32 checksum;
    dr_uint8 segmentCount;
    dr_uint8 segmentTable[255];
} drflac_ogg_page_header;
#endif

typedef struct
{
    drflac_read_proc onRead;
    drflac_seek_proc onSeek;
    drflac_meta_proc onMeta;
    void* pUserData;
    void* pUserDataMD;
    drflac_container container;
    dr_uint32 sampleRate;
    dr_uint8  channels;
    dr_uint8  bitsPerSample;
    dr_uint64 totalSampleCount;
    dr_uint16 maxBlockSize;
    dr_uint64 runningFilePos;
    dr_bool32 hasMetadataBlocks;

#ifndef DR_FLAC_NO_OGG
    dr_uint32 oggSerial;
    dr_uint64 oggFirstBytePos;
    drflac_ogg_page_header oggBosHeader;
#endif
} drflac_init_info;

static DRFLAC_INLINE void drflac__decode_block_header(dr_uint32 blockHeader, dr_uint8* isLastBlock, dr_uint8* blockType, dr_uint32* blockSize)
{
    blockHeader = drflac__be2host_32(blockHeader);
    *isLastBlock = (blockHeader & (0x01 << 31)) >> 31;
    *blockType   = (blockHeader & (0x7F << 24)) >> 24;
    *blockSize   = (blockHeader & 0xFFFFFF);
}

static DRFLAC_INLINE dr_bool32 drflac__read_and_decode_block_header(drflac_read_proc onRead, void* pUserData, dr_uint8* isLastBlock, dr_uint8* blockType, dr_uint32* blockSize)
{
    dr_uint32 blockHeader;
    if (onRead(pUserData, &blockHeader, 4) != 4) {
        return DR_FALSE;
    }

    drflac__decode_block_header(blockHeader, isLastBlock, blockType, blockSize);
    return DR_TRUE;
}

dr_bool32 drflac__read_streaminfo(drflac_read_proc onRead, void* pUserData, drflac_streaminfo* pStreamInfo)
{
    // min/max block size.
    dr_uint32 blockSizes;
    if (onRead(pUserData, &blockSizes, 4) != 4) {
        return DR_FALSE;
    }

    // min/max frame size.
    dr_uint64 frameSizes = 0;
    if (onRead(pUserData, &frameSizes, 6) != 6) {
        return DR_FALSE;
    }

    // Sample rate, channels, bits per sample and total sample count.
    dr_uint64 importantProps;
    if (onRead(pUserData, &importantProps, 8) != 8) {
        return DR_FALSE;
    }

    // MD5
    dr_uint8 md5[16];
    if (onRead(pUserData, md5, sizeof(md5)) != sizeof(md5)) {
        return DR_FALSE;
    }

    blockSizes     = drflac__be2host_32(blockSizes);
    frameSizes     = drflac__be2host_64(frameSizes);
    importantProps = drflac__be2host_64(importantProps);

    pStreamInfo->minBlockSize     = (blockSizes & 0xFFFF0000) >> 16;
    pStreamInfo->maxBlockSize     = blockSizes & 0x0000FFFF;
    pStreamInfo->minFrameSize     = (dr_uint32)((frameSizes & 0xFFFFFF0000000000ULL) >> 40ULL);
    pStreamInfo->maxFrameSize     = (dr_uint32)((frameSizes & 0x000000FFFFFF0000ULL) >> 16ULL);
    pStreamInfo->sampleRate       = (dr_uint32)((importantProps & 0xFFFFF00000000000ULL) >> 44ULL);
    pStreamInfo->channels         = (dr_uint8 )((importantProps & 0x00000E0000000000ULL) >> 41ULL) + 1;
    pStreamInfo->bitsPerSample    = (dr_uint8 )((importantProps & 0x000001F000000000ULL) >> 36ULL) + 1;
    pStreamInfo->totalSampleCount = (importantProps & 0x0000000FFFFFFFFFULL) * pStreamInfo->channels;
    memcpy(pStreamInfo->md5, md5, sizeof(md5));

    return DR_TRUE;
}

dr_bool32 drflac__read_and_decode_metadata(drflac* pFlac)
{
    assert(pFlac != NULL);

    // We want to keep track of the byte position in the stream of the seektable. At the time of calling this function we know that
    // we'll be sitting on byte 42.
    dr_uint64 runningFilePos = 42;
    dr_uint64 seektablePos  = 0;
    dr_uint32 seektableSize = 0;

    for (;;)
    {
        dr_uint8 isLastBlock = 0;
        dr_uint8 blockType;
        dr_uint32 blockSize;
        if (!drflac__read_and_decode_block_header(pFlac->bs.onRead, pFlac->bs.pUserData, &isLastBlock, &blockType, &blockSize)) {
            return DR_FALSE;
        }
        runningFilePos += 4;


        drflac_metadata metadata;
        metadata.type = blockType;
        metadata.pRawData = NULL;
        metadata.rawDataSize = 0;

        switch (blockType)
        {
            case DRFLAC_METADATA_BLOCK_TYPE_APPLICATION:
            {
                if (pFlac->onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return DR_FALSE;
                    }

                    if (pFlac->bs.onRead(pFlac->bs.pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return DR_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;
                    metadata.data.application.id       = drflac__be2host_32(*(dr_uint32*)pRawData);
                    metadata.data.application.pData    = (const void*)((dr_uint8*)pRawData + sizeof(dr_uint32));
                    metadata.data.application.dataSize = blockSize - sizeof(dr_uint32);
                    pFlac->onMeta(pFlac->pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_SEEKTABLE:
            {
                seektablePos  = runningFilePos;
                seektableSize = blockSize;

                if (pFlac->onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return DR_FALSE;
                    }

                    if (pFlac->bs.onRead(pFlac->bs.pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return DR_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;
                    metadata.data.seektable.seekpointCount = blockSize/sizeof(drflac_seekpoint);
                    metadata.data.seektable.pSeekpoints = (const drflac_seekpoint*)pRawData;

                    // Endian swap.
                    for (dr_uint32 iSeekpoint = 0; iSeekpoint < metadata.data.seektable.seekpointCount; ++iSeekpoint) {
                        drflac_seekpoint* pSeekpoint = (drflac_seekpoint*)pRawData + iSeekpoint;
                        pSeekpoint->firstSample = drflac__be2host_64(pSeekpoint->firstSample);
                        pSeekpoint->frameOffset = drflac__be2host_64(pSeekpoint->frameOffset);
                        pSeekpoint->sampleCount = drflac__be2host_16(pSeekpoint->sampleCount);
                    }

                    pFlac->onMeta(pFlac->pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT:
            {
                if (pFlac->onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return DR_FALSE;
                    }

                    if (pFlac->bs.onRead(pFlac->bs.pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return DR_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;

                    const char* pRunningData = (const char*)pRawData;
                    metadata.data.vorbis_comment.vendorLength = drflac__le2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.vorbis_comment.vendor       = pRunningData;                                 pRunningData += metadata.data.vorbis_comment.vendorLength;
                    metadata.data.vorbis_comment.commentCount = drflac__le2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.vorbis_comment.comments     = pRunningData;
                    pFlac->onMeta(pFlac->pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_CUESHEET:
            {
                if (pFlac->onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return DR_FALSE;
                    }

                    if (pFlac->bs.onRead(pFlac->bs.pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return DR_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;

                    const char* pRunningData = (const char*)pRawData;
                    memcpy(metadata.data.cuesheet.catalog, pRunningData, 128);                               pRunningData += 128;
                    metadata.data.cuesheet.leadInSampleCount = drflac__be2host_64(*(dr_uint64*)pRunningData); pRunningData += 4;
                    metadata.data.cuesheet.isCD              = ((pRunningData[0] & 0x80) >> 7) != 0;         pRunningData += 259;
                    metadata.data.cuesheet.trackCount        = pRunningData[0];                              pRunningData += 1;
                    metadata.data.cuesheet.pTrackData        = (const dr_uint8*)pRunningData;
                    pFlac->onMeta(pFlac->pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_PICTURE:
            {
                if (pFlac->onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return DR_FALSE;
                    }

                    if (pFlac->bs.onRead(pFlac->bs.pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return DR_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;

                    const char* pRunningData = (const char*)pRawData;
                    metadata.data.picture.type              = drflac__be2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.mimeLength        = drflac__be2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.mime              = pRunningData;                                 pRunningData += metadata.data.picture.mimeLength;
                    metadata.data.picture.descriptionLength = drflac__be2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.description       = pRunningData;
                    metadata.data.picture.width             = drflac__be2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.height            = drflac__be2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.colorDepth        = drflac__be2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.indexColorCount   = drflac__be2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.pictureDataSize   = drflac__be2host_32(*(dr_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.pPictureData      = (const dr_uint8*)pRunningData;
                    pFlac->onMeta(pFlac->pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_PADDING:
            {
                if (pFlac->onMeta) {
                    metadata.data.padding.unused = 0;

                    // Padding doesn't have anything meaningful in it, so just skip over it.
                    if (!pFlac->bs.onSeek(pFlac->bs.pUserData, blockSize, drflac_seek_origin_current)) {
                        return DR_FALSE;
                    }

                    pFlac->onMeta(pFlac->pUserDataMD, &metadata);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_INVALID:
            {
                // Invalid chunk. Just skip over this one.
                if (pFlac->onMeta) {
                    if (!pFlac->bs.onSeek(pFlac->bs.pUserData, blockSize, drflac_seek_origin_current)) {
                        return DR_FALSE;
                    }
                }
            }

            default:
            {
                // It's an unknown chunk, but not necessarily invalid. There's a chance more metadata blocks might be defined later on, so we
                // can at the very least report the chunk to the application and let it look at the raw data.
                if (pFlac->onMeta) {
                    void* pRawData = malloc(blockSize);
                    if (pRawData == NULL) {
                        return DR_FALSE;
                    }

                    if (pFlac->bs.onRead(pFlac->bs.pUserData, pRawData, blockSize) != blockSize) {
                        free(pRawData);
                        return DR_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;
                    pFlac->onMeta(pFlac->pUserDataMD, &metadata);

                    free(pRawData);
                }
            } break;
        }

        // If we're not handling metadata, just skip over the block. If we are, it will have been handled earlier in the switch statement above.
        if (pFlac->onMeta == NULL) {
            if (!pFlac->bs.onSeek(pFlac->bs.pUserData, blockSize, drflac_seek_origin_current)) {
                return DR_FALSE;
            }
        }

        runningFilePos += blockSize;
        if (isLastBlock) {
            break;
        }
    }

    pFlac->seektablePos = seektablePos;
    pFlac->seektableSize = seektableSize;
    pFlac->firstFramePos = runningFilePos;

    return DR_TRUE;
}

dr_bool32 drflac__init_private__native(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD)
{
    (void)onSeek;

    // Pre: The bit stream should be sitting just past the 4-byte id header.

    pInit->container = drflac_container_native;

    // The first metadata block should be the STREAMINFO block.
    dr_uint8 isLastBlock;
    dr_uint8 blockType;
    dr_uint32 blockSize;
    if (!drflac__read_and_decode_block_header(onRead, pUserData, &isLastBlock, &blockType, &blockSize)) {
        return DR_FALSE;
    }

    if (blockType != DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO || blockSize != 34) {
        return DR_FALSE;    // Invalid block type. First block must be the STREAMINFO block.
    }


    drflac_streaminfo streaminfo;
    if (!drflac__read_streaminfo(onRead, pUserData, &streaminfo)) {
        return DR_FALSE;
    }

    pInit->sampleRate       = streaminfo.sampleRate;
    pInit->channels         = streaminfo.channels;
    pInit->bitsPerSample    = streaminfo.bitsPerSample;
    pInit->totalSampleCount = streaminfo.totalSampleCount;
    pInit->maxBlockSize     = streaminfo.maxBlockSize;    // Don't care about the min block size - only the max (used for determining the size of the memory allocation).

    if (onMeta) {
        drflac_metadata metadata;
        metadata.type = DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO;
        metadata.pRawData = NULL;
        metadata.rawDataSize = 0;
        metadata.data.streaminfo = streaminfo;
        onMeta(pUserDataMD, &metadata);
    }

    pInit->hasMetadataBlocks = !isLastBlock;
    return DR_TRUE;
}

#ifndef DR_FLAC_NO_OGG
static DRFLAC_INLINE dr_bool32 drflac_ogg__is_capture_pattern(dr_uint8 pattern[4])
{
    return pattern[0] == 'O' && pattern[1] == 'g' && pattern[2] == 'g' && pattern[3] == 'S';
}

static DRFLAC_INLINE dr_uint32 drflac_ogg__get_page_header_size(drflac_ogg_page_header* pHeader)
{
    return 27 + pHeader->segmentCount;
}

static DRFLAC_INLINE dr_uint32 drflac_ogg__get_page_body_size(drflac_ogg_page_header* pHeader)
{
    dr_uint32 pageBodySize = 0;
    for (int i = 0; i < pHeader->segmentCount; ++i) {
        pageBodySize += pHeader->segmentTable[i];
    }

    return pageBodySize;
}

dr_bool32 drflac_ogg__read_page_header_after_capture_pattern(drflac_read_proc onRead, void* pUserData, drflac_ogg_page_header* pHeader, dr_uint32* pHeaderSize)
{
    if (onRead(pUserData, &pHeader->structureVersion, 1) != 1 || pHeader->structureVersion != 0) {
        return DR_FALSE;   // Unknown structure version. Possibly corrupt stream.
    }
    if (onRead(pUserData, &pHeader->headerType, 1) != 1) {
        return DR_FALSE;
    }
    if (onRead(pUserData, &pHeader->granulePosition, 8) != 8) {
        return DR_FALSE;
    }
    if (onRead(pUserData, &pHeader->serialNumber, 4) != 4) {
        return DR_FALSE;
    }
    if (onRead(pUserData, &pHeader->sequenceNumber, 4) != 4) {
        return DR_FALSE;
    }
    if (onRead(pUserData, &pHeader->checksum, 4) != 4) {
        return DR_FALSE;
    }
    if (onRead(pUserData, &pHeader->segmentCount, 1) != 1 || pHeader->segmentCount == 0) {
        return DR_FALSE;   // Should not have a segment count of 0.
    }
    if (onRead(pUserData, &pHeader->segmentTable, pHeader->segmentCount) != pHeader->segmentCount) {
        return DR_FALSE;
    }

    if (pHeaderSize) *pHeaderSize = (27 + pHeader->segmentCount);
    return DR_TRUE;
}

dr_bool32 drflac_ogg__read_page_header(drflac_read_proc onRead, void* pUserData, drflac_ogg_page_header* pHeader, dr_uint32* pHeaderSize)
{
    dr_uint8 id[4];
    if (onRead(pUserData, id, 4) != 4) {
        return DR_FALSE;
    }

    if (id[0] != 'O' || id[1] != 'g' || id[2] != 'g' || id[3] != 'S') {
        return DR_FALSE;
    }

    return drflac_ogg__read_page_header_after_capture_pattern(onRead, pUserData, pHeader, pHeaderSize);
}


// The main part of the Ogg encapsulation is the conversion from the physical Ogg bitstream to the native FLAC bitstream. It works
// in three general stages: Ogg Physical Bitstream -> Ogg/FLAC Logical Bitstream -> FLAC Native Bitstream. dr_flac is architecured
// in such a way that the core sections assume everything is delivered in native format. Therefore, for each encapsulation type
// dr_flac is supporting there needs to be a layer sitting on top of the onRead and onSeek callbacks that ensures the bits read from
// the physical Ogg bitstream are converted and delivered in native FLAC format.
typedef struct
{
    drflac_read_proc onRead;    // The original onRead callback from drflac_open() and family.
    drflac_seek_proc onSeek;    // The original onSeek callback from drflac_open() and family.
    void* pUserData;            // The user data passed on onRead and onSeek. This is the user data that was passed on drflac_open() and family.
    dr_uint64 currentBytePos;    // The position of the byte we are sitting on in the physical byte stream. Used for efficient seeking.
    dr_uint64 firstBytePos;      // The position of the first byte in the physical bitstream. Points to the start of the "OggS" identifier of the FLAC bos page.
    dr_uint32 serialNumber;      // The serial number of the FLAC audio pages. This is determined by the initial header page that was read during initialization.
    drflac_ogg_page_header bosPageHeader;   // Used for seeking.
    drflac_ogg_page_header currentPageHeader;
    dr_uint32 bytesRemainingInPage;
} drflac_oggbs; // oggbs = Ogg Bitstream

static size_t drflac_oggbs__read_physical(drflac_oggbs* oggbs, void* bufferOut, size_t bytesToRead)
{
    size_t bytesActuallyRead = oggbs->onRead(oggbs->pUserData, bufferOut, bytesToRead);
    oggbs->currentBytePos += bytesActuallyRead;

    return bytesActuallyRead;
}

static dr_bool32 drflac_oggbs__seek_physical(drflac_oggbs* oggbs, dr_uint64 offset, drflac_seek_origin origin)
{
    if (origin == drflac_seek_origin_start)
    {
        if (offset <= 0x7FFFFFFF) {
            if (!oggbs->onSeek(oggbs->pUserData, (int)offset, drflac_seek_origin_start)) {
                return DR_FALSE;
            }
            oggbs->currentBytePos = offset;

            return DR_TRUE;
        } else {
            if (!oggbs->onSeek(oggbs->pUserData, 0x7FFFFFFF, drflac_seek_origin_start)) {
                return DR_FALSE;
            }
            oggbs->currentBytePos = offset;

            return drflac_oggbs__seek_physical(oggbs, offset - 0x7FFFFFFF, drflac_seek_origin_current);
        }
    }
    else
    {
        while (offset > 0x7FFFFFFF) {
            if (!oggbs->onSeek(oggbs->pUserData, 0x7FFFFFFF, drflac_seek_origin_current)) {
                return DR_FALSE;
            }
            oggbs->currentBytePos += 0x7FFFFFFF;
            offset -= 0x7FFFFFFF;
        }

        if (!oggbs->onSeek(oggbs->pUserData, (int)offset, drflac_seek_origin_current)) {    // <-- Safe cast thanks to the loop above.
            return DR_FALSE;
        }
        oggbs->currentBytePos += offset;

        return DR_TRUE;
    }
}

static dr_bool32 drflac_oggbs__goto_next_page(drflac_oggbs* oggbs)
{
    drflac_ogg_page_header header;
    for (;;)
    {
        dr_uint32 headerSize;
        if (!drflac_ogg__read_page_header(oggbs->onRead, oggbs->pUserData, &header, &headerSize)) {
            return DR_FALSE;
        }
        oggbs->currentBytePos += headerSize;


        dr_uint32 pageBodySize = drflac_ogg__get_page_body_size(&header);

        if (header.serialNumber == oggbs->serialNumber) {
            oggbs->currentPageHeader = header;
            oggbs->bytesRemainingInPage = pageBodySize;
            return DR_TRUE;
        }

        // If we get here it means the page is not a FLAC page - skip it.
        if (pageBodySize > 0 && !drflac_oggbs__seek_physical(oggbs, pageBodySize, drflac_seek_origin_current)) {    // <-- Safe cast - maximum size of a page is way below that of an int.
            return DR_FALSE;
        }
    }
}

// Function below is unused at the moment, but I might be re-adding it later.
#if 0
static dr_uint8 drflac_oggbs__get_current_segment_index(drflac_oggbs* oggbs, dr_uint8* pBytesRemainingInSeg)
{
    dr_uint32 bytesConsumedInPage = drflac_ogg__get_page_body_size(&oggbs->currentPageHeader) - oggbs->bytesRemainingInPage;
    dr_uint8 iSeg = 0;
    dr_uint32 iByte = 0;
    while (iByte < bytesConsumedInPage)
    {
        dr_uint8 segmentSize = oggbs->currentPageHeader.segmentTable[iSeg];
        if (iByte + segmentSize > bytesConsumedInPage) {
            break;
        } else {
            iSeg += 1;
            iByte += segmentSize;
        }
    }

    *pBytesRemainingInSeg = oggbs->currentPageHeader.segmentTable[iSeg] - (dr_uint8)(bytesConsumedInPage - iByte);
    return iSeg;
}

static dr_bool32 drflac_oggbs__seek_to_next_packet(drflac_oggbs* oggbs)
{
    // The current packet ends when we get to the segment with a lacing value of < 255 which is not at the end of a page.
    for (;;)    // <-- Loop over pages.
    {
        dr_bool32 atEndOfPage = DR_FALSE;

        dr_uint8 bytesRemainingInSeg;
        dr_uint8 iFirstSeg = drflac_oggbs__get_current_segment_index(oggbs, &bytesRemainingInSeg);

        dr_uint32 bytesToEndOfPacketOrPage = bytesRemainingInSeg;
        for (dr_uint8 iSeg = iFirstSeg; iSeg < oggbs->currentPageHeader.segmentCount; ++iSeg) {
            dr_uint8 segmentSize = oggbs->currentPageHeader.segmentTable[iSeg];
            if (segmentSize < 255) {
                if (iSeg == oggbs->currentPageHeader.segmentCount-1) {
                    atEndOfPage = DR_TRUE;
                }

                break;
            }

            bytesToEndOfPacketOrPage += segmentSize;
        }

        // At this point we will have found either the packet or the end of the page. If were at the end of the page we'll
        // want to load the next page and keep searching for the end of the frame.
        drflac_oggbs__seek_physical(oggbs, bytesToEndOfPacketOrPage, drflac_seek_origin_current);
        oggbs->bytesRemainingInPage -= bytesToEndOfPacketOrPage;

        if (atEndOfPage)
        {
            // We're potentially at the next packet, but we need to check the next page first to be sure because the packet may
            // straddle pages.
            if (!drflac_oggbs__goto_next_page(oggbs)) {
                return DR_FALSE;
            }

            // If it's a fresh packet it most likely means we're at the next packet.
            if ((oggbs->currentPageHeader.headerType & 0x01) == 0) {
                return DR_TRUE;
            }
        }
        else
        {
            // We're at the next frame.
            return DR_TRUE;
        }
    }
}

static dr_bool32 drflac_oggbs__seek_to_next_frame(drflac_oggbs* oggbs)
{
    // The bitstream should be sitting on the first byte just after the header of the frame.

    // What we're actually doing here is seeking to the start of the next packet.
    return drflac_oggbs__seek_to_next_packet(oggbs);
}
#endif

static size_t drflac__on_read_ogg(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    drflac_oggbs* oggbs = (drflac_oggbs*)pUserData;
    assert(oggbs != NULL);

    dr_uint8* pRunningBufferOut = (dr_uint8*)bufferOut;

    // Reading is done page-by-page. If we've run out of bytes in the page we need to move to the next one.
    size_t bytesRead = 0;
    while (bytesRead < bytesToRead)
    {
        size_t bytesRemainingToRead = bytesToRead - bytesRead;

        if (oggbs->bytesRemainingInPage >= bytesRemainingToRead) {
            bytesRead += oggbs->onRead(oggbs->pUserData, pRunningBufferOut, bytesRemainingToRead);
            oggbs->bytesRemainingInPage -= (dr_uint32)bytesRemainingToRead;
            break;
        }

        // If we get here it means some of the requested data is contained in the next pages.
        if (oggbs->bytesRemainingInPage > 0) {
            size_t bytesJustRead = oggbs->onRead(oggbs->pUserData, pRunningBufferOut, oggbs->bytesRemainingInPage);
            bytesRead += bytesJustRead;
            pRunningBufferOut += bytesJustRead;

            if (bytesJustRead != oggbs->bytesRemainingInPage) {
                break;  // Ran out of data.
            }
        }

        assert(bytesRemainingToRead > 0);
        if (!drflac_oggbs__goto_next_page(oggbs)) {
            break;  // Failed to go to the next chunk. Might have simply hit the end of the stream.
        }
    }

    oggbs->currentBytePos += bytesRead;
    return bytesRead;
}

static dr_bool32 drflac__on_seek_ogg(void* pUserData, int offset, drflac_seek_origin origin)
{
    drflac_oggbs* oggbs = (drflac_oggbs*)pUserData;
    assert(oggbs != NULL);
    assert(offset > 0 || (offset == 0 && origin == drflac_seek_origin_start));

    // Seeking is always forward which makes things a lot simpler.
    if (origin == drflac_seek_origin_start) {
        int startBytePos = (int)oggbs->firstBytePos + (79-42);  // 79 = size of bos page; 42 = size of FLAC header data. Seek up to the first byte of the native FLAC data.
        if (!drflac_oggbs__seek_physical(oggbs, startBytePos, drflac_seek_origin_start)) {
            return DR_FALSE;
        }

        oggbs->currentPageHeader = oggbs->bosPageHeader;
        oggbs->bytesRemainingInPage = 42;   // 42 = size of the native FLAC header data. That's our start point for seeking.

        return drflac__on_seek_ogg(pUserData, offset, drflac_seek_origin_current);
    }


    assert(origin == drflac_seek_origin_current);

    int bytesSeeked = 0;
    while (bytesSeeked < offset)
    {
        int bytesRemainingToSeek = offset - bytesSeeked;
        assert(bytesRemainingToSeek >= 0);

        if (oggbs->bytesRemainingInPage >= (size_t)bytesRemainingToSeek) {
            if (!drflac_oggbs__seek_physical(oggbs, bytesRemainingToSeek, drflac_seek_origin_current)) {
                return DR_FALSE;
            }

            bytesSeeked += bytesRemainingToSeek;
            oggbs->bytesRemainingInPage -= bytesRemainingToSeek;
            break;
        }

        // If we get here it means some of the requested data is contained in the next pages.
        if (oggbs->bytesRemainingInPage > 0) {
            if (!drflac_oggbs__seek_physical(oggbs, oggbs->bytesRemainingInPage, drflac_seek_origin_current)) {
                return DR_FALSE;
            }

            bytesSeeked += (int)oggbs->bytesRemainingInPage;
        }

        assert(bytesRemainingToSeek > 0);
        if (!drflac_oggbs__goto_next_page(oggbs)) {
            break;  // Failed to go to the next chunk. Might have simply hit the end of the stream.
        }
    }

    return DR_TRUE;
}

dr_bool32 drflac_ogg__seek_to_sample(drflac* pFlac, dr_uint64 sample)
{
    drflac_oggbs* oggbs = (drflac_oggbs*)(((dr_int32*)pFlac->pExtraData) + pFlac->maxBlockSize*pFlac->channels);

    dr_uint64 originalBytePos = oggbs->currentBytePos;   // For recovery.

    // First seek to the first frame.
    if (!drflac__seek_to_byte(&pFlac->bs, pFlac->firstFramePos)) {
        return DR_FALSE;
    }
    oggbs->bytesRemainingInPage = 0;

    dr_uint64 runningGranulePosition = 0;
    dr_uint64 runningFrameBytePos = oggbs->currentBytePos;   // <-- Points to the OggS identifier.
    for (;;)
    {
        if (!drflac_oggbs__goto_next_page(oggbs)) {
            drflac_oggbs__seek_physical(oggbs, originalBytePos, drflac_seek_origin_start);
            return DR_FALSE;   // Never did find that sample...
        }

        runningFrameBytePos = oggbs->currentBytePos - drflac_ogg__get_page_header_size(&oggbs->currentPageHeader);
        if (oggbs->currentPageHeader.granulePosition*pFlac->channels >= sample) {
            break; // The sample is somewhere in the previous page.
        }


        // At this point we know the sample is not in the previous page. It could possibly be in this page. For simplicity we
        // disregard any pages that do not begin a fresh packet.
        if ((oggbs->currentPageHeader.headerType & 0x01) == 0) {    // <-- Is it a fresh page?
            if (oggbs->currentPageHeader.segmentTable[0] >= 2) {
                dr_uint8 firstBytesInPage[2];
                if (drflac_oggbs__read_physical(oggbs, firstBytesInPage, 2) != 2) {
                    drflac_oggbs__seek_physical(oggbs, originalBytePos, drflac_seek_origin_start);
                    return DR_FALSE;
                }
                if ((firstBytesInPage[0] == 0xFF) && (firstBytesInPage[1] & 0xFC) == 0xF8) {    // <-- Does the page begin with a frame's sync code?
                    runningGranulePosition = oggbs->currentPageHeader.granulePosition*pFlac->channels;
                }

                if (!drflac_oggbs__seek_physical(oggbs, (int)oggbs->bytesRemainingInPage-2, drflac_seek_origin_current)) {
                    drflac_oggbs__seek_physical(oggbs, originalBytePos, drflac_seek_origin_start);
                    return DR_FALSE;
                }

                continue;
            }
        }

        if (!drflac_oggbs__seek_physical(oggbs, (int)oggbs->bytesRemainingInPage, drflac_seek_origin_current)) {
            drflac_oggbs__seek_physical(oggbs, originalBytePos, drflac_seek_origin_start);
            return DR_FALSE;
        }
    }


    // We found the page that that is closest to the sample, so now we need to find it. The first thing to do is seek to the
    // start of that page. In the loop above we checked that it was a fresh page which means this page is also the start of
    // a new frame. This property means that after we've seeked to the page we can immediately start looping over frames until
    // we find the one containing the target sample.
    if (!drflac_oggbs__seek_physical(oggbs, runningFrameBytePos, drflac_seek_origin_start)) {
        return DR_FALSE;
    }
    if (!drflac_oggbs__goto_next_page(oggbs)) {
        return DR_FALSE;
    }


    // At this point we'll be sitting on the first byte of the frame header of the first frame in the page. We just keep
    // looping over these frames until we find the one containing the sample we're after.
    dr_uint64 firstSampleInFrame = runningGranulePosition;
    for (;;)
    {
        // NOTE for later: When using Ogg's page/segment based seeking later on we can't use this function (or any drflac__*
        // reading functions) because otherwise it will pull extra data for use in it's own internal caches which will then
        // break the positioning of the read pointer for the Ogg bitstream.
        if (!drflac__read_next_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFrame.header)) {
            return DR_FALSE;
        }

        int channels = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.header.channelAssignment);
        dr_uint64 lastSampleInFrame = firstSampleInFrame + (pFlac->currentFrame.header.blockSize*channels);
        lastSampleInFrame -= 1; // <-- Zero based.

        if (sample >= firstSampleInFrame && sample <= lastSampleInFrame) {
            break;  // The sample is in this frame.
        }


        // If we get here it means the sample is not in this frame so we need to move to the next one. Now the cool thing
        // with Ogg is that we can efficiently seek past the frame by looking at the lacing values of each segment in
        // the page.
        firstSampleInFrame = lastSampleInFrame+1;

#if 1
        // Slow way. This uses the native FLAC decoder to seek past the frame. This is slow because it needs to do a partial
        // decode of the frame. Although this is how the native version works, we can use Ogg's framing system to make it
        // more efficient. Leaving this here for reference and to use as a basis for debugging purposes.
        if (!drflac__seek_to_next_frame(pFlac)) {
            return DR_FALSE;
        }
#else
        // TODO: This is not yet complete. See note at the top of this loop body.

        // Fast(er) way. This uses Ogg's framing system to seek past the frame. This should be much more efficient than the
        // native FLAC seeking.
        if (!drflac_oggbs__seek_to_next_frame(oggbs)) {
            return DR_FALSE;
        }
#endif
    }

    assert(firstSampleInFrame <= sample);

    if (!drflac__decode_frame(pFlac)) {
        return DR_FALSE;
    }

    size_t samplesToDecode = (size_t)(sample - firstSampleInFrame);    // <-- Safe cast because the maximum number of samples in a frame is 65535.
    return drflac_read_s32(pFlac, samplesToDecode, NULL) == samplesToDecode;
}


dr_bool32 drflac__init_private__ogg(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD)
{
    // Pre: The bit stream should be sitting just past the 4-byte OggS capture pattern.

    pInit->container = drflac_container_ogg;
    pInit->oggFirstBytePos = 0;

    // We'll get here if the first 4 bytes of the stream were the OggS capture pattern, however it doesn't necessarily mean the
    // stream includes FLAC encoded audio. To check for this we need to scan the beginning-of-stream page markers and check if
    // any match the FLAC specification. Important to keep in mind that the stream may be multiplexed.
    drflac_ogg_page_header header;

    dr_uint32 headerSize;
    if (!drflac_ogg__read_page_header_after_capture_pattern(onRead, pUserData, &header, &headerSize)) {
        return DR_FALSE;
    }
    pInit->runningFilePos = headerSize;

    for (;;)
    {
        // Break if we're past the beginning of stream page.
        if ((header.headerType & 0x02) == 0) {
            return DR_FALSE;
        }


        // Check if it's a FLAC header.
        int pageBodySize = drflac_ogg__get_page_body_size(&header);
        if (pageBodySize == 51)   // 51 = the lacing value of the FLAC header packet.
        {
            // It could be a FLAC page...
            dr_uint32 bytesRemainingInPage = pageBodySize;

            dr_uint8 packetType;
            if (onRead(pUserData, &packetType, 1) != 1) {
                return DR_FALSE;
            }

            bytesRemainingInPage -= 1;
            if (packetType == 0x7F)
            {
                // Increasingly more likely to be a FLAC page...
                dr_uint8 sig[4];
                if (onRead(pUserData, sig, 4) != 4) {
                    return DR_FALSE;
                }

                bytesRemainingInPage -= 4;
                if (sig[0] == 'F' && sig[1] == 'L' && sig[2] == 'A' && sig[3] == 'C')
                {
                    // Almost certainly a FLAC page...
                    dr_uint8 mappingVersion[2];
                    if (onRead(pUserData, mappingVersion, 2) != 2) {
                        return DR_FALSE;
                    }

                    if (mappingVersion[0] != 1) {
                        return DR_FALSE;   // Only supporting version 1.x of the Ogg mapping.
                    }

                    // The next 2 bytes are the non-audio packets, not including this one. We don't care about this because we're going to
                    // be handling it in a generic way based on the serial number and packet types.
                    if (!onSeek(pUserData, 2, drflac_seek_origin_current)) {
                        return DR_FALSE;
                    }

                    // Expecting the native FLAC signature "fLaC".
                    if (onRead(pUserData, sig, 4) != 4) {
                        return DR_FALSE;
                    }

                    if (sig[0] == 'f' && sig[1] == 'L' && sig[2] == 'a' && sig[3] == 'C')
                    {
                        // The remaining data in the page should be the STREAMINFO block.
                        dr_uint8 isLastBlock;
                        dr_uint8 blockType;
                        dr_uint32 blockSize;
                        if (!drflac__read_and_decode_block_header(onRead, pUserData, &isLastBlock, &blockType, &blockSize)) {
                            return DR_FALSE;
                        }

                        if (blockType != DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO || blockSize != 34) {
                            return DR_FALSE;    // Invalid block type. First block must be the STREAMINFO block.
                        }

                        drflac_streaminfo streaminfo;
                        if (drflac__read_streaminfo(onRead, pUserData, &streaminfo))
                        {
                            // Success!
                            pInit->sampleRate       = streaminfo.sampleRate;
                            pInit->channels         = streaminfo.channels;
                            pInit->bitsPerSample    = streaminfo.bitsPerSample;
                            pInit->totalSampleCount = streaminfo.totalSampleCount;
                            pInit->maxBlockSize     = streaminfo.maxBlockSize;

                            if (onMeta) {
                                drflac_metadata metadata;
                                metadata.type = DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO;
                                metadata.pRawData = NULL;
                                metadata.rawDataSize = 0;
                                metadata.data.streaminfo = streaminfo;
                                onMeta(pUserDataMD, &metadata);
                            }

                            pInit->runningFilePos  += pageBodySize;
                            pInit->oggFirstBytePos  = pInit->runningFilePos - 79;   // Subtracting 79 will place us right on top of the "OggS" identifier of the FLAC bos page.
                            pInit->oggSerial        = header.serialNumber;
                            pInit->oggBosHeader     = header;
                            break;
                        }
                        else
                        {
                            // Failed to read STREAMINFO block. Aww, so close...
                            return DR_FALSE;
                        }
                    }
                    else
                    {
                        // Invalid file.
                        return DR_FALSE;
                    }
                }
                else
                {
                    // Not a FLAC header. Skip it.
                    if (!onSeek(pUserData, bytesRemainingInPage, drflac_seek_origin_current)) {
                        return DR_FALSE;
                    }
                }
            }
            else
            {
                // Not a FLAC header. Seek past the entire page and move on to the next.
                if (!onSeek(pUserData, bytesRemainingInPage, drflac_seek_origin_current)) {
                    return DR_FALSE;
                }
            }
        }
        else
        {
            if (!onSeek(pUserData, pageBodySize, drflac_seek_origin_current)) {
                return DR_FALSE;
            }
        }

        pInit->runningFilePos += pageBodySize;


        // Read the header of the next page.
        if (!drflac_ogg__read_page_header(onRead, pUserData, &header, &headerSize)) {
            return DR_FALSE;
        }
        pInit->runningFilePos += headerSize;
    }


    // If we get here it means we found a FLAC audio stream. We should be sitting on the first byte of the header of the next page. The next
    // packets in the FLAC logical stream contain the metadata. The only thing left to do in the initialiation phase for Ogg is to create the
    // Ogg bistream object.
    pInit->hasMetadataBlocks = DR_TRUE;    // <-- Always have at least VORBIS_COMMENT metadata block.
    return DR_TRUE;
}
#endif

dr_bool32 drflac__init_private(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD)
{
    if (pInit == NULL || onRead == NULL || onSeek == NULL) {
        return DR_FALSE;
    }

    pInit->onRead        = onRead;
    pInit->onSeek        = onSeek;
    pInit->onMeta        = onMeta;
    pInit->pUserData     = pUserData;
    pInit->pUserDataMD   = pUserDataMD;

    dr_uint8 id[4];
    if (onRead(pUserData, id, 4) != 4) {
        return DR_FALSE;
    }

    if (id[0] == 'f' && id[1] == 'L' && id[2] == 'a' && id[3] == 'C') {
        return drflac__init_private__native(pInit, onRead, onSeek, onMeta, pUserData, pUserDataMD);
    }

#ifndef DR_FLAC_NO_OGG
    if (id[0] == 'O' && id[1] == 'g' && id[2] == 'g' && id[3] == 'S') {
        return drflac__init_private__ogg(pInit, onRead, onSeek, onMeta, pUserData, pUserDataMD);
    }
#endif

    // Unsupported container.
    return DR_FALSE;
}

void drflac__init_from_info(drflac* pFlac, drflac_init_info* pInit)
{
    assert(pFlac != NULL);
    assert(pInit != NULL);

    memset(pFlac, 0, sizeof(*pFlac));
    pFlac->bs.onRead        = pInit->onRead;
    pFlac->bs.onSeek        = pInit->onSeek;
    pFlac->bs.pUserData     = pInit->pUserData;
    pFlac->bs.nextL2Line    = sizeof(pFlac->bs.cacheL2) / sizeof(pFlac->bs.cacheL2[0]); // <-- Initialize to this to force a client-side data retrieval right from the start.
    pFlac->bs.consumedBits  = sizeof(pFlac->bs.cache)*8;

    pFlac->onMeta           = pInit->onMeta;
    pFlac->pUserDataMD      = pInit->pUserDataMD;
    pFlac->maxBlockSize     = pInit->maxBlockSize;
    pFlac->sampleRate       = pInit->sampleRate;
    pFlac->channels         = (dr_uint8)pInit->channels;
    pFlac->bitsPerSample    = (dr_uint8)pInit->bitsPerSample;
    pFlac->totalSampleCount = pInit->totalSampleCount;
    pFlac->container        = pInit->container;
}

drflac* drflac_open_with_metadata_private(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD)
{
    drflac_init_info init;
    if (!drflac__init_private(&init, onRead, onSeek, onMeta, pUserData, pUserDataMD)) {
        return NULL;
    }

    size_t allocationSize = sizeof(drflac);
    allocationSize += init.maxBlockSize * init.channels * sizeof(dr_int32);
    //allocationSize += init.seektableSize;


#ifndef DR_FLAC_NO_OGG
    // There's additional data required for Ogg streams.
    if (init.container == drflac_container_ogg) {
        allocationSize += sizeof(drflac_oggbs);
    }
#endif

    drflac* pFlac = (drflac*)malloc(allocationSize);
    drflac__init_from_info(pFlac, &init);
    pFlac->pDecodedSamples = (dr_int32*)pFlac->pExtraData;

#ifndef DR_FLAC_NO_OGG
    if (init.container == drflac_container_ogg) {
        drflac_oggbs* oggbs = (drflac_oggbs*)(((dr_int32*)pFlac->pExtraData) + init.maxBlockSize*init.channels);
        oggbs->onRead = onRead;
        oggbs->onSeek = onSeek;
        oggbs->pUserData = pUserData;
        oggbs->currentBytePos = init.oggFirstBytePos;
        oggbs->firstBytePos = init.oggFirstBytePos;
        oggbs->serialNumber = init.oggSerial;
        oggbs->bosPageHeader = init.oggBosHeader;
        oggbs->bytesRemainingInPage = 0;

        // The Ogg bistream needs to be layered on top of the original bitstream.
        pFlac->bs.onRead = drflac__on_read_ogg;
        pFlac->bs.onSeek = drflac__on_seek_ogg;
        pFlac->bs.pUserData = (void*)oggbs;
    }
#endif

    // Decode metadata before returning.
    if (init.hasMetadataBlocks) {
        if (!drflac__read_and_decode_metadata(pFlac)) {
            free(pFlac);
            return NULL;
        }
    }

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

static dr_bool32 drflac__on_seek_stdio(void* pUserData, int offset, drflac_seek_origin origin)
{
    assert(offset > 0 || (offset == 0 && origin == drflac_seek_origin_start));

    return fseek((FILE*)pUserData, offset, (origin == drflac_seek_origin_current) ? SEEK_CUR : SEEK_SET) == 0;
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

static dr_bool32 drflac__on_seek_stdio(void* pUserData, int offset, drflac_seek_origin origin)
{
    assert(offset > 0 || (offset == 0 && origin == drflac_seek_origin_start));

    return SetFilePointer((HANDLE)pUserData, offset, NULL, (origin == drflac_seek_origin_current) ? FILE_CURRENT : FILE_BEGIN) != INVALID_SET_FILE_POINTER;
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

    drflac* pFlac = drflac_open(drflac__on_read_stdio, drflac__on_seek_stdio, (void*)file);
    if (pFlac == NULL) {
        drflac__close_file_handle(file);
        return NULL;
    }

    return pFlac;
}

drflac* drflac_open_file_with_metadata(const char* filename, drflac_meta_proc onMeta, void* pUserData)
{
    drflac_file file = drflac__open_file_handle(filename);
    if (file == NULL) {
        return NULL;
    }

    drflac* pFlac = drflac_open_with_metadata_private(drflac__on_read_stdio, drflac__on_seek_stdio, onMeta, (void*)file, pUserData);
    if (pFlac == NULL) {
        drflac__close_file_handle(file);
        return pFlac;
    }

    return pFlac;
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

static dr_bool32 drflac__on_seek_memory(void* pUserData, int offset, drflac_seek_origin origin)
{
    drflac__memory_stream* memoryStream = (drflac__memory_stream*)pUserData;
    assert(memoryStream != NULL);
    assert(offset > 0 || (offset == 0 && origin == drflac_seek_origin_start));

    if (origin == drflac_seek_origin_current) {
        if (memoryStream->currentReadPos + offset <= memoryStream->dataSize) {
            memoryStream->currentReadPos += offset;
        } else {
            memoryStream->currentReadPos = memoryStream->dataSize;  // Trying to seek too far forward.
        }
    } else {
        if ((dr_uint32)offset <= memoryStream->dataSize) {
            memoryStream->currentReadPos = offset;
        } else {
            memoryStream->currentReadPos = memoryStream->dataSize;  // Trying to seek too far forward.
        }
    }

    return DR_TRUE;
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

    // This is an awful hack...
#ifndef DR_FLAC_NO_OGG
    if (pFlac->container == drflac_container_ogg)
    {
        drflac_oggbs* oggbs = (drflac_oggbs*)(((dr_int32*)pFlac->pExtraData) + pFlac->maxBlockSize*pFlac->channels);
        oggbs->pUserData = &pFlac->memoryStream;
    }
    else
#endif
    {
        pFlac->bs.pUserData = &pFlac->memoryStream;
    }

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

    // This is an awful hack...
#ifndef DR_FLAC_NO_OGG
    if (pFlac->container == drflac_container_ogg)
    {
        drflac_oggbs* oggbs = (drflac_oggbs*)(((dr_int32*)pFlac->pExtraData) + pFlac->maxBlockSize*pFlac->channels);
        oggbs->pUserData = &pFlac->memoryStream;
    }
    else
#endif
    {
        pFlac->bs.pUserData = &pFlac->memoryStream;
    }

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
    if (pFlac->bs.onRead == drflac__on_read_stdio) {
        drflac__close_file_handle((drflac_file)pFlac->bs.pUserData);
    }

#ifndef DR_FLAC_NO_OGG
    // Need to clean up Ogg streams a bit differently due to the way the bit streaming is chained.
    if (pFlac->container == drflac_container_ogg) {
        assert(pFlac->bs.onRead == drflac__on_read_ogg);
        drflac_oggbs* oggbs = (drflac_oggbs*)((dr_int32*)pFlac->pExtraData + pFlac->maxBlockSize*pFlac->channels);
        if (oggbs->onRead == drflac__on_read_stdio) {
            drflac__close_file_handle((drflac_file)oggbs->pUserData);
        }
    }
#endif
#endif

    free(pFlac);
}

dr_uint64 drflac__read_s32__misaligned(drflac* pFlac, dr_uint64 samplesToRead, dr_int32* bufferOut)
{
    unsigned int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.header.channelAssignment);

    // We should never be calling this when the number of samples to read is >= the sample count.
    assert(samplesToRead < channelCount);
    assert(pFlac->currentFrame.samplesRemaining > 0 && samplesToRead <= pFlac->currentFrame.samplesRemaining);


    dr_uint64 samplesRead = 0;
    while (samplesToRead > 0)
    {
        dr_uint64 totalSamplesInFrame = pFlac->currentFrame.header.blockSize * channelCount;
        dr_uint64 samplesReadFromFrameSoFar = totalSamplesInFrame - pFlac->currentFrame.samplesRemaining;
        unsigned int channelIndex = samplesReadFromFrameSoFar % channelCount;

        dr_uint64 nextSampleInFrame = samplesReadFromFrameSoFar / channelCount;

        int decodedSample = 0;
        switch (pFlac->currentFrame.header.channelAssignment)
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


        decodedSample <<= ((32 - pFlac->bitsPerSample) + pFlac->currentFrame.subframes[channelIndex].wastedBitsPerSample);

        if (bufferOut) {
            *bufferOut++ = decodedSample;
        }

        samplesRead += 1;
        pFlac->currentFrame.samplesRemaining -= 1;
        samplesToRead -= 1;
    }

    return samplesRead;
}

dr_uint64 drflac__seek_forward_by_samples(drflac* pFlac, dr_uint64 samplesToRead)
{
    dr_uint64 samplesRead = 0;
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

dr_uint64 drflac_read_s32(drflac* pFlac, dr_uint64 samplesToRead, dr_int32* bufferOut)
{
    // Note that <bufferOut> is allowed to be null, in which case this will be treated as something like a seek.
    if (pFlac == NULL || samplesToRead == 0) {
        return 0;
    }

    if (bufferOut == NULL) {
        return drflac__seek_forward_by_samples(pFlac, samplesToRead);
    }


    dr_uint64 samplesRead = 0;
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

            unsigned int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFrame.header.channelAssignment);
            dr_uint64 totalSamplesInFrame = pFlac->currentFrame.header.blockSize * channelCount;
            dr_uint64 samplesReadFromFrameSoFar = totalSamplesInFrame - pFlac->currentFrame.samplesRemaining;

            int misalignedSampleCount = samplesReadFromFrameSoFar % channelCount;
            if (misalignedSampleCount > 0) {
                dr_uint64 misalignedSamplesRead = drflac__read_s32__misaligned(pFlac, misalignedSampleCount, bufferOut);
                samplesRead   += misalignedSamplesRead;
                samplesReadFromFrameSoFar += misalignedSamplesRead;
                bufferOut     += misalignedSamplesRead;
                samplesToRead -= misalignedSamplesRead;
            }


            dr_uint64 alignedSampleCountPerChannel = samplesToRead / channelCount;
            if (alignedSampleCountPerChannel > pFlac->currentFrame.samplesRemaining / channelCount) {
                alignedSampleCountPerChannel = pFlac->currentFrame.samplesRemaining / channelCount;
            }

            dr_uint64 firstAlignedSampleInFrame = samplesReadFromFrameSoFar / channelCount;
            unsigned int unusedBitsPerSample = 32 - pFlac->bitsPerSample;

            switch (pFlac->currentFrame.header.channelAssignment)
            {
                case DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE:
                {
                    const dr_int32* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const dr_int32* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    for (dr_uint64 i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int left  = pDecodedSamples0[i];
                        int side  = pDecodedSamples1[i];
                        int right = left - side;

                        bufferOut[i*2+0] = left  << (unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample);
                        bufferOut[i*2+1] = right << (unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample);
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                {
                    const dr_int32* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const dr_int32* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    for (dr_uint64 i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int side  = pDecodedSamples0[i];
                        int right = pDecodedSamples1[i];
                        int left  = right + side;

                        bufferOut[i*2+0] = left  << (unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample);
                        bufferOut[i*2+1] = right << (unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample);
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE:
                {
                    const dr_int32* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const dr_int32* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    for (dr_uint64 i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int side = pDecodedSamples1[i];
                        int mid  = (((dr_uint32)pDecodedSamples0[i]) << 1) | (side & 0x01);

                        bufferOut[i*2+0] = ((mid + side) >> 1) << (unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample);
                        bufferOut[i*2+1] = ((mid - side) >> 1) << (unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample);
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT:
                default:
                {
                    if (pFlac->currentFrame.header.channelAssignment == 1) // 1 = Stereo
                    {
                        // Stereo optimized inner loop unroll.
                        const dr_int32* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                        const dr_int32* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                        for (dr_uint64 i = 0; i < alignedSampleCountPerChannel; ++i) {
                            bufferOut[i*2+0] = pDecodedSamples0[i] << (unusedBitsPerSample + pFlac->currentFrame.subframes[0].wastedBitsPerSample);
                            bufferOut[i*2+1] = pDecodedSamples1[i] << (unusedBitsPerSample + pFlac->currentFrame.subframes[1].wastedBitsPerSample);
                        }
                    }
                    else
                    {
                        // Generic interleaving.
                        for (dr_uint64 i = 0; i < alignedSampleCountPerChannel; ++i) {
                            for (unsigned int j = 0; j < channelCount; ++j) {
                                bufferOut[(i*channelCount)+j] = (pFlac->currentFrame.subframes[j].pDecodedSamples[firstAlignedSampleInFrame + i]) << (unusedBitsPerSample + pFlac->currentFrame.subframes[j].wastedBitsPerSample);
                            }
                        }
                    }
                } break;
            }

            dr_uint64 alignedSamplesRead = alignedSampleCountPerChannel * channelCount;
            samplesRead   += alignedSamplesRead;
            samplesReadFromFrameSoFar += alignedSamplesRead;
            bufferOut     += alignedSamplesRead;
            samplesToRead -= alignedSamplesRead;
            pFlac->currentFrame.samplesRemaining -= (unsigned int)alignedSamplesRead;



            // At this point we may still have some excess samples left to read.
            if (samplesToRead > 0 && pFlac->currentFrame.samplesRemaining > 0)
            {
                dr_uint64 excessSamplesRead = 0;
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

dr_uint64 drflac_read_s16(drflac* pFlac, dr_uint64 samplesToRead, dr_int16* pBufferOut)
{
    // This reads samples in 2 passes and can probably be optimized.
    dr_uint64 totalSamplesRead = 0;

    while (samplesToRead > 0) {
        dr_int32 samples32[4096];
        dr_uint64 samplesJustRead = drflac_read_s32(pFlac, (samplesToRead > 4096) ? 4096 : samplesToRead, samples32);
        if (samplesJustRead == 0) {
            break;  // Reached the end.
        }

        // s32 -> s16
        for (dr_uint64 i = 0; i < samplesJustRead; ++i) {
            pBufferOut[i] = (dr_int16)(samples32[i] >> 16);
        }

        totalSamplesRead += samplesJustRead;
        samplesToRead -= samplesJustRead;
        pBufferOut += samplesJustRead;
    }

    return totalSamplesRead;
}

dr_uint64 drflac_read_f32(drflac* pFlac, dr_uint64 samplesToRead, float* pBufferOut)
{
    // This reads samples in 2 passes and can probably be optimized.
    dr_uint64 totalSamplesRead = 0;

    while (samplesToRead > 0) {
        dr_int32 samples32[4096];
        dr_uint64 samplesJustRead = drflac_read_s32(pFlac, (samplesToRead > 4096) ? 4096 : samplesToRead, samples32);
        if (samplesJustRead == 0) {
            break;  // Reached the end.
        }

        // s32 -> f32
        for (dr_uint64 i = 0; i < samplesJustRead; ++i) {
            pBufferOut[i] = (float)(samples32[i] / 2147483648.0);
        }

        totalSamplesRead += samplesJustRead;
        samplesToRead -= samplesJustRead;
        pBufferOut += samplesJustRead;
    }

    return totalSamplesRead;
}

dr_bool32 drflac_seek_to_sample(drflac* pFlac, dr_uint64 sampleIndex)
{
    if (pFlac == NULL) {
        return DR_FALSE;
    }

    if (sampleIndex == 0) {
        return drflac__seek_to_first_frame(pFlac);
    }

    // Clamp the sample to the end.
    if (sampleIndex >= pFlac->totalSampleCount) {
        sampleIndex  = pFlac->totalSampleCount - 1;
    }


    // Different techniques depending on encapsulation. Using the native FLAC seektable with Ogg encapsulation is a bit awkward so
    // we'll instead use Ogg's natural seeking facility.
#ifndef DR_FLAC_NO_OGG
    if (pFlac->container == drflac_container_ogg)
    {
        return drflac_ogg__seek_to_sample(pFlac, sampleIndex);
    }
    else
#endif
    {
        // First try seeking via the seek table. If this fails, fall back to a brute force seek which is much slower.
        if (!drflac__seek_to_sample__seek_table(pFlac, sampleIndex)) {
            return drflac__seek_to_sample__brute_force(pFlac, sampleIndex);
        }
    }


    return DR_TRUE;
}



//// High Level APIs ////

// Using a macro as the definition of the drflac__full_decode_and_close_*() API family. Sue me.
#define DRFLAC_DEFINE_FULL_DECODE_AND_CLOSE(extension, type) \
static type* drflac__full_decode_and_close_ ## extension (drflac* pFlac, unsigned int* channelsOut, unsigned int* sampleRateOut, dr_uint64* totalSampleCountOut)    \
{                                                                                                                                                                   \
    assert(pFlac != NULL);                                                                                                                                          \
                                                                                                                                                                    \
    type* pSampleData = NULL;                                                                                                                                       \
    dr_uint64 totalSampleCount = pFlac->totalSampleCount;                                                                                                           \
                                                                                                                                                                    \
    if (totalSampleCount == 0)                                                                                                                                      \
    {                                                                                                                                                               \
        type buffer[4096];                                                                                                                                          \
                                                                                                                                                                    \
        size_t sampleDataBufferSize = sizeof(buffer);                                                                                                               \
        pSampleData = (type*)malloc(sampleDataBufferSize);                                                                                                          \
        if (pSampleData == NULL) {                                                                                                                                  \
            goto on_error;                                                                                                                                          \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        dr_uint64 samplesRead;                                                                                                                                      \
        while ((samplesRead = (dr_uint64)drflac_read_##extension(pFlac, sizeof(buffer)/sizeof(buffer[0]), buffer)) > 0) {                                           \
            if (((totalSampleCount + samplesRead) * sizeof(type)) > sampleDataBufferSize) {                                                                         \
                sampleDataBufferSize *= 2;                                                                                                                          \
                type* pNewSampleData = (type*)realloc(pSampleData, sampleDataBufferSize);                                                                           \
                if (pNewSampleData == NULL) {                                                                                                                       \
                    free(pSampleData);                                                                                                                              \
                    goto on_error;                                                                                                                                  \
                }                                                                                                                                                   \
                                                                                                                                                                    \
                pSampleData = pNewSampleData;                                                                                                                       \
            }                                                                                                                                                       \
                                                                                                                                                                    \
            memcpy(pSampleData + totalSampleCount, buffer, (size_t)(samplesRead*sizeof(type)));                                                                     \
            totalSampleCount += samplesRead;                                                                                                                        \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        /* At this point everything should be decoded, but we just want to fill the unused part buffer with silence - need to                                       \
           protect those ears from random noise! */                                                                                                                 \
        memset(pSampleData + totalSampleCount, 0, (size_t)(sampleDataBufferSize - totalSampleCount*sizeof(type)));                                                  \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        dr_uint64 dataSize = totalSampleCount * sizeof(type);                                                                                                       \
        if (dataSize > SIZE_MAX) {                                                                                                                                  \
            goto on_error;  /* The decoded data is too big. */                                                                                                      \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        pSampleData = (type*)malloc((size_t)dataSize);    /* <-- Safe cast as per the check above. */                                                               \
        if (pSampleData == NULL) {                                                                                                                                  \
            goto on_error;                                                                                                                                          \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        dr_uint64 samplesDecoded = drflac_read_##extension(pFlac, pFlac->totalSampleCount, pSampleData);                                                            \
        if (samplesDecoded != pFlac->totalSampleCount) {                                                                                                            \
            free(pSampleData);                                                                                                                                      \
            goto on_error;  /* Something went wrong when decoding the FLAC stream. */                                                                               \
        }                                                                                                                                                           \
    }                                                                                                                                                               \
                                                                                                                                                                    \
    if (sampleRateOut) *sampleRateOut = pFlac->sampleRate;                                                                                                          \
    if (channelsOut) *channelsOut = pFlac->channels;                                                                                                                \
    if (totalSampleCountOut) *totalSampleCountOut = totalSampleCount;                                                                                               \
                                                                                                                                                                    \
    drflac_close(pFlac);                                                                                                                                            \
    return pSampleData;                                                                                                                                             \
                                                                                                                                                                    \
on_error:                                                                                                                                                           \
    drflac_close(pFlac);                                                                                                                                            \
    return NULL;                                                                                                                                                    \
}

DRFLAC_DEFINE_FULL_DECODE_AND_CLOSE(s32, dr_int32)
DRFLAC_DEFINE_FULL_DECODE_AND_CLOSE(s16, dr_int16)
DRFLAC_DEFINE_FULL_DECODE_AND_CLOSE(f32, float)

dr_int32* drflac_open_and_decode_s32(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount)
{
    // Safety.
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open(onRead, onSeek, pUserData);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close_s32(pFlac, channels, sampleRate, totalSampleCount);
}

dr_int16* drflac_open_and_decode_s16(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount)
{
    // Safety.
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open(onRead, onSeek, pUserData);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close_s16(pFlac, channels, sampleRate, totalSampleCount);
}

float* drflac_open_and_decode_f32(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount)
{
    // Safety.
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open(onRead, onSeek, pUserData);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close_f32(pFlac, channels, sampleRate, totalSampleCount);
}

#ifndef DR_FLAC_NO_STDIO
dr_int32* drflac_open_and_decode_file_s32(const char* filename, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open_file(filename);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close_s32(pFlac, channels, sampleRate, totalSampleCount);
}

dr_int16* drflac_open_and_decode_file_s16(const char* filename, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open_file(filename);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close_s16(pFlac, channels, sampleRate, totalSampleCount);
}

float* drflac_open_and_decode_file_f32(const char* filename, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open_file(filename);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close_f32(pFlac, channels, sampleRate, totalSampleCount);
}
#endif

dr_int32* drflac_open_and_decode_memory_s32(const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open_memory(data, dataSize);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close_s32(pFlac, channels, sampleRate, totalSampleCount);
}

dr_int16* drflac_open_and_decode_memory_s16(const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open_memory(data, dataSize);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close_s16(pFlac, channels, sampleRate, totalSampleCount);
}

float* drflac_open_and_decode_memory_f32(const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate, dr_uint64* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drflac* pFlac = drflac_open_memory(data, dataSize);
    if (pFlac == NULL) {
        return NULL;
    }

    return drflac__full_decode_and_close_f32(pFlac, channels, sampleRate, totalSampleCount);
}

void drflac_free(void* pSampleDataReturnedByOpenAndDecode)
{
    free(pSampleDataReturnedByOpenAndDecode);
}




void drflac_init_vorbis_comment_iterator(drflac_vorbis_comment_iterator* pIter, dr_uint32 commentCount, const char* pComments)
{
    if (pIter == NULL) {
        return;
    }

    pIter->countRemaining = commentCount;
    pIter->pRunningData   = pComments;
}

const char* drflac_next_vorbis_comment(drflac_vorbis_comment_iterator* pIter, dr_uint32* pCommentLengthOut)
{
    // Safety.
    if (pCommentLengthOut) *pCommentLengthOut = 0;

    if (pIter == NULL || pIter->countRemaining == 0 || pIter->pRunningData == NULL) {
        return NULL;
    }

    dr_uint32 length = drflac__le2host_32(*(dr_uint32*)pIter->pRunningData);
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
// v0.4d - 2016-12-26
//   - Add support for 32-bit floating-point PCM decoding.
//   - Use dr_int*/dr_uint* sized types to improve compiler support.
//   - Minor improvements to documentation.
//
// v0.4c - 2016-12-26
//   - Add support for signed 16-bit integer PCM decoding.
//
// v0.4b - 2016-10-23
//   - A minor change to dr_bool8 and dr_bool32 types.
//
// v0.4a - 2016-10-11
//   - Rename drBool32 to dr_bool32 for styling consistency.
//
// v0.4 - 2016-09-29
//   - API/ABI CHANGE: Use fixed size 32-bit booleans instead of the built-in bool type.
//   - API CHANGE: Rename drflac_open_and_decode*() to drflac_open_and_decode*_s32()
//   - API CHANGE: Swap the order of "channels" and "sampleRate" parameters in drflac_open_and_decode*(). Rationale for this is to
//     keep it consistent with dr_audio.
//
// v0.3f - 2016-09-21
//   - Fix a warning with GCC.
//
// v0.3e - 2016-09-18
//   - Fixed a bug where GCC 4.3+ was not getting properly identified.
//   - Fixed a few typos.
//   - Changed date formats to ISO 8601 (YYYY-MM-DD).
//
// v0.3d - 2016-06-11
//   - Minor clean up.
//
// v0.3c - 2016-05-28
//   - Fixed compilation error.
//
// v0.3b - 2016-05-16
//   - Fixed Linux/GCC build.
//   - Updated documentation.
//
// v0.3a - 2016-05-15
//   - Minor fixes to documentation.
//
// v0.3 - 2016-05-11
//   - Optimizations. Now at about parity with the reference implementation on 32-bit builds.
//   - Lots of clean up.
//
// v0.2b - 2016-05-10
//   - Bug fixes.
//
// v0.2a - 2016-05-10
//   - Made drflac_open_and_decode() more robust.
//   - Removed an unused debugging variable
//
// v0.2 - 2016-05-09
//   - Added support for Ogg encapsulation.
//   - API CHANGE. Have the onSeek callback take a third argument which specifies whether or not the seek
//     should be relative to the start or the current position. Also changes the seeking rules such that
//     seeking offsets will never be negative.
//   - Have drflac_open_and_decode() fail gracefully if the stream has an unknown total sample count.
//
// v0.1b - 2016-05-07
//   - Properly close the file handle in drflac_open_file() and family when the decoder fails to initialize.
//   - Removed a stale comment.
//
// v0.1a - 2016-05-05
//   - Minor formatting changes.
//   - Fixed a warning on the GCC build.
//
// v0.1 - 2016-05-03
//   - Initial versioned release.


// TODO
// - Add support for initializing the decoder without a header STREAMINFO block.
// - Test CUESHEET metadata blocks.


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
