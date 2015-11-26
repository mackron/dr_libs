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


#define EASYAUDIO_MAX_DEVICE_COUNT          16
#define EASYAUDIO_MAX_MARKER_COUNT          4
#define EASYAUDIO_MAX_MESSAGE_QUEUE_SIZE    1024        // The maximum number of messages that can be cached in the internal message queues.


#if defined(_WIN32) && !defined(EASYAUDIO_NO_DIRECTSOUND)
#define EASYAUDIO_BUILD_DSOUND
#endif


#define EASYAUDIO_EVENT_ID_STOP     0xFFFFFFFF
#define EASYAUDIO_EVENT_ID_PAUSE    0xFFFFFFFE
#define EASYAUDIO_EVENT_ID_PLAY     0xFFFFFFFD
#define EASYAUDIO_EVENT_ID_MARKER   0

#define EASYAUDIO_ENABLE_3D         (1 << 0)
#define EASYAUDIO_RELATIVE_3D       (1 << 1)        // <-- Uses relative 3D positioning by default instead of absolute. Only used if EASYAUDIO_ENABLE_3D is also specified.


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

// 3D processing modes.
typedef enum
{
    easyaudio_3d_mode_absolute,
    easyaudio_3d_mode_relative,
    easyaudio_3d_mode_disabled

} easyaudio_3d_mode;


typedef struct easyaudio_context easyaudio_context;
typedef struct easyaudio_device easyaudio_device;
typedef struct easyaudio_buffer easyaudio_buffer;

typedef void (* easyaudio_event_callback_proc)(easyaudio_buffer* pBuffer, unsigned int eventID, void *pUserData);

typedef struct
{
    /// The callback function.
    easyaudio_event_callback_proc callback;

    /// The user data.
    void* pUserData;

} easyaudio_event_callback;

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

/// Determines whether or not the given audio buffer is looping.
bool easyaudio_is_looping(easyaudio_buffer* pBuffer);


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
///     This will fail if the buffer is not in a stopped state and the callback is non-null. It is fine to call this
///     with a null callback while the buffer is in the middle of playback in which case the callback will be cleared.
///     @par
///     The will replace any previous callback.
bool easyaudio_register_stop_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);

/// Registers the callback to fire when the buffer is paused.
///
/// @remarks
///     This will fail if the buffer is not in a stopped state and the callback is non-null. It is fine to call this
///     with a null callback while the buffer is in the middle of playback in which case the callback will be cleared.
///     @par
///     The will replace any previous callback.
bool easyaudio_register_pause_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);

/// Registers the callback to fire when the buffer begins playing from either a stopped or paused state.
///
/// @remarks
///     This will fail if the buffer is not in a stopped state and the callback is non-null. It is fine to call this
///     with a null callback while the buffer is in the middle of playback in which case the callback will be cleared.
///     @par
///     The will replace any previous callback.
bool easyaudio_register_play_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);


/// Retrieves the callback that is currently set for the stop event.
easyaudio_event_callback easyaudio_get_stop_callback(easyaudio_buffer* pBuffer);

/// Retrieves the callback that is currently set for the pause event.
easyaudio_event_callback easyaudio_get_pause_callback(easyaudio_buffer* pBuffer);

/// Retrieves the callback that is currently set for the play event.
easyaudio_event_callback easyaudio_get_play_callback(easyaudio_buffer* pBuffer);


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


/// Sets the 3D processing mode (absolute, relative or disabled).
///
/// @remarks
///     This applies to position, orientation and velocity.
void easyaudio_set_3d_mode(easyaudio_buffer* pBuffer, easyaudio_3d_mode mode);

/// Retrieves the 3D processing mode (absolute, relative or disabled).
easyaudio_3d_mode easyaudio_get_3d_mode(easyaudio_buffer* pBuffer);



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

//// SYNCHRONIZATION ////

typedef void* easyaudio_mutex;

/// Creates a mutex object.
easyaudio_mutex easyaudio_create_mutex();

/// Deletes a mutex object.
void easyaudio_delete_mutex(easyaudio_mutex mutex);

/// Locks the given mutex.
void easyaudio_lock_mutex(easyaudio_mutex mutex);

/// Unlocks the given mutex.
void easyaudio_unlock_mutex(easyaudio_mutex mutex);




//// STREAMING ////
typedef int ea_bool;

typedef ea_bool (* easyaudio_stream_read_proc)(void* pUserData, void* pDataOut, unsigned int bytesToRead, unsigned int* bytesReadOut);
typedef ea_bool (* easyaudio_stream_seek_proc)(void* pUserData, unsigned int offsetInBytesFromStart);

typedef struct
{
    /// A pointer to the user data to pass to each callback.
    void* pUserData;

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
easyaudio_buffer* easyaudio_create_streaming_buffer(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc, easyaudio_streaming_callbacks callbacks, unsigned int extraDataSize);

/// Retrieves the size of the extra data of the given streaming buffer..
unsigned int easyaudio_get_streaming_buffer_extra_data_size(easyaudio_buffer* pBuffer);

/// Retrieves a pointer to the extra data of the given streaming buffer.
void* easyaudio_get_streaming_buffer_extra_data(easyaudio_buffer* pBuffer);

/// Begins playing the given streaming buffer.
bool easyaudio_play_streaming_buffer(easyaudio_buffer* pBuffer, bool loop);

/// Determines whether or not the given streaming buffer is looping.
bool easyaudio_is_streaming_buffer_looping(easyaudio_buffer* pBuffer);





///////////////////////////////////////////////////////////////////////////////
//
// Sound World
//
// When a sound is created, whether or not it will be streamed is determined by
// whether or not a pointer to some initial data is specified when creating the
// sound. When initial data is specified, the sound data will be loaded into
// buffer once at creation time. If no data is specified, sound data will be
// loaded dynamically at playback time.
//
///////////////////////////////////////////////////////////////////////////////

typedef struct easyaudio_sound easyaudio_sound;
typedef struct easyaudio_world easyaudio_world;

typedef void    (* easyaudio_on_sound_delete_proc)   (easyaudio_sound* pSound);
typedef ea_bool (* easyaudio_on_sound_read_data_proc)(easyaudio_sound* pSound, void* pDataOut, unsigned int bytesToRead, unsigned int* bytesReadOut);
typedef ea_bool (* easyaudio_on_sound_seek_data_proc)(easyaudio_sound* pSound, unsigned int offsetInBytesFromStart);

/// The structure that is used for creating a sound object.
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

    /// The size in bytes of the data. When this is non-zero, and pInitialData is non-null, the onRead and onSeek streaming
    /// callbacks are not used, and instead the sound's audio data is made up exclusively with this data.
    unsigned int sizeInBytes;

    /// A pointer to the initial data. Can be null, in which case the audio data is streamed with the onRead and onSeek
    /// callbacks below. It is an error for this to be null in addition to onRead and onSeek.
    void* pInitialData;


    /// A pointer to the function to call when the sound is being deleted. This gives the application the opportunity
    /// to delete internal objects that are used for streaming or whatnot.
    easyaudio_on_sound_delete_proc onDelete;

    /// A pointer to the function to call when easy_audio needs to request a chunk of audio data. This is only used when
    /// streaming data.
    easyaudio_on_sound_read_data_proc onRead;

    /// A pointer to the function to call when easy_audio needs to seek the audio data. This is only used when streaming
    /// data.
    easyaudio_on_sound_seek_data_proc onSeek;


    /// The size of the extra data to associate with the sound. Extra data is how an application can link custom data to the
    /// sound object.
    unsigned int extraDataSize;

    /// A pointer to a buffer containing the initial extra data. This buffer is copied when the sound is initially created,
    /// and can be null.
    const void* pExtraData;

} easyaudio_sound_desc;

struct easyaudio_sound
{
    /// A pointer to the world that owns the sound.
    easyaudio_world* pWorld;

    /// A pointer to the audio buffer for playback.
    easyaudio_buffer* pBuffer;


    /// [Internal Use Only] The state of the buffer's playback at the time the associated world overwrote it.
    easyaudio_playback_state prevPlaybackState;

    /// [Internal Use Only] A pointer to the next sound in the local list.
    easyaudio_sound* pNextSound;

    /// [Internal Use Only] A pointer ot the previous sound in the local list.
    easyaudio_sound* pPrevSound;

    /// [Internal Use Only] Keeps track of whether or not a streaming buffer is being used.
    bool isUsingStreamingBuffer;

    /// [Internal Use Only] the onDelete function. Can be null.
    easyaudio_on_sound_delete_proc onDelete;

    /// [Internal Use Only] The onRead streaming function. Can be null, in which case streaming will not be used.
    easyaudio_on_sound_read_data_proc onRead;

    /// [Internal Use Only] The onSeek streaming function. Can be null, in which case streaming will not be used.
    easyaudio_on_sound_seek_data_proc onSeek;
};

struct easyaudio_world
{
    /// A pointer to the easy_audio device to output audio to.
    easyaudio_device* pDevice;

    /// The global playback state of the world.
    easyaudio_playback_state playbackState;

    /// A pointer to the first sound in the local list of sounds.
    easyaudio_sound* pFirstSound;

    /// Mutex for thread-safety.
    easyaudio_mutex lock;
};


/// Creates a new sound world which will output audio from the given device.
easyaudio_world* easyaudio_create_world(easyaudio_device* pDevice);

/// Deletes a sound world that was previously created with easyaudio_create_world().
void easyaudio_delete_world(easyaudio_world* pWorld);


/// Creates a sound in 3D space.
easyaudio_sound* easyaudio_create_sound(easyaudio_world* pWorld, easyaudio_sound_desc desc);

/// Deletes a sound that was previously created with easyaudio_create_sound().
void easyaudio_delete_sound(easyaudio_sound* pSound);

/// Deletes every sound from the given world.
void easyaudio_delete_all_sounds(easyaudio_world* pWorld);


/// Retrieves the size in bytes of the given sound's extra data.
unsigned int easyaudio_get_sound_extra_data_size(easyaudio_sound* pSound);

/// Retrieves a pointer to the buffer containing the given sound's extra data.
void* easyaudio_get_sound_extra_data(easyaudio_sound* pSound);


/// Plays or resumes the given sound.
void easyaudio_play_sound(easyaudio_sound* pSound, bool loop);

/// Pauses playback the given sound.
void easyaudio_pause_sound(easyaudio_sound* pSound);

/// Stops playback of the given sound.
void easyaudio_stop_sound(easyaudio_sound* pSound);

/// Retrieves the playback state of the given sound.
easyaudio_playback_state easyaudio_get_sound_playback_state(easyaudio_sound* pSound);

/// Determines if the given sound is looping.
bool easyaudio_is_sound_looping(easyaudio_sound* pSound);


/// Begins playing a sound using the given streaming callbacks.
void easyaudio_play_inline_sound(easyaudio_world* pWorld, easyaudio_sound_desc desc);

/// Begins playing the given sound at the given position.
void easyaudio_play_inline_sound_3f(easyaudio_world* pWorld, easyaudio_sound_desc desc, float posX, float posY, float posZ);


/// Stops playback of all sounds in the given world.
void easyaudio_stop_all_sounds(easyaudio_world* pWorld);

/// Pauses playback of all sounds in the given world.
void easyaudio_pause_all_sounds(easyaudio_world* pWorld);

/// Resumes playback of all sounds in the given world.
void easyaudio_resume_all_sounds(easyaudio_world* pWorld);


/// Sets the callback for the stop event for the given sound.
void easyaudio_set_sound_stop_callback(easyaudio_sound* pSound, easyaudio_event_callback_proc callback, void* pUserData);

/// Sets the callback for the pause event for the given sound.
void easyaudio_set_sound_pause_callback(easyaudio_sound* pSound, easyaudio_event_callback_proc callback, void* pUserData);

/// Sets the callback for the play event for the given sound.
void easyaudio_set_sound_play_callback(easyaudio_sound* pSound, easyaudio_event_callback_proc callback, void* pUserData);


/// Sets the position of the given sound.
void easyaudio_set_sound_position(easyaudio_sound* pSound, float posX, float posY, float posZ);


/// Sets the 3D mode of the given sound (absolute positioning, relative positioning, no positioning).
void easyaudio_set_sound_3d_mode(easyaudio_sound* pSound, easyaudio_3d_mode mode);

/// Retrieves the 3D mode of the given sound.
easyaudio_3d_mode easyaudio_get_sound_3d_mode(easyaudio_sound* pSound);



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
