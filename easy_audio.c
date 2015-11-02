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



typedef void                     (* easyaudio_delete_context_proc)(easyaudio_context* pContext);
typedef easyaudio_device*        (* easyaudio_create_output_device_proc)(easyaudio_context* pContext, unsigned int deviceIndex);
typedef void                     (* easyaudio_delete_output_device_proc)(easyaudio_device* pDevice);
typedef unsigned int             (* easyaudio_get_output_device_count_proc)(easyaudio_context* pContext);
typedef bool                     (* easyaudio_get_output_device_info_proc)(easyaudio_context* pContext, unsigned int deviceIndex, easyaudio_device_info* pInfoOut);
typedef easyaudio_buffer*        (* easyaudio_create_buffer_proc)(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc);
typedef void                     (* easyaudio_delete_buffer_proc)(easyaudio_buffer* pBuffer);
typedef void                     (* easyaudio_set_buffer_data_proc)(easyaudio_buffer* pBuffer, unsigned int offset, const void* pData, unsigned int dataSizeInBytes);
typedef void                     (* easyaudio_play_proc)(easyaudio_buffer* pBuffer, bool loop);
typedef void                     (* easyaudio_pause_proc)(easyaudio_buffer* pBuffer);
typedef void                     (* easyaudio_stop_proc)(easyaudio_buffer* pBuffer);
typedef easyaudio_playback_state (* easyaudio_get_playback_state_proc)(easyaudio_buffer* pBuffer);
typedef void                     (* easyaudio_set_buffer_position_proc)(easyaudio_buffer* pBuffer, float x, float y, float z);
typedef void                     (* easyaudio_get_buffer_position_proc)(easyaudio_buffer* pBuffer, float* pPosOut);
typedef bool                     (* easyaudio_register_marker_callback_proc)(easyaudio_buffer* pBuffer, unsigned int offsetInBytes, easyaudio_event_callback_proc callback, unsigned int eventID, void* pUserData);
typedef bool                     (* easyaudio_register_stop_callback_proc)(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);
typedef bool                     (* easyaudio_register_pause_callback_proc)(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);
typedef bool                     (* easyaudio_register_play_callback_proc)(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData);
typedef void                     (* easyaudio_set_listener_position_proc)(easyaudio_device* pDevice, float x, float y, float z);
typedef void                     (* easyaudio_get_listener_position_proc)(easyaudio_device* pDevice, float* pPosOut);
typedef void                     (* easyaudio_set_listener_orientation_proc)(easyaudio_device* pDevice, float forwardX, float forwardY, float forwardZ, float upX, float upY, float upZ);
typedef void                     (* easyaudio_get_listener_orientation_proc)(easyaudio_device* pDevice, float* pForwardOut, float* pUpOut);

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
    easyaudio_set_buffer_data_proc set_buffer_data;
    easyaudio_play_proc play;
    easyaudio_pause_proc pause;
    easyaudio_stop_proc stop;
    easyaudio_get_playback_state_proc get_playback_state;
    easyaudio_set_buffer_position_proc set_buffer_position;
    easyaudio_get_buffer_position_proc get_buffer_position;
    easyaudio_register_marker_callback_proc register_marker_callback;
    easyaudio_register_stop_callback_proc register_stop_callback;
    easyaudio_register_pause_callback_proc register_pause_callback;
    easyaudio_register_play_callback_proc register_play_callback;
    easyaudio_set_listener_position_proc set_listener_position;
    easyaudio_get_listener_position_proc get_listener_position;
    easyaudio_set_listener_orientation_proc set_listener_orientation;
    easyaudio_get_listener_orientation_proc get_listener_orientation;
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

    return pContext->create_output_device(pContext, deviceIndex);
}

void easyaudio_delete_output_device(easyaudio_device* pDevice)
{
    if (pDevice == NULL) {
        return;
    }

    assert(pDevice->pContext != NULL);
    pDevice->pContext->delete_output_device(pDevice);
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


void easyaudio_set_buffer_position(easyaudio_buffer* pBuffer, float x, float y, float z)
{
    if (pBuffer == NULL) {
        return;
    }

    pBuffer->pDevice->pContext->set_buffer_position(pBuffer, x, y, z);
}

void easyaudio_get_buffer_position(easyaudio_buffer* pBuffer, float* pPosOut)
{
    if (pBuffer == NULL) {
        return;
    }

    if (pPosOut == NULL) {
        return;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    pBuffer->pDevice->pContext->get_buffer_position(pBuffer, pPosOut);
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

    if (easyaudio_get_playback_state(pBuffer) != easyaudio_stopped) {
        return false;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->register_stop_callback(pBuffer, callback, pUserData);
}

bool easyaudio_register_pause_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    if (pBuffer == NULL) {
        return false;
    }

    if (easyaudio_get_playback_state(pBuffer) != easyaudio_stopped) {
        return false;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->register_pause_callback(pBuffer, callback, pUserData);
}

bool easyaudio_register_play_callback(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    if (pBuffer == NULL) {
        return false;
    }

    if (easyaudio_get_playback_state(pBuffer) != easyaudio_stopped) {
        return false;
    }

    assert(pBuffer->pDevice != NULL);
    assert(pBuffer->pDevice->pContext != NULL);
    return pBuffer->pDevice->pContext->register_play_callback(pBuffer, callback, pUserData);
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


/// Deactivates (but does not delete) every event associated with the given buffer.
void ea_deactivate_buffer_events_dsound(easyaudio_buffer* pBuffer);


//// Event Management ////

typedef struct ea_event_manager_dsound ea_event_manager_dsound;
typedef struct ea_event_dsound ea_event_dsound;

struct ea_event_dsound
{
    /// A pointer to the event manager that owns this event.
    ea_event_manager_dsound* pEventManager;

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
    ea_event_dsound* pNextEvent;

    /// A pointer to the previous event.
    ea_event_dsound* pPrevEvent;
};

struct ea_event_manager_dsound
{
    /// A handle to the event worker thread.
    HANDLE hThread;

    /// A handle to the terminator event object.
    HANDLE hTerminateEvent;

    /// A handle to the refresher event object.
    HANDLE hRefreshEvent;

    /// The synchronization lock.
    HANDLE hLock;

    /// The semaphore to wait on when closing an event handle. This is released in the worker thread, and waited on by
    /// ea_close_win32_event_handle_dsound(). The initial value is 0.
    HANDLE hCloseEventSemaphore;

    /// The first event in a list.
    ea_event_dsound* pFirstEvent;

    /// The last event in the list of events.
    ea_event_dsound* pLastEvent;

};


/// Locks the event manager.
void ea_lock_events_dsound(ea_event_manager_dsound* pEventManager)
{
    WaitForSingleObject(pEventManager->hLock, INFINITE);
}

/// Unlocks the event manager.
void ea_unlock_events_dsound(ea_event_manager_dsound* pEventManager)
{
    SetEvent(pEventManager->hLock);
}


/// Removes the given event from the event lists.
///
/// @remarks
///     This will be used when moving the event to a new location in the list or when it is being deleted.
void ea_remove_event_dsound_nolock(ea_event_dsound* pEvent)
{
    assert(pEvent != NULL);
    
    ea_event_manager_dsound* pEventManager = pEvent->pEventManager;
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

/// @copydoc ea_remove_event_dsound_nolock()
void ea_remove_event_dsound(ea_event_dsound* pEvent)
{
    assert(pEvent != NULL);

    ea_event_manager_dsound* pEventManager = pEvent->pEventManager;
    ea_lock_events_dsound(pEventManager);
    {
        ea_remove_event_dsound_nolock(pEvent);
    }
    ea_unlock_events_dsound(pEventManager);
}

/// Adds the given event to the end of the internal list.
void ea_append_event_dsound(ea_event_dsound* pEvent)
{
    assert(pEvent != NULL);

    ea_event_manager_dsound* pEventManager = pEvent->pEventManager;
    ea_lock_events_dsound(pEventManager);
    {
        ea_remove_event_dsound_nolock(pEvent);

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
    ea_unlock_events_dsound(pEventManager);
}

/// Adds the given event to the start of the internal list.
void ea_prepend_event_dsound(ea_event_dsound* pEvent)
{
    assert(pEvent != NULL);

    ea_event_manager_dsound* pEventManager = pEvent->pEventManager;
    ea_lock_events_dsound(pEventManager);
    {
        ea_remove_event_dsound_nolock(pEvent);
        
        assert(pEvent->pNextEvent == NULL);

        if (pEventManager->pFirstEvent != NULL) {
            pEvent->pNextEvent = pEventManager->pFirstEvent;
            pEvent->pNextEvent->pPrevEvent = pEvent;
        }

        if (pEventManager->pLastEvent == NULL) {
            pEventManager->pLastEvent = pEvent;
        }

        pEventManager->pFirstEvent = pEvent;
    }
    ea_unlock_events_dsound(pEventManager);
}


/// Closes the Win32 event handle of the given event.
void ea_close_win32_event_handle_dsound(ea_event_dsound* pEvent)
{
    // We need to ensure the event object is not getting waited on by WaitForMultipleObjects() in the worker thread. To do this, we just
    // signal the event which will cause WaitForMultipleObjects() to return. The problem now, however, is that just because we signaled
    // the event it doesn't mean we can just assume WaitForMultipleObjects() has immediately returned. The worker thread will release a
    // semaphore to let other threads know that it has returned from WaitForMultipleObjects().
    //
    // The worker thread also needs to know that an event has been marked for deletion. We do this by having everything set to NULL in
    // the event object. When the worker thread sees this, it assumes an event is being signaled due to it being deleted.
    assert(pEvent != NULL);
    assert(pEvent->pEventManager != NULL);

    ea_lock_events_dsound(pEvent->pEventManager);
    {
        assert(pEvent->hEvent != NULL);

        HANDLE hEventToClose = pEvent->hEvent;
        pEvent->hEvent = NULL;

        // Signal the event to force WaitForMultipleObjects() in the worker thread to return.
        SetEvent(hEventToClose);

        // Wait for the worker thread to release the semaphore. Once this returns we are free to close the event handle.
        WaitForSingleObject(pEvent->pEventManager->hCloseEventSemaphore, INFINITE);

        // At this point we can assume the worker thread is not waiting on the event so we can now close it.
        CloseHandle(hEventToClose);
    }
    ea_unlock_events_dsound(pEvent->pEventManager);
}


/// Updates the given event to use the given callback and user data.
void ea_update_event_dsound(ea_event_dsound* pEvent, easyaudio_event_callback_proc callback, void* pUserData)
{
    assert(pEvent != NULL);

    pEvent->callback  = callback;
    pEvent->pUserData = pUserData;

    // When an event has changed, we need to let the worker thread know that it needs to refresh it's own local state. The
    // problem is that it is probably already waiting on other events with WaitForMultipleObjects(). Thus, we need to signal
    // a special event to force WaitForMultipleObjects() to return and request the worker thread to refresh it's local list
    // of event objects to wait on.
    SetEvent(pEvent->pEventManager->hRefreshEvent);
}

/// Creates a new event, but does not activate it.
///
/// @remarks
///     When an event is created, it just sits dormant and will never be triggered until it has been
///     activated with ea_activate_event_dsound().
ea_event_dsound* ea_create_event_dsound(ea_event_manager_dsound* pEventManager, easyaudio_event_callback_proc callback, easyaudio_buffer* pBuffer, unsigned int eventID, void* pUserData)
{
    ea_event_dsound* pEvent = malloc(sizeof(ea_event_dsound));
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
        ea_append_event_dsound(pEvent);

        // This roundabout way of setting the callback and user data is to ensure the worker thread is made aware that it needs
        // to refresh it's local event data.
        ea_update_event_dsound(pEvent, callback, pUserData);
    }

    return pEvent;
}

/// Deletes an event, and deactivates it.
///
/// @remarks
///     This will not return until the event has been deleted completely.
void ea_delete_event_dsound(ea_event_dsound* pEvent)
{
    assert(pEvent != NULL);

    // We assume the event has been removed from all internal lists by a higher level function.
    assert(pEvent->pNextEvent == NULL);
    
    // Close the Win32 event handle first.
    if (pEvent->hEvent != NULL) {
        ea_close_win32_event_handle_dsound(pEvent);
    }

    // Remove the event from the list.
    ea_remove_event_dsound(pEvent);

    // At this point everything has been closed so we can safely free the memory and return.
    free(pEvent);
}


/// Gathers the event handles and callback data for all of the relevant buffer events.
///
/// @remarks
///     Note that this creates copies of the events.
unsigned int ea_gather_events_dsound(ea_event_manager_dsound *pEventManager, HANDLE* pHandlesOut, ea_event_dsound* pEventsOut, unsigned int outputBufferSize)
{
    assert(pEventManager != NULL);
    assert(pHandlesOut != NULL);
    assert(pEventsOut != NULL);
    assert(outputBufferSize >= 2);

    pHandlesOut[0] = pEventManager->hTerminateEvent;
    memset(pEventsOut + 0, 0, sizeof(ea_event_dsound));

    pHandlesOut[1] = pEventManager->hRefreshEvent;
    memset(pEventsOut + 1, 0, sizeof(ea_event_dsound));


    unsigned int i = 2;
    ea_event_dsound* pEvent = pEventManager->pFirstEvent;
    while (i < outputBufferSize && pEvent != NULL)
    {
        assert(pEvent->hEvent != NULL);

        pHandlesOut[i] = pEvent->hEvent;
        memcpy(pEventsOut + i, pEvent, sizeof(*pEvent));

        i += 1;
        pEvent = pEvent->pNextEvent;
    }

    return i;
}



/// The entry point to the event worker thread.
DWORD WINAPI DSound_EventWorkerThreadProc(ea_event_manager_dsound *pEventManager)
{
    if (pEventManager != NULL)
    {
        HANDLE hTerminateEvent = pEventManager->hTerminateEvent;
        HANDLE hRefreshEvent   = pEventManager->hRefreshEvent;

        HANDLE eventHandles[1024];
        ea_event_dsound events[1024];
        unsigned int eventCount = 0;

        bool needsRefresh = true;
        for (;;)
        {
            if (needsRefresh) {
                eventCount = ea_gather_events_dsound(pEventManager, eventHandles, events, 1024);
                needsRefresh = false;
            }

            DWORD rc = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);
            if (rc >= WAIT_OBJECT_0 && rc < eventCount)
            {
                const unsigned int eventIndex = rc - WAIT_OBJECT_0;
                HANDLE hEvent = eventHandles[eventIndex];

                if (hEvent == hTerminateEvent)
                {
                    // The terminator event was signaled. We just return from the thread immediately.
                    printf("TERMINATED\n");
                    return 0;
                }

                if (hEvent == hRefreshEvent)
                {
                    // This event will get signaled when a new set of events need to be waited on, such as when a new event has been registered on a buffer.
                    needsRefresh = true;
                    continue;
                }


                // If we get here if means we have hit a callback event.
                ea_event_dsound* pEvent = &events[eventIndex];
                if (pEvent->callback != NULL)
                {
                    assert(pEvent->hEvent == hEvent);

                    // The stop event will be signaled by DirectSound when IDirectSoundBuffer::Stop() is called. The problem is that we need to call that when the
                    // sound is paused as well. Thus, we need to check if we got the stop event, and if so DON'T call the callback function if it is in a non-stopped
                    // state.
                    bool isStopEventButNotStopped = pEvent->eventID == EASYAUDIO_EVENT_ID_STOP && easyaudio_get_playback_state(pEvent->pBuffer) != easyaudio_stopped;
                    if (!isStopEventButNotStopped) {
                        pEvent->callback(pEvent->pBuffer, pEvent->eventID, pEvent->pUserData);
                    }
                }


                if (pEvent->hEvent == NULL)
                {
                    // The event's Win32 event handle has been set to NULL which is our cue that the event is wanting to be deleted. The other thread is
                    // waiting for this through the use of a semaphore, so we need to release it here.
                    ReleaseSemaphore(pEventManager->hCloseEventSemaphore, 1, NULL);
                    needsRefresh = true;
                }
            }
        }
    }

    return 0;
}


/// Initializes the event manager by creating the thread and event objects.
bool ea_init_event_manager_dsound(ea_event_manager_dsound* pEventManager)
{
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

    HANDLE hLock = CreateEventA(NULL, FALSE, TRUE,  NULL);
    if (hLock == NULL)
    {
        CloseHandle(hTerminateEvent);
        CloseHandle(hRefreshEvent);
        return false;
    }

    HANDLE hCloseEventSemaphore = CreateSemaphoreA(NULL, 0, 1, NULL);
    if (hCloseEventSemaphore == NULL)
    {
        CloseHandle(hTerminateEvent);
        CloseHandle(hRefreshEvent);
        CloseHandle(hLock);
        return false;
    }


    HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)DSound_EventWorkerThreadProc, pEventManager, 0, NULL);
    if (hThread == NULL)
    {
        CloseHandle(hTerminateEvent);
        CloseHandle(hRefreshEvent);
        CloseHandle(hLock);
        CloseHandle(hCloseEventSemaphore);
        return false;
    }


    pEventManager->hTerminateEvent      = hTerminateEvent;
    pEventManager->hRefreshEvent        = hRefreshEvent;
    pEventManager->hLock                = hLock;
    pEventManager->hCloseEventSemaphore = hCloseEventSemaphore;
    pEventManager->hThread              = hThread;

    pEventManager->pFirstEvent = NULL;
    pEventManager->pLastEvent  = NULL;

    return true;
}

/// Shuts down the event manager by closing the thread and event objects.
///
/// @remarks
///     This does not return until the worker thread has been terminated completely.
///     @par
///     This will delete every event, so any pointers to events will be made invalid upon calling this function.
void ea_uninit_event_manager_dsound(ea_event_manager_dsound* pEventManager)
{
    assert(pEventManager != NULL);


    // Cleanly delete every event first.
    while (pEventManager->pFirstEvent != NULL) {
        ea_delete_event_dsound(pEventManager->pFirstEvent);
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


    // Delete the lock.
    CloseHandle(pEventManager->hLock);
    pEventManager->hLock = NULL;
}


//// End Event Management ////


static GUID g_DSListenerGUID           = {0x279AFA84, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60};
static GUID g_DirectSoundBuffer8GUID   = {0x6825a449, 0x7524, 0x4d82, 0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e};
static GUID g_DirectSound3DBuffer8GUID = {0x279AFA86, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60};
static GUID g_DirectSoundNotifyGUID    = {0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16};

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
    ea_event_manager_dsound eventManager;

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
    ea_event_dsound* pMarkerEvents[EASYAUDIO_MAX_MARKER_COUNT];

    /// The event to trigger when the sound is stopped.
    ea_event_dsound* pStopEvent;

    /// The event to trigger when the sound is paused.
    ea_event_dsound* pPauseEvent;

    /// The event to trigger when the sound is played or resumed.
    ea_event_dsound* pPlayEvent;

} easyaudio_buffer_dsound;


void ea_activate_buffer_events_dsound(easyaudio_buffer* pBuffer)
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


    HRESULT hr = pBufferDS->pDSNotify->lpVtbl->SetNotificationPositions(pBufferDS->pDSNotify, dwPositionNotifies, n);
    if (FAILED(hr)) {
        printf("WARNING: FAILED TO CREATE DIRECTSOUND NOTIFIERS\n");
    }
}

void ea_deactivate_buffer_events_dsound(easyaudio_buffer* pBuffer)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);


    HRESULT hr = pBufferDS->pDSNotify->lpVtbl->SetNotificationPositions(pBufferDS->pDSNotify, 0, NULL);
    if (FAILED(hr)) {
        printf("WARNING: FAILED TO CLEAR DIRECTSOUND NOTIFIERS\n");
    }
}


void easyaudio_delete_context_dsound(easyaudio_context* pContext)
{
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pContext;
    assert(pContextDS != NULL);

    ea_uninit_event_manager_dsound(&pContextDS->eventManager);

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

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(WAVEFORMATEX));
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = 2;
    wfx.nSamplesPerSec  = 44100;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = (wfx.wBitsPerSample / 8 * wfx.nChannels);
    wfx.nAvgBytesPerSec = (wfx.nSamplesPerSec * wfx.nBlockAlign);
    hr = IDirectSoundBuffer_SetFormat(pDSPrimaryBuffer, &wfx);
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

    IDirectSound3DListener_Release(pDeviceDS->pDSListener);
    IDirectSoundBuffer_Release(pDeviceDS->pDSPrimaryBuffer);
    IDirectSound_Release(pDeviceDS->pDS);
    free(pDeviceDS);
}


easyaudio_buffer* easyaudio_create_buffer_dsound(easyaudio_device* pDevice, easyaudio_buffer_desc* pBufferDesc)
{
    easyaudio_device_dsound* pDeviceDS = (easyaudio_device_dsound*)pDevice;
    assert(pDeviceDS != NULL);
    assert(pBufferDesc != NULL);

    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pDevice->pContext;
    assert(pContextDS != NULL);


    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.cbSize          = sizeof(wf);
    wf.nChannels       = pBufferDesc->channels;
    wf.nSamplesPerSec  = pBufferDesc->sampleRate;
    wf.wBitsPerSample  = pBufferDesc->bitsPerSample;
    wf.nBlockAlign     = (wf.nChannels * wf.wBitsPerSample) / 8;
    wf.nAvgBytesPerSec = wf.nBlockAlign * pBufferDesc->sampleRate;

    if (pBufferDesc->format == easyaudio_format_pcm) {
        wf.wFormatTag = WAVE_FORMAT_PCM;
    } else if (pBufferDesc->format == easyaudio_format_float) {
        wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
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
    descDS.lpwfxFormat     = &wf;

    LPDIRECTSOUNDBUFFER8   pDSBuffer   = NULL;
    LPDIRECTSOUND3DBUFFER8 pDSBuffer3D = NULL;
    if (wf.nChannels > 1)
    {
        // 3D Disabled.
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


        hr = pDSBuffer->lpVtbl->QueryInterface(pDSBuffer, &g_DirectSound3DBuffer8GUID, &pDSBuffer3D);
        if (FAILED(hr)) {
            return NULL;
        }

        pDSBuffer3D->lpVtbl->SetPosition(pDSBuffer3D, 0, 0, 0, DS3D_IMMEDIATE);
    }



    // We need to create a notification object so we can notify the host application when the playback buffer hits a certain point.
    LPDIRECTSOUNDNOTIFY8 pDSNotify;
    HRESULT hr = pDSBuffer->lpVtbl->QueryInterface(pDSBuffer, &g_DirectSoundNotifyGUID, &pDSNotify);
    if (FAILED(hr)) {
        IDirectSound3DBuffer_Release(pDSBuffer3D);
        IDirectSoundBuffer8_Release(pDSBuffer);
        return NULL;
    }


    easyaudio_buffer_dsound* pBufferDS = malloc(sizeof(easyaudio_buffer_dsound));
    if (pBufferDS == NULL) {
        IDirectSound3DBuffer_Release(pDSBuffer3D);
        IDirectSoundBuffer8_Release(pDSBuffer);
        return NULL;
    }

    pBufferDS->base.pDevice     = pDevice;
    pBufferDS->pDSBuffer        = pDSBuffer;
    pBufferDS->pDSBuffer3D      = pDSBuffer3D;
    pBufferDS->pDSNotify        = pDSNotify;
    pBufferDS->playbackState    = easyaudio_stopped;

    pBufferDS->markerEventCount = 0;
    memset(pBufferDS->pMarkerEvents, 0, sizeof(pBufferDS->pMarkerEvents));
    pBufferDS->pStopEvent       = NULL;
    pBufferDS->pPauseEvent      = NULL;
    pBufferDS->pPlayEvent       = NULL;


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


    if (pBufferDS->pDSBuffer3D != NULL) {
        IDirectSound3DBuffer_Release(pBufferDS->pDSBuffer3D);
    }

    if (pBufferDS->pDSBuffer != NULL) {
        IDirectSoundBuffer8_Release(pBufferDS->pDSBuffer);
    }

    free(pBufferDS);
}

bool easyaudio_set_buffer_data_dsound(easyaudio_buffer* pBuffer, unsigned int offset, const void* pData, unsigned int dataSizeInBytes)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    assert(pData != NULL);

    LPVOID lpvWrite;
    DWORD dwLength;
    HRESULT hr = IDirectSoundBuffer8_Lock(pBufferDS->pDSBuffer, offset, dataSizeInBytes, &lpvWrite, &dwLength, NULL, NULL, DSBLOCK_ENTIREBUFFER);
    if (FAILED(hr)) {
        return false;
    }

    assert(dwLength == dataSizeInBytes);
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

    if (pBufferDS->playbackState == easyaudio_playing) {
        return;
    }


    // Events need to be activated.
    if (pBufferDS->playbackState == easyaudio_stopped) {
        ea_activate_buffer_events_dsound(pBuffer);
    }


    DWORD dwFlags = 0;
    if (loop) {
        dwFlags |= DSBPLAY_LOOPING;
    }

    pBufferDS->playbackState = easyaudio_playing;
    IDirectSoundBuffer8_Play(pBufferDS->pDSBuffer, 0, 0, dwFlags);

    // If we have a play event we need to signal the event which will cause the worker thread to call the callback function.
    if (pBufferDS->pPlayEvent != NULL) {
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


void easyaudio_set_buffer_position_dsound(easyaudio_buffer* pBuffer, float x, float y, float z)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    if (pBufferDS->pDSBuffer3D != NULL) {
        IDirectSound3DBuffer_SetPosition(pBufferDS->pDSBuffer3D, x, y, z, DS3D_IMMEDIATE);
    }
}

void easyaudio_get_buffer_position_dsound(easyaudio_buffer* pBuffer, float* pPosOut)
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

    ea_event_dsound* pEvent = ea_create_event_dsound(&pContextDS->eventManager, callback, pBuffer, eventID, pUserData);
    if (pEvent == NULL) {
        return false;
    }

    // ea_create_event_dsound() will initialize the marker offset to 0, so we'll need to set it manually here.
    pEvent->markerOffset = offsetInBytes;

    pBufferDS->pMarkerEvents[pBufferDS->markerEventCount] = pEvent;
    pBufferDS->markerEventCount += 1;

    return true;
}

bool easyaudio_register_stop_callback_dsound(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);

    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pBuffer->pDevice->pContext;

    // If we already have a stop event, just replace the existing one.
    if (pBufferDS->pStopEvent != NULL) {
        ea_update_event_dsound(pBufferDS->pStopEvent, callback, pUserData);
    } else {
        pBufferDS->pStopEvent = ea_create_event_dsound(&pContextDS->eventManager, callback, pBuffer, EASYAUDIO_EVENT_ID_STOP, pUserData);
    }

    return pBufferDS->pStopEvent != NULL;
}

bool easyaudio_register_pause_callback_dsound(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pBuffer->pDevice->pContext;

    // If we already have a stop event, just replace the existing one.
    if (pBufferDS->pPauseEvent != NULL) {
        ea_update_event_dsound(pBufferDS->pPauseEvent, callback, pUserData);
    } else {
        pBufferDS->pPauseEvent = ea_create_event_dsound(&pContextDS->eventManager, callback, pBuffer, EASYAUDIO_EVENT_ID_PAUSE, pUserData);
    }

    return pBufferDS->pPauseEvent != NULL;
}

bool easyaudio_register_play_callback_dsound(easyaudio_buffer* pBuffer, easyaudio_event_callback_proc callback, void* pUserData)
{
    easyaudio_buffer_dsound* pBufferDS = (easyaudio_buffer_dsound*)pBuffer;
    assert(pBufferDS != NULL);
    
    easyaudio_context_dsound* pContextDS = (easyaudio_context_dsound*)pBuffer->pDevice->pContext;

    // If we already have a stop event, just replace the existing one.
    if (pBufferDS->pPlayEvent != NULL) {
        ea_update_event_dsound(pBufferDS->pPlayEvent, callback, pUserData);
    } else {
        pBufferDS->pPlayEvent = ea_create_event_dsound(&pContextDS->eventManager, callback, pBuffer, EASYAUDIO_EVENT_ID_PLAY, pUserData);
    }

    return pBufferDS->pPlayEvent != NULL;
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
        pContext->base.delete_context           = easyaudio_delete_context_dsound;
        pContext->base.create_output_device     = easyaudio_create_output_device_dsound;
        pContext->base.delete_output_device     = easyaudio_delete_output_device_dsound;
        pContext->base.get_output_device_count  = easyaudio_get_output_device_count_dsound;
        pContext->base.get_output_device_info   = easyaudio_get_output_device_info_dsound;
        pContext->base.create_buffer            = easyaudio_create_buffer_dsound;
        pContext->base.delete_buffer            = easyaudio_delete_buffer_dsound;
        pContext->base.set_buffer_data          = easyaudio_set_buffer_data_dsound;
        pContext->base.play                     = easyaudio_play_dsound;
        pContext->base.pause                    = easyaudio_pause_dsound;
        pContext->base.stop                     = easyaudio_stop_dsound;
        pContext->base.get_playback_state       = easyaudio_get_playback_state_dsound;
        pContext->base.set_buffer_position      = easyaudio_set_buffer_position_dsound;
        pContext->base.get_buffer_position      = easyaudio_get_buffer_position_dsound;
        pContext->base.register_marker_callback = easyaudio_register_marker_callback_dsound;
        pContext->base.register_stop_callback   = easyaudio_register_stop_callback_dsound;
        pContext->base.register_pause_callback  = easyaudio_register_pause_callback_dsound;
        pContext->base.register_play_callback   = easyaudio_register_play_callback_dsound;
        pContext->base.set_listener_position    = easyaudio_set_listener_position_dsound;
        pContext->base.get_listener_position    = easyaudio_get_listener_position_dsound;
        pContext->base.set_listener_orientation = easyaudio_set_listener_orientation_dsound;
        pContext->base.get_listener_orientation = easyaudio_get_listener_orientation_dsound;

        pContext->hDSoundDLL                    = hDSoundDLL;
        pContext->pDirectSoundCreate8           = pDirectSoundCreate8;
        pContext->pDirectSoundEnumerateA        = pDirectSoundEnumerateA;
        pContext->pDirectSoundCaptureCreate8    = pDirectSoundCaptureCreate8;
        pContext->pDirectSoundCaptureEnumerateA = pDirectSoundCaptureEnumerateA;

        // Enumerate output devices.
        pContext->outputDeviceCount = 0;
        pContext->pDirectSoundEnumerateA(DSEnumCallback_OutputDevices, pContext);

        // Enumerate input devices.
        pContext->inputDeviceCount = 0;
        pContext->pDirectSoundCaptureEnumerateA(DSEnumCallback_InputDevices, pContext);


        // The event manager.
        if (!ea_init_event_manager_dsound(&pContext->eventManager))
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
