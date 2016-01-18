// Public Domain. See "unlicense" statement at the end of this file.

#include "easy_gui.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include <stdio.h>  // For testing. Delete Me.


/////////////////////////////////////////////////////////////////
//
// PRIVATE CORE API
//
/////////////////////////////////////////////////////////////////


// Context Flags
#define IS_CONTEXT_DEAD                     (1U << 0)
#define IS_AUTO_DIRTY_DISABLED              (1U << 1)

// Element Flags
#define IS_ELEMENT_HIDDEN                   (1U << 0)
#define IS_ELEMENT_CLIPPING_DISABLED        (1U << 1)
#define IS_ELEMENT_DEAD                     (1U << 31)


/// Increments the inbound event counter
///
/// @remarks
///     This is called from every easygui_post_inbound_event_*() function and is used to keep track of whether or
///     not an inbound event is being processed. We need to track this because if we are in the middle of event
///     processing and an element is deleted, we want to delay it's deletion until the end of the event processing.
///     @par
///     Use easygui_end_inbound_event() to decrement the counter.
void easygui_begin_inbound_event(easygui_context* pContext);

/// Decrements the inbound event counter.
///
/// @remarks
///     This is called from every easygui_post_inbound_event_*() function.
///     @par
///     When the internal counter reaches zero, deleted elements will be garbage collected.
void easygui_end_inbound_event(easygui_context* pContext);

/// Determines whether or not inbound events are being processed.
///
/// @remarks
///     This is used to determine whether or not an element can be deleted immediately or should be garbage collected
///     at the end of event processing.
bool easygui_is_handling_inbound_event(const easygui_context* pContext);


/// Increments the outbound event counter.
///
/// @remarks
///     This will validate that the given element is allowed to have an event posted. When false is returned, nothing
///     will have been locked and the outbound event should be cancelled.
///     @par
///     This will return false if the given element has been marked as dead, or if there is some other reason it should
///     not be receiving events.
bool easygui_begin_outbound_event(easygui_element* pElement);

/// Decrements the outbound event counter.
void easygui_end_outbound_event(easygui_element* pElement);

/// Determines whether or not and outbound event is being processed.
bool easygui_is_handling_outbound_event(easygui_context* pContext);


/// Marks the given element as dead.
void easygui_mark_element_as_dead(easygui_element* pElement);

/// Determines whether or not the given element is marked as dead.
bool easygui_is_element_marked_as_dead(const easygui_element* pElement);

/// Deletes every element that has been marked as dead.
void easygui_delete_elements_marked_as_dead(easygui_context* pContext);


/// Marks the given context as deleted.
void easygui_mark_context_as_dead(easygui_context* pContext);

/// Determines whether or not the given context is marked as dead.
bool easygui_is_context_marked_as_dead(const easygui_context* pContext);


/// Deletes the given context for real.
///
/// If a context is deleted during the processing of an inbound event it will not be deleting immediately - this
/// will delete the context for real.
void easygui_delete_context_for_real(easygui_context* pContext);

/// Deletes the given element for real.
///
/// Sometimes an element will not be deleted straight away but instead just marked as dead. We use this to delete
/// the given element for real.
void easygui_delete_element_for_real(easygui_element* pElement);


/// Orphans the given element without triggering a redraw of the parent nor the child.
void easygui_detach_without_redraw(easygui_element* pChildElement);

/// Appends the given element without first detaching it from the old parent, nor does it post a redraw.
void easygui_append_without_detach_or_redraw(easygui_element* pChildElement, easygui_element* pParentElement);

/// Appends the given element without first detaching it from the old parent.
void easygui_append_without_detach(easygui_element* pChildElement, easygui_element* pParentElement);

/// Prepends the given element without first detaching it from the old parent, nor does it post a redraw.
void easygui_prepend_without_detach_or_redraw(easygui_element* pChildElement, easygui_element* pParentElement);

/// Prepends the given element without first detaching it from the old parent.
void easygui_prepend_without_detach(easygui_element* pChildElement, easygui_element* pParentElement);

/// Appends an element to another as it's sibling, but does not detach it from the previous parent nor trigger a redraw.
void easygui_append_sibling_without_detach_or_redraw(easygui_element* pElementToAppend, easygui_element* pElementToAppendTo);

/// Appends an element to another as it's sibling, but does not detach it from the previous parent.
void easygui_append_sibling_without_detach(easygui_element* pElementToAppend, easygui_element* pElementToAppendTo);

/// Prepends an element to another as it's sibling, but does not detach it from the previous parent nor trigger a redraw.
void easygui_prepend_sibling_without_detach_or_redraw(easygui_element* pElementToPrepend, easygui_element* pElementToPrependTo);

/// Prepends an element to another as it's sibling, but does not detach it from the previous parent.
void easygui_prepends_sibling_without_detach(easygui_element* pElementToPrepend, easygui_element* pElementToPrependTo);


/// Begins accumulating an invalidation rectangle.
void easygui_begin_auto_dirty(easygui_element* pElement, easygui_rect relativeRect);

/// Ends accumulating the invalidation rectangle and posts on_dirty is auto-dirty is enabled.
void easygui_end_auto_dirty(easygui_element* pElement);

/// Marks the given region of the given top level element as dirty, but only if automatic dirtying is enabled.
///
/// @remarks
///     This is equivalent to easygui_begin_auto_dirty() immediately followed by easygui_end_auto_dirty().
void easygui_auto_dirty(easygui_element* pTopLevelElement, easygui_rect rect);


/// Recursively applies the given offset to the absolute positions of the children of the given element.
///
/// @remarks
///     This is called when the absolute position of an element is changed.
void easygui_apply_offset_to_children_recursive(easygui_element* pParentElement, float offsetX, float offsetY);


/// The function to call when the mouse may have entered into a new element.
void easygui_update_mouse_enter_and_leave_state(easygui_context* pContext, easygui_element* pNewElementUnderMouse);


/// Functions for posting outbound events.
void easygui_post_outbound_event_move(easygui_element* pElement, float newRelativePosX, float newRelativePosY);
void easygui_post_outbound_event_size(easygui_element* pElement, float newWidth, float newHeight);
void easygui_post_outbound_event_mouse_enter(easygui_element* pElement);
void easygui_post_outbound_event_mouse_leave(easygui_element* pElement);
void easygui_post_outbound_event_mouse_move(easygui_element* pElement, int relativeMousePosX, int relativeMousePosY);
void easygui_post_outbound_event_mouse_button_down(easygui_element* pElement, int mouseButton, int relativeMousePosX, int relativeMousePosY);
void easygui_post_outbound_event_mouse_button_up(easygui_element* pElement, int mouseButton, int relativeMousePosX, int relativeMousePosY);
void easygui_post_outbound_event_mouse_button_dblclick(easygui_element* pElement, int mouseButton, int relativeMousePosX, int relativeMousePosY);
void easygui_post_outbound_event_mouse_wheel(easygui_element* pElement, int delta, int relativeMousePosX, int relativeMousePosY);
void easygui_post_outbound_event_key_down(easygui_element* pElement, easygui_key key, unsigned int stateFlags);
void easygui_post_outbound_event_key_up(easygui_element* pElement, easygui_key key, unsigned int stateFlags);
void easygui_post_outbound_event_printable_key_down(easygui_element* pElement, unsigned int character, unsigned int stateFlags);
void easygui_post_outbound_event_dirty(easygui_element* pElement, easygui_rect relativeRect);
void easygui_post_outbound_event_dirty_global(easygui_element* pElement, easygui_rect relativeRect);
void easygui_post_outbound_event_capture_mouse(easygui_element* pElement);
void easygui_post_outbound_event_capture_mouse_global(easygui_element* pElement);
void easygui_post_outbound_event_release_mouse(easygui_element* pElement);
void easygui_post_outbound_event_release_mouse_global(easygui_element* pElement);
void easygui_post_outbound_event_capture_keyboard(easygui_element* pElement);
void easygui_post_outbound_event_capture_keyboard_global(easygui_element* pElement);
void easygui_post_outbound_event_release_keyboard(easygui_element* pElement);
void easygui_post_outbound_event_release_keyboard_global(easygui_element* pElement);

/// Posts a log message.
void easygui_log(easygui_context* pContext, const char* message);

/// Retrieves the internal font that would be used when scaled by the given amount.
///
/// @remarks
///     If an internal font of the appropriate size has not yet been created, this function will create it.
PRIVATE easygui_resource easygui_get_internal_font_by_scale(easygui_font* pFont, float scaleY);


void easygui_begin_inbound_event(easygui_context* pContext)
{
    assert(pContext != NULL);

    pContext->inboundEventCounter += 1;
}

void easygui_end_inbound_event(easygui_context* pContext)
{
    assert(pContext != NULL);
    assert(pContext->inboundEventCounter > 0);

    pContext->inboundEventCounter -= 1;


    // Here is where we want to clean up any elements that are marked as dead. When events are being handled elements are not deleted
    // immediately but instead only marked for deletion. This function will be called at the end of event processing which makes it
    // an appropriate place for cleaning up dead elements.
    if (!easygui_is_handling_inbound_event(pContext))
    {
        easygui_delete_elements_marked_as_dead(pContext);

        // If the context has been marked for deletion than we will need to delete that too.
        if (easygui_is_context_marked_as_dead(pContext))
        {
            easygui_delete_context_for_real(pContext);
        }
    }
}

bool easygui_is_handling_inbound_event(const easygui_context* pContext)
{
    assert(pContext != NULL);

    return pContext->inboundEventCounter > 0;
}



bool easygui_begin_outbound_event(easygui_element* pElement)
{
    assert(pElement != NULL);
    assert(pElement->pContext != NULL);


    // We want to cancel the outbound event if the element is marked as dead.
    if (easygui_is_element_marked_as_dead(pElement)) {
        easygui_log(pElement->pContext, "WARNING: Attemping to post an event to an element that is marked for deletion.");
        return false;
    }


    // At this point everything should be fine so we just increment the count (which should never go above 1) and return true.
    pElement->pContext->outboundEventLockCounter += 1;

    return true;
}

void easygui_end_outbound_event(easygui_element* pElement)
{
    assert(pElement != NULL);
    assert(pElement->pContext != NULL);
    assert(pElement->pContext->outboundEventLockCounter > 0);

    pElement->pContext->outboundEventLockCounter -= 1;
}

bool easygui_is_handling_outbound_event(easygui_context* pContext)
{
    assert(pContext != NULL);
    return pContext->outboundEventLockCounter > 0;
}


void easygui_mark_element_as_dead(easygui_element* pElement)
{
    assert(pElement != NULL);
    assert(pElement->pContext != NULL);

    pElement->flags |= IS_ELEMENT_DEAD;


    if (pElement->pContext->pFirstDeadElement != NULL) {
        pElement->pNextDeadElement = pElement->pContext->pFirstDeadElement;
    }

    pElement->pContext->pFirstDeadElement = pElement;
}

bool easygui_is_element_marked_as_dead(const easygui_element* pElement)
{
    assert(pElement != NULL);

    return (pElement->flags & IS_ELEMENT_DEAD) != 0;
}

void easygui_delete_elements_marked_as_dead(easygui_context* pContext)
{
    assert(pContext != NULL);

    while (pContext->pFirstDeadElement != NULL)
    {
        easygui_element* pDeadElement = pContext->pFirstDeadElement;
        pContext->pFirstDeadElement = pContext->pFirstDeadElement->pNextDeadElement;

        easygui_delete_element_for_real(pDeadElement);
    }
}


void easygui_mark_context_as_dead(easygui_context* pContext)
{
    assert(pContext != NULL);
    assert(!easygui_is_context_marked_as_dead(pContext));

    pContext->flags |= IS_CONTEXT_DEAD;
}

bool easygui_is_context_marked_as_dead(const easygui_context* pContext)
{
    assert(pContext != NULL);

    return (pContext->flags & IS_CONTEXT_DEAD) != 0;
}



void easygui_delete_context_for_real(easygui_context* pContext)
{
    assert(pContext != NULL);

    // All elements marked as dead need to be deleted.
    easygui_delete_elements_marked_as_dead(pContext);

    free(pContext);
}

void easygui_delete_element_for_real(easygui_element* pElement)
{
    assert(pElement != NULL);

    free(pElement);
}


void easygui_detach_without_redraw(easygui_element* pElement)
{
    if (pElement->pParent != NULL) {
        if (pElement->pParent->pFirstChild == pElement) {
            pElement->pParent->pFirstChild = pElement->pNextSibling;
        }

        if (pElement->pParent->pLastChild == pElement) {
            pElement->pParent->pLastChild = pElement->pPrevSibling;
        }


        if (pElement->pPrevSibling != NULL) {
            pElement->pPrevSibling->pNextSibling = pElement->pNextSibling;
        }

        if (pElement->pNextSibling != NULL) {
            pElement->pNextSibling->pPrevSibling = pElement->pPrevSibling;
        }
    }

    pElement->pParent      = NULL;
    pElement->pPrevSibling = NULL;
    pElement->pNextSibling = NULL;
}

void easygui_append_without_detach_or_redraw(easygui_element* pChildElement, easygui_element* pParentElement)
{
    pChildElement->pParent = pParentElement;
    if (pChildElement->pParent != NULL) {
        if (pChildElement->pParent->pLastChild != NULL) {
            pChildElement->pPrevSibling = pChildElement->pParent->pLastChild;
            pChildElement->pPrevSibling->pNextSibling = pChildElement;
        }

        if (pChildElement->pParent->pFirstChild == NULL) {
            pChildElement->pParent->pFirstChild = pChildElement;
        }

        pChildElement->pParent->pLastChild = pChildElement;
    }
}

void easygui_append_without_detach(easygui_element* pChildElement, easygui_element* pParentElement)
{
    easygui_append_without_detach_or_redraw(pChildElement, pParentElement);
    easygui_auto_dirty(pChildElement, easygui_make_rect(0, 0, pChildElement->width, pChildElement->height));
}

void easygui_prepend_without_detach_or_redraw(easygui_element* pChildElement, easygui_element* pParentElement)
{
    pChildElement->pParent = pParentElement;
    if (pChildElement->pParent != NULL) {
        if (pChildElement->pParent->pFirstChild != NULL) {
            pChildElement->pNextSibling = pChildElement->pParent->pFirstChild;
            pChildElement->pNextSibling->pPrevSibling = pChildElement;
        }

        if (pChildElement->pParent->pLastChild == NULL) {
            pChildElement->pParent->pLastChild = pChildElement;
        }

        pChildElement->pParent->pFirstChild = pChildElement;
    }
}

void easygui_prepend_without_detach(easygui_element* pChildElement, easygui_element* pParentElement)
{
    easygui_prepend_without_detach_or_redraw(pChildElement, pParentElement);
    easygui_auto_dirty(pChildElement, easygui_make_rect(0, 0, pChildElement->width, pChildElement->height));
}

void easygui_append_sibling_without_detach_or_redraw(easygui_element* pElementToAppend, easygui_element* pElementToAppendTo)
{
    assert(pElementToAppend   != NULL);
    assert(pElementToAppendTo != NULL);

    pElementToAppend->pParent = pElementToAppendTo->pParent;
    if (pElementToAppend->pParent != NULL)
    {
        pElementToAppend->pNextSibling = pElementToAppendTo->pNextSibling;
        pElementToAppend->pPrevSibling = pElementToAppendTo;

        pElementToAppendTo->pNextSibling->pPrevSibling = pElementToAppend;
        pElementToAppendTo->pNextSibling = pElementToAppend;

        if (pElementToAppend->pParent->pLastChild == pElementToAppendTo) {
            pElementToAppend->pParent->pLastChild = pElementToAppend;
        }
    }
}

void easygui_append_sibling_without_detach(easygui_element* pElementToAppend, easygui_element* pElementToAppendTo)
{
    easygui_append_sibling_without_detach_or_redraw(pElementToAppend, pElementToAppendTo);
    easygui_auto_dirty(pElementToAppend, easygui_make_rect(0, 0, pElementToAppend->width, pElementToAppend->height));
}

void easygui_prepend_sibling_without_detach_or_redraw(easygui_element* pElementToPrepend, easygui_element* pElementToPrependTo)
{
    assert(pElementToPrepend   != NULL);
    assert(pElementToPrependTo != NULL);

    pElementToPrepend->pParent = pElementToPrependTo->pParent;
    if (pElementToPrepend->pParent != NULL)
    {
        pElementToPrepend->pPrevSibling = pElementToPrependTo->pNextSibling;
        pElementToPrepend->pNextSibling = pElementToPrependTo;

        pElementToPrependTo->pPrevSibling->pNextSibling = pElementToPrepend;
        pElementToPrependTo->pNextSibling = pElementToPrepend;

        if (pElementToPrepend->pParent->pFirstChild == pElementToPrependTo) {
            pElementToPrepend->pParent->pFirstChild = pElementToPrepend;
        }
    }
}

void easygui_prepend_sibling_without_detach(easygui_element* pElementToPrepend, easygui_element* pElementToPrependTo)
{
    easygui_prepend_sibling_without_detach_or_redraw(pElementToPrepend, pElementToPrependTo);
    easygui_auto_dirty(pElementToPrepend, easygui_make_rect(0, 0, pElementToPrepend->width, pElementToPrepend->height));
}


void easygui_begin_auto_dirty(easygui_element* pElement, easygui_rect relativeRect)
{
    assert(pElement           != NULL);
    assert(pElement->pContext != NULL);

    if (easygui_is_auto_dirty_enabled(pElement->pContext))
    {
        easygui_context* pContext = pElement->pContext;

        if (pContext->pDirtyTopLevelElement == NULL) {
            pContext->pDirtyTopLevelElement = easygui_find_top_level_element(pElement);
        }

        assert(pContext->pDirtyTopLevelElement == easygui_find_top_level_element(pElement));


        pContext->dirtyRect = easygui_rect_union(pContext->dirtyRect, easygui_make_rect_absolute(pElement, &relativeRect));
        pContext->dirtyCounter += 1;
    }
}

void easygui_end_auto_dirty(easygui_element* pElement)
{
    assert(pElement           != NULL);
    assert(pElement->pContext != NULL);

    easygui_context* pContext = pElement->pContext;
    if (easygui_is_auto_dirty_enabled(pContext))
    {
        assert(pContext->pDirtyTopLevelElement != NULL);
        assert(pContext->dirtyCounter > 0);

        pContext->dirtyCounter -= 1;
        if (pContext->dirtyCounter == 0)
        {
            easygui_dirty(pContext->pDirtyTopLevelElement, pContext->dirtyRect);

            pContext->pDirtyTopLevelElement = NULL;
            pContext->dirtyRect             = easygui_make_inside_out_rect();
        }
    }
}

void easygui_auto_dirty(easygui_element* pElement, easygui_rect relativeRect)
{
    assert(pElement != NULL);
    assert(pElement->pContext != NULL);

    if (easygui_is_auto_dirty_enabled(pElement->pContext)) {
        easygui_begin_auto_dirty(pElement, relativeRect);
        easygui_end_auto_dirty(pElement);
    }
}


void easygui_apply_offset_to_children_recursive(easygui_element* pParentElement, float offsetX, float offsetY)
{
    assert(pParentElement != NULL);

    for (easygui_element* pChild = pParentElement->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling)
    {
        easygui_begin_auto_dirty(pParentElement, easygui_get_local_rect(pParentElement));
        {
            pChild->absolutePosX += offsetX;
            pChild->absolutePosY += offsetY;

            easygui_apply_offset_to_children_recursive(pChild, offsetX, offsetY);
        }
        easygui_end_auto_dirty(pParentElement);
    }
}

PRIVATE void easygui_post_on_mouse_leave_recursive(easygui_context* pContext, easygui_element* pNewElementUnderMouse, easygui_element* pOldElementUnderMouse)
{
    easygui_element* pOldAncestor = pOldElementUnderMouse;
    while (pOldAncestor != NULL)
    {
        bool isOldElementUnderMouse = pNewElementUnderMouse == pOldAncestor || easygui_is_ancestor(pOldAncestor, pNewElementUnderMouse);
        if (!isOldElementUnderMouse)
        {
            easygui_post_outbound_event_mouse_leave(pOldAncestor);
        }

        pOldAncestor = pOldAncestor->pParent;
    }
}

PRIVATE void easygui_post_on_mouse_enter_recursive(easygui_context* pContext, easygui_element* pNewElementUnderMouse, easygui_element* pOldElementUnderMouse)
{
    if (pNewElementUnderMouse == NULL) {
        return;
    }


    if (pNewElementUnderMouse->pParent != NULL) {
        easygui_post_on_mouse_enter_recursive(pContext, pNewElementUnderMouse->pParent, pOldElementUnderMouse);
    }

    bool wasNewElementUnderMouse = pOldElementUnderMouse == pNewElementUnderMouse || easygui_is_ancestor(pNewElementUnderMouse, pOldElementUnderMouse);
    if (!wasNewElementUnderMouse)
    {
        easygui_post_outbound_event_mouse_enter(pNewElementUnderMouse);
    }
}

void easygui_update_mouse_enter_and_leave_state(easygui_context* pContext, easygui_element* pNewElementUnderMouse)
{
    if (pContext == NULL) {
        return;
    }

    easygui_element* pOldElementUnderMouse = pContext->pElementUnderMouse;
    if (pOldElementUnderMouse != pNewElementUnderMouse)
    {
        // We don't change the enter and leave state if an element is capturing the mouse.
        if (pContext->pElementWithMouseCapture == NULL)
        {
            pContext->pElementUnderMouse = pNewElementUnderMouse;


            // The the event handlers below, remember that ancestors are considered hovered if a descendant is the element under the mouse.

            // on_mouse_leave
            easygui_post_on_mouse_leave_recursive(pContext, pNewElementUnderMouse, pOldElementUnderMouse);

            // on_mouse_enter
            easygui_post_on_mouse_enter_recursive(pContext, pNewElementUnderMouse, pOldElementUnderMouse);
        }
    }
}


void easygui_post_outbound_event_move(easygui_element* pElement, float newRelativePosX, float newRelativePosY)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onMove) {
            pElement->onMove(pElement, newRelativePosX, newRelativePosY);
        }

        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_size(easygui_element* pElement, float newWidth, float newHeight)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onSize) {
            pElement->onSize(pElement, newWidth, newHeight);
        }

        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_mouse_enter(easygui_element* pElement)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onMouseEnter) {
            pElement->onMouseEnter(pElement);
        }

        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_mouse_leave(easygui_element* pElement)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onMouseLeave) {
            pElement->onMouseLeave(pElement);
        }

        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_mouse_move(easygui_element* pElement, int relativeMousePosX, int relativeMousePosY)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onMouseMove)
        {
            float scaleX;
            float scaleY;
            easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

            pElement->onMouseMove(pElement, (int)(relativeMousePosX / scaleX), (int)(relativeMousePosY / scaleY));
        }
        
        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_mouse_button_down(easygui_element* pElement, int mouseButton, int relativeMousePosX, int relativeMousePosY)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onMouseButtonDown)
        {
            float scaleX;
            float scaleY;
            easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

            pElement->onMouseButtonDown(pElement, mouseButton, (int)(relativeMousePosX / scaleX), (int)(relativeMousePosY / scaleY));
        }
        
        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_mouse_button_up(easygui_element* pElement, int mouseButton, int relativeMousePosX, int relativeMousePosY)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onMouseButtonUp)
        {
            float scaleX;
            float scaleY;
            easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

            pElement->onMouseButtonUp(pElement, mouseButton, (int)(relativeMousePosX / scaleX), (int)(relativeMousePosY / scaleY));
        }
        
        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_mouse_button_dblclick(easygui_element* pElement, int mouseButton, int relativeMousePosX, int relativeMousePosY)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onMouseButtonDblClick)
        {
            float scaleX;
            float scaleY;
            easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

            pElement->onMouseButtonDblClick(pElement, mouseButton, (int)(relativeMousePosX / scaleX), (int)(relativeMousePosY / scaleY));
        }
        
        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_mouse_wheel(easygui_element* pElement, int delta, int relativeMousePosX, int relativeMousePosY)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onMouseWheel)
        {
            float scaleX;
            float scaleY;
            easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

            pElement->onMouseWheel(pElement, delta, (int)(relativeMousePosX / scaleX), (int)(relativeMousePosY / scaleY));
        }
        
        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_key_down(easygui_element* pElement, easygui_key key, unsigned int stateFlags)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onKeyDown) {
            pElement->onKeyDown(pElement, key, stateFlags);
        }
        
        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_key_up(easygui_element* pElement, easygui_key key, unsigned int stateFlags)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onKeyUp) {
            pElement->onKeyUp(pElement, key, stateFlags);
        }
        
        easygui_end_outbound_event(pElement);
    }
}

void easygui_post_outbound_event_printable_key_down(easygui_element* pElement, unsigned int character, unsigned int stateFlags)
{
    if (easygui_begin_outbound_event(pElement))
    {
        if (pElement->onPrintableKeyDown) {
            pElement->onPrintableKeyDown(pElement, character, stateFlags);
        }
        
        easygui_end_outbound_event(pElement);
    }
}


void easygui_post_outbound_event_dirty(easygui_element* pElement, easygui_rect relativeRect)
{
    if (pElement != NULL)
    {
        if (pElement->onDirty) {
            pElement->onDirty(pElement, relativeRect);
        }
    }
}

void easygui_post_outbound_event_dirty_global(easygui_element* pElement, easygui_rect relativeRect)
{
    if (pElement != NULL && pElement->pContext != NULL)
    {
        if (pElement->pContext->onGlobalDirty)
        {
            float scaleX;
            float scaleY;
            easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

            pElement->pContext->onGlobalDirty(pElement, easygui_scale_rect(relativeRect, scaleX, scaleY));
        }
    }
}

void easygui_post_outbound_event_capture_mouse(easygui_element* pElement)
{
    if (pElement != NULL)
    {
        if (pElement->onCaptureMouse) {
            pElement->onCaptureMouse(pElement);
        }
    }
}

void easygui_post_outbound_event_capture_mouse_global(easygui_element* pElement)
{
    if (pElement != NULL && pElement->pContext != NULL)
    {
        if (pElement->pContext->onGlobalCaptureMouse) {
            pElement->pContext->onGlobalCaptureMouse(pElement);
        }
    }
}

void easygui_post_outbound_event_release_mouse(easygui_element* pElement)
{
    if (pElement != NULL)
    {
        if (pElement->onReleaseMouse) {
            pElement->onReleaseMouse(pElement);
        }
    }
}

void easygui_post_outbound_event_release_mouse_global(easygui_element* pElement)
{
    if (pElement != NULL && pElement->pContext != NULL)
    {
        if (pElement->pContext->onGlobalReleaseMouse) {
            pElement->pContext->onGlobalReleaseMouse(pElement);
        }
    }
}


void easygui_post_outbound_event_capture_keyboard(easygui_element* pElement)
{
    if (pElement != NULL)
    {
        if (pElement->onCaptureKeyboard) {
            pElement->onCaptureKeyboard(pElement);
        }
    }
}

void easygui_post_outbound_event_capture_keyboard_global(easygui_element* pElement)
{
    if (pElement != NULL && pElement->pContext != NULL)
    {
        if (pElement->pContext->onGlobalCaptureKeyboard) {
            pElement->pContext->onGlobalCaptureKeyboard(pElement);
        }
    }
}

void easygui_post_outbound_event_release_keyboard(easygui_element* pElement)
{
    if (pElement != NULL)
    {
        if (pElement->onReleaseKeyboard) {
            pElement->onReleaseKeyboard(pElement);
        }
    }
}

void easygui_post_outbound_event_release_keyboard_global(easygui_element* pElement)
{
    if (pElement != NULL && pElement->pContext != NULL)
    {
        if (pElement->pContext->onGlobalReleaseKeyboard) {
            pElement->pContext->onGlobalReleaseKeyboard(pElement);
        }
    }
}


void easygui_log(easygui_context* pContext, const char* message)
{
    if (pContext != NULL)
    {
        if (pContext->onLog) {
            pContext->onLog(pContext, message);
        }
    }
}


PRIVATE easygui_resource easygui_get_internal_font_by_scale(easygui_font* pFont, float scaleY)
{
    assert(pFont != NULL);
    assert(pFont->pContext != NULL);

    if (pFont->pContext->paintingCallbacks.getFontSize == NULL)
    {
        if (pFont->internalFontCount == 0) {
            return NULL;
        }

        return pFont->pInternalFonts[0];
    }
    

    // First check to see if a font of the appropriate size has already been created.
    unsigned int targetSize = (unsigned int)(pFont->size * scaleY);
    if (targetSize == 0) {
        targetSize = 1;
    }

    for (unsigned int i = 0; i < pFont->internalFontCount; ++i)
    {
        if (pFont->pContext->paintingCallbacks.getFontSize(pFont->pInternalFonts[i]) == targetSize) {
            return pFont->pInternalFonts[i];
        }
    }


    // At this point we know that a font of the appropriate size has not yet been loaded, so we need to try and load it now.
    if (pFont->pContext->paintingCallbacks.createFont == NULL) {
        return NULL;
    }

    easygui_resource internalFont = pFont->pContext->paintingCallbacks.createFont(pFont->pContext->pPaintingContext, pFont->family, targetSize, pFont->weight, pFont->slant, pFont->rotation);
    if (internalFont == NULL) {
        return NULL;
    }

    easygui_resource* pOldInternalFonts = pFont->pInternalFonts;
    easygui_resource* pNewInternalFonts = malloc(sizeof(*pNewInternalFonts) * (pFont->internalFontCount + 1));

    for (size_t i = 0; i < pFont->internalFontCount; ++i) {
        pNewInternalFonts[i] = pOldInternalFonts[i];
    }
    pNewInternalFonts[pFont->internalFontCount] = internalFont;

    pFont->pInternalFonts     = pNewInternalFonts;
    pFont->internalFontCount += 1;


    free(pOldInternalFonts);
    return internalFont;
}



/////////////////////////////////////////////////////////////////
//
// CORE API
//
/////////////////////////////////////////////////////////////////

easygui_context* easygui_create_context()
{
    easygui_context* pContext = malloc(sizeof(easygui_context));
    if (pContext != NULL) {
        pContext->pPaintingContext                           = NULL;
        pContext->paintingCallbacks.drawBegin                = NULL;
        pContext->paintingCallbacks.drawEnd                  = NULL;
        pContext->paintingCallbacks.setClip                  = NULL;
        pContext->paintingCallbacks.getClip                  = NULL;
        pContext->paintingCallbacks.drawLine                 = NULL;
        pContext->paintingCallbacks.drawRect                 = NULL;
        pContext->paintingCallbacks.drawRectOutline          = NULL;
        pContext->paintingCallbacks.drawRectWithOutline      = NULL;
        pContext->paintingCallbacks.drawRoundRect            = NULL;
        pContext->paintingCallbacks.drawRoundRectOutline     = NULL;
        pContext->paintingCallbacks.drawRoundRectWithOutline = NULL;
        pContext->paintingCallbacks.drawText                 = NULL;
        pContext->paintingCallbacks.drawImage                = NULL;
        pContext->paintingCallbacks.createFont               = NULL;
        pContext->paintingCallbacks.deleteFont               = NULL;
        pContext->paintingCallbacks.getFontSize              = NULL;
        pContext->paintingCallbacks.getFontMetrics           = NULL;
        pContext->paintingCallbacks.getGlyphMetrics          = NULL;
        pContext->paintingCallbacks.measureString            = NULL;
        pContext->paintingCallbacks.createImage              = NULL;
        pContext->paintingCallbacks.deleteImage              = NULL;
        pContext->paintingCallbacks.getImageSize             = NULL;
        pContext->inboundEventCounter                        = 0;
        pContext->outboundEventLockCounter                   = 0;
        pContext->pFirstDeadElement                          = NULL;
        pContext->pElementUnderMouse                         = NULL;
        pContext->pElementWithMouseCapture                   = NULL;
        pContext->pElementWithKeyboardCapture                = NULL;
        pContext->flags                                      = 0;
        pContext->onGlobalDirty                              = NULL;
        pContext->onGlobalCaptureMouse                       = NULL;
        pContext->onGlobalReleaseMouse                       = NULL;
        pContext->onGlobalCaptureKeyboard                    = NULL;
        pContext->onGlobalReleaseKeyboard                    = NULL;
        pContext->onLog                                      = NULL;
        pContext->pLastMouseMoveTopLevelElement              = NULL;
        pContext->lastMouseMovePosX                          = 0;
        pContext->lastMouseMovePosY                          = 0;
        pContext->pDirtyTopLevelElement                      = NULL;
        pContext->dirtyRect                                  = easygui_make_inside_out_rect();
        pContext->dirtyCounter                               = 0;
    }

    return pContext;
}

void easygui_delete_context(easygui_context* pContext)
{
    if (pContext == NULL) {
        return;
    }


    // Make sure the mouse capture is released.
    if (pContext->pElementWithMouseCapture != NULL)
    {
        easygui_log(pContext, "WARNING: Deleting the GUI context while an element still has the mouse capture.");
        easygui_release_mouse(pContext);
    }

    // Make sure the keyboard capture is released.
    if (pContext->pElementWithKeyboardCapture != NULL)
    {
        easygui_log(pContext, "WARNING: Deleting the GUI context while an element still has the keyboard capture.");
        easygui_release_keyboard(pContext);
    }


    if (easygui_is_handling_inbound_event(pContext))
    {
        // An inbound event is still being processed - we don't want to delete the context straight away because we can't
        // trust external event handlers to not try to access the context later on. To do this we just set the flag that
        // the context is deleted. It will then be deleted for real at the end of the inbound event handler.
        easygui_mark_context_as_dead(pContext);
    }
    else
    {
        // An inbound event is not being processed, so delete the context straight away.
        easygui_delete_context_for_real(pContext);
    }
}



/////////////////////////////////////////////////////////////////
// Events

void easygui_post_inbound_event_mouse_leave(easygui_element* pTopLevelElement)
{
    if (pTopLevelElement == NULL) {
        return;
    }

    easygui_context* pContext = pTopLevelElement->pContext;
    if (pContext == NULL) {
        return;
    }

    easygui_begin_inbound_event(pContext);
    {
        // We assume that was previously under the mouse was either pTopLevelElement itself or one of it's descendants.
        easygui_update_mouse_enter_and_leave_state(pContext, NULL);
    }
    easygui_end_inbound_event(pContext);
}

void easygui_post_inbound_event_mouse_move(easygui_element* pTopLevelElement, int mousePosX, int mousePosY)
{
    if (pTopLevelElement == NULL || pTopLevelElement->pContext == NULL) {
        return;
    }


    easygui_begin_inbound_event(pTopLevelElement->pContext);
    {
        /// A pointer to the top level element that was passed in from the last inbound mouse move event.
        pTopLevelElement->pContext->pLastMouseMoveTopLevelElement = pTopLevelElement;

        /// The position of the mouse that was passed in from the last inbound mouse move event.
        pTopLevelElement->pContext->lastMouseMovePosX = (float)mousePosX;
        pTopLevelElement->pContext->lastMouseMovePosY = (float)mousePosY;



        // The first thing we need to do is find the new element that's sitting under the mouse.
        easygui_element* pNewElementUnderMouse = easygui_find_element_under_point(pTopLevelElement, (float)mousePosX, (float)mousePosY);

        // Now that we know which element is sitting under the mouse we need to check if the mouse has entered into a new element.
        easygui_update_mouse_enter_and_leave_state(pTopLevelElement->pContext, pNewElementUnderMouse);


        easygui_element* pEventReceiver = pTopLevelElement->pContext->pElementWithMouseCapture;
        if (pEventReceiver == NULL)
        {
            pEventReceiver = pNewElementUnderMouse;
        }

        if (pEventReceiver != NULL)
        {
            float relativeMousePosX = (float)mousePosX;
            float relativeMousePosY = (float)mousePosY;
            easygui_make_point_relative(pEventReceiver, &relativeMousePosX, &relativeMousePosY);

            easygui_post_outbound_event_mouse_move(pEventReceiver, (int)relativeMousePosX, (int)relativeMousePosY);
        }
    }
    easygui_end_inbound_event(pTopLevelElement->pContext);
}

void easygui_post_inbound_event_mouse_button_down(easygui_element* pTopLevelElement, int mouseButton, int mousePosX, int mousePosY)
{
    if (pTopLevelElement == NULL || pTopLevelElement->pContext == NULL) {
        return;
    }

    easygui_context* pContext = pTopLevelElement->pContext;
    easygui_begin_inbound_event(pContext);
    {
        easygui_element* pEventReceiver = pContext->pElementWithMouseCapture;
        if (pEventReceiver == NULL)
        {
            pEventReceiver = pContext->pElementUnderMouse;

            if (pEventReceiver == NULL)
            {
                // We'll get here if this message is posted without a prior mouse move event.
                pEventReceiver = easygui_find_element_under_point(pTopLevelElement, (float)mousePosX, (float)mousePosY);
            }
        }


        if (pEventReceiver != NULL)
        {
            float relativeMousePosX = (float)mousePosX;
            float relativeMousePosY = (float)mousePosY;
            easygui_make_point_relative(pEventReceiver, &relativeMousePosX, &relativeMousePosY);

            easygui_post_outbound_event_mouse_button_down(pEventReceiver, mouseButton, (int)relativeMousePosX, (int)relativeMousePosY);
        }
    }
    easygui_end_inbound_event(pContext);
}

void easygui_post_inbound_event_mouse_button_up(easygui_element* pTopLevelElement, int mouseButton, int mousePosX, int mousePosY)
{
    if (pTopLevelElement == NULL || pTopLevelElement->pContext == NULL) {
        return;
    }

    easygui_context* pContext = pTopLevelElement->pContext;
    easygui_begin_inbound_event(pContext);
    {
        easygui_element* pEventReceiver = pContext->pElementWithMouseCapture;
        if (pEventReceiver == NULL)
        {
            pEventReceiver = pContext->pElementUnderMouse;

            if (pEventReceiver == NULL)
            {
                // We'll get here if this message is posted without a prior mouse move event.
                pEventReceiver = easygui_find_element_under_point(pTopLevelElement, (float)mousePosX, (float)mousePosY);
            }
        }


        if (pEventReceiver != NULL)
        {
            float relativeMousePosX = (float)mousePosX;
            float relativeMousePosY = (float)mousePosY;
            easygui_make_point_relative(pEventReceiver, &relativeMousePosX, &relativeMousePosY);

            easygui_post_outbound_event_mouse_button_up(pEventReceiver, mouseButton, (int)relativeMousePosX, (int)relativeMousePosY);
        }
    }
    easygui_end_inbound_event(pContext);
}

void easygui_post_inbound_event_mouse_button_dblclick(easygui_element* pTopLevelElement, int mouseButton, int mousePosX, int mousePosY)
{
    if (pTopLevelElement == NULL || pTopLevelElement->pContext == NULL) {
        return;
    }

    easygui_context* pContext = pTopLevelElement->pContext;
    easygui_begin_inbound_event(pContext);
    {
        easygui_element* pEventReceiver = pContext->pElementWithMouseCapture;
        if (pEventReceiver == NULL)
        {
            pEventReceiver = pContext->pElementUnderMouse;

            if (pEventReceiver == NULL)
            {
                // We'll get here if this message is posted without a prior mouse move event.
                pEventReceiver = easygui_find_element_under_point(pTopLevelElement, (float)mousePosX, (float)mousePosY);
            }
        }


        if (pEventReceiver != NULL)
        {
            float relativeMousePosX = (float)mousePosX;
            float relativeMousePosY = (float)mousePosY;
            easygui_make_point_relative(pEventReceiver, &relativeMousePosX, &relativeMousePosY);

            easygui_post_outbound_event_mouse_button_dblclick(pEventReceiver, mouseButton, (int)relativeMousePosX, (int)relativeMousePosY);
        }
    }
    easygui_end_inbound_event(pContext);
}

void easygui_post_inbound_event_mouse_wheel(easygui_element* pTopLevelElement, int delta, int mousePosX, int mousePosY)
{
    if (pTopLevelElement == NULL || pTopLevelElement->pContext == NULL) {
        return;
    }

    easygui_context* pContext = pTopLevelElement->pContext;
    easygui_begin_inbound_event(pContext);
    {
        easygui_element* pEventReceiver = pContext->pElementWithMouseCapture;
        if (pEventReceiver == NULL)
        {
            pEventReceiver = pContext->pElementUnderMouse;

            if (pEventReceiver == NULL)
            {
                // We'll get here if this message is posted without a prior mouse move event.
                pEventReceiver = easygui_find_element_under_point(pTopLevelElement, (float)mousePosX, (float)mousePosY);
            }
        }


        if (pEventReceiver != NULL)
        {
            float relativeMousePosX = (float)mousePosX;
            float relativeMousePosY = (float)mousePosY;
            easygui_make_point_relative(pEventReceiver, &relativeMousePosX, &relativeMousePosY);

            easygui_post_outbound_event_mouse_wheel(pEventReceiver, delta, (int)relativeMousePosX, (int)relativeMousePosY);
        }
    }
    easygui_end_inbound_event(pContext);
}

void easygui_post_inbound_event_key_down(easygui_context* pContext, easygui_key key, unsigned int stateFlags)
{
    if (pContext == NULL) {
        return;
    }

    easygui_begin_inbound_event(pContext);
    {
        if (pContext->pElementWithKeyboardCapture != NULL) {
            easygui_post_outbound_event_key_down(pContext->pElementWithKeyboardCapture, key, stateFlags);
        }
    }
    easygui_end_inbound_event(pContext);
}

void easygui_post_inbound_event_key_up(easygui_context* pContext, easygui_key key, unsigned int stateFlags)
{
    if (pContext == NULL) {
        return;
    }

    easygui_begin_inbound_event(pContext);
    {
        if (pContext->pElementWithKeyboardCapture != NULL) {
            easygui_post_outbound_event_key_up(pContext->pElementWithKeyboardCapture, key, stateFlags);
        }
    }
    easygui_end_inbound_event(pContext);
}

void easygui_post_inbound_event_printable_key_down(easygui_context* pContext, unsigned int character, unsigned int stateFlags)
{
    if (pContext == NULL) {
        return;
    }

    easygui_begin_inbound_event(pContext);
    {
        if (pContext->pElementWithKeyboardCapture != NULL) {
            easygui_post_outbound_event_printable_key_down(pContext->pElementWithKeyboardCapture, character, stateFlags);
        }
    }
    easygui_end_inbound_event(pContext);
}



void easygui_set_global_on_dirty(easygui_context * pContext, easygui_on_dirty_proc onDirty)
{
    if (pContext != NULL) {
        pContext->onGlobalDirty = onDirty;
    }
}

void easygui_set_global_on_capture_mouse(easygui_context* pContext, easygui_on_capture_mouse_proc onCaptureMouse)
{
    if (pContext != NULL) {
        pContext->onGlobalCaptureMouse = onCaptureMouse;
    }
}

void easygui_set_global_on_release_mouse(easygui_context* pContext, easygui_on_release_mouse_proc onReleaseMouse)
{
    if (pContext != NULL) {
        pContext->onGlobalReleaseMouse = onReleaseMouse;
    }
}

void easygui_set_global_on_capture_keyboard(easygui_context* pContext, easygui_on_capture_keyboard_proc onCaptureKeyboard)
{
    if (pContext != NULL) {
        pContext->onGlobalCaptureKeyboard = onCaptureKeyboard;
    }
}

void easygui_set_global_on_release_keyboard(easygui_context* pContext, easygui_on_capture_keyboard_proc onReleaseKeyboard)
{
    if (pContext != NULL) {
        pContext->onGlobalReleaseKeyboard = onReleaseKeyboard;
    }
}

void easygui_set_on_log(easygui_context* pContext, easygui_on_log onLog)
{
    if (pContext != NULL) {
        pContext->onLog = onLog;
    }
}



/////////////////////////////////////////////////////////////////
// Elements

easygui_element* easygui_create_element(easygui_context* pContext, easygui_element* pParent, size_t extraDataSize)
{
    if (pContext != NULL)
    {
        easygui_element* pElement = (easygui_element*)malloc(sizeof(easygui_element) - sizeof(pElement->pExtraData) + extraDataSize);
        if (pElement != NULL)
        {
            pElement->pContext              = pContext;
            pElement->pParent               = pParent;
            pElement->pFirstChild           = NULL;
            pElement->pLastChild            = NULL;
            pElement->pNextSibling          = NULL;
            pElement->pPrevSibling          = NULL;
            pElement->pNextDeadElement      = NULL;
            pElement->absolutePosX          = 0;
            pElement->absolutePosY          = 0;
            pElement->width                 = 0;
            pElement->height                = 0;
            pElement->innerScaleX           = 1;
            pElement->innerScaleY           = 1;
            pElement->flags                 = 0;
            pElement->onMove                = NULL;
            pElement->onSize                = NULL;
            pElement->onMouseEnter          = NULL;
            pElement->onMouseLeave          = NULL;
            pElement->onMouseMove           = NULL;
            pElement->onMouseButtonDown     = NULL;
            pElement->onMouseButtonUp       = NULL;
            pElement->onMouseButtonDblClick = NULL;
            pElement->onMouseWheel          = NULL;
            pElement->onKeyDown             = NULL;
            pElement->onKeyUp               = NULL;
            pElement->onPrintableKeyDown    = NULL;
            pElement->onPaint               = NULL;
            pElement->onDirty               = NULL;
            pElement->onHitTest             = NULL;
            pElement->onCaptureMouse        = NULL;
            pElement->onReleaseMouse        = NULL;
            pElement->onCaptureKeyboard     = NULL;
            pElement->onReleaseKeyboard     = NULL;
            pElement->extraDataSize         = extraDataSize;
            memset(pElement->pExtraData, 0, extraDataSize);

            // Add to the the hierarchy.
            easygui_append_without_detach_or_redraw(pElement, pElement->pParent);
            

            // Have the element positioned at 0,0 relative to the parent by default.
            if (pParent != NULL) {
                pElement->absolutePosX = pParent->absolutePosX;
                pElement->absolutePosY = pParent->absolutePosY;
            }


            return pElement;
        }
    }

    return NULL;
}

void easygui_delete_element(easygui_element* pElement)
{
    if (pElement == NULL) {
        return;
    }

    easygui_context* pContext = pElement->pContext;
    if (pContext == NULL) {
        return;
    }

    if (easygui_is_element_marked_as_dead(pElement)) {
        easygui_log(pContext, "WARNING: Attempting to delete an element that is already marked for deletion.");
        return;
    }


    

    // Orphan the element first.
    easygui_detach_without_redraw(pElement);


    // If this was element is marked as the one that was last under the mouse it needs to be unset.
    bool needsMouseUpdate = false;
    if (pContext->pElementUnderMouse == pElement)
    {
        pContext->pElementUnderMouse = NULL;
        needsMouseUpdate = true;
    }

    if (pContext->pLastMouseMoveTopLevelElement == pElement)
    {
        pContext->pLastMouseMoveTopLevelElement = NULL;
        pContext->lastMouseMovePosX = 0;
        pContext->lastMouseMovePosY = 0;
        needsMouseUpdate = false;       // It was a top-level element so the mouse enter/leave state doesn't need an update.
    }


    // If this element has the mouse capture it needs to be released.
    if (pContext->pElementWithMouseCapture == pElement)
    {
        easygui_log(pContext, "WARNING: Deleting an element while it still has the mouse capture.");
        easygui_release_mouse(pContext);
    }

    // If this element has the keyboard capture it needs to be released.
    if (pContext->pElementWithKeyboardCapture == pElement)
    {
        easygui_log(pContext, "WARNING: Deleting an element while it still has the keyboard capture.");
        easygui_release_keyboard(pContext);
    }

    // Is this element in the middle of being marked as dirty?
    if (pContext->pDirtyTopLevelElement == pElement)
    {
        easygui_log(pContext, "WARNING: Deleting an element while it is being marked as dirty.");
        pContext->pDirtyTopLevelElement = NULL;
    }
    


    // Deleting this element may have resulted in the mouse entering a new element. Here is where we do a mouse enter/leave update.
    if (needsMouseUpdate)
    {
        pElement->onHitTest = easygui_pass_through_hit_test;        // <-- This ensures we don't include this element when searching for the new element under the mouse.
        easygui_update_mouse_enter_and_leave_state(pContext, easygui_find_element_under_point(pContext->pLastMouseMoveTopLevelElement, pContext->lastMouseMovePosX, pContext->lastMouseMovePosY));
    }



    // Children need to be deleted before deleting the element itself.
    while (pElement->pLastChild != NULL)
    {
        easygui_delete_element(pElement->pLastChild);
    }



    // Finally, we either need to mark the element as dead or delete it for real. We only mark it for deletion if we are in the middle
    // of processing an inbound event because there is a chance that an external event handler may try referencing the element.
    if (easygui_is_handling_inbound_event(pContext))
    {
        easygui_mark_element_as_dead(pElement);
    }
    else
    {
        easygui_delete_element_for_real(pElement);
    }
}


size_t easygui_get_extra_data_size(easygui_element* pElement)
{
    if (pElement != NULL) {
        return pElement->extraDataSize;
    }

    return 0;
}

void* easygui_get_extra_data(easygui_element* pElement)
{
    if (pElement != NULL) {
        return pElement->pExtraData;
    }

    return NULL;
}


void easygui_hide(easygui_element* pElement)
{
    if (pElement != NULL) {
        pElement->flags |= IS_ELEMENT_HIDDEN;
        easygui_auto_dirty(pElement, easygui_get_local_rect(pElement));
    }
}

void easygui_show(easygui_element* pElement)
{
    if (pElement != NULL) {
        pElement->flags &= ~IS_ELEMENT_HIDDEN;
        easygui_auto_dirty(pElement, easygui_get_local_rect(pElement));
    }
}

bool easygui_is_visible(const easygui_element* pElement)
{
    if (pElement != NULL) {
        return (pElement->flags & IS_ELEMENT_HIDDEN) == 0;
    }

    return false;
}

bool easygui_is_visible_recursive(const easygui_element* pElement)
{
    if (easygui_is_visible(pElement))
    {
        assert(pElement->pParent != NULL);

        if (pElement->pParent != NULL) {
            return easygui_is_visible(pElement->pParent);
        }
    }

    return false;
}


void easygui_disable_clipping(easygui_element* pElement)
{
    if (pElement != NULL) {
        pElement->flags |= IS_ELEMENT_CLIPPING_DISABLED;
    }
}

void easygui_enable_clipping(easygui_element* pElement)
{
    if (pElement != NULL) {
        pElement->flags &= ~IS_ELEMENT_CLIPPING_DISABLED;
    }
}

bool easygui_is_clipping_enabled(const easygui_element* pElement)
{
    if (pElement != NULL) {
        return (pElement->flags & IS_ELEMENT_CLIPPING_DISABLED) == 0;
    }

    return true;
}



void easygui_capture_mouse(easygui_element* pElement)
{
    if (pElement == NULL) {
        return;
    }

    if (pElement->pContext == NULL) {
        return;
    }


    if (pElement->pContext->pElementWithMouseCapture != pElement)
    {
        // Release the previous capture first.
        if (pElement->pContext->pElementWithMouseCapture != NULL) {
            easygui_release_mouse(pElement->pContext);
        }

        assert(pElement->pContext->pElementWithMouseCapture == NULL);

        pElement->pContext->pElementWithMouseCapture = pElement;

        // Two events need to be posted - the global on_capture_mouse event and the local on_capture_mouse event.
        easygui_post_outbound_event_capture_mouse(pElement);
        easygui_post_outbound_event_capture_mouse_global(pElement);
    }
}

void easygui_release_mouse(easygui_context* pContext)
{
    if (pContext == NULL) {
        return;
    }


    // Events need to be posted before setting the internal pointer.
    easygui_post_outbound_event_release_mouse(pContext->pElementWithMouseCapture);
    easygui_post_outbound_event_release_mouse_global(pContext->pElementWithMouseCapture);

    // We want to set the internal pointer to NULL after posting the events since that is when it has truly released the mouse.
    pContext->pElementWithMouseCapture = NULL;


    // After releasing the mouse the cursor may be sitting on top of a different element - we want to recheck that.
    easygui_update_mouse_enter_and_leave_state(pContext, easygui_find_element_under_point(pContext->pLastMouseMoveTopLevelElement, pContext->lastMouseMovePosX, pContext->lastMouseMovePosY));
}

easygui_element* easygui_get_element_with_mouse_capture(easygui_context* pContext)
{
    if (pContext == NULL) {
        return NULL;
    }

    return pContext->pElementWithMouseCapture;
}


void easygui_capture_keyboard(easygui_element* pElement)
{
    if (pElement == NULL) {
        return;
    }

    if (pElement->pContext == NULL) {
        return;
    }


    if (pElement->pContext->pElementWithKeyboardCapture != pElement)
    {
        // Release the previous capture first.
        if (pElement->pContext->pElementWithKeyboardCapture != NULL) {
            easygui_release_keyboard(pElement->pContext);
        }

        assert(pElement->pContext->pElementWithKeyboardCapture == NULL);

        pElement->pContext->pElementWithKeyboardCapture = pElement;

        // Two events need to be posted - the global on_capture_mouse event and the local on_capture_mouse event.
        easygui_post_outbound_event_capture_keyboard(pElement);
        easygui_post_outbound_event_capture_keyboard_global(pElement);
    }
}

void easygui_release_keyboard(easygui_context* pContext)
{
    if (pContext == NULL) {
        return;
    }


    // Events need to be posted before setting the internal pointer.
    easygui_post_outbound_event_release_keyboard(pContext->pElementWithKeyboardCapture);
    easygui_post_outbound_event_release_keyboard_global(pContext->pElementWithKeyboardCapture);


    pContext->pElementWithKeyboardCapture = NULL;
}

easygui_element* easygui_get_element_with_keyboard_capture(easygui_context* pContext)
{
    if (pContext == NULL) {
        return NULL;
    }

    return pContext->pElementWithKeyboardCapture;
}



//// Events ////

void easygui_set_on_move(easygui_element * pElement, easygui_on_move_proc callback)
{
    if (pElement != NULL) {
        pElement->onMove = callback;
    }
}

void easygui_set_on_size(easygui_element * pElement, easygui_on_size_proc callback)
{
    if (pElement != NULL) {
        pElement->onSize = callback;
    }
}

void easygui_set_on_mouse_enter(easygui_element* pElement, easygui_on_mouse_enter_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseEnter = callback;
    }
}

void easygui_set_on_mouse_leave(easygui_element* pElement, easygui_on_mouse_leave_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseLeave = callback;
    }
}

void easygui_set_on_mouse_move(easygui_element* pElement, easygui_on_mouse_move_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseMove = callback;
    }
}

void easygui_set_on_mouse_button_down(easygui_element* pElement, easygui_on_mouse_button_down_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseButtonDown = callback;
    }
}

void easygui_set_on_mouse_button_up(easygui_element* pElement, easygui_on_mouse_button_up_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseButtonUp = callback;
    }
}

void easygui_set_on_mouse_button_dblclick(easygui_element* pElement, easygui_on_mouse_button_dblclick_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseButtonDblClick = callback;
    }
}

void easygui_set_on_mouse_wheel(easygui_element* pElement, easygui_on_mouse_wheel_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseWheel = callback;
    }
}

void easygui_set_on_key_down(easygui_element* pElement, easygui_on_key_down_proc callback)
{
    if (pElement != NULL) {
        pElement->onKeyDown = callback;
    }
}

void easygui_set_on_key_up(easygui_element* pElement, easygui_on_key_up_proc callback)
{
    if (pElement != NULL) {
        pElement->onKeyUp = callback;
    }
}

void easygui_set_on_printable_key_down(easygui_element* pElement, easygui_on_printable_key_down_proc callback)
{
    if (pElement != NULL) {
        pElement->onPrintableKeyDown = callback;
    }
}

void easygui_set_on_paint(easygui_element* pElement, easygui_on_paint_proc callback)
{
    if (pElement != NULL) {
        pElement->onPaint = callback;
    }
}

void easygui_set_on_dirty(easygui_element * pElement, easygui_on_dirty_proc callback)
{
    if (pElement != NULL) {
        pElement->onDirty = callback;
    }
}

void easygui_set_on_hittest(easygui_element* pElement, easygui_on_hittest_proc callback)
{
    if (pElement != NULL) {
        pElement->onHitTest = callback;
    }
}

void easygui_set_on_capture_mouse(easygui_element* pElement, easygui_on_capture_mouse_proc callback)
{
    if (pElement != NULL) {
        pElement->onCaptureMouse = callback;
    }
}

void easygui_set_on_release_mouse(easygui_element* pElement, easygui_on_release_mouse_proc callback)
{
    if (pElement != NULL) {
        pElement->onReleaseMouse = callback;
    }
}

void easygui_set_on_capture_keyboard(easygui_element* pElement, easygui_on_capture_keyboard_proc callback)
{
    if (pElement != NULL) {
        pElement->onCaptureKeyboard = callback;
    }
}

void easygui_set_on_release_keyboard(easygui_element* pElement, easygui_on_release_keyboard_proc callback)
{
    if (pElement != NULL) {
        pElement->onReleaseKeyboard = callback;
    }
}



bool easygui_is_point_inside_element_bounds(const easygui_element* pElement, float absolutePosX, float absolutePosY)
{
    if (absolutePosX < pElement->absolutePosX ||
        absolutePosX < pElement->absolutePosY)
    {
        return false;
    }
    
    if (absolutePosX >= pElement->absolutePosX + pElement->width ||
        absolutePosY >= pElement->absolutePosY + pElement->height)
    {
        return false;
    }

    return true;
}

bool easygui_is_point_inside_element(easygui_element* pElement, float absolutePosX, float absolutePosY)
{
    if (easygui_is_point_inside_element_bounds(pElement, absolutePosX, absolutePosY))
    {
        // It is valid for onHitTest to be null, in which case we use the default hit test which assumes the element is just a rectangle
        // equal to the size of it's bounds. It's equivalent to onHitTest always returning true.

        if (pElement->onHitTest) {
            return pElement->onHitTest(pElement, absolutePosX - pElement->absolutePosX, absolutePosY - pElement->absolutePosY);
        }

        return true;
    }

    return false;
}



typedef struct
{
    easygui_element* pElementUnderPoint;
    float absolutePosX;
    float absolutePosY;
}easygui_find_element_under_point_data;

bool easygui_find_element_under_point_iterator(easygui_element* pElement, easygui_rect* pRelativeVisibleRect, void* pUserData)
{
    assert(pElement             != NULL);
    assert(pRelativeVisibleRect != NULL);

    easygui_find_element_under_point_data* pData = pUserData;
    assert(pData != NULL);

    float innerScaleX;
    float innerScaleY;
    easygui_get_absolute_inner_scale(pElement->pParent, &innerScaleX, &innerScaleY);

    float relativePosX = pData->absolutePosX / innerScaleX;
    float relativePosY = pData->absolutePosY / innerScaleY;
    easygui_make_point_relative(pElement, &relativePosX, &relativePosY);
    
    if (easygui_rect_contains_point(*pRelativeVisibleRect, relativePosX, relativePosY))
    {
        if (pElement->onHitTest) {
            if (pElement->onHitTest(pElement, relativePosX, relativePosY)) {
                pData->pElementUnderPoint = pElement;
            }
        } else {
            pData->pElementUnderPoint = pElement;
        }
    }


    // Always return true to ensure the entire hierarchy is checked.
    return true;
}

easygui_element* easygui_find_element_under_point(easygui_element* pTopLevelElement, float absolutePosX, float absolutePosY)
{
    if (pTopLevelElement == NULL) {
        return NULL;
    }

    easygui_find_element_under_point_data data;
    data.pElementUnderPoint = NULL;
    data.absolutePosX = absolutePosX;
    data.absolutePosY = absolutePosY;
    easygui_iterate_visible_elements(pTopLevelElement, easygui_get_absolute_rect(pTopLevelElement), easygui_find_element_under_point_iterator, &data);

    return data.pElementUnderPoint;
}

bool easygui_is_element_under_mouse(easygui_element* pElement)
{
    if (pElement == NULL) {
        return false;
    }

    return easygui_find_element_under_point(pElement->pContext->pLastMouseMoveTopLevelElement, pElement->pContext->lastMouseMovePosX, pElement->pContext->lastMouseMovePosY);
}



//// Hierarchy ////

void easygui_detach(easygui_element* pChildElement)
{
    if (pChildElement == NULL) {
        return;
    }

    easygui_element* pOldParent = pChildElement->pParent;


    // We orphan the element using the private API. This will not mark the parent element as dirty so we need to do that afterwards.
    easygui_detach_without_redraw(pChildElement);

    // The region of the old parent needs to be redrawn.
    if (pOldParent != NULL) {
        easygui_auto_dirty(pOldParent, easygui_get_relative_rect(pOldParent));
    }
}

void easygui_append(easygui_element* pChildElement, easygui_element* pParentElement)
{
    if (pChildElement == NULL) {
        return;
    }

    // We first need to orphan the element. If the parent element is the new parent is the same as the old one, as in we
    // are just moving the child element to the end of the children list, we want to delay the repaint until the end. To
    // do this we use easygui_detach_without_redraw() because that will not trigger a redraw.
    if (pChildElement->pParent != pParentElement) {
        easygui_detach(pChildElement);
    } else {
        easygui_detach_without_redraw(pChildElement);
    }
    

    // Now we attach it to the end of the new parent.
    if (pParentElement != NULL) {
        easygui_append_without_detach(pChildElement, pParentElement);
    }
}

void easygui_prepend(easygui_element* pChildElement, easygui_element* pParentElement)
{
    if (pChildElement == NULL) {
        return;
    }

    // See comment in easygui_append() for explanation on this.
    if (pChildElement->pParent != pParentElement) {
        easygui_detach(pChildElement);
    } else {
        easygui_detach_without_redraw(pChildElement);
    }


    // Now we need to attach the element to the beginning of the parent.
    if (pParentElement != NULL) {
        easygui_prepend_without_detach(pChildElement, pParentElement);
    }
}

void easygui_append_sibling(easygui_element* pElementToAppend, easygui_element* pElementToAppendTo)
{
    if (pElementToAppend == NULL || pElementToAppendTo == NULL) {
        return;
    }

    // See comment in easygui_append() for explanation on this.
    if (pElementToAppend->pParent != pElementToAppendTo->pParent) {
        easygui_detach(pElementToAppend);
    } else {
        easygui_detach_without_redraw(pElementToAppend);
    }


    // Now we need to attach the element such that it comes just after pElementToAppendTo
    easygui_append_sibling_without_detach(pElementToAppend, pElementToAppendTo);
}

void easygui_prepend_sibling(easygui_element* pElementToPrepend, easygui_element* pElementToPrependTo)
{
    if (pElementToPrepend == NULL || pElementToPrependTo == NULL) {
        return;
    }

    // See comment in easygui_append() for explanation on this.
    if (pElementToPrepend->pParent != pElementToPrependTo->pParent) {
        easygui_detach(pElementToPrepend);
    } else {
        easygui_detach_without_redraw(pElementToPrepend);
    }


    // Now we need to attach the element such that it comes just after pElementToPrependTo
    easygui_prepend_sibling_without_detach(pElementToPrepend, pElementToPrependTo);
}

easygui_element* easygui_find_top_level_element(easygui_element* pElement)
{
    if (pElement == NULL) {
        return NULL;
    }

    if (pElement->pParent != NULL) {
        return easygui_find_top_level_element(pElement->pParent);
    }

    return pElement;
}

bool easygui_is_parent(easygui_element* pParentElement, easygui_element* pChildElement)
{
    if (pParentElement == NULL || pChildElement == NULL) {
        return false;
    }

    return pParentElement == pChildElement->pParent;
}

bool easygui_is_child(easygui_element* pChildElement, easygui_element* pParentElement)
{
    return easygui_is_parent(pParentElement, pChildElement);
}

bool easygui_is_ancestor(easygui_element* pAncestorElement, easygui_element* pChildElement)
{
    if (pAncestorElement == NULL || pChildElement == NULL) {
        return false;
    }

    easygui_element* pParent = pChildElement->pParent;
    while (pParent != NULL)
    {
        if (pParent == pAncestorElement) {
            return true;
        }

        pParent = pParent->pParent;
    }


    return false;
}

bool easygui_is_descendant(easygui_element* pChildElement, easygui_element* pAncestorElement)
{
    return easygui_is_ancestor(pAncestorElement, pChildElement);
}



//// Layout ////

void easygui_set_absolute_position(easygui_element* pElement, float positionX, float positionY)
{
    if (pElement != NULL)
    {
        if (pElement->absolutePosX != positionX || pElement->absolutePosY != positionY)
        {
            float oldRelativePosX = easygui_get_relative_position_x(pElement);
            float oldRelativePosY = easygui_get_relative_position_y(pElement);

            easygui_begin_auto_dirty(pElement, easygui_get_local_rect(pElement));   // <-- Previous rectangle.
            {
                float offsetX = positionX - pElement->absolutePosX;
                float offsetY = positionY - pElement->absolutePosY;

                pElement->absolutePosX = positionX;
                pElement->absolutePosY = positionY;
                easygui_auto_dirty(pElement, easygui_get_local_rect(pElement));     // <-- New rectangle.


                float newRelativePosX = easygui_get_relative_position_x(pElement);
                float newRelativePosY = easygui_get_relative_position_y(pElement);

                if (newRelativePosX != oldRelativePosX || newRelativePosY != oldRelativePosY) {
                    easygui_post_outbound_event_move(pElement, newRelativePosX, newRelativePosY);
                }
                

                easygui_apply_offset_to_children_recursive(pElement, offsetX, offsetY);
            }
            easygui_end_auto_dirty(pElement);
        }
    }
}

void easygui_get_absolute_position(const easygui_element* pElement, float * positionXOut, float * positionYOut)
{
    if (pElement != NULL)
    {
        if (positionXOut != NULL) {
            *positionXOut = pElement->absolutePosX;
        }

        if (positionYOut != NULL) {
            *positionYOut = pElement->absolutePosY;
        }
    }
}

float easygui_get_absolute_position_x(const easygui_element* pElement)
{
    if (pElement != NULL) {
        return pElement->absolutePosX;
    }

    return 0.0f;
}

float easygui_get_absolute_position_y(const easygui_element* pElement)
{
    if (pElement != NULL) {
        return pElement->absolutePosY;
    }

    return 0.0f;
}


void easygui_set_relative_position(easygui_element* pElement, float relativePosX, float relativePosY)
{
    if (pElement != NULL) {
        if (pElement->pParent != NULL)
        {
            easygui_set_absolute_position(pElement, pElement->pParent->absolutePosX + relativePosX, pElement->pParent->absolutePosY + relativePosY);
        }
        else
        {
            easygui_set_absolute_position(pElement, relativePosX, relativePosY);
        }
    }
}

void easygui_get_relative_position(const easygui_element* pElement, float* positionXOut, float* positionYOut)
{
    if (pElement != NULL)
    {
        if (pElement->pParent != NULL)
        {
            if (positionXOut != NULL) {
                *positionXOut = pElement->absolutePosX - pElement->pParent->absolutePosX;
            }

            if (positionYOut != NULL) {
                *positionYOut = pElement->absolutePosY - pElement->pParent->absolutePosY;
            }
        }
        else
        {
            if (positionXOut != NULL) {
                *positionXOut = pElement->absolutePosX;
            }

            if (positionYOut != NULL) {
                *positionYOut = pElement->absolutePosY;
            }
        }
    }
}

float easygui_get_relative_position_x(const easygui_element* pElement)
{
    if (pElement != NULL) {
        if (pElement->pParent != NULL) {
            return pElement->absolutePosX - pElement->pParent->absolutePosX;
        } else {
            return pElement->absolutePosX;
        }
    }

    return 0;
}

float easygui_get_relative_position_y(const easygui_element* pElement)
{
    if (pElement != NULL) {
        if (pElement->pParent != NULL) {
            return pElement->absolutePosY - pElement->pParent->absolutePosY;
        } else {
            return pElement->absolutePosY;
        }
    }

    return 0;
}


void easygui_set_size(easygui_element* pElement, float width, float height)
{
    if (pElement != NULL)
    {
        if (pElement->width != width || pElement->height != height)
        {
            easygui_begin_auto_dirty(pElement, easygui_get_local_rect(pElement));   // <-- Previous rectangle.
            {
                pElement->width  = width;
                pElement->height = height;
                easygui_auto_dirty(pElement, easygui_get_local_rect(pElement));     // <-- New rectangle.

                easygui_post_outbound_event_size(pElement, width, height);
            }
            easygui_end_auto_dirty(pElement);
        }
    }
}

void easygui_get_size(const easygui_element* pElement, float* widthOut, float* heightOut)
{
    if (pElement != NULL) {
        if (widthOut != NULL) {
            *widthOut = pElement->width;
        }

        if (heightOut != NULL) {
            *heightOut = pElement->height;
        }
    }
}

float easygui_get_width(const easygui_element * pElement)
{
    if (pElement != NULL) {
        return pElement->width;
    }

    return 0;
}

float easygui_get_height(const easygui_element * pElement)
{
    if (pElement != NULL) {
        return pElement->height;
    }

    return 0;
}


void easygui_set_inner_scale(easygui_element* pElement, float innerScaleX, float innerScaleY)
{
    if (pElement == NULL){
        return;
    }

    pElement->innerScaleX = innerScaleX;
    pElement->innerScaleY = innerScaleY;

    if (easygui_is_auto_dirty_enabled(pElement->pContext)) {
        easygui_dirty(pElement, easygui_get_local_rect(pElement));
    }
}

void easygui_get_inner_scale(easygui_element* pElement, float* pInnerScaleXOut, float* pInnerScaleYOut)
{
    float innerScaleX = 1;
    float innerScaleY = 1;

    if (pElement != NULL)
    {
        innerScaleX = pElement->innerScaleX;
        innerScaleY = pElement->innerScaleY;
    }


    if (pInnerScaleXOut) {
        *pInnerScaleXOut = innerScaleX;
    }
    if (pInnerScaleYOut) {
        *pInnerScaleYOut = innerScaleY;
    }
}

void easygui_get_absolute_inner_scale(easygui_element* pElement, float* pInnerScaleXOut, float* pInnerScaleYOut)
{
    float innerScaleX = 1;
    float innerScaleY = 1;

    if (pElement != NULL)
    {
        innerScaleX = pElement->innerScaleX;
        innerScaleY = pElement->innerScaleY;

        if (pElement->pParent != NULL)
        {
            float parentInnerScaleX;
            float parentInnerScaleY;
            easygui_get_absolute_inner_scale(pElement->pParent, &parentInnerScaleX, &parentInnerScaleY);

            innerScaleX *= parentInnerScaleX;
            innerScaleY *= parentInnerScaleY;
        }
    }

    if (pInnerScaleXOut) {
        *pInnerScaleXOut = innerScaleX;
    }
    if (pInnerScaleYOut) {
        *pInnerScaleYOut = innerScaleY;
    }
}


easygui_rect easygui_get_absolute_rect(const easygui_element* pElement)
{
    easygui_rect rect;
    if (pElement != NULL)
    {
        rect.left   = pElement->absolutePosX;
        rect.top    = pElement->absolutePosY;
        rect.right  = rect.left + pElement->width;
        rect.bottom = rect.top + pElement->height;
    }
    else
    {
        rect.left   = 0;
        rect.top    = 0;
        rect.right  = 0;
        rect.bottom = 0;
    }
    
    return rect;
}

easygui_rect easygui_get_relative_rect(const easygui_element* pElement)
{
    easygui_rect rect;
    if (pElement != NULL)
    {
        rect.left   = easygui_get_relative_position_x(pElement);
        rect.top    = easygui_get_relative_position_y(pElement);
        rect.right  = rect.left + pElement->width;
        rect.bottom = rect.top  + pElement->height;
    }
    else
    {
        rect.left   = 0;
        rect.top    = 0;
        rect.right  = 0;
        rect.bottom = 0;
    }
    
    return rect;
}

easygui_rect easygui_get_local_rect(const easygui_element* pElement)
{
    easygui_rect rect;
    rect.left = 0;
    rect.top  = 0;

    if (pElement != NULL)
    {
        rect.right  = pElement->width;
        rect.bottom = pElement->height;
    }
    else
    {
        rect.right  = 0;
        rect.bottom = 0;
    }

    return rect;
}



//// Painting ////

bool easygui_register_painting_callbacks(easygui_context* pContext, void* pPaintingContext, easygui_painting_callbacks callbacks)
{
    if (pContext == NULL) {
        return false;
    }

    // Fail if the painting callbacks have already been registered.
    if (pContext->pPaintingContext != NULL) {
        return false;
    }


    pContext->pPaintingContext  = pPaintingContext;
    pContext->paintingCallbacks = callbacks;

    return true;
}


bool easygui_iterate_visible_elements(easygui_element* pParentElement, easygui_rect relativeRect, easygui_visible_iteration_proc callback, void* pUserData)
{
    if (pParentElement == NULL) {
        return false;
    }

    if (callback == NULL) {
        return false;
    }


    if (!easygui_is_visible(pParentElement)) {
        return true;
    }

    easygui_rect clampedRelativeRect = relativeRect;
    if (easygui_clamp_rect_to_element(pParentElement, &clampedRelativeRect))
    {
        // We'll only get here if some part of the rectangle was inside the element.
        if (!callback(pParentElement, &clampedRelativeRect, pUserData)) {
            return false;
        }
    }

    for (easygui_element* pChild = pParentElement->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling)
    {
        float childRelativePosX = easygui_get_relative_position_x(pChild);
        float childRelativePosY = easygui_get_relative_position_y(pChild);

        easygui_rect childRect;
        if (easygui_is_clipping_enabled(pChild)) {
            childRect = clampedRelativeRect;
        } else {
            childRect = relativeRect;
        }


        childRect.left   -= childRelativePosX;
        childRect.top    -= childRelativePosY;
        childRect.right  -= childRelativePosX;
        childRect.bottom -= childRelativePosY;

        if (!easygui_iterate_visible_elements(pChild, childRect, callback, pUserData)) {
            return false;
        }
    }


    return true;
}

void easygui_disable_auto_dirty(easygui_context* pContext)
{
    if (pContext != NULL) {
        pContext->flags |= IS_AUTO_DIRTY_DISABLED;
    }
}

void easygui_enable_auto_dirty(easygui_context* pContext)
{
    if (pContext != NULL) {
        pContext->flags &= ~IS_AUTO_DIRTY_DISABLED;
    }
}

bool easygui_is_auto_dirty_enabled(easygui_context* pContext)
{
    if (pContext != NULL) {
        return (pContext->flags & IS_AUTO_DIRTY_DISABLED) == 0;
    }

    return false;
}


void easygui_dirty(easygui_element* pElement, easygui_rect relativeRect)
{
    if (pElement == NULL) {
        return;
    }

    easygui_post_outbound_event_dirty_global(pElement, relativeRect);
}


bool easygui_draw_iteration_callback(easygui_element* pElement, easygui_rect* pRelativeRect, void* pUserData)
{
    assert(pElement      != NULL);
    assert(pRelativeRect != NULL);

    if (pElement->onPaint != NULL)
    {
        // We want to set the initial clipping rectangle before drawing.
        easygui_set_clip(pElement, *pRelativeRect, pUserData);

        // We now call the painting function, but only after setting the clipping rectangle.
        pElement->onPaint(pElement, *pRelativeRect, pUserData);

        // The on_paint event handler may have adjusted the clipping rectangle so we need to ensure it's restored.
        easygui_set_clip(pElement, *pRelativeRect, pUserData);
    }

    return true;
}

void easygui_draw(easygui_element* pElement, easygui_rect relativeRect, void* pPaintData)
{
    if (pElement == NULL) {
        return;
    }

    easygui_context* pContext = pElement->pContext;
    if (pContext == NULL) {
        return;
    }

    assert(pContext->paintingCallbacks.drawBegin != NULL);
    assert(pContext->paintingCallbacks.drawEnd   != NULL);

    pContext->paintingCallbacks.drawBegin(pPaintData);
    {
        easygui_iterate_visible_elements(pElement, relativeRect, easygui_draw_iteration_callback, pPaintData);
    }
    pContext->paintingCallbacks.drawEnd(pPaintData);
}

void easygui_get_clip(easygui_element* pElement, easygui_rect* pRelativeRect, void* pPaintData)
{
    if (pElement == NULL || pElement->pContext == NULL) {
        return;
    }

    pElement->pContext->paintingCallbacks.getClip(pRelativeRect, pPaintData);

    if (pRelativeRect)
    {
        float scaleX;
        float scaleY;
        easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

        pRelativeRect->left   /= scaleX;
        pRelativeRect->top    /= scaleY;
        pRelativeRect->right  /= scaleX;
        pRelativeRect->bottom /= scaleY;
    }

    // The clip returned by the drawing callback will be absolute so we'll need to convert that to relative.
    easygui_make_rect_relative(pElement, pRelativeRect);
}

void easygui_set_clip(easygui_element* pElement, easygui_rect relativeRect, void* pPaintData)
{
    if (pElement == NULL || pElement->pContext == NULL) {
        return;
    }


    // Make sure the rectangle is not negative.
    if (relativeRect.right < relativeRect.left) {
        relativeRect.right = relativeRect.left;
    }

    if (relativeRect.bottom < relativeRect.top) {
        relativeRect.bottom = relativeRect.top;
    }

    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    easygui_rect absoluteRect = relativeRect;
    easygui_make_rect_absolute(pElement, &absoluteRect);
    absoluteRect = easygui_scale_rect(absoluteRect, scaleX, scaleY);

    pElement->pContext->paintingCallbacks.setClip(absoluteRect, pPaintData);
}

void easygui_draw_rect(easygui_element* pElement, easygui_rect relativeRect, easygui_color color, void* pPaintData)
{
    if (pElement == NULL) {
        return;
    }

    assert(pElement->pContext != NULL);

    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    easygui_rect absoluteRect = relativeRect;
    easygui_make_rect_absolute(pElement, &absoluteRect);
    absoluteRect = easygui_scale_rect(absoluteRect, scaleX, scaleY);

    pElement->pContext->paintingCallbacks.drawRect(absoluteRect, color, pPaintData);
}

void easygui_draw_rect_outline(easygui_element* pElement, easygui_rect relativeRect, easygui_color color, float outlineWidth, void* pPaintData)
{
    if (pElement == NULL) {
        return;
    }

    assert(pElement->pContext != NULL);

    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    easygui_rect absoluteRect = relativeRect;
    easygui_make_rect_absolute(pElement, &absoluteRect);
    absoluteRect = easygui_scale_rect(absoluteRect, scaleX, scaleY);

    if (scaleX == scaleY)
    {
        pElement->pContext->paintingCallbacks.drawRectOutline(absoluteRect, color, (outlineWidth * scaleX), pPaintData);
    }
    else
    {
        // TODO: This is incorrect. The left and right borders need to be scaled by scaleX and the top and bottom borders need to be scaled by scaleY.
        pElement->pContext->paintingCallbacks.drawRectOutline(absoluteRect, color, (outlineWidth * scaleX), pPaintData);
    }
}

void easygui_draw_rect_with_outline(easygui_element * pElement, easygui_rect relativeRect, easygui_color color, float outlineWidth, easygui_color outlineColor, void * pPaintData)
{
    if (pElement == NULL) {
        return;
    }

    assert(pElement->pContext != NULL);

    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    easygui_rect absoluteRect = relativeRect;
    easygui_make_rect_absolute(pElement, &absoluteRect);
    absoluteRect = easygui_scale_rect(absoluteRect, scaleX, scaleY);

    if (scaleX == scaleY)
    {
        pElement->pContext->paintingCallbacks.drawRectWithOutline(absoluteRect, color, (outlineWidth * scaleX), outlineColor, pPaintData);
    }
    else
    {
        // TODO: This is incorrect. The left and right borders need to be scaled by scaleX and the top and bottom borders need to be scaled by scaleY.
        pElement->pContext->paintingCallbacks.drawRectWithOutline(absoluteRect, color, (outlineWidth * scaleX), outlineColor, pPaintData);
    }
}

void easygui_draw_round_rect(easygui_element* pElement, easygui_rect relativeRect, easygui_color color, float radius, void* pPaintData)
{
    if (pElement == NULL) {
        return;
    }

    assert(pElement->pContext != NULL);

    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    easygui_rect absoluteRect = relativeRect;
    easygui_make_rect_absolute(pElement, &absoluteRect);
    absoluteRect = easygui_scale_rect(absoluteRect, scaleX, scaleY);

    if (scaleX == scaleY)
    {
        pElement->pContext->paintingCallbacks.drawRoundRect(absoluteRect, color, (radius * scaleX), pPaintData);
    }
    else
    {
        // TODO: The corners need to be rounded based on an ellipse rather than a circle.
        pElement->pContext->paintingCallbacks.drawRoundRect(absoluteRect, color, (radius * scaleX), pPaintData);
    }
}

void easygui_draw_round_rect_outline(easygui_element* pElement, easygui_rect relativeRect, easygui_color color, float radius, float outlineWidth, void* pPaintData)
{
    if (pElement == NULL) {
        return;
    }

    assert(pElement->pContext != NULL);

    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    easygui_rect absoluteRect = relativeRect;
    easygui_make_rect_absolute(pElement, &absoluteRect);
    absoluteRect = easygui_scale_rect(absoluteRect, scaleX, scaleY);

    if (scaleX == scaleY)
    {
        pElement->pContext->paintingCallbacks.drawRoundRectOutline(absoluteRect, color, (radius * scaleX), floorf(outlineWidth * scaleX), pPaintData);
    }
    else
    {
        // TODO: This is incorrect. The left and right borders need to be scaled by scaleX and the top and bottom borders need to be scaled by scaleY. The corners need to be rounded based on an ellipse rather than a circle.
        pElement->pContext->paintingCallbacks.drawRoundRectOutline(absoluteRect, color, (radius * scaleX), (outlineWidth * scaleX), pPaintData);
    }
}

void easygui_draw_round_rect_with_outline(easygui_element* pElement, easygui_rect relativeRect, easygui_color color, float radius, float outlineWidth, easygui_color outlineColor, void* pPaintData)
{
    if (pElement == NULL) {
        return;
    }

    assert(pElement->pContext != NULL);

    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    easygui_rect absoluteRect = relativeRect;
    easygui_make_rect_absolute(pElement, &absoluteRect);
    absoluteRect = easygui_scale_rect(absoluteRect, scaleX, scaleY);

    if (scaleX == scaleY)
    {
        pElement->pContext->paintingCallbacks.drawRoundRectWithOutline(absoluteRect, color, (radius * scaleX), (outlineWidth * scaleX), outlineColor, pPaintData);
    }
    else
    {
        // TODO: This is incorrect. The left and right borders need to be scaled by scaleX and the top and bottom borders need to be scaled by scaleY. The corners need to be rounded based on an ellipse rather than a circle.
        pElement->pContext->paintingCallbacks.drawRoundRectWithOutline(absoluteRect, color, (radius * scaleX), (outlineWidth * scaleX), outlineColor, pPaintData);
    }
}

void easygui_draw_text(easygui_element* pElement, easygui_font* pFont, const char* text, int textLengthInBytes, float posX, float posY, easygui_color color, easygui_color backgroundColor, void* pPaintData)
{
    if (pElement == NULL || pFont == NULL) {
        return;
    }

    assert(pElement->pContext != NULL);

    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    float absolutePosX = posX * scaleX;
    float absolutePosY = posY * scaleX;
    easygui_make_point_absolute(pElement, &absolutePosX, &absolutePosY);

    easygui_resource font = easygui_get_internal_font_by_scale(pFont, scaleY);
    if (font == NULL) {
        return;
    }

    pElement->pContext->paintingCallbacks.drawText(font, text, textLengthInBytes, absolutePosX, absolutePosY, color, backgroundColor, pPaintData);
}

void easygui_draw_image(easygui_element* pElement, easygui_image* pImage, easygui_draw_image_args* pArgs, void* pPaintData)
{
    if (pElement == NULL || pImage == NULL) {
        return;
    }

    assert(pElement->pContext != NULL);

    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    easygui_make_point_absolute(pElement, &pArgs->dstX, &pArgs->dstY);
    easygui_make_point_absolute(pElement, &pArgs->dstBoundsX, &pArgs->dstBoundsY);

    pArgs->dstX            *= scaleX;
    pArgs->dstY            *= scaleY;
    pArgs->dstWidth        *= scaleX;
    pArgs->dstHeight       *= scaleY;
    pArgs->dstBoundsX      *= scaleX;
    pArgs->dstBoundsY      *= scaleY;
    pArgs->dstBoundsWidth  *= scaleX;
    pArgs->dstBoundsHeight *= scaleY;


    pElement->pContext->paintingCallbacks.drawImage(pImage->hResource, pArgs, pPaintData);
}


easygui_font* easygui_create_font(easygui_context* pContext, const char* family, unsigned int size, easygui_font_weight weight, easygui_font_slant slant, float rotation)
{
    if (pContext == NULL) {
        return NULL;
    }

    if (pContext->paintingCallbacks.createFont == NULL) {
        return NULL;
    }


    easygui_resource internalFont = pContext->paintingCallbacks.createFont(pContext->pPaintingContext, family, size, weight, slant, rotation);
    if (internalFont == NULL) {
        return NULL;
    }

    easygui_font* pFont = malloc(sizeof(easygui_font));
    if (pFont == NULL) {
        return NULL;
    }

    pFont->pContext          = pContext;
    pFont->family[0]         = '\0';
    pFont->size              = size;
    pFont->weight            = weight;
    pFont->slant             = slant;
    pFont->rotation          = rotation;
    pFont->internalFontCount = 1;
    pFont->pInternalFonts    = malloc(sizeof(easygui_resource) * pFont->internalFontCount);
    pFont->pInternalFonts[0] = internalFont;

    if (family != NULL) {
        strcpy_s(pFont->family, sizeof(pFont->family), family);
    }

    return pFont;
}

void easygui_delete_font(easygui_font* pFont)
{
    if (pFont == NULL) {
        return;
    }

    assert(pFont->pContext != NULL);

    // Delete the internal font objects first.
    if (pFont->pContext->paintingCallbacks.deleteFont) {
        for (size_t i = 0; i < pFont->internalFontCount; ++i) {
            pFont->pContext->paintingCallbacks.deleteFont(pFont->pInternalFonts[i]);
        }
    }

    free(pFont->pInternalFonts);
    free(pFont);
}

bool easygui_get_font_metrics(easygui_font* pFont, float scaleX, float scaleY, easygui_font_metrics* pMetricsOut)
{
    (void)scaleX;

    if (pFont == NULL || pMetricsOut == NULL) {
        return false;
    }

    assert(pFont->pContext != NULL);

    if (pFont->pContext->paintingCallbacks.getFontMetrics == NULL) {
        return false;
    }

    easygui_resource font = easygui_get_internal_font_by_scale(pFont, scaleY);
    if (font == NULL) {
        return false;
    }

    bool result = pFont->pContext->paintingCallbacks.getFontMetrics(font, pMetricsOut);
    if (result)
    {
        if (pMetricsOut != NULL)
        {
            pMetricsOut->ascent     = (unsigned int)(pMetricsOut->ascent     / scaleY);
            pMetricsOut->descent    = (unsigned int)(pMetricsOut->descent    / scaleY);
            pMetricsOut->lineHeight = (unsigned int)(pMetricsOut->lineHeight / scaleY);
            pMetricsOut->spaceWidth = (unsigned int)(pMetricsOut->spaceWidth / scaleX);
        }
    }

    return result;
}

bool easygui_get_font_metrics_by_element(easygui_font* pFont, easygui_element* pElement, easygui_font_metrics* pMetricsOut)
{
    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    return easygui_get_font_metrics(pFont, scaleX, scaleY, pMetricsOut);
}

bool easygui_get_glyph_metrics(easygui_font* pFont, unsigned int utf32, float scaleX, float scaleY, easygui_glyph_metrics* pMetricsOut)
{
    (void)scaleY;

    if (pFont == NULL || pMetricsOut == NULL) {
        return false;
    }

    assert(pFont->pContext != NULL);

    if (pFont->pContext->paintingCallbacks.getGlyphMetrics == NULL) {
        return false;
    }

    easygui_resource font = easygui_get_internal_font_by_scale(pFont, scaleY);
    if (font == NULL) {
        return false;
    }

    bool result = pFont->pContext->paintingCallbacks.getGlyphMetrics(font, utf32, pMetricsOut);
    if (result)
    {
        if (pMetricsOut != NULL)
        {
            pMetricsOut->width    = (unsigned int)(pMetricsOut->width    / scaleX);
            pMetricsOut->height   = (unsigned int)(pMetricsOut->height   / scaleY);
            pMetricsOut->originX  = (unsigned int)(pMetricsOut->originX  / scaleX);
            pMetricsOut->originY  = (unsigned int)(pMetricsOut->originY  / scaleY);
            pMetricsOut->advanceX = (unsigned int)(pMetricsOut->advanceX / scaleX);
            pMetricsOut->advanceY = (unsigned int)(pMetricsOut->advanceY / scaleY);
        }
    }
    
    return result;
}

bool easygui_get_glyph_metrics_by_element(easygui_font* pFont, unsigned int utf32, easygui_element* pElement, easygui_glyph_metrics* pMetricsOut)
{
    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    return easygui_get_glyph_metrics(pFont, utf32, scaleX, scaleY, pMetricsOut);
}

bool easygui_measure_string(easygui_font* pFont, const char* text, size_t textLengthInBytes, float scaleX, float scaleY, float* pWidthOut, float* pHeightOut)
{
    if (pFont == NULL) {
        return false;
    }

    if (text == NULL || textLengthInBytes == 0)
    {
        easygui_font_metrics metrics;
        if (!easygui_get_font_metrics(pFont, scaleX, scaleY, &metrics)) {
            return false;
        }

        if (pWidthOut) {
            *pWidthOut = 0;
        }
        if (pHeightOut) {
            *pHeightOut = (float)metrics.lineHeight;
        }

        return true;
    }

    

    assert(pFont->pContext != NULL);

    if (pFont->pContext->paintingCallbacks.measureString == NULL) {
        return false;
    }

    easygui_resource font = easygui_get_internal_font_by_scale(pFont, scaleY);
    if (font == NULL) {
        return false;
    }

    bool result = pFont->pContext->paintingCallbacks.measureString(font, text, textLengthInBytes, pWidthOut, pHeightOut);
    if (result)
    {
        if (pWidthOut) {
            *pWidthOut = (*pWidthOut / scaleX);
        }
        if (pHeightOut) {
            *pHeightOut = (*pHeightOut / scaleY);
        }
    }

    return result;
}

bool easygui_measure_string_by_element(easygui_font* pFont, const char* text, size_t textLengthInBytes, easygui_element* pElement, float* pWidthOut, float* pHeightOut)
{
    float scaleX;
    float scaleY;
    easygui_get_absolute_inner_scale(pElement, &scaleX, &scaleY);

    return easygui_measure_string(pFont, text, textLengthInBytes, scaleX, scaleY, pWidthOut, pHeightOut);
}

bool easygui_get_text_cursor_position_from_point(easygui_font* pFont, const char* text, size_t textSizeInBytes, float maxWidth, float inputPosX, float scaleX, float scaleY, float* pTextCursorPosXOut, unsigned int* pCharacterIndexOut)
{
    if (pFont == NULL) {
        return false;
    }

    assert(pFont->pContext != NULL);

    easygui_resource font = easygui_get_internal_font_by_scale(pFont, scaleY);
    if (font == NULL) {
        return false;
    }

    if (pFont->pContext->paintingCallbacks.getTextCursorPositionFromPoint) {
        return pFont->pContext->paintingCallbacks.getTextCursorPositionFromPoint(font, text, textSizeInBytes, maxWidth, inputPosX, pTextCursorPosXOut, pCharacterIndexOut);
    }

    return false;
}

bool easygui_get_text_cursor_position_from_char(easygui_font* pFont, const char* text, unsigned int characterIndex, float scaleX, float scaleY, float* pTextCursorPosXOut)
{
    if (pFont == NULL) {
        return false;
    }

    assert(pFont->pContext != NULL);

    easygui_resource font = easygui_get_internal_font_by_scale(pFont, scaleY);
    if (font == NULL) {
        return false;
    }

    if (pFont->pContext->paintingCallbacks.getTextCursorPositionFromChar) {
        return pFont->pContext->paintingCallbacks.getTextCursorPositionFromChar(font, text, characterIndex, pTextCursorPosXOut);
    }

    return false;
}



easygui_image* easygui_create_image(easygui_context* pContext, unsigned int width, unsigned int height, unsigned int stride, const void* pData)
{
    if (pContext == NULL) {
        return NULL;
    }

    if (pContext->paintingCallbacks.createImage == NULL) {
        return NULL;
    }


    easygui_resource internalImage = pContext->paintingCallbacks.createImage(pContext->pPaintingContext, width, height, stride, pData);
    if (internalImage == NULL) {
        return NULL;
    }

    easygui_image* pImage = malloc(sizeof(*pImage));
    if (pImage == NULL) {
        return NULL;
    }

    pImage->pContext  = pContext;
    pImage->hResource = internalImage;


    return pImage;
}

void easygui_delete_image(easygui_image* pImage)
{
    if (pImage == NULL) {
        return;
    }

    assert(pImage->pContext != NULL);

    // Delete the internal font object.
    if (pImage->pContext->paintingCallbacks.deleteImage) {
        pImage->pContext->paintingCallbacks.deleteImage(pImage->hResource);
    }

    // Free the font object last.
    free(pImage);
}

void easygui_get_image_size(easygui_image* pImage, unsigned int* pWidthOut, unsigned int* pHeightOut)
{
    if (pImage == NULL) {
        return;
    }

    assert(pImage->pContext != NULL);

    if (pImage->pContext->paintingCallbacks.getImageSize == NULL) {
        return;
    }

    pImage->pContext->paintingCallbacks.getImageSize(pImage->hResource, pWidthOut, pHeightOut);
}



/////////////////////////////////////////////////////////////////
//
// HIGH-LEVEL API
//
/////////////////////////////////////////////////////////////////

//// Hit Testing and Layout ////

void easygui_on_size_fit_children_to_parent(easygui_element* pElement, float newWidth, float newHeight)
{
    float scaleX;
    float scaleY;
    easygui_get_inner_scale(pElement, &scaleX, &scaleY);

    for (easygui_element* pChild = pElement->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling)
    {
        easygui_set_size(pChild, newWidth / scaleX, newHeight / scaleY);
    }
}

bool easygui_pass_through_hit_test(easygui_element* pElement, float mousePosX, float mousePosY)
{
    (void)pElement;
    (void)mousePosX;
    (void)mousePosY;

    return false;
}



//// Painting ////

void easygui_draw_border(easygui_element* pElement, float borderWidth, easygui_color color, void* pUserData)
{
    easygui_draw_rect_outline(pElement, easygui_get_local_rect(pElement), color, borderWidth, pUserData);
}



/////////////////////////////////////////////////////////////////
//
// UTILITY API
//
/////////////////////////////////////////////////////////////////

easygui_color easygui_rgba(easygui_byte r, easygui_byte g, easygui_byte b, easygui_byte a)
{
    easygui_color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;

    return color;
}

easygui_color easygui_rgb(easygui_byte r, easygui_byte g, easygui_byte b)
{
    easygui_color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = 255;

    return color;
}

easygui_rect easygui_clamp_rect(easygui_rect rect, easygui_rect other)
{
    easygui_rect result;
    result.left   = (rect.left   >= other.left)   ? rect.left   : other.left;
    result.top    = (rect.top    >= other.top)    ? rect.top    : other.top;
    result.right  = (rect.right  <= other.right)  ? rect.right  : other.right;
    result.bottom = (rect.bottom <= other.bottom) ? rect.bottom : other.bottom;

    return result;
}

bool easygui_clamp_rect_to_element(const easygui_element* pElement, easygui_rect* pRelativeRect)
{
    if (pElement == NULL || pRelativeRect == NULL) {
        return false;
    }


    if (pRelativeRect->left < 0) {
        pRelativeRect->left = 0;
    }
    if (pRelativeRect->top < 0) {
        pRelativeRect->top = 0;
    }

    if (pRelativeRect->right > pElement->width) {
        pRelativeRect->right = pElement->width;
    }
    if (pRelativeRect->bottom > pElement->height) {
        pRelativeRect->bottom = pElement->height;
    }


    return (pRelativeRect->right - pRelativeRect->left > 0) && (pRelativeRect->bottom - pRelativeRect->top > 0);
}

easygui_rect easygui_make_rect_relative(const easygui_element* pElement, easygui_rect* pRect)
{
    if (pElement == NULL || pRect == NULL) {
        return easygui_make_rect(0, 0, 0, 0);
    }

    pRect->left   -= pElement->absolutePosX;
    pRect->top    -= pElement->absolutePosY;
    pRect->right  -= pElement->absolutePosX;
    pRect->bottom -= pElement->absolutePosY;

    return *pRect;
}

easygui_rect easygui_make_rect_absolute(const easygui_element * pElement, easygui_rect * pRect)
{
    if (pElement == NULL || pRect == NULL) {
        return easygui_make_rect(0, 0, 0, 0);
    }

    pRect->left   += pElement->absolutePosX;
    pRect->top    += pElement->absolutePosY;
    pRect->right  += pElement->absolutePosX;
    pRect->bottom += pElement->absolutePosY;

    return *pRect;
}

void easygui_make_point_relative(const easygui_element* pElement, float* positionX, float* positionY)
{
    if (pElement != NULL)
    {
        if (positionX != NULL) {
            *positionX -= pElement->absolutePosX;
        }

        if (positionY != NULL) {
            *positionY -= pElement->absolutePosY;
        }
    }
}

void easygui_make_point_absolute(const easygui_element* pElement, float* positionX, float* positionY)
{
    if (pElement != NULL)
    {
        if (positionX != NULL) {
            *positionX += pElement->absolutePosX;
        }

        if (positionY != NULL) {
            *positionY += pElement->absolutePosY;
        }
    }
}

easygui_rect easygui_make_rect(float left, float top, float right, float bottom)
{
    easygui_rect rect;
    rect.left   = left;
    rect.top    = top;
    rect.right  = right;
    rect.bottom = bottom;

    return rect;
}

easygui_rect easygui_make_inside_out_rect()
{
    easygui_rect rect;
    rect.left   =  FLT_MAX;
    rect.top    =  FLT_MAX;
    rect.right  = -FLT_MAX;
    rect.bottom = -FLT_MAX;

    return rect;
}

easygui_rect easygui_grow_rect(easygui_rect rect, float amount)
{
    easygui_rect result = rect;
    result.left   -= amount;
    result.top    -= amount;
    result.right  += amount;
    result.bottom += amount;

    return result;
}

easygui_rect easygui_scale_rect(easygui_rect rect, float scaleX, float scaleY)
{
    easygui_rect result = rect;
    result.left   *= scaleX;
    result.top    *= scaleY;
    result.right  *= scaleX;
    result.bottom *= scaleY;

    return result;
}

easygui_rect easygui_offset_rect(easygui_rect rect, float offsetX, float offsetY)
{
    return easygui_make_rect(rect.left + offsetX, rect.top + offsetY, rect.right + offsetX, rect.bottom + offsetY);
}

easygui_rect easygui_rect_union(easygui_rect rect0, easygui_rect rect1)
{
    easygui_rect result;
    result.left   = (rect0.left   < rect1.left)   ? rect0.left   : rect1.left;
    result.top    = (rect0.top    < rect1.top)    ? rect0.top    : rect1.top;
    result.right  = (rect0.right  > rect1.right)  ? rect0.right  : rect1.right;
    result.bottom = (rect0.bottom > rect1.bottom) ? rect0.bottom : rect1.bottom;

    return result;
}

bool easygui_rect_contains_point(easygui_rect rect, float posX, float posY)
{
    if (posX < rect.left || posY < rect.top) {
        return false;
    }

    if (posX >= rect.right || posY >= rect.bottom) {
        return false;
    }

    return true;
}

bool easygui_rect_equal(easygui_rect rect0, easygui_rect rect1)
{
    return
        rect0.left   == rect1.left  &&
        rect0.top    == rect1.top   &&
        rect0.right  == rect1.right &&
        rect0.bottom == rect1.bottom;
}




/////////////////////////////////////////////////////////////////
//
// EASY_DRAW-SPECIFIC API
//
/////////////////////////////////////////////////////////////////
#ifndef EASYGUI_NO_EASY_DRAW

void easygui_draw_begin_easy_draw(void* pPaintData);
void easygui_draw_end_easy_draw(void* pPaintData);
void easygui_set_clip_easy_draw(easygui_rect rect, void* pPaintData);
void easygui_get_clip_easy_draw(easygui_rect* pRectOut, void* pPaintData);
void easygui_draw_rect_easy_draw(easygui_rect rect, easygui_color color, void* pPaintData);
void easygui_draw_rect_outline_easy_draw(easygui_rect, easygui_color, float, void*);
void easygui_draw_rect_with_outline_easy_draw(easygui_rect, easygui_color, float, easygui_color, void*);
void easygui_draw_round_rect_easy_draw(easygui_rect, easygui_color, float, void*);
void easygui_draw_round_rect_outline_easy_draw(easygui_rect, easygui_color, float, float, void*);
void easygui_draw_round_rect_with_outline_easy_draw(easygui_rect, easygui_color, float, float, easygui_color, void*);
void easygui_draw_text_easy_draw(easygui_resource, const char*, int, float, float, easygui_color, easygui_color, void*);
void easygui_draw_image_easy_draw(easygui_resource image, easygui_draw_image_args* pArgs, void* pPaintData);

easygui_resource easygui_create_font_easy_draw(void*, const char*, unsigned int, easygui_font_weight, easygui_font_slant, float);
void easygui_delete_font_easy_draw(easygui_resource);
unsigned int easygui_get_font_size_easy_draw(easygui_resource hFont);
bool easygui_get_font_metrics_easy_draw(easygui_resource, easygui_font_metrics*);
bool easygui_get_glyph_metrics_easy_draw(easygui_resource, unsigned int, easygui_glyph_metrics*);
bool easygui_measure_string_easy_draw(easygui_resource, const char*, size_t, float*, float*);
bool easygui_get_text_cursor_position_from_point_easy_draw(easygui_resource font, const char* text, size_t textSizeInBytes, float maxWidth, float inputPosX, float* pTextCursorPosXOut, unsigned int* pCharacterIndexOut);
bool easygui_get_text_cursor_position_from_char_easy_draw(easygui_resource font, const char* text, unsigned int characterIndex, float* pTextCursorPosXOut);

easygui_resource easygui_create_image_easy_draw(void* pPaintingContext, unsigned int width, unsigned int height, unsigned int stride, const void* pImageData);
void easygui_delete_image_easy_draw(easygui_resource image);
void easygui_get_image_size_easy_draw(easygui_resource image, unsigned int* pWidthOut, unsigned int* pHeightOut);

easygui_context* easygui_create_context_easy_draw(easy2d_context* pDrawingContext)
{
    easygui_context* pContext = easygui_create_context();
    if (pContext != NULL) {
        easygui_register_easy_draw_callbacks(pContext, pDrawingContext);
    }

    return pContext;
}

void easygui_register_easy_draw_callbacks(easygui_context* pContext, easy2d_context* pDrawingContext)
{
    easygui_painting_callbacks callbacks;
    callbacks.drawBegin                      = easygui_draw_begin_easy_draw;
    callbacks.drawEnd                        = easygui_draw_end_easy_draw;
    callbacks.setClip                        = easygui_set_clip_easy_draw;
    callbacks.getClip                        = easygui_get_clip_easy_draw;
    callbacks.drawRect                       = easygui_draw_rect_easy_draw;
    callbacks.drawRectOutline                = easygui_draw_rect_outline_easy_draw;
    callbacks.drawRectWithOutline            = easygui_draw_rect_with_outline_easy_draw;
    callbacks.drawRoundRect                  = easygui_draw_round_rect_easy_draw;
    callbacks.drawRoundRectOutline           = easygui_draw_round_rect_outline_easy_draw;
    callbacks.drawRoundRectWithOutline       = easygui_draw_round_rect_with_outline_easy_draw;
    callbacks.drawText                       = easygui_draw_text_easy_draw;
    callbacks.drawImage                      = easygui_draw_image_easy_draw;

    callbacks.createFont                     = easygui_create_font_easy_draw;
    callbacks.deleteFont                     = easygui_delete_font_easy_draw;
    callbacks.getFontSize                    = easygui_get_font_size_easy_draw;
    callbacks.getFontMetrics                 = easygui_get_font_metrics_easy_draw;
    callbacks.getGlyphMetrics                = easygui_get_glyph_metrics_easy_draw;
    callbacks.measureString                  = easygui_measure_string_easy_draw;

    callbacks.createImage                    = easygui_create_image_easy_draw;
    callbacks.deleteImage                    = easygui_delete_image_easy_draw;
    callbacks.getImageSize                   = easygui_get_image_size_easy_draw;
    callbacks.getTextCursorPositionFromPoint = easygui_get_text_cursor_position_from_point_easy_draw;
    callbacks.getTextCursorPositionFromChar  = easygui_get_text_cursor_position_from_char_easy_draw;

    easygui_register_painting_callbacks(pContext, pDrawingContext, callbacks);
}


void easygui_draw_begin_easy_draw(void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_begin_draw(pSurface);
}

void easygui_draw_end_easy_draw(void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_end_draw(pSurface);
}

void easygui_set_clip_easy_draw(easygui_rect rect, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_set_clip(pSurface, rect.left, rect.top, rect.right, rect.bottom);
}

void easygui_get_clip_easy_draw(easygui_rect* pRectOut, void* pPaintData)
{
    assert(pRectOut != NULL);

    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_get_clip(pSurface, &pRectOut->left, &pRectOut->top, &pRectOut->right, &pRectOut->bottom);
}

void easygui_draw_rect_easy_draw(easygui_rect rect, easygui_color color, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_draw_rect(pSurface, rect.left, rect.top, rect.right, rect.bottom, easy2d_rgba(color.r, color.g, color.b, color.a));
}

void easygui_draw_rect_outline_easy_draw(easygui_rect rect, easygui_color color, float outlineWidth, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_draw_rect_outline(pSurface, rect.left, rect.top, rect.right, rect.bottom, easy2d_rgba(color.r, color.g, color.b, color.a), outlineWidth);
}

void easygui_draw_rect_with_outline_easy_draw(easygui_rect rect, easygui_color color, float outlineWidth, easygui_color outlineColor, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_draw_rect_with_outline(pSurface, rect.left, rect.top, rect.right, rect.bottom, easy2d_rgba(color.r, color.g, color.b, color.a), outlineWidth, easy2d_rgba(outlineColor.r, outlineColor.g, outlineColor.b, outlineColor.a));
}

void easygui_draw_round_rect_easy_draw(easygui_rect rect, easygui_color color, float radius, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_draw_round_rect(pSurface, rect.left, rect.top, rect.right, rect.bottom, easy2d_rgba(color.r, color.g, color.b, color.a), radius);
}

void easygui_draw_round_rect_outline_easy_draw(easygui_rect rect, easygui_color color, float radius, float outlineWidth, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_draw_round_rect_outline(pSurface, rect.left, rect.top, rect.right, rect.bottom, easy2d_rgba(color.r, color.g, color.b, color.a), radius, outlineWidth);
}

void easygui_draw_round_rect_with_outline_easy_draw(easygui_rect rect, easygui_color color, float radius, float outlineWidth, easygui_color outlineColor, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_draw_round_rect_with_outline(pSurface, rect.left, rect.top, rect.right, rect.bottom, easy2d_rgba(color.r, color.g, color.b, color.a), radius, outlineWidth, easy2d_rgba(outlineColor.r, outlineColor.g, outlineColor.b, outlineColor.a));
}

void easygui_draw_text_easy_draw(easygui_resource font, const char* text, int textSizeInBytes, float posX, float posY, easygui_color color, easygui_color backgroundColor, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_draw_text(pSurface, font, text, textSizeInBytes, posX, posY, easy2d_rgba(color.r, color.g, color.b, color.a), easy2d_rgba(backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a));
}

void easygui_draw_image_easy_draw(easygui_resource image, easygui_draw_image_args* pArgs, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_draw_image_args args;
    args.dstX            = pArgs->dstX;
    args.dstY            = pArgs->dstY;
    args.dstWidth        = pArgs->dstWidth;
    args.dstHeight       = pArgs->dstHeight;
    args.srcX            = pArgs->srcX;
    args.srcY            = pArgs->srcY;
    args.srcWidth        = pArgs->srcWidth;
    args.srcHeight       = pArgs->srcHeight;
    args.dstBoundsX      = pArgs->dstBoundsX;
    args.dstBoundsY      = pArgs->dstBoundsY;
    args.dstBoundsWidth  = pArgs->dstBoundsWidth;
    args.dstBoundsHeight = pArgs->dstBoundsHeight;
    args.foregroundTint  = easy2d_rgba(pArgs->foregroundTint.r, pArgs->foregroundTint.g, pArgs->foregroundTint.b, pArgs->foregroundTint.a);
    args.backgroundColor = easy2d_rgba(pArgs->backgroundColor.r, pArgs->backgroundColor.g, pArgs->backgroundColor.b, pArgs->backgroundColor.a);
    args.boundsColor     = easy2d_rgba(pArgs->boundsColor.r, pArgs->boundsColor.g, pArgs->boundsColor.b, pArgs->boundsColor.a);
    args.options         = pArgs->options;
    easy2d_draw_image(pSurface, image, &args);
}


easygui_resource easygui_create_font_easy_draw(void* pPaintingContext, const char* family, unsigned int size, easygui_font_weight weight, easygui_font_slant slant, float rotation)
{
    return easy2d_create_font(pPaintingContext, family, size, weight, slant, rotation);
}

void easygui_delete_font_easy_draw(easygui_resource font)
{
    easy2d_delete_font(font);
}

unsigned int easygui_get_font_size_easy_draw(easygui_resource font)
{
    return easy2d_get_font_size(font);
}

bool easygui_get_font_metrics_easy_draw(easygui_resource font, easygui_font_metrics* pMetricsOut)
{
    assert(pMetricsOut != NULL);

    easy2d_font_metrics metrics;
    if (!easy2d_get_font_metrics(font, &metrics)) {
        return false;
    }

    pMetricsOut->ascent     = metrics.ascent;
    pMetricsOut->descent    = metrics.descent;
    pMetricsOut->lineHeight = metrics.lineHeight;
    pMetricsOut->spaceWidth = metrics.spaceWidth;

    return true;
}

bool easygui_get_glyph_metrics_easy_draw(easygui_resource font, unsigned int utf32, easygui_glyph_metrics* pMetricsOut)
{
    assert(pMetricsOut != NULL);

    easy2d_glyph_metrics metrics;
    if (!easy2d_get_glyph_metrics(font, utf32, &metrics)) {
        return false;
    }

    pMetricsOut->width    = metrics.width;
    pMetricsOut->height   = metrics.height;
    pMetricsOut->originX  = metrics.originX;
    pMetricsOut->originY  = metrics.originY;
    pMetricsOut->advanceX = metrics.advanceX;
    pMetricsOut->advanceY = metrics.advanceY;

    return true;
}

bool easygui_measure_string_easy_draw(easygui_resource font, const char* text, size_t textSizeInBytes, float* pWidthOut, float* pHeightOut)
{
    return easy2d_measure_string(font, text, textSizeInBytes, pWidthOut, pHeightOut);
}

bool easygui_get_text_cursor_position_from_point_easy_draw(easygui_resource font, const char* text, size_t textSizeInBytes, float maxWidth, float inputPosX, float* pTextCursorPosXOut, unsigned int* pCharacterIndexOut)
{
    return easy2d_get_text_cursor_position_from_point(font, text, textSizeInBytes, maxWidth, inputPosX, pTextCursorPosXOut, pCharacterIndexOut);
}

bool easygui_get_text_cursor_position_from_char_easy_draw(easygui_resource font, const char* text, unsigned int characterIndex, float* pTextCursorPosXOut)
{
    return easy2d_get_text_cursor_position_from_char(font, text, characterIndex, pTextCursorPosXOut);
}


easygui_resource easygui_create_image_easy_draw(void* pPaintingContext, unsigned int width, unsigned int height, unsigned int stride, const void* pImageData)
{
    return easy2d_create_image(pPaintingContext, width, height, stride, pImageData);
}

void easygui_delete_image_easy_draw(easygui_resource image)
{
    easy2d_delete_image(image);
}

void easygui_get_image_size_easy_draw(easygui_resource image, unsigned int* pWidthOut, unsigned int* pHeightOut)
{
    easy2d_get_image_size(image, pWidthOut, pHeightOut);
}

#endif

