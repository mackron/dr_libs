// Audio playback, recording and mixing. Public domain. See "unlicense" statement at the end of this file.
// dr_audio - v0.0 (unversioned) - Release Date TBD
//
// David Reid - mackron@gmail.com

// !!!!! THIS IS WORK IN PROGRESS !!!!!

// This is attempt #2 at creating an easy to use library for audio playback and recording. The first attempt
// had too much reliance on the backend API which made adding new ones too complex and error prone. It was
// also badly designed with respect to how the API was layered.

// DEVELOPMENT NOTES AND BRAINSTORMING
//
// This is just random brainstorming and is likely very out of date and often just outright incorrect.
//
//
// API Hierarchy (from lowest level to highest).
//
// Platform specific
// dra_backend (dra_backend_alsa, dra_backend_dsound) <-- This is the ONLY place with platform-specific code.
// dra_backend_device
//
// Cross platform
// dra_context                                        <-- Has an instantiation of a dra_backend object. Cross-platform.
// dra_device                                         <-- Created and owned by a dra_context object and be an input (recording) or an output (playback) device.
// dra_voice                                          <-- Created and owned by a dra_device object and used by an application to deliver audio data to the backend.
//
//
// In order to make the API easier to use, have simple no-hassle APIs which use appropriate defaults, and then
// have an _ex version for the complex stuff. Example:
//
//   dra_device_open_ex(pContext, deviceType, deviceID, format, sampleRate, channels, latencyInMilliseconds);
//   dra_device_open(pContext, deviceType) ==> dra_device_open_ex(pContext, deviceType, defaultDeviceID, dra_format_pcm_32, 48000, deviceChannelCount, DR_AUDIO_DEFAULT_LATENCY);
//
//
// Buffers are optimal if they're created in the same format as the device. If they're in a different format
// they must go through a conversion process.
//
//
// Latency
//
// When a device is created it'll create a "hardware buffer" which is basically the buffer that the hardware
// device will read from when it needs to play audio. The hardware buffer is divided into two halves. As the
// buffer plays, it moves the playback pointer forward through the buffer and loops. When it hits the half
// way point it notifies the application that it needs more data to continue playing. Once one half starts
// playing the data within it is committed and cannot be changed. The size of each half determines the latency
// of the device.
//
// It sounds tempting to set this to something small like 1ms, but making it too small will
// increase the chance that the CPU won't be able to keep filling it with fresh data. In addition it will
// incrase overall CPU usage because operating system's scheduler will need to wake up the thread more often.
// Increasing the latency will increase memory usage and make playback of new sound sources take longer to
// begin playing. For example, if the latency was set to something like 1 second, a sound effect in a game
// may take up to a whole second to start playing. A balance needs to be made when setting the latency, and
// it can be configured when the device is created.
//
// (mention the fragments system to help avoiding the CPU running out of time to fill new audio data)
//
//
// Mixing
//
// Mixing is done via dra_mixer objects. Buffers can be attached to a mixer, but not more than one at a time.
// By default buffers are attached to a master mixer. Effects like volume can be applied on a per-buffer and
// per-mixer basis.
//
// Mixers can be chained together in a hierarchial manner. Child mixers will be mixed first with the result
// then passed on to higher level mixing. The master mixer is always the top level mixer.
//
// An obvious use case for mixers in games is to have separate volume controls for different categories of
// sounds, such as music, voices and sounds effects. Another example may be in music production where you may
// want to have separate mixers for vocal tracks, percussion tracks, etc.
//
// A mixer can be thought of as a complex buffer - it can be played/stopped/paused and have effects such as
// volume apllied to it. All of this affects all attached buffers and sub-mixers. You can, for example, pause
// every buffer attached to the mixer by simply pausing the mixer. This is an efficient and easy way to pause
// a group of audio buffers at the same time, such as when the user hits the pause button in a game.
//
// Every device includes a master mixer which is the one that buffers are automatically attached to. This one
// is intentionally hidden from the public facing API in order to keep it simpler. For basic audio playback
// using the master mixer will work just fine, however for more complex sound interactions you'll want to use
// your own mixers. Mixers, like buffers, are attached to the master mixer by default
//
//
// Thread Safety
//
// Everything in dr_audio should be thread-safe.
//
// Backends are implemented as if they are always run from a single thread. It's up to the cross-platform
// section to ensure thread-safety. This is an important property because if each backend is responsible for
// their own thread safety it increases the likelyhood of subtle backend-specific bugs.
//
// Every device has their own thread for asynchronous playback. This thread is created when the device is
// created, and deleted when the device is deleted.
//
// (Note edge cases when thread-safety may be an issue)
//
//
//
// More Random Thoughts on Mixing
//
// - Normalize everything to 32-bit float at mix time to simplify everything.
//   - Would then make sense to have devices always be in 32-bit float format, which would further simplify
//     the public API as an added bonus...
//
// - Mixers will need a place to store the floating point samples in an internal buffer. Probably most efficient
//   to place this right next to the buffer that will store the final mix. Can sample rate conversion be done
//   at this time as well?

// USAGE
//
// dr_audio is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_AUDIO_IMPLEMENTATION
//   #include "dr_audio.h"
//
// You can then #include this file in other parts of the program as you would with any other header file.
//
//
// OPTIONS
// #define these options before including this file.
//
// #define DR_AUDIO_NO_DSOUND
//   Disables the DirectSound backend. Note that this is the only backend for the Windows platform.
//
// #define DR_AUDIO_NO_ALSA
//   Disables the ALSA backend. Note that this is the only backend for the Linux platform.


#ifndef dr_audio2_h
#define dr_audio2_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef DR_AUDIO_MAX_CHANNEL_COUNT
#define DR_AUDIO_MAX_CHANNEL_COUNT      16
#endif

#ifndef DR_AUDIO_MAX_EVENT_COUNT
#define DR_AUDIO_MAX_EVENT_COUNT        16
#endif

#define DR_AUDIO_EVENT_ID_STOP  0xFFFFFFFFFFFFFFFFULL
#define DR_AUDIO_EVENT_ID_PLAY  0xFFFFFFFFFFFFFFFEULL

typedef enum
{
    dra_device_type_playback = 0,
    dra_device_type_recording
} dra_device_type;

typedef enum
{
    dra_format_u8 = 0,
    dra_format_s16,
    dra_format_s24,
    dra_format_s32,
    dra_format_f32,
    dra_format_default = dra_format_f32
} dra_format;

// dra_thread_event_type is used internally for thread management.
typedef enum
{
    dra_thread_event_type_none,
    dra_thread_event_type_terminate,
    dra_thread_event_type_play
} dra_thread_event_type;

typedef struct dra_backend dra_backend;
typedef struct dra_backend_device dra_backend_device;

typedef struct dra_context dra_context;
typedef struct dra_device dra_device;
typedef struct dra_mixer dra_mixer;
typedef struct dra_voice dra_voice;

typedef void* dra_thread;
typedef void* dra_mutex;
typedef void* dra_semaphore;

typedef void (* dra_voice_event_proc) (dra_voice* pVoice, uint64_t eventID, void* pUserData);

typedef struct
{
    uint64_t id;
    void* pUserData;
    uint64_t sampleIndex;
    dra_voice_event_proc proc;
    dra_voice* pVoice;
} dra__event;

typedef struct
{
    size_t firstEvent;
    size_t eventCount;
    size_t eventBufferSize;
    dra__event* pEvents;
    dra_mutex lock;
} dra__event_queue;

struct dra_context
{
    dra_backend* pBackend;
};

struct dra_device
{
    // The context that created and owns this device.
    dra_context* pContext;

    // The backend device. This is used to connect the cross-platform front-end with the backend.
    dra_backend_device* pBackendDevice;

    // The main mutex for handling thread-safety.
    dra_mutex mutex;

    // The main thread. For playback devices, this is the thread that waits for fragments to finish processing an then
    // mixes and pushes new audio data to the hardware buffer for playback.
    dra_thread thread;

    // The semaphore used by the main thread to determine whether or not an event is waiting to be processed.
    dra_semaphore threadEventSem;

    // The event type of the most recent event.
    dra_thread_event_type nextThreadEventType;

    // Whether or not the device owns the context. This basically just means whether or not the device was created with a null
    // context and needs to delete the context itself when the device is deleted.
    bool ownsContext;

    // Whether or not the device is being closed. This is used by the thread to determine if it needs to terminate. When
    // dra_device_close() is called, this flag will be set and threadEventSem released. The thread will then see this as it's
    // signal to terminate.
    bool isClosed;

    // Whether or not the device is currently playing. When at least one voice is playing, this will be true. When there
    // are no voices playing, this will be set to false and the background thread will sit dormant until another voice
    // starts playing or the device is closed.
    bool isPlaying;

    // Whether or not the device should stop on the next fragment. This is used for stopping playback of devices that
    // have no voice's playing.
    bool stopOnNextFragment;


    // The master mixer. This is the one and only top-level mixer.
    dra_mixer* pMasterMixer;

    // The number of voices currently being played. This is used to determine whether or not the device should be placed
    // into a dormant state when nothing is being played.
    size_t playingVoicesCount;


    // When a playback event is scheduled it is added to this queue. Events are not posted straight away, but are instead
    // placed in a queue for processing later at specific times to ensure the event is posted AFTER the device has actually
    // played the sample the event is set for.
    dra__event_queue eventQueue;


    // The number of channels being used by the device.
    unsigned int channels;

    // The sample rate in seconds.
    unsigned int sampleRate;
};

struct dra_mixer
{
    // The device that created and owns this mixer.
    dra_device* pDevice;


    // The parent mixer.
    dra_mixer* pParentMixer;

    // The first child mixer.
    dra_mixer* pFirstChildMixer;

    // The last child mixer.
    dra_mixer* pLastChildMixer;

    // The next sibling mixer.
    dra_mixer* pNextSiblingMixer;

    // The previous sibling mixer.
    dra_mixer* pPrevSiblingMixer;


    // The first voice attached to the mixer.
    dra_voice* pFirstVoice;

    // The last voice attached to the mixer.
    dra_voice* pLastVoice;


    // The volume of the buffer as a linear scale. A value of 0.5 means the sound is at half volume. There is no hard
    // limit on the volume, however going beyond 1 may introduce clipping.
    float linearVolume;


    // Mixers output the results of the final mix into a buffer referred to as the staging buffer. A parent mixer will
    // use the staging buffer when it mixes the results of it's submixers. This is an offset of pData.
    float* pStagingBuffer;

    // A pointer to the buffer containing the sample data of the buffer currently being mixed, as floating point values.
    // This is an offset of pData.
    float* pNextSamplesToMix;


    // The sample data for pStagingBuffer and pNextSamplesToMix.
    float pData[1];
};

struct dra_voice
{
    // The device that created and owns this voice.
    dra_device* pDevice;

    // The mixer the voice is attached to. Should never be null. The mixer doesn't "own" the voice - the voice
    // is simply attached to it.
    dra_mixer* pMixer;


    // The next voice in the linked list of voices attached to the mixer.
    dra_voice* pNextVoice;

    // The previous voice in the linked list of voices attached to the mixer.
    dra_voice* pPrevVoice;


    // The format of the audio data contained within this voice.
    dra_format format;

    // The number of channels.
    unsigned int channels;

    // The sample rate in seconds.
    unsigned int sampleRate;


    // The volume of the voice as a linear scale. A value of 0.5 means the sound is at half volume. There is no hard
    // limit on the volume, however going beyond 1 may introduce clipping.
    float linearVolume;



    // Whether or not the voice is currently playing.
    bool isPlaying;

    // Whether or not the voice is currently looping. Whether or not the voice is looping is determined by the last
    // call to dra_voice_play().
    bool isLooping;


    // The total number of frames in the voice.
    size_t frameCount;

    // The current read position, in frames. An important detail with this is that it's based on the sample rate of the
    // device, not the voice.
    size_t currentReadPos;


    // A voice for storing converted frames. This is used by dra_voice__next_frame(). As frames are conversion to
    // floats, that are placed into this voice.
    float convertedFrame[DR_AUDIO_MAX_CHANNEL_COUNT];


    // Data for sample rate conversion. Different SRC algorithms will use different data, which will be stored in their
    // own structure.
    union
    {
        struct
        {
            size_t nearFrameIndex;
            float nearFrame[DR_AUDIO_MAX_CHANNEL_COUNT];
        } nearest;

        struct
        {
            size_t prevFrameIndex;
            float prevFrame[DR_AUDIO_MAX_CHANNEL_COUNT];
            float nextFrame[DR_AUDIO_MAX_CHANNEL_COUNT];
        } linear;
    } src;


    // The number of playback notification events. This does not include the stop and play events.
    size_t playbackEventCount;

    // A pointer to the list of playback events.
    dra__event playbackEvents[DR_AUDIO_MAX_EVENT_COUNT];

    // The event to call when the voice has stopped playing, either naturally or explicitly with dra_voice_stop().
    dra__event stopEvent;

    // The event to call when the voice starts playing.
    dra__event playEvent;


    // The size of the buffer in bytes.
    size_t sizeInBytes;

    // The actual buffer containing the raw audio data in it's native format. At mix time the data will be converted
    // to floats.
    uint8_t pData[1];
};


// dra_context_create()
dra_context* dra_context_create();
void dra_context_delete(dra_context* pContext);

// dra_device_open_ex()
//
// If deviceID is 0 the default device for the given type is used.
// format can be dra_format_default which is dra_format_s32.
// If channels is set to 0, defaults 2 channels (stereo).
// If sampleRate is set to 0, defaults to 48000.
// If latency is 0, defaults to 50 milliseconds. See notes about latency above.
dra_device* dra_device_open_ex(dra_context* pContext, dra_device_type type, unsigned int deviceID, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds);
dra_device* dra_device_open(dra_context* pContext, dra_device_type type);
void dra_device_close(dra_device* pDevice);


// dra_mixer_create()
dra_mixer* dra_mixer_create(dra_device* pDevice);

// dra_mixer_delete()
//
// Deleting a mixer will detach any attached voices and sub-mixers and attach them to the master mixer. It is
// up to the application to manage the allocation of sub-mixers and voices. Typically you'll want to delete
// child mixers and voices before deleting a mixer.
void dra_mixer_delete(dra_mixer* pMixer);

// dra_mixer_attach_submixer()
void dra_mixer_attach_submixer(dra_mixer* pMixer, dra_mixer* pSubmixer);

// dra_mixer_detach_submixer()
void dra_mixer_detach_submixer(dra_mixer* pMixer, dra_mixer* pSubmixer);

// dra_mixer_detach_all_submixers()
void dra_mixer_detach_all_submixers(dra_mixer* pMixer);

// dra_mixer_attach_voice()
void dra_mixer_attach_voice(dra_mixer* pMixer, dra_voice* pVoice);

// dra_mixer_detach_voice()
void dra_mixer_detach_voice(dra_mixer* pMixer, dra_voice* pVoice);

// dra_mixer_detach_all_voices()
void dra_mixer_detach_all_voices(dra_mixer* pMixer);

// dra_voice_set_volume()
void dra_mixer_set_volume(dra_mixer* pMixer, float linearVolume);

// dra_voice_get_volume()
float dra_mixer_get_volume(dra_mixer* pMixer);

// Mixes the next number of frameCount and moves the playback position appropriately.
//
// pMixer     [in] The mixer.
// frameCount [in] The number of frames to mix.
//
// Returns the number of frames actually mixed.
//
// The return value is used to determine whether or not there's anything left to mix in the future. When there are
// no samples left to mix, the device can be put into a dormant state to prevent unnecessary processing.
//
// Mixed samples will be placed in pMixer->pStagingBuffer.
size_t dra_mixer_mix_next_frames(dra_mixer* pMixer, size_t frameCount);


// dra_voice_create()
dra_voice* dra_voice_create(dra_device* pDevice, dra_format format, unsigned int channels, unsigned int sampleRate, size_t sizeInBytes, const void* pInitialData);
dra_voice* dra_voice_create_compatible(dra_device* pDevice, size_t sizeInBytes, const void* pInitialData);

// dra_voice_delete()
void dra_voice_delete(dra_voice* pVoice);

// dra_voice_play()
//
// If the mixer the voice is attached to is not playing, the voice will be marked as playing, but won't actually play anything until
// the mixer is started again.
void dra_voice_play(dra_voice* pVoice, bool loop);

// dra_voice_stop()
void dra_voice_stop(dra_voice* pVoice);

// dra_voice_is_playing()
bool dra_voice_is_playing(dra_voice* pVoice);

// dra_voice_is_looping()
bool dra_voice_is_looping(dra_voice* pVoice);


// dra_voice_set_volum()
void dra_voice_set_volume(dra_voice* pVoice, float linearVolume);

// dra_voice_get_volume()
float dra_voice_get_volume(dra_voice* pVoice);


void dra_voice_set_on_stop(dra_voice* pVoice, dra_voice_event_proc proc, void* pUserData);
void dra_voice_set_on_play(dra_voice* pVoice, dra_voice_event_proc proc, void* pUserData);
bool dra_voice_add_playback_event(dra_voice* pVoice, uint64_t sampleIndex, uint64_t eventID, dra_voice_event_proc proc, void* pUserData);
void dra_voice_remove_playback_event(dra_voice* pVoice, uint64_t eventID);


// Utility APIs

// Retrieves the number of bits per sample based on the given format.
unsigned int dra_get_bits_per_sample_by_format(dra_format format);

// Determines whether or not the given format is floating point.
bool dra_is_format_float(dra_format format);

// Retrieves the number of bytes per sample based on the given format.
unsigned int dra_get_bytes_per_sample_by_format(dra_format format);



#ifdef __cplusplus
}
#endif
#endif  //dr_audio2_h



///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DR_AUDIO_IMPLEMENTATION
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>  // For good old printf debugging. Delete later.

#ifdef _MSC_VER
#define DR_AUDIO_INLINE static __forceinline
#else
#define DR_AUDIO_INLINE static inline
#endif

#define DR_AUDIO_DEFAULT_CHANNEL_COUNT  2
#define DR_AUDIO_DEFAULT_SAMPLE_RATE    48000
#define DR_AUDIO_DEFAULT_LATENCY        100     // Milliseconds. TODO: Test this with very low values. DirectSound appears to not signal the fragment events when it's too small. With values of about 20 it sounds crackly.
#define DR_AUDIO_DEFAULT_FRAGMENT_COUNT 2       // The hardware buffer is divided up into latency-sized blocks. This controls that number. Must be at least 2.

#define DR_AUDIO_BACKEND_TYPE_NULL      0
#define DR_AUDIO_BACKEND_TYPE_DSOUND    1
#define DR_AUDIO_BACKEND_TYPE_ALSA      2


// Thanks to good old Bit Twiddling Hacks for this one: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
DR_AUDIO_INLINE unsigned int dra_next_power_of_2(unsigned int x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;

    return x;
}

DR_AUDIO_INLINE unsigned int dra_prev_power_of_2(unsigned int x)
{
    return dra_next_power_of_2(x) >> 1;
}


DR_AUDIO_INLINE float dra_mixf(float x, float y, float a)
{
    return x*(1-a) + y*a;
}


///////////////////////////////////////////////////////////////////////////////
//
// Platform Specific
//
///////////////////////////////////////////////////////////////////////////////

// Every backend struct should begin with these properties.
struct dra_backend
{
#define DR_AUDIO_BASE_BACKEND_ATTRIBS \
    unsigned int type; \

    DR_AUDIO_BASE_BACKEND_ATTRIBS
};

// Every backend device struct should begin with these properties.
struct dra_backend_device
{
#define DR_AUDIO_BASE_BACKEND_DEVICE_ATTRIBS \
    dra_backend* pBackend; \
    dra_device_type type; \
    unsigned int channels; \
    unsigned int sampleRate; \
    unsigned int fragmentCount; \
    unsigned int samplesPerFragment; \

    DR_AUDIO_BASE_BACKEND_DEVICE_ATTRIBS
};




// Compile-time platform detection and #includes.
#ifdef _WIN32
#include <windows.h>

//// Threading (Win32) ////
typedef DWORD (* dra_thread_entry_proc)(LPVOID pData);

dra_thread dra_thread_create(dra_thread_entry_proc entryProc, void* pData)
{
    return (dra_thread)CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)entryProc, pData, 0, NULL);
}

void dra_thread_delete(dra_thread thread)
{
    CloseHandle((HANDLE)thread);
}

void dra_thread_wait(dra_thread thread)
{
    WaitForSingleObject((HANDLE)thread, INFINITE);
}


dra_mutex dra_mutex_create()
{
    return (dra_mutex)CreateEventA(NULL, FALSE, TRUE, NULL);
}

void dra_mutex_delete(dra_mutex mutex)
{
    CloseHandle((HANDLE)mutex);
}

void dra_mutex_lock(dra_mutex mutex)
{
    WaitForSingleObject((HANDLE)mutex, INFINITE);
}

void dra_mutex_unlock(dra_mutex mutex)
{
    SetEvent((HANDLE)mutex);
}


dra_semaphore dra_semaphore_create(int initialValue)
{
    return (void*)CreateSemaphoreA(NULL, initialValue, LONG_MAX, NULL);
}

void dra_semaphore_delete(dra_semaphore semaphore)
{
    CloseHandle(semaphore);
}

bool dra_semaphore_wait(dra_semaphore semaphore)
{
    return WaitForSingleObject((HANDLE)semaphore, INFINITE) == WAIT_OBJECT_0;
}

bool dra_semaphore_release(dra_semaphore semaphore)
{
    return ReleaseSemaphore((HANDLE)semaphore, 1, NULL) != 0;
}



//// DirectSound ////
#ifndef DR_AUDIO_NO_DSOUND
#define DR_AUDIO_ENABLE_DSOUND
#include <dsound.h>
#include <mmreg.h>  // WAVEFORMATEX

GUID DR_AUDIO_GUID_NULL = {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

//static GUID _g_DSListenerGUID                       = {0x279AFA84, 0x4981, 0x11CE, {0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60}};
//static GUID _g_DirectSoundBuffer8GUID               = {0x6825a449, 0x7524, 0x4d82, {0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e}};
//static GUID _g_DirectSound3DBuffer8GUID             = {0x279AFA86, 0x4981, 0x11CE, {0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60}};
static GUID _g_DirectSoundNotifyGUID                = {0xb0210783, 0x89cd, 0x11d0, {0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16}};
//static GUID _g_KSDATAFORMAT_SUBTYPE_PCM_GUID        = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static GUID _g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

#ifdef __cplusplus
//static GUID g_DSListenerGUID                        = _g_DSListenerGUID;
//static GUID g_DirectSoundBuffer8GUID                = _g_DirectSoundBuffer8GUID;
//static GUID g_DirectSound3DBuffer8GUID              = _g_DirectSound3DBuffer8GUID;
static GUID g_DirectSoundNotifyGUID                 = _g_DirectSoundNotifyGUID;
//static GUID g_KSDATAFORMAT_SUBTYPE_PCM_GUID         = _g_KSDATAFORMAT_SUBTYPE_PCM_GUID;
static GUID g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID  = _g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID;
#else
//static GUID* g_DSListenerGUID                       = &_g_DSListenerGUID;
//static GUID* g_DirectSoundBuffer8GUID               = &_g_DirectSoundBuffer8GUID;
//static GUID* g_DirectSound3DBuffer8GUID             = &_g_DirectSound3DBuffer8GUID;
static GUID* g_DirectSoundNotifyGUID                = &_g_DirectSoundNotifyGUID;
//static GUID* g_KSDATAFORMAT_SUBTYPE_PCM_GUID        = &_g_KSDATAFORMAT_SUBTYPE_PCM_GUID;
static GUID* g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID = &_g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID;
#endif

typedef HRESULT (WINAPI * pDirectSoundCreate8Proc)(LPCGUID pcGuidDevice, LPDIRECTSOUND8 *ppDS8, LPUNKNOWN pUnkOuter);
typedef HRESULT (WINAPI * pDirectSoundEnumerateAProc)(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID pContext);
typedef HRESULT (WINAPI * pDirectSoundCaptureCreate8Proc)(LPCGUID pcGuidDevice, LPDIRECTSOUNDCAPTURE8 *ppDSC8, LPUNKNOWN pUnkOuter);
typedef HRESULT (WINAPI * pDirectSoundCaptureEnumerateAProc)(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID pContext);

typedef struct
{
    DR_AUDIO_BASE_BACKEND_ATTRIBS

    // A handle to the dsound DLL for doing run-time linking.
    HMODULE hDSoundDLL;

    pDirectSoundCreate8Proc pDirectSoundCreate8;
    pDirectSoundEnumerateAProc pDirectSoundEnumerateA;
} dra_backend_dsound;

typedef struct
{
    DR_AUDIO_BASE_BACKEND_DEVICE_ATTRIBS

    // The main device object for use with DirectSound.
    LPDIRECTSOUND8 pDS;

    // The DirectSound "primary buffer". It's basically just representing the connection between us and the hardware device.
    LPDIRECTSOUNDBUFFER pDSPrimaryBuffer;

    // The DirectSound "secondary buffer". This is where the actual audio data will be written to by dr_audio when it's time
    // for play back some audio through the speakers. This represents the hardware buffer.
    LPDIRECTSOUNDBUFFER pDSSecondaryBuffer;

    // The notification object used by DirectSound to notify dr_audio that it's ready for the next fragment of audio data.
    LPDIRECTSOUNDNOTIFY pDSNotify;

    // Notification events for each fragment.
    HANDLE pNotifyEvents[DR_AUDIO_DEFAULT_FRAGMENT_COUNT];

    // The event the main playback thread will wait on to determine whether or not the playback loop should terminate.
    HANDLE hStopEvent;

    // The index of the fragment that is currently being played.
    unsigned int currentFragmentIndex;

    // The address of the mapped fragment. This is set with IDirectSoundBuffer8::Lock() and passed to IDriectSoundBuffer8::Unlock().
    void* pLockPtr;

    // The size of the locked buffer. This is set with IDirectSoundBuffer8::Lock() and passed to IDriectSoundBuffer8::Unlock().
    DWORD lockSize;

} dra_backend_device_dsound;

typedef struct
{
    unsigned int deviceID;
    unsigned int counter;
    const GUID* pGuid;
} dra_dsound__device_enum_data;

static BOOL CALLBACK dra_dsound__get_device_guid_by_id__callback(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext)
{
    (void)lpcstrDescription;
    (void)lpcstrModule;

    dra_dsound__device_enum_data* pData = (dra_dsound__device_enum_data*)lpContext;
    assert(pData != NULL);

    if (pData->counter == pData->deviceID) {
        pData->pGuid = lpGuid;
        return false;
    }

    pData->counter += 1;
    return true;
}

const GUID* dra_dsound__get_device_guid_by_id(dra_backend* pBackend, unsigned int deviceID)
{
    // From MSDN:
    //
    // The first device enumerated is always called the Primary Sound Driver, and the lpGUID parameter of the callback is
    // NULL. This device represents the preferred output device set by the user in Control Panel.
    if (deviceID == 0) {
        return NULL;
    }

    dra_backend_dsound* pBackendDS = (dra_backend_dsound*)pBackend;
    if (pBackendDS == NULL) {
        return NULL;
    }

    // The device ID is treated as the device index. The actual ID for use by DirectSound is a GUID. We use DirectSoundEnumerateA()
    // iterate over each device. This function is usually only going to be used during initialization time so it won't be a performance
    // issue to not cache these.
    dra_dsound__device_enum_data data = {0};
    data.deviceID = deviceID;
    pBackendDS->pDirectSoundEnumerateA(dra_dsound__get_device_guid_by_id__callback, &data);

    return data.pGuid;
}

dra_backend* dra_backend_create_dsound()
{
    dra_backend_dsound* pBackendDS = (dra_backend_dsound*)calloc(1, sizeof(*pBackendDS));   // <-- Note the calloc() - makes it easier to handle the on_error goto.
    if (pBackendDS == NULL) {
        return NULL;
    }

    pBackendDS->type = DR_AUDIO_BACKEND_TYPE_DSOUND;

    pBackendDS->hDSoundDLL = LoadLibraryW(L"dsound.dll");
    if (pBackendDS->hDSoundDLL == NULL) {
        goto on_error;
    }

    pBackendDS->pDirectSoundCreate8 = (pDirectSoundCreate8Proc)GetProcAddress(pBackendDS->hDSoundDLL, "DirectSoundCreate8");
    if (pBackendDS->pDirectSoundCreate8 == NULL){
        goto on_error;
    }

    pBackendDS->pDirectSoundEnumerateA = (pDirectSoundEnumerateAProc)GetProcAddress(pBackendDS->hDSoundDLL, "DirectSoundEnumerateA");
    if (pBackendDS->pDirectSoundEnumerateA == NULL){
        goto on_error;
    }


    return (dra_backend*)pBackendDS;

on_error:
    if (pBackendDS != NULL) {
        if (pBackendDS->hDSoundDLL != NULL) {
            FreeLibrary(pBackendDS->hDSoundDLL);
        }

        free(pBackendDS);
    }

    return NULL;
}

void dra_backend_delete_dsound(dra_backend* pBackend)
{
    dra_backend_dsound* pBackendDS = (dra_backend_dsound*)pBackend;
    if (pBackendDS == NULL) {
        return;
    }

    if (pBackendDS->hDSoundDLL != NULL) {
        FreeLibrary(pBackendDS->hDSoundDLL);
    }

    free(pBackendDS);
}

dra_backend_device* dra_backend_device_open_playback_dsound(dra_backend* pBackend, unsigned int deviceID, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    dra_backend_dsound* pBackendDS = (dra_backend_dsound*)pBackend;
    if (pBackendDS == NULL) {
        return NULL;
    }

    dra_backend_device_dsound* pDeviceDS = (dra_backend_device_dsound*)calloc(1, sizeof(*pDeviceDS));
    if (pDeviceDS == NULL) {
        goto on_error;
    }

    pDeviceDS->pBackend = pBackend;
    pDeviceDS->type = dra_device_type_playback;
    pDeviceDS->channels = channels;
    pDeviceDS->sampleRate = sampleRate;

    HRESULT hr = pBackendDS->pDirectSoundCreate8(dra_dsound__get_device_guid_by_id(pBackend, deviceID), &pDeviceDS->pDS, NULL);
    if (FAILED(hr)) {
        goto on_error;
    }

    // The cooperative level must be set before doing anything else.
    hr = IDirectSound_SetCooperativeLevel(pDeviceDS->pDS, GetForegroundWindow(), DSSCL_PRIORITY);
    if (FAILED(hr)) {
        goto on_error;
    }


    // The primary buffer is basically just the connection to the hardware.
    DSBUFFERDESC descDSPrimary;
    memset(&descDSPrimary, 0, sizeof(DSBUFFERDESC));
    descDSPrimary.dwSize  = sizeof(DSBUFFERDESC);
    descDSPrimary.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;

    hr = IDirectSound_CreateSoundBuffer(pDeviceDS->pDS, &descDSPrimary, &pDeviceDS->pDSPrimaryBuffer, NULL);
    if (FAILED(hr)) {
        goto on_error;
    }


    // If the channel count is 0 then we need to use the default. From MSDN:
    //
    // The method succeeds even if the hardware does not support the requested format; DirectSound sets the buffer to the closest
    // supported format. To determine whether this has happened, an application can call the GetFormat method for the primary buffer
    // and compare the result with the format that was requested with the SetFormat method.
    if (channels == 0) {
        channels = DR_AUDIO_DEFAULT_CHANNEL_COUNT;
    }

    WAVEFORMATEXTENSIBLE wf = {0};
    wf.Format.cbSize               = sizeof(wf);
    wf.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels            = (WORD)channels;
    wf.Format.nSamplesPerSec       = (DWORD)sampleRate;
    wf.Format.wBitsPerSample       = sizeof(float)*8;
    wf.Format.nBlockAlign          = (wf.Format.nChannels * wf.Format.wBitsPerSample) / 8;
    wf.Format.nAvgBytesPerSec      = wf.Format.nBlockAlign * wf.Format.nSamplesPerSec;
    wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
    wf.dwChannelMask               = 0;
    wf.SubFormat                   = _g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID;
    if (channels > 2) {
        wf.dwChannelMask = ~(((DWORD)-1) << channels);
    }

    hr = IDirectSoundBuffer_SetFormat(pDeviceDS->pDSPrimaryBuffer, (WAVEFORMATEX*)&wf);
    if (FAILED(hr)) {
        goto on_error;
    }


    // Get the ACTUAL properties of the buffer. This is silly API design...
    DWORD requiredSize;
    hr = IDirectSoundBuffer_GetFormat(pDeviceDS->pDSPrimaryBuffer, NULL, 0, &requiredSize);
    if (FAILED(hr)) {
        goto on_error;
    }

    char rawdata[1024];
    WAVEFORMATEXTENSIBLE* actualFormat = (WAVEFORMATEXTENSIBLE*)rawdata;
    hr = IDirectSoundBuffer_GetFormat(pDeviceDS->pDSPrimaryBuffer, (WAVEFORMATEX*)actualFormat, requiredSize, NULL);
    if (FAILED(hr)) {
        goto on_error;
    }

    pDeviceDS->channels = actualFormat->Format.nChannels;
    pDeviceDS->sampleRate = actualFormat->Format.nSamplesPerSec;

    // DirectSound always has the same number of fragments.
    pDeviceDS->fragmentCount = DR_AUDIO_DEFAULT_FRAGMENT_COUNT;


    // The secondary buffer is the buffer where the real audio data will be written to and used by the hardware device. It's
    // size is based on the latency, sample rate and channels.
    //
    // The format of the secondary buffer should exactly match the primary buffer as to avoid unnecessary data conversions.
    size_t sampleRateInMilliseconds = pDeviceDS->sampleRate / 1000;
    if (sampleRateInMilliseconds == 0) {
        sampleRateInMilliseconds = 1;
    }

    // The size of a fragment is sized such that the number of frames contained within it is a multiple of 2. The reason for
    // this is to keep it consistent with the ALSA backend.
    unsigned int proposedFramesPerFragment = sampleRateInMilliseconds * latencyInMilliseconds;
    unsigned int framesPerFragment = dra_prev_power_of_2(proposedFramesPerFragment);
    if (framesPerFragment == 0) {
        framesPerFragment = 2;
    }

    pDeviceDS->samplesPerFragment = framesPerFragment * pDeviceDS->channels;

    size_t fragmentSize = pDeviceDS->samplesPerFragment * sizeof(float);
    size_t hardwareBufferSize = fragmentSize * pDeviceDS->fragmentCount;
    assert(hardwareBufferSize > 0);     // <-- If you've triggered this is means you've got something set to 0. You haven't been setting that latency to 0 have you?! That's not allowed!

    // Meaning of dwFlags (from MSDN):
    //
    // DSBCAPS_CTRLPOSITIONNOTIFY
    //   The buffer has position notification capability.
    //
    // DSBCAPS_GLOBALFOCUS
    //   With this flag set, an application using DirectSound can continue to play its buffers if the user switches focus to
    //   another application, even if the new application uses DirectSound.
    //
    // DSBCAPS_GETCURRENTPOSITION2
    //   In the first version of DirectSound, the play cursor was significantly ahead of the actual playing sound on emulated
    //   sound cards; it was directly behind the write cursor. Now, if the DSBCAPS_GETCURRENTPOSITION2 flag is specified, the
    //   application can get a more accurate play cursor.
    DSBUFFERDESC descDS;
    memset(&descDS, 0, sizeof(DSBUFFERDESC));
    descDS.dwSize = sizeof(DSBUFFERDESC);
    descDS.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
    descDS.dwBufferBytes = (DWORD)hardwareBufferSize;
    descDS.lpwfxFormat = (WAVEFORMATEX*)&wf;
    hr = IDirectSound_CreateSoundBuffer(pDeviceDS->pDS, &descDS, &pDeviceDS->pDSSecondaryBuffer, NULL);
    if (FAILED(hr)) {
        goto on_error;
    }


    // As DirectSound is playing back the hardware buffer it needs to notify dr_audio when it's ready for new data. This is done
    // through a notification object which we retrieve from the secondary buffer.
    hr = IDirectSoundBuffer8_QueryInterface(pDeviceDS->pDSSecondaryBuffer, g_DirectSoundNotifyGUID, (void**)&pDeviceDS->pDSNotify);
    if (FAILED(hr)) {
        goto on_error;
    }

    DSBPOSITIONNOTIFY notifyPoints[DR_AUDIO_DEFAULT_FRAGMENT_COUNT];  // One notification event for each fragment.
    for (int i = 0; i < DR_AUDIO_DEFAULT_FRAGMENT_COUNT; ++i)
    {
        pDeviceDS->pNotifyEvents[i] = CreateEventA(NULL, FALSE, FALSE, NULL);
        if (pDeviceDS->pNotifyEvents[i] == NULL) {
            goto on_error;
        }

        notifyPoints[i].dwOffset = i * fragmentSize;    // <-- This is in bytes.
        notifyPoints[i].hEventNotify = pDeviceDS->pNotifyEvents[i];
    }

    hr = IDirectSoundNotify_SetNotificationPositions(pDeviceDS->pDSNotify, DR_AUDIO_DEFAULT_FRAGMENT_COUNT, notifyPoints);
    if (FAILED(hr)) {
        goto on_error;
    }



    // The termination event is used to determine when the playback thread should be terminated. The playback thread
    // will wait on this event in addition to the notification events in it's main loop.
    pDeviceDS->hStopEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (pDeviceDS->hStopEvent == NULL) {
        goto on_error;
    }

    return (dra_backend_device*)pDeviceDS;

on_error:
    if (pDeviceDS != NULL) {
        if (pDeviceDS->pDSNotify != NULL) {
            IDirectSoundNotify_Release(pDeviceDS->pDSNotify);
        }

        if (pDeviceDS->pDSSecondaryBuffer != NULL) {
            IDirectSoundBuffer_Release(pDeviceDS->pDSSecondaryBuffer);
        }

        if (pDeviceDS->pDSPrimaryBuffer != NULL) {
            IDirectSoundBuffer_Release(pDeviceDS->pDSPrimaryBuffer);
        }

        if (pDeviceDS->pDS != NULL) {
            IDirectSound_Release(pDeviceDS->pDS);
        }


        for (int i = 0; i < DR_AUDIO_DEFAULT_FRAGMENT_COUNT; ++i) {
            CloseHandle(pDeviceDS->pNotifyEvents[i]);
        }

        if (pDeviceDS->hStopEvent != NULL) {
            CloseHandle(pDeviceDS->hStopEvent);
        }

        free(pDeviceDS);
    }

    return NULL;
}

dra_backend_device* dra_backend_device_open_recording_dsound(dra_backend* pBackend, unsigned int deviceID, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    // Not yet implemented.
    (void)pBackend;
    (void)deviceID;
    (void)channels;
    (void)sampleRate;
    (void)latencyInMilliseconds;
    return NULL;
}

dra_backend_device* dra_backend_device_open_dsound(dra_backend* pBackend, dra_device_type type, unsigned int deviceID, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    if (type == dra_device_type_playback) {
        return dra_backend_device_open_playback_dsound(pBackend, deviceID, channels, sampleRate, latencyInMilliseconds);
    } else {
        return dra_backend_device_open_recording_dsound(pBackend, deviceID, channels, sampleRate, latencyInMilliseconds);
    }
}

void dra_backend_device_close_dsound(dra_backend_device* pDevice)
{
    dra_backend_device_dsound* pDeviceDS = (dra_backend_device_dsound*)pDevice;
    if (pDeviceDS == NULL) {
        return;
    }

    if (pDeviceDS->pDSNotify != NULL) {
        IDirectSoundNotify_Release(pDeviceDS->pDSNotify);
    }

    if (pDeviceDS->pDSSecondaryBuffer != NULL) {
        IDirectSoundBuffer_Release(pDeviceDS->pDSSecondaryBuffer);
    }

    if (pDeviceDS->pDSPrimaryBuffer != NULL) {
        IDirectSoundBuffer_Release(pDeviceDS->pDSPrimaryBuffer);
    }

    if (pDeviceDS->pDS != NULL) {
        IDirectSound_Release(pDeviceDS->pDS);
    }


    for (int i = 0; i < DR_AUDIO_DEFAULT_FRAGMENT_COUNT; ++i) {
        CloseHandle(pDeviceDS->pNotifyEvents[i]);
    }

    if (pDeviceDS->hStopEvent != NULL) {
        CloseHandle(pDeviceDS->hStopEvent);
    }

    free(pDeviceDS);
}

void dra_backend_device_play(dra_backend_device* pDevice)
{
    dra_backend_device_dsound* pDeviceDS = (dra_backend_device_dsound*)pDevice;
    if (pDeviceDS == NULL) {
        return;
    }

    IDirectSoundBuffer_Play(pDeviceDS->pDSSecondaryBuffer, 0, 0, DSBPLAY_LOOPING);
}

void dra_backend_device_stop(dra_backend_device* pDevice)
{
    dra_backend_device_dsound* pDeviceDS = (dra_backend_device_dsound*)pDevice;
    if (pDeviceDS == NULL) {
        return;
    }

    // Don't do anything if the buffer is not already playing.
    DWORD status;
    IDirectSoundBuffer_GetStatus(pDeviceDS->pDSSecondaryBuffer, &status);
    if ((status & DSBSTATUS_PLAYING) == 0) {
        return; // The buffer is already stopped.
    }

    // Stop the playback straight away to ensure output to the hardware device is stopped as soon as possible.
    IDirectSoundBuffer_Stop(pDeviceDS->pDSSecondaryBuffer);
    IDirectSoundBuffer_SetCurrentPosition(pDeviceDS->pDSSecondaryBuffer, 0);

    // Now we just need to make dra_backend_device_play() return which in the case of DirectSound we do by
    // simply signaling the stop event.
    SetEvent(pDeviceDS->hStopEvent);
}

bool dra_backend_device_wait(dra_backend_device* pDevice)   // <-- Returns true if the function has returned because it needs more data; false if the device has been stopped or an error has occured.
{
    dra_backend_device_dsound* pDeviceDS = (dra_backend_device_dsound*)pDevice;
    if (pDeviceDS == NULL) {
        return false;
    }

    unsigned int eventCount = DR_AUDIO_DEFAULT_FRAGMENT_COUNT + 1;
    HANDLE eventHandles[DR_AUDIO_DEFAULT_FRAGMENT_COUNT + 1];   // +1 for the stop event.
    memcpy(eventHandles, pDeviceDS->pNotifyEvents, sizeof(HANDLE) * DR_AUDIO_DEFAULT_FRAGMENT_COUNT);
    eventHandles[DR_AUDIO_DEFAULT_FRAGMENT_COUNT] = pDeviceDS->hStopEvent;

    DWORD rc = WaitForMultipleObjects(DR_AUDIO_DEFAULT_FRAGMENT_COUNT + 1, eventHandles, FALSE, INFINITE);
    if (rc >= WAIT_OBJECT_0 && rc < eventCount)
    {
        unsigned int eventIndex = rc - WAIT_OBJECT_0;
        HANDLE hEvent = eventHandles[eventIndex];

        // Has the device been stopped? If so, need to return false.
        if (hEvent == pDeviceDS->hStopEvent) {
            return false;
        }

        // If we get here it means the event that's been signaled represents a fragment.
        pDeviceDS->currentFragmentIndex = eventIndex;
        return true;
    }

    return false;
}

void* dra_backend_device_map_next_fragment(dra_backend_device* pDevice, size_t* pSamplesInFragmentOut)
{
    assert(pSamplesInFragmentOut != NULL);

    dra_backend_device_dsound* pDeviceDS = (dra_backend_device_dsound*)pDevice;
    if (pDeviceDS == NULL) {
        return NULL;
    }

    if (pDeviceDS->pLockPtr != NULL) {
        return NULL;    // A fragment is already mapped. Can only have a single fragment mapped at a time.
    }


    // If the device is not currently playing, we just return the first fragment. Otherwise we return the fragment that's sitting just past the
    // one that's currently playing.
    DWORD dwOffset = 0;
    DWORD dwBytes = pDeviceDS->samplesPerFragment * sizeof(float);

    DWORD status;
    IDirectSoundBuffer_GetStatus(pDeviceDS->pDSSecondaryBuffer, &status);
    if ((status & DSBSTATUS_PLAYING) != 0) {
        dwOffset = (((pDeviceDS->currentFragmentIndex + 1) % pDeviceDS->fragmentCount) * pDeviceDS->samplesPerFragment) * sizeof(float);
    }


    HRESULT hr = IDirectSoundBuffer_Lock(pDeviceDS->pDSSecondaryBuffer, dwOffset, dwBytes, &pDeviceDS->pLockPtr, &pDeviceDS->lockSize, NULL, NULL, 0);
    if (FAILED(hr)) {
        return NULL;
    }

    *pSamplesInFragmentOut = pDeviceDS->samplesPerFragment;
    return pDeviceDS->pLockPtr;
}

void dra_backend_device_unmap_next_fragment(dra_backend_device* pDevice)
{
    dra_backend_device_dsound* pDeviceDS = (dra_backend_device_dsound*)pDevice;
    if (pDeviceDS == NULL) {
        return;
    }

    if (pDeviceDS->pLockPtr == NULL) {
        return;     // Nothing is mapped.
    }

    IDirectSoundBuffer_Unlock(pDeviceDS->pDSSecondaryBuffer, pDeviceDS->pLockPtr, pDeviceDS->lockSize, NULL, 0);

    pDeviceDS->pLockPtr = NULL;
    pDeviceDS->lockSize = 0;
}
#endif  // DR_AUDIO_NO_SOUND
#endif  // _WIN32

#ifdef __linux__
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>

//// Threading (POSIX) ////
typedef void* (* dra_thread_entry_proc)(void* pData);

dra_thread dra_thread_create(dra_thread_entry_proc entryProc, void* pData)
{
    pthread_t thread;
    if (pthread_create(&thread, NULL, entryProc, pData) != 0) {
        return NULL;
    }

    return (dra_thread)thread;
}

void dra_thread_delete(dra_thread thread)
{
    (void)thread;
}

void dra_thread_wait(dra_thread thread)
{
    pthread_join((pthread_t)thread, NULL);
}


dra_mutex dra_mutex_create()
{
    // The pthread_mutex_t object is not a void* castable handle type. Just create it on the heap and be done with it.
    pthread_mutex_t* mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(mutex, NULL) != 0) {
        free(mutex);
        mutex = NULL;
    }

    return mutex;
}

void dra_mutex_delete(dra_mutex mutex)
{
    pthread_mutex_destroy((pthread_mutex_t*)mutex);
}

void dra_mutex_lock(dra_mutex mutex)
{
    pthread_mutex_lock((pthread_mutex_t*)mutex);
}

void dra_mutex_unlock(dra_mutex mutex)
{
    pthread_mutex_unlock((pthread_mutex_t*)mutex);
}


dra_semaphore dra_semaphore_create(int initialValue)
{
    sem_t* semaphore = (sem_t*)malloc(sizeof(sem_t));
    if (sem_init(semaphore, 0, (unsigned int)initialValue) == -1) {
        free(semaphore);
        semaphore = NULL;
    }

    return (dra_semaphore)semaphore;
}

void dra_semaphore_delete(dra_semaphore semaphore)
{
    sem_close((sem_t*)semaphore);
}

bool dra_semaphore_wait(dra_semaphore semaphore)
{
    return sem_wait(semaphore) != -1;
}

bool dra_semaphore_release(dra_semaphore semaphore)
{
    return sem_post(semaphore) != -1;
}


//// ALSA ////

#ifndef DR_AUDIO_NO_ALSA
#define DR_AUDIO_ENABLE_ALSA
#include <alsa/asoundlib.h>

typedef struct
{
    DR_AUDIO_BASE_BACKEND_ATTRIBS

    int unused;
} dra_backend_alsa;

typedef struct
{
    DR_AUDIO_BASE_BACKEND_DEVICE_ATTRIBS

    // The ALSA device handle.
    snd_pcm_t* deviceALSA;

    // Whether or not the device is currently playing.
    bool isPlaying;

    // Whether or not the intermediary buffer is mapped.
    bool isBufferMapped;

    // The intermediary buffer where audio data is written before being submitted to the device.
    float* pIntermediaryBuffer;
} dra_backend_device_alsa;


static bool dra_alsa__get_device_name_by_id(dra_backend* pBackend, unsigned int deviceID, char* deviceNameOut)
{
    assert(pBackend != NULL);
    assert(deviceNameOut != NULL);

    deviceNameOut[0] = '\0';    // Safety.

    if (deviceID == 0) {
        strcpy(deviceNameOut, "default");
        return true;
    }


    unsigned int iDevice = 0;

    char** deviceHints;
    if (snd_device_name_hint(-1, "pcm", (void***)&deviceHints) < 0) {
        printf("Failed to iterate devices.");
        return -1;
    }

    char** nextDeviceHint = deviceHints;
    while (*nextDeviceHint != NULL && iDevice < deviceID) {
        nextDeviceHint += 1;
        iDevice += 1;
    }

    bool result = false;
    if (iDevice == deviceID) {
        strcpy(deviceNameOut, snd_device_name_get_hint(*nextDeviceHint, "NAME"));
        result = true;
    }

    snd_device_name_free_hint((void**)deviceHints);


    return result;
}


dra_backend* dra_backend_create_alsa()
{
    dra_backend_alsa* pBackendALSA = (dra_backend_alsa*)calloc(1, sizeof(*pBackendALSA));   // <-- Note the calloc() - makes it easier to handle the on_error goto.
    if (pBackendALSA == NULL) {
        return NULL;
    }

    pBackendALSA->type = DR_AUDIO_BACKEND_TYPE_ALSA;


    return (dra_backend*)pBackendALSA;

#if 0
on_error:
    if (pBackendALSA != NULL) {
        free(pBackendALSA);
    }

    return NULL;
#endif
}

void dra_backend_delete_alsa(dra_backend* pBackend)
{
    dra_backend_alsa* pBackendALSA = (dra_backend_alsa*)pBackend;
    if (pBackendALSA == NULL) {
        return;
    }

    free(pBackend);
}


dra_backend_device* dra_backend_device_open_playback_alsa(dra_backend* pBackend, unsigned int deviceID, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    dra_backend_alsa* pBackendALSA = (dra_backend_alsa*)pBackend;
    if (pBackendALSA == NULL) {
        return NULL;
    }

    snd_pcm_hw_params_t* pHWParams = NULL;

    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)calloc(1, sizeof(*pDeviceALSA));
    if (pDeviceALSA == NULL) {
        goto on_error;
    }

    pDeviceALSA->pBackend = pBackend;
    pDeviceALSA->type = dra_device_type_playback;
    pDeviceALSA->channels = channels;
    pDeviceALSA->sampleRate = sampleRate;

    char deviceName[1024];
    if (!dra_alsa__get_device_name_by_id(pBackend, deviceID, deviceName)) {     // <-- This will return "default" if deviceID is 0.
        goto on_error;
    }

    if (snd_pcm_open(&pDeviceALSA->deviceALSA, deviceName, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        goto on_error;
    }


    if (snd_pcm_hw_params_malloc(&pHWParams) < 0) {
        goto on_error;
    }

    if (snd_pcm_hw_params_any(pDeviceALSA->deviceALSA, pHWParams) < 0) {
        goto on_error;
    }

    if (snd_pcm_hw_params_set_access(pDeviceALSA->deviceALSA, pHWParams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        goto on_error;
    }

    if (snd_pcm_hw_params_set_format(pDeviceALSA->deviceALSA, pHWParams, SND_PCM_FORMAT_FLOAT_LE) < 0) {
        goto on_error;
    }


    if (snd_pcm_hw_params_set_rate_near(pDeviceALSA->deviceALSA, pHWParams, &sampleRate, 0) < 0) {
        goto on_error;
    }

    if (snd_pcm_hw_params_set_channels_near(pDeviceALSA->deviceALSA, pHWParams, &channels) < 0) {
        goto on_error;
    }

    pDeviceALSA->sampleRate = sampleRate;
    pDeviceALSA->channels = channels;



    unsigned int periods = DR_AUDIO_DEFAULT_FRAGMENT_COUNT;
    int dir = 1;
    if (snd_pcm_hw_params_set_periods_near(pDeviceALSA->deviceALSA, pHWParams, &periods, &dir) < 0) {
        //printf("Failed to set periods.\n");
        goto on_error;
    }

    pDeviceALSA->fragmentCount = periods;

    //printf("Periods: %d | Direction: %d\n", periods, dir);


    size_t sampleRateInMilliseconds = pDeviceALSA->sampleRate / 1000;
    if (sampleRateInMilliseconds == 0) {
        sampleRateInMilliseconds = 1;
    }


    // According to the ALSA documentation, the value passed to snd_pcm_sw_params_set_avail_min() must be a power
    // of 2 on some hardware. The value passed to this function is the size in frames of a fragment. Thus, to be
    // as robust as possible the size of the hardware buffer should be sized based on the size of a closest power-
    // of-two fragment.
    //
    // To calculate the size of a fragment, the first step is to determine the initial proposed size. From that
    // it is dropped to the previous power of two. The reason for this is that, based on testing, ALSA has good
    // latency characteristics, and less latency is always preferable.
    unsigned int proposedFramesPerFragment = sampleRateInMilliseconds * latencyInMilliseconds;
    unsigned int framesPerFragment = dra_prev_power_of_2(proposedFramesPerFragment);
    if (framesPerFragment == 0) {
        framesPerFragment = 2;
    }

    pDeviceALSA->samplesPerFragment = framesPerFragment * pDeviceALSA->channels;

    if (snd_pcm_hw_params_set_buffer_size(pDeviceALSA->deviceALSA, pHWParams, framesPerFragment * pDeviceALSA->fragmentCount) < 0) {
        //printf("Failed to set buffer size.\n");
        goto on_error;
    }


    if (snd_pcm_hw_params(pDeviceALSA->deviceALSA, pHWParams) < 0) {
        goto on_error;
    }

    snd_pcm_hw_params_free(pHWParams);



    // Software params. There needs to be at least fragmentSize bytes in the hardware buffer before playing it, and there needs
    // be fragmentSize bytes available after every wait.
    snd_pcm_sw_params_t* pSWParams = NULL;
    if (snd_pcm_sw_params_malloc(&pSWParams) < 0) {
        goto on_error;
    }

    if (snd_pcm_sw_params_current(pDeviceALSA->deviceALSA, pSWParams) != 0) {
        goto on_error;
    }

    if (snd_pcm_sw_params_set_start_threshold(pDeviceALSA->deviceALSA, pSWParams, framesPerFragment) != 0) {
        goto on_error;
    }
    if (snd_pcm_sw_params_set_avail_min(pDeviceALSA->deviceALSA, pSWParams, framesPerFragment) != 0) {
        goto on_error;
    }

    if (snd_pcm_sw_params(pDeviceALSA->deviceALSA, pSWParams) != 0) {
        goto on_error;
    }
    snd_pcm_sw_params_free(pSWParams);


    // The intermediary buffer that will be used for mapping/unmapping.
    pDeviceALSA->isBufferMapped = false;
    pDeviceALSA->pIntermediaryBuffer = (float*)malloc(pDeviceALSA->samplesPerFragment * sizeof(float));
    if (pDeviceALSA->pIntermediaryBuffer == NULL) {
        goto on_error;
    }

    return (dra_backend_device*)pDeviceALSA;

on_error:
    if (pHWParams) {
        snd_pcm_hw_params_free(pHWParams);
    }

    if (pDeviceALSA != NULL) {
        if (pDeviceALSA->deviceALSA != NULL) {
            snd_pcm_close(pDeviceALSA->deviceALSA);
        }

        free(pDeviceALSA->pIntermediaryBuffer);
        free(pDeviceALSA);
    }

    return NULL;
}

dra_backend_device* dra_backend_device_open_recording_alsa(dra_backend* pBackend, unsigned int deviceID, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    // Not yet implemented.
    (void)pBackend;
    (void)deviceID;
    (void)channels;
    (void)sampleRate;
    (void)latencyInMilliseconds;
    return NULL;
}

dra_backend_device* dra_backend_device_open_alsa(dra_backend* pBackend, dra_device_type type, unsigned int deviceID, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    if (type == dra_device_type_playback) {
        return dra_backend_device_open_playback_alsa(pBackend, deviceID, channels, sampleRate, latencyInMilliseconds);
    } else {
        return dra_backend_device_open_recording_alsa(pBackend, deviceID, channels, sampleRate, latencyInMilliseconds);
    }
}

void dra_backend_device_close_alsa(dra_backend_device* pDevice)
{
    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)pDevice;
    if (pDeviceALSA == NULL) {
        return;
    }

    if (pDeviceALSA->deviceALSA != NULL) {
        snd_pcm_close(pDeviceALSA->deviceALSA);
    }

    free(pDeviceALSA->pIntermediaryBuffer);
    free(pDeviceALSA);
}

void dra_backend_device_play(dra_backend_device* pDevice)
{
    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)pDevice;
    if (pDeviceALSA == NULL) {
        return;
    }

    snd_pcm_prepare(pDeviceALSA->deviceALSA);
    pDeviceALSA->isPlaying = true;
}

void dra_backend_device_stop(dra_backend_device* pDevice)
{
    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)pDevice;
    if (pDeviceALSA == NULL) {
        return;
    }

    snd_pcm_drop(pDeviceALSA->deviceALSA);
    pDeviceALSA->isPlaying = false;
}

bool dra_backend_device_wait(dra_backend_device* pDevice)   // <-- Returns true if the function has returned because it needs more data; false if the device has been stopped or an error has occured.
{
    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)pDevice;
    if (pDeviceALSA == NULL) {
        return false;
    }

    if (!pDeviceALSA->isPlaying) {
        return false;
    }

    int result = snd_pcm_wait(pDeviceALSA->deviceALSA, -1);
    if (result > 0) {
        return true;
    }

    if (result == -EPIPE) {
        // xrun. Prepare the device again and just return true.
        snd_pcm_prepare(pDeviceALSA->deviceALSA);
        return true;
    }

    return false;
}

void* dra_backend_device_map_next_fragment(dra_backend_device* pDevice, size_t* pSamplesInFragmentOut)
{
    assert(pSamplesInFragmentOut != NULL);

    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)pDevice;
    if (pDeviceALSA == NULL) {
        return NULL;
    }

    if (pDeviceALSA->isBufferMapped) {
        return NULL;    // A fragment is already mapped. Can only have a single fragment mapped at a time.
    }

    *pSamplesInFragmentOut = pDeviceALSA->samplesPerFragment;
    return pDeviceALSA->pIntermediaryBuffer;
}

void dra_backend_device_unmap_next_fragment(dra_backend_device* pDevice)
{
    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)pDevice;
    if (pDeviceALSA == NULL) {
        return;
    }

    if (pDeviceALSA->isBufferMapped) {
        return;    // Nothing is mapped.
    }

    // Unammping is when the data is written to the device.
    snd_pcm_writei(pDeviceALSA->deviceALSA, pDeviceALSA->pIntermediaryBuffer, pDeviceALSA->samplesPerFragment / pDeviceALSA->channels);
}
#endif  // DR_AUDIO_NO_ALSA
#endif  // __linux__


void dra_thread_wait_and_delete(dra_thread thread)
{
    dra_thread_wait(thread);
    dra_thread_delete(thread);
}


dra_backend* dra_backend_create()
{
    dra_backend* pBackend = NULL;

#ifdef DR_AUDIO_ENABLE_DSOUND
    pBackend = dra_backend_create_dsound();
    if (pBackend != NULL) {
        return pBackend;
    }
#endif

#ifdef DR_AUDIO_ENABLE_ALSA
    pBackend = dra_backend_create_alsa();
    if (pBackend != NULL) {
        return pBackend;
    }
#endif

    // If we get here it means we couldn't find a backend. Default to a NULL backend? Returning NULL makes it clearer that an error occured.
    return NULL;
}

void dra_backend_delete(dra_backend* pBackend)
{
    if (pBackend == NULL) {
        return;
    }

#ifdef DR_AUDIO_ENABLE_DSOUND
    if (pBackend->type == DR_AUDIO_BACKEND_TYPE_DSOUND) {
        dra_backend_delete_dsound(pBackend);
        return;
    }
#endif

#ifdef DR_AUDIO_ENABLE_ALSA
    if (pBackend->type == DR_AUDIO_BACKEND_TYPE_ALSA) {
        dra_backend_delete_alsa(pBackend);
        return;
    }
#endif

    // Should never get here. If this assert is triggered it means you haven't plugged in the API in the list above.
    assert(false);
}


dra_backend_device* dra_backend_device_open(dra_backend* pBackend, dra_device_type type, unsigned int deviceID, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    if (pBackend == NULL) {
        return NULL;
    }

#ifdef DR_AUDIO_ENABLE_DSOUND
    if (pBackend->type == DR_AUDIO_BACKEND_TYPE_DSOUND) {
        return dra_backend_device_open_dsound(pBackend, type, deviceID, channels, sampleRate, latencyInMilliseconds);
    }
#endif

#ifdef DR_AUDIO_ENABLE_ALSA
    if (pBackend->type == DR_AUDIO_BACKEND_TYPE_ALSA) {
        return dra_backend_device_open_alsa(pBackend, type, deviceID, channels, sampleRate, latencyInMilliseconds);
    }
#endif


    // Should never get here. If this assert is triggered it means you haven't plugged in the API in the list above.
    assert(false);
    return NULL;
}

void dra_backend_device_close(dra_backend_device* pDevice)
{
    if (pDevice == NULL) {
        return;
    }

    assert(pDevice->pBackend != NULL);

#ifdef DR_AUDIO_ENABLE_DSOUND
    if (pDevice->pBackend->type == DR_AUDIO_BACKEND_TYPE_DSOUND) {
        dra_backend_device_close_dsound(pDevice);
        return;
    }
#endif

#ifdef DR_AUDIO_ENABLE_ALSA
    if (pDevice->pBackend->type == DR_AUDIO_BACKEND_TYPE_ALSA) {
        dra_backend_device_close_alsa(pDevice);
        return;
    }
#endif
}



///////////////////////////////////////////////////////////////////////////////
//
// Cross Platform
//
///////////////////////////////////////////////////////////////////////////////

// Reads the next frame.
//
// Frames are retrieved with respect to the device the voice is attached to. What this basically means is
// that any data conversions will be done within this function.
//
// The return value is a pointer to the voice containing the converted samples, always as floating point.
//
// If the voice is in the same format as the device (floating point, same sample rate and channels), then
// this function will be on a fast path and will return almost immediately with a pointer that points to
// the voice's actual data without any data conversion.
//
// If an error occurs, null is returned. Null will be returned if the end of the voice's buffer is reached
// and it's non-looping. This will not return NULL if the voice is looping - it will just loop back to the
// start as one would expect.
//
// This function is not thread safe, but can be called from multiple threads if you do your own
// synchronization. Just keep in mind that the return value may point to the voice's actual internal data.
float* dra_voice__next_frame(dra_voice* pVoice);

// dra_voice__next_frames()
size_t dra_voice__next_frames(dra_voice* pVoice, size_t frameCount, float* pSamplesOut);


dra_context* dra_context_create()
{
    // We need a backend first.
    dra_backend* pBackend = dra_backend_create();
    if (pBackend == NULL) {
        return NULL;    // Failed to create a backend.
    }

    dra_context* pContext = (dra_context*)malloc(sizeof(*pContext));
    if (pContext == NULL) {
        return NULL;
    }

    pContext->pBackend = pBackend;

    return pContext;
}

void dra_context_delete(dra_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    dra_backend_delete(pContext->pBackend);
    free(pContext);
}


void dra_event_queue__schedule_event(dra__event_queue* pQueue, dra__event* pEvent)
{
    if (pQueue == NULL || pEvent == NULL) {
        return;
    }

    dra_mutex_lock(pQueue->lock);
    {
        if (pQueue->eventCount == pQueue->eventBufferSize)
        {
            // Ran out of room. Resize.
            size_t newEventBufferSize = (pQueue->eventBufferSize == 0) ? 16 : pQueue->eventBufferSize*2;
            dra__event* pNewEvents = (dra__event*)malloc(newEventBufferSize * sizeof(*pNewEvents));
            if (pNewEvents == NULL) {
                return;
            }

            for (size_t i = 0; i < pQueue->eventCount; ++i) {
                pQueue->pEvents[i] = pQueue->pEvents[(pQueue->firstEvent + i) % pQueue->eventBufferSize];
            }

            pQueue->firstEvent = 0;
            pQueue->eventBufferSize = newEventBufferSize;
            pQueue->pEvents = pNewEvents;
        }

        assert(pQueue->eventCount < pQueue->eventBufferSize);

        pQueue->pEvents[(pQueue->firstEvent + pQueue->eventCount) % pQueue->eventBufferSize] = *pEvent;
        pQueue->eventCount += 1;
    }
    dra_mutex_unlock(pQueue->lock);
}

void dra_event_queue__cancel_events_of_buffer(dra__event_queue* pQueue, dra_voice* pVoice)
{
    if (pQueue == NULL || pVoice == NULL) {
        return;
    }

    dra_mutex_lock(pQueue->lock);
    {
        // We don't actually remove anything from the queue, but instead zero out the event's data.
        for (size_t i = 0; i < pQueue->eventCount; ++i) {
            dra__event* pEvent = &pQueue->pEvents[(pQueue->firstEvent + i) % pQueue->eventBufferSize];
            if (pEvent->pVoice == pVoice) {
                pEvent->pVoice = NULL;
                pEvent->proc = NULL;
            }
        }
    }
    dra_mutex_unlock(pQueue->lock);
}

bool dra_event_queue__next_event(dra__event_queue* pQueue, dra__event* pEventOut)
{
    if (pQueue == NULL || pEventOut == NULL) {
        return false;
    }

    bool result = false;
    dra_mutex_lock(pQueue->lock);
    {
        if (pQueue->eventCount > 0) {
            *pEventOut = pQueue->pEvents[pQueue->firstEvent];
            pQueue->firstEvent = (pQueue->firstEvent + 1) % pQueue->eventBufferSize;
            pQueue->eventCount -= 1;
            result = true;
        }
    }
    dra_mutex_unlock(pQueue->lock);

    return result;
}

void dra_event_queue__post_events(dra__event_queue* pQueue)
{
    if (pQueue == NULL) {
        return;
    }

    dra__event nextEvent;
    while (dra_event_queue__next_event(pQueue, &nextEvent)) {
        if (nextEvent.proc) {
            nextEvent.proc(nextEvent.pVoice, nextEvent.id, nextEvent.pUserData);
        }
    }
}


void dra_device__post_event(dra_device* pDevice, dra_thread_event_type type)
{
    assert(pDevice != NULL);

    pDevice->nextThreadEventType = type;
    dra_semaphore_release(pDevice->threadEventSem);
}

void dra_device__lock(dra_device* pDevice)
{
    assert(pDevice != NULL);
    dra_mutex_lock(pDevice->mutex);
}

void dra_device__unlock(dra_device* pDevice)
{
    assert(pDevice != NULL);
    dra_mutex_unlock(pDevice->mutex);
}

bool dra_device__is_playing_nolock(dra_device* pDevice)
{
    assert(pDevice != NULL);
    return pDevice->isPlaying;
}

bool dra_device__mix_next_fragment(dra_device* pDevice)
{
    assert(pDevice != NULL);

    size_t samplesInFragment;
    void* pSampleData = dra_backend_device_map_next_fragment(pDevice->pBackendDevice, &samplesInFragment);
    if (pSampleData == NULL) {
        dra_backend_device_stop(pDevice->pBackendDevice);
        return false;
    }

    size_t framesInFragment = samplesInFragment / pDevice->channels;
    size_t framesMixed = dra_mixer_mix_next_frames(pDevice->pMasterMixer, framesInFragment);

    memcpy(pSampleData, pDevice->pMasterMixer->pStagingBuffer, (size_t)samplesInFragment * sizeof(float));
    dra_backend_device_unmap_next_fragment(pDevice->pBackendDevice);

    if (framesMixed < framesInFragment) {
        pDevice->stopOnNextFragment = true;
    }

    printf("Mixed next fragment into %p\n", pSampleData);
    return true;
}

void dra_device__play(dra_device* pDevice)
{
    assert(pDevice != NULL);

    dra_device__lock(pDevice);
    {
        // Don't do anything if the device is already playing.
        if (!dra_device__is_playing_nolock(pDevice))
        {
            assert(pDevice->playingVoicesCount > 0);

            // Before playing the backend device we need to ensure the first fragment is filled with valid audio data.
            if (dra_device__mix_next_fragment(pDevice))
            {
                dra_backend_device_play(pDevice->pBackendDevice);
                dra_device__post_event(pDevice, dra_thread_event_type_play);
                pDevice->isPlaying = true;
                pDevice->stopOnNextFragment = false;
            }
        }
    }
    dra_device__unlock(pDevice);
}

void dra_device__stop(dra_device* pDevice)
{
    assert(pDevice != NULL);

    dra_device__lock(pDevice);
    {
        // Don't do anything if the device is already stopped.
        if (dra_device__is_playing_nolock(pDevice))
        {
            assert(pDevice->playingVoicesCount == 0);

            dra_backend_device_stop(pDevice->pBackendDevice);
            pDevice->isPlaying = false;
        }
    }
    dra_device__unlock(pDevice);
}

void dra_device__voice_playback_count_inc(dra_device* pDevice)
{
    assert(pDevice != NULL);

    dra_device__lock(pDevice);
    {
        pDevice->playingVoicesCount += 1;
    }
    dra_device__unlock(pDevice);
}

void dra_device__voice_playback_count_dec(dra_device* pDevice)
{
    dra_device__lock(pDevice);
    {
        pDevice->playingVoicesCount -= 1;
    }
    dra_device__unlock(pDevice);
}

// The entry point signature is slightly different depending on whether or not we're using Win32 or POSIX threads.
#ifdef _WIN32
DWORD dra_device__thread_proc(LPVOID pData)
#else
void* dra_device__thread_proc(void* pData)
#endif
{
    dra_device* pDevice = (dra_device*)pData;
    assert(pDevice != NULL);

    // The thread is always open for the life of the device. The loop below will only terminate when a terminate message is received.
    for (;;)
    {
        // Wait for an event...
        dra_semaphore_wait(pDevice->threadEventSem);

        if (pDevice->nextThreadEventType == dra_thread_event_type_terminate) {
            printf("Terminated!\n");
            break;
        }

        if (pDevice->nextThreadEventType == dra_thread_event_type_play)
        {
            // There could be "play" events needing to be posted.
            dra_event_queue__post_events(&pDevice->eventQueue);

            // Wait for the device to request more data...
            while (dra_backend_device_wait(pDevice->pBackendDevice))
            {
                dra_event_queue__post_events(&pDevice->eventQueue);

                if (pDevice->stopOnNextFragment) {
                    dra_device__stop(pDevice);  // <-- Don't break from the loop here. Instead have dra_backend_device_wait() return naturally from the stop notification.
                } else {
                    dra_device__mix_next_fragment(pDevice);
                }
            }

            // There could be some events needing to be posted.
            dra_event_queue__post_events(&pDevice->eventQueue);
            printf("Stopped!\n");

            // Don't fall through.
            continue;
        }
    }

    return 0;
}

dra_device* dra_device_open_ex(dra_context* pContext, dra_device_type type, unsigned int deviceID, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    bool ownsContext = false;
    if (pContext == NULL) {
        pContext = dra_context_create();
        if (pContext == NULL) {
            return NULL;
        }

        ownsContext = true;
    }


    if (sampleRate == 0) {
        sampleRate = DR_AUDIO_DEFAULT_SAMPLE_RATE;
    }


    dra_device* pDevice = (dra_device*)calloc(1, sizeof(*pDevice));
    if (pDevice == NULL) {
        return NULL;
    }

    pDevice->pContext    = pContext;
    pDevice->channels    = channels;
    pDevice->sampleRate  = sampleRate;
    pDevice->ownsContext = ownsContext;

    pDevice->pBackendDevice = dra_backend_device_open(pContext->pBackend, type, deviceID, channels, sampleRate, latencyInMilliseconds);
    if (pDevice->pBackendDevice == NULL) {
        goto on_error;
    }


    pDevice->mutex = dra_mutex_create();
    if (pDevice->mutex == NULL) {
        goto on_error;
    }

    pDevice->threadEventSem = dra_semaphore_create(0);
    if (pDevice->threadEventSem == NULL) {
        goto on_error;
    }

    pDevice->pMasterMixer = dra_mixer_create(pDevice);
    if (pDevice->pMasterMixer == NULL) {
        goto on_error;
    }


    pDevice->eventQueue.lock = dra_mutex_create();
    if (pDevice->eventQueue.lock == NULL) {
        goto on_error;
    }


    // Create the thread last to ensure the device is in a valid state as soon as the entry procedure is run.
    pDevice->thread = dra_thread_create(dra_device__thread_proc, pDevice);
    if (pDevice->thread == NULL) {
        goto on_error;
    }

    return pDevice;

on_error:
    if (pDevice != NULL) {
        if (pDevice->pMasterMixer != NULL) {
            dra_mixer_delete(pDevice->pMasterMixer);
        }

        if (pDevice->pBackendDevice != NULL) {
            dra_backend_device_close(pDevice->pBackendDevice);
        }

        if (pDevice->threadEventSem != NULL) {
            dra_semaphore_delete(pDevice->threadEventSem);
        }

        if (pDevice->mutex != NULL) {
            dra_mutex_delete(pDevice->mutex);
        }

        if (pDevice->ownsContext) {
            dra_context_delete(pDevice->pContext);
        }

        free(pDevice);
    }

    return NULL;
}

dra_device* dra_device_open(dra_context* pContext, dra_device_type type)
{
    return dra_device_open_ex(pContext, type, 0, 0, DR_AUDIO_DEFAULT_SAMPLE_RATE, DR_AUDIO_DEFAULT_LATENCY);
}

void dra_device_close(dra_device* pDevice)
{
    if (pDevice == NULL) {
        return;
    }

    // Mark the device as closed in order to prevent other threads from doing work after closing.
    dra_device__lock(pDevice);
    {
        pDevice->isClosed = true;
    }
    dra_device__unlock(pDevice);

    // Stop playback before doing anything else.
    dra_device__stop(pDevice);

    // The background thread needs to be terminated at this point.
    dra_device__post_event(pDevice, dra_thread_event_type_terminate);
    dra_thread_wait_and_delete(pDevice->thread);


    // At this point the device is marked as closed which should prevent voice's and mixers from being created and deleted. We now need
    // to delete the master mixer which in turn will delete all of the attached voices and submixers.
    if (pDevice->pMasterMixer != NULL) {
        dra_mixer_delete(pDevice->pMasterMixer);
    }


    if (pDevice->pBackendDevice != NULL) {
        dra_backend_device_close(pDevice->pBackendDevice);
    }

    if (pDevice->threadEventSem != NULL) {
        dra_semaphore_delete(pDevice->threadEventSem);
    }

    if (pDevice->mutex != NULL) {
        dra_mutex_delete(pDevice->mutex);
    }

    if (pDevice->ownsContext) {
        dra_context_delete(pDevice->pContext);
    }

    free(pDevice);
}




dra_mixer* dra_mixer_create(dra_device* pDevice)
{
    // There needs to be two blocks of memory at the end of the mixer - one for the staging buffer and another for the buffer that
    // will store the float32 samples of the voice currently being mixed.
    size_t extraDataSize = (size_t)pDevice->pBackendDevice->samplesPerFragment * sizeof(float) * 2;

    dra_mixer* pMixer = (dra_mixer*)malloc(sizeof(*pMixer) - sizeof(pMixer->pData) + extraDataSize);
    if (pMixer == NULL) {
        return NULL;
    }
    memset(pMixer, 0, sizeof(*pMixer));

    pMixer->pDevice = pDevice;
    pMixer->linearVolume = 1;

    pMixer->pStagingBuffer = pMixer->pData;
    pMixer->pNextSamplesToMix = pMixer->pStagingBuffer + pDevice->pBackendDevice->samplesPerFragment;

    // Attach the mixer to the master mixer by default. If the master mixer is null it means we're creating the master mixer itself.
    if (pDevice->pMasterMixer != NULL) {
        dra_mixer_attach_submixer(pDevice->pMasterMixer, pMixer);
    }

    return pMixer;
}

void dra_mixer_delete(dra_mixer* pMixer)
{
    if (pMixer == NULL) {
        return;
    }

    dra_mixer_detach_all_submixers(pMixer);
    dra_mixer_detach_all_voices(pMixer);

    if (pMixer->pParentMixer != NULL) {
        dra_mixer_detach_submixer(pMixer->pParentMixer, pMixer);
    }

    free(pMixer);
}

void dra_mixer_attach_submixer(dra_mixer* pMixer, dra_mixer* pSubmixer)
{
    if (pMixer == NULL || pSubmixer == NULL) {
        return;
    }

    if (pSubmixer->pParentMixer != NULL) {
        dra_mixer_detach_submixer(pSubmixer->pParentMixer, pSubmixer);
    }


    pSubmixer->pParentMixer = pMixer;

    if (pMixer->pFirstChildMixer == NULL) {
        pMixer->pFirstChildMixer = pSubmixer;
        pMixer->pLastChildMixer  = pSubmixer;
        return;
    }

    assert(pMixer->pLastChildMixer != NULL);
    pMixer->pLastChildMixer->pNextSiblingMixer = pSubmixer;
    pSubmixer->pNextSiblingMixer = pMixer->pLastChildMixer;
    pMixer->pLastChildMixer = pSubmixer;
}

void dra_mixer_detach_submixer(dra_mixer* pMixer, dra_mixer* pSubmixer)
{
    if (pMixer == NULL || pSubmixer == NULL) {
        return;
    }

    if (pSubmixer->pParentMixer == pMixer) {
        return; // Doesn't have the same parent.
    }


    // Detach from parent.
    if (pSubmixer->pParentMixer->pFirstChildMixer == pSubmixer) {
        pSubmixer->pParentMixer->pFirstChildMixer = pSubmixer->pNextSiblingMixer;
    }
    if (pSubmixer->pParentMixer->pLastChildMixer == pSubmixer) {
        pSubmixer->pParentMixer->pLastChildMixer = pSubmixer->pPrevSiblingMixer;
    }

    pSubmixer->pParentMixer = NULL;


    // Detach from siblings.
    if (pSubmixer->pPrevSiblingMixer) {
        pSubmixer->pPrevSiblingMixer->pNextSiblingMixer = pSubmixer->pNextSiblingMixer;
    }
    if (pSubmixer->pNextSiblingMixer) {
        pSubmixer->pNextSiblingMixer->pPrevSiblingMixer = pSubmixer->pPrevSiblingMixer;
    }

    pSubmixer->pNextSiblingMixer = NULL;
    pSubmixer->pPrevSiblingMixer = NULL;
}

void dra_mixer_detach_all_submixers(dra_mixer* pMixer)
{
    if (pMixer == NULL) {
        return;
    }

    while (pMixer->pFirstChildMixer != NULL) {
        dra_mixer_detach_submixer(pMixer, pMixer->pFirstChildMixer);
    }
}

void dra_mixer_attach_voice(dra_mixer* pMixer, dra_voice* pVoice)
{
    if (pMixer == NULL || pVoice == NULL) {
        return;
    }

    if (pVoice->pMixer != NULL) {
        dra_mixer_detach_voice(pVoice->pMixer, pVoice);
    }

    pVoice->pMixer = pMixer;

    if (pMixer->pFirstVoice == NULL) {
        pMixer->pFirstVoice = pVoice;
        pMixer->pLastVoice  = pVoice;
        return;
    }

    // Attach the voice to the end of the list.
    pVoice->pPrevVoice = pMixer->pLastVoice;
    pVoice->pNextVoice = NULL;

    pMixer->pLastVoice->pNextVoice = pVoice;
    pMixer->pLastVoice = pVoice;
}

void dra_mixer_detach_voice(dra_mixer* pMixer, dra_voice* pVoice)
{
    if (pMixer == NULL || pVoice == NULL) {
        return;
    }


    // Detach from mixer.
    if (pMixer->pFirstVoice == pVoice) {
        pMixer->pFirstVoice = pMixer->pFirstVoice->pNextVoice;
    }
    if (pMixer->pLastVoice == pVoice) {
        pMixer->pLastVoice = pMixer->pLastVoice->pPrevVoice;
    }

    pVoice->pMixer = NULL;


    // Remove from list.
    if (pVoice->pNextVoice) {
        pVoice->pNextVoice->pPrevVoice = pVoice->pPrevVoice;
    }
    if (pVoice->pPrevVoice) {
        pVoice->pPrevVoice->pNextVoice = pVoice->pNextVoice;
    }

    pVoice->pNextVoice = NULL;
    pVoice->pPrevVoice = NULL;

}

void dra_mixer_detach_all_voices(dra_mixer* pMixer)
{
    if (pMixer == NULL) {
        return;
    }

    while (pMixer->pFirstVoice) {
        dra_mixer_detach_voice(pMixer, pMixer->pFirstVoice);
    }
}

void dra_mixer_set_volume(dra_mixer* pMixer, float linearVolume)
{
    if (pMixer == NULL) {
        return;
    }

    pMixer->linearVolume = linearVolume;
}

float dra_mixer_get_volume(dra_mixer* pMixer)
{
    if (pMixer == NULL) {
        return 0;
    }

    return pMixer->linearVolume;
}

size_t dra_mixer_mix_next_frames(dra_mixer* pMixer, size_t frameCount)
{
    if (pMixer == NULL) {
        return 0;
    }

    if (pMixer->pFirstVoice == NULL && pMixer->pFirstChildMixer) {
        return 0;
    }

    size_t framesMixed = 0;

    // Mixing works by simply adding together the sample data of each voice and submixer. We just start at 0 and then
    // just accumulate each one.
    memset(pMixer->pStagingBuffer, 0, frameCount * pMixer->pDevice->channels * sizeof(float));

    // Voices first. Doesn't really matter if we do voices or submixers first.
    for (dra_voice* pVoice = pMixer->pFirstVoice; pVoice != NULL; pVoice = pVoice->pNextVoice)
    {
        if (pVoice->isPlaying) {
            size_t framesJustRead = dra_voice__next_frames(pVoice, frameCount, pMixer->pNextSamplesToMix);
            for (size_t i = 0; i < framesJustRead * pMixer->pDevice->channels; ++i) {
                pMixer->pStagingBuffer[i] += (pMixer->pNextSamplesToMix[i] * pVoice->linearVolume);
            }

            // Has the end of the voice's buffer been reached?
            if (framesJustRead < frameCount)
            {
                // We'll get here if the end of the voice's buffer has been reached. The voice needs to be forcefully stopped to
                // ensure the device is aware of it and is able to put itself into a dormant state if necessary. Also note that
                // the playback position is moved back to start. The rationale for this is that it's a little bit more useful than
                // just leaving the playback position sitting on the end. Also it allows an application to restart playback with
                // a single call to dra_voice_play() without having to explicitly set the playback position.
                pVoice->currentReadPos = 0;
                dra_voice_stop(pVoice);
            }

            if (framesMixed < framesJustRead) {
                framesMixed = framesJustRead;
            }
        }
    }

    // Submixers.
    for (dra_mixer* pSubmixer = pMixer->pFirstChildMixer; pSubmixer != NULL; pSubmixer = pSubmixer->pNextSiblingMixer)
    {
        size_t framesJustMixed = dra_mixer_mix_next_frames(pSubmixer, frameCount);
        for (size_t i = 0; i < framesJustMixed * pMixer->pDevice->channels; ++i) {
            pMixer->pStagingBuffer[i] += pSubmixer->pStagingBuffer[i];
        }

        if (framesMixed < framesJustMixed) {
            framesMixed = framesJustMixed;
        }
    }


    // At this point the mixer's effects and volume need to be applied to each sample.
    size_t samplesMixed = framesMixed * pMixer->pDevice->channels;
    for (size_t i = 0; i < samplesMixed; ++i) {
        pMixer->pStagingBuffer[i] *= pMixer->linearVolume;
    }


    // Finally we need to ensure every samples is clamped to -1 to 1. There are two easy ways to do this: clamp or normalize. For now I'm just
    // clamping to keep it simple, but it might be valuable to make this configurable.
    for (size_t i = 0; i < framesMixed * pMixer->pDevice->channels; ++i)
    {
        // TODO: Investigate using SSE here (MINPS/MAXPS)
        // TODO: Investigate if the backends clamp the samples themselves, thus making this redundant.
        if (pMixer->pStagingBuffer[i] < -1) {
            pMixer->pStagingBuffer[i] = -1;
        } else if (pMixer->pStagingBuffer[i] > 1) {
            pMixer->pStagingBuffer[i] = 1;
        }
    }

    return framesMixed;
}



dra_voice* dra_voice_create(dra_device* pDevice, dra_format format, unsigned int channels, unsigned int sampleRate, size_t sizeInBytes, const void* pInitialData)
{
    if (pDevice == NULL || sizeInBytes == 0) {
        return NULL;
    }

    size_t bytesPerSample = dra_get_bytes_per_sample_by_format(format);

    // The number of bytes must be a multiple of the size of a frame.
    if ((sizeInBytes % (bytesPerSample * channels)) != 0) {
        return NULL;
    }


    dra_voice* pVoice = (dra_voice*)calloc(1, sizeof(*pVoice) - sizeof(pVoice->pData) + sizeInBytes);
    if (pVoice == NULL) {
        return NULL;
    }

    pVoice->pDevice = pDevice;
    pVoice->pMixer = NULL;
    pVoice->format = format;
    pVoice->channels = channels;
    pVoice->sampleRate = sampleRate;
    pVoice->linearVolume = 1;
    pVoice->isPlaying = false;
    pVoice->isLooping = false;
    pVoice->frameCount = sizeInBytes / (bytesPerSample * channels);
    pVoice->currentReadPos = 0;
    pVoice->sizeInBytes = sizeInBytes;
    pVoice->pNextVoice = NULL;
    pVoice->pPrevVoice = NULL;

    if (pInitialData != NULL) {
        memcpy(pVoice->pData, pInitialData, sizeInBytes);
    }


    // Sample rate conversion.
    {
        // Nearest.
        pVoice->src.nearest.nearFrameIndex = (size_t)-1;   // <-- Initializing ensures the first frame is handled correctly.
    }


    // Attach the voice to the master mixer by default.
    if (pDevice->pMasterMixer != NULL) {
        dra_mixer_attach_voice(pDevice->pMasterMixer, pVoice);
    }

    return pVoice;
}

dra_voice* dra_voice_create_compatible(dra_device* pDevice, size_t sizeInBytes, const void* pInitialData)
{
    if (pDevice == NULL) {
        return NULL;
    }

    return dra_voice_create(pDevice, dra_format_f32, pDevice->channels, pDevice->sampleRate, sizeInBytes, pInitialData);
}

void dra_voice_delete(dra_voice* pVoice)
{
    if (pVoice == NULL) {
        return;
    }

    dra_voice_stop(pVoice);

    if (pVoice->pMixer != NULL) {
        dra_mixer_detach_voice(pVoice->pMixer, pVoice);
    }

    free(pVoice);
}

void dra_voice_play(dra_voice* pVoice, bool loop)
{
    if (pVoice == NULL) {
        return;
    }

    if (!dra_voice_is_playing(pVoice)) {
        dra_device__voice_playback_count_inc(pVoice->pDevice);
    } else {
        if (dra_voice_is_looping(pVoice) == loop) {
            return;     // Nothing has changed - don't need to do anything.
        }
    }

    pVoice->isPlaying = true;
    pVoice->isLooping = loop;

    dra_event_queue__schedule_event(&pVoice->pDevice->eventQueue, &pVoice->playEvent);

    // When playing a voice we need to ensure the backend device is playing.
    dra_device__play(pVoice->pDevice);
}

void dra_voice_stop(dra_voice* pVoice)
{
    if (pVoice == NULL) {
        return;
    }

    if (!dra_voice_is_playing(pVoice)) {
        return; // The voice is already stopped.
    }

    dra_device__voice_playback_count_dec(pVoice->pDevice);

    pVoice->isPlaying = false;
    pVoice->isLooping = false;

    dra_event_queue__schedule_event(&pVoice->pDevice->eventQueue, &pVoice->stopEvent);
}

bool dra_voice_is_playing(dra_voice* pVoice)
{
    if (pVoice == NULL) {
        return false;
    }

    return pVoice->isPlaying;
}

bool dra_voice_is_looping(dra_voice* pVoice)
{
    if (pVoice == NULL) {
        return false;
    }

    return pVoice->isLooping;
}


void dra_voice_set_volume(dra_voice* pVoice, float linearVolume)
{
    if (pVoice == NULL) {
        return;
    }

    pVoice->linearVolume = linearVolume;
}

float dra_voice_get_volume(dra_voice* pVoice)
{
    if (pVoice == NULL) {
        return 0;
    }

    return pVoice->linearVolume;
}


void dra_f32_to_f32(float* pOut, const float* pIn, size_t sampleCount)
{
    memcpy(pOut, pIn, sampleCount * sizeof(float));
}

void dra_s32_to_f32(float* pOut, const int32_t* pIn, size_t sampleCount)
{
    // TODO: Try SSE-ifying this.
    for (size_t i = 0; i < sampleCount; ++i) {
        pOut[i] = pIn[i] / 2147483648.0f;
    }
}

void dra_s24_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount)
{
    // TODO: Try SSE-ifying this.
    for (size_t i = 0; i < sampleCount; ++i) {
        uint8_t s0 = pIn[i*3 + 0];
        uint8_t s1 = pIn[i*3 + 1];
        uint8_t s2 = pIn[i*3 + 2];

        int32_t sample32 = (int32_t)((s0 << 8) | (s1 << 16) | (s2 << 24));
        pOut[i] = sample32 / 2147483648.0f;
    }
}

void dra_s16_to_f32(float* pOut, const int16_t* pIn, size_t sampleCount)
{
    // TODO: Try SSE-ifying this.
    for (size_t i = 0; i < sampleCount; ++i) {
        pOut[i] = pIn[i] / 32768.0f;
    }
}

void dra_u8_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount)
{
    // TODO: Try SSE-ifying this.
    for (size_t i = 0; i < sampleCount; ++i) {
        pOut[i] = (pIn[i] / 127.5f) - 1;
    }
}


// Generic sample format conversion function. To add basic, unoptimized support for a new format, just add it to this function.
// Format-specific optimizations need to be implemented specifically for each format, at a higher level.
void dra_to_f32(float* pOut, const void* pIn, size_t sampleCount, dra_format format)
{
    switch (format)
    {
        case dra_format_f32:
        {
            dra_f32_to_f32(pOut, (float*)pIn, sampleCount);
        } break;

        case dra_format_s32:
        {
            dra_s32_to_f32(pOut, (int32_t*)pIn, sampleCount);
        } break;

        case dra_format_s24:
        {
            dra_s24_to_f32(pOut, (uint8_t*)pIn, sampleCount);
        } break;

        case dra_format_s16:
        {
            dra_s16_to_f32(pOut, (int16_t*)pIn, sampleCount);
        } break;

        case dra_format_u8:
        {
            dra_u8_to_f32(pOut, (uint8_t*)pIn, sampleCount);
        } break;

        default: break; // Unknown or unsupported format.
    }
}


// Notes on channel shuffling.
//
// Channels are shuffled frame-by-frame by first normalizing everything to floats. Then, a shuffling function is called to
// shuffle the channels in a particular way depending on the destination and source channel assignments.
void dra_shuffle_channels__generic_inc(float* pOut, const float* pIn, unsigned int channelsOut, unsigned int channelsIn)
{
    // This is the generic function for taking a frame with a smaller number of channels and expanding it to a frame with
    // a greater number of channels. This just copies the first channelsIn samples to the output and silences the remaing
    // channels.
    assert(channelsOut > channelsIn);

    for (unsigned int i = 0; i < channelsIn; ++i) {
        pOut[i] = pIn[i];
    }

    // Silence the left over.
    for (unsigned int i = channelsIn; i < channelsOut; ++i) {
        pOut[i] = 0;
    }
}

void dra_shuffle_channels__generic_dec(float* pOut, const float* pIn, unsigned int channelsOut, unsigned int channelsIn)
{
    // This is the opposite of dra_shuffle_channels__generic_inc() - it decreases the number of channels in the input stream
    // by simply stripping the excess channels.
    assert(channelsOut < channelsIn);
    (void)channelsIn;

    // Just copy the first channelsOut.
    for (unsigned int i = 0; i < channelsOut; ++i) {
        pOut[i] = pIn[i];
    }
}

void dra_shuffle_channels(float* pOut, const float* pIn, unsigned int channelsOut, unsigned int channelsIn)
{
    assert(channelsOut != 0);
    assert(channelsIn  != 0);

    if (channelsOut == channelsIn) {
        for (unsigned int i = 0; i < channelsOut; ++i) {
            pOut[i] = pIn[i];
        }
    } else {
        switch (channelsIn)
        {
            case 1:
            {
                // Mono input. This is a simple case - just copy the value of the mono channel to every output channel.
                for (unsigned int i = 0; i < channelsOut; ++i) {
                    pOut[i] = pIn[0];
                }
            } break;

            case 2:
            {
                // Stereo input.
                if (channelsOut == 1)
                {
                    // For mono output, just average.
                    pOut[0] = (pIn[0] + pIn[1]) * 0.5f;
                }
                else
                {
                    // TODO: Do a specialized implementation for all major formats, in particluar 5.1.
                    dra_shuffle_channels__generic_inc(pOut, pIn, channelsOut, channelsIn);
                }
            } break;

            default:
            {
                if (channelsOut == 1)
                {
                    // For mono output, just average each sample.
                    float total = 0;
                    for (unsigned int i = 0; i < channelsIn; ++i) {
                        total += pIn[i];
                    }

                    pOut[0] = total / channelsIn;
                }
                else
                {
                    if (channelsOut > channelsIn) {
                        dra_shuffle_channels__generic_inc(pOut, pIn, channelsOut, channelsIn);
                    } else {
                        dra_shuffle_channels__generic_dec(pOut, pIn, channelsOut, channelsIn);
                    }
                }
            } break;
        }
    }
}

float* dra_voice__next_frame(dra_voice* pVoice)
{
    if (pVoice == NULL) {
        return NULL;
    }


    if (pVoice->format == dra_format_f32 && pVoice->sampleRate == pVoice->pDevice->sampleRate && pVoice->channels == pVoice->pDevice->channels)
    {
        // Fast path.
        if (!pVoice->isLooping && pVoice->currentReadPos == pVoice->frameCount) {
            return NULL;    // At the end of a non-looping voice.
        }

        float* pOut = (float*)pVoice->pData + (pVoice->currentReadPos * pVoice->channels);

        pVoice->currentReadPos += 1;
        if (pVoice->currentReadPos == pVoice->frameCount && pVoice->isLooping) {
            pVoice->currentReadPos = 0;
        }

        return pOut;
    }
    else
    {
        size_t bytesPerSample = dra_get_bytes_per_sample_by_format(pVoice->format);

        if (pVoice->sampleRate == pVoice->pDevice->sampleRate)
        {
            // Same sample rate. This path isn't ideal, but it's not too bad since there is no need for sample rate conversion. There's an annoying
            // switch, though...
            if (!pVoice->isLooping && pVoice->currentReadPos == pVoice->frameCount) {
                return NULL;    // At the end of a non-looping voice.
            }

            float* pOut = pVoice->convertedFrame;

            unsigned int channelsIn  = pVoice->channels;
            unsigned int channelsOut = pVoice->pDevice->channels;
            float tempFrame[DR_AUDIO_MAX_CHANNEL_COUNT];
            size_t sampleOffset = pVoice->currentReadPos * channelsIn;

            // The conversion is done differently depending on the format of the voice.
            if (pVoice->format == dra_format_f32) {
                dra_shuffle_channels(pOut, (float*)pVoice->pData + sampleOffset, channelsOut, channelsIn);
            } else {
                sampleOffset = pVoice->currentReadPos * (channelsIn * bytesPerSample);
                dra_to_f32(tempFrame, (uint8_t*)pVoice->pData + sampleOffset, channelsIn, pVoice->format);
                dra_shuffle_channels(pOut, tempFrame, channelsOut, channelsIn);
            }

            pVoice->currentReadPos += 1;
            if (pVoice->currentReadPos == pVoice->frameCount && pVoice->isLooping) {
                pVoice->currentReadPos = 0;
            }

            return pOut;
        }
        else
        {
            // Different sample rate. This is the truly slow path.
            unsigned int sampleRateIn  = pVoice->sampleRate;
            unsigned int sampleRateOut = pVoice->pDevice->sampleRate;
            unsigned int channelsIn    = pVoice->channels;
            unsigned int channelsOut   = pVoice->pDevice->channels;

            float factor = (float)sampleRateOut / (float)sampleRateIn;
            float invfactor = 1 / factor;

            if (!pVoice->isLooping && pVoice->currentReadPos >= (pVoice->frameCount * factor)) {
                return NULL;    // At the end of a non-looping voice.
            }

            float* pOut = pVoice->convertedFrame;

#if 0
            // Nearest filtering. This results in audio that could be considered outright incorrect. This is placeholder.
            size_t nearFrameIndexIn = (size_t)(pVoice->currentReadPos * invfactor);
            if (nearFrameIndexIn != pVoice->src.nearest.nearFrameIndex) {
                size_t sampleOffset = nearFrameIndexIn * (channelsIn * bytesPerSample);
                dra_to_f32(pVoice->src.nearest.nearFrame, (uint8_t*)pVoice->pData + sampleOffset, channelsIn, pVoice->format);
                pVoice->src.nearest.nearFrameIndex = nearFrameIndexIn;
            }

            dra_shuffle_channels(pOut, pVoice->src.nearest.nearFrame, channelsOut, channelsIn);
#else
            // Linear filtering.
            float timeIn = pVoice->currentReadPos * invfactor;
            size_t prevFrameIndexIn = (size_t)(timeIn);
            size_t nextFrameIndexIn = prevFrameIndexIn + 1;
            if (nextFrameIndexIn >= pVoice->frameCount) {
                nextFrameIndexIn  = pVoice->frameCount-1;
            }

            if (prevFrameIndexIn != pVoice->src.linear.prevFrameIndex)
            {
                size_t sampleOffset = prevFrameIndexIn * (channelsIn * bytesPerSample);
                dra_to_f32(pVoice->src.linear.prevFrame, (uint8_t*)pVoice->pData + sampleOffset, channelsIn, pVoice->format);

                sampleOffset = nextFrameIndexIn * (channelsIn * bytesPerSample);
                dra_to_f32(pVoice->src.linear.nextFrame, (uint8_t*)pVoice->pData + sampleOffset, channelsIn, pVoice->format);

                pVoice->src.linear.prevFrameIndex = prevFrameIndexIn;
            }

            float alpha = timeIn - prevFrameIndexIn;
            float frame[DR_AUDIO_MAX_CHANNEL_COUNT];
            for (unsigned int i = 0; i < pVoice->channels; ++i) {
                frame[i] = dra_mixf(pVoice->src.linear.prevFrame[i], pVoice->src.linear.nextFrame[i], alpha);
            }

            dra_shuffle_channels(pOut, frame, channelsOut, channelsIn);
#endif


            pVoice->currentReadPos += 1;
            if (pVoice->currentReadPos >= (pVoice->frameCount * factor) && pVoice->isLooping) {
                pVoice->currentReadPos = 0;
            }

            return pOut;
        }
    }
}

size_t dra_voice__next_frames(dra_voice* pVoice, size_t frameCount, float* pSamplesOut)
{
    // TODO: Check for the fast path and do a bulk copy rather than frame-by-frame.

    size_t framesRead = 0;
    
    uint64_t prevReadPosLocal = pVoice->currentReadPos * pVoice->channels;

    float* pNextFrame = NULL;
    while ((pNextFrame = dra_voice__next_frame(pVoice)) != NULL && (framesRead < frameCount)) {
        memcpy(pSamplesOut, pNextFrame, pVoice->pDevice->channels * sizeof(float));
        pSamplesOut += pVoice->pDevice->channels;
        framesRead += 1;
    }


    float sampleRateFactor = (pVoice->pDevice->sampleRate / (float)pVoice->sampleRate);

    // Now we need to check if we've got past any notification events and post events for them if so.
    uint64_t currentReadPosLocal = prevReadPosLocal + (framesRead * pVoice->channels);
    for (size_t i = 0; i < pVoice->playbackEventCount; ++i) {
        dra__event* pEvent = &pVoice->playbackEvents[i];
        if (pEvent->sampleIndex*sampleRateFactor > prevReadPosLocal && pEvent->sampleIndex*sampleRateFactor <= currentReadPosLocal) {
            dra_event_queue__schedule_event(&pVoice->pDevice->eventQueue, pEvent);
        }
    }

    return framesRead;
}


void dra_voice_set_on_stop(dra_voice* pVoice, dra_voice_event_proc proc, void* pUserData)
{
    if (pVoice == NULL) {
        return;
    }

    pVoice->stopEvent.id = DR_AUDIO_EVENT_ID_STOP;
    pVoice->stopEvent.pUserData = pUserData;
    pVoice->stopEvent.sampleIndex = 0;
    pVoice->stopEvent.proc = proc;
    pVoice->stopEvent.pVoice = pVoice;
}

void dra_voice_set_on_play(dra_voice* pVoice, dra_voice_event_proc proc, void* pUserData)
{
    if (pVoice == NULL) {
        return;
    }

    pVoice->playEvent.id = DR_AUDIO_EVENT_ID_STOP;
    pVoice->playEvent.pUserData = pUserData;
    pVoice->playEvent.sampleIndex = 0;
    pVoice->playEvent.proc = proc;
    pVoice->playEvent.pVoice = pVoice;
}

bool dra_voice_add_playback_event(dra_voice* pVoice, uint64_t sampleIndex, uint64_t eventID, dra_voice_event_proc proc, void* pUserData)
{
    if (pVoice == NULL) {
        return false;
    }

    if (pVoice->playbackEventCount >= DR_AUDIO_MAX_EVENT_COUNT) {
        return false;
    }

    pVoice->playbackEvents[pVoice->playbackEventCount].id = eventID;
    pVoice->playbackEvents[pVoice->playbackEventCount].pUserData = pUserData;
    pVoice->playbackEvents[pVoice->playbackEventCount].sampleIndex = sampleIndex;
    pVoice->playbackEvents[pVoice->playbackEventCount].proc = proc;
    pVoice->playbackEvents[pVoice->playbackEventCount].pVoice = pVoice;

    pVoice->playbackEventCount += 1;
    return true;
}

void dra_voice_remove_playback_event(dra_voice* pVoice, uint64_t eventID)
{
    if (pVoice == NULL) {
        return;
    }

    for (size_t i = 0; i < pVoice->playbackEventCount; /* DO NOTHING */) {
        if (pVoice->playbackEvents[i].id == eventID) {
            memmove(&pVoice->playbackEvents[i], &pVoice->playbackEvents[i + 1], (pVoice->playbackEventCount - (i+1)) * sizeof(dra__event));
            pVoice->playbackEventCount -= 1;
        } else {
            i += 1;
        }
    }
}




//// Utility APIs ////

unsigned int dra_get_bits_per_sample_by_format(dra_format format)
{
    unsigned int lookup[] = {
        8,      // dra_format_u8
        16,     // dra_format_s16
        24,     // dra_format_s24
        32,     // dra_format_s32
        32      // dra_format_f32
    };

    return lookup[format];
}

unsigned int dra_get_bytes_per_sample_by_format(dra_format format)
{
    return dra_get_bits_per_sample_by_format(format) / 8;
}

bool dra_is_format_float(dra_format format)
{
    return format == dra_format_f32;
}

#endif  //DR_AUDIO_IMPLEMENTATION


// TODO
//
// - Forward declare every backend function and document them.


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
