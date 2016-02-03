// Public domain. See "unlicense" statement at the end of this file.

// ABOUT
//
// This is a simple library is for loading .wav files and retrieving it's audio data. It does not explicitly support
// every possible combination of data formats and configurations, but should work fine for the most common ones.
//
//
//
// USAGE
//
// This is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_WAV_IMPLEMENTATION
//   #include "dr_wav.h"
//
// You can then #include this file in other parts of the program as you would with any other header file.
//
//
//
// QUICK NOTES
//
// - Samples are always interleaved.
// - The default read function does not do any data conversion. Use drwav_read_f32() to read and convert audio data
//   to IEEE 32-bit floating point samples. Tested and supported internal formats include the following:
//   - Unsigned 8-bit PCM
//   - Signed 12-bit PCM
//   - Signed 16-bit PCM
//   - Signed 24-bit PCM
//   - Signed 32-bit PCM
//   - IEEE 32-bit floating point.
//   - IEEE 64-bit floating point.
//   - A-law and u-law
// - Microsoft ADPCM is not currently supported.
// - This library does not do strict validation - it will try it's hardest to open every wav file.
//
//
//
// OPTIONS
//
// #define DR_WAV_NO_CONVERSION_API
//   Excludes conversion APIs such as drwav_read_f32() and drwav_s16PCM_to_f32().
//
// #define DR_WAV_NO_STDIO
//   Excludes drwav_open_file().

#ifndef dr_wav_h
#define dr_wav_h

#include <stdint.h>

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

typedef struct drwav drwav;

typedef struct
{
    /// The format tag exactly as specified in the wave file's "fmt" chunk. This can be used by applications
    /// that require support for data formats not listed in the drwav_format enum.
    unsigned short formatTag;

    /// The number of channels making up the audio data. When this is set to 1 it is mono, 2 is stereo, etc.
    unsigned short channels;

    /// The sample rate. Usually set to something like 44100.
    unsigned int sampleRate;

    /// Average bytes per second. You probably don't need this, but it's left here for informational purposes.
    unsigned int avgBytesPerSec;

    /// Block align. This is equal to the number of channels * bytes per sample.
    unsigned short blockAlign;

    /// Bit's per sample.
    unsigned short bitsPerSample;

    /// The size of the extended data. Only used internally for validation, but left here for informational purposes.
    unsigned short extendedSize;

    /// The number of valid bits per sample. When <formatTag> is equal to WAVE_FORMAT_EXTENSIBLE, <bitsPerSample>
    /// is always rounded up to the nearest multiple of 8. This variable contains information about exactly how
    /// many bits a valid per sample. Mainly used for informational purposes.
    unsigned short validBitsPerSample;

    /// The channel mask. Not used at the moment.
    unsigned int channelMask;

    /// The sub-format, exactly as specified by the wave file.
    unsigned char subFormat[16];

} drwav_fmt;

typedef struct
{
    /// Structure containing format information exactly as specified by the wav file.
    drwav_fmt fmt;

    /// The number of bytes per sample.
    unsigned short bytesPerSample;

    /// Equal to fmt.formatTag, or the value specified by fmt.subFormat is fmt.formatTag is equal to 65534 (WAVE_FORMAT_EXTENSIBLE).
    unsigned short translatedFormatTag;

    /// The total number of samples making up the audio data. Use <sampleCount> * <bytesPerSample> to calculate
    /// the required size of a buffer to hold the entire audio data.
    unsigned int sampleCount;

} drwav_info;


/// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* drwav_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

/// Callback for when data needs to be seeked. Offset is always relative to the current position. Return value
/// is 0 on failure, non-zero success.
typedef int (* drwav_seek_proc)(void* userData, int offset);


/// Opens a .wav file using the given callbacks.
///
/// @remarks
///     Returns null on error.
drwav* drwav_open(drwav_read_proc onRead, drwav_seek_proc onSeek, void* userData);

/// Closes the given drwav object.
void drwav_close(drwav* wav);

/// Retrieves information about the given wav file.
drwav_info drwav_get_info(drwav* wav);

/// Reads raw audio data.
///
/// @remarks
///     This is the lowest level function for reading audio data. It simply reads the given number of
///     bytes of the raw internal sample data.
size_t drwav_read_raw(drwav* wav, void* bufferOut, size_t bytesToRead);

/// Reads a chunk of audio data in the native internal format.
///
/// @remarks
///     This is typically the most efficient way to retrieve audio data, but it does not do any format
///     conversions which means you'll need to convert the data manually if required.
///     @par
///     If the return value is less than <samplesToRead> it means the end of the file has been reached.
///     @par
///     The number of samples that are actually read is clamped based on the size of the output buffer.
///     @par
///     This function will only work when sample data is of a fixed size. If you are using an unusual
///     format which uses variable sized samples, consider using drwav_read_raw(), but don't combine them.
unsigned int drwav_read(drwav* wav, unsigned int samplesToRead, void* bufferOut, size_t bufferOutSize);

/// Seeks to the given sample.
///
/// @return Zero if an error occurs, non-zero if successful.
int drwav_seek(drwav* wav, unsigned int sample);



//// Convertion Utilities ////
#ifndef DR_WAV_NO_CONVERSION_API

/// Reads a chunk of audio data and converts it to IEEE 32-bit floating point samples.
///
/// @return The number of samples actually read.
///
/// @remarks
///     If the return value is less than <samplesToRead> it means the end of the file has been reached.
unsigned int drwav_read_f32(drwav* wav, unsigned int samplesToRead, float* bufferOut);

/// Low-level function for converting unsigned 8-bit PCM samples to IEEE 32-bit floating point samples.
void drwav_u8PCM_to_f32(unsigned int sampleCount, const unsigned char* u8PCM, float* f32Out);

/// Low-level function for converting signed 16-bit PCM samples to IEEE 32-bit floating point samples.
void drwav_s16PCM_to_f32(unsigned int sampleCount, const short* s16PCM, float* f32Out);

/// Low-level function for converting signed 24-bit PCM samples to IEEE 32-bit floating point samples.
void drwav_s24PCM_to_f32(unsigned int sampleCount, const unsigned char* s24PCM, float* f32Out);

/// Low-level function for converting signed 32-bit PCM samples to IEEE 32-bit floating point samples.
void drwav_s32PCM_to_f32(unsigned int sampleCount, const int* s32PCM, float* f32Out);

/// Low-level function for converting IEEE 64-bit floating point samples to IEEE 32-bit floating point samples.
void drwav_f64_to_f32(unsigned int sampleCount, const double* f64In, float* f32Out);

/// Low-level function for converting A-law samples to IEEE 32-bit floating point samples.
void drwav_alaw_to_f32(unsigned int sampleCount, const unsigned char* alaw, float* f32Out);

/// Low-level function for converting u-law samples to IEEE 32-bit floating point samples.
void drwav_ulaw_to_f32(unsigned int sampleCount, const unsigned char* ulaw, float* f32Out);

#endif  //DR_WAV_NO_CONVERSION_API


//// High-Level Convenience Helpers ////

#ifndef DR_WAV_NO_STDIO

/// Helper for opening a wave file using stdio.
///
/// @remarks
///     This holds the internal FILE object until drwav_close() is called. Keep this in mind if you're
///     employing caching.
drwav* drwav_open_file(const char* filename);

#endif  //DR_WAV_NO_STDIO

/// Helper for opening a file from a pre-allocated memory buffer.
///
/// @remarks
///     This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
///     the lifetime of the drwav object.
///     @par
///     The buffer should contain the contents of the entire wave file, not just the sample data.
drwav* drwav_open_memory(const void* data, size_t dataSize);



///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////

#ifdef DR_WAV_IMPLEMENTATION
#include <stdlib.h>
#include <string.h> // For memcpy()
#include <assert.h>

#ifndef DR_WAV_NO_STDIO
#include <stdio.h>
#endif

struct drwav
{
    /// Information about the wav file.
    drwav_info info;

    /// A pointer to the function to call when more data is needed.
    drwav_read_proc onRead;

    /// A pointer to the function to call when the wav file needs to be seeked.
    drwav_seek_proc onSeek;

    /// The user data to pass to callbacks.
    void* userData;

    /// The number of bytes remaining in the data chunk.
    size_t bytesRemaining;
};

static int drwav__is_little_endian()
{
    int n = 1;
    return (*(char*)&n) == 1;
}

static unsigned short drwav__read_u16(const unsigned char* data)
{
    if (drwav__is_little_endian()) {
        return (data[0] << 0) | (data[1] << 8);
    } else {
        return (data[1] << 0) | (data[0] << 8);
    }
}

static unsigned int drwav__read_u32(const unsigned char* data)
{
    if (drwav__is_little_endian()) {
        return (data[0] << 0) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    } else {
        return (data[3] << 0) | (data[2] << 8) | (data[1] << 16) | (data[0] << 24);
    }
}

static void drwav__read_guid(const unsigned char* data, unsigned char* guid)
{
    for (int i = 0; i < 16; ++i) {
        guid[i] = data[i];
    }
}

static int drwav__read_fmt(drwav_read_proc onRead, drwav_seek_proc onSeek, void* userData, drwav_fmt* fmtOut)
{
    unsigned char fmt[24];
    if (onRead(userData, fmt, sizeof(fmt)) != sizeof(fmt)) {
        return 0;    // Failed to read data.
    }

    if (fmt[0] != 'f' || fmt[1] != 'm' || fmt[2] != 't' || fmt[3] != ' ') {
        return 0;    // Expecting "fmt " (lower case).
    }

    unsigned int chunkSize = drwav__read_u32(fmt + 4);
    if (chunkSize < 16) {
        return 0;    // The fmt chunk should always be at least 16 bytes.
    }

    if (chunkSize != 16 && chunkSize != 18 && chunkSize != 40) {
        return 0;    // Unexpected chunk size.
    }

    fmtOut->formatTag      = drwav__read_u16(fmt + 8);
    fmtOut->channels       = drwav__read_u16(fmt + 10);
    fmtOut->sampleRate     = drwav__read_u32(fmt + 12);
    fmtOut->avgBytesPerSec = drwav__read_u32(fmt + 16);
    fmtOut->blockAlign     = drwav__read_u16(fmt + 20);
    fmtOut->bitsPerSample  = drwav__read_u16(fmt + 22);
    
    if (chunkSize > 16) {
        if (chunkSize == 18) {
            return onSeek(userData, 2);
        } else {
            assert(chunkSize == 40);

            unsigned char fmt_cbSize[2];
            if (onRead(userData, fmt_cbSize, sizeof(fmt_cbSize)) != sizeof(fmt_cbSize)) {
                return 0;    // Expecting more data.
            }

            fmtOut->extendedSize = drwav__read_u16(fmt_cbSize + 0);
            if (fmtOut->extendedSize != 22) {
                return 0;    // Expecting cbSize to equal 22.
            }

            unsigned char fmtext[22];
            if (onRead(userData, fmtext, sizeof(fmtext)) != sizeof(fmtext)) {
                return 0;    // Expecting more data.
            }

            fmtOut->validBitsPerSample = drwav__read_u16(fmtext + 0);
            fmtOut->channelMask        = drwav__read_u32(fmtext + 2);
            drwav__read_guid(fmtext + 6, fmtOut->subFormat);
        }
    } else {
        fmtOut->extendedSize       = 0;
        fmtOut->validBitsPerSample = 0;
        fmtOut->channelMask        = 0;
        
        for (int i = 0; i < sizeof(fmtOut->subFormat); ++i) {
            fmtOut->subFormat[i] = 0;
        }
    }

    return 1;
}


#ifndef DR_WAV_NO_STDIO
static size_t drwav__on_read_stdio(void* userData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)userData);
}

static int drwav__on_seek_stdio(void* userData, int offset)
{
    return fseek((FILE*)userData, offset, SEEK_CUR) == 0;
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

    return drwav_open(drwav__on_read_stdio, drwav__on_seek_stdio, pFile);
}
#endif  //DR_WAV_NO_STDIO


typedef struct
{
    /// A pointer to the beginning of the data. We use a char as the type here for easy offsetting.
    const unsigned char* data;

    /// The size of the data.
    size_t dataSize;

    /// The position we're currently sitting at.
    size_t currentReadPos;

} drwav_memory;

static size_t drwav__on_read_memory(void* userData, void* bufferOut, size_t bytesToRead)
{
    drwav_memory* memory = userData;
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

static int drwav__on_seek_memory(void* userData, int offset)
{
    drwav_memory* memory = userData;
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

drwav* drwav_open_memory(const void* data, size_t dataSize)
{
    drwav_memory* userData = malloc(sizeof(*userData));
    if (userData == NULL) {
        return NULL;
    }

    userData->data = data;
    userData->dataSize = dataSize;
    userData->currentReadPos = 0;
    return drwav_open(drwav__on_read_memory, drwav__on_seek_memory, userData);
}


drwav* drwav_open(drwav_read_proc onRead, drwav_seek_proc onSeek, void* userData)
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
        return NULL;    // Expecting "RIFF".
    }

    unsigned int chunkSize = drwav__read_u32(riff + 4);
    if (chunkSize < 36) {
        return NULL;    // Chunk size should always be at least 36 bytes.
    }
    
    if (riff[8] != 'W' || riff[9] != 'A' || riff[10] != 'V' || riff[11] != 'E') {
        return NULL;    // Expecting "WAVE".
    }



    // The next 24 bytes should be the "fmt " chunk.
    drwav_fmt fmt;
    if (!drwav__read_fmt(onRead, onSeek, userData, &fmt)) {
        return NULL;    // Failed to read the "fmt " chunk.
    }


    // Translate the internal format.
    unsigned short translatedFormatTag = fmt.formatTag;
    if (translatedFormatTag == DR_WAVE_FORMAT_EXTENSIBLE) {
        translatedFormatTag = (fmt.subFormat[0] << 0) | (fmt.subFormat[1] << 1);
    }

    
    // The next chunk we care about is the "data" chunk. This is not necessarily the next chunk so we'll need to loop.
    unsigned int dataSize;
    for (;;)
    {
        unsigned char chunk[8];
        if (onRead(userData, chunk, sizeof(chunk)) != sizeof(chunk)) {
            return NULL;    // Failed to read data. Probably reached the end.
        }

        dataSize = drwav__read_u32(chunk + 4);
        if (chunk[0] == 'd' && chunk[1] == 'a' && chunk[2] == 't' && chunk[3] == 'a') {
            break;  // We found the data chunk.
        }

        // If we get here it means we didn't find the "data" chunk. Seek past it.

        // Make sure we seek past the pad byte.
        if (dataSize % 2 != 0) {
            dataSize += 1;
        }

        if (!onSeek(userData, (int)dataSize)) {
            return NULL;    // Failed to seek past the chunk. Probably reached the end.
        }
    }

    // At this point we should be sitting on the first byte of the raw audio data.

    drwav* wav = malloc(sizeof(*wav));
    if (wav == NULL) {
        return NULL;
    }

    wav->info.fmt                 = fmt;
    wav->info.bytesPerSample      = (unsigned int)(fmt.blockAlign / fmt.channels);
    wav->info.translatedFormatTag = translatedFormatTag;
    wav->info.sampleCount         = dataSize / wav->info.bytesPerSample;

    wav->onRead         = onRead;
    wav->onSeek         = onSeek;
    wav->userData       = userData;
    wav->bytesRemaining = dataSize;

    return wav;
}

void drwav_close(drwav* wav)
{
    if (wav == NULL) {
        return;
    }

#ifndef DR_WAV_NO_STDIO
    // If we opened the file with drwav_open_file() we will want to close the file handle. We can know whether or not drwav_open_file()
    // was used by looking at the onRead and onSeek callbacks.
    if (wav->onRead == drwav__on_read_stdio && wav->onSeek == drwav__on_seek_stdio) {
        fclose((FILE*)wav->userData);
    }
#endif

    // If we opened the file with drwav_open_memory() we will want to free() the user data.
    if (wav->onRead == drwav__on_read_memory && wav->onSeek == drwav__on_seek_memory) {
        free(wav->userData);
    }

    free(wav);
}


drwav_info drwav_get_info(drwav* wav)
{
    if (wav == NULL) {
        return (drwav_info){0};
    }

    return wav->info;
}


size_t drwav_read_raw(drwav* wav, void* bufferOut, size_t bytesToRead)
{
    if (wav == NULL || bytesToRead == 0 || bufferOut == NULL) {
        return 0;
    }

    if (bytesToRead > wav->bytesRemaining) {
        bytesToRead = wav->bytesRemaining;
    }

    size_t bytesRead = wav->onRead(wav->userData, bufferOut, bytesToRead);

    wav->bytesRemaining -= bytesRead;
    return bytesRead;
}

unsigned int drwav_read(drwav* wav, unsigned int samplesToRead, void* bufferOut, size_t bufferOutSize)
{
    if (wav == NULL || samplesToRead == 0 || bufferOut == NULL) {
        return 0;
    }

    unsigned int maxSamples = (unsigned int)(bufferOutSize / wav->info.bytesPerSample);
    if (samplesToRead > maxSamples) {
        samplesToRead = maxSamples;
    }

    size_t bytesRead = drwav_read_raw(wav, bufferOut, samplesToRead * wav->info.bytesPerSample);
    
    return (unsigned int)(bytesRead / wav->info.bytesPerSample);
}

int drwav_seek(drwav* wav, unsigned int sample)
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


    size_t totalSizeInBytes = wav->info.sampleCount * wav->info.bytesPerSample;
    assert(totalSizeInBytes >= wav->bytesRemaining);

    size_t currentBytePos = totalSizeInBytes - wav->bytesRemaining;
    size_t targetBytePos  = sample * wav->info.bytesPerSample;

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


#ifndef DR_WAV_NO_CONVERSION_API
static int drwav__pcm_to_f32(unsigned int sampleCount, const unsigned char* pPCM, unsigned short bytesPerSample, float* f32Out)
{
    if (pPCM == NULL || f32Out == NULL) {
        return 0;
    }

    // Special case for 8-bit sample data because it's treated as unsigned.
    if (bytesPerSample == 1) {
        drwav_u8PCM_to_f32(sampleCount, pPCM, f32Out);
        return 1;
    }


    // Slightly more optimal implementation for common formats.
    if (bytesPerSample == 2) {
        drwav_s16PCM_to_f32(sampleCount, (const short*)pPCM, f32Out);
        return 1;
    }
    if (bytesPerSample == 3) {
        drwav_s24PCM_to_f32(sampleCount, pPCM, f32Out);
        return 1;
    }
    if (bytesPerSample == 4) {
        drwav_s32PCM_to_f32(sampleCount, (const int*)pPCM, f32Out);
        return 1;
    }


    // Generic, slow converter.
    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        unsigned int sample = 0;
        unsigned int shift  = (8 - bytesPerSample) * 8;
        for (unsigned short j = 0; j < bytesPerSample && j < 4; ++j) {
            sample |= (unsigned int)(pPCM[j]) << shift;
            shift  += 8;
        }

        pPCM += bytesPerSample;
        *f32Out++ = (float)((int)sample / 2147483648.0);
    }

    return 1;
}

static int drwav__ieee_to_f32(unsigned int sampleCount, const unsigned char* pDataIn, unsigned short bytesPerSample, float* f32Out)
{
    if (pDataIn == NULL || f32Out == NULL) {
        return 0;
    }

    if (bytesPerSample == 32) {
        for (unsigned int i = 0; i < sampleCount; ++i) {
            *f32Out++ = ((float*)pDataIn)[i];
        }
        return 1;
    } else {
        drwav_f64_to_f32(sampleCount, (double*)pDataIn, f32Out);
        return 1;
    }

    return 0;
}


unsigned int drwav_read_f32(drwav* wav, unsigned int samplesToRead, float* bufferOut)
{
    if (wav == NULL || samplesToRead == 0 || bufferOut == NULL) {
        return 0;
    }

    // Fast path.
    if (wav->info.translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT && wav->info.bytesPerSample == 4) {
        return drwav_read(wav, samplesToRead, bufferOut, samplesToRead * sizeof(float));
    }


    // Slow path. Need to read and convert.
    unsigned int totalSamplesRead = 0;

    if (wav->info.translatedFormatTag == DR_WAVE_FORMAT_PCM)
    {
        char sampleData[4096];
        while (samplesToRead > 0)
        {
            unsigned int samplesRead = drwav_read(wav, samplesToRead, sampleData, sizeof(sampleData));
            if (samplesRead == 0) {
                break;
            }

            drwav__pcm_to_f32(samplesRead, sampleData, wav->info.bytesPerSample, bufferOut);
            bufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    if (wav->info.translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT)
    {
        char sampleData[4096];
        while (samplesToRead > 0)
        {
            unsigned int samplesRead = drwav_read(wav, samplesToRead, sampleData, sizeof(sampleData));
            if (samplesRead == 0) {
                break;
            }

            drwav__ieee_to_f32(samplesRead, sampleData, wav->info.bytesPerSample, bufferOut);
            bufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    if (wav->info.translatedFormatTag == DR_WAVE_FORMAT_ALAW)
    {
        char sampleData[4096];
        while (samplesToRead > 0)
        {
            unsigned int samplesRead = drwav_read(wav, samplesToRead, sampleData, sizeof(sampleData));
            if (samplesRead == 0) {
                break;
            }

            drwav_alaw_to_f32(samplesRead, sampleData, bufferOut);
            bufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    if (wav->info.translatedFormatTag == DR_WAVE_FORMAT_MULAW)
    {
        char sampleData[4096];
        while (samplesToRead > 0)
        {
            unsigned int samplesRead = drwav_read(wav, samplesToRead, sampleData, sizeof(sampleData));
            if (samplesRead == 0) {
                break;
            }

            drwav_ulaw_to_f32(samplesRead, sampleData, bufferOut);
            bufferOut += samplesRead;

            samplesToRead    -= samplesRead;
            totalSamplesRead += samplesRead;
        }

        return totalSamplesRead;
    }

    return totalSamplesRead;
}

void drwav_u8PCM_to_f32(unsigned int sampleCount, const unsigned char* u8PCM, float* f32Out)
{
    if (u8PCM == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        *f32Out++ = (u8PCM[i] / 255.0f) * 2 - 1;
    }
}

void drwav_s16PCM_to_f32(unsigned int sampleCount, const short* s16PCM, float* f32Out)
{
    if (s16PCM == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        *f32Out++ = s16PCM[i] / 32768.0f;
    }
}

void drwav_s24PCM_to_f32(unsigned int sampleCount, const unsigned char* s24PCM, float* f32Out)
{
    if (s24PCM == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        unsigned int s0 = s24PCM[i*3 + 0];
        unsigned int s1 = s24PCM[i*3 + 1];
        unsigned int s2 = s24PCM[i*3 + 2];

        int sample32 = (int)((s0 << 8) | (s1 << 16) | (s2 << 24));
        *f32Out++ = (float)(sample32 / 2147483648.0);
    }
}

void drwav_s32PCM_to_f32(unsigned int sampleCount, const int* s32PCM, float* f32Out)
{
    if (s32PCM == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        *f32Out++ = (float)(s32PCM[i] / 2147483648.0);
    }
}

void drwav_f64_to_f32(unsigned int sampleCount, const double* f64In, float* f32Out)
{
    if (f64In == NULL || f32Out == NULL) {
        return;
    }

    for (unsigned int i = 0; i < sampleCount; ++i)
    {
        *f32Out++ = (float)f64In[i];
    }
}

void drwav_alaw_to_f32(unsigned int sampleCount, const unsigned char* alaw, float* f32Out)
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

void drwav_ulaw_to_f32(unsigned int sampleCount, const unsigned char* ulaw, float* f32Out)
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
#endif DR_WAV_NO_CONVERSION_API

#endif  //DR_WAV_IMPLEMENTATION

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