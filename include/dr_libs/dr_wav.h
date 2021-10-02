/*
WAV audio loader and writer. Choice of public domain or MIT-0. See license
statements at the end of this file. dr_wav - v0.13.2 - 2021-10-02

David Reid - mackron@gmail.com

GitHub: https://github.com/mackron/dr_libs
*/

/*
Introduction
============
This is a single file library. To use it, do something like the following in one
.c file.

    ```c
    #define DR_WAV_IMPLEMENTATION
    #include "dr_wav.h"
    ```

You can then #include this file in other parts of the program as you would with
any other header file. Do something like the following to read audio data:

    ```c
    drwav wav;
    if (!drwav_init_file(&wav, "my_song.wav", NULL)) {
        // Error opening WAV file.
    }

    drwav_int32* pDecodedInterleavedPCMFrames = malloc(wav.totalPCMFrameCount *
wav.channels * sizeof(drwav_int32)); size_t numberOfSamplesActuallyDecoded =
drwav_read_pcm_frames_s32(&wav, wav.totalPCMFrameCount,
pDecodedInterleavedPCMFrames);

    ...

    drwav_uninit(&wav);
    ```

If you just want to quickly open and read the audio data in a single operation
you can do something like this:

    ```c
    unsigned int channels;
    unsigned int sampleRate;
    drwav_uint64 totalPCMFrameCount;
    float* pSampleData = drwav_open_file_and_read_pcm_frames_f32("my_song.wav",
&channels, &sampleRate, &totalPCMFrameCount, NULL); if (pSampleData == NULL) {
        // Error opening and reading WAV file.
    }

    ...

    drwav_free(pSampleData, NULL);
    ```

The examples above use versions of the API that convert the audio data to a
consistent format (32-bit signed PCM, in this case), but you can still output
the audio data in its internal format (see notes below for supported formats):

    ```c
    size_t framesRead = drwav_read_pcm_frames(&wav, wav.totalPCMFrameCount,
pDecodedInterleavedPCMFrames);
    ```

You can also read the raw bytes of audio data, which could be useful if dr_wav
does not have native support for a particular data format:

    ```c
    size_t bytesRead = drwav_read_raw(&wav, bytesToRead, pRawDataBuffer);
    ```

dr_wav can also be used to output WAV files. This does not currently support
compressed formats. To use this, look at `drwav_init_write()`,
`drwav_init_file_write()`, etc. Use `drwav_write_pcm_frames()` to write samples,
or `drwav_write_raw()` to write raw data in the "data" chunk.

    ```c
    drwav_data_format format;
    format.container = drwav_container_riff;     // <-- drwav_container_riff =
normal WAV files, drwav_container_w64 = Sony Wave64. format.format =
DR_WAVE_FORMAT_PCM;          // <-- Any of the DR_WAVE_FORMAT_* codes.
    format.channels = 2;
    format.sampleRate = 44100;
    format.bitsPerSample = 16;
    drwav_init_file_write(&wav, "data/recording.wav", &format, NULL);

    ...

    drwav_uint64 framesWritten = drwav_write_pcm_frames(pWav, frameCount,
pSamples);
    ```

dr_wav has seamless support the Sony Wave64 format. The decoder will
automatically detect it and it should Just Work without any manual intervention.


Build Options
=============
#define these options before including this file.

#define DR_WAV_NO_CONVERSION_API
  Disables conversion APIs such as `drwav_read_pcm_frames_f32()` and
`drwav_s16_to_f32()`.

#define DR_WAV_NO_STDIO
  Disables APIs that initialize a decoder from a file such as
`drwav_init_file()`, `drwav_init_file_write()`, etc.



Notes
=====
- Samples are always interleaved.
- The default read function does not do any data conversion. Use
`drwav_read_pcm_frames_f32()`, `drwav_read_pcm_frames_s32()` and
`drwav_read_pcm_frames_s16()` to read and convert audio data to 32-bit floating
point, signed 32-bit integer and signed 16-bit integer samples respectively.
Tested and supported internal formats include the following:
  - Unsigned 8-bit PCM
  - Signed 12-bit PCM
  - Signed 16-bit PCM
  - Signed 24-bit PCM
  - Signed 32-bit PCM
  - IEEE 32-bit floating point
  - IEEE 64-bit floating point
  - A-law and u-law
  - Microsoft ADPCM
  - IMA ADPCM (DVI, format code 0x11)
- dr_wav will try to read the WAV file as best it can, even if it's not strictly
conformant to the WAV format.
*/

#ifndef dr_wav_h
#define dr_wav_h

#ifdef __cplusplus
extern "C" {
#endif

#define DRWAV_STRINGIFY(x) #x
#define DRWAV_XSTRINGIFY(x) DRWAV_STRINGIFY(x)

#define DRWAV_VERSION_MAJOR 0
#define DRWAV_VERSION_MINOR 13
#define DRWAV_VERSION_REVISION 2
#define DRWAV_VERSION_STRING                                                                       \
  DRWAV_XSTRINGIFY(DRWAV_VERSION_MAJOR)                                                            \
  "." DRWAV_XSTRINGIFY(DRWAV_VERSION_MINOR) "." DRWAV_XSTRINGIFY(DRWAV_VERSION_REVISION)

#include <stddef.h> /* For size_t. */

/* Sized types. */
typedef signed char drwav_int8;
typedef unsigned char drwav_uint8;
typedef signed short drwav_int16;
typedef unsigned short drwav_uint16;
typedef signed int drwav_int32;
typedef unsigned int drwav_uint32;
#if defined(_MSC_VER) && !defined(__clang__)
typedef signed __int64 drwav_int64;
typedef unsigned __int64 drwav_uint64;
#else
#if defined(__clang__) ||                                                                          \
    (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlong-long"
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wc++11-long-long"
#endif
#endif
typedef signed long long drwav_int64;
typedef unsigned long long drwav_uint64;
#if defined(__clang__) ||                                                                          \
    (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
#pragma GCC diagnostic pop
#endif
#endif
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) ||        \
    defined(_M_X64) || defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) ||              \
    defined(_M_ARM64) || defined(__powerpc64__)
typedef drwav_uint64 drwav_uintptr;
#else
typedef drwav_uint32 drwav_uintptr;
#endif
typedef drwav_uint8 drwav_bool8;
typedef drwav_uint32 drwav_bool32;
#define DRWAV_TRUE 1
#define DRWAV_FALSE 0

#if !defined(DRWAV_API)
#if defined(DRWAV_DLL)
#if defined(_WIN32)
#define DRWAV_DLL_IMPORT __declspec(dllimport)
#define DRWAV_DLL_EXPORT __declspec(dllexport)
#define DRWAV_DLL_PRIVATE static
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define DRWAV_DLL_IMPORT __attribute__((visibility("default")))
#define DRWAV_DLL_EXPORT __attribute__((visibility("default")))
#define DRWAV_DLL_PRIVATE __attribute__((visibility("hidden")))
#else
#define DRWAV_DLL_IMPORT
#define DRWAV_DLL_EXPORT
#define DRWAV_DLL_PRIVATE static
#endif
#endif

#if defined(DR_WAV_IMPLEMENTATION) || defined(DRWAV_IMPLEMENTATION)
#define DRWAV_API DRWAV_DLL_EXPORT
#else
#define DRWAV_API DRWAV_DLL_IMPORT
#endif
#define DRWAV_PRIVATE DRWAV_DLL_PRIVATE
#else
#define DRWAV_API extern
#define DRWAV_PRIVATE static
#endif
#endif

typedef drwav_int32 drwav_result;
#define DRWAV_SUCCESS 0
#define DRWAV_ERROR -1 /* A generic error. */
#define DRWAV_INVALID_ARGS -2
#define DRWAV_INVALID_OPERATION -3
#define DRWAV_OUT_OF_MEMORY -4
#define DRWAV_OUT_OF_RANGE -5
#define DRWAV_ACCESS_DENIED -6
#define DRWAV_DOES_NOT_EXIST -7
#define DRWAV_ALREADY_EXISTS -8
#define DRWAV_TOO_MANY_OPEN_FILES -9
#define DRWAV_INVALID_FILE -10
#define DRWAV_TOO_BIG -11
#define DRWAV_PATH_TOO_LONG -12
#define DRWAV_NAME_TOO_LONG -13
#define DRWAV_NOT_DIRECTORY -14
#define DRWAV_IS_DIRECTORY -15
#define DRWAV_DIRECTORY_NOT_EMPTY -16
#define DRWAV_END_OF_FILE -17
#define DRWAV_NO_SPACE -18
#define DRWAV_BUSY -19
#define DRWAV_IO_ERROR -20
#define DRWAV_INTERRUPT -21
#define DRWAV_UNAVAILABLE -22
#define DRWAV_ALREADY_IN_USE -23
#define DRWAV_BAD_ADDRESS -24
#define DRWAV_BAD_SEEK -25
#define DRWAV_BAD_PIPE -26
#define DRWAV_DEADLOCK -27
#define DRWAV_TOO_MANY_LINKS -28
#define DRWAV_NOT_IMPLEMENTED -29
#define DRWAV_NO_MESSAGE -30
#define DRWAV_BAD_MESSAGE -31
#define DRWAV_NO_DATA_AVAILABLE -32
#define DRWAV_INVALID_DATA -33
#define DRWAV_TIMEOUT -34
#define DRWAV_NO_NETWORK -35
#define DRWAV_NOT_UNIQUE -36
#define DRWAV_NOT_SOCKET -37
#define DRWAV_NO_ADDRESS -38
#define DRWAV_BAD_PROTOCOL -39
#define DRWAV_PROTOCOL_UNAVAILABLE -40
#define DRWAV_PROTOCOL_NOT_SUPPORTED -41
#define DRWAV_PROTOCOL_FAMILY_NOT_SUPPORTED -42
#define DRWAV_ADDRESS_FAMILY_NOT_SUPPORTED -43
#define DRWAV_SOCKET_NOT_SUPPORTED -44
#define DRWAV_CONNECTION_RESET -45
#define DRWAV_ALREADY_CONNECTED -46
#define DRWAV_NOT_CONNECTED -47
#define DRWAV_CONNECTION_REFUSED -48
#define DRWAV_NO_HOST -49
#define DRWAV_IN_PROGRESS -50
#define DRWAV_CANCELLED -51
#define DRWAV_MEMORY_ALREADY_MAPPED -52
#define DRWAV_AT_END -53

/* Common data formats. */
#define DR_WAVE_FORMAT_PCM 0x1
#define DR_WAVE_FORMAT_ADPCM 0x2
#define DR_WAVE_FORMAT_IEEE_FLOAT 0x3
#define DR_WAVE_FORMAT_ALAW 0x6
#define DR_WAVE_FORMAT_MULAW 0x7
#define DR_WAVE_FORMAT_DVI_ADPCM 0x11
#define DR_WAVE_FORMAT_EXTENSIBLE 0xFFFE

/* Flags to pass into drwav_init_ex(), etc. */
#define DRWAV_SEQUENTIAL 0x00000001

DRWAV_API void drwav_version(drwav_uint32* pMajor, drwav_uint32* pMinor, drwav_uint32* pRevision);
DRWAV_API const char* drwav_version_string(void);

typedef enum { drwav_seek_origin_start, drwav_seek_origin_current } drwav_seek_origin;

typedef enum { drwav_container_riff, drwav_container_w64, drwav_container_rf64 } drwav_container;

typedef struct {
  union {
    drwav_uint8 fourcc[4];
    drwav_uint8 guid[16];
  } id;

  /* The size in bytes of the chunk. */
  drwav_uint64 sizeInBytes;

  /*
  RIFF = 2 byte alignment.
  W64  = 8 byte alignment.
  */
  unsigned int paddingSize;
} drwav_chunk_header;

typedef struct {
  /*
  The format tag exactly as specified in the wave file's "fmt" chunk. This can
  be used by applications that require support for data formats not natively
  supported by dr_wav.
  */
  drwav_uint16 formatTag;

  /* The number of channels making up the audio data. When this is set to 1 it
   * is mono, 2 is stereo, etc. */
  drwav_uint16 channels;

  /* The sample rate. Usually set to something like 44100. */
  drwav_uint32 sampleRate;

  /* Average bytes per second. You probably don't need this, but it's left here
   * for informational purposes. */
  drwav_uint32 avgBytesPerSec;

  /* Block align. This is equal to the number of channels * bytes per sample. */
  drwav_uint16 blockAlign;

  /* Bits per sample. */
  drwav_uint16 bitsPerSample;

  /* The size of the extended data. Only used internally for validation, but
   * left here for informational purposes. */
  drwav_uint16 extendedSize;

  /*
  The number of valid bits per sample. When <formatTag> is equal to
  WAVE_FORMAT_EXTENSIBLE, <bitsPerSample> is always rounded up to the nearest
  multiple of 8. This variable contains information about exactly how many bits
  are valid per sample. Mainly used for informational purposes.
  */
  drwav_uint16 validBitsPerSample;

  /* The channel mask. Not used at the moment. */
  drwav_uint32 channelMask;

  /* The sub-format, exactly as specified by the wave file. */
  drwav_uint8 subFormat[16];
} drwav_fmt;

DRWAV_API drwav_uint16 drwav_fmt_get_format(const drwav_fmt* pFMT);

/*
Callback for when data is read. Return value is the number of bytes actually
read.

pUserData   [in]  The user data that was passed to drwav_init() and family.
pBufferOut  [out] The output buffer.
bytesToRead [in]  The number of bytes to read.

Returns the number of bytes actually read.

A return value of less than bytesToRead indicates the end of the stream. Do
_not_ return from this callback until either the entire bytesToRead is filled or
you have reached the end of the stream.
*/
typedef size_t (*drwav_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

/*
Callback for when data is written. Returns value is the number of bytes actually
written.

pUserData    [in]  The user data that was passed to drwav_init_write() and
family. pData        [out] A pointer to the data to write. bytesToWrite [in] The
number of bytes to write.

Returns the number of bytes actually written.

If the return value differs from bytesToWrite, it indicates an error.
*/
typedef size_t (*drwav_write_proc)(void* pUserData, const void* pData, size_t bytesToWrite);

/*
Callback for when data needs to be seeked.

pUserData [in] The user data that was passed to drwav_init() and family.
offset    [in] The number of bytes to move, relative to the origin. Will never
be negative. origin    [in] The origin of the seek - the current position or the
start of the stream.

Returns whether or not the seek was successful.

Whether or not it is relative to the beginning or current position is determined
by the "origin" parameter which will be either drwav_seek_origin_start or
drwav_seek_origin_current.
*/
typedef drwav_bool32 (*drwav_seek_proc)(void* pUserData, int offset, drwav_seek_origin origin);

/*
Callback for when drwav_init_ex() finds a chunk.

pChunkUserData    [in] The user data that was passed to the pChunkUserData
parameter of drwav_init_ex() and family. onRead            [in] A pointer to the
function to call when reading. onSeek            [in] A pointer to the function
to call when seeking. pReadSeekUserData [in] The user data that was passed to
the pReadSeekUserData parameter of drwav_init_ex() and family. pChunkHeader [in]
A pointer to an object containing basic header information about the chunk. Use
this to identify the chunk. container         [in] Whether or not the WAV file
is a RIFF or Wave64 container. If you're unsure of the difference, assume RIFF.
pFMT              [in] A pointer to the object containing the contents of the
"fmt" chunk.

Returns the number of bytes read + seeked.

To read data from the chunk, call onRead(), passing in pReadSeekUserData as the
first parameter. Do the same for seeking with onSeek(). The return value must be
the total number of bytes you have read _plus_ seeked.

Use the `container` argument to discriminate the fields in `pChunkHeader->id`.
If the container is `drwav_container_riff` or `drwav_container_rf64` you should
use `id.fourcc`, otherwise you should use `id.guid`.

The `pFMT` parameter can be used to determine the data format of the wave file.
Use `drwav_fmt_get_format()` to get the sample format, which will be one of the
`DR_WAVE_FORMAT_*` identifiers.

The read pointer will be sitting on the first byte after the chunk's header. You
must not attempt to read beyond the boundary of the chunk.
*/
typedef drwav_uint64 (*drwav_chunk_proc)(void* pChunkUserData, drwav_read_proc onRead,
                                         drwav_seek_proc onSeek, void* pReadSeekUserData,
                                         const drwav_chunk_header* pChunkHeader,
                                         drwav_container container, const drwav_fmt* pFMT);

typedef struct {
  void* pUserData;
  void* (*onMalloc)(size_t sz, void* pUserData);
  void* (*onRealloc)(void* p, size_t sz, void* pUserData);
  void (*onFree)(void* p, void* pUserData);
} drwav_allocation_callbacks;

/* Structure for internal use. Only used for loaders opened with
 * drwav_init_memory(). */
typedef struct {
  const drwav_uint8* data;
  size_t dataSize;
  size_t currentReadPos;
} drwav__memory_stream;

/* Structure for internal use. Only used for writers opened with
 * drwav_init_memory_write(). */
typedef struct {
  void** ppData;
  size_t* pDataSize;
  size_t dataSize;
  size_t dataCapacity;
  size_t currentWritePos;
} drwav__memory_stream_write;

typedef struct {
  drwav_container container; /* RIFF, W64. */
  drwav_uint32 format;       /* DR_WAVE_FORMAT_* */
  drwav_uint32 channels;
  drwav_uint32 sampleRate;
  drwav_uint32 bitsPerSample;
} drwav_data_format;

typedef enum {
  drwav_metadata_type_none = 0,

  /*
  Unknown simply means a chunk that drwav does not handle specifically. You can
  still ask to receive these chunks as metadata objects. It is then up to you to
  interpret the chunk's data. You can also write unknown metadata to a wav file.
  Be careful writing unknown chunks if you have also edited the audio data. The
  unknown chunks could represent offsets/sizes that no longer correctly
  correspond to the audio data.
  */
  drwav_metadata_type_unknown = 1 << 0,

  /* Only 1 of each of these metadata items are allowed in a wav file. */
  drwav_metadata_type_smpl = 1 << 1,
  drwav_metadata_type_inst = 1 << 2,
  drwav_metadata_type_cue = 1 << 3,
  drwav_metadata_type_acid = 1 << 4,
  drwav_metadata_type_bext = 1 << 5,

  /*
  Wav files often have a LIST chunk. This is a chunk that contains a set of
  subchunks. For this higher-level metadata API, we don't make a distinction
  between a regular chunk and a LIST subchunk. Instead, they are all just
  'metadata' items.

  There can be multiple of these metadata items in a wav file.
  */
  drwav_metadata_type_list_label = 1 << 6,
  drwav_metadata_type_list_note = 1 << 7,
  drwav_metadata_type_list_labelled_cue_region = 1 << 8,

  drwav_metadata_type_list_info_software = 1 << 9,
  drwav_metadata_type_list_info_copyright = 1 << 10,
  drwav_metadata_type_list_info_title = 1 << 11,
  drwav_metadata_type_list_info_artist = 1 << 12,
  drwav_metadata_type_list_info_comment = 1 << 13,
  drwav_metadata_type_list_info_date = 1 << 14,
  drwav_metadata_type_list_info_genre = 1 << 15,
  drwav_metadata_type_list_info_album = 1 << 16,
  drwav_metadata_type_list_info_tracknumber = 1 << 17,

  /* Other type constants for convenience. */
  drwav_metadata_type_list_all_info_strings =
      drwav_metadata_type_list_info_software | drwav_metadata_type_list_info_copyright |
      drwav_metadata_type_list_info_title | drwav_metadata_type_list_info_artist |
      drwav_metadata_type_list_info_comment | drwav_metadata_type_list_info_date |
      drwav_metadata_type_list_info_genre | drwav_metadata_type_list_info_album |
      drwav_metadata_type_list_info_tracknumber,

  drwav_metadata_type_list_all_adtl = drwav_metadata_type_list_label |
                                      drwav_metadata_type_list_note |
                                      drwav_metadata_type_list_labelled_cue_region,

  drwav_metadata_type_all = -2,                  /*0xFFFFFFFF & ~drwav_metadata_type_unknown,*/
  drwav_metadata_type_all_including_unknown = -1 /*0xFFFFFFFF,*/
} drwav_metadata_type;

/*
Sampler Metadata

The sampler chunk contains information about how a sound should be played in the
context of a whole audio production, and when used in a sampler. See
https://en.wikipedia.org/wiki/Sample-based_synthesis.
*/
typedef enum {
  drwav_smpl_loop_type_forward = 0,
  drwav_smpl_loop_type_pingpong = 1,
  drwav_smpl_loop_type_backward = 2
} drwav_smpl_loop_type;

typedef struct {
  /* The ID of the associated cue point, see drwav_cue and drwav_cue_point. As
   * with all cue point IDs, this can correspond to a label chunk to give this
   * loop a name, see drwav_list_label_or_note. */
  drwav_uint32 cuePointId;

  /* See drwav_smpl_loop_type. */
  drwav_uint32 type;

  /* The byte offset of the first sample to be played in the loop. */
  drwav_uint32 firstSampleByteOffset;

  /* The byte offset into the audio data of the last sample to be played in the
   * loop. */
  drwav_uint32 lastSampleByteOffset;

  /* A value to represent that playback should occur at a point between samples.
   * This value ranges from 0 to UINT32_MAX. Where a value of 0 means no
   * fraction, and a value of (UINT32_MAX / 2) would mean half a sample. */
  drwav_uint32 sampleFraction;

  /* Number of times to play the loop. 0 means loop infinitely. */
  drwav_uint32 playCount;
} drwav_smpl_loop;

typedef struct {
  /* IDs for a particular MIDI manufacturer. 0 if not used. */
  drwav_uint32 manufacturerId;
  drwav_uint32 productId;

  /* The period of 1 sample in nanoseconds. */
  drwav_uint32 samplePeriodNanoseconds;

  /* The MIDI root note of this file. 0 to 127. */
  drwav_uint32 midiUnityNote;

  /* The fraction of a semitone up from the given MIDI note. This is a value
   * from 0 to UINT32_MAX, where 0 means no change and (UINT32_MAX / 2) is half
   * a semitone (AKA 50 cents). */
  drwav_uint32 midiPitchFraction;

  /* Data relating to SMPTE standards which are used for syncing audio and
   * video. 0 if not used. */
  drwav_uint32 smpteFormat;
  drwav_uint32 smpteOffset;

  /* drwav_smpl_loop loops. */
  drwav_uint32 sampleLoopCount;

  /* Optional sampler-specific data. */
  drwav_uint32 samplerSpecificDataSizeInBytes;

  drwav_smpl_loop* pLoops;
  drwav_uint8* pSamplerSpecificData;
} drwav_smpl;

/*
Instrument Metadata

The inst metadata contains data about how a sound should be played as part of an
instrument. This commonly read by samplers. See
https://en.wikipedia.org/wiki/Sample-based_synthesis.
*/
typedef struct {
  drwav_int8 midiUnityNote; /* The root note of the audio as a MIDI note number.
                               0 to 127. */
  drwav_int8 fineTuneCents; /* -50 to +50 */
  drwav_int8 gainDecibels;  /* -64 to +64 */
  drwav_int8 lowNote;       /* 0 to 127 */
  drwav_int8 highNote;      /* 0 to 127 */
  drwav_int8 lowVelocity;   /* 1 to 127 */
  drwav_int8 highVelocity;  /* 1 to 127 */
} drwav_inst;

/*
Cue Metadata

Cue points are markers at specific points in the audio. They often come with an
associated piece of drwav_list_label_or_note metadata which contains the text
for the marker.
*/
typedef struct {
  /* Unique identification value. */
  drwav_uint32 id;

  /* Set to 0. This is only relevant if there is a 'playlist' chunk - which is
   * not supported by dr_wav. */
  drwav_uint32 playOrderPosition;

  /* Should always be "data". This represents the fourcc value of the chunk that
   * this cue point corresponds to. dr_wav only supports a single data chunk so
   * this should always be "data". */
  drwav_uint8 dataChunkId[4];

  /* Set to 0. This is only relevant if there is a wave list chunk. dr_wav, like
   * lots of readers/writers, do not support this. */
  drwav_uint32 chunkStart;

  /* Set to 0 for uncompressed formats. Else the last byte in compressed wave
   * data where decompression can begin to find the value of the corresponding
   * sample value. */
  drwav_uint32 blockStart;

  /* For uncompressed formats this is the byte offset of the cue point into the
   * audio data. For compressed formats this is relative to the block specified
   * with blockStart. */
  drwav_uint32 sampleByteOffset;
} drwav_cue_point;

typedef struct {
  drwav_uint32 cuePointCount;
  drwav_cue_point* pCuePoints;
} drwav_cue;

/*
Acid Metadata

This chunk contains some information about the time signature and the tempo of
the audio.
*/
typedef enum {
  drwav_acid_flag_one_shot = 1, /* If this is not set, then it is a loop instead of a one-shot. */
  drwav_acid_flag_root_note_set = 2,
  drwav_acid_flag_stretch = 4,
  drwav_acid_flag_disk_based = 8,
  drwav_acid_flag_acidizer = 16 /* Not sure what this means. */
} drwav_acid_flag;

typedef struct {
  /* A bit-field, see drwav_acid_flag. */
  drwav_uint32 flags;

  /* Valid if flags contains drwav_acid_flag_root_note_set. It represents the
   * MIDI root note the file - a value from 0 to 127. */
  drwav_uint16 midiUnityNote;

  /* Reserved values that should probably be ignored. reserved1 seems to often
   * be 128 and reserved2 is 0. */
  drwav_uint16 reserved1;
  float reserved2;

  /* Number of beats. */
  drwav_uint32 numBeats;

  /* The time signature of the audio. */
  drwav_uint16 meterDenominator;
  drwav_uint16 meterNumerator;

  /* Beats per minute of the track. Setting a value of 0 suggests that there is
   * no tempo. */
  float tempo;
} drwav_acid;

/*
Cue Label or Note metadata

These are 2 different types of metadata, but they have the exact same format.
Labels tend to be the more common and represent a short name for a cue point.
Notes might be used to represent a longer comment.
*/
typedef struct {
  /* The ID of a cue point that this label or note corresponds to. */
  drwav_uint32 cuePointId;

  /* Size of the string not including any null terminator. */
  drwav_uint32 stringLength;

  /* The string. The *init_with_metadata functions null terminate this for
   * convenience. */
  char* pString;
} drwav_list_label_or_note;

/*
BEXT metadata, also known as Broadcast Wave Format (BWF)

This metadata adds some extra description to an audio file. You must check the
version field to determine if the UMID or the loudness fields are valid.
*/
typedef struct {
  /*
  These top 3 fields, and the umid field are actually defined in the standard as
  a statically sized buffers. In order to reduce the size of this struct (and
  therefore the union in the metadata struct), we instead store these as
  pointers.
  */
  char* pDescription;         /* Can be NULL or a null-terminated string, must be <= 256
                                 characters. */
  char* pOriginatorName;      /* Can be NULL or a null-terminated string, must be <=
                                 32 characters. */
  char* pOriginatorReference; /* Can be NULL or a null-terminated string, must
                                 be <= 32 characters. */
  char pOriginationDate[10];  /* ASCII "yyyy:mm:dd". */
  char pOriginationTime[8];   /* ASCII "hh:mm:ss". */
  drwav_uint64 timeReference; /* First sample count since midnight. */
  drwav_uint16 version;       /* Version of the BWF, check this to see if the fields
                                 below are valid. */

  /*
  Unrestricted ASCII characters containing a collection of strings terminated by
  CR/LF. Each string shall contain a description of a coding process applied to
  the audio data.
  */
  char* pCodingHistory;
  drwav_uint32 codingHistorySize;

  /* Fields below this point are only valid if the version is 1 or above. */
  drwav_uint8* pUMID; /* Exactly 64 bytes of SMPTE UMID */

  /* Fields below this point are only valid if the version is 2 or above. */
  drwav_uint16 loudnessValue;        /* Integrated Loudness Value of the file in LUFS
                                        (multiplied by 100). */
  drwav_uint16 loudnessRange;        /* Loudness Range of the file in LU (multiplied by 100). */
  drwav_uint16 maxTruePeakLevel;     /* Maximum True Peak Level of the file
                                        expressed as dBTP (multiplied by 100). */
  drwav_uint16 maxMomentaryLoudness; /* Highest value of the Momentary Loudness Level of
                                        the file in LUFS (multiplied by 100). */
  drwav_uint16 maxShortTermLoudness; /* Highest value of the Short-Term Loudness Level of
                                        the file in LUFS (multiplied by 100). */
} drwav_bext;

/*
Info Text Metadata

There a many different types of information text that can be saved in this
format. This is where things like the album name, the artists, the year it was
produced, etc are saved. See drwav_metadata_type for the full list of types that
dr_wav supports.
*/
typedef struct {
  /* Size of the string not including any null terminator. */
  drwav_uint32 stringLength;

  /* The string. The *init_with_metadata functions null terminate this for
   * convenience. */
  char* pString;
} drwav_list_info_text;

/*
Labelled Cue Region Metadata

The labelled cue region metadata is used to associate some region of audio with
text. The region starts at a cue point, and extends for the given number of
samples.
*/
typedef struct {
  /* The ID of a cue point that this object corresponds to. */
  drwav_uint32 cuePointId;

  /* The number of samples from the cue point forwards that should be considered
   * this region */
  drwav_uint32 sampleLength;

  /* Four characters used to say what the purpose of this region is. */
  drwav_uint8 purposeId[4];

  /* Unsure of the exact meanings of these. It appears to be acceptable to set
   * them all to 0. */
  drwav_uint16 country;
  drwav_uint16 language;
  drwav_uint16 dialect;
  drwav_uint16 codePage;

  /* Size of the string not including any null terminator. */
  drwav_uint32 stringLength;

  /* The string. The *init_with_metadata functions null terminate this for
   * convenience. */
  char* pString;
} drwav_list_labelled_cue_region;

/*
Unknown Metadata

This chunk just represents a type of chunk that dr_wav does not understand.

Unknown metadata has a location attached to it. This is because wav files can
have a LIST chunk that contains subchunks. These LIST chunks can be one of two
types. An adtl list, or an INFO list. This enum is used to specify the location
of a chunk that dr_wav currently doesn't support.
*/
typedef enum {
  drwav_metadata_location_invalid,
  drwav_metadata_location_top_level,
  drwav_metadata_location_inside_info_list,
  drwav_metadata_location_inside_adtl_list
} drwav_metadata_location;

typedef struct {
  drwav_uint8 id[4];
  drwav_metadata_location chunkLocation;
  drwav_uint32 dataSizeInBytes;
  drwav_uint8* pData;
} drwav_unknown_metadata;

/*
Metadata is saved as a union of all the supported types.
*/
typedef struct {
  /* Determines which item in the union is valid. */
  drwav_metadata_type type;

  union {
    drwav_cue cue;
    drwav_smpl smpl;
    drwav_acid acid;
    drwav_inst inst;
    drwav_bext bext;
    drwav_list_label_or_note labelOrNote; /* List label or list note. */
    drwav_list_labelled_cue_region labelledCueRegion;
    drwav_list_info_text infoText; /* Any of the list info types. */
    drwav_unknown_metadata unknown;
  } data;
} drwav_metadata;

typedef struct {
  /* A pointer to the function to call when more data is needed. */
  drwav_read_proc onRead;

  /* A pointer to the function to call when data needs to be written. Only used
   * when the drwav object is opened in write mode. */
  drwav_write_proc onWrite;

  /* A pointer to the function to call when the wav file needs to be seeked. */
  drwav_seek_proc onSeek;

  /* The user data to pass to callbacks. */
  void* pUserData;

  /* Allocation callbacks. */
  drwav_allocation_callbacks allocationCallbacks;

  /* Whether or not the WAV file is formatted as a standard RIFF file or W64. */
  drwav_container container;

  /* Structure containing format information exactly as specified by the wav
   * file. */
  drwav_fmt fmt;

  /* The sample rate. Will be set to something like 44100. */
  drwav_uint32 sampleRate;

  /* The number of channels. This will be set to 1 for monaural streams, 2 for
   * stereo, etc. */
  drwav_uint16 channels;

  /* The bits per sample. Will be set to something like 16, 24, etc. */
  drwav_uint16 bitsPerSample;

  /* Equal to fmt.formatTag, or the value specified by fmt.subFormat if
   * fmt.formatTag is equal to 65534 (WAVE_FORMAT_EXTENSIBLE). */
  drwav_uint16 translatedFormatTag;

  /* The total number of PCM frames making up the audio data. */
  drwav_uint64 totalPCMFrameCount;

  /* The size in bytes of the data chunk. */
  drwav_uint64 dataChunkDataSize;

  /* The position in the stream of the first data byte of the data chunk. This
   * is used for seeking. */
  drwav_uint64 dataChunkDataPos;

  /* The number of bytes remaining in the data chunk. */
  drwav_uint64 bytesRemaining;

  /* The current read position in PCM frames. */
  drwav_uint64 readCursorInPCMFrames;

  /*
  Only used in sequential write mode. Keeps track of the desired size of the
  "data" chunk at the point of initialization time. Always set to 0 for
  non-sequential writes and when the drwav object is opened in read mode. Used
  for validation.
  */
  drwav_uint64 dataChunkDataSizeTargetWrite;

  /* Keeps track of whether or not the wav writer was initialized in sequential
   * mode. */
  drwav_bool32 isSequentialWrite;

  /* A bit-field of drwav_metadata_type values, only bits set in this variable
   * are parsed and saved */
  drwav_metadata_type allowedMetadataTypes;

  /* A array of metadata. This is valid after the *init_with_metadata call
   * returns. It will be valid until drwav_uninit() is called. You can take
   * ownership of this data with drwav_take_ownership_of_metadata(). */
  drwav_metadata* pMetadata;
  drwav_uint32 metadataCount;

  /* A hack to avoid a DRWAV_MALLOC() when opening a decoder with
   * drwav_init_memory(). */
  drwav__memory_stream memoryStream;
  drwav__memory_stream_write memoryStreamWrite;

  /* Microsoft ADPCM specific data. */
  struct {
    drwav_uint32 bytesRemainingInBlock;
    drwav_uint16 predictor[2];
    drwav_int32 delta[2];
    drwav_int32 cachedFrames[4]; /* Samples are stored in this cache during decoding. */
    drwav_uint32 cachedFrameCount;
    drwav_int32 prevFrames[2][2]; /* The previous 2 samples for each channel (2
                                     channels at most). */
  } msadpcm;

  /* IMA ADPCM specific data. */
  struct {
    drwav_uint32 bytesRemainingInBlock;
    drwav_int32 predictor[2];
    drwav_int32 stepIndex[2];
    drwav_int32 cachedFrames[16]; /* Samples are stored in this cache during
                                     decoding. */
    drwav_uint32 cachedFrameCount;
  } ima;
} drwav;

/*
Initializes a pre-allocated drwav object for reading.

pWav                         [out]          A pointer to the drwav object being
initialized. onRead                       [in]           The function to call
when data needs to be read from the client. onSeek                       [in]
The function to call when the read position of the client data needs to move.
onChunk                      [in, optional] The function to call when a chunk is
enumerated at initialized time. pUserData, pReadSeekUserData [in, optional] A
pointer to application defined data that will be passed to onRead and onSeek.
pChunkUserData               [in, optional] A pointer to application defined
data that will be passed to onChunk. flags                        [in, optional]
A set of flags for controlling how things are loaded.

Returns true if successful; false otherwise.

Close the loader with drwav_uninit().

This is the lowest level function for initializing a WAV file. You can also use
drwav_init_file() and drwav_init_memory() to open the stream from a file or from
a block of memory respectively.

Possible values for flags:
  DRWAV_SEQUENTIAL: Never perform a backwards seek while loading. This disables
the chunk callback and will cause this function to return as soon as the data
chunk is found. Any chunks after the data chunk will be ignored.

drwav_init() is equivalent to "drwav_init_ex(pWav, onRead, onSeek, NULL,
pUserData, NULL, 0);".

The onChunk callback is not called for the WAVE or FMT chunks. The contents of
the FMT chunk can be read from pWav->fmt after the function returns.

See also: drwav_init_file(), drwav_init_memory(), drwav_uninit()
*/
DRWAV_API drwav_bool32 drwav_init(drwav* pWav, drwav_read_proc onRead, drwav_seek_proc onSeek,
                                  void* pUserData,
                                  const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_ex(drwav* pWav, drwav_read_proc onRead, drwav_seek_proc onSeek,
                                     drwav_chunk_proc onChunk, void* pReadSeekUserData,
                                     void* pChunkUserData, drwav_uint32 flags,
                                     const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_with_metadata(
    drwav* pWav, drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData,
    drwav_uint32 flags, const drwav_allocation_callbacks* pAllocationCallbacks);

/*
Initializes a pre-allocated drwav object for writing.

onWrite               [in]           The function to call when data needs to be
written. onSeek                [in]           The function to call when the
write position needs to move. pUserData             [in, optional] A pointer to
application defined data that will be passed to onWrite and onSeek. metadata,
numMetadata [in, optional] An array of metadata objects that should be written
to the file. The array is not edited. You are responsible for this metadata
memory and it must maintain valid until drwav_uninit() is called.

Returns true if successful; false otherwise.

Close the writer with drwav_uninit().

This is the lowest level function for initializing a WAV file. You can also use
drwav_init_file_write() and drwav_init_memory_write() to open the stream from a
file or from a block of memory respectively.

If the total sample count is known, you can use drwav_init_write_sequential().
This avoids the need for dr_wav to perform a post-processing step for storing
the total sample count and the size of the data chunk which requires a backwards
seek.

See also: drwav_init_file_write(), drwav_init_memory_write(), drwav_uninit()
*/
DRWAV_API drwav_bool32 drwav_init_write(drwav* pWav, const drwav_data_format* pFormat,
                                        drwav_write_proc onWrite, drwav_seek_proc onSeek,
                                        void* pUserData,
                                        const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_write_sequential(
    drwav* pWav, const drwav_data_format* pFormat, drwav_uint64 totalSampleCount,
    drwav_write_proc onWrite, void* pUserData,
    const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_write_sequential_pcm_frames(
    drwav* pWav, const drwav_data_format* pFormat, drwav_uint64 totalPCMFrameCount,
    drwav_write_proc onWrite, void* pUserData,
    const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_write_with_metadata(
    drwav* pWav, const drwav_data_format* pFormat, drwav_write_proc onWrite, drwav_seek_proc onSeek,
    void* pUserData, const drwav_allocation_callbacks* pAllocationCallbacks,
    drwav_metadata* pMetadata, drwav_uint32 metadataCount);

/*
Utility function to determine the target size of the entire data to be written
(including all headers and chunks).

Returns the target size in bytes.

The metadata argument can be NULL meaning no metadata exists.

Useful if the application needs to know the size to allocate.

Only writing to the RIFF chunk and one data chunk is currently supported.

See also: drwav_init_write(), drwav_init_file_write(), drwav_init_memory_write()
*/
DRWAV_API drwav_uint64 drwav_target_write_size_bytes(const drwav_data_format* pFormat,
                                                     drwav_uint64 totalFrameCount,
                                                     drwav_metadata* pMetadata,
                                                     drwav_uint32 metadataCount);

/*
Take ownership of the metadata objects that were allocated via one of the
init_with_metadata() function calls. The init_with_metdata functions perform a
single heap allocation for this metadata.

Useful if you want the data to persist beyond the lifetime of the drwav object.

You must free the data returned from this function using drwav_free().
*/
DRWAV_API drwav_metadata* drwav_take_ownership_of_metadata(drwav* pWav);

/*
Uninitializes the given drwav object.

Use this only for objects initialized with drwav_init*() functions
(drwav_init(), drwav_init_ex(), drwav_init_write(),
drwav_init_write_sequential()).
*/
DRWAV_API drwav_result drwav_uninit(drwav* pWav);

/*
Reads raw audio data.

This is the lowest level function for reading audio data. It simply reads the
given number of bytes of the raw internal sample data.

Consider using drwav_read_pcm_frames_s16(), drwav_read_pcm_frames_s32() or
drwav_read_pcm_frames_f32() for reading sample data in a consistent format.

pBufferOut can be NULL in which case a seek will be performed.

Returns the number of bytes actually read.
*/
DRWAV_API size_t drwav_read_raw(drwav* pWav, size_t bytesToRead, void* pBufferOut);

/*
Reads up to the specified number of PCM frames from the WAV file.

The output data will be in the file's internal format, converted to
native-endian byte order. Use drwav_read_pcm_frames_s16/f32/s32() to read data
in a specific format.

If the return value is less than <framesToRead> it means the end of the file has
been reached or you have requested more PCM frames than can possibly fit in the
output buffer.

This function will only work when sample data is of a fixed size and
uncompressed. If you are using a compressed format consider using
drwav_read_raw() or drwav_read_pcm_frames_s16/s32/f32().

pBufferOut can be NULL in which case a seek will be performed.
*/
DRWAV_API drwav_uint64 drwav_read_pcm_frames(drwav* pWav, drwav_uint64 framesToRead,
                                             void* pBufferOut);
DRWAV_API drwav_uint64 drwav_read_pcm_frames_le(drwav* pWav, drwav_uint64 framesToRead,
                                                void* pBufferOut);
DRWAV_API drwav_uint64 drwav_read_pcm_frames_be(drwav* pWav, drwav_uint64 framesToRead,
                                                void* pBufferOut);

/*
Seeks to the given PCM frame.

Returns true if successful; false otherwise.
*/
DRWAV_API drwav_bool32 drwav_seek_to_pcm_frame(drwav* pWav, drwav_uint64 targetFrameIndex);

/*
Retrieves the current read position in pcm frames.
*/
DRWAV_API drwav_result drwav_get_cursor_in_pcm_frames(drwav* pWav, drwav_uint64* pCursor);

/*
Retrieves the length of the file.
*/
DRWAV_API drwav_result drwav_get_length_in_pcm_frames(drwav* pWav, drwav_uint64* pLength);

/*
Writes raw audio data.

Returns the number of bytes actually written. If this differs from bytesToWrite,
it indicates an error.
*/
DRWAV_API size_t drwav_write_raw(drwav* pWav, size_t bytesToWrite, const void* pData);

/*
Writes PCM frames.

Returns the number of PCM frames written.

Input samples need to be in native-endian byte order. On big-endian
architectures the input data will be converted to little-endian. Use
drwav_write_raw() to write raw audio data without performing any conversion.
*/
DRWAV_API drwav_uint64 drwav_write_pcm_frames(drwav* pWav, drwav_uint64 framesToWrite,
                                              const void* pData);
DRWAV_API drwav_uint64 drwav_write_pcm_frames_le(drwav* pWav, drwav_uint64 framesToWrite,
                                                 const void* pData);
DRWAV_API drwav_uint64 drwav_write_pcm_frames_be(drwav* pWav, drwav_uint64 framesToWrite,
                                                 const void* pData);

/* Conversion Utilities */
#ifndef DR_WAV_NO_CONVERSION_API

/*
Reads a chunk of audio data and converts it to signed 16-bit PCM samples.

pBufferOut can be NULL in which case a seek will be performed.

Returns the number of PCM frames actually read.

If the return value is less than <framesToRead> it means the end of the file has
been reached.
*/
DRWAV_API drwav_uint64 drwav_read_pcm_frames_s16(drwav* pWav, drwav_uint64 framesToRead,
                                                 drwav_int16* pBufferOut);
DRWAV_API drwav_uint64 drwav_read_pcm_frames_s16le(drwav* pWav, drwav_uint64 framesToRead,
                                                   drwav_int16* pBufferOut);
DRWAV_API drwav_uint64 drwav_read_pcm_frames_s16be(drwav* pWav, drwav_uint64 framesToRead,
                                                   drwav_int16* pBufferOut);

/* Low-level function for converting unsigned 8-bit PCM samples to signed 16-bit
 * PCM samples. */
DRWAV_API void drwav_u8_to_s16(drwav_int16* pOut, const drwav_uint8* pIn, size_t sampleCount);

/* Low-level function for converting signed 24-bit PCM samples to signed 16-bit
 * PCM samples. */
DRWAV_API void drwav_s24_to_s16(drwav_int16* pOut, const drwav_uint8* pIn, size_t sampleCount);

/* Low-level function for converting signed 32-bit PCM samples to signed 16-bit
 * PCM samples. */
DRWAV_API void drwav_s32_to_s16(drwav_int16* pOut, const drwav_int32* pIn, size_t sampleCount);

/* Low-level function for converting IEEE 32-bit floating point samples to
 * signed 16-bit PCM samples. */
DRWAV_API void drwav_f32_to_s16(drwav_int16* pOut, const float* pIn, size_t sampleCount);

/* Low-level function for converting IEEE 64-bit floating point samples to
 * signed 16-bit PCM samples. */
DRWAV_API void drwav_f64_to_s16(drwav_int16* pOut, const double* pIn, size_t sampleCount);

/* Low-level function for converting A-law samples to signed 16-bit PCM samples.
 */
DRWAV_API void drwav_alaw_to_s16(drwav_int16* pOut, const drwav_uint8* pIn, size_t sampleCount);

/* Low-level function for converting u-law samples to signed 16-bit PCM samples.
 */
DRWAV_API void drwav_mulaw_to_s16(drwav_int16* pOut, const drwav_uint8* pIn, size_t sampleCount);

/*
Reads a chunk of audio data and converts it to IEEE 32-bit floating point
samples.

pBufferOut can be NULL in which case a seek will be performed.

Returns the number of PCM frames actually read.

If the return value is less than <framesToRead> it means the end of the file has
been reached.
*/
DRWAV_API drwav_uint64 drwav_read_pcm_frames_f32(drwav* pWav, drwav_uint64 framesToRead,
                                                 float* pBufferOut);
DRWAV_API drwav_uint64 drwav_read_pcm_frames_f32le(drwav* pWav, drwav_uint64 framesToRead,
                                                   float* pBufferOut);
DRWAV_API drwav_uint64 drwav_read_pcm_frames_f32be(drwav* pWav, drwav_uint64 framesToRead,
                                                   float* pBufferOut);

/* Low-level function for converting unsigned 8-bit PCM samples to IEEE 32-bit
 * floating point samples. */
DRWAV_API void drwav_u8_to_f32(float* pOut, const drwav_uint8* pIn, size_t sampleCount);

/* Low-level function for converting signed 16-bit PCM samples to IEEE 32-bit
 * floating point samples. */
DRWAV_API void drwav_s16_to_f32(float* pOut, const drwav_int16* pIn, size_t sampleCount);

/* Low-level function for converting signed 24-bit PCM samples to IEEE 32-bit
 * floating point samples. */
DRWAV_API void drwav_s24_to_f32(float* pOut, const drwav_uint8* pIn, size_t sampleCount);

/* Low-level function for converting signed 32-bit PCM samples to IEEE 32-bit
 * floating point samples. */
DRWAV_API void drwav_s32_to_f32(float* pOut, const drwav_int32* pIn, size_t sampleCount);

/* Low-level function for converting IEEE 64-bit floating point samples to IEEE
 * 32-bit floating point samples. */
DRWAV_API void drwav_f64_to_f32(float* pOut, const double* pIn, size_t sampleCount);

/* Low-level function for converting A-law samples to IEEE 32-bit floating point
 * samples. */
DRWAV_API void drwav_alaw_to_f32(float* pOut, const drwav_uint8* pIn, size_t sampleCount);

/* Low-level function for converting u-law samples to IEEE 32-bit floating point
 * samples. */
DRWAV_API void drwav_mulaw_to_f32(float* pOut, const drwav_uint8* pIn, size_t sampleCount);

/*
Reads a chunk of audio data and converts it to signed 32-bit PCM samples.

pBufferOut can be NULL in which case a seek will be performed.

Returns the number of PCM frames actually read.

If the return value is less than <framesToRead> it means the end of the file has
been reached.
*/
DRWAV_API drwav_uint64 drwav_read_pcm_frames_s32(drwav* pWav, drwav_uint64 framesToRead,
                                                 drwav_int32* pBufferOut);
DRWAV_API drwav_uint64 drwav_read_pcm_frames_s32le(drwav* pWav, drwav_uint64 framesToRead,
                                                   drwav_int32* pBufferOut);
DRWAV_API drwav_uint64 drwav_read_pcm_frames_s32be(drwav* pWav, drwav_uint64 framesToRead,
                                                   drwav_int32* pBufferOut);

/* Low-level function for converting unsigned 8-bit PCM samples to signed 32-bit
 * PCM samples. */
DRWAV_API void drwav_u8_to_s32(drwav_int32* pOut, const drwav_uint8* pIn, size_t sampleCount);

/* Low-level function for converting signed 16-bit PCM samples to signed 32-bit
 * PCM samples. */
DRWAV_API void drwav_s16_to_s32(drwav_int32* pOut, const drwav_int16* pIn, size_t sampleCount);

/* Low-level function for converting signed 24-bit PCM samples to signed 32-bit
 * PCM samples. */
DRWAV_API void drwav_s24_to_s32(drwav_int32* pOut, const drwav_uint8* pIn, size_t sampleCount);

/* Low-level function for converting IEEE 32-bit floating point samples to
 * signed 32-bit PCM samples. */
DRWAV_API void drwav_f32_to_s32(drwav_int32* pOut, const float* pIn, size_t sampleCount);

/* Low-level function for converting IEEE 64-bit floating point samples to
 * signed 32-bit PCM samples. */
DRWAV_API void drwav_f64_to_s32(drwav_int32* pOut, const double* pIn, size_t sampleCount);

/* Low-level function for converting A-law samples to signed 32-bit PCM samples.
 */
DRWAV_API void drwav_alaw_to_s32(drwav_int32* pOut, const drwav_uint8* pIn, size_t sampleCount);

/* Low-level function for converting u-law samples to signed 32-bit PCM samples.
 */
DRWAV_API void drwav_mulaw_to_s32(drwav_int32* pOut, const drwav_uint8* pIn, size_t sampleCount);

#endif /* DR_WAV_NO_CONVERSION_API */

/* High-Level Convenience Helpers */

#ifndef DR_WAV_NO_STDIO
/*
Helper for initializing a wave file for reading using stdio.

This holds the internal FILE object until drwav_uninit() is called. Keep this in
mind if you're caching drwav objects because the operating system may restrict
the number of file handles an application can have open at any given time.
*/
DRWAV_API drwav_bool32 drwav_init_file(drwav* pWav, const char* filename,
                                       const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_file_ex(drwav* pWav, const char* filename,
                                          drwav_chunk_proc onChunk, void* pChunkUserData,
                                          drwav_uint32 flags,
                                          const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_file_w(drwav* pWav, const wchar_t* filename,
                                         const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_file_ex_w(drwav* pWav, const wchar_t* filename,
                                            drwav_chunk_proc onChunk, void* pChunkUserData,
                                            drwav_uint32 flags,
                                            const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32
drwav_init_file_with_metadata(drwav* pWav, const char* filename, drwav_uint32 flags,
                              const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32
drwav_init_file_with_metadata_w(drwav* pWav, const wchar_t* filename, drwav_uint32 flags,
                                const drwav_allocation_callbacks* pAllocationCallbacks);

/*
Helper for initializing a wave file for writing using stdio.

This holds the internal FILE object until drwav_uninit() is called. Keep this in
mind if you're caching drwav objects because the operating system may restrict
the number of file handles an application can have open at any given time.
*/
DRWAV_API drwav_bool32
drwav_init_file_write(drwav* pWav, const char* filename, const drwav_data_format* pFormat,
                      const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_file_write_sequential(
    drwav* pWav, const char* filename, const drwav_data_format* pFormat,
    drwav_uint64 totalSampleCount, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_file_write_sequential_pcm_frames(
    drwav* pWav, const char* filename, const drwav_data_format* pFormat,
    drwav_uint64 totalPCMFrameCount, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32
drwav_init_file_write_w(drwav* pWav, const wchar_t* filename, const drwav_data_format* pFormat,
                        const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_file_write_sequential_w(
    drwav* pWav, const wchar_t* filename, const drwav_data_format* pFormat,
    drwav_uint64 totalSampleCount, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_file_write_sequential_pcm_frames_w(
    drwav* pWav, const wchar_t* filename, const drwav_data_format* pFormat,
    drwav_uint64 totalPCMFrameCount, const drwav_allocation_callbacks* pAllocationCallbacks);
#endif /* DR_WAV_NO_STDIO */

/*
Helper for initializing a loader from a pre-allocated memory buffer.

This does not create a copy of the data. It is up to the application to ensure
the buffer remains valid for the lifetime of the drwav object.

The buffer should contain the contents of the entire wave file, not just the
sample data.
*/
DRWAV_API drwav_bool32 drwav_init_memory(drwav* pWav, const void* data, size_t dataSize,
                                         const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_memory_ex(drwav* pWav, const void* data, size_t dataSize,
                                            drwav_chunk_proc onChunk, void* pChunkUserData,
                                            drwav_uint32 flags,
                                            const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32
drwav_init_memory_with_metadata(drwav* pWav, const void* data, size_t dataSize, drwav_uint32 flags,
                                const drwav_allocation_callbacks* pAllocationCallbacks);

/*
Helper for initializing a writer which outputs data to a memory buffer.

dr_wav will manage the memory allocations, however it is up to the caller to
free the data with drwav_free().

The buffer will remain allocated even after drwav_uninit() is called. The buffer
should not be considered valid until after drwav_uninit() has been called.
*/
DRWAV_API drwav_bool32 drwav_init_memory_write(
    drwav* pWav, void** ppData, size_t* pDataSize, const drwav_data_format* pFormat,
    const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_memory_write_sequential(
    drwav* pWav, void** ppData, size_t* pDataSize, const drwav_data_format* pFormat,
    drwav_uint64 totalSampleCount, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_bool32 drwav_init_memory_write_sequential_pcm_frames(
    drwav* pWav, void** ppData, size_t* pDataSize, const drwav_data_format* pFormat,
    drwav_uint64 totalPCMFrameCount, const drwav_allocation_callbacks* pAllocationCallbacks);

#ifndef DR_WAV_NO_CONVERSION_API
/*
Opens and reads an entire wav file in a single operation.

The return value is a heap-allocated buffer containing the audio data. Use
drwav_free() to free the buffer.
*/
DRWAV_API drwav_int16*
drwav_open_and_read_pcm_frames_s16(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData,
                                   unsigned int* channelsOut, unsigned int* sampleRateOut,
                                   drwav_uint64* totalFrameCountOut,
                                   const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API float*
drwav_open_and_read_pcm_frames_f32(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData,
                                   unsigned int* channelsOut, unsigned int* sampleRateOut,
                                   drwav_uint64* totalFrameCountOut,
                                   const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_int32*
drwav_open_and_read_pcm_frames_s32(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData,
                                   unsigned int* channelsOut, unsigned int* sampleRateOut,
                                   drwav_uint64* totalFrameCountOut,
                                   const drwav_allocation_callbacks* pAllocationCallbacks);
#ifndef DR_WAV_NO_STDIO
/*
Opens and decodes an entire wav file in a single operation.

The return value is a heap-allocated buffer containing the audio data. Use
drwav_free() to free the buffer.
*/
DRWAV_API drwav_int16* drwav_open_file_and_read_pcm_frames_s16(
    const char* filename, unsigned int* channelsOut, unsigned int* sampleRateOut,
    drwav_uint64* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API float* drwav_open_file_and_read_pcm_frames_f32(
    const char* filename, unsigned int* channelsOut, unsigned int* sampleRateOut,
    drwav_uint64* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_int32* drwav_open_file_and_read_pcm_frames_s32(
    const char* filename, unsigned int* channelsOut, unsigned int* sampleRateOut,
    drwav_uint64* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_int16* drwav_open_file_and_read_pcm_frames_s16_w(
    const wchar_t* filename, unsigned int* channelsOut, unsigned int* sampleRateOut,
    drwav_uint64* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API float* drwav_open_file_and_read_pcm_frames_f32_w(
    const wchar_t* filename, unsigned int* channelsOut, unsigned int* sampleRateOut,
    drwav_uint64* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_int32* drwav_open_file_and_read_pcm_frames_s32_w(
    const wchar_t* filename, unsigned int* channelsOut, unsigned int* sampleRateOut,
    drwav_uint64* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks);
#endif
/*
Opens and decodes an entire wav file from a block of memory in a single
operation.

The return value is a heap-allocated buffer containing the audio data. Use
drwav_free() to free the buffer.
*/
DRWAV_API drwav_int16* drwav_open_memory_and_read_pcm_frames_s16(
    const void* data, size_t dataSize, unsigned int* channelsOut, unsigned int* sampleRateOut,
    drwav_uint64* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API float* drwav_open_memory_and_read_pcm_frames_f32(
    const void* data, size_t dataSize, unsigned int* channelsOut, unsigned int* sampleRateOut,
    drwav_uint64* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks);
DRWAV_API drwav_int32* drwav_open_memory_and_read_pcm_frames_s32(
    const void* data, size_t dataSize, unsigned int* channelsOut, unsigned int* sampleRateOut,
    drwav_uint64* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks);
#endif

/* Frees data that was allocated internally by dr_wav. */
DRWAV_API void drwav_free(void* p, const drwav_allocation_callbacks* pAllocationCallbacks);

/* Converts bytes from a wav stream to a sized type of native endian. */
DRWAV_API drwav_uint16 drwav_bytes_to_u16(const drwav_uint8* data);
DRWAV_API drwav_int16 drwav_bytes_to_s16(const drwav_uint8* data);
DRWAV_API drwav_uint32 drwav_bytes_to_u32(const drwav_uint8* data);
DRWAV_API drwav_int32 drwav_bytes_to_s32(const drwav_uint8* data);
DRWAV_API drwav_uint64 drwav_bytes_to_u64(const drwav_uint8* data);
DRWAV_API drwav_int64 drwav_bytes_to_s64(const drwav_uint8* data);
DRWAV_API float drwav_bytes_to_f32(const drwav_uint8* data);

/* Compares a GUID for the purpose of checking the type of a Wave64 chunk. */
DRWAV_API drwav_bool32 drwav_guid_equal(const drwav_uint8 a[16], const drwav_uint8 b[16]);

/* Compares a four-character-code for the purpose of checking the type of a RIFF
 * chunk. */
DRWAV_API drwav_bool32 drwav_fourcc_equal(const drwav_uint8* a, const char* b);

#ifdef __cplusplus
}
#endif
#endif /* dr_wav_h */

/*
REVISION HISTORY
================
v0.13.2 - 2021-10-02
  - Fix a possible buffer overflow when reading from compressed formats.

v0.13.1 - 2021-07-31
  - Fix platform detection for ARM64.

v0.13.0 - 2021-07-01
  - Improve support for reading and writing metadata. Use the `_with_metadata()`
APIs to initialize a WAV decoder and store the metadata within the `drwav`
object. Use the `pMetadata` and `metadataCount` members of the `drwav` object to
read the data. The old way of handling metadata via a callback is still usable
and valid.
  - API CHANGE: drwav_target_write_size_bytes() now takes extra parameters for
calculating the required write size when writing metadata.
  - Add drwav_get_cursor_in_pcm_frames()
  - Add drwav_get_length_in_pcm_frames()
  - Fix a bug where drwav_read_raw() can call the read callback with a byte
count of zero.

v0.12.20 - 2021-06-11
  - Fix some undefined behavior.

v0.12.19 - 2021-02-21
  - Fix a warning due to referencing _MSC_VER when it is undefined.
  - Minor improvements to the management of some internal state concerning the
data chunk cursor.

v0.12.18 - 2021-01-31
  - Clean up some static analysis warnings.

v0.12.17 - 2021-01-17
  - Minor fix to sample code in documentation.
  - Correctly qualify a private API as private rather than public.
  - Code cleanup.

v0.12.16 - 2020-12-02
  - Fix a bug when trying to read more bytes than can fit in a size_t.

v0.12.15 - 2020-11-21
  - Fix compilation with OpenWatcom.

v0.12.14 - 2020-11-13
  - Minor code clean up.

v0.12.13 - 2020-11-01
  - Improve compiler support for older versions of GCC.

v0.12.12 - 2020-09-28
  - Add support for RF64.
  - Fix a bug in writing mode where the size of the RIFF chunk incorrectly
includes the header section.

v0.12.11 - 2020-09-08
  - Fix a compilation error on older compilers.

v0.12.10 - 2020-08-24
  - Fix a bug when seeking with ADPCM formats.

v0.12.9 - 2020-08-02
  - Simplify sized types.

v0.12.8 - 2020-07-25
  - Fix a compilation warning.

v0.12.7 - 2020-07-15
  - Fix some bugs on big-endian architectures.
  - Fix an error in s24 to f32 conversion.

v0.12.6 - 2020-06-23
  - Change drwav_read_*() to allow NULL to be passed in as the output buffer
which is equivalent to a forward seek.
  - Fix a buffer overflow when trying to decode invalid IMA-ADPCM files.
  - Add include guard for the implementation section.

v0.12.5 - 2020-05-27
  - Minor documentation fix.

v0.12.4 - 2020-05-16
  - Replace assert() with DRWAV_ASSERT().
  - Add compile-time and run-time version querying.
    - DRWAV_VERSION_MINOR
    - DRWAV_VERSION_MAJOR
    - DRWAV_VERSION_REVISION
    - DRWAV_VERSION_STRING
    - drwav_version()
    - drwav_version_string()

v0.12.3 - 2020-04-30
  - Fix compilation errors with VC6.

v0.12.2 - 2020-04-21
  - Fix a bug where drwav_init_file() does not close the file handle after
attempting to load an erroneous file.

v0.12.1 - 2020-04-13
  - Fix some pedantic warnings.

v0.12.0 - 2020-04-04
  - API CHANGE: Add container and format parameters to the chunk callback.
  - Minor documentation updates.

v0.11.5 - 2020-03-07
  - Fix compilation error with Visual Studio .NET 2003.

v0.11.4 - 2020-01-29
  - Fix some static analysis warnings.
  - Fix a bug when reading f32 samples from an A-law encoded stream.

v0.11.3 - 2020-01-12
  - Minor changes to some f32 format conversion routines.
  - Minor bug fix for ADPCM conversion when end of file is reached.

v0.11.2 - 2019-12-02
  - Fix a possible crash when using custom memory allocators without a custom
realloc() implementation.
  - Fix an integer overflow bug.
  - Fix a null pointer dereference bug.
  - Add limits to sample rate, channels and bits per sample to tighten up some
validation.

v0.11.1 - 2019-10-07
  - Internal code clean up.

v0.11.0 - 2019-10-06
  - API CHANGE: Add support for user defined memory allocation routines. This
system allows the program to specify their own memory allocation routines with a
user data pointer for client-specific contextual data. This adds an extra
parameter to the end of the following APIs:
    - drwav_init()
    - drwav_init_ex()
    - drwav_init_file()
    - drwav_init_file_ex()
    - drwav_init_file_w()
    - drwav_init_file_w_ex()
    - drwav_init_memory()
    - drwav_init_memory_ex()
    - drwav_init_write()
    - drwav_init_write_sequential()
    - drwav_init_write_sequential_pcm_frames()
    - drwav_init_file_write()
    - drwav_init_file_write_sequential()
    - drwav_init_file_write_sequential_pcm_frames()
    - drwav_init_file_write_w()
    - drwav_init_file_write_sequential_w()
    - drwav_init_file_write_sequential_pcm_frames_w()
    - drwav_init_memory_write()
    - drwav_init_memory_write_sequential()
    - drwav_init_memory_write_sequential_pcm_frames()
    - drwav_open_and_read_pcm_frames_s16()
    - drwav_open_and_read_pcm_frames_f32()
    - drwav_open_and_read_pcm_frames_s32()
    - drwav_open_file_and_read_pcm_frames_s16()
    - drwav_open_file_and_read_pcm_frames_f32()
    - drwav_open_file_and_read_pcm_frames_s32()
    - drwav_open_file_and_read_pcm_frames_s16_w()
    - drwav_open_file_and_read_pcm_frames_f32_w()
    - drwav_open_file_and_read_pcm_frames_s32_w()
    - drwav_open_memory_and_read_pcm_frames_s16()
    - drwav_open_memory_and_read_pcm_frames_f32()
    - drwav_open_memory_and_read_pcm_frames_s32()
    Set this extra parameter to NULL to use defaults which is the same as the
previous behaviour. Setting this NULL will use DRWAV_MALLOC, DRWAV_REALLOC and
DRWAV_FREE.
  - Add support for reading and writing PCM frames in an explicit endianness.
New APIs:
    - drwav_read_pcm_frames_le()
    - drwav_read_pcm_frames_be()
    - drwav_read_pcm_frames_s16le()
    - drwav_read_pcm_frames_s16be()
    - drwav_read_pcm_frames_f32le()
    - drwav_read_pcm_frames_f32be()
    - drwav_read_pcm_frames_s32le()
    - drwav_read_pcm_frames_s32be()
    - drwav_write_pcm_frames_le()
    - drwav_write_pcm_frames_be()
  - Remove deprecated APIs.
  - API CHANGE: The following APIs now return native-endian data. Previously
they returned little-endian data.
    - drwav_read_pcm_frames()
    - drwav_read_pcm_frames_s16()
    - drwav_read_pcm_frames_s32()
    - drwav_read_pcm_frames_f32()
    - drwav_open_and_read_pcm_frames_s16()
    - drwav_open_and_read_pcm_frames_s32()
    - drwav_open_and_read_pcm_frames_f32()
    - drwav_open_file_and_read_pcm_frames_s16()
    - drwav_open_file_and_read_pcm_frames_s32()
    - drwav_open_file_and_read_pcm_frames_f32()
    - drwav_open_file_and_read_pcm_frames_s16_w()
    - drwav_open_file_and_read_pcm_frames_s32_w()
    - drwav_open_file_and_read_pcm_frames_f32_w()
    - drwav_open_memory_and_read_pcm_frames_s16()
    - drwav_open_memory_and_read_pcm_frames_s32()
    - drwav_open_memory_and_read_pcm_frames_f32()

v0.10.1 - 2019-08-31
  - Correctly handle partial trailing ADPCM blocks.

v0.10.0 - 2019-08-04
  - Remove deprecated APIs.
  - Add wchar_t variants for file loading APIs:
      drwav_init_file_w()
      drwav_init_file_ex_w()
      drwav_init_file_write_w()
      drwav_init_file_write_sequential_w()
  - Add drwav_target_write_size_bytes() which calculates the total size in bytes
of a WAV file given a format and sample count.
  - Add APIs for specifying the PCM frame count instead of the sample count when
opening in sequential write mode: drwav_init_write_sequential_pcm_frames()
      drwav_init_file_write_sequential_pcm_frames()
      drwav_init_file_write_sequential_pcm_frames_w()
      drwav_init_memory_write_sequential_pcm_frames()
  - Deprecate drwav_open*() and drwav_close():
      drwav_open()
      drwav_open_ex()
      drwav_open_write()
      drwav_open_write_sequential()
      drwav_open_file()
      drwav_open_file_ex()
      drwav_open_file_write()
      drwav_open_file_write_sequential()
      drwav_open_memory()
      drwav_open_memory_ex()
      drwav_open_memory_write()
      drwav_open_memory_write_sequential()
      drwav_close()
  - Minor documentation updates.

v0.9.2 - 2019-05-21
  - Fix warnings.

v0.9.1 - 2019-05-05
  - Add support for C89.
  - Change license to choice of public domain or MIT-0.

v0.9.0 - 2018-12-16
  - API CHANGE: Add new reading APIs for reading by PCM frames instead of
samples. Old APIs have been deprecated and will be removed in v0.10.0.
Deprecated APIs and their replacements: drwav_read()                     ->
drwav_read_pcm_frames() drwav_read_s16()                 ->
drwav_read_pcm_frames_s16() drwav_read_f32()                 ->
drwav_read_pcm_frames_f32() drwav_read_s32()                 ->
drwav_read_pcm_frames_s32() drwav_seek_to_sample()           ->
drwav_seek_to_pcm_frame() drwav_write()                    ->
drwav_write_pcm_frames() drwav_open_and_read_s16()        ->
drwav_open_and_read_pcm_frames_s16() drwav_open_and_read_f32()        ->
drwav_open_and_read_pcm_frames_f32() drwav_open_and_read_s32()        ->
drwav_open_and_read_pcm_frames_s32() drwav_open_file_and_read_s16()   ->
drwav_open_file_and_read_pcm_frames_s16() drwav_open_file_and_read_f32()   ->
drwav_open_file_and_read_pcm_frames_f32() drwav_open_file_and_read_s32()   ->
drwav_open_file_and_read_pcm_frames_s32() drwav_open_memory_and_read_s16() ->
drwav_open_memory_and_read_pcm_frames_s16() drwav_open_memory_and_read_f32() ->
drwav_open_memory_and_read_pcm_frames_f32() drwav_open_memory_and_read_s32() ->
drwav_open_memory_and_read_pcm_frames_s32() drwav::totalSampleCount          ->
drwav::totalPCMFrameCount
  - API CHANGE: Rename drwav_open_and_read_file_*() to
drwav_open_file_and_read_*().
  - API CHANGE: Rename drwav_open_and_read_memory_*() to
drwav_open_memory_and_read_*().
  - Add built-in support for smpl chunks.
  - Add support for firing a callback for each chunk in the file at
initialization time.
    - This is enabled through the drwav_init_ex(), etc. family of APIs.
  - Handle invalid FMT chunks more robustly.

v0.8.5 - 2018-09-11
  - Const correctness.
  - Fix a potential stack overflow.

v0.8.4 - 2018-08-07
  - Improve 64-bit detection.

v0.8.3 - 2018-08-05
  - Fix C++ build on older versions of GCC.

v0.8.2 - 2018-08-02
  - Fix some big-endian bugs.

v0.8.1 - 2018-06-29
  - Add support for sequential writing APIs.
  - Disable seeking in write mode.
  - Fix bugs with Wave64.
  - Fix typos.

v0.8 - 2018-04-27
  - Bug fix.
  - Start using major.minor.revision versioning.

v0.7f - 2018-02-05
  - Restrict ADPCM formats to a maximum of 2 channels.

v0.7e - 2018-02-02
  - Fix a crash.

v0.7d - 2018-02-01
  - Fix a crash.

v0.7c - 2018-02-01
  - Set drwav.bytesPerSample to 0 for all compressed formats.
  - Fix a crash when reading 16-bit floating point WAV files. In this case
dr_wav will output silence for all format conversion reading APIs (*_s16, *_s32,
*_f32 APIs).
  - Fix some divide-by-zero errors.

v0.7b - 2018-01-22
  - Fix errors with seeking of compressed formats.
  - Fix compilation error when DR_WAV_NO_CONVERSION_API

v0.7a - 2017-11-17
  - Fix some GCC warnings.

v0.7 - 2017-11-04
  - Add writing APIs.

v0.6 - 2017-08-16
  - API CHANGE: Rename dr_* types to drwav_*.
  - Add support for custom implementations of malloc(), realloc(), etc.
  - Add support for Microsoft ADPCM.
  - Add support for IMA ADPCM (DVI, format code 0x11).
  - Optimizations to drwav_read_s16().
  - Bug fixes.

v0.5g - 2017-07-16
  - Change underlying type for booleans to unsigned.

v0.5f - 2017-04-04
  - Fix a minor bug with drwav_open_and_read_s16() and family.

v0.5e - 2016-12-29
  - Added support for reading samples as signed 16-bit integers. Use the _s16()
family of APIs for this.
  - Minor fixes to documentation.

v0.5d - 2016-12-28
  - Use drwav_int* and drwav_uint* sized types to improve compiler support.

v0.5c - 2016-11-11
  - Properly handle JUNK chunks that come before the FMT chunk.

v0.5b - 2016-10-23
  - A minor change to drwav_bool8 and drwav_bool32 types.

v0.5a - 2016-10-11
  - Fixed a bug with drwav_open_and_read() and family due to incorrect argument
ordering.
  - Improve A-law and mu-law efficiency.

v0.5 - 2016-09-29
  - API CHANGE. Swap the order of "channels" and "sampleRate" parameters in
drwav_open_and_read*(). Rationale for this is to keep it consistent with
dr_audio and dr_flac.

v0.4b - 2016-09-18
  - Fixed a typo in documentation.

v0.4a - 2016-09-18
  - Fixed a typo.
  - Change date format to ISO 8601 (YYYY-MM-DD)

v0.4 - 2016-07-13
  - API CHANGE. Make onSeek consistent with dr_flac.
  - API CHANGE. Rename drwav_seek() to drwav_seek_to_sample() for clarity and
consistency with dr_flac.
  - Added support for Sony Wave64.

v0.3a - 2016-05-28
  - API CHANGE. Return drwav_bool32 instead of int in onSeek callback.
  - Fixed a memory leak.

v0.3 - 2016-05-22
  - Lots of API changes for consistency.

v0.2a - 2016-05-16
  - Fixed Linux/GCC build.

v0.2 - 2016-05-11
  - Added support for reading data as signed 32-bit PCM for consistency with
dr_flac.

v0.1a - 2016-05-07
  - Fixed a bug in drwav_open_file() where the file handle would not be closed
if the loader failed to initialize.

v0.1 - 2016-05-04
  - Initial versioned release.
*/

/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2020 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
