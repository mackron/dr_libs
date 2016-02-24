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

// Check if we can enable 64-bit optimizations.
#if defined(_WIN64)
#define DRFLAC_64BIT
#endif

#if defined(__GNUC__)
#if defined(__x86_64__) || defined(__ppc64__)
#define DRFLAC_64BIT
#endif
#endif


// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* drflac_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

// Callback for when data needs to be seeked. Offset is always relative to the current position. Return value is 0 on failure, non-zero success.
typedef int (* drflac_seek_proc)(void* userData, int offset);


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

    // The user data to pass around to onRead and onSeek.
    void* userData;


    // The current byte position in the client's data stream.
    uint64_t currentBytePos;


    // The cached data which was most recently read from the client. When data is read from the client, it is placed within this
    // variable. As data is read, it's bit-shifted such that the next valid bit is sitting on the most significant bit.
#ifdef DRFLAC_64BIT
    uint64_t cache;
    uint64_t cacheL2[32];
#else
    uint32_t cache;
    uint32_t cacheL2[32];
#endif

    // The index of the next valid cache line in the "L2" cache.
    size_t nextL2Line;

    // The number of bits that have been consumed by the cache. This is used to determine how many valid bits are remaining.
    size_t consumedBits;

    // Unused L2 lines. This will always be 0 until the end of the stream is hit. Used for correctly calculating the current byte
    // position of the read pointer in the stream.
    size_t unusedL2Lines;


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
bool drflac_open(drflac* pFlac, drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData);

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

    return drflac_open(pFlac, drflac__on_read_stdio, drflac__on_seek_stdio, pFile);
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
    return drflac_open(pFlac, drflac__on_read_memory, drflac__on_seek_memory, pUserData);
}


//// Endian Management ////
static inline bool drflac__is_little_endian()
{
    int n = 1;
    return (*(char*)&n) == 1;
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



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// THE ADVENTURES OF EFFICIENTLY READING DATA FROM A BITSTREAM
//     by David Reid
//
// There are three main performance issues to consider when decoding a FLAC stream:
//   1) Reading data from a stream of bits
//   2) Rice codes
//   3) Prediction evaluation
//
// So far dr_flac does a terrible job of all three of these things, but the hardest thing for me has been figuring out the bit
// streaming stuff. This little section is a sort of journal in my adventures in figuring out how to efficiently read variable
// length numbers from a stream of bits.
//
//
// --- Chapter 1: Lessons Learn so Far ---
// The very first implementation I tried worked by loading one single byte of data at a time from the client. This was terrible.
// The problem is quite obvious - there was just too many calls being made back to the client for extra data. The obvious fix for
// this is to read in several bytes at a time - say, 8 bytes (64 bits).
//
// Loading 64 bits at a time instead of 8 makes a huge difference - the time spent on the client side retrieving data barely
// shows up in profiling. This is good, but of course when one issue is fixed, other issues become more obvious. The problem I
// now faced is that the 32-bit build was almost 2x slower than the 64-bit build. The reason is obvious - the 64-bit build uses
// native 64-bit arithmatic which is not available to the 32-bit build. I could be lazy and just leave it as is, but unfortunately
// many developers still distribute 32-bit builds of their software for the Windows platform. Also, it turns out even when using
// a 64-bit bin the reference FLAC decoder is still 3x faster. A complete rethinking of this is needed.
// 
// So how do we fix it? As I'm writing this sentence I've still got no idea. The next sections are going to detail my thought
// process on how I try to figure out this problem and implement a truly optimal solution.
//
//
// --- Chapter 2: The Experiments ---
// Experiment #1 - February 22, 2016 - 3:39 PM
// As I'm writing this sentence I'm going through that phase where I think every programmer has been where I feel like I need to
// take a step back and completely rethink the problem. One of the problems I had with the previous implementations is that I
// actually struggled to get the functionality working at all nevermind getting is optimal. This is an issue for me because the
// problem itself is conceptually quite simple - you just read a given number of bits from the cached data, but if there isn't
// enough just fetch more data and continue. Often times I've found that by simplifying a problem, it often ends up faster as
// a side effect, so I think my first experiment will be one of figuring out how to simplify the problem.
//
// The complexities I faced with the previous implementations off the top of my head:
// 1) Figuring out the masking to use when isolating specific bits.
// 2) Figuring out the exact number of bits to shift.
// 3) Trying to shift outside the range of [0..63]. A bit shift of 64 is actually erroneous on 64-bit builds in MSVC.
//
// So lets now brainstorm ideas on how to simplify these problems.
//
// Problem 1) The first problem is figuring how to mask out the specific bits needing to be read. The existing implementation uses
// a variable to keep track of how many valid bits are remaining in the cache. This combined with the number of bits being
// requested is used to calculate the mask. The current implementation leaves the cached data exactly as is for it's lifetime
// and depends on calculating a mask depending on how many valid bits are remaining and how many bits are being requested. After
// thinking about this for a bit, we'll experiment with bit-shifting the cached data such that the next valid bit is sitting on
// the most significant bit (1 << 63). This should simplify the mask calculations quite a bit.
//
// Problem 2) When we isolate the bits within the cache they need to be shifted in a way where it's in a suitable format for the
// output variable. Calculating this shift is annoyingly complex in the currently implementation, but with the above experiment
// it may become a bit simpler - will see how that one turns out first.
//
// Result - February 22, 2016 - 10:12 PM
// Excitement got the better of me and I just jumped straight in. This new implementation is a LOT simpler, and is now "only" 2x
// slower than the reference implementation. At this point I'm happy with the simplicity of this system so I'm keeping the
// general design as-is for now. There is still the issue of 64-bit wide bit manipulation so the next experiment will be looking
// at using 32-bit shifts instead.
//
//
// Experiment #2 - February 22, 2016 - 10:16 PM
// The next experiment is to use a 32-bit cache instead of 64-bit. This will result in more frequent fetches from the client, but
// more efficient bit manipulation. Some differences from the 64-bit version to think about:
// - When reading an integer, it'll be quicker to have the uint32 version be the base version. When a 64-bit integer needs to be
//   read, it can be done with 2 bitwise-ored 32-bit integers. Could also look at this for the 64-bit build for consistency.
//
// Experiment #3 - February 23, 2016 - 8:08 AM
// The 32-bit version is now working... and it's slower. Looking at the profiling results, it appears we spend about 22% of our
// time in fread() when loading from a file. This isn't good enough. We need to employ more efficient caching, and I think I've
// got an idea. I think I want to try borrowing the concept of the L1/L2 caches system used in CPU architecture, where we use
// our normal 32/64-bit cached value as the L1, and then have the L2 be a larger cache. The idea I have at the moment is that
// when the L1 is exhausted it will be reloaded from L2. If the L2 is exchaused it will be reloaded from the client. In order
// for this system to be effective however, we need to ensure fetching data from L2 is significantly more efficient than fetching
// data straight from the client. Ideas on how to do this:
// - All data in the L2 cache should be aligned to the size of the L1 cache. This way we can move a value from L2 to L1 with a
//   single mov instruction.
//   - This presents a problem when we reach the end of the file which will likely not be nicely aligned to the size of L1. In
//     this case we'll need to fall back to a slow path which reads data straight from the client to L1.
// - The size of L2 should be large enough to prevent too many data requests from the client, but small enough that it doesn't
//   blow out the size of the drflac structure too much. Using the heap is not an option.
//
// Experiment #4 - February 23, 2016 - 12:28 PM
// The "L2" cache has worked nicely with an improvement of about 20%. The next issue is now one of project management. On the
// 64-bit build we retrieve values from the bit stream via drflac__read_uint64(), however the 32-bit build does it via
// drflac__read_uint32() (a 64-bit value is bitwise-ored from two seperate 32-bit values). 64-bit values are relatively rare
// compared to 32-bit, so the next experiment will be to convert the 64-bit build to use drflac__read_uint32() as that base
// and see if we can get comparible performance. If so we can avoid some annoying code duplication.
//
// Result: As expected there is no noticeable difference in performance so now drflac__read_uint32() is the base function for
// reading values from the bit stream for all platforms. At this point I'm happy with the overarching design of the bit streaming
// functionality so I'll now be focusing on more important things. We'll probably be coming back to this later on when we start
// figuring out how to efficiently read a string of Rice codes...
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// BIT READING ATTEMPT #2
//
// This uses a 64-bit dynamically bit-shifted cache - as bits are read, the cache is shifted such that the first valid bit is
// sitting on the most significant bit.
#define DRFLAC_CACHE_L1_SIZE_BYTES                 (sizeof(pFlac->cache))
#define DRFLAC_CACHE_L1_SIZE_BITS                  (sizeof(pFlac->cache)*8)
#define DRFLAC_CACHE_L1_BITS_REMAINING             (DRFLAC_CACHE_L1_SIZE_BITS - (pFlac->consumedBits))
#ifdef DRFLAC_64BIT
#define DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount)  (~(((uint64_t)-1LL) >> (_bitCount)))
#else
#define DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount)  (~(((uint32_t)-1) >> (_bitCount)))
#endif
#define DRFLAC_CACHE_L1_SELECTION_SHIFT(_bitCount) (DRFLAC_CACHE_L1_SIZE_BITS - (_bitCount))
#define DRFLAC_CACHE_L1_SELECT(_bitCount)          ((pFlac->cache) & DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount))
#define DRFLAC_CACHE_L1_SELECT_AND_SHIFT(_bitCount)(DRFLAC_CACHE_L1_SELECT(_bitCount) >> DRFLAC_CACHE_L1_SELECTION_SHIFT(_bitCount))
#define DRFLAC_CACHE_L2_SIZE_BYTES                 (sizeof(pFlac->cacheL2))
#define DRFLAC_CACHE_L2_LINE_COUNT                 (sizeof(pFlac->cacheL2) / sizeof(pFlac->cacheL2[0]))
#define DRFLAC_CACHE_L2_LINES_REMAINING            (DRFLAC_CACHE_L2_LINE_COUNT - pFlac->nextL2Line)

static inline bool drflac__reload_cache_from_l2(drflac* pFlac)
{
    if (pFlac->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT) {
        pFlac->cache = pFlac->cacheL2[pFlac->nextL2Line++];
        return true;
    }


    // If we get here it means we've run out of data in the L2 cache. We'll need to fetch more from the client.
    size_t bytesRead = pFlac->onRead(pFlac->userData, pFlac->cacheL2, DRFLAC_CACHE_L2_SIZE_BYTES);
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
            pFlac->onSeek(pFlac->userData, -(int)unalignedBytes);
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
            pFlac->onSeek(pFlac->userData, -(int)bytesRead);
            pFlac->currentBytePos -= bytesRead;
        }

        pFlac->nextL2Line = DRFLAC_CACHE_L2_LINE_COUNT;
        return false;
    }
}

static inline bool drflac__reload_cache(drflac* pFlac)
{
    if (drflac__reload_cache_from_l2(pFlac)) {
#ifdef DRFLAC_64BIT
        pFlac->cache = drflac__be2host_64(pFlac->cache);
#else
        pFlac->cache = drflac__be2host_32(pFlac->cache);
#endif
        pFlac->consumedBits = 0;
        return true;
    }

    // If we get here it means we have failed to load the L1 cache from the L2. Likely we've just reached the end of the stream and the last
    // few bytes did not meet the alignment requirements for the L2 cache. In this case we need to fall back to a slower path and read the
    // data straight from the client into the L1 cache. This should only really happen once per stream so efficiency is not important.
    size_t bytesRead = pFlac->onRead(pFlac->userData, &pFlac->cache, DRFLAC_CACHE_L1_SIZE_BYTES);
    if (bytesRead == 0) {
        return false;
    }

    pFlac->currentBytePos += bytesRead;

    assert(bytesRead < DRFLAC_CACHE_L1_SIZE_BYTES);
    pFlac->consumedBits = (DRFLAC_CACHE_L1_SIZE_BYTES - bytesRead) * 8;
    
#ifdef DRFLAC_64BIT
    pFlac->cache = drflac__be2host_64(pFlac->cache);
#else
    pFlac->cache = drflac__be2host_32(pFlac->cache);
#endif

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

                pFlac->onSeek(pFlac->userData, (int)wholeBytesRemaining);
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

static inline bool drflac__read_uint32(drflac* pFlac, unsigned int bitCount, uint32_t* pResultOut)
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

static inline bool drflac__read_int32(drflac* pFlac, unsigned int bitCount, int32_t* pResult)
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

static inline bool drflac__read_uint64(drflac* pFlac, unsigned int bitCount, uint64_t* pResultOut)
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

static inline bool drflac__read_int64(drflac* pFlac, unsigned int bitCount, int64_t* pResultOut)
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

static inline bool drflac__read_uint16(drflac* pFlac, unsigned int bitCount, uint16_t* pResult)
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

static inline bool drflac__read_int16(drflac* pFlac, unsigned int bitCount, int16_t* pResult)
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

static inline bool drflac__read_uint8(drflac* pFlac, unsigned int bitCount, uint8_t* pResult)
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

static inline bool drflac__read_int8(drflac* pFlac, unsigned int bitCount, int8_t* pResult)
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


static inline int drflac__read_next_bit(drflac* pFlac)
{
    assert(pFlac != NULL);

    if (DRFLAC_CACHE_L1_BITS_REMAINING == 0) {
        if (!drflac__reload_cache(pFlac)) {
            return -1;
        }
    }

    int result = DRFLAC_CACHE_L1_SELECT_AND_SHIFT(1);
    pFlac->consumedBits += 1;
    pFlac->cache <<= 1;

    return result;
}

static inline bool drflac__seek_past_next_set_bit(drflac* pFlac, unsigned int* pOffsetOut)
{
#if 0
    // Slow naive method.
    unsigned int zeroCounter = 0;
    while (drflac__read_next_bit(pFlac) == 0) {
        zeroCounter += 1;
    }
    
    *pOffsetOut = zeroCounter;
    return true;
#endif

#if 0
    // Experiment #1: Not-so-slow-but-still-slow naive method.
    size_t prevConsumedBits = pFlac->consumedBits;
    while (pFlac->consumedBits < DRFLAC_CACHE_L1_SIZE_BITS)
    {
        if (DRFLAC_CACHE_L1_SELECT(1)) {
            *pOffsetOut = (unsigned int)(pFlac->consumedBits - prevConsumedBits);
            pFlac->consumedBits += 1;
            pFlac->cache <<= 1;
            return true;
        }

        pFlac->consumedBits += 1;
        pFlac->cache <<= 1;
    }

    // If we get here it means we've run out of data in the cache.
    if (!drflac__reload_cache(pFlac)) {
        return false;
    }

    size_t counter1 = DRFLAC_CACHE_L1_SIZE_BITS - prevConsumedBits;
    for (;;)
    {
        size_t prevConsumedBits2 = pFlac->consumedBits;
        while (pFlac->consumedBits < DRFLAC_CACHE_L1_SIZE_BITS)
        {
            if (DRFLAC_CACHE_L1_SELECT(1)) {
                *pOffsetOut = (unsigned int)(counter1 + (pFlac->consumedBits - prevConsumedBits2));
                pFlac->consumedBits += 1;
                pFlac->cache <<= 1;
                return true;
            }

            pFlac->consumedBits += 1;
            pFlac->cache <<= 1;
        }

        counter1 += DRFLAC_CACHE_L1_SIZE_BITS;
        if (!drflac__reload_cache(pFlac)) {
            return false;
        }
    }

    return false;
#endif

#if 1
    unsigned int zeroCounter = 0;
    while (pFlac->cache == 0) {
        zeroCounter += (unsigned int)DRFLAC_CACHE_L1_BITS_REMAINING;
        if (!drflac__reload_cache(pFlac)) {
            return false;
        }
    }

    // At this point the cache should not be zero, in which case we know the first set bit should be somewhere in here. There is
    // not need for us to perform any cache reloading logic here which should make things much faster.
    assert(pFlac->cache != 0);

    unsigned int setBitOffsetPlus1;
    if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(1))) {
        setBitOffsetPlus1 = 1;
    } else if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(2))) {
        setBitOffsetPlus1 = 2;
    } else if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(3))) {
        setBitOffsetPlus1 = 3;
    } else if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(4))) {
        setBitOffsetPlus1 = 4;
    } else {
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
#endif
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

    pFlac->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS;
    pFlac->cache = 0;
    pFlac->nextL2Line = DRFLAC_CACHE_L2_LINE_COUNT; // <-- This clears the L2 cache.

    return result;
}

static inline long long drflac__tell(drflac* pFlac)
{
    assert(pFlac != NULL);

    size_t uneadBytesFromL1 = (DRFLAC_CACHE_L1_SIZE_BYTES - (pFlac->consumedBits/8));
    size_t uneadBytesFromL2 = (DRFLAC_CACHE_L2_SIZE_BYTES - ((pFlac->nextL2Line - pFlac->unusedL2Lines)*DRFLAC_CACHE_L1_SIZE_BYTES));

    return pFlac->currentBytePos - uneadBytesFromL1 - uneadBytesFromL2;
}



static bool drflac__read_utf8_coded_number(drflac* pFlac, unsigned long long* pNumberOut)
{
    assert(pFlac != NULL);
    assert(pNumberOut != NULL);

    // We should never need to read UTF-8 data while not being aligned to a byte boundary. Therefore we can grab the data
    // directly from the input stream rather than using drflac__read_uint8().
    assert((pFlac->consumedBits % 8) == 0);

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

static inline bool drflac__read_and_decode_rice(drflac* pFlac, unsigned char riceParam, int* pValueOut)
{
    unsigned int zeroCounter;
    if (!drflac__seek_past_next_set_bit(pFlac, &zeroCounter)) {
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

static int drflac__calculate_prediction_32(unsigned int order, int shift, const short* coefficients, int* pDecodedSamples)
{
    int prediction = 0;
    for (int j = 0; j < (int)order; ++j) {
        prediction += coefficients[j] * pDecodedSamples[-j-1];
    }
    prediction >>= shift;

    return prediction;
}

static int drflac__calculate_prediction(unsigned int order, int shift, const short* coefficients, int* pDecodedSamples)
{
    long long prediction = 0;
    for (int j = 0; j < (int)order; ++j) {
        prediction += (long long)coefficients[j] * (long long)pDecodedSamples[-j-1];
    }
    prediction >>= shift;

    return (int)prediction;
}

// Reads and decodes a string of residual values as Rice codes. The decoder should be sitting on the first bit of the Rice codes.
//
// This is the most expensive function in the library. It does both the Rice decoding and the prediction in a single loop iteration.
//
// Brainstorming Session #1
// - Each Rice code is made up of two general parts: The part up to and including the first set bit, and the <riceParam> bits following.
// - Can quickly check if any bits are set in the cache by simply checking if it is equal to 0. If it's equal to 0 we can immediately
//   load in the next cache line. If it's not zero, we can find the first set bit while avoiding cache reloading logic.
static bool drflac__decode_samples_with_residual__rice(drflac* pFlac, unsigned int count, unsigned char riceParam, unsigned int wastedBitsPerSample, unsigned int order, int shift, const short* coefficients, int* pSamplesOut)
{
    assert(pFlac != NULL);
    assert(count > 0);
    assert(pSamplesOut != NULL);

    for (int i = 0; i < (int)count; ++i)
    {
#if 0
        // Simpler, but slightly slower way of decoding the Rice code.
        int decodedRice;
        if (!drflac__read_and_decode_rice(pFlac, riceParam, &decodedRice)) {
            return false;
        }
#endif

#if 1
        unsigned int zeroCounter = 0;
        while (pFlac->cache == 0) {
            zeroCounter += (unsigned int)DRFLAC_CACHE_L1_BITS_REMAINING;
            if (!drflac__reload_cache(pFlac)) {
                return false;
            }
        }

        // At this point the cache should not be zero, in which case we know the first set bit should be somewhere in here. There is
        // not need for us to perform any cache reloading logic here which should make things much faster.
        assert(pFlac->cache != 0);


        unsigned int decodedRice;

        unsigned int setBitOffsetPlus1;
        if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(1))) {
            setBitOffsetPlus1 = 1;
            decodedRice = (zeroCounter + 0) << riceParam;
        } else if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(2))) {
            setBitOffsetPlus1 = 2;
            decodedRice = (zeroCounter + 1) << riceParam;
        } else if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(3))) {
            setBitOffsetPlus1 = 3;
            decodedRice = (zeroCounter + 2) << riceParam;
        } else if ((pFlac->cache & DRFLAC_CACHE_L1_SELECT(4))) {
            setBitOffsetPlus1 = 4;
            decodedRice = (zeroCounter + 3) << riceParam;
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

        unsigned int bitsLo;
        unsigned int riceLength = setBitOffsetPlus1 + riceParam;
        if (riceLength < DRFLAC_CACHE_L1_BITS_REMAINING)
        {
            bitsLo = (pFlac->cache & (DRFLAC_CACHE_L1_SELECTION_MASK(riceParam) >> setBitOffsetPlus1)) >> (DRFLAC_CACHE_L1_SIZE_BITS - riceLength);

            pFlac->consumedBits += riceLength;
            pFlac->cache <<= riceLength;
        }
        else
        {
            pFlac->consumedBits += setBitOffsetPlus1;
            pFlac->cache <<= setBitOffsetPlus1;

            if (riceParam > 0)
            {
                // It straddles the cached data. It will never cover more than the next chunk. We just read the number in two parts and combine them.
                size_t bitCountHi = DRFLAC_CACHE_L1_BITS_REMAINING;
                size_t bitCountLo = riceParam - bitCountHi;
                uint32_t resultHi = DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bitCountHi);
        
                if (!drflac__reload_cache(pFlac)) {
                    return false;
                }

                bitsLo = (resultHi << bitCountLo) | DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bitCountLo);
                pFlac->consumedBits += bitCountLo;
                pFlac->cache <<= bitCountLo;
            }
            else {
                bitsLo = 0;
            }
        }

        
        decodedRice |= bitsLo;
        if ((decodedRice & 0x01)) {
            decodedRice = ~(decodedRice >> 1);
        } else {
            decodedRice = (decodedRice >> 1);
        }
#endif
        

        // In order to properly calculate the prediction when the bits per sample is >16 we need to do it using 64-bit arithmetic. We can assume this
        // is probably going to be slower on 32-bit systems so we'll do a more optimized 32-bit version when the bits per sample is low enough.
        if (pFlac->currentFrame.bitsPerSample <= 16) {
            pSamplesOut[i] = (((int)decodedRice + drflac__calculate_prediction_32(order, shift, coefficients, pSamplesOut + i)) << wastedBitsPerSample);
        } else {
            pSamplesOut[i] = (((int)decodedRice + drflac__calculate_prediction(order, shift, coefficients, pSamplesOut + i)) << wastedBitsPerSample);
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

static bool drflac__decode_samples_with_residual__unencoded(drflac* pFlac, unsigned int count, unsigned char unencodedBitsPerSample, unsigned int wastedBitsPerSample, unsigned int order, int shift, const short* coefficients, int* pSamplesOut)
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
        pSamplesOut[i] <<= wastedBitsPerSample;
    }

    return true;
}


// Reads and decodes the residual for the sub-frame the decoder is currently sitting on. This function should be called
// when the decoder is sitting at the very start of the RESIDUAL block. The first <order> residuals will be ignored. The
// <blockSize> and <order> parameters are used to determine how many residual values need to be decoded.
static bool drflac__decode_samples_with_residual(drflac* pFlac, unsigned int blockSize, unsigned int wastedBitsPerSample, unsigned int order, int shift, const short* coefficients, int* pDecodedSamples)
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
            if (!drflac__decode_samples_with_residual__rice(pFlac, samplesInPartition, riceParam, wastedBitsPerSample, order, shift, coefficients, pDecodedSamples)) {
                return false;
            }
        } else {
            unsigned char unencodedBitsPerSample = 0;
            if (!drflac__read_uint8(pFlac, 5, &unencodedBitsPerSample)) {
                return false;
            }

            if (!drflac__decode_samples_with_residual__unencoded(pFlac, samplesInPartition, unencodedBitsPerSample, wastedBitsPerSample, order, shift, coefficients, pDecodedSamples)) {
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
        pSubframe->pDecodedSamples[i] = sample << pSubframe->wastedBitsPerSample;
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

        pSubframe->pDecodedSamples[i] = sample << pSubframe->wastedBitsPerSample;
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

            
    if (!drflac__decode_samples_with_residual(pFlac, pFlac->currentFrame.blockSize, pSubframe->wastedBitsPerSample, pSubframe->lpcOrder, 0, lpcCoefficientsTable[pSubframe->lpcOrder], pSubframe->pDecodedSamples)) {
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

    if (!drflac__decode_samples_with_residual(pFlac, pFlac->currentFrame.blockSize, pSubframe->wastedBitsPerSample, pSubframe->lpcOrder, lpcShift, pSubframe->lpcCoefficients, pSubframe->pDecodedSamples)) {
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
    if (!drflac__seek_bits(pFlac, (DRFLAC_CACHE_L1_BITS_REMAINING & 7) + 16)) {
        return false;
    }


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
    return drflac__seek_bits(pFlac, (DRFLAC_CACHE_L1_BITS_REMAINING & 7) + 16);
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
    pFlac->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS;
    pFlac->cache = 0;

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

    assert(firstSampleInFrame <= sampleIndex);

    // At this point we are just sitting on the byte after the frame header. We need to decode the frame before reading anything from it.
    if (!drflac__decode_frame(pFlac)) {
        return false;
    }

    size_t samplesToDecode = (size_t)(sampleIndex - firstSampleInFrame);    // <-- Safe cast because the maximum number of samples in a frame is 65535.
    return drflac_read_s32(pFlac, samplesToDecode, NULL) == samplesToDecode;
}


bool drflac_open(drflac* pFlac, drflac_read_proc onRead, drflac_seek_proc onSeek, void* userData)
{
    if (pFlac == NULL || onRead == NULL || onSeek == NULL) {
        return false;
    }

    unsigned char id[4];
    if (onRead(userData, id, 4) != 4 || id[0] != 'f' || id[1] != 'L' || id[2] != 'a' || id[3] != 'C') {
        return false;    // Not a FLAC stream.
    }

    memset(pFlac, 0, sizeof(*pFlac));
    pFlac->onRead         = onRead;
    pFlac->onSeek         = onSeek;
    pFlac->userData       = userData;
    pFlac->currentBytePos = 4;   // Set to 4 because we just read the ID which is 4 bytes.
    pFlac->nextL2Line     = DRFLAC_CACHE_L2_LINE_COUNT; // <-- Initialize to this to force a client-side data retrieval right from the start.
    pFlac->consumedBits   = DRFLAC_CACHE_L1_SIZE_BITS;
    
    
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

                    for (size_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int left  = pDecodedSamples0[i];
                        int side  = pDecodedSamples1[i];
                        int right = left - side;

                        bufferOut[i*2+0] = left  << unusedBitsPerSample;
                        bufferOut[i*2+1] = right << unusedBitsPerSample;
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                {
                    const int* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const int* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    for (size_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int side  = pDecodedSamples0[i];
                        int right = pDecodedSamples1[i];
                        int left  = right + side;

                        bufferOut[i*2+0] = left  << unusedBitsPerSample;
                        bufferOut[i*2+1] = right << unusedBitsPerSample;
                    }
                } break;

                case DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE:
                {
                    const int* pDecodedSamples0 = pFlac->currentFrame.subframes[0].pDecodedSamples + firstAlignedSampleInFrame;
                    const int* pDecodedSamples1 = pFlac->currentFrame.subframes[1].pDecodedSamples + firstAlignedSampleInFrame;

                    for (size_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                        int side = pDecodedSamples1[i];
                        int mid  = (((uint32_t)pDecodedSamples0[i]) << 1) | (side & 0x01);

                        bufferOut[i*2+0] = ((mid + side) >> 1) << unusedBitsPerSample;
                        bufferOut[i*2+1] = ((mid - side) >> 1) << unusedBitsPerSample;
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

                        for (size_t i = 0; i < alignedSampleCountPerChannel; ++i) {
                            bufferOut[i*2+0] = pDecodedSamples0[i] << unusedBitsPerSample;
                            bufferOut[i*2+1] = pDecodedSamples1[i] << unusedBitsPerSample;
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
