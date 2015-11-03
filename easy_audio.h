// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// If you've stumbled across this library, be aware that this is very, very early in development. A lot of APIs
// are very temporary, in particular the effects API which at the moment are tied very closely to DirectSound.
//
// Currently, the only backend available is DirectSound while I figure out the API. 
//
// General
// - This library is NOT thread safe. Functions can be called from any thread, but it is up to the host application
//   to do synchronization.
// - This library is only concerned with playback and recording of raw audio data. It does not load audio files
//   such as WAV, OGG and MP3.
// - Before you can create an output (playback) or input (recording) device you need to first create a context.
// - Each backend (DirectSound, ALSA, etc.) has it's own context. Using easyaudio_create_context() will find
//   a backend implementation based on the platform easy_audio has been compiled for.
// - A context for a specific backend can be created as well. For example, easyaudio_create_context_dsound() will
//   create a context which uses DirectSound as it's backend.
// - Currently, devices are enumerated once when the context is created. Thus, when a device is plugged in or
//   unplugged it will not be detected by easy_audio and the context will need to be deleted and re-created.
// - Currently, easy_audio will only consider the first EASYAUDIO_MAX_DEVICE_COUNT output and input devices, which
//   is currently set to 16 and should be plenty for the vast majority of cases. Feel free to increase (or decrease)
//   this number to suit your own requirements.
//
// Events
// - Events are handled via callbacks. The different types of events include stop, pause, play and markers.
// - The Stop event is fired when an output buffer is stopped, either from finishing it's playback or if it was
//   stopped manually.
// - The Pause event is fired when the output buffer is paused.
// - The Play event is fired when the output buffer begins being played from either a stopped or paused state.
// - A Marker event is fired when the playback position of an output buffer reaches a certain point within the
//   buffer. This is useful for streaming audio data because it can tell you when a particular section of the
//   buffer can be filled with new data.
// - Due to the inherent multi-threaded nature of audio playback, events can be fired from any thread. It is up
//   to the application to ensure events are handled safely.
// - Currently, the maximum number of markers is set by EASYAUDIO_MAX_MARKER_COUNT which is set to 4 by default. This
//   can be increased, however doing so increases memory usage for each sound buffer.
//
// Performance Considerations
// - Creating and deleting buffers should be considered an expensive operation because there is quite a bit of thread
//   management being performed under the hood. Prefer caching sound buffers.
//

//
// OPTIONS
//
// #define EASYAUDIO_NO_DIRECTSOUND
//   Disables support for the DirectSound backend. Note that at the moment this is the only backend available for
//   Windows platforms, so you will likely not want to set this. DirectSound will only be compiled on Win32 builds.
//

//
// TODO
//
// - DirectSound: Consider using Win32 critical sections instead of events where possible.
// - DirectSound: Remove the semaphore and replace with an auto-reset event.
// - Implement a better error handling API.
// - Implement effects
// - Implement velocity
// - Implement cones
// - Implement attenuation min/max distances
//

#ifndef easy_audio
#define easy_audio

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>


#define EASYAUDIO_MAX_DEVICE_COUNT  16
#define EASYAUDIO_MAX_MARKER_COUNT  4


#if defined(_WIN32) && !defined(EASYAUDIO_NO_DIRECTSOUND)
#define EASYAUDIO_BUILD_DSOUND
#endif


#define EASYAUDIO_EVENT_ID_STOP     0xFFFFFFFF
#define EASYAUDIO_EVENT_ID_PAUSE    0xFFFFFFFE
#define EASYAUDIO_EVENT_ID_PLAY     0xFFFFFFFD
#define EASYAUDIO_EVENT_ID_MARKER   0

#define EASYAUDIO_ENABLE_3D         (1 << 0)


// Data formats.
typedef enum
{
    easyaudio_format_pcm,
    easyaudio_format_float

} easyaudio_format;

// Playback states.
typedef enum
{
    easyaudio_stopped,
    easyaudio_paused,
    easyaudio_playing

} easyaudio_playback_state;

// Effect types.
typedef enum
{
    easyaudio_effect_type_none,
    easyaudio_effect_type_i3dl2reverb,
    easyaudio_effect_type_chorus,
    easyaudio_effect_type_compressor,
    easyaudio_effect_type_distortion,
    easyaudio_effect_type_echo,
    easyaudio_effect_type_flanger

} easyaudio_effect_type;


typedef struct easyaudio_context easyaudio_context;
typedef struct easyaudio_device easyaudio_device;
typedef struct easyaudio_buffer easyaudio_buffer;

typedef void (* easyaudio_event_callback_proc)(easyaudio_buffer* pBuffer, unsigned int eventID, void *pUserData);

typedef struct
{
    /// The description of the device.
    char description[256];

} easyaudio_device_info;

typedef struct
{
    /// Boolean flags.
    ///   EASYAUDIO_ENABLE_3D: Enable 3D positioning
    unsigned int flags;

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

typedef struct
{
    /// The effect type.
    easyaudio_effect_type type;

    struct
    {
        float room;
        float roomHF;
        float roomRolloffFactor;
        float decayTime;
        float reflections;
        float reflectionsDelay;
        float reverb;
        float reverbDelay;
        float diffusion;
        float density;
        float hfReference;
    } i3dl2reverb;

    struct
    {
        int waveform;
        int phase;
        float depth;
        float feedback;
        float frequency;
        float delay;
    } chorus;

    struct
    {
        float gain;
        float attack;
        float release;
        float threshold;
        float ratio;
        float predelay;
    } compressor;

    struct
    {
        float gain;
        float edge;
        float postEQCenterFrequency;
        float postEQBandwidth;
        float preLowpassCutoff;
    } distortion;

    struct
    {
        float wetDryMix;
        float feedback;
        float leftDelay;
        float rightDelay;
        float panDelay;
    } echo;

    struct
    {
        float wetDryMix;
        float depth;
        float feedback;
        float frequency;
        float waveform;
        float delay;
        float phase;
    } flanger;

} easyaudio_effect;


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
// OUTPUT / PLAYBACK
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/// Retrieves the number of output devices that were enumerated when the context was created.
unsigned int easyaudio_get_output_device_count(easyaudio_context* pContext);

/// Retrieves information about the device at the given index.
bool easyaudio_get_output_device_info(easyaudio_context* pContext, unsigned int deviceIndex, easyaudio_device_info* pInfoOut);


/// Creates a output device.
///
/// @param pContext    [in] A pointer to the main context.
/// @param deviceIndex [in] The index of the device.
///
/// @remarks
///     Use a device index of 0 to use the default output device.
easyaudio_device* easyaudio_create_output_device(easyaudio_context* pContext, unsigned int deviceIndex);

/// Deletes the given output device.
void easyaudio_delete_output_device(easyaudio_device* pDevice);


/// Create a buffer.
///
/// @remarks
///     This will fail if 3D positioning is requested when the sound has more than 1 channel.
easyaudio_buffer* easyaudio_create_buffer(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc, unsigned int extraDataSize);

/// Deletes the given buffer.
void easyaudio_delete_buffer(easyaudio_buffer* pBuffer);


/// Retrieves the size in bytes of the given buffer's extra data.
unsigned int easyaudio_get_buffer_extra_data_size(easyaudio_buffer* pBuffer);

/// Retrieves a pointer to the given buffer's extra data.
void* easyaudio_get_buffer_extra_data(easyaudio_buffer* pBuffer);


/// Sets the audio data of the given buffer.
void easyaudio_set_buffer_data(easyaudio_buffer* pBuffer, unsigned int offset, const void* pData, unsigned int dataSizeInBytes);


/// Begins or resumes playing the given buffer.
///
/// @remarks
///     If the sound is already playing, it will continue to play, but the \c loop setting will be replaced with that specified
///     by the most recent call.
void easyaudio_play(easyaudio_buffer* pBuffer, bool loop);

/// Pauses playback of the given buffer.
void easyaudio_pause(easyaudio_buffer* pBuffer);

/// Stops playback of the given buffer.
void easyaudio_stop(easyaudio_buffer* pBuffer);

/// Retrieves the playback state of the given buffer.
easyaudio_playback_state easyaudio_get_playback_state(easyaudio_buffer* pBuffer);


/// Sets the playback position for the given buffer.
void easyaudio_set_playback_position(easyaudio_buffer* pBuffer, unsigned int position);

/// Retrieves hte playback position of the given buffer.
unsigned int easyaudio_get_playback_position(easyaudio_buffer* pBuffer);


/// Sets the pan of the given buffer.
///
/// @remarks
///     This does nothing for 3D sounds.
void easyaudio_set_pan(easyaudio_buffer* pBuffer, float pan);

/// Retrieves the pan of the given buffer.
float easyaudio_get_pan(easyaudio_buffer* pBuffer);


/// Sets the volume of the given buffer.
/// 
/// @param volume [in] The new volume.
///
/// @remarks
///     Amplificiation is not currently supported, so the maximum value is 1. A value of 1 represents the volume of the original
///     data.
void easyaudio_set_volume(easyaudio_buffer* pBuffer, float volume);

/// Retrieves the volume of the sound.
float easyaudio_get_volume(easyaudio_buffer* pBuffer);


/// Removes every marker.
void easyaudio_remove_markers(easyaudio_buffer* pBuffer);

/// Registers the callback to fire when the playback position hits a certain position in the given buffer.
///
/// @param eventID [in] The event ID that will be passed to the callback and can be used to identify a specific marker.
///
/// @remarks
///     This will fail if the buffer is not in a stopped state.
///     @par
///     Set the event ID to EASYAUDIO_EVENT_ID_MARKER + n, where "n" is your own application-specific identifier.
bool easyaudio_register_marker_callback(easyaudio_buffer* pBuffer, unsigned int offsetInBytes, easyaudio_event_callback_proc callback, unsigned int eventID, void* pUserData);

/// Registers the callback to fire when the buffer stops playing.
///
/// @remarks
///     This will fail if the buffer is not in a stopped state.
///     @par
///     The will replace any previous callback.
bool easyaudio_register_stop_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);

/// Registers the callback to fire when the buffer is paused.
///
/// @remarks
///     This will fail if the buffer is not in a stopped state.
///     @par
///     The will replace any previous callback.
bool easyaudio_register_pause_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);

/// Registers the callback to fire when the buffer begins playing from either a stopped or paused state.
///
/// @remarks
///     This will fail if the buffer is not in a stopped state.
///     @par
///     The will replace any previous callback.
bool easyaudio_register_play_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);


/// Sets the position of the given buffer in 3D space.
///
/// @remarks
///     This does nothing for buffers that do not support 3D positioning.
void easyaudio_set_position(easyaudio_buffer* pBuffer, float x, float y, float z);

/// Retrieves the position of the given buffer in 3D space.
void easyaudio_get_position(easyaudio_buffer* pBuffer, float* pPosOut);

/// Sets the position of the listener for the given output device.
void easyaudio_set_listener_position(easyaudio_device* pDevice, float x, float y, float z);

/// Retrieves the position of the listner for the given output device.
void easyaudio_get_listener_position(easyaudio_device* pDevice, float* pPosOut);

/// Sets the orientation of the listener for the given output device.
void easyaudio_set_listener_orientation(easyaudio_device* pDevice, float forwardX, float forwardY, float forwardZ, float upX, float upY, float upZ);

/// Retrieves the orientation of the listener for the given output device.
void easyaudio_get_listener_orientation(easyaudio_device* pDevice, float* pForwardOut, float* pUpOut);




///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// INPUT / RECORDING
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////





///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// HIGH-LEVEL API
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//// HELPERS ////



//// STREAMING ////
typedef bool (* easyaudio_stream_read_proc)(void* pUserData, void* pDataOut, unsigned int bytesToRead, unsigned int* bytesReadOut);
typedef bool (* easyaudio_stream_seek_proc)(void* pUserData, unsigned int offsetInBytesFromStart);

typedef struct
{
    /// A pointer to the function to call when more data needs to be read.
    easyaudio_stream_read_proc read;

    /// Seeks source data from the beginning of the file.
    easyaudio_stream_seek_proc seek;

} easyaudio_streaming_callbacks;


/// Creates a buffer that's pre-configured for use for streaming audio data.
///
/// @remarks
///     This function is just a high-level convenience wrapper. The returned buffer is just a regular buffer with pre-configured
///     markers attached to the buffer. This will attach 3 markers in total which means there is only EASYAUDIO_MAX_MARKER_COUNT - 3
///     marker slots available to the application.
///     @par
///     You must play the buffer with easyaudio_play_streaming_buffer() because the underlying buffer management is slightly different
///     to a regular buffer.
///     @par
///     Looping and stop callbacks may be inaccurate by up to half a second.
///     @par
///     Callback functions use bytes to determine how much data to process. This is always a multiple of samples * channels, however.
///     @par
///     The first chunk of data is not loaded until the buffer is played with easyaudio_play_streaming_buffer().
easyaudio_buffer* easyaudio_create_streaming_buffer(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc, easyaudio_streaming_callbacks callbacks, void* pUserData);

/// Begins playing the given streaming buffer.
bool easyaudio_play_streaming_buffer(easyaudio_buffer* pBuffer, bool loop);



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
