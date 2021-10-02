/*
MP3 audio decoder. Choice of public domain or MIT-0. See license statements at
the end of this file. dr_mp3 - v0.6.31 - 2021-08-22

David Reid - mackron@gmail.com

GitHub: https://github.com/mackron/dr_libs

Based on minimp3 (https://github.com/lieff/minimp3) which is where the real work
was done. See the bottom of this file for differences between minimp3 and
dr_mp3.
*/

#ifndef dr_mp3_h
#define dr_mp3_h

#ifdef __cplusplus
extern "C" {
#endif

#define DRMP3_STRINGIFY(x) #x
#define DRMP3_XSTRINGIFY(x) DRMP3_STRINGIFY(x)

#define DRMP3_VERSION_MAJOR 0
#define DRMP3_VERSION_MINOR 6
#define DRMP3_VERSION_REVISION 31
#define DRMP3_VERSION_STRING                                                                       \
  DRMP3_XSTRINGIFY(DRMP3_VERSION_MAJOR)                                                            \
  "." DRMP3_XSTRINGIFY(DRMP3_VERSION_MINOR) "." DRMP3_XSTRINGIFY(DRMP3_VERSION_REVISION)

#include <stdbool.h> /* For true and false. */
#include <stddef.h>  /* For size_t. */
#include <stdint.h>  /* For sized types. */

#if !defined(DRMP3_API)
#if defined(DRMP3_DLL)
#if defined(_WIN32)
#define DRMP3_DLL_IMPORT __declspec(dllimport)
#define DRMP3_DLL_EXPORT __declspec(dllexport)
#define DRMP3_DLL_PRIVATE static
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define DRMP3_DLL_IMPORT __attribute__((visibility("default")))
#define DRMP3_DLL_EXPORT __attribute__((visibility("default")))
#define DRMP3_DLL_PRIVATE __attribute__((visibility("hidden")))
#else
#define DRMP3_DLL_IMPORT
#define DRMP3_DLL_EXPORT
#define DRMP3_DLL_PRIVATE static
#endif
#endif

#if defined(DR_MP3_IMPLEMENTATION) || defined(DRMP3_IMPLEMENTATION)
#define DRMP3_API DRMP3_DLL_EXPORT
#else
#define DRMP3_API DRMP3_DLL_IMPORT
#endif
#define DRMP3_PRIVATE DRMP3_DLL_PRIVATE
#else
#define DRMP3_API extern
#define DRMP3_PRIVATE static
#endif
#endif

typedef int32_t drmp3_result;
#define DRMP3_SUCCESS 0
#define DRMP3_ERROR -1 /* A generic error. */
#define DRMP3_INVALID_ARGS -2
#define DRMP3_INVALID_OPERATION -3
#define DRMP3_OUT_OF_MEMORY -4
#define DRMP3_OUT_OF_RANGE -5
#define DRMP3_ACCESS_DENIED -6
#define DRMP3_DOES_NOT_EXIST -7
#define DRMP3_ALREADY_EXISTS -8
#define DRMP3_TOO_MANY_OPEN_FILES -9
#define DRMP3_INVALID_FILE -10
#define DRMP3_TOO_BIG -11
#define DRMP3_PATH_TOO_LONG -12
#define DRMP3_NAME_TOO_LONG -13
#define DRMP3_NOT_DIRECTORY -14
#define DRMP3_IS_DIRECTORY -15
#define DRMP3_DIRECTORY_NOT_EMPTY -16
#define DRMP3_END_OF_FILE -17
#define DRMP3_NO_SPACE -18
#define DRMP3_BUSY -19
#define DRMP3_IO_ERROR -20
#define DRMP3_INTERRUPT -21
#define DRMP3_UNAVAILABLE -22
#define DRMP3_ALREADY_IN_USE -23
#define DRMP3_BAD_ADDRESS -24
#define DRMP3_BAD_SEEK -25
#define DRMP3_BAD_PIPE -26
#define DRMP3_DEADLOCK -27
#define DRMP3_TOO_MANY_LINKS -28
#define DRMP3_NOT_IMPLEMENTED -29
#define DRMP3_NO_MESSAGE -30
#define DRMP3_BAD_MESSAGE -31
#define DRMP3_NO_DATA_AVAILABLE -32
#define DRMP3_INVALID_DATA -33
#define DRMP3_TIMEOUT -34
#define DRMP3_NO_NETWORK -35
#define DRMP3_NOT_UNIQUE -36
#define DRMP3_NOT_SOCKET -37
#define DRMP3_NO_ADDRESS -38
#define DRMP3_BAD_PROTOCOL -39
#define DRMP3_PROTOCOL_UNAVAILABLE -40
#define DRMP3_PROTOCOL_NOT_SUPPORTED -41
#define DRMP3_PROTOCOL_FAMILY_NOT_SUPPORTED -42
#define DRMP3_ADDRESS_FAMILY_NOT_SUPPORTED -43
#define DRMP3_SOCKET_NOT_SUPPORTED -44
#define DRMP3_CONNECTION_RESET -45
#define DRMP3_ALREADY_CONNECTED -46
#define DRMP3_NOT_CONNECTED -47
#define DRMP3_CONNECTION_REFUSED -48
#define DRMP3_NO_HOST -49
#define DRMP3_IN_PROGRESS -50
#define DRMP3_CANCELLED -51
#define DRMP3_MEMORY_ALREADY_MAPPED -52
#define DRMP3_AT_END -53

#define DRMP3_MAX_PCM_FRAMES_PER_MP3_FRAME 1152
#define DRMP3_MAX_SAMPLES_PER_FRAME (DRMP3_MAX_PCM_FRAMES_PER_MP3_FRAME * 2)

#ifdef _MSC_VER
#define DRMP3_INLINE __forceinline
#elif defined(__GNUC__)
/*
I've had a bug report where GCC is emitting warnings about functions possibly
not being inlineable. This warning happens when the
__attribute__((always_inline)) attribute is defined without an "inline"
statement. I think therefore there must be some case where "__inline__" is not
always defined, thus the compiler emitting these warnings. When using -std=c89
or -ansi on the command line, we cannot use the "inline" keyword and instead
need to use "__inline__". In an attempt to work around this issue I am using
"__inline__" only when we're compiling in strict ANSI mode.
*/
#if defined(__STRICT_ANSI__)
#define DRMP3_INLINE __inline__ __attribute__((always_inline))
#else
#define DRMP3_INLINE inline __attribute__((always_inline))
#endif
#elif defined(__WATCOMC__)
#define DRMP3_INLINE __inline
#else
#define DRMP3_INLINE
#endif

DRMP3_API void drmp3_version(uint32_t* pMajor, uint32_t* pMinor, uint32_t* pRevision);
DRMP3_API const char* drmp3_version_string(void);

/*
Low Level Push API
==================
*/
typedef struct {
  int frame_bytes, channels, hz, layer, bitrate_kbps;
} drmp3dec_frame_info;

typedef struct {
  float mdct_overlap[2][9 * 32], qmf_state[15 * 2 * 32];
  int reserv, free_format_bytes;
  uint8_t header[4], reserv_buf[511];
} drmp3dec;

/* Initializes a low level decoder. */
DRMP3_API void drmp3dec_init(drmp3dec* dec);

/* Reads a frame from a low level decoder. */
DRMP3_API int drmp3dec_decode_frame(drmp3dec* dec, const uint8_t* mp3, int mp3_bytes, void* pcm,
                                    drmp3dec_frame_info* info);

/* Helper for converting between f32 and s16. */
DRMP3_API void drmp3dec_f32_to_s16(const float* in, int16_t* out, size_t num_samples);

/*
Main API (Pull API)
===================
*/
typedef enum { drmp3_seek_origin_start, drmp3_seek_origin_current } drmp3_seek_origin;

typedef struct {
  uint64_t seekPosInBytes;     /* Points to the first byte of an MP3 frame. */
  uint64_t pcmFrameIndex;      /* The index of the PCM frame this seek point targets. */
  uint16_t mp3FramesToDiscard; /* The number of whole MP3 frames to be
                                      discarded before pcmFramesToDiscard. */
  uint16_t pcmFramesToDiscard; /* The number of leading samples to read and discard.
                                  These are discarded after mp3FramesToDiscard. */
} drmp3_seek_point;

/*
Callback for when data is read. Return value is the number of bytes actually
read.

pUserData   [in]  The user data that was passed to drmp3_init(), drmp3_open()
and family. pBufferOut  [out] The output buffer. bytesToRead [in]  The number of
bytes to read.

Returns the number of bytes actually read.

A return value of less than bytesToRead indicates the end of the stream. Do
_not_ return from this callback until either the entire bytesToRead is filled or
you have reached the end of the stream.
*/
typedef size_t (*drmp3_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

/*
Callback for when data needs to be seeked.

pUserData [in] The user data that was passed to drmp3_init(), drmp3_open() and
family. offset    [in] The number of bytes to move, relative to the origin. Will
never be negative. origin    [in] The origin of the seek - the current position
or the start of the stream.

Returns whether or not the seek was successful.

Whether or not it is relative to the beginning or current position is determined
by the "origin" parameter which will be either drmp3_seek_origin_start or
drmp3_seek_origin_current.
*/
typedef bool (*drmp3_seek_proc)(void* pUserData, int offset, drmp3_seek_origin origin);

typedef struct {
  void* pUserData;
  void* (*onMalloc)(size_t sz, void* pUserData);
  void* (*onRealloc)(void* p, size_t sz, void* pUserData);
  void (*onFree)(void* p, void* pUserData);
} drmp3_allocation_callbacks;

typedef struct {
  uint32_t channels;
  uint32_t sampleRate;
} drmp3_config;

typedef struct {
  drmp3dec decoder;
  drmp3dec_frame_info frameInfo;
  uint32_t channels;
  uint32_t sampleRate;
  drmp3_read_proc onRead;
  drmp3_seek_proc onSeek;
  void* pUserData;
  drmp3_allocation_callbacks allocationCallbacks;
  uint32_t mp3FrameChannels;   /* The number of channels in the currently
                                      loaded MP3 frame. Internal use only. */
  uint32_t mp3FrameSampleRate; /* The sample rate of the currently loaded
                                      MP3 frame. Internal use only. */
  uint32_t pcmFramesConsumedInMP3Frame;
  uint32_t pcmFramesRemainingInMP3Frame;
  uint8_t pcmFrames[sizeof(float) * DRMP3_MAX_SAMPLES_PER_FRAME]; /* <-- Multipled by sizeof(float)
                                                                     to ensure there's enough room
                                                                     for DR_MP3_FLOAT_OUTPUT. */
  uint64_t currentPCMFrame;      /* The current PCM frame, globally, based on the
                                    output sample rate. Mainly used for seeking. */
  uint64_t streamCursor;         /* The current byte the decoder is sitting on in
                                        the raw stream. */
  drmp3_seek_point* pSeekPoints; /* NULL by default. Set with drmp3_bind_seek_table(). Memory
                                    is owned by the client. dr_mp3 will never attempt to free
                                    this pointer. */
  uint32_t seekPointCount;       /* The number of items in pSeekPoints. When set to 0
                                    assumes to no seek table. Defaults to zero. */
  size_t dataSize;
  size_t dataCapacity;
  size_t dataConsumed;
  uint8_t* pData;
  bool atEnd : 1;
  struct {
    const uint8_t* pData;
    size_t dataSize;
    size_t currentReadPos;
  } memory; /* Only used for decoders that were opened against a block of
               memory. */
} drmp3;

/*
Initializes an MP3 decoder.

onRead    [in]           The function to call when data needs to be read from
the client. onSeek    [in]           The function to call when the read position
of the client data needs to move. pUserData [in, optional] A pointer to
application defined data that will be passed to onRead and onSeek.

Returns true if successful; false otherwise.

Close the loader with drmp3_uninit().

See also: drmp3_init_file(), drmp3_init_memory(), drmp3_uninit()
*/
DRMP3_API drmp3* drmp3_init(drmp3_read_proc onRead, drmp3_seek_proc onSeek, void* pUserData,
                            const drmp3_allocation_callbacks* pAllocationCallbacks);

/*
Initializes an MP3 decoder from a block of memory.

This does not create a copy of the data. It is up to the application to ensure
the buffer remains valid for the lifetime of the drmp3 object.

The buffer should contain the contents of the entire MP3 file.
*/
DRMP3_API drmp3* drmp3_init_memory(const void* pData, size_t dataSize,
                                   const drmp3_allocation_callbacks* pAllocationCallbacks);

#ifndef DR_MP3_NO_STDIO
/*
Initializes an MP3 decoder from a file.

This holds the internal FILE object until drmp3_uninit() is called. Keep this in
mind if you're caching drmp3 objects because the operating system may restrict
the number of file handles an application can have open at any given time.
*/
DRMP3_API drmp3* drmp3_init_file(const char* pFilePath,
                                 const drmp3_allocation_callbacks* pAllocationCallbacks);
DRMP3_API drmp3* drmp3_init_file_w(const wchar_t* pFilePath,
                                   const drmp3_allocation_callbacks* pAllocationCallbacks);
#endif

/*
Uninitializes an MP3 decoder.
*/
DRMP3_API void drmp3_uninit(drmp3* pMP3);

/*
Reads PCM frames as interleaved 32-bit IEEE floating point PCM.

Note that framesToRead specifies the number of PCM frames to read, _not_ the
number of MP3 frames.
*/
DRMP3_API uint64_t drmp3_read_pcm_frames_f32(drmp3* pMP3, uint64_t framesToRead, float* pBufferOut);

/*
Reads PCM frames as interleaved signed 16-bit integer PCM.

Note that framesToRead specifies the number of PCM frames to read, _not_ the
number of MP3 frames.
*/
DRMP3_API uint64_t drmp3_read_pcm_frames_s16(drmp3* pMP3, uint64_t framesToRead,
                                             int16_t* pBufferOut);

/*
Seeks to a specific frame.

Note that this is _not_ an MP3 frame, but rather a PCM frame.
*/
DRMP3_API bool drmp3_seek_to_pcm_frame(drmp3* pMP3, uint64_t frameIndex);

/*
Calculates the total number of PCM frames in the MP3 stream. Cannot be used for
infinite streams such as internet radio. Runs in linear time. Returns 0 on
error.
*/
DRMP3_API uint64_t drmp3_get_pcm_frame_count(drmp3* pMP3);

/*
Calculates the total number of MP3 frames in the MP3 stream. Cannot be used for
infinite streams such as internet radio. Runs in linear time. Returns 0 on
error.
*/
DRMP3_API uint64_t drmp3_get_mp3_frame_count(drmp3* pMP3);

/*
Calculates the total number of MP3 and PCM frames in the MP3 stream. Cannot be
used for infinite streams such as internet radio. Runs in linear time. Returns 0
on error.

This is equivalent to calling drmp3_get_mp3_frame_count() and
drmp3_get_pcm_frame_count() except that it's more efficient.
*/
DRMP3_API bool drmp3_get_mp3_and_pcm_frame_count(drmp3* pMP3, uint64_t* pMP3FrameCount,
                                                 uint64_t* pPCMFrameCount);

/*
Calculates the seekpoints based on PCM frames. This is slow.

pSeekpoint count is a pointer to a uint32 containing the seekpoint count. On
input it contains the desired count. On output it contains the actual count. The
reason for this design is that the client may request too many seekpoints, in
which case dr_mp3 will return a corrected count.

Note that seektable seeking is not quite sample exact when the MP3 stream
contains inconsistent sample rates.
*/
DRMP3_API bool drmp3_calculate_seek_points(drmp3* pMP3, uint32_t* pSeekPointCount,
                                           drmp3_seek_point* pSeekPoints);

/*
Binds a seek table to the decoder.

This does _not_ make a copy of pSeekPoints - it only references it. It is up to
the application to ensure this remains valid while it is bound to the decoder.

Use drmp3_calculate_seek_points() to calculate the seek points.
*/
DRMP3_API bool drmp3_bind_seek_table(drmp3* pMP3, uint32_t seekPointCount,
                                     drmp3_seek_point* pSeekPoints);

/*
Opens an decodes an entire MP3 stream as a single operation.

On output pConfig will receive the channel count and sample rate of the stream.

Free the returned pointer with drmp3_free().
*/
DRMP3_API float*
drmp3_open_and_read_pcm_frames_f32(drmp3_read_proc onRead, drmp3_seek_proc onSeek, void* pUserData,
                                   drmp3_config* pConfig, uint64_t* pTotalFrameCount,
                                   const drmp3_allocation_callbacks* pAllocationCallbacks);
DRMP3_API int16_t*
drmp3_open_and_read_pcm_frames_s16(drmp3_read_proc onRead, drmp3_seek_proc onSeek, void* pUserData,
                                   drmp3_config* pConfig, uint64_t* pTotalFrameCount,
                                   const drmp3_allocation_callbacks* pAllocationCallbacks);

DRMP3_API float*
drmp3_open_memory_and_read_pcm_frames_f32(const void* pData, size_t dataSize, drmp3_config* pConfig,
                                          uint64_t* pTotalFrameCount,
                                          const drmp3_allocation_callbacks* pAllocationCallbacks);
DRMP3_API int16_t*
drmp3_open_memory_and_read_pcm_frames_s16(const void* pData, size_t dataSize, drmp3_config* pConfig,
                                          uint64_t* pTotalFrameCount,
                                          const drmp3_allocation_callbacks* pAllocationCallbacks);

#ifndef DR_MP3_NO_STDIO
DRMP3_API float*
drmp3_open_file_and_read_pcm_frames_f32(const char* filePath, drmp3_config* pConfig,
                                        uint64_t* pTotalFrameCount,
                                        const drmp3_allocation_callbacks* pAllocationCallbacks);
DRMP3_API int16_t*
drmp3_open_file_and_read_pcm_frames_s16(const char* filePath, drmp3_config* pConfig,
                                        uint64_t* pTotalFrameCount,
                                        const drmp3_allocation_callbacks* pAllocationCallbacks);
#endif

/*
Allocates a block of memory on the heap.
*/
DRMP3_API void* drmp3_malloc(size_t sz, const drmp3_allocation_callbacks* pAllocationCallbacks);

/*
Frees any memory that was allocated by a public drmp3 API.
*/
DRMP3_API void drmp3_free(void* p, const drmp3_allocation_callbacks* pAllocationCallbacks);

#ifdef __cplusplus
}
#endif
#endif /* dr_mp3_h */

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

/*
    https://github.com/lieff/minimp3
    To the extent possible under law, the author(s) have dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty. See
   <http://creativecommons.org/publicdomain/zero/1.0/>.
*/
