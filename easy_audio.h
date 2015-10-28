// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// General
// - This library is only concerned with playback and recording of raw audio data. It does not load audio files
//   such as WAV, OGG and MP3.
// - Before you can create an playback or recording device you need to first create a context.
// - Each backend (DirectSound, PulseAudio, etc.) has it's own context. Using easyaudio_create_context() will find
//   a backend implementation based on the platform easy_audio has been compiled for.
// - A context for a specific backend can be created as well. For example, easyaudio_create_context_dsound() will
//   create a context which uses DirectSound as it's backend.
// - Playback and recording devices are enumerated once when the context is created. Thus, when a device is plugged
//   in or unplugged it will not be detected by easy_audio and the context will need to be deleted and re-created.
// - Currently, easy_audio will only consider the first EASYAUDIO_MAX_DEVICE_COUNT playback and audio devices, which
//   is currently set to 16 and should be plenty for the vast majority of cases. Feel free to increase (or decrease)
//   this number to suit your own requirements.
//

//
// OPTIONS
//
// #define EASYAUDIO_NO_DIRECTSOUND
//   Disables support for the DirectSound backend. Note that at the moment this is the only backend available for
//   Windows platforms, so you will likely not want to set this. DirectSound will only be compiled on Win32 builds.
//

#ifndef easy_audio
#define easy_audio

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>


#define EASYAUDIO_MAX_DEVICE_COUNT  16


#if defined(_WIN32) && !defined(EASYAUDIO_NO_DIRECTSOUND)
#define EASYAUDIO_BUILD_DSOUND
#endif


// Data formats.
typedef enum
{
    easyaudio_format_pcm,
    easyaudio_format_float,
    easyaudio_format_alaw,
    easyaudio_format_mulaw

} easyaudio_format;


typedef struct easyaudio_context easyaudio_context;
typedef struct easyaudio_device easyaudio_device;
typedef struct easyaudio_buffer easyaudio_buffer;

typedef struct
{
    /// The description of the device.
    char description[256];

} easyaudio_device_info;

typedef struct
{
    /// The data format.
    easyaudio_format format;

    /// The number of channels. This should be 1 for mono, 2 for stereo.
    unsigned int channels;

    /// The sample rate.
    unsigned int sampleRate;

    /// The number of bits per sample.
    unsigned int bitsPerSample;

    /// The size in bytes of the data.
    unsigned int sizeInBytes;

    /// A pointer to the initial data.
    void* pInitialData;

} easyaudio_buffer_desc;


/// Creates a context which chooses an appropriate backend based on the given platform.
easyaudio_context* easyaudio_create_context();

#ifdef EASYAUDIO_BUILD_DSOUND
/// Creates a context that uses DirectSound for it's backend.
easyaudio_context* easyaudio_create_context_dsound();
#endif

/// Deletes the given context.
void easyaudio_delete_context(easyaudio_context* pContext);




///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// Playback
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/// Retrieves the number of playback devices that were enumerated when the context was created.
unsigned int easyaudio_get_playback_device_count(easyaudio_context* pContext);

/// Retrieves information about the device at the given index.
bool easyaudio_get_playback_device_info(easyaudio_context* pContext, unsigned int deviceIndex, easyaudio_device_info* pInfoOut);


/// Creates a playback device.
///
/// @param pContext    [in] A pointer to the main context.
/// @param deviceIndex [in] The index of the device.
///
/// @remarks
///     Use a device index of 0 to use the default playback device.
easyaudio_device* easyaudio_create_playback_device(easyaudio_context* pContext, unsigned int deviceIndex);

/// Deletes the given playback device.
void easyaudio_delete_playback_device(easyaudio_device* pDevice);


/// Create a buffer.
easyaudio_buffer* easyaudio_create_buffer(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc);

/// Deletes the given buffer.
void easyaudio_delete_buffer(easyaudio_buffer* pBuffer);

/// Sets the data of the given buffer.
void easyaudio_set_buffer_data(easyaudio_buffer* pBuffer, unsigned int offset, const void* pData, unsigned int dataSizeInBytes);

/// Begins playing the givne buffer.
void easyaudio_play(easyaudio_buffer* pBuffer, bool loop);



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// Recording
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////





///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// TESTING
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void easyaudio_dsound_test1();



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
