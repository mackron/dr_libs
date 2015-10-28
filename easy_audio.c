// Public domain. See "unlicense" statement at the end of this file.

#include "easy_audio.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>


////////////////////////////////////////////////////////
// Utilities

// strcpy()
int easyaudio_strcpy(char* dst, size_t dstSizeInBytes, const char* src)
{
#if defined(_MSC_VER)
    return strcpy_s(dst, dstSizeInBytes, src);
#else
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return ERANGE;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }
    
    char* iDst = dst;
    const char* iSrc = src;
    size_t remainingSizeInBytes = dstSizeInBytes;
    while (remainingSizeInBytes > 0 && iSrc[0] != '\0')
    {
        iDst[0] = iSrc[0];

        iDst += 1;
        iSrc += 1;
        remainingSizeInBytes -= 1;
    }

    if (remainingSizeInBytes > 0) {
        iDst[0] = '\0';
    } else {
        dst[0] = '\0';
        return ERANGE;
    }

    return 0;
#endif
}



typedef void              (* easyaudio_delete_context_proc)(easyaudio_context* pContext);
typedef easyaudio_device* (* easyaudio_create_playback_device_proc)(easyaudio_context* pContext, unsigned int deviceIndex);
typedef void              (* easyaudio_delete_playback_device_proc)(easyaudio_device* pDevice);
typedef unsigned int      (* easyaudio_get_playback_device_count_proc)(easyaudio_context* pContext);
typedef bool              (* easyaudio_get_playback_device_info_proc)(easyaudio_context* pContext, unsigned int deviceIndex, easyaudio_device_info* pInfoOut);
typedef easyaudio_buffer* (* easyaudio_create_buffer_proc)(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc);
typedef void              (* easyaudio_delete_buffer_proc)(easyaudio_buffer* pBuffer);
typedef void              (* easyaudio_set_buffer_data_proc)(easyaudio_buffer* pBuffer, unsigned int offset, const void* pData, unsigned int dataSizeInBytes);
typedef void              (* easyaudio_play_proc)(easyaudio_buffer* pBuffer, bool loop);

struct easyaudio_context
{
    // Callbacks.
    easyaudio_delete_context_proc delete_context;
    easyaudio_create_playback_device_proc create_playback_device;
    easyaudio_delete_playback_device_proc delete_playback_device;
    easyaudio_get_playback_device_count_proc get_playback_device_count;
    easyaudio_get_playback_device_info_proc get_playback_device_info;
    easyaudio_create_buffer_proc create_buffer;
    easyaudio_delete_buffer_proc delete_buffer;
    easyaudio_set_buffer_data_proc set_buffer_data;
    easyaudio_play_proc play;
};

struct easyaudio_device
{
    /// The context that owns this device.
    easyaudio_context* pContext;
};

struct easyaudio_buffer
{
    /// The device that owns this buffer.
    easyaudio_device* pDevice;
};


easyaudio_context* easyaudio_create_context()
{
    easyaudio_context* pContext = NULL;

#ifdef EASYAUDIO_BUILD_DSOUND
    pContext = easyaudio_create_context_dsound();
    if (pContext != NULL) {
        return pContext;
    }
#endif


    // If we get here it means we weren't able to create a context, so return NULL. Later on we're going to have a null implementation so that
    // we don't ever need to return NULL here.
    assert(pContext == NULL);
    return pContext;
}

void easyaudio_delete_context(easyaudio_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    pContext->delete_context(pContext);
}





///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// Playback
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

unsigned int easyaudio_get_playback_device_count(easyaudio_context* pContext)
{
    if (pContext == NULL) {
        return 0;
    }

    return pContext->get_playback_device_count(pContext);
}

bool easyaudio_get_playback_device_info(easyaudio_context* pContext, unsigned int deviceIndex, easyaudio_device_info* pInfoOut)
{
    if (pContext == NULL) {
        return false;
    }

    if (pInfoOut == NULL) {
        return false;
    }

    return pContext->get_playback_device_info(pContext, deviceIndex, pInfoOut);
}


easyaudio_device* easyaudio_create_playback_device(easyaudio_context* pContext, unsigned int deviceIndex)
{
    if (pContext == NULL) {
        return NULL;
    }

    return pContext->create_playback_device(pContext, deviceIndex);
}

void easyaudio_delete_playback_device(easyaudio_device* pDevice)
{
    if (pDevice == NULL) {
        return;
    }

    assert(pDevice->pContext != NULL);
    pDevice->pContext->delete_playback_device(pDevice);
}


easyaudio_buffer* easyaudio_create_buffer(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc)
{
    if (pDevice == NULL) {
        return NULL;
    }

    if (pBufferDesc == NULL) {
        return NULL;
    }

    assert(pDevice->pContext != NULL);
    return pDevice->pContext->create_buffer(pDevice, pBufferDesc);
}

void easyaudio_delete_buffer(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->delete_buffer(pBuffer);
}

void easyaudio_set_buffer_data(easyaudio_buffer* pBuffer, unsigned int offset, const void* pData, unsigned int dataSizeInBytes)
{
    if (pBuffer == NULL) {
        return;
    }

    if (pData == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->set_buffer_data(pBuffer, offset, pData, dataSizeInBytes);
}

void easyaudio_play(easyaudio_buffer* pBuffer, bool loop)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->play(pBuffer, loop);
}




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
// BACKENDS
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// DirectSound
//
///////////////////////////////////////////////////////////////////////////////
#ifdef EASYAUDIO_BUILD_DSOUND
#include <windows.h>
#include <dsound.h>
#include <mmreg.h>
#include <stdio.h>      // For testing and debugging with printf(). Delete this later.

typedef HRESULT (WINAPI * pDirectSoundCreate8Proc)(_In_opt_ LPCGUID pcGuidDevice, _Outptr_ LPDIRECTSOUND8 *ppDS8, _Pre_null_ LPUNKNOWN pUnkOuter);
typedef HRESULT (WINAPI * pDirectSoundEnumerateAProc)(_In_ LPDSENUMCALLBACKA pDSEnumCallback, _In_opt_ LPVOID pContext);

typedef HRESULT (WINAPI * pDirectSoundCaptureCreate8Proc)(_In_opt_ LPCGUID pcGuidDevice, _Outptr_ LPDIRECTSOUNDCAPTURE8 *ppDSC8, _Pre_null_ LPUNKNOWN pUnkOuter);
typedef HRESULT (WINAPI * pDirectSoundCaptureEnumerateAProc)(_In_ LPDSENUMCALLBACKA pDSEnumCallback, _In_opt_ LPVOID pContext);

typedef struct
{
    /// A pointer to the GUID of the device. This will be set to all zeros for the default device.
    GUID guid;

    /// The description of the device.
    char description[256];

    /// The module name of the DirectSound driver corresponding to this device.
    char moduleName[256];

} easyaudio_device_info_dsound;

typedef struct
{
    /// The base context data. This must always be the first item in the struct.
    easyaudio_context base;

    /// A handle to the dsound.dll file that was loaded by LoadLibrary().
    HMODULE hDSoundDLL;

    // DirectSound APIs.
    pDirectSoundCreate8Proc pDirectSoundCreate8;
    pDirectSoundEnumerateAProc pDirectSoundEnumerateA;
    pDirectSoundCaptureCreate8Proc pDirectSoundCaptureCreate8;
    pDirectSoundCaptureEnumerateAProc pDirectSoundCaptureEnumerateA;


    /// The number of playback devices that were iterated when the context was created. This is static, so if the user was to unplug
    /// a device one would need to re-create the context.
    unsigned int playbackDeviceCount;

    /// The buffer containing the list of enumerated playback devices.
    easyaudio_device_info_dsound playbackDeviceInfo[EASYAUDIO_MAX_DEVICE_COUNT];


    /// The number of capture devices that were iterated when the context was created. This is static, so if the user was to unplug
    /// a device one would need to re-create the context.
    unsigned int recordingDeviceCount;

    /// The buffer containing the list of enumerated playback devices.
    easyaudio_device_info_dsound recordingDeviceInfo[EASYAUDIO_MAX_DEVICE_COUNT];


} easyaudio_context_dsound;

typedef struct
{
    /// The base device data. This must always be the first item in the struct.
    easyaudio_device base;

    /// A pointer to the DIRECTSOUND object that was created with DirectSoundCreate8().
    LPDIRECTSOUND8 pDS;

} easyaudio_device_dsound;

typedef struct
{
    /// The base buffer data. This must always be the first item in the struct.
    easyaudio_buffer base;

    /// A pointer to the DirectSound buffer object.
    LPDIRECTSOUNDBUFFER pDSBuffer;

} easyaudio_buffer_dsound;


void easyaudio_delete_context_dsound(easyaudio_context* pContext)
{
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pContext;
    assert(pContextDS != NULL);
    
    FreeLibrary(pContextDS->hDSoundDLL);
    free(pContextDS);
}


unsigned int easyaudio_get_playback_device_count_dsound(easyaudio_context* pContext)
{
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pContext;
    assert(pContextDS != NULL);

    return pContextDS->playbackDeviceCount;
}

bool easyaudio_get_playback_device_info_dsound(easyaudio_context* pContext, unsigned int deviceIndex, easyaudio_device_info* pInfoOut)
{
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pContext;
    assert(pContextDS != NULL);
    assert(pInfoOut != NULL);

    if (deviceIndex >= pContextDS->playbackDeviceCount) {
        return false;
    }


    easyaudio_strcpy(pInfoOut->description, sizeof(pInfoOut->description), pContextDS->playbackDeviceInfo[deviceIndex].description);

    return true;
}


easyaudio_device* easyaudio_create_playback_device_dsound(easyaudio_context* pContext, unsigned int deviceIndex)
{
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pContext;
    assert(pContextDS != NULL);

    if (deviceIndex >= pContextDS->playbackDeviceCount) {
        return NULL;
    }


    LPDIRECTSOUND8 pDS;

    // Create the device.
    HRESULT hr;
    if (deviceIndex == 0) {
        hr = pContextDS->pDirectSoundCreate8(NULL, &pDS, NULL);
    } else {
        hr = pContextDS->pDirectSoundCreate8(&pContextDS->playbackDeviceInfo[deviceIndex].guid, &pDS, NULL);
    }
    
    if (FAILED(hr)) {
        return NULL;
    }


    // Set the cooperative level. Must be done before anything else.
    hr = IDirectSound_SetCooperativeLevel(pDS, GetForegroundWindow(), DSSCL_EXCLUSIVE);
    if (FAILED(hr)) {
        IDirectSound_Release(pDS);
        return NULL;
    }


    easyaudio_device_dsound* pDeviceDS = malloc(sizeof(easyaudio_device_dsound));
    if (pDeviceDS != NULL)
    {
        pDeviceDS->base.pContext = pContext;
        pDeviceDS->pDS = pDS;

        return (easyaudio_device*)pDeviceDS;
    }
    else
    {
        IDirectSound_Release(pDS);
        return NULL;
    }
}

void easyaudio_delete_playback_device_dsound(easyaudio_device* pDevice)
{
    easyaudio_device_dsound* pDeviceDS = (easyaudio_device_dsound*)pDevice;
    assert(pDeviceDS != NULL);

    IDirectSound_Release(pDeviceDS->pDS);
    free(pDeviceDS);
}


easyaudio_buffer* easyaudio_create_buffer_dsound(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc)
{
    easyaudio_device_dsound* pDeviceDS = (easyaudio_device_dsound*)pDevice;
    assert(pDeviceDS != NULL);
    assert(pBufferDesc != NULL);

    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.cbSize          = sizeof(wf);
    wf.nChannels       = pBufferDesc->channels;
    wf.nSamplesPerSec  = pBufferDesc->sampleRate;
    wf.wBitsPerSample  = pBufferDesc->bitsPerSample;
    wf.nAvgBytesPerSec = 4 * pBufferDesc->sampleRate;
    wf.nBlockAlign     = (wf.nChannels * wf.wBitsPerSample) / 8;

    if (pBufferDesc->format == easyaudio_format_pcm) {
        wf.wFormatTag = WAVE_FORMAT_PCM;
    } else if (pBufferDesc->format == easyaudio_format_float) {
        wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    } else {
        return NULL;
    }


    DSBUFFERDESC descDS;
    memset(&descDS, 0, sizeof(DSBUFFERDESC)); 
    descDS.dwSize        = sizeof(DSBUFFERDESC); 
    descDS.dwFlags       = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2;
    descDS.dwBufferBytes = pBufferDesc->sizeInBytes;
    descDS.lpwfxFormat   = &wf;

    LPDIRECTSOUNDBUFFER pDSBuffer;
    HRESULT hr = IDirectSound_CreateSoundBuffer(pDeviceDS->pDS, &descDS, &pDSBuffer, NULL);
    if (FAILED(hr)) {
        return NULL;
    }


    easyaudio_buffer_dsound* pBufferDS = malloc(sizeof(easyaudio_buffer_dsound));
    if (pBufferDS == NULL) {
        IDirectSoundBuffer_Release(pDSBuffer);
        return NULL;
    }

    pBufferDS->base.pDevice = pDevice;
    pBufferDS->pDSBuffer    = pDSBuffer;

    if (pBufferDesc->pInitialData != NULL) {
        easyaudio_set_buffer_data((easyaudio_buffer*)pBufferDS, 0, pBufferDesc->pInitialData, pBufferDesc->sizeInBytes);
    }

    return (easyaudio_buffer*)pBufferDS;
}

void easyaudio_delete_buffer_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    IDirectSoundBuffer_Release(pBufferDS->pDSBuffer);
    free(pBufferDS);
}

bool easyaudio_set_buffer_data_dsound(easyaudio_buffer* pBuffer, unsigned int offset, const void* pData, unsigned int dataSizeInBytes)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    assert(pData != NULL);

    LPVOID lpvWrite;
    DWORD dwLength;
    HRESULT hr = IDirectSoundBuffer_Lock(pBufferDS->pDSBuffer, offset, dataSizeInBytes, &lpvWrite, &dwLength, NULL, NULL, DSBLOCK_ENTIREBUFFER);
    if (FAILED(hr)) {
        return false;
    }

    assert(dwLength == dataSizeInBytes);
    memcpy(lpvWrite, pData, dataSizeInBytes);

    hr = IDirectSoundBuffer_Unlock(pBufferDS->pDSBuffer, lpvWrite, dwLength, NULL, 0);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

void easyaudio_play_dsound(easyaudio_buffer* pBuffer, bool loop)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    DWORD dwFlags = 0;
    if (loop) {
        dwFlags |= DSBPLAY_LOOPING;
    }

    IDirectSoundBuffer_Play(pBufferDS->pDSBuffer, 0, 0, dwFlags);
}


static BOOL CALLBACK DSEnumCallback_PlaybackDevices(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext)
{
    // From MSDN:
    //
    // The first device enumerated is always called the Primary Sound Driver, and the lpGUID parameter of the callback is
    // NULL. This device represents the preferred playback device set by the user in Control Panel.

    easyaudio_context_dsound* pContextDS = lpContext;
    assert(pContextDS != NULL);

    if (pContextDS->playbackDeviceCount < EASYAUDIO_MAX_DEVICE_COUNT)
    {
        if (lpGuid != NULL) {
            memcpy(&pContextDS->playbackDeviceInfo[pContextDS->playbackDeviceCount].guid, lpGuid, sizeof(GUID));
        } else {
            memset(&pContextDS->playbackDeviceInfo[pContextDS->playbackDeviceCount].guid, 0, sizeof(GUID));
        }
        
        easyaudio_strcpy(pContextDS->playbackDeviceInfo[pContextDS->playbackDeviceCount].description, 256, lpcstrDescription);
        easyaudio_strcpy(pContextDS->playbackDeviceInfo[pContextDS->playbackDeviceCount].moduleName,  256, lpcstrModule);

        //printf("Playback Device: (%d) %s %s\n", pContextDS->playbackDeviceCount, lpcstrDescription, lpcstrModule);

        pContextDS->playbackDeviceCount += 1;
        return TRUE;
    }
    else
    {
        // Ran out of device slots.
        return FALSE;
    }
}

static BOOL CALLBACK DSEnumCallback_RecordingDevices(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext)
{
    // From MSDN:
    //
    // The first device enumerated is always called the Primary Sound Driver, and the lpGUID parameter of the callback is
    // NULL. This device represents the preferred playback device set by the user in Control Panel.

    easyaudio_context_dsound* pContextDS = lpContext;
    assert(pContextDS != NULL);

    if (pContextDS->recordingDeviceCount < EASYAUDIO_MAX_DEVICE_COUNT)
    {
        if (lpGuid != NULL) {
            memcpy(&pContextDS->recordingDeviceInfo[pContextDS->recordingDeviceCount].guid, lpGuid, sizeof(GUID));
        } else {
            memset(&pContextDS->recordingDeviceInfo[pContextDS->recordingDeviceCount].guid, 0, sizeof(GUID));
        }

        easyaudio_strcpy(pContextDS->recordingDeviceInfo[pContextDS->recordingDeviceCount].description, 256, lpcstrDescription);
        easyaudio_strcpy(pContextDS->recordingDeviceInfo[pContextDS->recordingDeviceCount].moduleName,  256, lpcstrModule);

        pContextDS->recordingDeviceCount += 1;
        return TRUE;
    }
    else
    {
        // Ran out of device slots.
        return FALSE;
    }
}

easyaudio_context* easyaudio_create_context_dsound()
{
    // Load the DLL.
    HMODULE hDSoundDLL = LoadLibraryW(L"dsound.dll");
    if (FAILED(hDSoundDLL)) {
        return NULL;
    }


    // Retrieve the APIs.
    pDirectSoundCreate8Proc pDirectSoundCreate8 = (pDirectSoundCreate8Proc)GetProcAddress(hDSoundDLL, "DirectSoundCreate8");
    if (pDirectSoundCreate8 == NULL ){
        FreeLibrary(hDSoundDLL);
        return NULL;
    }

    pDirectSoundEnumerateAProc pDirectSoundEnumerateA = (pDirectSoundEnumerateAProc)GetProcAddress(hDSoundDLL, "DirectSoundEnumerateA");
    if (pDirectSoundEnumerateA == NULL ){
        FreeLibrary(hDSoundDLL);
        return NULL;
    }

    pDirectSoundCaptureCreate8Proc pDirectSoundCaptureCreate8 = (pDirectSoundCaptureCreate8Proc)GetProcAddress(hDSoundDLL, "DirectSoundCaptureCreate8");
    if (pDirectSoundCaptureCreate8 == NULL) {
        FreeLibrary(hDSoundDLL);
        return NULL;
    }

    pDirectSoundCaptureEnumerateAProc pDirectSoundCaptureEnumerateA = (pDirectSoundCaptureEnumerateAProc)GetProcAddress(hDSoundDLL, "DirectSoundCaptureEnumerateA");
    if (pDirectSoundCaptureEnumerateA == NULL ){
        FreeLibrary(hDSoundDLL);
        return NULL;
    }



    // At this point we can almost certainly assume DirectSound is usable so we'll now go ahead and create the context.
    easyaudio_context_dsound* pContext = malloc(sizeof(easyaudio_context_dsound));
    if (pContext != NULL)
    {
        pContext->base.delete_context            = easyaudio_delete_context_dsound;
        pContext->base.create_playback_device    = easyaudio_create_playback_device_dsound;
        pContext->base.delete_playback_device    = easyaudio_delete_playback_device_dsound;
        pContext->base.get_playback_device_count = easyaudio_get_playback_device_count_dsound;
        pContext->base.get_playback_device_info  = easyaudio_get_playback_device_info_dsound;
        pContext->base.create_buffer             = easyaudio_create_buffer_dsound;
        pContext->base.delete_buffer             = easyaudio_delete_buffer_dsound;
        pContext->base.set_buffer_data           = easyaudio_set_buffer_data_dsound;
        pContext->base.play                      = easyaudio_play_dsound;

        pContext->hDSoundDLL                     = hDSoundDLL;
        pContext->pDirectSoundCreate8            = pDirectSoundCreate8;
        pContext->pDirectSoundEnumerateA         = pDirectSoundEnumerateA;
        pContext->pDirectSoundCaptureCreate8     = pDirectSoundCaptureCreate8;
        pContext->pDirectSoundCaptureEnumerateA  = pDirectSoundCaptureEnumerateA;

        // Enumerate playback devices.
        pContext->playbackDeviceCount = 0;
        pContext->pDirectSoundEnumerateA(DSEnumCallback_PlaybackDevices, pContext);

        // Enumerate recording devices.
        pContext->recordingDeviceCount = 0;
        pContext->pDirectSoundCaptureEnumerateA(DSEnumCallback_RecordingDevices, pContext);
    }

    return (easyaudio_context*)pContext;
}
#endif  // !EASYAUDIO_BUILD_DSOUND



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// TESTING
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#if 1
#include <stdio.h>

#define STB_VORBIS_HEADER_ONLY
#include "../easy_ge/source/external/stb_vorbis.c"

void easyaudio_dsound_test1()
{
    easyaudio_context* pContext = easyaudio_create_context_dsound();
    if (pContext == NULL) {
        return;
    }

    easyaudio_device* pPlaybackDevice = easyaudio_create_playback_device(pContext, 0);
    if (pPlaybackDevice == NULL) {
        printf("Failed to create DirectSound playback device\n");
        easyaudio_delete_context(pContext);
        return;
    }



    int channels;
    int sampleRate;
    short* pData;
    int samplesDecoded = stb_vorbis_decode_filename("Track09.ogg", &channels, &sampleRate, &pData);
    if (samplesDecoded == -1) {
        printf("Failed to load sound file\n");
        return;
    }


    easyaudio_buffer_desc buffer;
    buffer.format        = easyaudio_format_pcm;
    buffer.channels      = channels;
    buffer.sampleRate    = sampleRate;
    buffer.bitsPerSample = sizeof(short) * 8;
    buffer.sizeInBytes   = samplesDecoded * channels * sizeof(short);
    buffer.pInitialData  = pData;

    easyaudio_buffer* pBuffer = easyaudio_create_buffer(pPlaybackDevice, &buffer);
    if (pBuffer == NULL) {
        printf("Failed to create DirectSound buffer\n");
        return;
    }

    easyaudio_play(pBuffer, true);  // "true" means to loop.
}
#endif  // TESTING