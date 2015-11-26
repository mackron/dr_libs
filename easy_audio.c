// Public domain. See "unlicense" statement at the end of this file.

#include "easy_audio.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


// Annotations
#define PRIVATE


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


typedef void                     (* easyaudio_delete_context_proc)(easyaudio_context* pContext);
typedef easyaudio_device*        (* easyaudio_create_output_device_proc)(easyaudio_context* pContext, unsigned int deviceIndex);
typedef void                     (* easyaudio_delete_output_device_proc)(easyaudio_device* pDevice);
typedef unsigned int             (* easyaudio_get_output_device_count_proc)(easyaudio_context* pContext);
typedef bool                     (* easyaudio_get_output_device_info_proc)(easyaudio_context* pContext, unsigned int deviceIndex, easyaudio_device_info* pInfoOut);
typedef easyaudio_buffer*        (* easyaudio_create_buffer_proc)(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc, unsigned int extraDataSize);
typedef void                     (* easyaudio_delete_buffer_proc)(easyaudio_buffer* pBuffer);
typedef unsigned int             (* easyaudio_get_buffer_extra_data_size_proc)(easyaudio_buffer* pBuffer);
typedef void*                    (* easyaudio_get_buffer_extra_data_proc)(easyaudio_buffer* pBuffer);
typedef void                     (* easyaudio_set_buffer_data_proc)(easyaudio_buffer* pBuffer, unsigned int offset, const void* pData, unsigned int dataSizeInBytes);
typedef void                     (* easyaudio_play_proc)(easyaudio_buffer* pBuffer, bool loop);
typedef void                     (* easyaudio_pause_proc)(easyaudio_buffer* pBuffer);
typedef void                     (* easyaudio_stop_proc)(easyaudio_buffer* pBuffer);
typedef easyaudio_playback_state (* easyaudio_get_playback_state_proc)(easyaudio_buffer* pBuffer);
typedef void                     (* easyaudio_set_playback_position_proc)(easyaudio_buffer* pBuffer, unsigned int position);
typedef unsigned int             (* easyaudio_get_playback_position_proc)(easyaudio_buffer* pBuffer);
typedef void                     (* easyaudio_set_pan_proc)(easyaudio_buffer* pBuffer, float pan);
typedef float                    (* easyaudio_get_pan_proc)(easyaudio_buffer* pBuffer);
typedef void                     (* easyaudio_set_volume_proc)(easyaudio_buffer* pBuffer, float volume);
typedef float                    (* easyaudio_get_volume_proc)(easyaudio_buffer* pBuffer);
typedef void                     (* easyaudio_remove_markers_proc)(easyaudio_buffer* pBuffer);
typedef bool                     (* easyaudio_register_marker_callback_proc)(easyaudio_buffer* pBuffer, unsigned int offsetInBytes, easyaudio_event_callback_proc callback, unsigned int eventID, void* pUserData);
typedef bool                     (* easyaudio_register_stop_callback_proc)(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);
typedef bool                     (* easyaudio_register_pause_callback_proc)(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);
typedef bool                     (* easyaudio_register_play_callback_proc)(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);
typedef void                     (* easyaudio_set_position_proc)(easyaudio_buffer* pBuffer, float x, float y, float z);
typedef void                     (* easyaudio_get_position_proc)(easyaudio_buffer* pBuffer, float* pPosOut);
typedef void                     (* easyaudio_set_listener_position_proc)(easyaudio_device* pDevice, float x, float y, float z);
typedef void                     (* easyaudio_get_listener_position_proc)(easyaudio_device* pDevice, float* pPosOut);
typedef void                     (* easyaudio_set_listener_orientation_proc)(easyaudio_device* pDevice, float forwardX, float forwardY, float forwardZ, float upX, float upY, float upZ);
typedef void                     (* easyaudio_get_listener_orientation_proc)(easyaudio_device* pDevice, float* pForwardOut, float* pUpOut);
typedef void                     (* easyaudio_set_3d_mode_proc)(easyaudio_buffer* pBuffer, easyaudio_3d_mode mode);
typedef easyaudio_3d_mode        (* easyaudio_get_3d_mode_proc)(easyaudio_buffer* pBuffer);

struct easyaudio_context
{
    // Callbacks.
    easyaudio_delete_context_proc delete_context;
    easyaudio_create_output_device_proc create_output_device;
    easyaudio_delete_output_device_proc delete_output_device;
    easyaudio_get_output_device_count_proc get_output_device_count;
    easyaudio_get_output_device_info_proc get_output_device_info;
    easyaudio_create_buffer_proc create_buffer;
    easyaudio_delete_buffer_proc delete_buffer;
    easyaudio_get_buffer_extra_data_size_proc get_buffer_extra_data_size;
    easyaudio_get_buffer_extra_data_proc get_buffer_extra_data;
    easyaudio_set_buffer_data_proc set_buffer_data;
    easyaudio_play_proc play;
    easyaudio_pause_proc pause;
    easyaudio_stop_proc stop;
    easyaudio_get_playback_state_proc get_playback_state;
    easyaudio_set_playback_position_proc set_playback_position;
    easyaudio_get_playback_position_proc get_playback_position;
    easyaudio_set_pan_proc set_pan;
    easyaudio_get_pan_proc get_pan;
    easyaudio_set_volume_proc set_volume;
    easyaudio_get_volume_proc get_volume;
    easyaudio_remove_markers_proc remove_markers;
    easyaudio_register_marker_callback_proc register_marker_callback;
    easyaudio_register_stop_callback_proc register_stop_callback;
    easyaudio_register_pause_callback_proc register_pause_callback;
    easyaudio_register_play_callback_proc register_play_callback;
    easyaudio_set_position_proc set_position;
    easyaudio_get_position_proc get_position;
    easyaudio_set_listener_position_proc set_listener_position;
    easyaudio_get_listener_position_proc get_listener_position;
    easyaudio_set_listener_orientation_proc set_listener_orientation;
    easyaudio_get_listener_orientation_proc get_listener_orientation;
    easyaudio_set_3d_mode_proc set_3d_mode;
    easyaudio_get_3d_mode_proc get_3d_mode;
};

struct easyaudio_device
{
    /// The context that owns this device.
    easyaudio_context* pContext;

    /// Whether or not the device is marked for deletion. A device is not always deleted straight away, so we use this
    /// to determine whether or not it has been marked for deletion.
    bool markedForDeletion;
};

struct easyaudio_buffer
{
    /// The device that owns this buffer.
    easyaudio_device* pDevice;

    /// The stop callback.
    easyaudio_event_callback stopCallback;

    /// The pause callback.
    easyaudio_event_callback pauseCallback;

    /// The play callback.
    easyaudio_event_callback playCallback;

    /// Whether or not playback is looping.
    bool isLooping;

    /// Whether or not the buffer is marked for deletion. A buffer is not always deleted straight away, so we use this
    /// to determine whether or not it has been marked for deletion.
    bool markedForDeletion;
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
// OUTPUT
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

unsigned int easyaudio_get_output_device_count(easyaudio_context* pContext)
{
    if (pContext == NULL) {
        return 0;
    }

    return pContext->get_output_device_count(pContext);
}

bool easyaudio_get_output_device_info(easyaudio_context* pContext, unsigned int deviceIndex, easyaudio_device_info* pInfoOut)
{
    if (pContext == NULL) {
        return false;
    }

    if (pInfoOut == NULL) {
        return false;
    }

    return pContext->get_output_device_info(pContext, deviceIndex, pInfoOut);
}


easyaudio_device* easyaudio_create_output_device(easyaudio_context* pContext, unsigned int deviceIndex)
{
    if (pContext == NULL) {
        return NULL;
    }

    easyaudio_device* pDevice = pContext->create_output_device(pContext, deviceIndex);
    if (pDevice != NULL)
    {
        pDevice->markedForDeletion = false;
    }

    return pDevice;
}

void easyaudio_delete_output_device(easyaudio_device* pDevice)
{
    if (pDevice == NULL) {
        return;
    }

    // If the device is already marked for deletion we just return straight away. However, this is an erroneous case so we trigger a failed assertion in this case.
    if (pDevice->markedForDeletion) {
        assert(false);
        return;
    }

    pDevice->markedForDeletion = true;

    assert(pDevice->pContext != NULL);
    pDevice->pContext->delete_output_device(pDevice);
}


easyaudio_buffer* easyaudio_create_buffer(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc, unsigned int extraDataSize)
{
    if (pDevice == NULL) {
        return NULL;
    }

    if (pBufferDesc == NULL) {
        return NULL;
    }

    assert(pDevice->pContext != NULL);

    easyaudio_buffer* pBuffer = pDevice->pContext->create_buffer(pDevice, pBufferDesc, extraDataSize);
    if (pBuffer != NULL)
    {
        pBuffer->pDevice           = pDevice;
        pBuffer->stopCallback      = (easyaudio_event_callback){NULL, NULL};
        pBuffer->pauseCallback     = (easyaudio_event_callback){NULL, NULL};
        pBuffer->playCallback      = (easyaudio_event_callback){NULL, NULL};
        pBuffer->isLooping         = false;
        pBuffer->markedForDeletion = false;
    }

    return pBuffer;
}

void easyaudio_delete_buffer(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return;
    }

    // We don't want to do anything if the buffer is marked for deletion.
    if (pBuffer->markedForDeletion) {
        assert(false);
        return;
    }

    pBuffer->markedForDeletion = true;


    // The sound needs to be stopped first.
    easyaudio_stop(pBuffer);

    // Now we need to remove every event.
    easyaudio_remove_markers(pBuffer);
    easyaudio_register_stop_callback(pBuffer, NULL, NULL);
    easyaudio_register_pause_callback(pBuffer, NULL, NULL);
    easyaudio_register_play_callback(pBuffer, NULL, NULL);


    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->delete_buffer(pBuffer);
}


unsigned int easyaudio_get_buffer_extra_data_size(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return 0;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->get_buffer_extra_data_size(pBuffer);
}

void* easyaudio_get_buffer_extra_data(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return NULL;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->get_buffer_extra_data(pBuffer);
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

    pBuffer->isLooping = loop;

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->play(pBuffer, loop);
}

void easyaudio_pause(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->pause(pBuffer);
}

void easyaudio_stop(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->stop(pBuffer);
}

easyaudio_playback_state easyaudio_get_playback_state(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return easyaudio_stopped;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->get_playback_state(pBuffer);
}

bool easyaudio_is_looping(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return false;
    }

    return pBuffer->isLooping;
}


void easyaudio_set_playback_position(easyaudio_buffer* pBuffer, unsigned int position)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->set_playback_position(pBuffer, position);
}

unsigned int easyaudio_get_playback_position(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return 0;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->get_playback_position(pBuffer);
}


void easyaudio_set_pan(easyaudio_buffer* pBuffer, float pan)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->set_pan(pBuffer, pan);
}

float easyaudio_get_pan(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return 0.0f;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->get_pan(pBuffer);
}


void easyaudio_set_volume(easyaudio_buffer* pBuffer, float volume)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->set_volume(pBuffer, volume);
}

float easyaudio_get_volume(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return 1.0f;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->get_volume(pBuffer);
}


void easyaudio_remove_markers(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->remove_markers(pBuffer);
}

bool easyaudio_register_marker_callback(easyaudio_buffer* pBuffer, unsigned int offsetInBytes, easyaudio_event_callback_proc callback, unsigned int eventID, void* pUserData)
{
    if (pBuffer == NULL) {
        return false;
    }

    if (eventID == EASYAUDIO_EVENT_ID_STOP ||
        eventID == EASYAUDIO_EVENT_ID_PAUSE ||
        eventID == EASYAUDIO_EVENT_ID_PLAY)
    {
        return false;
    }

    if (easyaudio_get_playback_state(pBuffer) != easyaudio_stopped) {
        return false;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->register_marker_callback(pBuffer, offsetInBytes, callback, eventID, pUserData);
}

bool easyaudio_register_stop_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    if (pBuffer == NULL) {
        return false;
    }

    if (callback != NULL && easyaudio_get_playback_state(pBuffer) != easyaudio_stopped) {
        return false;
    }

    pBuffer->stopCallback.callback  = callback;
    pBuffer->stopCallback.pUserData = pUserData;

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->register_stop_callback(pBuffer, callback, pUserData);
}

bool easyaudio_register_pause_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    if (pBuffer == NULL) {
        return false;
    }

    if (callback != NULL && easyaudio_get_playback_state(pBuffer) != easyaudio_stopped) {
        return false;
    }

    pBuffer->pauseCallback.callback  = callback;
    pBuffer->pauseCallback.pUserData = pUserData;

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->register_pause_callback(pBuffer, callback, pUserData);
}

bool easyaudio_register_play_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    if (pBuffer == NULL) {
        return false;
    }

    if (callback != NULL && easyaudio_get_playback_state(pBuffer) != easyaudio_stopped) {
        return false;
    }

    pBuffer->playCallback.callback  = callback;
    pBuffer->playCallback.pUserData = pUserData;

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->register_play_callback(pBuffer, callback, pUserData);
}


easyaudio_event_callback easyaudio_get_stop_callback(easyaudio_buffer* pBuffer)
{
    if (pBuffer != NULL) {
        return pBuffer->stopCallback;
    } else {
        return (easyaudio_event_callback){NULL, NULL};
    }
}

easyaudio_event_callback easyaudio_get_pause_callback(easyaudio_buffer* pBuffer)
{
    if (pBuffer != NULL) {
        return pBuffer->pauseCallback;
    } else {
        return (easyaudio_event_callback){NULL, NULL};
    }
}

easyaudio_event_callback easyaudio_get_play_callback(easyaudio_buffer* pBuffer)
{
    if (pBuffer != NULL) {
        return pBuffer->playCallback;
    } else {
        return (easyaudio_event_callback){NULL, NULL};
    }
}


void easyaudio_set_position(easyaudio_buffer* pBuffer, float x, float y, float z)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->set_position(pBuffer, x, y, z);
}

void easyaudio_get_position(easyaudio_buffer* pBuffer, float* pPosOut)
{
    if (pBuffer == NULL) {
        return;
    }

    if (pPosOut == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->get_position(pBuffer, pPosOut);
}


void easyaudio_set_listener_position(easyaudio_device* pDevice, float x, float y, float z)
{
    if (pDevice == NULL) {
        return;
    }

    pDevice->pContext->set_listener_position(pDevice, x, y, z);
}

void easyaudio_get_listener_position(easyaudio_device* pDevice, float* pPosOut)
{
    if (pDevice == NULL || pPosOut == NULL) {
        return;
    }

    pDevice->pContext->get_listener_position(pDevice, pPosOut);
}

void easyaudio_set_listener_orientation(easyaudio_device* pDevice, float forwardX, float forwardY, float forwardZ, float upX, float upY, float upZ)
{
    if (pDevice == NULL) {
        return;
    }

    pDevice->pContext->set_listener_orientation(pDevice, forwardX, forwardY, forwardZ, upX, upY, upZ);
}

void easyaudio_get_listener_orientation(easyaudio_device* pDevice, float* pForwardOut, float* pUpOut)
{
    if (pDevice == NULL) {
        return;
    }

    pDevice->pContext->get_listener_orientation(pDevice, pForwardOut, pUpOut);
}


void easyaudio_set_3d_mode(easyaudio_buffer* pBuffer, easyaudio_3d_mode mode)
{
    if (pBuffer == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->set_3d_mode(pBuffer, mode);
}

easyaudio_3d_mode easyaudio_get_3d_mode(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return easyaudio_3d_mode_disabled;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->get_3d_mode(pBuffer);
}




///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// INPUT
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

#if defined(_WIN32)
#include <windows.h>

easyaudio_mutex easyaudio_create_mutex()
{
    easyaudio_mutex mutex = malloc(sizeof(CRITICAL_SECTION));
    if (mutex != NULL)
    {
        InitializeCriticalSection(mutex);
    }

    return mutex;
}

void easyaudio_delete_mutex(easyaudio_mutex mutex)
{
    DeleteCriticalSection(mutex);
    free(mutex);
}

void easyaudio_lock_mutex(easyaudio_mutex mutex)
{
    EnterCriticalSection(mutex);
}

void easyaudio_unlock_mutex(easyaudio_mutex mutex)
{
    LeaveCriticalSection(mutex);
}
#endif



//// STREAMING ////

#define EASYAUDIO_STREAMING_MARKER_0    EASYAUDIO_EVENT_ID_MARKER + 0
#define EASYAUDIO_STREAMING_MARKER_1    EASYAUDIO_EVENT_ID_MARKER + 1

#define EASYAUDIO_STREAMING_CHUNK_INVALID   0

typedef struct
{
    /// The steaming buffer callbacks.
    easyaudio_streaming_callbacks callbacks;

    /// Keeps track of whether or not we are at the start of the playback.
    bool atStart;

    /// Keeps track of whether or not we should stop at the end of the next chunk.
    bool stopAtEndOfCurrentChunk;

    /// Keeps track of whether or not the sound should loop.
    bool isLoopingEnabled;

    /// The size of the extra data.
    unsigned int extraDataSize;

    /// The size of an individual chunk. A chunk is half the size of the buffer.
    unsigned int chunkSize;

    /// A pointer to the temporary buffer for loading chunk data.
    unsigned char pTempChunkData[1];

} ea_streaming_buffer_data;


bool ea_streaming_buffer_load_next_chunk(easyaudio_buffer* pBuffer, ea_streaming_buffer_data* pStreamingData, unsigned int offset, unsigned int chunkSize)
{
    assert(pStreamingData != NULL);
    assert(pStreamingData->callbacks.read != NULL);
    assert(pStreamingData->callbacks.seek != NULL);
    assert(pStreamingData->chunkSize >= chunkSize);

    // A chunk size of 0 is valid, but we just return immediately.
    if (chunkSize == 0) {
        return true;
    }

    unsigned int bytesRead;
    if (!pStreamingData->callbacks.read(pStreamingData->callbacks.pUserData, pStreamingData->pTempChunkData, chunkSize, &bytesRead))
    {
        // There was an error reading the data. We might have run out of data.
        return false;
    }


    pStreamingData->stopAtEndOfCurrentChunk = false;

    easyaudio_set_buffer_data(pBuffer, offset, pStreamingData->pTempChunkData, bytesRead);

    if (chunkSize > bytesRead)
    {
        // The number of bytes read is less than our chunk size. This is our cue that we've reached the end of the steam. If we're looping, we
        // just seek back to the start and read more data. There is a chance the data total size of the streaming data is smaller than our
        // chunk, so we actually want to do this recursively.
        //
        // If we're not looping, we fill the remaining data with silence.
        if (pStreamingData->isLoopingEnabled)
        {
            pStreamingData->callbacks.seek(pStreamingData->callbacks.pUserData, 0);
            return ea_streaming_buffer_load_next_chunk(pBuffer, pStreamingData, offset + bytesRead, chunkSize - bytesRead);
        }
        else
        {
            memset(pStreamingData->pTempChunkData + bytesRead, 0, chunkSize - bytesRead);
            easyaudio_set_buffer_data(pBuffer, offset + bytesRead, pStreamingData->pTempChunkData + bytesRead, chunkSize - bytesRead);

            pStreamingData->stopAtEndOfCurrentChunk = true;
        }
    }

    return true;
}

void ea_steaming_buffer_marker_callback(easyaudio_buffer* pBuffer, unsigned int eventID, void *pUserData)
{
    ea_streaming_buffer_data* pStreamingData = pUserData;
    assert(pStreamingData != NULL);
    
    unsigned int offset = 0;
    if (eventID == EASYAUDIO_STREAMING_MARKER_0) {
        offset = pStreamingData->chunkSize;
    }

    if (pStreamingData->stopAtEndOfCurrentChunk)
    {
        if (!pStreamingData->atStart) {
            easyaudio_stop(pBuffer);
        }
    }
    else
    {
        ea_streaming_buffer_load_next_chunk(pBuffer, pStreamingData, offset, pStreamingData->chunkSize);
    }

    pStreamingData->atStart = false;
}


easyaudio_buffer* easyaudio_create_streaming_buffer(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc, easyaudio_streaming_callbacks callbacks, unsigned int extraDataSize)
{
    if (callbacks.read == NULL) {
        return NULL;
    }

    if (pBufferDesc == NULL) {
        return NULL;
    }


    // We are determining for ourselves what the size of the buffer should be. We need to create our own copy rather than modify the input descriptor.
    easyaudio_buffer_desc bufferDesc = *pBufferDesc;
    bufferDesc.sizeInBytes  = pBufferDesc->sampleRate * pBufferDesc->channels * (pBufferDesc->bitsPerSample / 8);
    bufferDesc.pInitialData = NULL;

    unsigned int chunkSize = bufferDesc.sizeInBytes / 2;

    easyaudio_buffer* pBuffer = easyaudio_create_buffer(pDevice, &bufferDesc, sizeof(ea_streaming_buffer_data) - sizeof(unsigned char) + chunkSize + extraDataSize);
    if (pBuffer == NULL) {
        return NULL;
    }


    ea_streaming_buffer_data* pStreamingData = easyaudio_get_buffer_extra_data(pBuffer);
    assert(pStreamingData != NULL);

    pStreamingData->callbacks               = callbacks;
    pStreamingData->atStart                 = true;
    pStreamingData->stopAtEndOfCurrentChunk = false;
    pStreamingData->isLoopingEnabled        = false;
    pStreamingData->chunkSize               = chunkSize;

    // Register two markers - one for the first half and another for the second half. When a half is finished playing we need to 
    // replace it with new data.
    easyaudio_register_marker_callback(pBuffer, 0,         ea_steaming_buffer_marker_callback, EASYAUDIO_STREAMING_MARKER_0, pStreamingData);
    easyaudio_register_marker_callback(pBuffer, chunkSize, ea_steaming_buffer_marker_callback, EASYAUDIO_STREAMING_MARKER_1, pStreamingData);


    return pBuffer;
}


unsigned int easyaudio_get_streaming_buffer_extra_data_size(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return 0;
    }

    ea_streaming_buffer_data* pStreamingData = easyaudio_get_buffer_extra_data(pBuffer);
    assert(pStreamingData != NULL);

    return pStreamingData->extraDataSize;
}

void* easyaudio_get_streaming_buffer_extra_data(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return NULL;
    }

    ea_streaming_buffer_data* pStreamingData = easyaudio_get_buffer_extra_data(pBuffer);
    assert(pStreamingData != NULL);

    return ((char*)pStreamingData->pTempChunkData) + pStreamingData->chunkSize;
}


bool easyaudio_play_streaming_buffer(easyaudio_buffer* pBuffer, bool loop)
{
    if (pBuffer == NULL) {
        return false;
    }


    ea_streaming_buffer_data* pStreamingData = easyaudio_get_buffer_extra_data(pBuffer);
    assert(pStreamingData != NULL);

    // If the buffer was previously in a paused state, we just play like normal. If it was in a stopped state we need to start from the beginning.
    if (easyaudio_get_playback_state(pBuffer) == easyaudio_stopped)
    {
        // We need to load some initial data into the first chunk.
        pStreamingData->atStart = true;
        pStreamingData->callbacks.seek(pStreamingData->callbacks.pUserData, 0);

        if (!ea_streaming_buffer_load_next_chunk(pBuffer, pStreamingData, 0, pStreamingData->chunkSize))
        {
            // There was an error loading the initial data.
            return false;
        }
    }


    pStreamingData->isLoopingEnabled = loop;
    easyaudio_play(pBuffer, true);      // <-- Always loop on a streaming buffer. Actual looping is done a bit differently for streaming buffers.

    return true;
}

bool easyaudio_is_streaming_buffer_looping(easyaudio_buffer* pBuffer)
{
    if (pBuffer == NULL) {
        return false;
    }

    ea_streaming_buffer_data* pStreamingData = easyaudio_get_buffer_extra_data(pBuffer);
    assert(pStreamingData != NULL);

    return pStreamingData->isLoopingEnabled;
}



///////////////////////////////////////////////////////////////////////////////
//
// Sound World
//
///////////////////////////////////////////////////////////////////////////////

PRIVATE static easyaudio_bool easyaudio_on_sound_read_callback(void* pUserData, void* pDataOut, unsigned int bytesToRead, unsigned int* bytesReadOut)
{
    easyaudio_sound* pSound = pUserData;
    assert(pSound != NULL);
    assert(pSound->onRead != NULL);
    
    return pSound->onRead(pSound, pDataOut, bytesToRead, bytesReadOut);
}

PRIVATE static easyaudio_bool easyaudio_on_sound_seek_callback(void* pUserData, unsigned int offsetInBytesFromStart)
{
    easyaudio_sound* pSound = pUserData;
    assert(pSound != NULL);
    assert(pSound->onRead != NULL);
    
    return pSound->onSeek(pSound, offsetInBytesFromStart);
}


PRIVATE static void easyaudio_inline_sound_stop_callback(easyaudio_buffer* pBuffer, unsigned int eventID, void *pUserData)
{
    (void)eventID;

    assert(pBuffer != NULL);
    assert(eventID == EASYAUDIO_EVENT_ID_STOP);
    assert(pUserData != NULL);

    easyaudio_sound* pSound = pUserData;
    easyaudio_delete_sound(pSound);
}


PRIVATE void easyaudio_prepend_sound(easyaudio_sound* pSound)
{
    assert(pSound != NULL);
    assert(pSound->pWorld != NULL);
    assert(pSound->pPrevSound == NULL);

    easyaudio_lock_mutex(pSound->pWorld->lock);
    {
        pSound->pNextSound = pSound->pWorld->pFirstSound;

        if (pSound->pNextSound != NULL) {
            pSound->pNextSound->pPrevSound = pSound;
        }

        pSound->pWorld->pFirstSound = pSound;
    }
    easyaudio_unlock_mutex(pSound->pWorld->lock);
}

PRIVATE void easyaudio_remove_sound(easyaudio_sound* pSound)
{
    assert(pSound != NULL);
    assert(pSound->pWorld != NULL);

    easyaudio_lock_mutex(pSound->pWorld->lock);
    {
        if (pSound == pSound->pWorld->pFirstSound) {
            pSound->pWorld->pFirstSound = pSound->pNextSound;
        }

        if (pSound->pNextSound != NULL) {
            pSound->pNextSound->pPrevSound = pSound->pPrevSound;
        }
    
        if (pSound->pPrevSound != NULL) {
            pSound->pPrevSound->pNextSound = pSound->pNextSound;
        }
    }
    easyaudio_unlock_mutex(pSound->pWorld->lock);
}


PRIVATE bool easyaudio_is_inline_sound(easyaudio_sound* pSound)
{
    assert(pSound != NULL);
    return easyaudio_get_stop_callback(pSound->pBuffer).callback == easyaudio_inline_sound_stop_callback;
}


easyaudio_world* easyaudio_create_world(easyaudio_device* pDevice)
{
    easyaudio_world* pWorld = malloc(sizeof(*pWorld));
    if (pWorld != NULL)
    {
        pWorld->pDevice       = pDevice;
        pWorld->playbackState = easyaudio_playing;
        pWorld->pFirstSound   = NULL;
        pWorld->lock          = easyaudio_create_mutex();
    }

    return pWorld;
}

void easyaudio_delete_world(easyaudio_world* pWorld)
{
    if (pWorld == NULL) {
        return;
    }

    // Delete every sound first.
    easyaudio_delete_all_sounds(pWorld);

    // Delete the lock after deleting every sound because we still need thread-safety at this point.
    easyaudio_delete_mutex(pWorld->lock);

    // Free the world last.
    free(pWorld);
}


easyaudio_sound* easyaudio_create_sound(easyaudio_world* pWorld, easyaudio_sound_desc desc)
{
    if (pWorld == NULL) {
        return NULL;
    }

    if ((desc.pInitialData == NULL || desc.sizeInBytes == 0) && (desc.onRead == NULL || desc.onSeek == NULL)) {
        // When streaming is not being used, the initial data must be valid at creation time.
        return NULL;
    }

    easyaudio_sound* pSound = malloc(sizeof(*pSound));
    if (pSound == NULL) {
        return NULL;
    }

    pSound->pWorld                 = pWorld;
    pSound->prevPlaybackState      = easyaudio_stopped;
    pSound->pNextSound             = NULL;
    pSound->pPrevSound             = NULL;
    pSound->isUsingStreamingBuffer = desc.sizeInBytes == 0 || desc.pInitialData == NULL;
    pSound->onDelete               = desc.onDelete;
    pSound->onRead                 = desc.onRead;
    pSound->onSeek                 = desc.onSeek;

    easyaudio_buffer_desc bufferDesc;
    bufferDesc.flags         = desc.flags;
    bufferDesc.format        = desc.format;
    bufferDesc.channels      = desc.channels;
    bufferDesc.sampleRate    = desc.sampleRate;
    bufferDesc.bitsPerSample = desc.bitsPerSample;
    bufferDesc.sizeInBytes   = desc.sizeInBytes;
    bufferDesc.pInitialData  = desc.pInitialData;

    if (pSound->isUsingStreamingBuffer)
    {
        easyaudio_streaming_callbacks streamingCallbacks;
        streamingCallbacks.pUserData = pSound;
        streamingCallbacks.read      = easyaudio_on_sound_read_callback;
        streamingCallbacks.seek      = easyaudio_on_sound_seek_callback;

        pSound->pBuffer = easyaudio_create_streaming_buffer(pWorld->pDevice, &bufferDesc, streamingCallbacks, desc.extraDataSize);
        if (pSound->pBuffer != NULL && desc.pExtraData != NULL)
        {
            memcpy(easyaudio_get_streaming_buffer_extra_data(pSound->pBuffer), desc.pExtraData, desc.extraDataSize);
        }
    }
    else
    {
        pSound->pBuffer = easyaudio_create_buffer(pWorld->pDevice, &bufferDesc, desc.extraDataSize);
        if (pSound->pBuffer != NULL && desc.pExtraData != NULL)
        {
            memcpy(easyaudio_get_buffer_extra_data(pSound->pBuffer), desc.pExtraData, desc.extraDataSize);
        }
    }


    // Return NULL if we failed to create the internal audio buffer.
    if (pSound->pBuffer == NULL) {
        free(pSound);
        return NULL;
    }


    // Only attach the sound to the internal list at the end when we know everything has worked.
    easyaudio_prepend_sound(pSound);

    return pSound;
}

void easyaudio_delete_sound(easyaudio_sound* pSound)
{
    if (pSound == NULL) {
        return;
    }


    // Remove the sound from the internal list first.
    easyaudio_remove_sound(pSound);


    // If we're deleting an inline sound, we want to remove the stop event callback. If we don't do this, we'll end up trying to delete
    // the sound twice.
    if (easyaudio_is_inline_sound(pSound)) {
        easyaudio_register_stop_callback(pSound->pBuffer, NULL, NULL);
    }


    // Let the application know that the sound is being deleted. We want to do this after removing the stop event just to be sure the
    // application doesn't try to explicitly stop the sound in this callback - that would be a problem for inlined sounds because they
    // are configured to delete themselves upon stopping which we are already in the process of doing.
    if (pSound->onDelete != NULL) {
        pSound->onDelete(pSound);
    }


    // Delete the internal audio buffer before letting the host application know about the deletion.
    easyaudio_delete_buffer(pSound->pBuffer);


    // Only free the sound after the application has been made aware the sound is being deleted.
    free(pSound);
}

void easyaudio_delete_all_sounds(easyaudio_world* pWorld)
{
    if (pWorld == NULL) {
        return;
    }

    while (pWorld->pFirstSound != NULL) {
        easyaudio_delete_sound(pWorld->pFirstSound);
    }
}


unsigned int easyaudio_get_sound_extra_data_size(easyaudio_sound* pSound)
{
    if (pSound == NULL) {
        return 0;
    }

    if (pSound->isUsingStreamingBuffer) {
        return easyaudio_get_streaming_buffer_extra_data_size(pSound->pBuffer);
    } else {
        return easyaudio_get_buffer_extra_data_size(pSound->pBuffer);
    }
}

void* easyaudio_get_sound_extra_data(easyaudio_sound* pSound)
{
    if (pSound == NULL) {
        return NULL;
    }

    if (pSound->isUsingStreamingBuffer) {
        return easyaudio_get_streaming_buffer_extra_data(pSound->pBuffer);
    } else {
        return easyaudio_get_buffer_extra_data(pSound->pBuffer);
    }
}


void easyaudio_play_sound(easyaudio_sound* pSound, bool loop)
{
    if (pSound != NULL) {
        if (pSound->isUsingStreamingBuffer) {
            easyaudio_play_streaming_buffer(pSound->pBuffer, loop);
        } else {
            easyaudio_play(pSound->pBuffer, loop);
        }
    }
}

void easyaudio_pause_sound(easyaudio_sound* pSound)
{
    if (pSound != NULL) {
        easyaudio_pause(pSound->pBuffer);
    }
}

void easyaudio_stop_sound(easyaudio_sound* pSound)
{
    if (pSound != NULL) {
        easyaudio_stop(pSound->pBuffer);
    }
}

easyaudio_playback_state easyaudio_get_sound_playback_state(easyaudio_sound* pSound)
{
    if (pSound == NULL) {
        return easyaudio_stopped;
    }

    return easyaudio_get_playback_state(pSound->pBuffer);
}

bool easyaudio_is_sound_looping(easyaudio_sound* pSound)
{
    if (pSound == NULL) {
        return false;
    }

    if (pSound->isUsingStreamingBuffer) {
        return easyaudio_is_streaming_buffer_looping(pSound->pBuffer);
    } else {
        return easyaudio_is_looping(pSound->pBuffer);
    }
}



void easyaudio_play_inline_sound(easyaudio_world* pWorld, easyaudio_sound_desc desc)
{
    if (pWorld == NULL) {
        return;
    }

    // We need to explicitly ensure 3D positioning is disabled.
    desc.flags &= ~EASYAUDIO_ENABLE_3D;

    easyaudio_sound* pSound = easyaudio_create_sound(pWorld, desc);
    if (pSound != NULL)
    {
        // For inline sounds we set a callback for when the sound is stopped. When this callback is fired, the sound is deleted.
        easyaudio_set_sound_stop_callback(pSound, easyaudio_inline_sound_stop_callback, pSound);

        // Start playing the sound once everything else has been set up.
        easyaudio_play_sound(pSound, false);
    }
}

void easyaudio_play_inline_sound_3f(easyaudio_world* pWorld, easyaudio_sound_desc desc, float posX, float posY, float posZ)
{
    if (pWorld == NULL) {
        return;
    }

    easyaudio_sound* pSound = easyaudio_create_sound(pWorld, desc);
    if (pSound != NULL)
    {
        // For inline sounds we set a callback for when the sound is stopped. When this callback is fired, the sound is deleted.
        easyaudio_set_sound_stop_callback(pSound, easyaudio_inline_sound_stop_callback, pSound);

        // Set the position before playing anything.
        easyaudio_set_sound_position(pSound, posX, posY, posZ);

        // Start playing the sound once everything else has been set up.
        easyaudio_play_sound(pSound, false);
    }
}


void easyaudio_stop_all_sounds(easyaudio_world* pWorld)
{
    if (pWorld == NULL) {
        return;
    }

    bool wasPlaying = pWorld->playbackState == easyaudio_playing;
    if (pWorld->playbackState != easyaudio_stopped)
    {
        // We need to loop over every sound and stop them. We also need to keep track of their previous playback state
        // so that when resume_all_sounds() is called, it can be restored correctly.
        for (easyaudio_sound* pSound = pWorld->pFirstSound; pSound != NULL; pSound = pSound->pNextSound)
        {
            if (wasPlaying) {
                pSound->prevPlaybackState = easyaudio_get_sound_playback_state(pSound);
            }

            easyaudio_stop_sound(pSound);
        }
    }
}

void easyaudio_pause_all_sounds(easyaudio_world* pWorld)
{
    if (pWorld == NULL) {
        return;
    }

    if (pWorld->playbackState == easyaudio_playing)
    {
        // We need to loop over every sound and stop them. We also need to keep track of their previous playback state
        // so that when resume_all_sounds() is called, it can be restored correctly.
        for (easyaudio_sound* pSound = pWorld->pFirstSound; pSound != NULL; pSound = pSound->pNextSound)
        {
            pSound->prevPlaybackState = easyaudio_get_sound_playback_state(pSound);
            easyaudio_pause_sound(pSound);
        }
    }
}

void easyaudio_resume_all_sounds(easyaudio_world* pWorld)
{
    if (pWorld == NULL) {
        return;
    }

    if (pWorld->playbackState != easyaudio_playing)
    {
        // When resuming playback, we use the previous playback state to determine how to resume.
        for (easyaudio_sound* pSound = pWorld->pFirstSound; pSound != NULL; pSound = pSound->pNextSound)
        {
            if (pSound->prevPlaybackState == easyaudio_playing) {
                easyaudio_play_sound(pSound, easyaudio_is_sound_looping(pSound));
            }
        }
    }
}


void easyaudio_set_sound_stop_callback(easyaudio_sound* pSound, easyaudio_event_callback_proc callback, void* pUserData)
{
    if (pSound != NULL) {
        easyaudio_register_stop_callback(pSound->pBuffer, callback, pUserData);
    }
}

void easyaudio_set_sound_pause_callback(easyaudio_sound* pSound, easyaudio_event_callback_proc callback, void* pUserData)
{
    if (pSound != NULL) {
        easyaudio_register_pause_callback(pSound->pBuffer, callback, pUserData);
    }
}

void easyaudio_set_sound_play_callback(easyaudio_sound* pSound, easyaudio_event_callback_proc callback, void* pUserData)
{
    if (pSound != NULL) {
        easyaudio_register_play_callback(pSound->pBuffer, callback, pUserData);
    }
}


void easyaudio_set_sound_position(easyaudio_sound* pSound, float posX, float posY, float posZ)
{
    if (pSound != NULL) {
        easyaudio_set_position(pSound->pBuffer, posX, posY, posZ);
    }
}


void easyaudio_set_sound_3d_mode(easyaudio_sound* pSound, easyaudio_3d_mode mode)
{
    if (pSound != NULL) {
        easyaudio_set_3d_mode(pSound->pBuffer, mode);
    }
}

easyaudio_3d_mode easyaudio_get_sound_3d_mode(easyaudio_sound* pSound)
{
    if (pSound == NULL) {
        return easyaudio_3d_mode_disabled;
    }

    return easyaudio_get_3d_mode(pSound->pBuffer);
}





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
// The DirectSound backend is mostly simple, except for event handling. Events
// are achieved through the use of Win32 event objects and waiting on them to
// be put into a signaled state by DirectSound. Due to this mechanic we need to
// create a worker thread that waits on each event.
//
// The worker thread waits on three general types of events. The first is an
// event that is signaled when the thread needs to be terminated. The second
// is an event that is signaled when a new set of events need to be waited on.
// The third is a set of events that correspond to an output buffer event (such
// as stop, pause, play and marker events.)
//
///////////////////////////////////////////////////////////////////////////////
#ifdef EASYAUDIO_BUILD_DSOUND
#include <windows.h>
#include <dsound.h>
#include <mmreg.h>
#include <stdio.h>      // For testing and debugging with printf(). Delete this later.



//// Message Queue ////

#define EASYAUDIO_MESSAGE_ID_UNKNOWN            0
#define EASYAUDIO_MESSAGE_ID_EVENT              1
#define EASYAUDIO_MESSAGE_ID_DELETE_BUFFER      2
#define EASYAUDIO_MESSAGE_ID_DELETE_DEVICE      3
#define EASYAUDIO_MESSAGE_ID_TERMINATE_THREAD   4

/// Structure representing an individual message
typedef struct
{
    /// The message ID.
    int id;

    /// A pointer to the relevant buffer.
    easyaudio_buffer* pBuffer;


    // The message-specific data.
    union
    {
        struct
        {
            /// A pointer to the callback function.
            easyaudio_event_callback_proc callback;

            /// The event ID.
            unsigned int eventID;

            /// The callback user data.
            void* pUserData;

        } callback_event;


        struct
        {
            /// A pointer to the DirectSound buffer object.
            LPDIRECTSOUNDBUFFER8 pDSBuffer;

            /// A pointer to the 3D DirectSound buffer object. This will be NULL if 3D positioning is disabled for the buffer.
            LPDIRECTSOUND3DBUFFER8 pDSBuffer3D;

            /// A pointer to the object for handling notification events.
            LPDIRECTSOUNDNOTIFY8 pDSNotify;

        } delete_buffer;


        struct
        {
            /// A pointer to the DIRECTSOUND object that was created with DirectSoundCreate8().
            LPDIRECTSOUND8 pDS;

            /// A pointer to the DIRECTSOUNDBUFFER object for the primary buffer.
            LPDIRECTSOUNDBUFFER pDSPrimaryBuffer;

            /// A pointer to the DIRECTSOUND3DLISTENER8 object associated with the device.
            LPDIRECTSOUND3DLISTENER8 pDSListener;

            /// A pointer to the device object being deleted.
            easyaudio_device* pDevice;

        } delete_device;

    } data;

} easyaudio_message_dsound;


/// Structure representing the main message queue.
///
/// The message queue is implemented as a fixed-sized cyclic array which means there should be no significant data
/// movement and fast pushing and popping.
typedef struct
{
    /// The buffer containing the list of events.
    easyaudio_message_dsound messages[EASYAUDIO_MAX_MESSAGE_QUEUE_SIZE];

    /// The number of active messages in the queue.
    unsigned int messageCount;

    /// The index of the first message in the queue.
    unsigned int iFirstMessage;

    /// The mutex for synchronizing access to message pushing and popping.
    easyaudio_mutex queueLock;

    /// The semaphore that's used for blocking easyaudio_next_message_dsound(). The maximum value is set to EASYAUDIO_MAX_MESSAGE_QUEUE_SIZE.
    HANDLE hMessageSemaphore;

    /// The message handling thread.
    HANDLE hMessageHandlingThread;

    /// Keeps track of whether or not the queue is deleted. We use this to ensure a thread does not try to post an event.
    bool isDeleted;

} easyaudio_message_queue_dsound;


/// The function to run on the message handling thread. This is implemented down the bottom.
DWORD WINAPI MessageHandlingThread_DSound(easyaudio_message_queue_dsound* pQueue);

/// Posts a new message. This is thread safe.
void easyaudio_post_message_dsound(easyaudio_message_queue_dsound* pQueue, easyaudio_message_dsound msg);



/// Initializes the given mesasge queue.
bool easyaudio_init_message_queue_dsound(easyaudio_message_queue_dsound* pQueue)
{
    if (pQueue == NULL) {
        return false;
    }

    pQueue->messageCount  = 0;
    pQueue->iFirstMessage = 0;

    pQueue->queueLock = easyaudio_create_mutex();
    if (pQueue->queueLock == NULL) {
        return false;
    }

    pQueue->hMessageSemaphore = CreateSemaphoreA(NULL, 0, EASYAUDIO_MAX_MESSAGE_QUEUE_SIZE, NULL);
    if (pQueue->hMessageSemaphore == NULL)
    {
        easyaudio_delete_mutex(pQueue->queueLock);
        return false;
    }

    pQueue->hMessageHandlingThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MessageHandlingThread_DSound, pQueue, 0, NULL);
    if (pQueue->hMessageHandlingThread == NULL)
    {
        CloseHandle(pQueue->hMessageSemaphore);
        easyaudio_delete_mutex(pQueue->queueLock);
        return false;
    }

    pQueue->isDeleted = false;

    return true;
}

/// Uninitializes the given message queue.
void easyaudio_uninit_message_queue_dsound(easyaudio_message_queue_dsound* pQueue)
{
    // We need to make sure the thread is closed properly before returning from here. To do this we just post an EASYAUDIO_MESSAGE_ID_TERMINATE_THREAD
    // event to the message queue and wait for the thread to finish.
    easyaudio_message_dsound msg;
    msg.id = EASYAUDIO_MESSAGE_ID_TERMINATE_THREAD;
    easyaudio_post_message_dsound(pQueue, msg);


    // Once we posted the event we just wait for the thread to process it and terminate.
    WaitForSingleObject(pQueue->hMessageHandlingThread, INFINITE);

    // At this point the thread has been terminated and we can clear everything.
    CloseHandle(pQueue->hMessageHandlingThread);
    pQueue->hMessageHandlingThread = NULL;

    CloseHandle(pQueue->hMessageSemaphore);
    pQueue->hMessageSemaphore = NULL;

    
    pQueue->isDeleted = true;
    easyaudio_lock_mutex(pQueue->queueLock);
    {
        pQueue->messageCount  = 0;
        pQueue->iFirstMessage = 0;
    }
    easyaudio_unlock_mutex(pQueue->queueLock);

    easyaudio_delete_mutex(pQueue->queueLock);
    pQueue->queueLock = NULL;
}


void easyaudio_post_message_dsound(easyaudio_message_queue_dsound* pQueue, easyaudio_message_dsound msg)
{
    assert(pQueue != NULL);

    if (pQueue->isDeleted) {
        return;
    }

    easyaudio_lock_mutex(pQueue->queueLock);
    {
        if (pQueue->messageCount < EASYAUDIO_MAX_MESSAGE_QUEUE_SIZE)
        {
            pQueue->messages[(pQueue->iFirstMessage + pQueue->messageCount) % EASYAUDIO_MAX_MESSAGE_QUEUE_SIZE] = msg;
            pQueue->messageCount += 1;

            ReleaseSemaphore(pQueue->hMessageSemaphore, 1, NULL);
        }
    }
    easyaudio_unlock_mutex(pQueue->queueLock);
}


/// Retrieves the next message in the queue.
///
/// @remarks
///     This blocks until a message is available. This will return false when it receives the EASYAUDIO_MESSAGE_ID_TERMINATE_THREAD message.
bool easyaudio_next_message_dsound(easyaudio_message_queue_dsound* pQueue, easyaudio_message_dsound* pMsgOut)
{
    if (WaitForSingleObject(pQueue->hMessageSemaphore, INFINITE) == WAIT_OBJECT_0)   // Wait for a message to become available.
    {
        easyaudio_message_dsound msg;
        msg.id = EASYAUDIO_MESSAGE_ID_UNKNOWN;

        easyaudio_lock_mutex(pQueue->queueLock);
        {
            assert(pQueue->messageCount > 0);

            msg = pQueue->messages[pQueue->iFirstMessage];

            pQueue->iFirstMessage = (pQueue->iFirstMessage + 1) % EASYAUDIO_MAX_MESSAGE_QUEUE_SIZE;
            pQueue->messageCount -= 1;
        }
        easyaudio_unlock_mutex(pQueue->queueLock);


        if (pMsgOut != NULL) {
            pMsgOut[0] = msg;
        }

        return msg.id != EASYAUDIO_MESSAGE_ID_TERMINATE_THREAD;
    }

    return false;
}


DWORD WINAPI MessageHandlingThread_DSound(easyaudio_message_queue_dsound* pQueue)
{
    assert(pQueue != NULL);

    easyaudio_message_dsound msg;
    while (easyaudio_next_message_dsound(pQueue, &msg))
    {
        assert(msg.id != EASYAUDIO_MESSAGE_ID_TERMINATE_THREAD);        // <-- easyaudio_next_message_dsound() will return false when it receives EASYAUDIO_MESSAGE_ID_TERMINATE_THREAD.

        switch (msg.id)
        {
            case EASYAUDIO_MESSAGE_ID_EVENT:
            {
                assert(msg.data.callback_event.callback != NULL);

                msg.data.callback_event.callback(msg.pBuffer, msg.data.callback_event.eventID, msg.data.callback_event.pUserData);
                break;
            }

            case EASYAUDIO_MESSAGE_ID_DELETE_BUFFER:
            {
                if (msg.data.delete_buffer.pDSNotify != NULL) {
                    IDirectSoundNotify_Release(msg.data.delete_buffer.pDSNotify);
                }

                if (msg.data.delete_buffer.pDSBuffer3D != NULL) {
                    IDirectSound3DBuffer_Release(msg.data.delete_buffer.pDSBuffer3D);
                }

                if (msg.data.delete_buffer.pDSBuffer != NULL) {
                    IDirectSoundBuffer8_Release(msg.data.delete_buffer.pDSBuffer);
                }

                free(msg.pBuffer);
                break;
            }

            case EASYAUDIO_MESSAGE_ID_DELETE_DEVICE:
            {
                if (msg.data.delete_device.pDSListener != NULL) {
                    IDirectSound3DListener_Release(msg.data.delete_device.pDSListener);
                }
                
                if (msg.data.delete_device.pDSPrimaryBuffer != NULL) {
                    IDirectSoundBuffer_Release(msg.data.delete_device.pDSPrimaryBuffer);
                }

                if (msg.data.delete_device.pDS != NULL) {
                    IDirectSound_Release(msg.data.delete_device.pDS);
                }

                free(msg.data.delete_device.pDevice);
                break;
            }

            default:
            {
                // Should never hit this.
                assert(false);
                break;
            }
        }
    }

    return 0;
}




/// Deactivates (but does not delete) every event associated with the given buffer.
void easyaudio_deactivate_buffer_events_dsound(easyaudio_buffer* pBuffer);


//// Event Management ////

typedef struct easyaudio_event_manager_dsound easyaudio_event_manager_dsound;
typedef struct easyaudio_event_dsound easyaudio_event_dsound;

struct easyaudio_event_dsound
{
    /// A pointer to the event manager that owns this event.
    easyaudio_event_manager_dsound* pEventManager;

    /// The event.
    HANDLE hEvent;

    /// The callback.
    easyaudio_event_callback_proc callback;

    /// A pointer to the applicable buffer.
    easyaudio_buffer* pBuffer;

    /// The event ID. For on_stop events, this will be set to EASYAUDIO_EVENT_STOP
    unsigned int eventID;

    /// A pointer to the user data.
    void* pUserData;

    /// The marker offset. Only used for marker events. Should be set to 0 for non-markers.
    DWORD markerOffset;

    /// Events are stored in a linked list. This is a pointer to the next event in the list.
    easyaudio_event_dsound* pNextEvent;

    /// A pointer to the previous event.
    easyaudio_event_dsound* pPrevEvent;
};

struct easyaudio_event_manager_dsound
{
    /// A pointer to the message queue where messages will be posted for event processing.
    easyaudio_message_queue_dsound* pMessageQueue;


    /// A handle to the event worker thread.
    HANDLE hThread;

    /// A handle to the terminator event object.
    HANDLE hTerminateEvent;

    /// A handle to the refresher event object.
    HANDLE hRefreshEvent;

    /// The mutex to use when refreshing the worker thread. This is used to ensure only one refresh is done at a time.
    easyaudio_mutex refreshMutex;

    /// The synchronization lock.
    easyaudio_mutex mainLock;

    /// The event object for notifying easy_audio when an event has finished being handled by the event handling thread.
    HANDLE hEventCompletionLock;


    /// The first event in a list.
    easyaudio_event_dsound* pFirstEvent;

    /// The last event in the list of events.
    easyaudio_event_dsound* pLastEvent;
};


/// Locks the event manager.
void easyaudio_lock_events_dsound(easyaudio_event_manager_dsound* pEventManager)
{
    easyaudio_lock_mutex(pEventManager->mainLock);
}

/// Unlocks the event manager.
void easyaudio_unlock_events_dsound(easyaudio_event_manager_dsound* pEventManager)
{
    easyaudio_unlock_mutex(pEventManager->mainLock);
}


/// Removes the given event from the event lists.
///
/// @remarks
///     This will be used when moving the event to a new location in the list or when it is being deleted.
void easyaudio_remove_event_dsound_nolock(easyaudio_event_dsound* pEvent)
{
    assert(pEvent != NULL);
    
    easyaudio_event_manager_dsound* pEventManager = pEvent->pEventManager;
    assert(pEventManager != NULL);

    if (pEventManager->pFirstEvent == pEvent) {
        pEventManager->pFirstEvent = pEvent->pNextEvent;
    }

    if (pEventManager->pLastEvent == pEvent) {
        pEventManager->pLastEvent = pEvent->pPrevEvent;
    }


    if (pEvent->pPrevEvent != NULL) {
        pEvent->pPrevEvent->pNextEvent = pEvent->pNextEvent;
    }

    if (pEvent->pNextEvent != NULL) {
        pEvent->pNextEvent->pPrevEvent = pEvent->pPrevEvent;
    }

    pEvent->pNextEvent = NULL;
    pEvent->pPrevEvent = NULL;
}

/// @copydoc easyaudio_remove_event_dsound_nolock()
void easyaudio_remove_event_dsound(easyaudio_event_dsound* pEvent)
{
    assert(pEvent != NULL);

    easyaudio_event_manager_dsound* pEventManager = pEvent->pEventManager;
    easyaudio_lock_events_dsound(pEventManager);
    {
        easyaudio_remove_event_dsound_nolock(pEvent);
    }
    easyaudio_unlock_events_dsound(pEventManager);
}

/// Adds the given event to the end of the internal list.
void easyaudio_append_event_dsound(easyaudio_event_dsound* pEvent)
{
    assert(pEvent != NULL);

    easyaudio_event_manager_dsound* pEventManager = pEvent->pEventManager;
    easyaudio_lock_events_dsound(pEventManager);
    {
        easyaudio_remove_event_dsound_nolock(pEvent);

        assert(pEvent->pNextEvent == NULL);

        if (pEventManager->pLastEvent != NULL) {
            pEvent->pPrevEvent = pEventManager->pLastEvent;
            pEvent->pPrevEvent->pNextEvent = pEvent;
        }

        if (pEventManager->pFirstEvent == NULL) {
            pEventManager->pFirstEvent = pEvent;
        }

        pEventManager->pLastEvent = pEvent;
    }
    easyaudio_unlock_events_dsound(pEventManager);
}

void easyaudio_refresh_worker_thread_event_queue(easyaudio_event_manager_dsound* pEventManager)
{
    assert(pEventManager != NULL);

    // To refresh the worker thread we just need to signal the refresh event. We then just need to wait for
    // processing to finish which we can do by waiting on another event to become signaled.

    easyaudio_lock_mutex(pEventManager->refreshMutex);
    {
        // Signal a refresh.
        SetEvent(pEventManager->hRefreshEvent);

        // Wait for refreshing to complete.
        WaitForSingleObject(pEventManager->hEventCompletionLock, INFINITE);
    }
    easyaudio_unlock_mutex(pEventManager->refreshMutex);
}


/// Closes the Win32 event handle of the given event.
void easyaudio_close_win32_event_handle_dsound(easyaudio_event_dsound* pEvent)
{
    assert(pEvent != NULL);
    assert(pEvent->pEventManager != NULL);


    // At the time of calling this function, pEvent should have been removed from the internal list. The issue is that
    // the event notification thread is waiting on it. Thus, we need to refresh the worker thread to ensure the event
    // have been flushed from that queue. To do this we just signal a special event that's used to trigger a refresh.
    easyaudio_refresh_worker_thread_event_queue(pEvent->pEventManager);

    // The worker thread should not be waiting on the event so we can go ahead and close the handle now.
    CloseHandle(pEvent->hEvent);
    pEvent->hEvent = NULL;
}


/// Updates the given event to use the given callback and user data.
void easyaudio_update_event_dsound(easyaudio_event_dsound* pEvent, easyaudio_event_callback_proc callback, void* pUserData)
{
    assert(pEvent != NULL);

    pEvent->callback  = callback;
    pEvent->pUserData = pUserData;

    easyaudio_refresh_worker_thread_event_queue(pEvent->pEventManager);
}

/// Creates a new event, but does not activate it.
///
/// @remarks
///     When an event is created, it just sits dormant and will never be triggered until it has been
///     activated with easyaudio_activate_event_dsound().
easyaudio_event_dsound* easyaudio_create_event_dsound(easyaudio_event_manager_dsound* pEventManager, easyaudio_event_callback_proc callback, easyaudio_buffer* pBuffer, unsigned int eventID, void* pUserData)
{
    easyaudio_event_dsound* pEvent = malloc(sizeof(easyaudio_event_dsound));
    if (pEvent != NULL)
    {
        pEvent->pEventManager = pEventManager;
        pEvent->hEvent        = CreateEventA(NULL, FALSE, FALSE, NULL);
        pEvent->callback      = NULL;
        pEvent->pBuffer       = pBuffer;
        pEvent->eventID       = eventID;
        pEvent->pUserData     = NULL;
        pEvent->markerOffset  = 0;
        pEvent->pNextEvent    = NULL;
        pEvent->pPrevEvent    = NULL;

        // Append the event to the internal list.
        easyaudio_append_event_dsound(pEvent);

        // This roundabout way of setting the callback and user data is to ensure the worker thread is made aware that it needs
        // to refresh it's local event data.
        easyaudio_update_event_dsound(pEvent, callback, pUserData);
    }

    return pEvent;
}

/// Deletes an event, and deactivates it.
///
/// @remarks
///     This will not return until the event has been deleted completely.
void easyaudio_delete_event_dsound(easyaudio_event_dsound* pEvent)
{
    assert(pEvent != NULL);

    // Set everything to NULL so the worker thread is aware that the event is about to get deleted.
    pEvent->pBuffer      = NULL;
    pEvent->callback     = NULL;
    pEvent->eventID      = 0;
    pEvent->pUserData    = NULL;
    pEvent->markerOffset = 0;

    // Remove the event from the list.
    easyaudio_remove_event_dsound(pEvent);

    // Close the Win32 event handle.
    if (pEvent->hEvent != NULL) {
        easyaudio_close_win32_event_handle_dsound(pEvent);
    }


    // At this point everything has been closed so we can safely free the memory and return.
    free(pEvent);
}


/// Gathers the event handles and callback data for all of the relevant buffer events.
unsigned int easyaudio_gather_events_dsound(easyaudio_event_manager_dsound *pEventManager, HANDLE* pHandlesOut, easyaudio_event_dsound** ppEventsOut, unsigned int outputBufferSize)
{
    assert(pEventManager != NULL);
    assert(pHandlesOut != NULL);
    assert(ppEventsOut != NULL);
    assert(outputBufferSize >= 2);

    unsigned int i = 2;
    easyaudio_lock_events_dsound(pEventManager);
    {
        pHandlesOut[0] = pEventManager->hTerminateEvent;
        ppEventsOut[0] = NULL;

        pHandlesOut[1] = pEventManager->hRefreshEvent;
        ppEventsOut[1] = NULL;


        easyaudio_event_dsound* pEvent = pEventManager->pFirstEvent;
        while (i < outputBufferSize && pEvent != NULL)
        {
            if (pEvent->hEvent != NULL)
            {
                pHandlesOut[i] = pEvent->hEvent;
                ppEventsOut[i] = pEvent;

                i += 1;
            }
        
            pEvent = pEvent->pNextEvent;
        }
    }
    easyaudio_unlock_events_dsound(pEventManager);

    return i;
}

/// The entry point to the event worker thread.
DWORD WINAPI DSound_EventWorkerThreadProc(easyaudio_event_manager_dsound *pEventManager)
{
    if (pEventManager != NULL)
    {
        HANDLE hTerminateEvent = pEventManager->hTerminateEvent;
        HANDLE hRefreshEvent   = pEventManager->hRefreshEvent;

        HANDLE eventHandles[1024];
        easyaudio_event_dsound* events[1024];
        unsigned int eventCount = easyaudio_gather_events_dsound(pEventManager, eventHandles, events, 1024);   // <-- Initial gather.

        bool requestedRefresh = false;
        for (;;)
        {
            if (requestedRefresh)
            {
                eventCount = easyaudio_gather_events_dsound(pEventManager, eventHandles, events, 1024);

                // Refreshing is done, so now we need to let other threads know about it.
                SetEvent(pEventManager->hEventCompletionLock);
                requestedRefresh = false;
            }

            

            DWORD rc = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);
            if (rc >= WAIT_OBJECT_0 && rc < eventCount)
            {
                const unsigned int eventIndex = rc - WAIT_OBJECT_0;
                HANDLE hEvent = eventHandles[eventIndex];

                if (hEvent == hTerminateEvent)
                {
                    // The terminator event was signaled. We just return from the thread immediately.
                    return 0;
                }

                if (hEvent == hRefreshEvent)
                {
                    assert(hRefreshEvent == pEventManager->hRefreshEvent);

                    // This event will get signaled when a new set of events need to be waited on, such as when a new event has been registered on a buffer.
                    requestedRefresh = true;
                    continue;
                }


                // If we get here if means we have hit a callback event.
                easyaudio_event_dsound* pEvent = events[eventIndex];
                if (pEvent->callback != NULL)
                {
                    assert(pEvent->hEvent == hEvent);

                    // The stop event will be signaled by DirectSound when IDirectSoundBuffer::Stop() is called. The problem is that we need to call that when the
                    // sound is paused as well. Thus, we need to check if we got the stop event, and if so DON'T call the callback function if it is in a non-stopped
                    // state.
                    bool isStopEventButNotStopped = pEvent->eventID == EASYAUDIO_EVENT_ID_STOP && easyaudio_get_playback_state(pEvent->pBuffer) != easyaudio_stopped;
                    if (!isStopEventButNotStopped)
                    {
                        // We don't call the callback directly. Instead we post a message to the message handling thread for processing later.
                        easyaudio_message_dsound msg;
                        msg.id      = EASYAUDIO_MESSAGE_ID_EVENT;
                        msg.pBuffer = pEvent->pBuffer;
                        msg.data.callback_event.callback  = pEvent->callback;
                        msg.data.callback_event.eventID   = pEvent->eventID;
                        msg.data.callback_event.pUserData = pEvent->pUserData;
                        easyaudio_post_message_dsound(pEventManager->pMessageQueue, msg);
                    }
                }
            }
        }
    }

    return 0;
}


/// Initializes the event manager by creating the thread and event objects.
bool easyaudio_init_event_manager_dsound(easyaudio_event_manager_dsound* pEventManager, easyaudio_message_queue_dsound* pMessageQueue)
{
    assert(pEventManager != NULL);
    assert(pMessageQueue != NULL);

    pEventManager->pMessageQueue = pMessageQueue;

    HANDLE hTerminateEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (hTerminateEvent == NULL) {
        return false;
    }

    HANDLE hRefreshEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (hRefreshEvent == NULL)
    {
        CloseHandle(hTerminateEvent);
        return false;
    }

    easyaudio_mutex refreshMutex = easyaudio_create_mutex();
    if (refreshMutex == NULL)
    {
        CloseHandle(hTerminateEvent);
        CloseHandle(hRefreshEvent);
        return false;
    }

    easyaudio_mutex mainLock = easyaudio_create_mutex();
    if (mainLock == NULL)
    {
        CloseHandle(hTerminateEvent);
        CloseHandle(hRefreshEvent);
        easyaudio_delete_mutex(refreshMutex);
        return false;
    }

    HANDLE hEventCompletionLock = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (hEventCompletionLock == NULL)
    {
        CloseHandle(hTerminateEvent);
        CloseHandle(hRefreshEvent);
        easyaudio_delete_mutex(refreshMutex);
        easyaudio_delete_mutex(mainLock);
        return false;
    }


    HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)DSound_EventWorkerThreadProc, pEventManager, 0, NULL);
    if (hThread == NULL)
    {
        CloseHandle(hTerminateEvent);
        CloseHandle(hRefreshEvent);
        easyaudio_delete_mutex(refreshMutex);
        easyaudio_delete_mutex(mainLock);
        CloseHandle(hEventCompletionLock);
        return false;
    }


    pEventManager->hTerminateEvent      = hTerminateEvent;
    pEventManager->hRefreshEvent        = hRefreshEvent;
    pEventManager->refreshMutex         = refreshMutex;
    pEventManager->mainLock             = mainLock;
    pEventManager->hEventCompletionLock = hEventCompletionLock;
    pEventManager->hThread              = hThread;

    pEventManager->pFirstEvent   = NULL;
    pEventManager->pLastEvent    = NULL;

    return true;
}

/// Shuts down the event manager by closing the thread and event objects.
///
/// @remarks
///     This does not return until the worker thread has been terminated completely.
///     @par
///     This will delete every event, so any pointers to events will be made invalid upon calling this function.
void easyaudio_uninit_event_manager_dsound(easyaudio_event_manager_dsound* pEventManager)
{
    assert(pEventManager != NULL);


    // Cleanly delete every event first.
    while (pEventManager->pFirstEvent != NULL) {
        easyaudio_delete_event_dsound(pEventManager->pFirstEvent);
    }



    // Terminate the thread and wait for the thread to finish executing before freeing the context for real.
    SignalObjectAndWait(pEventManager->hTerminateEvent, pEventManager->hThread, INFINITE, FALSE);

    // Only delete the thread after it has returned naturally.
    CloseHandle(pEventManager->hThread);
    pEventManager->hThread = NULL;


    // Once the thread has been terminated we can delete the terminator and refresher events.
    CloseHandle(pEventManager->hTerminateEvent);
    pEventManager->hTerminateEvent = NULL;

    CloseHandle(pEventManager->hRefreshEvent);
    pEventManager->hRefreshEvent = NULL;

    easyaudio_delete_mutex(pEventManager->refreshMutex);
    pEventManager->refreshMutex = NULL;

    easyaudio_delete_mutex(pEventManager->mainLock);
    pEventManager->mainLock = NULL;


    CloseHandle(pEventManager->hEventCompletionLock);
    pEventManager->hEventCompletionLock = NULL;
}


//// End Event Management ////

static GUID g_DSListenerGUID                       = {0x279AFA84, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60};
static GUID g_DirectSoundBuffer8GUID               = {0x6825a449, 0x7524, 0x4d82, 0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e};
static GUID g_DirectSound3DBuffer8GUID             = {0x279AFA86, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60};
static GUID g_DirectSoundNotifyGUID                = {0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16};
static GUID g_KSDATAFORMAT_SUBTYPE_PCM_GUID        = {0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
static GUID g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID = {0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};

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


    /// The number of output devices that were iterated when the context was created. This is static, so if the user was to unplug
    /// a device one would need to re-create the context.
    unsigned int outputDeviceCount;

    /// The buffer containing the list of enumerated output devices.
    easyaudio_device_info_dsound outputDeviceInfo[EASYAUDIO_MAX_DEVICE_COUNT];


    /// The number of capture devices that were iterated when the context was created. This is static, so if the user was to unplug
    /// a device one would need to re-create the context.
    unsigned int inputDeviceCount;

    /// The buffer containing the list of enumerated input devices.
    easyaudio_device_info_dsound inputDeviceInfo[EASYAUDIO_MAX_DEVICE_COUNT];


    /// The event manager.
    easyaudio_event_manager_dsound eventManager;


    /// The message queue.
    easyaudio_message_queue_dsound messageQueue;

} easyaudio_context_dsound;

typedef struct
{
    /// The base device data. This must always be the first item in the struct.
    easyaudio_device base;

    /// A pointer to the DIRECTSOUND object that was created with DirectSoundCreate8().
    LPDIRECTSOUND8 pDS;

    /// A pointer to the DIRECTSOUNDBUFFER object for the primary buffer.
    LPDIRECTSOUNDBUFFER pDSPrimaryBuffer;

    /// A pointer to the DIRECTSOUND3DLISTENER8 object associated with the device.
    LPDIRECTSOUND3DLISTENER8 pDSListener;

} easyaudio_device_dsound;

typedef struct
{
    /// The base buffer data. This must always be the first item in the struct.
    easyaudio_buffer base;

    /// A pointer to the DirectSound buffer object.
    LPDIRECTSOUNDBUFFER8 pDSBuffer;

    /// A pointer to the 3D DirectSound buffer object. This will be NULL if 3D positioning is disabled for the buffer.
    LPDIRECTSOUND3DBUFFER8 pDSBuffer3D;

    /// A pointer to the object for handling notification events.
    LPDIRECTSOUNDNOTIFY8 pDSNotify;

    /// The current playback state.
    easyaudio_playback_state playbackState;


    /// The number of marker events that have been registered. This will never be more than EASYAUDIO_MAX_MARKER_COUNT.
    unsigned int markerEventCount;

    /// The marker events.
    easyaudio_event_dsound* pMarkerEvents[EASYAUDIO_MAX_MARKER_COUNT];

    /// The event to trigger when the sound is stopped.
    easyaudio_event_dsound* pStopEvent;

    /// The event to trigger when the sound is paused.
    easyaudio_event_dsound* pPauseEvent;

    /// The event to trigger when the sound is played or resumed.
    easyaudio_event_dsound* pPlayEvent;


    /// The size in bytes of the buffer's extra data.
    unsigned int extraDataSize;

    /// The buffer's extra data.
    unsigned char pExtraData[1];

} easyaudio_buffer_dsound;


void easyaudio_activate_buffer_events_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    unsigned int dwPositionNotifies = 0;
    DSBPOSITIONNOTIFY n[EASYAUDIO_MAX_MARKER_COUNT + 1];        // +1 because we use this array for the markers + stop event.

    // Stop
    if (pBufferDS->pStopEvent != NULL)
    {
        LPDSBPOSITIONNOTIFY pN = n + dwPositionNotifies;
        pN->dwOffset     = DSBPN_OFFSETSTOP;
        pN->hEventNotify = pBufferDS->pStopEvent->hEvent;

        dwPositionNotifies += 1;
    }

    // Markers
    for (unsigned int iMarker = 0; iMarker < pBufferDS->markerEventCount; ++iMarker)
    {
        LPDSBPOSITIONNOTIFY pN = n + dwPositionNotifies;
        pN->dwOffset     = pBufferDS->pMarkerEvents[iMarker]->markerOffset;
        pN->hEventNotify = pBufferDS->pMarkerEvents[iMarker]->hEvent;

        dwPositionNotifies += 1;
    }


    HRESULT hr = IDirectSoundNotify_SetNotificationPositions(pBufferDS->pDSNotify, dwPositionNotifies, n);
#if 0
    if (FAILED(hr)) {
        printf("WARNING: FAILED TO CREATE DIRECTSOUND NOTIFIERS\n");
    }
#else
    (void)hr;
#endif
}

void easyaudio_deactivate_buffer_events_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);


    HRESULT hr = IDirectSoundNotify_SetNotificationPositions(pBufferDS->pDSNotify, 0, NULL);
#if 0
    if (FAILED(hr)) {
        printf("WARNING: FAILED TO CLEAR DIRECTSOUND NOTIFIERS\n");
    }
#else
    (void)hr;
#endif
}


void easyaudio_delete_context_dsound(easyaudio_context* pContext)
{
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pContext;
    assert(pContextDS != NULL);

    easyaudio_uninit_event_manager_dsound(&pContextDS->eventManager);

    // The message queue needs to uninitialized after the DirectSound marker notification thread.
    easyaudio_uninit_message_queue_dsound(&pContextDS->messageQueue);

    FreeLibrary(pContextDS->hDSoundDLL);
    free(pContextDS);
}


unsigned int easyaudio_get_output_device_count_dsound(easyaudio_context* pContext)
{
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pContext;
    assert(pContextDS != NULL);

    return pContextDS->outputDeviceCount;
}

bool easyaudio_get_output_device_info_dsound(easyaudio_context* pContext, unsigned int deviceIndex, easyaudio_device_info* pInfoOut)
{
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pContext;
    assert(pContextDS != NULL);
    assert(pInfoOut != NULL);

    if (deviceIndex >= pContextDS->outputDeviceCount) {
        return false;
    }


    easyaudio_strcpy(pInfoOut->description, sizeof(pInfoOut->description), pContextDS->outputDeviceInfo[deviceIndex].description);

    return true;
}


easyaudio_device* easyaudio_create_output_device_dsound(easyaudio_context* pContext, unsigned int deviceIndex)
{
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pContext;
    assert(pContextDS != NULL);

    if (deviceIndex >= pContextDS->outputDeviceCount) {
        return NULL;
    }


    LPDIRECTSOUND8 pDS;

    // Create the device.
    HRESULT hr;
    if (deviceIndex == 0) {
        hr = pContextDS->pDirectSoundCreate8(NULL, &pDS, NULL);
    } else {
        hr = pContextDS->pDirectSoundCreate8(&pContextDS->outputDeviceInfo[deviceIndex].guid, &pDS, NULL);
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


    // Primary buffer.
    DSBUFFERDESC descDSPrimary;
    memset(&descDSPrimary, 0, sizeof(DSBUFFERDESC));
    descDSPrimary.dwSize          = sizeof(DSBUFFERDESC);
    descDSPrimary.dwFlags         = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRL3D;
    descDSPrimary.guid3DAlgorithm = GUID_NULL;

    LPDIRECTSOUNDBUFFER pDSPrimaryBuffer;
    hr = IDirectSound_CreateSoundBuffer(pDS, &descDSPrimary, &pDSPrimaryBuffer, NULL);
    if (FAILED(hr)) {
        IDirectSound_Release(pDS);
        return NULL;
    }


    WAVEFORMATIEEEFLOATEX wf = {0};
    wf.Format.cbSize               = sizeof(wf);
    wf.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels            = 2;
    wf.Format.nSamplesPerSec       = 44100;
    wf.Format.wBitsPerSample       = 32;
    wf.Format.nBlockAlign          = (wf.Format.nChannels * wf.Format.wBitsPerSample) / 8;
    wf.Format.nAvgBytesPerSec      = wf.Format.nBlockAlign * wf.Format.nSamplesPerSec;
    wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
    wf.dwChannelMask               = 0;
    wf.SubFormat                   = g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID;
    hr = IDirectSoundBuffer_SetFormat(pDSPrimaryBuffer, (WAVEFORMATEX*)&wf);
    if (FAILED(hr)) {
        IDirectSoundBuffer_Release(pDSPrimaryBuffer);
        IDirectSound_Release(pDS);
        return NULL;
    }


    // Listener.
    LPDIRECTSOUND3DLISTENER8 pDSListener = NULL;
    hr = pDSPrimaryBuffer->lpVtbl->QueryInterface(pDSPrimaryBuffer, &g_DSListenerGUID, (LPVOID*)&pDSListener);
    if (FAILED(hr)) {
        IDirectSoundBuffer_Release(pDSPrimaryBuffer);
        IDirectSound_Release(pDS);
        return NULL;
    }


    easyaudio_device_dsound* pDeviceDS = malloc(sizeof(easyaudio_device_dsound));
    if (pDeviceDS != NULL)
    {
        pDeviceDS->base.pContext    = pContext;
        pDeviceDS->pDS              = pDS;
        pDeviceDS->pDSPrimaryBuffer = pDSPrimaryBuffer;
        pDeviceDS->pDSListener      = pDSListener;

        return (easyaudio_device*)pDeviceDS;
    }
    else
    {
        IDirectSound3DListener_Release(pDSListener);
        IDirectSoundBuffer_Release(pDeviceDS->pDSPrimaryBuffer);
        IDirectSound_Release(pDS);
        return NULL;
    }
}

void easyaudio_delete_output_device_dsound(easyaudio_device* pDevice)
{
    easyaudio_device_dsound* pDeviceDS = (easyaudio_device_dsound*)pDevice;
    assert(pDeviceDS != NULL);

    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pDevice->pContext;
    assert(pContextDS != NULL);

    // The device id not deleted straight away. Instead we post a message to the message for delayed processing. The reason for this is that buffer
    // deletion is also delayed which means we want to ensure any delayed processing of buffers is handled before deleting the device.
    easyaudio_message_dsound msg;
    msg.id      = EASYAUDIO_MESSAGE_ID_DELETE_DEVICE;
    msg.pBuffer = NULL;
    msg.data.delete_device.pDSListener      = pDeviceDS->pDSListener;
    msg.data.delete_device.pDSPrimaryBuffer = pDeviceDS->pDSPrimaryBuffer;
    msg.data.delete_device.pDS              = pDeviceDS->pDS;
    msg.data.delete_device.pDevice          = pDevice;
    easyaudio_post_message_dsound(&pContextDS->messageQueue, msg);

#if 0
    IDirectSound3DListener_Release(pDeviceDS->pDSListener);
    IDirectSoundBuffer_Release(pDeviceDS->pDSPrimaryBuffer);
    IDirectSound_Release(pDeviceDS->pDS);
    free(pDeviceDS);
#endif
}


easyaudio_buffer* easyaudio_create_buffer_dsound(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc, unsigned int extraDataSize)
{
    easyaudio_device_dsound* pDeviceDS = (easyaudio_device_dsound*)pDevice;
    assert(pDeviceDS != NULL);
    assert(pBufferDesc != NULL);

    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pDevice->pContext;
    assert(pContextDS != NULL);


    // 3D is only valid for mono sounds.
    if (pBufferDesc->channels > 1 && (pBufferDesc->flags & EASYAUDIO_ENABLE_3D) != 0) {
        return NULL;
    }

    WAVEFORMATIEEEFLOATEX wf = {0};
    wf.Format.cbSize = sizeof(wf);
    wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels = (WORD)pBufferDesc->channels;
    wf.Format.nSamplesPerSec = pBufferDesc->sampleRate;
    wf.Format.wBitsPerSample = (WORD)pBufferDesc->bitsPerSample;
    wf.Format.nBlockAlign = (wf.Format.nChannels * wf.Format.wBitsPerSample) / 8;
    wf.Format.nAvgBytesPerSec = wf.Format.nBlockAlign * wf.Format.nSamplesPerSec;
    wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
    wf.dwChannelMask = 0;

    if (pBufferDesc->format == easyaudio_format_pcm) {
        wf.SubFormat = g_KSDATAFORMAT_SUBTYPE_PCM_GUID;
    } else if (pBufferDesc->format == easyaudio_format_float) {
        wf.SubFormat = g_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID;
    } else {
        return NULL;
    }
    


    // We want to try and create a 3D enabled buffer, however this will fail whenever the number of channels is > 1. In this case
    // we do not want to attempt to create a 3D enabled buffer because it will just fail anyway. Instead we'll just create a normal
    // buffer with panning enabled.
    DSBUFFERDESC descDS;
    memset(&descDS, 0, sizeof(DSBUFFERDESC)); 
    descDS.dwSize          = sizeof(DSBUFFERDESC); 
    descDS.dwFlags         = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    descDS.dwBufferBytes   = pBufferDesc->sizeInBytes;
    descDS.lpwfxFormat     = (WAVEFORMATEX*)&wf;

    LPDIRECTSOUNDBUFFER8   pDSBuffer   = NULL;
    LPDIRECTSOUND3DBUFFER8 pDSBuffer3D = NULL;
    if ((pBufferDesc->flags & EASYAUDIO_ENABLE_3D) == 0)
    {
        // 3D Disabled.
        descDS.dwFlags |= DSBCAPS_CTRLPAN;

        LPDIRECTSOUNDBUFFER pDSBufferTemp;
        HRESULT hr = IDirectSound_CreateSoundBuffer(pDeviceDS->pDS, &descDS, &pDSBufferTemp, NULL);
        if (FAILED(hr)) {
            return NULL;
        }

        hr = pDSBufferTemp->lpVtbl->QueryInterface(pDSBufferTemp, &g_DirectSoundBuffer8GUID, &pDSBuffer);
        if (FAILED(hr)) {
            IDirectSoundBuffer_Release(pDSBufferTemp);
            return NULL;
        }
        IDirectSoundBuffer_Release(pDSBufferTemp);
    }
    else
    {
        // 3D Enabled.
        descDS.dwFlags |= DSBCAPS_CTRL3D;
        descDS.guid3DAlgorithm = DS3DALG_DEFAULT;

        LPDIRECTSOUNDBUFFER pDSBufferTemp;
        HRESULT hr = IDirectSound_CreateSoundBuffer(pDeviceDS->pDS, &descDS, &pDSBufferTemp, NULL);
        if (FAILED(hr)) {
            return NULL;
        }

        hr = pDSBufferTemp->lpVtbl->QueryInterface(pDSBufferTemp, &g_DirectSoundBuffer8GUID, &pDSBuffer);
        if (FAILED(hr)) {
            IDirectSoundBuffer_Release(pDSBufferTemp);
            return NULL;
        }
        IDirectSoundBuffer_Release(pDSBufferTemp);


        hr = IDirectSoundBuffer_QueryInterface(pDSBuffer, &g_DirectSound3DBuffer8GUID, &pDSBuffer3D);
        if (FAILED(hr)) {
            return NULL;
        }

        IDirectSound3DBuffer_SetPosition(pDSBuffer3D, 0, 0, 0, DS3D_IMMEDIATE);

        if ((pBufferDesc->flags & EASYAUDIO_RELATIVE_3D) != 0) {
            IDirectSound3DBuffer_SetMode(pDSBuffer3D, DS3DMODE_HEADRELATIVE, DS3D_IMMEDIATE);
        }
    }



    // We need to create a notification object so we can notify the host application when the playback buffer hits a certain point.
    LPDIRECTSOUNDNOTIFY8 pDSNotify;
    HRESULT hr = pDSBuffer->lpVtbl->QueryInterface(pDSBuffer, &g_DirectSoundNotifyGUID, &pDSNotify);
    if (FAILED(hr)) {
        IDirectSound3DBuffer_Release(pDSBuffer3D);
        IDirectSoundBuffer8_Release(pDSBuffer);
        return NULL;
    }


    easyaudio_buffer_dsound* pBufferDS = malloc(sizeof(easyaudio_buffer_dsound) - sizeof(pBufferDS->pExtraData) + extraDataSize);
    if (pBufferDS == NULL) {
        IDirectSound3DBuffer_Release(pDSBuffer3D);
        IDirectSoundBuffer8_Release(pDSBuffer);
        return NULL;
    }

    pBufferDS->base.pDevice      = pDevice;
    pBufferDS->pDSBuffer         = pDSBuffer;
    pBufferDS->pDSBuffer3D       = pDSBuffer3D;
    pBufferDS->pDSNotify         = pDSNotify;
    pBufferDS->playbackState     = easyaudio_stopped;

    pBufferDS->markerEventCount  = 0;
    memset(pBufferDS->pMarkerEvents, 0, sizeof(pBufferDS->pMarkerEvents));
    pBufferDS->pStopEvent        = NULL;
    pBufferDS->pPauseEvent       = NULL;
    pBufferDS->pPlayEvent        = NULL;
    


    // Fill with initial data, if applicable.
    if (pBufferDesc->pInitialData != NULL) {
        easyaudio_set_buffer_data((easyaudio_buffer*)pBufferDS, 0, pBufferDesc->pInitialData, pBufferDesc->sizeInBytes);
    }

    return (easyaudio_buffer*)pBufferDS;
}

void easyaudio_delete_buffer_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    assert(pBuffer->pDevice != NULL);

    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pBuffer->pDevice->pContext;
    assert(pContextDS != NULL);


    // Deactivate the DirectSound notify events for sanity.
    easyaudio_deactivate_buffer_events_dsound(pBuffer);


    easyaudio_message_dsound msg;
    msg.id      = EASYAUDIO_MESSAGE_ID_DELETE_BUFFER;
    msg.pBuffer = pBuffer;
    msg.data.delete_buffer.pDSNotify   = pBufferDS->pDSNotify;
    msg.data.delete_buffer.pDSBuffer3D = pBufferDS->pDSBuffer3D;
    msg.data.delete_buffer.pDSBuffer   = pBufferDS->pDSBuffer;
    easyaudio_post_message_dsound(&pContextDS->messageQueue, msg);

#if 0
    if (pBufferDS->pDSNotify != NULL) {
        IDirectSoundNotify_Release(pBufferDS->pDSNotify);
    }

    if (pBufferDS->pDSBuffer3D != NULL) {
        IDirectSound3DBuffer_Release(pBufferDS->pDSBuffer3D);
    }

    if (pBufferDS->pDSBuffer != NULL) {
        IDirectSoundBuffer8_Release(pBufferDS->pDSBuffer);
    }

    free(pBufferDS);
#endif
}


unsigned int easyaudio_get_buffer_extra_data_size_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    return pBufferDS->extraDataSize;
}

void* easyaudio_get_buffer_extra_data_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    return pBufferDS->pExtraData;
}


bool easyaudio_set_buffer_data_dsound(easyaudio_buffer* pBuffer, unsigned int offset, const void* pData, unsigned int dataSizeInBytes)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    assert(pData != NULL);

    LPVOID lpvWrite;
    DWORD dwLength;
    HRESULT hr = IDirectSoundBuffer8_Lock(pBufferDS->pDSBuffer, offset, dataSizeInBytes, &lpvWrite, &dwLength, NULL, NULL, 0);
    if (FAILED(hr)) {
        return false;
    }

    assert(dataSizeInBytes <= dwLength);
    memcpy(lpvWrite, pData, dataSizeInBytes);

    hr = IDirectSoundBuffer8_Unlock(pBufferDS->pDSBuffer, lpvWrite, dwLength, NULL, 0);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}


void easyaudio_play_dsound(easyaudio_buffer* pBuffer, bool loop)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    bool postEvent = true;
    if (pBufferDS->playbackState == easyaudio_playing) {
        postEvent = false;
    }


    // Events need to be activated.
    if (pBufferDS->playbackState == easyaudio_stopped) {
        easyaudio_activate_buffer_events_dsound(pBuffer);
    }


    DWORD dwFlags = 0;
    if (loop) {
        dwFlags |= DSBPLAY_LOOPING;
    }

    pBufferDS->playbackState = easyaudio_playing;
    IDirectSoundBuffer8_Play(pBufferDS->pDSBuffer, 0, 0, dwFlags);

    // If we have a play event we need to signal the event which will cause the worker thread to call the callback function.
    if (pBufferDS->pPlayEvent != NULL && postEvent) {
        SetEvent(pBufferDS->pPlayEvent->hEvent);
    }
}

void easyaudio_pause_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    if (pBufferDS->playbackState == easyaudio_playing)
    {
        pBufferDS->playbackState = easyaudio_paused;
        IDirectSoundBuffer8_Stop(pBufferDS->pDSBuffer);

        // If we have a pause event we need to signal the event which will cause the worker thread to call the callback function.
        if (pBufferDS->pPlayEvent != NULL) {
            SetEvent(pBufferDS->pPauseEvent->hEvent);
        }
    }
}

void easyaudio_stop_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    if (pBufferDS->playbackState == easyaudio_playing)
    {
        pBufferDS->playbackState = easyaudio_stopped;
        IDirectSoundBuffer8_Stop(pBufferDS->pDSBuffer);
        IDirectSoundBuffer8_SetCurrentPosition(pBufferDS->pDSBuffer, 0);
    }
    else if (pBufferDS->playbackState == easyaudio_paused)
    {
        pBufferDS->playbackState = easyaudio_stopped;
        IDirectSoundBuffer8_SetCurrentPosition(pBufferDS->pDSBuffer, 0);

        if (pBufferDS->pStopEvent != NULL) {
            SetEvent(pBufferDS->pStopEvent->hEvent);
        }
    }
}

easyaudio_playback_state easyaudio_get_playback_state_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    return pBufferDS->playbackState;
}


void easyaudio_set_playback_position_dsound(easyaudio_buffer* pBuffer, unsigned int position)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    pBufferDS->pDSBuffer->lpVtbl->SetCurrentPosition(pBufferDS->pDSBuffer, position);
}

unsigned int easyaudio_get_playback_position_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    DWORD position;
    HRESULT hr = pBufferDS->pDSBuffer->lpVtbl->GetCurrentPosition(pBufferDS->pDSBuffer, &position, NULL);
    if (FAILED(hr)) {
        return 0;
    }

    return position;
}


void easyaudio_set_pan_dsound(easyaudio_buffer* pBuffer, float pan)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    LONG panDB;
    if (pan == 0) {
        panDB = DSBPAN_CENTER;
    } else {
        if (pan > 1) {
            panDB = DSBPAN_RIGHT;
        } else if (pan < -1) {
            panDB = DSBPAN_LEFT;
        } else {
            if (pan < 0) {
                panDB =  (LONG)((20*log10f(1 + pan)) * 100);
            } else {
                panDB = -(LONG)((20*log10f(1 - pan)) * 100);
            }
        }
    }

    IDirectSoundBuffer_SetPan(pBufferDS->pDSBuffer, panDB);
}

float easyaudio_get_pan_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    LONG panDB;
    HRESULT hr = IDirectSoundBuffer_GetPan(pBufferDS->pDSBuffer, &panDB);
    if (FAILED(hr)) {
        return 0;
    }

    
    if (panDB < 0) {
        return -(1 - (float)(1.0f / powf(10.0f, -panDB / (20.0f*100.0f))));
    }

    if (panDB > 0) {
        return  (1 - (float)(1.0f / powf(10.0f,  panDB / (20.0f*100.0f))));
    }

    return 0;
}


void easyaudio_set_volume_dsound(easyaudio_buffer* pBuffer, float volume)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    LONG volumeDB;
    if (volume > 0) {
        if (volume < 1) {
            volumeDB = (LONG)((20*log10f(volume)) * 100);
        } else {
            volumeDB = DSBVOLUME_MAX;
        }
    } else {
        volumeDB = DSBVOLUME_MIN;
    }

    IDirectSoundBuffer_SetVolume(pBufferDS->pDSBuffer, volumeDB);
}

float easyaudio_get_volume_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    LONG volumeDB;
    HRESULT hr = IDirectSoundBuffer_GetVolume(pBufferDS->pDSBuffer, &volumeDB);
    if (FAILED(hr)) {
        return 1;
    }

    return (float)(1.0f / powf(10.0f, -volumeDB / (20.0f*100.0f)));
}


void easyaudio_remove_markers_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    for (unsigned int iMarker = 0; iMarker < pBufferDS->markerEventCount; ++iMarker)
    {
        if (pBufferDS->pMarkerEvents[iMarker] != NULL) {
            easyaudio_delete_event_dsound(pBufferDS->pMarkerEvents[iMarker]);
            pBufferDS->pMarkerEvents[iMarker] = NULL;
        }
    }

    pBufferDS->markerEventCount = 0;
}

bool easyaudio_register_marker_callback_dsound(easyaudio_buffer* pBuffer, unsigned int offsetInBytes, easyaudio_event_callback_proc callback, unsigned int eventID, void* pUserData)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    assert(pBufferDS->markerEventCount <= EASYAUDIO_MAX_MARKER_COUNT);

    if (pBufferDS->markerEventCount == EASYAUDIO_MAX_MARKER_COUNT) {
        // Too many markers.
        return false;
    }

    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pBuffer->pDevice->pContext;
    assert(pContextDS != NULL);

    easyaudio_event_dsound* pEvent = easyaudio_create_event_dsound(&pContextDS->eventManager, callback, pBuffer, eventID, pUserData);
    if (pEvent == NULL) {
        return false;
    }

    // easyaudio_create_event_dsound() will initialize the marker offset to 0, so we'll need to set it manually here.
    pEvent->markerOffset = offsetInBytes;

    pBufferDS->pMarkerEvents[pBufferDS->markerEventCount] = pEvent;
    pBufferDS->markerEventCount += 1;

    return true;
}

bool easyaudio_register_stop_callback_dsound(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    if (callback == NULL)
    {
        if (pBufferDS->pStopEvent != NULL) {
            easyaudio_delete_event_dsound(pBufferDS->pStopEvent);
            pBufferDS->pStopEvent = NULL;
        }

        return true;
    }
    else
    {
        easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pBuffer->pDevice->pContext;

        // If we already have a stop event, just replace the existing one.
        if (pBufferDS->pStopEvent != NULL) {
            easyaudio_update_event_dsound(pBufferDS->pStopEvent, callback, pUserData);
        } else {
            pBufferDS->pStopEvent = easyaudio_create_event_dsound(&pContextDS->eventManager, callback, pBuffer, EASYAUDIO_EVENT_ID_STOP, pUserData);
        }

        return pBufferDS->pStopEvent != NULL;
    }
}

bool easyaudio_register_pause_callback_dsound(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    
    if (callback == NULL)
    {
        if (pBufferDS->pPauseEvent != NULL) {
            easyaudio_delete_event_dsound(pBufferDS->pPauseEvent);
            pBufferDS->pPauseEvent = NULL;
        }

        return true;
    }
    else
    {
        easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pBuffer->pDevice->pContext;

        // If we already have a stop event, just replace the existing one.
        if (pBufferDS->pPauseEvent != NULL) {
            easyaudio_update_event_dsound(pBufferDS->pPauseEvent, callback, pUserData);
        } else {
            pBufferDS->pPauseEvent = easyaudio_create_event_dsound(&pContextDS->eventManager, callback, pBuffer, EASYAUDIO_EVENT_ID_PAUSE, pUserData);
        }

        return pBufferDS->pPauseEvent != NULL;
    }
}

bool easyaudio_register_play_callback_dsound(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    
    if (callback == NULL)
    {
        if (pBufferDS->pPlayEvent != NULL) {
            easyaudio_delete_event_dsound(pBufferDS->pPlayEvent);
            pBufferDS->pPlayEvent = NULL;
        }

        return true;
    }
    else
    {
        easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pBuffer->pDevice->pContext;

        // If we already have a stop event, just replace the existing one.
        if (pBufferDS->pPlayEvent != NULL) {
            easyaudio_update_event_dsound(pBufferDS->pPlayEvent, callback, pUserData);
        } else {
            pBufferDS->pPlayEvent = easyaudio_create_event_dsound(&pContextDS->eventManager, callback, pBuffer, EASYAUDIO_EVENT_ID_PLAY, pUserData);
        }

        return pBufferDS->pPlayEvent != NULL;
    }
}



void easyaudio_set_position_dsound(easyaudio_buffer* pBuffer, float x, float y, float z)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    if (pBufferDS->pDSBuffer3D != NULL) {
        IDirectSound3DBuffer_SetPosition(pBufferDS->pDSBuffer3D, x, y, z, DS3D_IMMEDIATE);
    }
}

void easyaudio_get_position_dsound(easyaudio_buffer* pBuffer, float* pPosOut)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    assert(pPosOut != NULL);

    if (pBufferDS->pDSBuffer3D != NULL)
    {
        D3DVECTOR pos;
        IDirectSound3DBuffer_GetPosition(pBufferDS->pDSBuffer3D, &pos);

        pPosOut[0] = pos.x;
        pPosOut[1] = pos.y;
        pPosOut[2] = pos.z;
    }
    else
    {
        pPosOut[0] = 0;
        pPosOut[1] = 1;
        pPosOut[2] = 2;
    }
}


void easyaudio_set_listener_position_dsound(easyaudio_device* pDevice, float x, float y, float z)
{
    easyaudio_device_dsound* pDeviceDS = (easyaudio_device_dsound*)pDevice;
    assert(pDeviceDS != NULL);

    IDirectSound3DListener_SetPosition(pDeviceDS->pDSListener, x, y, z, DS3D_IMMEDIATE);
}

void easyaudio_get_listener_position_dsound(easyaudio_device* pDevice, float* pPosOut)
{
    easyaudio_device_dsound* pDeviceDS = (easyaudio_device_dsound*)pDevice;
    assert(pDeviceDS != NULL);
    assert(pPosOut != NULL);

    D3DVECTOR pos;
    IDirectSound3DListener_GetPosition(pDeviceDS->pDSListener, &pos);

    pPosOut[0] = pos.x;
    pPosOut[1] = pos.y;
    pPosOut[2] = pos.z;
}


void easyaudio_set_listener_orientation_dsound(easyaudio_device* pDevice, float forwardX, float forwardY, float forwardZ, float upX, float upY, float upZ)
{
    easyaudio_device_dsound* pDeviceDS = (easyaudio_device_dsound*)pDevice;
    assert(pDeviceDS != NULL);

    IDirectSound3DListener_SetOrientation(pDeviceDS->pDSListener, forwardX, forwardY, forwardZ, upX, upY, upZ, DS3D_IMMEDIATE);
}

void easyaudio_get_listener_orientation_dsound(easyaudio_device* pDevice, float* pForwardOut, float* pUpOut)
{
    easyaudio_device_dsound* pDeviceDS = (easyaudio_device_dsound*)pDevice;
    assert(pDeviceDS != NULL);
    assert(pForwardOut != NULL);
    assert(pUpOut != NULL);

    D3DVECTOR forward;
    D3DVECTOR up;
    IDirectSound3DListener_GetOrientation(pDeviceDS->pDSListener, &forward, &up);

    pForwardOut[0] = forward.x;
    pForwardOut[1] = forward.y;
    pForwardOut[2] = forward.z;

    pUpOut[0] = up.x;
    pUpOut[1] = up.y;
    pUpOut[2] = up.z;
}

void easyaudio_set_3d_mode_dsound(easyaudio_buffer* pBuffer, easyaudio_3d_mode mode)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    if (pBufferDS->pDSBuffer3D == NULL) {
        return;
    }


    DWORD dwMode = DS3DMODE_NORMAL;
    if (mode == easyaudio_3d_mode_relative) {
        dwMode = DS3DMODE_HEADRELATIVE;
    } else if (mode == easyaudio_3d_mode_disabled) {
        dwMode = DS3DMODE_DISABLE;
    }

    IDirectSound3DBuffer_SetMode(pBufferDS->pDSBuffer3D, dwMode, DS3D_IMMEDIATE);
}

easyaudio_3d_mode easyaudio_get_3d_mode_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    if (pBufferDS->pDSBuffer3D == NULL) {
        return easyaudio_3d_mode_disabled;
    }


    DWORD dwMode;
    if (FAILED(IDirectSound3DBuffer_GetMode(pBufferDS->pDSBuffer3D, &dwMode))) {
        return easyaudio_3d_mode_disabled;
    }


    if (dwMode == DS3DMODE_NORMAL) {
        return easyaudio_3d_mode_absolute;
    }

    if (dwMode == DS3DMODE_HEADRELATIVE) {
        return easyaudio_3d_mode_relative;
    }

    return easyaudio_3d_mode_disabled;
}


static BOOL CALLBACK DSEnumCallback_OutputDevices(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext)
{
    // From MSDN:
    //
    // The first device enumerated is always called the Primary Sound Driver, and the lpGUID parameter of the callback is
    // NULL. This device represents the preferred output device set by the user in Control Panel.

    easyaudio_context_dsound* pContextDS = lpContext;
    assert(pContextDS != NULL);

    if (pContextDS->outputDeviceCount < EASYAUDIO_MAX_DEVICE_COUNT)
    {
        if (lpGuid != NULL) {
            memcpy(&pContextDS->outputDeviceInfo[pContextDS->outputDeviceCount].guid, lpGuid, sizeof(GUID));
        } else {
            memset(&pContextDS->outputDeviceInfo[pContextDS->outputDeviceCount].guid, 0, sizeof(GUID));
        }
        
        easyaudio_strcpy(pContextDS->outputDeviceInfo[pContextDS->outputDeviceCount].description, 256, lpcstrDescription);
        easyaudio_strcpy(pContextDS->outputDeviceInfo[pContextDS->outputDeviceCount].moduleName,  256, lpcstrModule);

        pContextDS->outputDeviceCount += 1;
        return TRUE;
    }
    else
    {
        // Ran out of device slots.
        return FALSE;
    }
}

static BOOL CALLBACK DSEnumCallback_InputDevices(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext)
{
    // From MSDN:
    //
    // The first device enumerated is always called the Primary Sound Driver, and the lpGUID parameter of the callback is
    // NULL. This device represents the preferred output device set by the user in Control Panel.

    easyaudio_context_dsound* pContextDS = lpContext;
    assert(pContextDS != NULL);

    if (pContextDS->inputDeviceCount < EASYAUDIO_MAX_DEVICE_COUNT)
    {
        if (lpGuid != NULL) {
            memcpy(&pContextDS->inputDeviceInfo[pContextDS->inputDeviceCount].guid, lpGuid, sizeof(GUID));
        } else {
            memset(&pContextDS->inputDeviceInfo[pContextDS->inputDeviceCount].guid, 0, sizeof(GUID));
        }

        easyaudio_strcpy(pContextDS->inputDeviceInfo[pContextDS->inputDeviceCount].description, 256, lpcstrDescription);
        easyaudio_strcpy(pContextDS->inputDeviceInfo[pContextDS->inputDeviceCount].moduleName,  256, lpcstrModule);

        pContextDS->inputDeviceCount += 1;
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
    if (pDirectSoundCreate8 == NULL){
        FreeLibrary(hDSoundDLL);
        return NULL;
    }

    pDirectSoundEnumerateAProc pDirectSoundEnumerateA = (pDirectSoundEnumerateAProc)GetProcAddress(hDSoundDLL, "DirectSoundEnumerateA");
    if (pDirectSoundEnumerateA == NULL){
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
        pContext->base.delete_context             = easyaudio_delete_context_dsound;
        pContext->base.create_output_device       = easyaudio_create_output_device_dsound;
        pContext->base.delete_output_device       = easyaudio_delete_output_device_dsound;
        pContext->base.get_output_device_count    = easyaudio_get_output_device_count_dsound;
        pContext->base.get_output_device_info     = easyaudio_get_output_device_info_dsound;
        pContext->base.create_buffer              = easyaudio_create_buffer_dsound;
        pContext->base.delete_buffer              = easyaudio_delete_buffer_dsound;
        pContext->base.get_buffer_extra_data_size = easyaudio_get_buffer_extra_data_size_dsound;
        pContext->base.get_buffer_extra_data      = easyaudio_get_buffer_extra_data_dsound;
        pContext->base.set_buffer_data            = easyaudio_set_buffer_data_dsound;
        pContext->base.play                       = easyaudio_play_dsound;
        pContext->base.pause                      = easyaudio_pause_dsound;
        pContext->base.stop                       = easyaudio_stop_dsound;
        pContext->base.get_playback_state         = easyaudio_get_playback_state_dsound;
        pContext->base.set_playback_position      = easyaudio_set_playback_position_dsound;
        pContext->base.get_playback_position      = easyaudio_get_playback_position_dsound;
        pContext->base.set_pan                    = easyaudio_set_pan_dsound;
        pContext->base.get_pan                    = easyaudio_get_pan_dsound;
        pContext->base.set_volume                 = easyaudio_set_volume_dsound;
        pContext->base.get_volume                 = easyaudio_get_volume_dsound;
        pContext->base.remove_markers             = easyaudio_remove_markers_dsound;
        pContext->base.register_marker_callback   = easyaudio_register_marker_callback_dsound;
        pContext->base.register_stop_callback     = easyaudio_register_stop_callback_dsound;
        pContext->base.register_pause_callback    = easyaudio_register_pause_callback_dsound;
        pContext->base.register_play_callback     = easyaudio_register_play_callback_dsound;
        pContext->base.set_position        = easyaudio_set_position_dsound;
        pContext->base.get_position        = easyaudio_get_position_dsound;
        pContext->base.set_listener_position      = easyaudio_set_listener_position_dsound;
        pContext->base.get_listener_position      = easyaudio_get_listener_position_dsound;
        pContext->base.set_listener_orientation   = easyaudio_set_listener_orientation_dsound;
        pContext->base.get_listener_orientation   = easyaudio_get_listener_orientation_dsound;
        pContext->base.set_3d_mode                = easyaudio_set_3d_mode_dsound;
        pContext->base.get_3d_mode                = easyaudio_get_3d_mode_dsound;

        pContext->hDSoundDLL                      = hDSoundDLL;
        pContext->pDirectSoundCreate8             = pDirectSoundCreate8;
        pContext->pDirectSoundEnumerateA          = pDirectSoundEnumerateA;
        pContext->pDirectSoundCaptureCreate8      = pDirectSoundCaptureCreate8;
        pContext->pDirectSoundCaptureEnumerateA   = pDirectSoundCaptureEnumerateA;

        // Enumerate output devices.
        pContext->outputDeviceCount = 0;
        pContext->pDirectSoundEnumerateA(DSEnumCallback_OutputDevices, pContext);

        // Enumerate input devices.
        pContext->inputDeviceCount = 0;
        pContext->pDirectSoundCaptureEnumerateA(DSEnumCallback_InputDevices, pContext);

        // The message queue and marker notification thread.
        if (!easyaudio_init_message_queue_dsound(&pContext->messageQueue) || !easyaudio_init_event_manager_dsound(&pContext->eventManager, &pContext->messageQueue))
        {
            // Failed to initialize the event manager.
            FreeLibrary(hDSoundDLL);
            free(pContext);

            return NULL;
        }
    }

    return (easyaudio_context*)pContext;
}
#endif  // !EASYAUDIO_BUILD_DSOUND


///////////////////////////////////////////////////////////////////////////////
//
// XAudio2
//
///////////////////////////////////////////////////////////////////////////////

#if 0
#define uuid(x)
#define DX_BUILD
#define INITGUID 1
#include <xaudio2.h>
#endif
