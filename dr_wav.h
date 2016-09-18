// WAV audio loader. Public domain. See "unlicense" statement at the end of this file.
// dr_wav - v0.4b - 2016-09-18
//
// David Reid - mackron@gmail.com

// USAGE
//
// This is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_WAV_IMPLEMENTATION
//   #include "dr_wav.h"
//
// You can then #include this file in other parts of the program as you would with any other header file. Do something
// like the following to read audio data:
//
//     drwav wav;
//     if (!drwav_init_file(&wav, "my_song.wav")) {
//         // Error opening WAV file.
//     }
//
//     int32_t* pDecodedInterleavedSamples = malloc(wav.totalSampleCount * sizeof(int32_t));
//     size_t numberOfSamplesActuallyDecoded = drwav_read_s32(&wav, wav.totalSampleCount, pDecodedInterleavedSamples);
//
//     ...
//
//     drwav_uninit(&wav);
//
// You can also use drwav_open() to allocate and initialize the loader for you:
//
//     drwav* pWav = drwav_open_file("my_song.wav");
//     if (pWav == NULL) {
//         // Error opening WAV file.
//     }
//
//     ...
//
//     drwav_close(pWav);
//
// If you just want to quickly open and read the audio data in a single operation you can do something like this:
//
//     unsigned int sampleRate;
//     unsigned int channels;
//     uint64_t totalSampleCount;
//     float* pSampleData = drwav_open_and_read_file_s32("my_song.wav", &sampleRate, &channels, &totalSampleCount);
//     if (pSampleData == NULL) {
//         // Error opening and reading WAV file.
//     }
//
//     ...
//
//     drwav_free(pSampleData);
//
// The examples above use versions of the API that convert the audio data to a consistent format (32-bit signed PCM, in
// this case), but you can still output the audio data in it's internal format (see notes below for supported formats):
//
//     size_t samplesRead = drwav_read(&wav, wav.totalSampleCount, pDecodedInterleavedSamples);
//
// You can also read the raw bytes of audio data, which could be useful if dr_wav does not have native support for
// a particular data format:
//
//     size_t bytesRead = drwav_read_raw(&wav, bytesToRead, pRawDataBuffer);
//
//
// dr_wav has seamless support the Sony Wave64 format. The decoder will automatically detect it and it should Just Work
// without any manual intervention.
//
//
//
// OPTIONS
// #define these options before including this file.
//
// #define DR_WAV_NO_CONVERSION_API
//   Disables conversion APIs such as drwav_read_f32() and drwav_s16_to_f32().
//
// #define DR_WAV_NO_STDIO
//   Disables drwav_open_file().
//
//
//
// QUICK NOTES
// - Samples are always interleaved.
// - The default read function does not do any data conversion. Use drwav_read_f32() to read and convert audio data
//   to IEEE 32-bit floating point samples. Likewise, use drwav_read_s32() to read and convert auto data to signed
//   32-bit PCM. Tested and supported internal formats include the following:
//   - Unsigned 8-bit PCM
//   - Signed 12-bit PCM
//   - Signed 16-bit PCM
//   - Signed 24-bit PCM
//   - Signed 32-bit PCM
//   - IEEE 32-bit floating point.
//   - IEEE 64-bit floating point.
//   - A-law and u-law
// - Microsoft ADPCM is not currently supported.
// - dr_wav will try to read the WAV file as best it can, even if it's not strictly conformant to the WAV format.


#ifndef dr_wav_h
#define dr_wav_h

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Common data formats.
#define DR_WAVE_FORMAT_PCM          0x1
#define DR_WAVE_FORMAT_ADPCM        0x2     // Not currently supported.
#define DR_WAVE_FORMAT_IEEE_FLOAT   0x3
#define DR_WAVE_FORMAT_ALAW         0x6
#define DR_WAVE_FORMAT_MULAW        0x7
#define DR_WAVE_FORMAT_EXTENSIBLE   0xFFFE

typedef enum
{
    drwav_seek_origin_start,
    drwav_seek_origin_current
} drwav_seek_origin;

typedef enum
{
    drwav_container_riff,
    drwav_container_w64
} drwav_container;

// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* drwav_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

// Callback for when data needs to be seeked. Offset is always relative to the current position. Return value
// is true on success; fale on failure.
typedef bool (* drwav_seek_proc)(void* pUserData, int offset, drwav_seek_origin origin);

// Structure for internal use. Only used for loaders opened with drwav_open_memory.
typedef struct
{
    const unsigned char* data;
    size_t dataSize;
    size_t currentReadPos;
} drwav__memory_stream;

typedef struct
{
    // The format tag exactly as specified in the wave file's "fmt" chunk. This can be used by applications
    // that require support for data formats not natively supported by dr_wav.
    unsigned short formatTag;

    // The number of channels making up the audio data. When this is set to 1 it is mono, 2 is stereo, etc.
    unsigned short channels;

    // The sample rate. Usually set to something like 44100.
    unsigned int sampleRate;

    // Average bytes per second. You probably don't need this, but it's left here for informational purposes.
    unsigned int avgBytesPerSec;

    // Block align. This is equal to the number of channels * bytes per sample.
    unsigned short blockAlign;

    // Bit's per sample.
    unsigned short bitsPerSample;

    // The size of the extended data. Only used internally for validation, but left here for informational purposes.
    unsigned short extendedSize;

    // The number of valid bits per sample. When <formatTag> is equal to WAVE_FORMAT_EXTENSIBLE, <bitsPerSample>
    // is always rounded up to the nearest multiple of 8. This variable contains information about exactly how
    // many bits a valid per sample. Mainly used for informational purposes.
    unsigned short validBitsPerSample;

    // The channel mask. Not used at the moment.
    unsigned int channelMask;

    // The sub-format, exactly as specified by the wave file.
    unsigned char subFormat[16];

} drwav_fmt;

typedef struct
{
    // A pointer to the function to call when more data is needed.
    drwav_read_proc onRead;

    // A pointer to the function to call when the wav file needs to be seeked.
    drwav_seek_proc onSeek;

    // The user data to pass to callbacks.
    void* pUserData;


    // Whether or not the WAV file is formatted as a standard RIFF file or W64.
    drwav_container container;


    // Structure containing format information exactly as specified by the wav file.
    drwav_fmt fmt;

    // The sample rate. Will be set to something like 44100.
    unsigned int sampleRate;

    // The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc.
    unsigned short channels;

    // The bits per sample. Will be set to somthing like 16, 24, etc.
    unsigned short bitsPerSample;

    // The number of bytes per sample.
    unsigned short bytesPerSample;

    // Equal to fmt.formatTag, or the value specified by fmt.subFormat if fmt.formatTag is equal to 65534 (WAVE_FORMAT_EXTENSIBLE).
    unsigned short translatedFormatTag;

    // The total number of samples making up the audio data. Use <totalSampleCount> * <bytesPerSample> to calculate
    // the required size of a buffer to hold the entire audio data.
    uint64_t totalSampleCount;

    
    // The number of bytes remaining in the data chunk.
    uint64_t bytesRemaining;


    // A hack to avoid a malloc() when opening a decoder with drwav_open_memory().
    drwav__memory_stream memoryStream;

} drwav;


// Initializes a pre-allocated drwav object.
//
// 
bool drwav_init(drwav* pWav, drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData);

// Uninitializes the given drwav object. Use this only for objects initialized with drwav_init().
void drwav_uninit(drwav* pWav);


// Opens a .wav file using the given callbacks.
//
// Returns null on error. Close the loader with drwav_close().
drwav* drwav_open(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData);

// Closes the given drwav object. Use this only for objects created with drwav_open().
void drwav_close(drwav* pWav);


// Reads raw audio data.
//
// This is the lowest level function for reading audio data. It simply reads the given number of
// bytes of the raw internal sample data.
//
// Returns the number of bytes actually read.
size_t drwav_read_raw(drwav* pWav, size_t bytesToRead, void* pBufferOut);

// Reads a chunk of audio data in the native internal format.
//
// This is typically the most efficient way to retrieve audio data, but it does not do any format
// conversions which means you'll need to convert the data manually if required.
//
// If the return value is less than <samplesToRead> it means the end of the file has been reached or
// you have requested more samples than can possibly fit in the output buffer.
//
// This function will only work when sample data is of a fixed size. If you are using an unusual
// format which uses variable sized samples, consider using drwav_read_raw(), but don't combine them.
uint64_t drwav_read(drwav* pWav, uint64_t samplesToRead, void* pBufferOut);

// Seeks to the given sample.
//
// The return value is false if an error occurs, true if successful.
bool drwav_seek_to_sample(drwav* pWav, uint64_t sample);



//// Convertion Utilities ////
#ifndef DR_WAV_NO_CONVERSION_API

// Reads a chunk of audio data and converts it to IEEE 32-bit floating point samples.
//
// Returns the number of samples actually read.
//
// If the return value is less than <samplesToRead> it means the end of the file has been reached.
uint64_t drwav_read_f32(drwav* pWav, uint64_t samplesToRead, float* pBufferOut);

// Low-level function for converting unsigned 8-bit PCM samples to IEEE 32-bit floating point samples.
void drwav_u8_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount);

// Low-level function for converting signed 16-bit PCM samples to IEEE 32-bit floating point samples.
void drwav_s16_to_f32(float* pOut, const int16_t* pIn, size_t sampleCount);

// Low-level function for converting signed 24-bit PCM samples to IEEE 32-bit floating point samples.
void drwav_s24_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount);

// Low-level function for converting signed 32-bit PCM samples to IEEE 32-bit floating point samples.
void drwav_s32_to_f32(float* pOut, const int32_t* pIn, size_t sampleCount);

// Low-level function for converting IEEE 64-bit floating point samples to IEEE 32-bit floating point samples.
void drwav_f64_to_f32(float* pOut, const double* pIn, size_t sampleCount);

// Low-level function for converting A-law samples to IEEE 32-bit floating point samples.
void drwav_alaw_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount);

// Low-level function for converting u-law samples to IEEE 32-bit floating point samples.
void drwav_ulaw_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount);


// Reads a chunk of audio data and converts it to signed 32-bit PCM samples.
//
// Returns the number of samples actually read.
//
// If the return value is less than <samplesToRead> it means the end of the file has been reached.
uint64_t drwav_read_s32(drwav* pWav, uint64_t samplesToRead, int32_t* pBufferOut);

// Low-level function for converting unsigned 8-bit PCM samples to signed 32-bit PCM samples.
void drwav_u8_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount);

// Low-level function for converting signed 16-bit PCM samples to signed 32-bit PCM samples.
void drwav_s16_to_s32(int32_t* pOut, const int16_t* pIn, size_t sampleCount);

// Low-level function for converting signed 24-bit PCM samples to signed 32-bit PCM samples.
void drwav_s24_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount);

// Low-level function for converting IEEE 32-bit floating point samples to signed 32-bit PCM samples.
void drwav_f32_to_s32(int32_t* pOut, const float* pIn, size_t sampleCount);

// Low-level function for converting IEEE 64-bit floating point samples to signed 32-bit PCM samples.
void drwav_f64_to_s32(int32_t* pOut, const double* pIn, size_t sampleCount);

// Low-level function for converting A-law samples to signed 32-bit PCM samples.
void drwav_alaw_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount);

// Low-level function for converting u-law samples to signed 32-bit PCM samples.
void drwav_ulaw_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount);

#endif  //DR_WAV_NO_CONVERSION_API


//// High-Level Convenience Helpers ////

#ifndef DR_WAV_NO_STDIO

// Helper for initializing a wave file using stdio.
//
// This holds the internal FILE object until drwav_uninit() is called. Keep this in mind if you're caching drwav
// objects because the operating system may restrict the number of file handles an application can have open at
// any given time.
bool drwav_init_file(drwav* pWav, const char* filename);

// Helper for opening a wave file using stdio.
//
// This holds the internal FILE object until drwav_close() is called. Keep this in mind if you're caching drwav
// objects because the operating system may restrict the number of file handles an application can have open at
// any given time.
drwav* drwav_open_file(const char* filename);

#endif  //DR_WAV_NO_STDIO

// Helper for initializing a file from a pre-allocated memory buffer.
//
// This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
// the lifetime of the drwav object.
//
// The buffer should contain the contents of the entire wave file, not just the sample data.
bool drwav_init_memory(drwav* pWav, const void* data, size_t dataSize);

// Helper for opening a file from a pre-allocated memory buffer.
//
// This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
// the lifetime of the drwav object.
//
// The buffer should contain the contents of the entire wave file, not just the sample data.
drwav* drwav_open_memory(const void* data, size_t dataSize);



#ifndef DR_WAV_NO_CONVERSION_API
// Opens and reads a wav file in a single operation.
float*   drwav_open_and_read_f32(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount);
int32_t* drwav_open_and_read_s32(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount);
#ifndef DR_WAV_NO_STDIO
// Opens an decodes a wav file in a single operation.
float*   drwav_open_and_read_file_f32(const char* filename, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount);
int32_t* drwav_open_and_read_file_s32(const char* filename, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount);
#endif

// Opens an decodes a wav file from a block of memory in a single operation.
float*   drwav_open_and_read_memory_f32(const void* data, size_t dataSize, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount);
int32_t* drwav_open_and_read_memory_s32(const void* data, size_t dataSize, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount);
#endif

// Frees data that was allocated internally by dr_wav.
void drwav_free(void* pDataReturnedByOpenAndRead);

#ifdef __cplusplus
}
#endif
#endif  // dr_wav_h


/////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
/////////////////////////////////////////////////////

#ifdef DR_WAV_IMPLEMENTATION
#include <stdlib.h>
#include <string.h> // For memcpy()
#include <limits.h>
#include <assert.h>

#ifndef DR_WAV_NO_STDIO
#include <stdio.h>
#endif

static const uint8_t drwavGUID_W64_RIFF[16] = {0x72,0x69,0x66,0x66, 0x2E,0x91, 0xCF,0x11, 0xA5,0xD6, 0x28,0xDB,0x04,0xC1,0x00,0x00};    // 66666972-912E-11CF-A5D6-28DB04C10000
static const uint8_t drwavGUID_W64_WAVE[16] = {0x77,0x61,0x76,0x65, 0xF3,0xAC, 0xD3,0x11, 0x8C,0xD1, 0x00,0xC0,0x4F,0x8E,0xDB,0x8A};    // 65766177-ACF3-11D3-8CD1-00C04F8EDB8A
static const uint8_t drwavGUID_W64_FMT [16] = {0x66,0x6D,0x74,0x20, 0xF3,0xAC, 0xD3,0x11, 0x8C,0xD1, 0x00,0xC0,0x4F,0x8E,0xDB,0x8A};    // 20746D66-ACF3-11D3-8CD1-00C04F8EDB8A
static const uint8_t drwavGUID_W64_DATA[16] = {0x64,0x61,0x74,0x61, 0xF3,0xAC, 0xD3,0x11, 0x8C,0xD1, 0x00,0xC0,0x4F,0x8E,0xDB,0x8A};    // 61746164-ACF3-11D3-8CD1-00C04F8EDB8A

static bool drwav__guid_equal(const uint8_t a[16], const uint8_t b[16])
{
    const uint32_t* a32 = (const uint32_t*)a;
    const uint32_t* b32 = (const uint32_t*)b;

    return
        a32[0] == b32[0] &&
        a32[1] == b32[1] &&
        a32[2] == b32[2] &&
        a32[3] == b32[3];
}

static bool drwav__fourcc_equal(const unsigned char* a, const char* b)
{
    return
        a[0] == b[0] &&
        a[1] == b[1] &&
        a[2] == b[2] &&
        a[3] == b[3];
}




static int drwav__is_little_endian()
{
    int n = 1;
    return (*(char*)&n) == 1;
}

static unsigned short drwav__bytes_to_u16(const unsigned char* data)
{
    if (drwav__is_little_endian()) {
        return (data[0] << 0) | (data[1] << 8);
    } else {
        return (data[1] << 0) | (data[0] << 8);
    }
}

static unsigned int drwav__bytes_to_u32(const unsigned char* data)
{
    if (drwav__is_little_endian()) {
        return (data[0] << 0) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    } else {
        return (data[3] << 0) | (data[2] << 8) | (data[1] << 16) | (data[0] << 24);
    }
}

static uint64_t drwav__bytes_to_u64(const unsigned char* data)
{
    if (drwav__is_little_endian()) {
        return
            ((uint64_t)data[0] <<  0ULL) | ((uint64_t)data[1] <<  8ULL) | ((uint64_t)data[2] << 16ULL) | ((uint64_t)data[3] << 24ULL) |
            ((uint64_t)data[4] << 32ULL) | ((uint64_t)data[5] << 40ULL) | ((uint64_t)data[6] << 48ULL) | ((uint64_t)data[7] << 56ULL);
    } else {
        return
            ((uint64_t)data[7] <<  0ULL) | ((uint64_t)data[6] <<  8ULL) | ((uint64_t)data[5] << 16ULL) | ((uint64_t)data[4] << 24ULL) |
            ((uint64_t)data[3] << 32ULL) | ((uint64_t)data[2] << 40ULL) | ((uint64_t)data[1] << 48ULL) | ((uint64_t)data[0] << 56ULL);
    }
}

static void drwav__bytes_to_guid(const unsigned char* data, uint8_t* guid)
{
    for (int i = 0; i < 16; ++i) {
        guid[i] = data[i];
    }
}


typedef struct
{
    union
    {
        uint8_t fourcc[4];
        uint8_t guid[16];
    } id;

    // The size in bytes of the chunk.
    uint64_t sizeInBytes;

    // RIFF = 2 byte alignment.
    // W64  = 8 byte alignment.
    unsigned int paddingSize;

} drwav__chunk_header;

static bool drwav__read_chunk_header(drwav_read_proc onRead, void* pUserData, drwav_container container, drwav__chunk_header* pHeaderOut)
{
    if (container == drwav_container_riff) {
        if (onRead(pUserData, pHeaderOut->id.fourcc, 4) != 4) {
            return false;
        }

        unsigned char sizeInBytes[4];
        if (onRead(pUserData, sizeInBytes, 4) != 4) {
            return false;
        }

        pHeaderOut->sizeInBytes = drwav__bytes_to_u32(sizeInBytes);
        pHeaderOut->paddingSize = pHeaderOut->sizeInBytes % 2;
    } else {
        if (onRead(pUserData, pHeaderOut->id.guid, 16) != 16) {
            return false;
        }

        unsigned char sizeInBytes[8];
        if (onRead(pUserData, sizeInBytes, 8) != 8) {
            return false;
        }

        pHeaderOut->sizeInBytes = drwav__bytes_to_u64(sizeInBytes) - 24;    // <-- Subtract 24 because w64 includes the size of the header.
        pHeaderOut->paddingSize = pHeaderOut->sizeInBytes % 8;
    }

    return true;
}


static bool drwav__read_fmt(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData, drwav_container container, drwav_fmt* fmtOut)
{
    drwav__chunk_header header;
    if (!drwav__read_chunk_header(onRead, pUserData, container, &header)) {
        return false;
    }

    // Validation.
    if (container == drwav_container_riff) {
        if (!drwav__fourcc_equal(header.id.fourcc, "fmt ")) {
            return false;
        }
    } else {
        if (!drwav__guid_equal(header.id.guid, drwavGUID_W64_FMT)) {
            return false;
        }
    }


    unsigned char fmt[16];
    if (onRead(pUserData, fmt, sizeof(fmt)) != sizeof(fmt)) {
        return false;
    }

    fmtOut->formatTag      = drwav__bytes_to_u16(fmt + 0);
    fmtOut->channels       = drwav__bytes_to_u16(fmt + 2);
    fmtOut->sampleRate     = drwav__bytes_to_u32(fmt + 4);
    fmtOut->avgBytesPerSec = drwav__bytes_to_u32(fmt + 8);
    fmtOut->blockAlign     = drwav__bytes_to_u16(fmt + 12);
    fmtOut->bitsPerSample  = drwav__bytes_to_u16(fmt + 14);

    fmtOut->extendedSize       = 0;
    fmtOut->validBitsPerSample = 0;
    fmtOut->channelMask        = 0;
    memset(fmtOut->subFormat, 0, sizeof(fmtOut->subFormat));

    if (header.sizeInBytes > 16) {
        unsigned char fmt_cbSize[2];
        if (onRead(pUserData, fmt_cbSize, sizeof(fmt_cbSize)) != sizeof(fmt_cbSize)) {
            return false;    // Expecting more data.
        }

        int bytesReadSoFar = 18;

        fmtOut->extendedSize = drwav__bytes_to_u16(fmt_cbSize);
        if (fmtOut->extendedSize > 0) {
            if (fmtOut->extendedSize != 22) {
                return false;   // The extended size should be equal to 22.
            }

            unsigned char fmtext[22];
            if (onRead(pUserData, fmtext, sizeof(fmtext)) != sizeof(fmtext)) {
                return false;    // Expecting more data.
            }

            fmtOut->validBitsPerSample = drwav__bytes_to_u16(fmtext + 0);
            fmtOut->channelMask        = drwav__bytes_to_u32(fmtext + 2);
            drwav__bytes_to_guid(fmtext + 6, fmtOut->subFormat);

            bytesReadSoFar += 22;
        }

        // Seek past any leftover bytes. For w64 the leftover will be defined based on the chunk size.
        if (!onSeek(pUserData, (int)(header.sizeInBytes - bytesReadSoFar), drwav_seek_origin_current)) {
            return false;
        }
    }

    if (header.paddingSize > 0) {
        if (!onSeek(pUserData, header.paddingSize, drwav_seek_origin_current)) {
            return false;
        }
    }

    return true;
}


#ifndef DR_WAV_NO_STDIO
static size_t drwav__on_read_stdio(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    return fread(pBufferOut, 1, bytesToRead, (FILE*)pUserData);
}

static bool drwav__on_seek_stdio(void* pUserData, int offset, drwav_seek_origin origin)
{
    return fseek((FILE*)pUserData, offset, (origin == drwav_seek_origin_current) ? SEEK_CUR : SEEK_SET) == 0;
}

bool drwav_init_file(drwav* pWav, const char* filename)
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

    return drwav_init(pWav, drwav__on_read_stdio, drwav__on_seek_stdio, (void*)pFile);
}

drwav* drwav_open_file(const char* filename)
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

    drwav* pWav = drwav_open(drwav__on_read_stdio, drwav__on_seek_stdio, (void*)pFile);
    if (pWav == NULL) {
        fclose(pFile);
        return NULL;
    }

    return pWav;
}
#endif  //DR_WAV_NO_STDIO


static size_t drwav__on_read_memory(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    drwav__memory_stream* memory = (drwav__memory_stream*)pUserData;
    assert(memory != NULL);
    assert(memory->dataSize >= memory->currentReadPos);

    size_t bytesRemaining = memory->dataSize - memory->currentReadPos;
    if (bytesToRead > bytesRemaining) {
        bytesToRead = bytesRemaining;
    }

    if (bytesToRead > 0) {
        memcpy(pBufferOut, memory->data + memory->currentReadPos, bytesToRead);
        memory->currentReadPos += bytesToRead;
    }

    return bytesToRead;
}

static bool drwav__on_seek_memory(void* pUserData, int offset, drwav_seek_origin origin)
{
    drwav__memory_stream* memory = (drwav__memory_stream*)pUserData;
    assert(memory != NULL);

    if (origin == drwav_seek_origin_current) {
        if (offset > 0) {
            if (memory->currentReadPos + offset > memory->dataSize) {
                offset = (int)(memory->dataSize - memory->currentReadPos);  // Trying to seek too far forward.
            }
        } else {
            if (memory->currentReadPos < (size_t)-offset) {
                offset = -(int)memory->currentReadPos;  // Trying to seek too far backwards.
            }
        }
    } else {
        if ((uint32_t)offset <= memory->dataSize) {
            memory->currentReadPos = offset;
        } else {
            memory->currentReadPos = memory->dataSize;  // Trying to seek too far forward.
        }
    }

    // This will never underflow thanks to the clamps above.
    memory->currentReadPos += offset;
    return true;
}

bool drwav_init_memory(drwav* pWav, const void* data, size_t dataSize)
{
    drwav__memory_stream memoryStream;
    memoryStream.data = (const unsigned char*)data;
    memoryStream.dataSize = dataSize;
    memoryStream.currentReadPos = 0;

    if (!drwav_init(pWav, drwav__on_read_memory, drwav__on_seek_memory, (void*)&memoryStream)) {
        return false;
    }

    pWav->memoryStream = memoryStream;
    pWav->pUserData = &pWav->memoryStream;
    return true;
}

drwav* drwav_open_memory(const void* data, size_t dataSize)
{
    drwav__memory_stream memoryStream;
    memoryStream.data = (const unsigned char*)data;
    memoryStream.dataSize = dataSize;
    memoryStream.currentReadPos = 0;

    drwav* pWav = drwav_open(drwav__on_read_memory, drwav__on_seek_memory, (void*)&memoryStream);
    if (pWav == NULL) {
        return NULL;
    }

    pWav->memoryStream = memoryStream;
    pWav->pUserData = &pWav->memoryStream;
    return pWav;
}


bool drwav_init(drwav* pWav, drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData)
{
    if (onRead == NULL || onSeek == NULL) {
        return false;
    }


    // The first 4 bytes should be the RIFF identifier.
    unsigned char riff[4];
    if (onRead(pUserData, riff, sizeof(riff)) != sizeof(riff)) {
        return false;    // Failed to read data.
    }

    // The first 4 bytes can be used to identify the container. For RIFF files it will start with "RIFF" and for
    // w64 it will start with "riff".
    if (drwav__fourcc_equal(riff, "RIFF")) {
        pWav->container = drwav_container_riff;
    } else if (drwav__fourcc_equal(riff, "riff")) {
        pWav->container = drwav_container_w64;

        // Check the rest of the GUID for validity.
        uint8_t riff2[12];
        if (onRead(pUserData, riff2, sizeof(riff2)) != sizeof(riff2)) {
            return false;
        }

        for (int i = 0; i < 12; ++i) {
            if (riff2[i] != drwavGUID_W64_RIFF[i+4]) {
                return false;
            }
        }
    } else {
        return false;   // Unknown or unsupported container.
    }


    if (pWav->container == drwav_container_riff) {
        // RIFF/WAVE
        unsigned char chunkSizeBytes[4];
        if (onRead(pUserData, chunkSizeBytes, sizeof(chunkSizeBytes)) != sizeof(chunkSizeBytes)) {
            return false;
        }

        unsigned int chunkSize = drwav__bytes_to_u32(chunkSizeBytes);
        if (chunkSize < 36) {
            return false;    // Chunk size should always be at least 36 bytes.
        }

        unsigned char wave[4];
        if (onRead(pUserData, wave, sizeof(wave)) != sizeof(wave)) {
            return false;
        }

        if (!drwav__fourcc_equal(wave, "WAVE")) {
            return false;    // Expecting "WAVE".
        }
    } else {
        // W64
        unsigned char chunkSize[8];
        if (onRead(pUserData, chunkSize, sizeof(chunkSize)) != sizeof(chunkSize)) {
            return false;
        }

        if (drwav__bytes_to_u64(chunkSize) < 84) {
            return false;
        }

        uint8_t wave[16];
        if (onRead(pUserData, wave, sizeof(wave)) != sizeof(wave)) {
            return false;
        }

        if (!drwav__guid_equal(wave, drwavGUID_W64_WAVE)) {
            return false;
        }
    }


    // The next 24 bytes should be the "fmt " chunk.
    drwav_fmt fmt;
    if (!drwav__read_fmt(onRead, onSeek, pUserData, pWav->container, &fmt)) {
        return false;    // Failed to read the "fmt " chunk.
    }


    // Translate the internal format.
    unsigned short translatedFormatTag = fmt.formatTag;
    if (translatedFormatTag == DR_WAVE_FORMAT_EXTENSIBLE) {
        translatedFormatTag = drwav__bytes_to_u16(fmt.subFormat + 0);
    }


    // The next chunk we care about is the "data" chunk. This is not necessarily the next chunk so we'll need to loop.
    uint64_t dataSize;
    for (;;)
    {
        drwav__chunk_header header;
        if (!drwav__read_chunk_header(onRead, pUserData, pWav->container, &header)) {
            return false;
        }

        dataSize = header.sizeInBytes;
        if (pWav->container == drwav_container_riff) {
            if (drwav__fourcc_equal(header.id.fourcc, "data")) {
                break;
            }
        } else {
            if (drwav__guid_equal(header.id.guid, drwavGUID_W64_DATA)) {
                break;
            }
        }

        // If we get here it means we didn't find the "data" chunk. Seek past it.

        // Make sure we seek past the padding.
        dataSize += header.paddingSize;

        uint64_t bytesRemainingToSeek = dataSize;
        while (bytesRemainingToSeek > 0) {
            if (bytesRemainingToSeek > 0x7FFFFFFF) {
                if (!onSeek(pUserData, 0x7FFFFFFF, drwav_seek_origin_current)) {
                    return false;
                }
                bytesRemainingToSeek -= 0x7FFFFFFF;
            } else {
                if (!onSeek(pUserData, (int)bytesRemainingToSeek, drwav_seek_origin_current)) {
                    return false;
                }
                bytesRemainingToSeek = 0;
            }
        }
    }

    // At this point we should be sitting on the first byte of the raw audio data.

    pWav->onRead              = onRead;
    pWav->onSeek              = onSeek;
    pWav->pUserData           = pUserData;
    pWav->fmt                 = fmt;
    pWav->sampleRate          = fmt.sampleRate;
    pWav->channels            = fmt.channels;
    pWav->bitsPerSample       = fmt.bitsPerSample;
    pWav->bytesPerSample      = (unsigned int)(fmt.blockAlign / fmt.channels);
    pWav->translatedFormatTag = translatedFormatTag;
    pWav->totalSampleCount    = dataSize / pWav->bytesPerSample;
    pWav->bytesRemaining      = dataSize;

    return true;
}

void drwav_uninit(drwav* pWav)
{
    if (pWav == NULL) {
        return;
    }

#ifndef DR_WAV_NO_STDIO
    // If we opened the file with drwav_open_file() we will want to close the file handle. We can know whether or not drwav_open_file()
    // was used by looking at the onRead and onSeek callbacks.
    if (pWav->onRead == drwav__on_read_stdio) {
        fclose((FILE*)pWav->pUserData);
    }
#endif
}


drwav* drwav_open(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData)
{
    drwav* pWav = (drwav*)malloc(sizeof(*pWav));
    if (pWav == NULL) {
        return NULL;
    }

    if (!drwav_init(pWav, onRead, onSeek, pUserData)) {
        free(pWav);
        return NULL;
    }

    return pWav;
}

void drwav_close(drwav* pWav)
{
    drwav_uninit(pWav);
    free(pWav);
}


size_t drwav_read_raw(drwav* pWav, size_t bytesToRead, void* pBufferOut)
{
    if (pWav == NULL || bytesToRead == 0 || pBufferOut == NULL) {
        return 0;
    }

    if (bytesToRead > pWav->bytesRemaining) {
        bytesToRead = (size_t)pWav->bytesRemaining;
    }

    size_t bytesRead = pWav->onRead(pWav->pUserData, pBufferOut, bytesToRead);

    pWav->bytesRemaining -= bytesRead;
    return bytesRead;
}

uint64_t drwav_read(drwav* pWav, uint64_t samplesToRead, void* pBufferOut)
{
    if (pWav == NULL || samplesToRead == 0 || pBufferOut == NULL) {
        return 0;
    }

    // Don't try to read more samples than can potentially fit in the output buffer.
    if (samplesToRead * pWav->bytesPerSample > SIZE_MAX) {
        samplesToRead = SIZE_MAX / pWav->bytesPerSample;
    }

    size_t bytesRead = drwav_read_raw(pWav, (size_t)(samplesToRead * pWav->bytesPerSample), pBufferOut);
    return bytesRead / pWav->bytesPerSample;
}

bool drwav_seek_to_sample(drwav* pWav, uint64_t sample)
{
    // Seeking should be compatible with wave files > 2GB.

    if (pWav == NULL || pWav->onSeek == NULL) {
        return 0;
    }

    // If there are no samples, just return true without doing anything.
    if (pWav->totalSampleCount == 0) {
        return 1;
    }

    // Make sure the sample is clamped.
    if (sample >= pWav->totalSampleCount) {
        sample = pWav->totalSampleCount - 1;
    }


    uint64_t totalSizeInBytes = pWav->totalSampleCount * pWav->bytesPerSample;
    assert(totalSizeInBytes >= pWav->bytesRemaining);

    uint64_t currentBytePos = totalSizeInBytes - pWav->bytesRemaining;
    uint64_t targetBytePos  = sample * pWav->bytesPerSample;

    uint64_t offset;
    int direction;
    if (currentBytePos < targetBytePos) {
        // Offset forward.
        offset = targetBytePos - currentBytePos;
        direction = 1;
    } else {
        // Offset backwards.
        offset = currentBytePos - targetBytePos;
        direction = -1;
    }

    while (offset > 0)
    {
        int offset32 = ((offset > INT_MAX) ? INT_MAX : (int)offset);
        pWav->onSeek(pWav->pUserData, offset32 * direction, drwav_seek_origin_current);

        pWav->bytesRemaining -= (offset32 * direction);
        offset -= offset32;
    }

    return 1;
}


#ifndef DR_WAV_NO_CONVERSION_API
#define drwav_min(a, b) (((a) < (b)) ? (a) : (b))

static int drwav__pcm_to_f32(float* pOut, const unsigned char* pIn, size_t sampleCount, unsigned short bytesPerSample)
{
    if (pOut == NULL || pIn == NULL) {
        return 0;
    }

    // Special case for 8-bit sample data because it's treated as unsigned.
    if (bytesPerSample == 1) {
        drwav_u8_to_f32(pOut, pIn, sampleCount);
        return 1;
    }


    // Slightly more optimal implementation for common formats.
    if (bytesPerSample == 2) {
        drwav_s16_to_f32(pOut, (const int16_t*)pIn, sampleCount);
        return 1;
    }
    if (bytesPerSample == 3) {
        drwav_s24_to_f32(pOut, pIn, sampleCount);
        return 1;
    }
    if (bytesPerSample == 4) {
        drwav_s32_to_f32(pOut, (const int32_t*)pIn, sampleCount);
        return 1;
    }


    // Generic, slow converter.
    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        unsigned int sample = 0;
        unsigned int shift  = (8 - bytesPerSample) * 8;
        for (unsigned short j = 0; j < bytesPerSample && j < 4; ++j) {
            sample |= (unsigned int)(pIn[j]) << shift;
            shift  += 8;
        }

        pIn += bytesPerSample;
        *pOut++ = (float)((int)sample / 2147483648.0);
    }

    return 1;
}

static int drwav__ieee_to_f32(float* pOut, const unsigned char* pIn, size_t sampleCount, unsigned short bytesPerSample)
{
    if (pOut == NULL || pIn == NULL) {
        return 0;
    }

    if (bytesPerSample == 4) {
        for (unsigned int i = 0; i < sampleCount; ++i) {
            *pOut++ = ((float*)pIn)[i];
        }
        return 1;
    } else {
        drwav_f64_to_f32(pOut, (double*)pIn, sampleCount);
        return 1;
    }
}


uint64_t drwav_read_f32(drwav* pWav, uint64_t samplesToRead, float* pBufferOut)
{
    if (pWav == NULL || samplesToRead == 0 || pBufferOut == NULL) {
        return 0;
    }

    // Fast path.
    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT && pWav->bytesPerSample == 4) {
        return drwav_read(pWav, samplesToRead, pBufferOut);
    }


    // Don't try to read more samples than can potentially fit in the output buffer.
    if (samplesToRead * sizeof(float) > SIZE_MAX) {
        samplesToRead = SIZE_MAX / sizeof(float);
    }


    // Slow path. Need to read and convert.
    uint64_t totalSamplesRead = 0;

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_PCM)
    {
        unsigned char sampleData[4096];
        while (samplesToRead > 0)
        {
            uint64_t samplesRead = drwav_read(pWav, drwav_min(samplesToRead, sizeof(sampleData)/pWav->bytesPerSample), sampleData);
            if (samplesRead == 0) {
                break;
            }

            drwav__pcm_to_f32(pBufferOut, sampleData, (size_t)samplesRead, pWav->bytesPerSample);
            pBufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT)
    {
        unsigned char sampleData[4096];
        while (samplesToRead > 0)
        {
            uint64_t samplesRead = drwav_read(pWav, drwav_min(samplesToRead, sizeof(sampleData)/pWav->bytesPerSample), sampleData);
            if (samplesRead == 0) {
                break;
            }

            drwav__ieee_to_f32(pBufferOut, sampleData, (size_t)samplesRead, pWav->bytesPerSample);
            pBufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ALAW)
    {
        unsigned char sampleData[4096];
        while (samplesToRead > 0)
        {
            uint64_t samplesRead = drwav_read(pWav, drwav_min(samplesToRead, sizeof(sampleData)/pWav->bytesPerSample), sampleData);
            if (samplesRead == 0) {
                break;
            }

            drwav_alaw_to_f32(pBufferOut, sampleData, (size_t)samplesRead);
            pBufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_MULAW)
    {
        unsigned char sampleData[4096];
        while (samplesToRead > 0)
        {
            uint64_t samplesRead = drwav_read(pWav, drwav_min(samplesToRead, sizeof(sampleData)/pWav->bytesPerSample), sampleData);
            if (samplesRead == 0) {
                break;
            }

            drwav_ulaw_to_f32(pBufferOut, sampleData, (size_t)samplesRead);
            pBufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    return totalSamplesRead;
}

void drwav_u8_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        *pOut++ = (pIn[i] / 255.0f) * 2 - 1;
    }
}

void drwav_s16_to_f32(float* pOut, const int16_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        *pOut++ = pIn[i] / 32768.0f;
    }
}

void drwav_s24_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        unsigned int s0 = pIn[i*3 + 0];
        unsigned int s1 = pIn[i*3 + 1];
        unsigned int s2 = pIn[i*3 + 2];

        int sample32 = (int)((s0 << 8) | (s1 << 16) | (s2 << 24));
        *pOut++ = (float)(sample32 / 2147483648.0);
    }
}

void drwav_s32_to_f32(float* pOut, const int32_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        *pOut++ = (float)(pIn[i] / 2147483648.0);
    }
}

void drwav_f64_to_f32(float* pOut, const double* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        *pOut++ = (float)pIn[i];
    }
}

void drwav_alaw_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i)
    {
        const unsigned char a = pIn[i] ^ 0x55;

        int t = (a & 0x0F) << 4;

        int s = ((unsigned int)a & 0x70) >> 4;
        switch (s)
        {
            case 0:
            {
                t += 8;
            } break;

            default:
            {
                t += 0x108;
                t <<= (s - 1);
            } break;
        }

        if ((a & 0x80) == 0) {
            t = -t;
        }

        *pOut++ = t / 32768.0f;
    }
}

void drwav_ulaw_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        const unsigned char u = ~pIn[i];

        int t = (((u & 0x0F) << 3) + 0x84) << (((unsigned int)u & 0x70) >> 4);
        if (u & 0x80) {
            t = 0x84 - t;
        } else {
            t = t - 0x84;
        }

        *pOut++ = t / 32768.0f;
    }
}



static int drwav__pcm_to_s32(int32_t* pOut, const unsigned char* pIn, size_t totalSampleCount, unsigned short bytesPerSample)
{
    if (pOut == NULL || pIn == NULL) {
        return 0;
    }

    // Special case for 8-bit sample data because it's treated as unsigned.
    if (bytesPerSample == 1) {
        drwav_u8_to_s32(pOut, pIn, totalSampleCount);
        return 1;
    }


    // Slightly more optimal implementation for common formats.
    if (bytesPerSample == 2) {
        drwav_s16_to_s32(pOut, (const int16_t*)pIn, totalSampleCount);
        return 1;
    }
    if (bytesPerSample == 3) {
        drwav_s24_to_s32(pOut, pIn, totalSampleCount);
        return 1;
    }
    if (bytesPerSample == 4) {
        for (unsigned int i = 0; i < totalSampleCount; ++i) {
           *pOut++ = ((int32_t*)pIn)[i];
        }
        return 1;
    }


    // Generic, slow converter.
    for (unsigned int i = 0; i < totalSampleCount; ++i)
    {
        unsigned int sample = 0;
        unsigned int shift  = (8 - bytesPerSample) * 8;
        for (unsigned short j = 0; j < bytesPerSample && j < 4; ++j) {
            sample |= (unsigned int)(pIn[j]) << shift;
            shift  += 8;
        }

        pIn += bytesPerSample;
        *pOut++ = sample;
    }

    return 1;
}

static int drwav__ieee_to_s32(int32_t* pOut, const unsigned char* pIn, size_t totalSampleCount, unsigned short bytesPerSample)
{
    if (pOut == NULL || pIn == NULL) {
        return 0;
    }

    if (bytesPerSample == 4) {
        drwav_f32_to_s32(pOut, (float*)pIn, totalSampleCount);
        return 1;
    } else {
        drwav_f64_to_s32(pOut, (double*)pIn, totalSampleCount);
        return 1;
    }
}

uint64_t drwav_read_s32(drwav* pWav, uint64_t samplesToRead, int32_t* pBufferOut)
{
    if (pWav == NULL || samplesToRead == 0 || pBufferOut == NULL) {
        return 0;
    }

    // Fast path.
    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_PCM && pWav->bytesPerSample == 4) {
        return drwav_read(pWav, samplesToRead, pBufferOut);
    }


    // Don't try to read more samples than can potentially fit in the output buffer.
    if (samplesToRead * sizeof(int32_t) > SIZE_MAX) {
        samplesToRead = SIZE_MAX / sizeof(int32_t);
    }


    // Slow path. Need to read and convert.
    uint64_t totalSamplesRead = 0;

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_PCM)
    {
        unsigned char sampleData[4096];
        while (samplesToRead > 0)
        {
            uint64_t samplesRead = drwav_read(pWav, drwav_min(samplesToRead, sizeof(sampleData)/pWav->bytesPerSample), sampleData);
            if (samplesRead == 0) {
                break;
            }

            drwav__pcm_to_s32(pBufferOut, sampleData, (size_t)samplesRead, pWav->bytesPerSample);
            pBufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT)
    {
        unsigned char sampleData[4096];
        while (samplesToRead > 0)
        {
            uint64_t samplesRead = drwav_read(pWav, drwav_min(samplesToRead, sizeof(sampleData)/pWav->bytesPerSample), sampleData);
            if (samplesRead == 0) {
                break;
            }

            drwav__ieee_to_s32(pBufferOut, sampleData, (size_t)samplesRead, pWav->bytesPerSample);
            pBufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ALAW)
    {
        unsigned char sampleData[4096];
        while (samplesToRead > 0)
        {
            uint64_t samplesRead = drwav_read(pWav, drwav_min(samplesToRead, sizeof(sampleData)/pWav->bytesPerSample), sampleData);
            if (samplesRead == 0) {
                break;
            }

            drwav_alaw_to_s32(pBufferOut, sampleData, (size_t)samplesRead);
            pBufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_MULAW)
    {
        unsigned char sampleData[4096];
        while (samplesToRead > 0)
        {
            uint64_t samplesRead = drwav_read(pWav, drwav_min(samplesToRead, sizeof(sampleData)/pWav->bytesPerSample), sampleData);
            if (samplesRead == 0) {
                break;
            }

            drwav_ulaw_to_s32(pBufferOut, sampleData, (size_t)samplesRead);
            pBufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    return totalSamplesRead;
}

void drwav_u8_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        *pOut++ = ((int)pIn[i] - 128) << 24;
    }
}

void drwav_s16_to_s32(int32_t* pOut, const int16_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        *pOut++ = pIn[i] << 16;
    }
}

void drwav_s24_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        unsigned int s0 = pIn[i*3 + 0];
        unsigned int s1 = pIn[i*3 + 1];
        unsigned int s2 = pIn[i*3 + 2];

        int32_t sample32 = (int32_t)((s0 << 8) | (s1 << 16) | (s2 << 24));
        *pOut++ = sample32;
    }
}

void drwav_f32_to_s32(int32_t* pOut, const float* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        *pOut++ = (int32_t)(2147483648.0 * pIn[i]);
    }
}

void drwav_f64_to_s32(int32_t* pOut, const double* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i) {
        *pOut++ = (int32_t)(2147483648.0 * pIn[i]);
    }
}

void drwav_alaw_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (size_t i = 0; i < sampleCount; ++i)
    {
        const unsigned char a = pIn[i] ^ 0x55;

        int t = (a & 0x0F) << 4;

        int s = ((unsigned int)a & 0x70) >> 4;
        switch (s)
        {
            case 0:
            {
                t += 8;
            } break;

            default:
            {
                t += 0x108;
                t <<= (s - 1);
            } break;
        }

        if ((a & 0x80) == 0) {
            t = -t;
        }

        *pOut++ = t << 16;
    }
}

void drwav_ulaw_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount)
{
    if (pOut == NULL || pIn == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        const unsigned char u = ~pIn[i];

        int t = (((u & 0x0F) << 3) + 0x84) << (((unsigned int)u & 0x70) >> 4);
        if (u & 0x80) {
            t = 0x84 - t;
        } else {
            t = t - 0x84;
        }

        *pOut++ = t << 16;
    }
}




float* drwav__read_and_close_f32(drwav* pWav, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    assert(pWav != NULL);

    uint64_t sampleDataSize = pWav->totalSampleCount * sizeof(float);
    if (sampleDataSize > SIZE_MAX) {
        drwav_uninit(pWav);
        return NULL;    // File's too big.
    }

    float* pSampleData = (float*)malloc((size_t)(pWav->totalSampleCount * sizeof(float)));    // <-- Safe cast due to the check above.
    if (pSampleData == NULL) {
        drwav_uninit(pWav);
        return NULL;    // Failed to allocate memory.
    }

    uint64_t samplesRead = drwav_read_f32(pWav, (size_t)pWav->totalSampleCount, pSampleData);
    if (samplesRead != pWav->totalSampleCount) {
        free(pSampleData);
        drwav_uninit(pWav);
        return NULL;    // There was an error reading the samples.
    }

    drwav_uninit(pWav);

    if (sampleRate) *sampleRate = pWav->sampleRate;
    if (channels) *channels = pWav->channels;
    if (totalSampleCount) *totalSampleCount = pWav->totalSampleCount;
    return pSampleData;
}

int32_t* drwav__read_and_close_s32(drwav* pWav, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    assert(pWav != NULL);

    uint64_t sampleDataSize = pWav->totalSampleCount * sizeof(int32_t);
    if (sampleDataSize > SIZE_MAX) {
        drwav_uninit(pWav);
        return NULL;    // File's too big.
    }

    int32_t* pSampleData = (int32_t*)malloc((size_t)(pWav->totalSampleCount * sizeof(int32_t)));    // <-- Safe cast due to the check above.
    if (pSampleData == NULL) {
        drwav_uninit(pWav);
        return NULL;    // Failed to allocate memory.
    }

    uint64_t samplesRead = drwav_read_s32(pWav, (size_t)pWav->totalSampleCount, pSampleData);
    if (samplesRead != pWav->totalSampleCount) {
        free(pSampleData);
        drwav_uninit(pWav);
        return NULL;    // There was an error reading the samples.
    }

    drwav_uninit(pWav);

    if (sampleRate) *sampleRate = pWav->sampleRate;
    if (channels) *channels = pWav->channels;
    if (totalSampleCount) *totalSampleCount = pWav->totalSampleCount;
    return pSampleData;
}



float* drwav_open_and_read_f32(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drwav wav;
    if (!drwav_init(&wav, onRead, onSeek, pUserData)) {
        return NULL;
    }

    return drwav__read_and_close_f32(&wav, sampleRate, channels, totalSampleCount);
}

int32_t* drwav_open_and_read_s32(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drwav wav;
    if (!drwav_init(&wav, onRead, onSeek, pUserData)) {
        return NULL;
    }

    return drwav__read_and_close_s32(&wav, sampleRate, channels, totalSampleCount);
}

#ifndef DR_WAV_NO_STDIO
float* drwav_open_and_read_file_f32(const char* filename, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drwav wav;
    if (!drwav_init_file(&wav, filename)) {
        return NULL;
    }

    return drwav__read_and_close_f32(&wav, sampleRate, channels, totalSampleCount);
}

int32_t* drwav_open_and_read_file_s32(const char* filename, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drwav wav;
    if (!drwav_init_file(&wav, filename)) {
        return NULL;
    }

    return drwav__read_and_close_s32(&wav, sampleRate, channels, totalSampleCount);
}
#endif

float* drwav_open_and_read_memory_f32(const void* data, size_t dataSize, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drwav wav;
    if (!drwav_init_memory(&wav, data, dataSize)) {
        return NULL;
    }

    return drwav__read_and_close_f32(&wav, sampleRate, channels, totalSampleCount);
}

int32_t* drwav_open_and_read_memory_s32(const void* data, size_t dataSize, unsigned int* sampleRate, unsigned int* channels, uint64_t* totalSampleCount)
{
    if (sampleRate) *sampleRate = 0;
    if (channels) *channels = 0;
    if (totalSampleCount) *totalSampleCount = 0;

    drwav wav;
    if (!drwav_init_memory(&wav, data, dataSize)) {
        return NULL;
    }

    return drwav__read_and_close_s32(&wav, sampleRate, channels, totalSampleCount);
}
#endif  //DR_WAV_NO_CONVERSION_API


void drwav_free(void* pDataReturnedByOpenAndRead)
{
    free(pDataReturnedByOpenAndRead);
}

#endif  //DR_WAV_IMPLEMENTATION


// REVISION HISTORY
//
// v0.4b - 2016-09-18
//   - Fixed a typo in documentation.
//
// v0.4a - 2016-09-18
//   - Fixed a typo.
//   - Change date format to ISO 8601 (YYYY-MM-DD)
//
// v0.4 - 2016-07-13
//   - API CHANGE. Make onSeek consistent with dr_flac.
//   - API CHANGE. Rename drwav_seek() to drwav_seek_to_sample() for clarity and consistency with dr_flac.
//   - Added support for Sony Wave64.
//
// v0.3a - 2016-05-28
//   - API CHANGE. Return bool instead of int in onSeek callback.
//   - Fixed a memory leak.
//
// v0.3 - 2016-05-22
//   - Lots of API changes for consistency.
//
// v0.2a - 2016-05-16
//   - Fixed Linux/GCC build.
//
// v0.2 - 2016-05-11
//   - Added support for reading data as signed 32-bit PCM for consistency with dr_flac.
//
// v0.1a - 2016-05-07
//   - Fixed a bug in drwav_open_file() where the file handle would not be closed if the loader failed to initialize.
//
// v0.1 - 2016-05-04
//   - Initial versioned release.


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
