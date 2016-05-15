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
// dra_buffer                                         <-- Created and owned by a dra_device object and used by an application to deliver audio data to the backend.
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

// TODO
//
// - Forward declare every backend function and document them.

// USAGE
//
// (write me)
//
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

typedef enum
{
    dra_device_type_playback = 0,
    dra_device_type_recording
} dra_device_type;

typedef enum
{
    dra_data_format_pcm_u8 = 0,
    dra_data_format_pcm_s16,
    dra_data_format_pcm_s24,
    dra_data_format_pcm_s32,
    dra_data_format_float_32,
    dra_data_format_float_64,
    dra_data_format_default = dra_data_format_pcm_s32
} dra_data_format;

// dra_event_type is used internally for thread management.
typedef enum
{
    dra_event_type_none,
    dra_event_type_terminate,
    dra_event_type_play
} dra_event_type;

typedef struct dra_backend dra_backend;
typedef struct dra_backend_device dra_backend_device;

typedef struct dra_context dra_context;
typedef struct dra_device dra_device;
typedef struct dra_mixer dra_mixer;
typedef struct dra_buffer dra_buffer;

typedef void* dra_thread;
typedef void* dra_mutex;
typedef void* dra_semaphore;

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
    dra_semaphore eventSem;

    // The event type of the most recent event.
    dra_event_type nextEventType;

    // Whether or not the device is being closed. This is used by the thread to determine if it needs to terminate. When
    // dra_device_close() is called, this flag will be set and eventSem released. The thread will then see this as it's
    // signal to terminate.
    bool isClosed;

    // Whether or not the device is currently playing. When at least one buffer is playing, this will be true. When there
    // are no buffers playing, this will be set to false and the background thread will sit dormant until another buffer
    // starts playing or the device is closed.
    bool isPlaying;


    // The master mixer. This is the one and only top-level mixer.
    dra_mixer* pMasterMixer;


    // The format of the audio data. If incoming audio data does not match it will need to be converted before mixing.
    dra_data_format format;

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
};

struct dra_buffer
{
    // The device that created and owns this buffer.
    dra_device* pDevice;

    // The mixer the buffer is attached to. Should never be null. The mixer doesn't "own" the buffer - the buffer
    // is simply attached to it.
    dra_mixer* pMixer;


    // The format of the audio data contained within this buffer.
    dra_data_format format;

    // The number of channels.
    unsigned int channels;

    // The sample rate in seconds.
    unsigned int sampleRate;


    // Whether or not the buffer is currently playing.
    bool isPlaying;

    // Whether or not the buffer is currently looping. Whether or not the buffer is looping is determined by the last
    // call to dra_buffer_play().
    bool isLooping;


    // The size of the buffer in bytes.
    size_t sizeInBytes;

    // The actual buffer containing the raw audio data.
    uint8_t pData[1];
};


// dra_context_create()
dra_context* dra_context_create();
void dra_context_delete(dra_context* pContext);

// dra_device_open_ex()
//
// If deviceID is 0 the default device for the given type is used.
// format can be dra_data_format_default which is dra_data_format_pcm_s32.
// If channels is set to 0, defaults 2 channels (stereo).
// If sampleRate is set to 0, defaults to 48000.
// If latency is 0, defaults to 50 milliseconds. See notes about latency above.
dra_device* dra_device_open_ex(dra_context* pContext, dra_device_type type, unsigned int deviceID, dra_data_format format, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds);
dra_device* dra_device_open(dra_context* pContext, dra_device_type type);
void dra_device_close(dra_device* pDevice);


// dra_mixer_create()
dra_mixer* dra_mixer_create(dra_device* pDevice);

// dra_mixer_delete()
//
// Deleting a mixer will detach any attached buffers and sub-mixers and attach them to the master mixer. It is
// up to the application to manage the allocation of sub-mixers and buffers. Typically you'll want to delete
// child mixers and buffers before deleting a mixer.
void dra_mixer_delete(dra_mixer* pMixer);

// dra_mixer_attach_submixer()
void dra_mixer_attach_submixer(dra_mixer* pMixer, dra_mixer* pSubmixer);

// dra_mixer_detach_submixer()
void dra_mixer_detach_submixer(dra_mixer* pMixer, dra_mixer* pSubmixer);

// dra_mixer_detach_all_submixers()
void dra_mixer_detach_all_submixers(dra_mixer* pMixer);

// dra_mixer_attach_buffer()
void dra_mixer_attach_buffer(dra_mixer* pMixer, dra_buffer* pBuffer);

// dra_mixer_detach_buffer()
void dra_mixer_detach_buffer(dra_mixer* pMixer, dra_buffer* pBuffer);

// dra_mixer_detach_all_buffers()
void dra_mixer_detach_all_buffers(dra_mixer* pMixer);


// dra_buffer_create()
dra_buffer* dra_buffer_create(dra_device* pDevice, dra_data_format format, unsigned int channels, unsigned int sampleRate, size_t sizeInBytes, const void* pInitialData);
dra_buffer* dra_buffer_create_compatible(dra_device* pDevice, size_t sizeInBytes, const void* pInitialData);

// dra_buffer_delete()
void dra_buffer_delete(dra_buffer* pBuffer);

// dra_buffer_play()
//
// If the mixer the buffer is attached to is not playing, the buffer will be marked as playing, but won't actually play anything until
// the mixer is started again.
void dra_buffer_play(dra_buffer* pBuffer, bool loop);

// dra_buffer_stop()
void dra_buffer_stop(dra_buffer* pBuffer);

// dra_buffer_wait()
void dra_buffer_wait(dra_buffer* pBuffer);

// dra_buffer_is_playing()
bool dra_buffer_is_playing(dra_buffer* pBuffer);

// dra_buffer_is_looping()
bool dra_buffer_is_looping(dra_buffer* pBuffer);


// Utility APIs

// Retrieves the number of bits per sample based on the given format.
unsigned int dra_get_bits_per_sample_by_format(dra_data_format format);

// Determines whether or not the given format is floating point.
bool dra_is_format_float(dra_data_format format);

// Retrieves the number of bytes per sample based on the given format.
unsigned int dra_get_bytes_per_sample_by_format(dra_data_format format);


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
#include <assert.h>
#include <stdio.h>  // For good old printf debugging. Delete later.

#define DR_AUDIO_DEFAULT_CHANNEL_COUNT  2
#define DR_AUDIO_DEFAULT_SAMPLE_RATE    48000
#define DR_AUDIO_DEFAULT_LATENCY        50      // Milliseconds
#define DR_AUDIO_DEFAULT_FRAGMENT_COUNT 2       // The hardware buffer is divided up into latency-sized blocks. This controls that number. Must be at least 2.

#define DR_AUDIO_BACKEND_TYPE_NULL      0
#define DR_AUDIO_BACKEND_TYPE_DSOUND    1
#define DR_AUDIO_BACKEND_TYPE_ALSA      2

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
    dra_data_format format; \
    unsigned int channels; \
    unsigned int sampleRate; \

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

static GUID _g_DSListenerGUID                       = {0x279AFA84, 0x4981, 0x11CE, {0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60}};
static GUID _g_DirectSoundBuffer8GUID               = {0x6825a449, 0x7524, 0x4d82, {0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e}};
static GUID _g_DirectSound3DBuffer8GUID             = {0x279AFA86, 0x4981, 0x11CE, {0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60}};
static GUID _g_DirectSoundNotifyGUID                = {0xb0210783, 0x89cd, 0x11d0, {0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16}};
static GUID _g_KSDATAFORMAT_SUBTYPE_PCM_GUID        = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static GUID _g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

#ifdef __cplusplus
static GUID g_DSListenerGUID                        = _g_DSListenerGUID;
static GUID g_DirectSoundBuffer8GUID                = _g_DirectSoundBuffer8GUID;
static GUID g_DirectSound3DBuffer8GUID              = _g_DirectSound3DBuffer8GUID;
static GUID g_DirectSoundNotifyGUID                 = _g_DirectSoundNotifyGUID;
static GUID g_KSDATAFORMAT_SUBTYPE_PCM_GUID         = _g_KSDATAFORMAT_SUBTYPE_PCM_GUID;
static GUID g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID  = _g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID;
#else
static GUID* g_DSListenerGUID                       = &_g_DSListenerGUID;
static GUID* g_DirectSoundBuffer8GUID               = &_g_DirectSoundBuffer8GUID;
static GUID* g_DirectSound3DBuffer8GUID             = &_g_DirectSound3DBuffer8GUID;
static GUID* g_DirectSoundNotifyGUID                = &_g_DirectSoundNotifyGUID;
static GUID* g_KSDATAFORMAT_SUBTYPE_PCM_GUID        = &_g_KSDATAFORMAT_SUBTYPE_PCM_GUID;
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

dra_backend_device* dra_backend_device_open_playback_dsound(dra_backend* pBackend, unsigned int deviceID, dra_data_format format, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
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
    pDeviceDS->format = format;
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


    // If the channel count is 0 then we need to use the maximum number of channels available. From MSDN:
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
    wf.Format.wBitsPerSample       = (WORD)dra_get_bits_per_sample_by_format(format);
    wf.Format.nBlockAlign          = (wf.Format.nChannels * wf.Format.wBitsPerSample) / 8;
    wf.Format.nAvgBytesPerSec      = wf.Format.nBlockAlign * wf.Format.nSamplesPerSec;
    wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
    wf.dwChannelMask               = 0;
    if (dra_is_format_float(format)) {
        wf.SubFormat = _g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID;
    } else {
        wf.SubFormat = _g_KSDATAFORMAT_SUBTYPE_PCM_GUID;
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



    // The secondary buffer is the buffer where the real audio data will be written to and used by the hardware device. It's
    // size is based on the latency, sample rate and channels.
    //
    // The format of the secondary buffer should exactly match the primary buffer as to avoid unnecessary data conversions.
    size_t sampleRateInMilliseconds = pDeviceDS->sampleRate / 1000;
    if (sampleRateInMilliseconds == 0) {
        sampleRateInMilliseconds = 1;
    }

    size_t fragmentSize = (pDeviceDS->channels * sampleRateInMilliseconds * latencyInMilliseconds) * dra_get_bytes_per_sample_by_format(format);
    size_t hardwareBufferSize = fragmentSize * DR_AUDIO_DEFAULT_FRAGMENT_COUNT;
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

dra_backend_device* dra_backend_device_open_recording_dsound(dra_backend* pBackend, unsigned int deviceID, dra_data_format format, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    // Not yet implemented.
    (void)pBackend;
    (void)deviceID;
    (void)format;
    (void)channels;
    (void)sampleRate;
    (void)latencyInMilliseconds;
    return NULL;
}

dra_backend_device* dra_backend_device_open_dsound(dra_backend* pBackend, dra_device_type type, unsigned int deviceID, dra_data_format format, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    if (type == dra_device_type_playback) {
        return dra_backend_device_open_playback_dsound(pBackend, deviceID, format, channels, sampleRate, latencyInMilliseconds);
    } else {
        return dra_backend_device_open_recording_dsound(pBackend, deviceID, format, channels, sampleRate, latencyInMilliseconds);
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

void dra_backend_device_play(dra_backend_device* pDevice)   // <-- This is a blocking call.
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
        const unsigned int eventIndex = rc - WAIT_OBJECT_0;
        HANDLE hEvent = eventHandles[eventIndex];

        // Has the device been stopped? If so, need to return false.
        if (hEvent == pDeviceDS->hStopEvent) {
            printf("Stopped!\n");
            return false;
        }


        // If we get here it means the event that's been signaled represents a fragment. This is where we fill
        // the hardware buffer with new audio data.
        printf("Requesting next fragment: %d\n", eventIndex);
        return true;
    }

    return false;
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
    pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
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
    sem_t* semaphore = malloc(sizeof(sem_t));
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

on_error:
    if (pBackendALSA != NULL) {
        free(pBackendALSA);
    }

    return NULL;
}

void dra_backend_delete_alsa(dra_backend* pBackend)
{
    dra_backend_alsa* pBackendALSA = (dra_backend_alsa*)pBackend;
    if (pBackendALSA == NULL) {
        return;
    }

    free(pBackend);
}


dra_backend_device* dra_backend_device_open_playback_alsa(dra_backend* pBackend, unsigned int deviceID, dra_data_format format, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    dra_backend_alsa* pBackendALSA = (dra_backend_alsa*)pBackend;
    if (pBackendALSA == NULL) {
        return NULL;
    }

    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)calloc(1, sizeof(*pDeviceALSA));
    if (pDeviceALSA == NULL) {
        goto on_error;
    }

    pDeviceALSA->pBackend = pBackend;
    pDeviceALSA->type = dra_device_type_playback;
    pDeviceALSA->format = format;
    pDeviceALSA->channels = channels;
    pDeviceALSA->sampleRate = sampleRate;

    char deviceName[1024];
    if (!dra_alsa__get_device_name_by_id(pBackend, deviceID, deviceName)) {     // <-- This will return "default" if deviceID is 0.
        goto on_error;
    }

    int error = snd_pcm_open(&pDeviceALSA->deviceALSA, deviceName, SND_PCM_STREAM_PLAYBACK, 0);
    if (error < 0) {
        goto on_error;
    }


    return (dra_backend_device*)pDeviceALSA;

on_error:
    if (pDeviceALSA != NULL) {
        if (pDeviceALSA->deviceALSA != NULL) {
            snd_pcm_close(pDeviceALSA->deviceALSA);
        }

        free(pDeviceALSA);
    }

    return NULL;
}

dra_backend_device* dra_backend_device_open_recording_alsa(dra_backend* pBackend, unsigned int deviceID, dra_data_format format, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    // Not yet implemented.
    (void)pBackend;
    (void)deviceID;
    (void)format;
    (void)channels;
    (void)sampleRate;
    (void)latencyInMilliseconds;
    return NULL;
}

dra_backend_device* dra_backend_device_open_alsa(dra_backend* pBackend, dra_device_type type, unsigned int deviceID, dra_data_format format, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    if (type == dra_device_type_playback) {
        return dra_backend_device_open_playback_alsa(pBackend, deviceID, format, channels, sampleRate, latencyInMilliseconds);
    } else {
        return dra_backend_device_open_recording_alsa(pBackend, deviceID, format, channels, sampleRate, latencyInMilliseconds);
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

    free(pDeviceALSA);
}

void dra_backend_device_play(dra_backend_device* pDevice)   // <-- This is a blocking call.
{
    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)pDevice;
    if (pDeviceALSA == NULL) {
        return;
    }

    // TODO: Implement Me.
}

void dra_backend_device_stop(dra_backend_device* pDevice)
{
    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)pDevice;
    if (pDeviceALSA == NULL) {
        return;
    }

    snd_pcm_drop(pDeviceALSA->deviceALSA);
}

bool dra_backend_device_wait(dra_backend_device* pDevice)   // <-- Returns true if the function has returned because it needs more data; false if the device has been stopped or an error has occured.
{
    dra_backend_device_alsa* pDeviceALSA = (dra_backend_device_alsa*)pDevice;
    if (pDeviceALSA == NULL) {
        return false;
    }

    // TODO: Implement Me.
    return false;
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


dra_backend_device* dra_backend_device_open(dra_backend* pBackend, dra_device_type type, unsigned int deviceID, dra_data_format format, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    if (pBackend == NULL) {
        return NULL;
    }

#ifdef DR_AUDIO_ENABLE_DSOUND
    if (pBackend->type == DR_AUDIO_BACKEND_TYPE_DSOUND) {
        return dra_backend_device_open_dsound(pBackend, type, deviceID, format, channels, sampleRate, latencyInMilliseconds);
    }
#endif

#ifdef DR_AUDIO_ENABLE_ALSA
    if (pBackend->type == DR_AUDIO_BACKEND_TYPE_ALSA) {
        return dra_backend_device_open_alsa(pBackend, type, deviceID, format, channels, sampleRate, latencyInMilliseconds);
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


void dra_device__post_event(dra_device* pDevice, dra_event_type type)
{
    assert(pDevice != NULL);

    pDevice->nextEventType = type;
    dra_semaphore_release(pDevice->eventSem);
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

void dra_device__play(dra_device* pDevice)
{
    assert(pDevice != NULL);

    dra_device__lock(pDevice);
    {
        // Don't do anything if the device is already playing.
        if (!dra_device__is_playing_nolock(pDevice)) {
            dra_backend_device_play(pDevice->pBackendDevice);
            dra_device__post_event(pDevice, dra_event_type_play);
            pDevice->isPlaying = true;
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
        if (dra_device__is_playing_nolock(pDevice)) {
            dra_backend_device_stop(pDevice->pBackendDevice);
            pDevice->isPlaying = false;
        }
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
        dra_semaphore_wait(pDevice->eventSem);

        if (pDevice->nextEventType == dra_event_type_terminate) {
            printf("Terminated!\n");
            break;
        }

        if (pDevice->nextEventType == dra_event_type_play)
        {
            dra_backend_device_play(pDevice->pBackendDevice);
            while (dra_backend_device_wait(pDevice->pBackendDevice)) {
                // If we get here it means there's a fragment that needs mixing and updating.
            }

            // Don't fall through.
            continue;   
        }
    }

    return 0;
}

dra_device* dra_device_open_ex(dra_context* pContext, dra_device_type type, unsigned int deviceID, dra_data_format format, unsigned int channels, unsigned int sampleRate, unsigned int latencyInMilliseconds)
{
    if (pContext == NULL) {
        return NULL;
    }

    if (sampleRate == 0) {
        sampleRate = DR_AUDIO_DEFAULT_SAMPLE_RATE;
    }


    dra_device* pDevice = (dra_device*)calloc(1, sizeof(*pDevice));
    if (pDevice == NULL) {
        return NULL;
    }

    pDevice->pContext = pContext;

    pDevice->pBackendDevice = dra_backend_device_open(pContext->pBackend, type, deviceID, format, channels, sampleRate, latencyInMilliseconds);
    if (pDevice->pBackendDevice == NULL) {
        goto on_error;
    }

    pDevice->mutex = dra_mutex_create();
    if (pDevice->mutex == NULL) {
        goto on_error;
    }

    pDevice->eventSem = dra_semaphore_create(0);
    if (pDevice->eventSem == NULL) {
        goto on_error;
    }

    pDevice->pMasterMixer = dra_mixer_create(pDevice);
    if (pDevice->pMasterMixer == NULL) {
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

        if (pDevice->eventSem != NULL) {
            dra_semaphore_delete(pDevice->eventSem);
        }

        if (pDevice->mutex != NULL) {
            dra_mutex_delete(pDevice->mutex);
        }

        free(pDevice);
    }

    return NULL;
}

dra_device* dra_device_open(dra_context* pContext, dra_device_type type)
{
    return dra_device_open_ex(pContext, type, 0, dra_data_format_default, 0, DR_AUDIO_DEFAULT_SAMPLE_RATE, DR_AUDIO_DEFAULT_LATENCY);
}

void dra_device_close(dra_device* pDevice)
{
    if (pDevice == NULL) {
        return;
    }

    // Mark the device as closed in order to prevent other threads from doing work after closing.
    dra_device__lock(pDevice);
    {
        pDevice->isClosed = false;
    }
    dra_device__unlock(pDevice);
    
    // Stop playback before doing anything else.
    dra_device__stop(pDevice);

    // The background thread needs to be terminated at this point.
    dra_device__post_event(pDevice, dra_event_type_terminate);


    // At this point the device is marked as closed which should prevent buffer's and mixers from being created and deleted. We now need
    // to delete the master mixer which in turn will delete all of the attached buffers and submixers.
    if (pDevice->pMasterMixer != NULL) {
        dra_mixer_delete(pDevice->pMasterMixer);
    }


    if (pDevice->pBackendDevice != NULL) {
        dra_backend_device_close(pDevice->pBackendDevice);
    }

    if (pDevice->eventSem != NULL) {
        dra_semaphore_delete(pDevice->eventSem);
    }

    if (pDevice->mutex != NULL) {
        dra_mutex_delete(pDevice->mutex);
    }

    free(pDevice);
}




dra_mixer* dra_mixer_create(dra_device* pDevice)
{
    dra_mixer* pMixer = (dra_mixer*)calloc(1, sizeof(*pMixer));
    if (pMixer == NULL) {
        return NULL;
    }


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
    dra_mixer_detach_all_buffers(pMixer);

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

    // TODO: Implement me.
}

void dra_mixer_detach_submixer(dra_mixer* pMixer, dra_mixer* pSubmixer)
{
    if (pMixer == NULL || pSubmixer == NULL) {
        return;
    }

    // TODO: Implement me.
}

void dra_mixer_detach_all_submixers(dra_mixer* pMixer)
{
    if (pMixer == NULL) {
        return;
    }

    // TODO: Implement me.
}

void dra_mixer_attach_buffer(dra_mixer* pMixer, dra_buffer* pBuffer)
{
    if (pMixer == NULL || pBuffer == NULL) {
        return;
    }

    // TODO: Implement me.
}

void dra_mixer_detach_buffer(dra_mixer* pMixer, dra_buffer* pBuffer)
{
    if (pMixer == NULL || pBuffer == NULL) {
        return;
    }

    // TODO: Implement me.
}

void dra_mixer_detach_all_buffers(dra_mixer* pMixer)
{
    if (pMixer == NULL) {
        return;
    }

    // TODO: Implement me.
}



dra_buffer* dra_buffer_create(dra_device* pDevice, dra_data_format format, unsigned int channels, unsigned int sampleRate, size_t sizeInBytes, const void* pInitialData)
{
    if (pDevice == NULL || sizeInBytes == 0) {
        return NULL;
    }

    dra_buffer* pBuffer = (dra_buffer*)malloc(sizeof(*pBuffer) - sizeof(pBuffer->pData) + sizeInBytes);
    if (pBuffer == NULL) {
        return NULL;
    }

    pBuffer->pDevice = pDevice;
    pBuffer->pMixer = NULL;
    pBuffer->format = format;
    pBuffer->channels = channels;
    pBuffer->sampleRate = sampleRate;
    pBuffer->sizeInBytes = sizeInBytes;

    if (pInitialData != NULL) {
        memcpy(pBuffer->pData, pInitialData, sizeInBytes);
    }

    // Attach the buffer to the master mixer by default.
    if (pDevice->pMasterMixer != NULL) {
        dra_mixer_attach_buffer(pDevice->pMasterMixer, pBuffer);
    }

    return pBuffer;
}

dra_buffer* dra_buffer_create_compatible(dra_device* pDevice, size_t sizeInBytes, const void* pInitialData)
{
    if (pDevice == NULL) {
        return NULL;
    }

    return dra_buffer_create(pDevice, pDevice->format, pDevice->channels, pDevice->sampleRate, sizeInBytes, pInitialData);
}

void dra_buffer_delete(dra_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return;
    }

    if (pBuffer->pMixer != NULL) {
        dra_mixer_detach_buffer(pBuffer->pMixer, pBuffer);
    }

    free(pBuffer);
}

void dra_buffer_play(dra_buffer* pBuffer, bool loop)
{
    if (pBuffer == NULL) {
        return;
    }

    if (dra_buffer_is_playing(pBuffer) && dra_buffer_is_looping(pBuffer) == loop) {
        return;
    }


    pBuffer->isPlaying = true;
    pBuffer->isLooping = loop;

    // When playing a buffer we need to ensure the backend device is playing.
    dra_device__play(pBuffer->pDevice);
}

void dra_buffer_stop(dra_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return;
    }

    if (!dra_buffer_is_playing(pBuffer)) {
        return; // The buffer is already stopped.
    }


    pBuffer->isPlaying = true;
    pBuffer->isLooping = false;

    // If no other buffer is playing then the backend device should stop playing.
    // TODO: Only stop the device if there are no other buffer's playing!
    dra_device__stop(pBuffer->pDevice);
}

void dra_buffer_wait(dra_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return;
    }

    if (!dra_buffer_is_playing(pBuffer)) {
        return; // The buffer is already stopped.
    }


    // TODO: Implement me.
}

bool dra_buffer_is_playing(dra_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return false;
    }

    return pBuffer->isPlaying;
}

bool dra_buffer_is_looping(dra_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return false;
    }

    return pBuffer->isLooping;
}


//// Utility APIs ////

unsigned int dra_get_bits_per_sample_by_format(dra_data_format format)
{
    unsigned int lookup[] = {
        8,      // dra_data_format_pcm_u8
        16,     // dra_data_format_pcm_s16
        24,     // dra_data_format_pcm_s24
        32,     // dra_data_format_pcm_s32
        32,     // dra_data_format_float_32
        64,     // dra_data_format_float_64
    };

    return lookup[format];
}

unsigned int dra_get_bytes_per_sample_by_format(dra_data_format format)
{
    return dra_get_bits_per_sample_by_format(format) / 8;
}

bool dra_is_format_float(dra_data_format format)
{
    return format == dra_data_format_float_32 || format == dra_data_format_float_64;
}


#endif  //DR_AUDIO_IMPLEMENTATION




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