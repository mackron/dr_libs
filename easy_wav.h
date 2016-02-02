// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - easy_wav is a simple library is for loading .wav files and retrieving it's audio data. It does not explicitly support
//   every possible combination of data formats and configurations, but should work fine for the most common ones.
// - This library is still very early in development. Expect a few bugs, especially on on big-endian architectures which
//   is completely untested.
// - Samples are always interleaved.
// - The default read function does not do any data conversion. Use easywav_read_f32() to read and convert audio data
//   to IEEE 32-bit floating point samples. Supported internal formats include the following:
//   - Signed 16-bit PCM
//   - Signed 24-bit PCM
//   - Signed 32-bit PCM
//   - Unsigned 8-bit PCM
//   - IEEE 32-bit floating point.
//   - IEEE 64-bit floating point.
//   - A-law and u-law
//

//
// OPTIONS
//
// #define EASY_WAV_NO_CONVERSION_API
//   Excludes conversion APIs such as easywav_read_f32() and easywav_s16PCM_to_f32().
//
// #define EASY_WAV_NO_STDIO
//   Excludes easywav_open_file().
//

#ifndef easy_wav_h
#define easy_wav_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The different support internal formats.
typedef enum
{
    easywav_format_unknown,
    easywav_format_signed_pcm_16,
    easywav_format_signed_pcm_24,
    easywav_format_signed_pcm_32,
    easywav_format_unsigned_pcm_8,
    easywav_format_float_32,
    easywav_format_float_64,
    easywav_format_alaw,
    easywav_format_ulaw
} easywav_format;

typedef struct easywav easywav;

typedef struct
{
    /// The number of channels making up the audio data. When this is set to 1 it is mono, 2 is stereo, etc.
    unsigned int channels;

    /// The sample rate. Usually set to something like 44100.
    unsigned int sampleRate;

    /// The internal format of the wav file. The audio data is converted from this format when it is read. This
    /// will be set to easywav_format_unknown if it is a format unrecognized by easy_wav. In this case, applications
    /// should use formatTag to identify the data format. In addition, easywav_read_f32() will fail if this is set
    /// to easywav_format_unknown.
    easywav_format internalFormat;

    /// The number of bits per sample. This is tied to <internalFormat> and is only really used internally.
    unsigned int bitsPerSample;

    /// The format tag exactly as specified in the wave file's "fmt" chunk. This can be used by applications
    /// that require support for data formats not listed in the easywav_format enum.
    unsigned short formatTag;

    /// The total number of samples making up the audio data. Use <sampleCount> * (<bitsPerSample> / 8) to
    /// calculate the required size of a buffer to hold the entire audio data.
    unsigned int sampleCount;

} easywav_info;


/// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* easywav_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

/// Callback for when data needs to be seeked. Offset is always relative to the current position. Return value
/// is 0 on failure, non-zero success.
typedef int (* easywav_seek_proc)(void* userData, int offset);


/// Opens a .wav file using the given callbacks.
///
/// @remarks
///     Returns null on error.
easywav* easywav_open(easywav_read_proc onRead, easywav_seek_proc onSeek, void* userData);

/// Closes the given easywav object.
void easywav_close(easywav* wav);

/// Retrieves information about the given wav file.
easywav_info easywav_get_info(easywav* wav);

/// Reads a chunk of audio data in the native internal format.
///
/// @remarks
///     This is typically the most efficient way to retrieve audio data, but it does not do any format
///     conversions which means you'll need to convert the data manually if required.
///     @par
///     If the return value is less than <samplesToRead> it means the end of the file has been reached.
unsigned int easywav_read(easywav* wav, unsigned int samplesToRead, void* bufferOut);

/// Seeks to the given sample.
///
/// @return Zero if an error occurs, non-zero if successful.
int easywav_seek(easywav* wav, unsigned int sample);



//// Convertion Utilities ////
#ifndef EASY_WAV_NO_CONVERSION_API

/// Reads a chunk of audio data and converts it to IEEE 32-bit floating point samples.
///
/// @return The number of samples actually read.
///
/// @remarks
///     If the return value is less than <samplesToRead> it means the end of the file has been reached.
unsigned int easywav_read_f32(easywav* wav, unsigned int samplesToRead, float* bufferOut);

/// Low-level function for converting signed 16-bit PCM samples to IEEE 32-bit floating point samples.
void easywav_s16PCM_to_f32(unsigned int sampleCount, const short* s16PCM, float* f32Out);

/// Low-level function for converting signed 24-bit PCM samples to IEEE 32-bit floating point samples.
void easywav_s24PCM_to_f32(unsigned int sampleCount, const unsigned char* s24PCM, float* f32Out);

/// Low-level function for converting signed 32-bit PCM samples to IEEE 32-bit floating point samples.
void easywav_s32PCM_to_f32(unsigned int sampleCount, const int* s32PCM, float* f32Out);

/// Low-level function for converting unsigned 8-bit PCM samples to IEEE 32-bit floating point samples.
void easywav_u8PCM_to_f32(unsigned int sampleCount, const unsigned char* u8PCM, float* f32Out);

/// Low-level function for converting IEEE 64-bit floating point samples to IEEE 32-bit floating point samples.
void easywav_f64_to_f32(unsigned int sampleCount, const double* f64In, float* f32Out);

/// Low-level function for converting A-law samples to IEEE 32-bit floating point samples.
void easywav_alaw_to_f32(unsigned int sampleCount, const unsigned char* alaw, float* f32Out);

/// Low-level function for converting u-law samples to IEEE 32-bit floating point samples.
void easywav_ulaw_to_f32(unsigned int sampleCount, const unsigned char* ulaw, float* f32Out);

#endif  //EASY_WAV_NO_CONVERSION_API


//// High-Level Convenience Helpers ////

#ifndef EASY_WAV_NO_STDIO

/// Helper for opening a wave file using stdio.
///
/// @remarks
///     This holds the internal FILE object until easywav_close() is called. Keep this in mind if you're
///     employing caching.
easywav* easywav_open_file(const char* filename);

#endif  //EASY_WAV_NO_STDIO

/// Helper for opening a file from a pre-allocated memory buffer.
///
/// @remarks
///     This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
///     the lifetime of the easywav object.
///     @par
///     The buffer should contain the contents of the entire wave file, not just the sample data.
easywav* easywav_open_memory(const void* data, size_t dataSize);



///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////

#ifdef EASY_WAV_IMPLEMENTATION
#include <stdlib.h>
#include <string.h> // For memcpy()
#include <assert.h>

#ifndef EASY_WAV_NO_STDIO
#include <stdio.h>
#endif

#define WAVE_FORMAT_PCM        0x1
#define WAVE_FORMAT_IEEE_FLOAT 0x3
#define WAVE_FORMAT_ALAW       0x6
#define WAVE_FORMAT_MULAW      0x7

struct easywav
{
    /// Information about the wav file.
    easywav_info info;

    /// A pointer to the function to call when more data is needed.
    easywav_read_proc onRead;

    /// A pointer to the function to call when the wav file needs to be seeked.
    easywav_seek_proc onSeek;

    /// The user data to pass to callbacks.
    void* userData;

    /// The number of bytes remaining in the data chunk.
    size_t bytesRemaining;
};

#ifndef EASY_WAV_NO_STDIO
static size_t easywav__on_read_stdio(void* userData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)userData);
}

static int easywav__on_seek_stdio(void* userData, int offset)
{
    return fseek((FILE*)userData, offset, SEEK_CUR) == 0;
}

easywav* easywav_open_file(const char* filename)
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

    return easywav_open(easywav__on_read_stdio, easywav__on_seek_stdio, pFile);
}
#endif  //EASY_WAV_NO_STDIO


typedef struct
{
    /// A pointer to the beginning of the data. We use a char as the type here for easy offsetting.
    const unsigned char* data;

    /// The size of the data.
    size_t dataSize;

    /// The position we're currently sitting at.
    size_t currentReadPos;

} easywav_memory;

static size_t easywav__on_read_memory(void* userData, void* bufferOut, size_t bytesToRead)
{
    easywav_memory* memory = userData;
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

static int easywav__on_seek_memory(void* userData, int offset)
{
    easywav_memory* memory = userData;
    assert(memory != NULL);

    if (offset > 0) {
        if (memory->currentReadPos + offset > memory->dataSize) {
            offset = memory->dataSize - memory->currentReadPos;     // Trying to seek too far forward.
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

easywav* easywav_open_memory(const void* data, size_t dataSize)
{
    easywav_memory* userData = malloc(sizeof(*userData));
    if (userData == NULL) {
        return NULL;
    }

    userData->data = data;
    userData->dataSize = dataSize;
    userData->currentReadPos = 0;
    return easywav_open(easywav__on_read_memory, easywav__on_seek_memory, userData);
}


easywav* easywav_open(easywav_read_proc onRead, easywav_seek_proc onSeek, void* userData)
{
    if (onRead == NULL || onSeek == NULL) {
        return NULL;
    }


    // The first 12 bytes should be the RIFF chunk.
    unsigned char riff[12];
    if (onRead(userData, riff, sizeof(riff)) != sizeof(riff)) {
        return NULL;    // Failed to read data.
    }

    if (riff[0] != 'R' || riff[1] != 'I' || riff[2] != 'F' || riff[3] != 'F') {
        return NULL;    // Expecting "RIFF". TODO LATER: Add support for "RIFX" (big-ending data).
    }

    unsigned int chunkSize = (riff[4] << 0) | (riff[5] << 8) | (riff[6] << 16) | (riff[7] << 24);
    if (chunkSize < 36) {
        return NULL;    // Chunk size should always be at least 36 bytes.
    }
    
    if (riff[8] != 'W' || riff[9] != 'A' || riff[10] != 'V' || riff[11] != 'E') {
        return NULL;    // Expecting "WAVE".
    }



    // The next 24 bytes should be the "fmt " chunk.
    unsigned char fmt[24];
    if (onRead(userData, fmt, sizeof(fmt)) != sizeof(fmt)) {
        return NULL;    // Failed to read data.
    }

    if (fmt[0] != 'f' || fmt[1] != 'm' || fmt[2] != 't' || fmt[3] != ' ') {
        return NULL;    // Expecting "fmt " (lower case).
    }

    chunkSize = (fmt[4] << 0) | (fmt[5] << 8) | (fmt[6] << 16) | (fmt[7] << 24);
    if (chunkSize < 16) {
        return NULL;    // The fmt chunk should always be at least 16 bytes.
    }

    unsigned short wFormatTag     = (fmt[ 8] << 0) | (fmt[ 9] << 8);
    unsigned short nChannels      = (fmt[10] << 0) | (fmt[11] << 8);
    unsigned int   nSamplesPerSec = (fmt[12] << 0) | (fmt[13] << 8) | (fmt[14] << 16) | (fmt[15] << 24);
    unsigned short wBitsPerSample = (fmt[22] << 0) | (fmt[23] << 8);
    
    if (chunkSize > 16) {
        onSeek(userData, chunkSize - 16);
    }

    // Validate the internal format.
    easywav_format internalFormat = easywav_format_unknown;
    if (wFormatTag == WAVE_FORMAT_PCM) {
        if (wBitsPerSample == 8) {
            internalFormat = easywav_format_unsigned_pcm_8;
        } else if (wBitsPerSample == 16) {
            internalFormat = easywav_format_signed_pcm_16;
        } else if (wBitsPerSample == 24) {
            internalFormat = easywav_format_signed_pcm_24;
        } else if (wBitsPerSample == 32) {
            internalFormat = easywav_format_signed_pcm_32;
        }
    } else if (wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        if (wBitsPerSample == 32) {
            internalFormat = easywav_format_float_32;
        } else if (wBitsPerSample == 64) {
            internalFormat = easywav_format_float_64;
        }
    } else if (wFormatTag == WAVE_FORMAT_ALAW) {
        if (wBitsPerSample == 8) {
            internalFormat = easywav_format_alaw;
        }
    } else if (wFormatTag == WAVE_FORMAT_MULAW) {
        if (wBitsPerSample == 8) {
            internalFormat = easywav_format_ulaw;
        }
    }


    
    // The next chunk we care about is the "data" chunk. This is not necessarily the next chunk so we'll need to loop.
    unsigned int dataSize;
    for (;;)
    {
        unsigned char chunk[8];
        if (onRead(userData, chunk, sizeof(chunk)) != sizeof(chunk)) {
            return NULL;    // Failed to read data. Probably reached the end.
        }

        dataSize = (chunk[4] << 0) | (chunk[5] << 8) | (chunk[6] << 16) | (chunk[7] << 24);
        if (chunk[0] == 'd' && chunk[1] == 'a' && chunk[2] == 't' && chunk[3] == 'a') {
            break;  // We found the data chunk.
        }

        // If we get here it means we didn't find the "data" chunk. Seek past it.
        if (!onSeek(userData, (int)dataSize)) {
            return NULL;    // Failed to seek past the chunk. Probably reached the end.
        }
    }

    // At this point we should be sitting on the first byte of the raw audio data.

    easywav* wav = malloc(sizeof(*wav));
    if (wav == NULL) {
        return NULL;
    }

    wav->info.channels       = (unsigned int)nChannels;
    wav->info.sampleRate     = nSamplesPerSec;
    wav->info.internalFormat = internalFormat;
    wav->info.bitsPerSample  = (unsigned int)wBitsPerSample;
    wav->info.formatTag      = wFormatTag;
    wav->info.sampleCount    = dataSize / (wBitsPerSample / 8);

    wav->onRead         = onRead;
    wav->onSeek         = onSeek;
    wav->userData       = userData;
    wav->bytesRemaining = dataSize;

    return wav;
}

void easywav_close(easywav* wav)
{
    if (wav == NULL) {
        return;
    }

#ifndef EASY_WAV_NO_STDIO
    // If we opened the file with easywav_open_file() we will want to close the file handle. We can know whether or not easywav_open_file()
    // was used by looking at the onRead and onSeek callbacks.
    if (wav->onRead == easywav__on_read_stdio && wav->onSeek == easywav__on_seek_stdio) {
        fclose((FILE*)wav->userData);
    }
#endif

    // If we opened the file with easywav_open_memory() we will want to free() the user data.
    if (wav->onRead == easywav__on_read_memory && wav->onSeek == easywav__on_seek_memory) {
        free(wav->userData);
    }

    free(wav);
}


easywav_info easywav_get_info(easywav* wav)
{
    if (wav == NULL) {
        return (easywav_info){0};
    }

    return wav->info;
}


unsigned int easywav_read(easywav* wav, unsigned int samplesToRead, void* bufferOut)
{
    if (wav == NULL || samplesToRead == 0 || bufferOut == NULL) {
        return 0;
    }

    size_t bytesToRead = samplesToRead * (wav->info.bitsPerSample / 8);
    if (bytesToRead > wav->bytesRemaining) {
        bytesToRead = wav->bytesRemaining;
    }

    size_t bytesRead = wav->onRead(wav->userData, bufferOut, bytesToRead);
    
    wav->bytesRemaining -= bytesRead;

    return bytesRead / (wav->info.bitsPerSample / 8);
}

int easywav_seek(easywav* wav, unsigned int sample)
{
    // Seeking should be compatible with wave files > 2GB.

    if (wav == NULL || wav->onSeek == NULL) {
        return 0;
    }

    // If there are no samples, just return true without doing anything.
    if (wav->info.sampleCount == 0) {
        return 1;
    }

    // Make sure the sample is clamped.
    if (sample >= wav->info.sampleCount) {
        sample = wav->info.sampleCount - 1;
    }


    size_t totalSizeInBytes = wav->info.sampleCount * (wav->info.bitsPerSample / 8);
    assert(totalSizeInBytes >= wav->bytesRemaining);

    size_t currentBytePos = totalSizeInBytes - wav->bytesRemaining;
    size_t targetBytePos  = sample * (wav->info.bitsPerSample / 8);

    size_t offset;
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
        wav->onSeek(wav->userData, offset32 * direction);

        wav->bytesRemaining -= (offset32 * direction);
        offset -= offset32;
    }

    return 1;
}


#ifndef EASY_WAV_NO_CONVERSION_API
unsigned int easywav_read_f32(easywav* wav, unsigned int samplesToRead, float* bufferOut)
{
    if (wav == NULL || samplesToRead == 0 || bufferOut == NULL) {
        return 0;
    }

    // Fast path.
    if (wav->info.internalFormat == easywav_format_float_32) {
        return easywav_read(wav, samplesToRead, bufferOut);
    }

    
    // Slow path. Need to read and convert.
    switch (wav->info.internalFormat)
    {
        case easywav_format_signed_pcm_16:
        {
            // signed 16-bit PCM -> 32-bit float
            while (samplesToRead > 0)
            {
                short rawSamples[4096/sizeof(short)];

                unsigned int rawSamplesToRead = sizeof(rawSamples)/sizeof(short);
                if (rawSamplesToRead > samplesToRead) {
                    rawSamplesToRead = samplesToRead;
                }
                
                unsigned int rawSamplesRead = easywav_read(wav, rawSamplesToRead, rawSamples);
                if (rawSamplesRead == 0) {
                    break;
                }

                easywav_s16PCM_to_f32(rawSamplesRead, rawSamples, bufferOut);
                bufferOut += rawSamplesRead;

                samplesToRead -= rawSamplesRead;
            }
        } break;

        case easywav_format_signed_pcm_24:
        {
            // signed 24-bit PCM -> 32-bit float
            while (samplesToRead > 0)
            {
                unsigned char rawSamples[1024*3];

                unsigned int rawSamplesToRead = 1024;
                if (rawSamplesToRead > samplesToRead) {
                    rawSamplesToRead = samplesToRead;
                }
                
                unsigned int rawSamplesRead = easywav_read(wav, rawSamplesToRead, rawSamples);
                if (rawSamplesRead == 0) {
                    break;
                }

                easywav_s24PCM_to_f32(rawSamplesRead, rawSamples, bufferOut);
                bufferOut += rawSamplesRead;

                samplesToRead -= rawSamplesRead;
            }
        } break;

        case easywav_format_signed_pcm_32:
        {
            // signed 32-bit PCM -> 32-bit float
            while (samplesToRead > 0)
            {
                int rawSamples[4096/sizeof(int)];

                unsigned int rawSamplesToRead = sizeof(rawSamples)/sizeof(int);
                if (rawSamplesToRead > samplesToRead) {
                    rawSamplesToRead = samplesToRead;
                }
                
                unsigned int rawSamplesRead = easywav_read(wav, rawSamplesToRead, rawSamples);
                if (rawSamplesRead == 0) {
                    break;
                }

                easywav_s32PCM_to_f32(rawSamplesRead, rawSamples, bufferOut);
                bufferOut += rawSamplesRead;

                samplesToRead -= rawSamplesRead;
            }
        } break;

        case easywav_format_unsigned_pcm_8:
        {
            // unsigned 8-bit PCM -> 32-bit float
            while (samplesToRead > 0)
            {
                unsigned char rawSamples[4096];

                unsigned int rawSamplesToRead = sizeof(rawSamples);
                if (rawSamplesToRead > samplesToRead) {
                    rawSamplesToRead = samplesToRead;
                }
                
                unsigned int rawSamplesRead = easywav_read(wav, rawSamplesToRead, rawSamples);
                if (rawSamplesRead == 0) {
                    break;
                }

                easywav_u8PCM_to_f32(rawSamplesRead, rawSamples, bufferOut);
                bufferOut += rawSamplesRead;

                samplesToRead -= rawSamplesRead;
            }
        } break;

        case easywav_format_float_64:
        {
            // 64-bit float -> 32-bit float
            while (samplesToRead > 0)
            {
                double rawSamples[4096/sizeof(double)];

                unsigned int rawSamplesToRead = sizeof(rawSamples)/sizeof(double);
                if (rawSamplesToRead > samplesToRead) {
                    rawSamplesToRead = samplesToRead;
                }
                
                unsigned int rawSamplesRead = easywav_read(wav, rawSamplesToRead, rawSamples);
                if (rawSamplesRead == 0) {
                    break;
                }

                easywav_f64_to_f32(rawSamplesRead, rawSamples, bufferOut);
                bufferOut += rawSamplesRead;

                samplesToRead -= rawSamplesRead;
            }
        } break;

        case easywav_format_alaw:
        {
            // A-law -> 32-bit float
            while (samplesToRead > 0)
            {
                unsigned char rawSamples[4096];

                unsigned int rawSamplesToRead = sizeof(rawSamples);
                if (rawSamplesToRead > samplesToRead) {
                    rawSamplesToRead = samplesToRead;
                }
                
                unsigned int rawSamplesRead = easywav_read(wav, rawSamplesToRead, rawSamples);
                if (rawSamplesRead == 0) {
                    break;
                }

                easywav_alaw_to_f32(rawSamplesRead, rawSamples, bufferOut);
                bufferOut += rawSamplesRead;

                samplesToRead -= rawSamplesRead;
            }
        } break;

        case easywav_format_ulaw:
        {
            // u-law -> 32-bit float
            while (samplesToRead > 0)
            {
                unsigned char rawSamples[4096];

                unsigned int rawSamplesToRead = sizeof(rawSamples);
                if (rawSamplesToRead > samplesToRead) {
                    rawSamplesToRead = samplesToRead;
                }
                
                unsigned int rawSamplesRead = easywav_read(wav, rawSamplesToRead, rawSamples);
                if (rawSamplesRead == 0) {
                    break;
                }

                easywav_ulaw_to_f32(rawSamplesRead, rawSamples, bufferOut);
                bufferOut += rawSamplesRead;

                samplesToRead -= rawSamplesRead;
            }
        } break;

        case easywav_format_float_32:
        case easywav_format_unknown:
        default: break;
    }


    return 0;
}

void easywav_s16PCM_to_f32(unsigned int sampleCount, const short* s16PCM, float* f32Out)
{
    if (s16PCM == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        *f32Out++ = s16PCM[i] / 32768.0f;
    }
}

void easywav_s24PCM_to_f32(unsigned int sampleCount, const unsigned char* s24PCM, float* f32Out)
{
    if (s24PCM == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        unsigned int s0 = s24PCM[i*3 + 0];
        unsigned int s1 = s24PCM[i*3 + 1];
        unsigned int s2 = s24PCM[i*3 + 2];

        int sample32 = ((s0 << 0) | (s1 << 8) | (s2 << 16));
        if (sample32 & 0x800000) {
            sample32 |= ~0xffffff;
        }

        *f32Out++ = sample32 / 8388607.0f;
    }
}

void easywav_s32PCM_to_f32(unsigned int sampleCount, const int* s32PCM, float* f32Out)
{
    if (s32PCM == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        *f32Out++ = s32PCM[i] / 2147483648.0f;
    }
}

void easywav_u8PCM_to_f32(unsigned int sampleCount, const unsigned char* u8PCM, float* f32Out)
{
    if (u8PCM == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        *f32Out++ = (u8PCM[i] / 255.0f) * 2 - 1;
    }
}

void easywav_f64_to_f32(unsigned int sampleCount, const double* f64In, float* f32Out)
{
    if (f64In == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        *f32Out++ = (float)f64In[i];
    }
}

void easywav_alaw_to_f32(unsigned int sampleCount, const unsigned char* alaw, float* f32Out)
{
    if (alaw == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        const unsigned char a = alaw[i] ^ 0x55;

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

        *f32Out++ = t / 32768.0f;
    }
}

void easywav_ulaw_to_f32(unsigned int sampleCount, const unsigned char* ulaw, float* f32Out)
{
    if (ulaw == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        const unsigned char u = ~ulaw[i];

        int t = (((u & 0x0F) << 3) + 0x84) << (((unsigned int)u & 0x70) >> 4);
        if (u & 0x80) {
            t = 0x84 - t;
        } else {
            t = t - 0x84;
        }

        *f32Out++ = t / 32768.0f;
    }
}
#endif EASY_WAV_NO_CONVERSION_API

#endif  //EASY_WAV_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif

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