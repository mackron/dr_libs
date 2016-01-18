// Public domain. See "unlicense" statement at the end of this file.

#include "easygui_scrollbar.h"
#include <math.h>       // For round()
#include <assert.h>

//#include <stdio.h>      // For testing. Delete me.

#define EASYGUI_MIN_SCROLLBAR_THUMB_SIZE    8

typedef struct
{
    /// The orientation.
    sb_orientation orientation;

    /// The minimum scroll range.
    int rangeMin;

    /// The maximum scroll range.
    int rangeMax;

    /// The page size.
    int pageSize;

    /// The current scroll position.
    int scrollPos;

    /// Whether or not to auto-hide the thumb.
    bool autoHideThumb;

    /// The mouse wheel scale.
    int mouseWheelScale;

    /// The color of the track.
    easygui_color trackColor;

    /// The color of the thumb while not hovered or pressed.
    easygui_color thumbColor;

    /// The color of the thumb while hovered.
    easygui_color thumbColorHovered;

    /// The color of the thumb while pressed.
    easygui_color thumbColorPressed;

    /// The function to call when the scroll position changes.
    sb_on_scroll_proc onScroll;


    /// The current size of the thumb.
    float thumbSize;

    /// The current position of the thumb.
    float thumbPos;

    /// The amount of padding between the edge of the scrollbar and the thumb.
    float thumbPadding;

    /// Whether or not we are hovered over the thumb.
    bool thumbHovered;

    /// Whether or not the thumb is pressed.
    bool thumbPressed;

    /// The relative position of the mouse on the x axis at the time the thumb was pressed with the mouse.
    float thumbClickPosX;

    /// The relative position of the mouse on the y axis at the time the thumb was pressed with the mouse.
    float thumbClickPosY;


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data.
    char pExtraData[1];

} easygui_scrollbar;


/// Refreshes the given scrollbar's thumb layout and redraws it.
PRIVATE void sb_refresh_thumb(easygui_element* pSBElement);

/// Calculates the size of the thumb. This does not change the state of the thumb.
PRIVATE float sb_calculate_thumb_size(easygui_element* pSBElement);

/// Calculates the position of the thumb. This does not change the state of the thumb.
PRIVATE float sb_calculate_thumb_position(easygui_element* pSBElement);

/// Retrieves the size of the given scrollbar's track. For vertical alignments, it's the height of the element, otherwise it's the width.
PRIVATE float sb_get_track_size(easygui_element* pSBElement);

/// Makes the given point that's relative to the given scrollbar relative to it's thumb.
PRIVATE void sb_make_relative_to_thumb(easygui_element* pSBElement, float* pPosX, float* pPosY);

/// Calculates the scroll position based on the current position of the thumb. This is used for scrolling while dragging the thumb.
PRIVATE int sb_calculate_scroll_pos_from_thumb_pos(easygui_element* pScrollba, float thumbPosr);

/// Simple clamp function.
PRIVATE float sb_clampf(float n, float lower, float upper)
{
    return n <= lower ? lower : n >= upper ? upper : n;
}

/// Simple clamp function.
PRIVATE int sb_clampi(int n, int lower, int upper)
{
    return n <= lower ? lower : n >= upper ? upper : n;
}

/// Simple max function.
PRIVATE int sb_maxi(int x, int y)
{
    return (x > y) ? x : y;
}


easygui_element* easygui_create_scrollbar(easygui_context* pContext, easygui_element* pParent, sb_orientation orientation, size_t extraDataSize, const void* pExtraData)
{
    if (pContext == NULL || orientation == sb_orientation_none) {
        return NULL;
    }

    easygui_element* pSBElement = easygui_create_element(pContext, pParent, sizeof(easygui_scrollbar) - sizeof(char) + extraDataSize);
    if (pSBElement == NULL) {
        return NULL;
    }

    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    pSB->orientation       = orientation;
    pSB->rangeMin          = 0;
    pSB->rangeMax          = 0;
    pSB->pageSize          = 0;
    pSB->scrollPos         = 0;
    pSB->autoHideThumb     = true;
    pSB->mouseWheelScale   = 1;
    pSB->trackColor        = easygui_rgb(80, 80, 80);
    pSB->thumbColor        = easygui_rgb(112, 112, 112);
    pSB->thumbColorHovered = easygui_rgb(144, 144, 144);
    pSB->thumbColorPressed = easygui_rgb(180, 180, 180);
    pSB->onScroll          = NULL;

    pSB->thumbSize         = EASYGUI_MIN_SCROLLBAR_THUMB_SIZE;
    pSB->thumbPos          = 0;
    pSB->thumbPadding      = 2;
    pSB->thumbHovered      = false;
    pSB->thumbPressed      = false;
    pSB->thumbClickPosX    = 0;
    pSB->thumbClickPosY    = 0;

    pSB->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pSB->pExtraData, pExtraData, extraDataSize);
    }


    // Default event handlers.
    easygui_set_on_size(pSBElement, sb_on_size);
    easygui_set_on_mouse_leave(pSBElement, sb_on_mouse_leave);
    easygui_set_on_mouse_move(pSBElement, sb_on_mouse_move);
    easygui_set_on_mouse_button_down(pSBElement, sb_on_mouse_button_down);
    easygui_set_on_mouse_button_up(pSBElement, sb_on_mouse_button_up);
    easygui_set_on_mouse_wheel(pSBElement, sb_on_mouse_wheel);
    easygui_set_on_paint(pSBElement, sb_on_paint);


    return pSBElement;
}

void easygui_delete_scrollbar(easygui_element* pSBElement)
{
    if (pSBElement == NULL) {
        return;
    }

    easygui_delete_element(pSBElement);
}


size_t sb_get_extra_data_size(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return 0;
    }

    return pSB->extraDataSize;
}

void* sb_get_extra_data(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return NULL;
    }

    return pSB->pExtraData;
}


sb_orientation sb_get_orientation(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return sb_orientation_none;
    }

    return pSB->orientation;
}


void sb_set_range(easygui_element* pSBElement, int rangeMin, int rangeMax)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->rangeMin = rangeMin;
    pSB->rangeMax = rangeMax;

    
    // Make sure the scroll position is still valid.
    sb_scroll_to(pSBElement, sb_get_scroll_position(pSBElement));

    // The thumb may have changed, so refresh it.
    sb_refresh_thumb(pSBElement);
}

void sb_get_range(easygui_element* pSBElement, int* pRangeMinOut, int* pRangeMaxOut)
{    
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (pRangeMinOut != NULL) {
        *pRangeMinOut = pSB->rangeMin;
    }
    
    if (pRangeMaxOut != NULL) {
        *pRangeMaxOut = pSB->rangeMax;
    }
}


void sb_set_page_size(easygui_element* pSBElement, int pageSize)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->pageSize = pageSize;


    // Make sure the scroll position is still valid.
    sb_scroll_to(pSBElement, sb_get_scroll_position(pSBElement));

    // The thumb may have changed, so refresh it.
    sb_refresh_thumb(pSBElement);
}

int sb_get_page_size(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return 0;
    }

    return pSB->pageSize;
}


void sb_set_range_and_page_size(easygui_element* pSBElement, int rangeMin, int rangeMax, int pageSize)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->rangeMin = rangeMin;
    pSB->rangeMax = rangeMax;
    pSB->pageSize = pageSize;

    
    // Make sure the scroll position is still valid.
    sb_scroll_to(pSBElement, sb_get_scroll_position(pSBElement));

    // The thumb may have changed, so refresh it.
    sb_refresh_thumb(pSBElement);
}


void sb_set_scroll_position(easygui_element* pSBElement, int position)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    int newScrollPos = sb_clampi(position, pSB->rangeMin, sb_maxi(0, pSB->rangeMax - pSB->pageSize + 1));
    if (newScrollPos != pSB->scrollPos)
    {
        pSB->scrollPos = newScrollPos;

        // The position of the thumb has changed, so refresh it.
        sb_refresh_thumb(pSBElement);
    }
}

int sb_get_scroll_position(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return 0;
    }

    return pSB->scrollPos;
}


void sb_scroll(easygui_element* pSBElement, int offset)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    sb_scroll_to(pSBElement, pSB->scrollPos + offset);
}

void sb_scroll_to(easygui_element* pSBElement, int newScrollPos)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    int oldScrollPos = pSB->scrollPos;
    sb_set_scroll_position(pSBElement, newScrollPos);

    if (oldScrollPos != pSB->scrollPos)
    {
        if (pSB->onScroll) {
            pSB->onScroll(pSBElement, pSB->scrollPos);
        }
    }
}


void sb_enable_thumb_auto_hide(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (pSB->autoHideThumb != true)
    {
        pSB->autoHideThumb = true;

        // The thumb needs to be refreshed in order to show the correct state.
        sb_refresh_thumb(pSBElement);
    }
}

void sb_disable_thumb_auto_hide(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (pSB->autoHideThumb != false)
    {
        pSB->autoHideThumb = false;

        // The thumb needs to be refreshed in order to show the correct state.
        sb_refresh_thumb(pSBElement);
    }
}

bool sb_is_thumb_auto_hide_enabled(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return false;
    }

    return pSB->autoHideThumb;
}

bool sb_is_thumb_visible(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return false;
    }

    // Always visible if auto-hiding is disabled.
    if (!pSB->autoHideThumb) {
        return true;
    }

    return pSB->pageSize < (pSB->rangeMax - pSB->rangeMin + 1) && pSB->pageSize > 0;
}


void sb_set_mouse_wheel_scele(easygui_element* pSBElement, int scale)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->mouseWheelScale = scale;
}

int sb_get_mouse_wheel_scale(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return 1;
    }

    return pSB->mouseWheelScale;
}


void sb_set_track_color(easygui_element* pSBElement, easygui_color color)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->trackColor = color;
}

void sb_set_default_thumb_color(easygui_element* pSBElement, easygui_color color)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->thumbColor = color;
}

void sb_set_hovered_thumb_color(easygui_element* pSBElement, easygui_color color)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->thumbColorHovered = color;
}

void sb_set_pressed_thumb_color(easygui_element* pSBElement, easygui_color color)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->thumbColorPressed = color;
}


void sb_set_on_scroll(easygui_element* pSBElement, sb_on_scroll_proc onScroll)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->onScroll = onScroll;
}

sb_on_scroll_proc sb_get_on_scroll(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return NULL;
    }

    return pSB->onScroll;
}


easygui_rect sb_get_thumb_rect(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return easygui_make_rect(0, 0, 0, 0);
    }

    easygui_rect rect = {0, 0, 0, 0};
    rect.left = pSB->thumbPadding;
    rect.top  = pSB->thumbPadding;
    
    if (pSB->orientation == sb_orientation_vertical)
    {
        // Vertical.
        rect.left   = pSB->thumbPadding;
        rect.right  = easygui_get_width(pSBElement) - pSB->thumbPadding;
        rect.top    = pSB->thumbPadding + pSB->thumbPos;
        rect.bottom = rect.top + pSB->thumbSize;
    }
    else
    {
        // Horizontal.
        rect.left   = pSB->thumbPadding + pSB->thumbPos;
        rect.right  = rect.left + pSB->thumbSize;
        rect.top    = pSB->thumbPadding;
        rect.bottom = easygui_get_height(pSBElement) - pSB->thumbPadding;
    }

    return rect;
}


void sb_on_size(easygui_element* pSBElement, float newWidth, float newHeight)
{
    (void)newWidth;
    (void)newHeight;

    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    sb_refresh_thumb(pSBElement);
}

void sb_on_mouse_leave(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    bool needsRedraw = false;
    if (pSB->thumbHovered)
    {
        needsRedraw = true;
        pSB->thumbHovered = false;
    }

    if (pSB->thumbPressed)
    {
        needsRedraw = true;
        pSB->thumbPressed = false;
    }

    if (needsRedraw) {
        easygui_dirty(pSBElement, sb_get_thumb_rect(pSBElement));
    }
}

void sb_on_mouse_move(easygui_element* pSBElement, int relativeMousePosX, int relativeMousePosY)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (pSB->thumbPressed)
    {
        // The thumb is pressed. Drag it.
        float thumbRelativeMousePosX = (float)relativeMousePosX;
        float thumbRelativeMousePosY = (float)relativeMousePosY;
        sb_make_relative_to_thumb(pSBElement, &thumbRelativeMousePosX, &thumbRelativeMousePosY);

        float dragOffsetX = thumbRelativeMousePosX - pSB->thumbClickPosX;
        float dragOffsetY = thumbRelativeMousePosY - pSB->thumbClickPosY;

        float destTrackPos = pSB->thumbPos;
        if (pSB->orientation == sb_orientation_vertical) {
            destTrackPos += dragOffsetY;
        } else {
            destTrackPos += dragOffsetX;
        }

        int destScrollPos = sb_calculate_scroll_pos_from_thumb_pos(pSBElement, destTrackPos);
        if (destScrollPos != pSB->scrollPos)
        {
            sb_scroll_to(pSBElement, destScrollPos);
        }
    }
    else
    {
        // The thumb is not pressed. We just need to check if the hovered state has change and redraw if required.
        if (sb_is_thumb_visible(pSBElement))
        {
            bool wasThumbHovered = pSB->thumbHovered;
        
            easygui_rect thumbRect = sb_get_thumb_rect(pSBElement);
            pSB->thumbHovered = easygui_rect_contains_point(thumbRect, (float)relativeMousePosX, (float)relativeMousePosY);
        
            if (wasThumbHovered != pSB->thumbHovered) {
                easygui_dirty(pSBElement, thumbRect);
            }
        }
    }
}

void sb_on_mouse_button_down(easygui_element* pSBElement, int button, int relativeMousePosX, int relativeMousePosY)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (button == EASYGUI_MOUSE_BUTTON_LEFT)
    {
        if (sb_is_thumb_visible(pSBElement))
        {
            easygui_rect thumbRect = sb_get_thumb_rect(pSBElement);
            if (easygui_rect_contains_point(thumbRect, (float)relativeMousePosX, (float)relativeMousePosY))
            {
                if (!pSB->thumbPressed)
                {
                    easygui_capture_mouse(pSBElement);
                    pSB->thumbPressed = true;

                    pSB->thumbClickPosX = (float)relativeMousePosX;
                    pSB->thumbClickPosY = (float)relativeMousePosY;
                    sb_make_relative_to_thumb(pSBElement, &pSB->thumbClickPosX, &pSB->thumbClickPosY);

                    easygui_dirty(pSBElement, sb_get_thumb_rect(pSBElement));
                }
            }
            else
            {
                // If the click position is above the thumb we want to scroll up by a page. If it's below the thumb, we scroll down by a page.
                if (relativeMousePosY < thumbRect.top) {
                    sb_scroll(pSBElement, -sb_get_page_size(pSBElement));
                } else if (relativeMousePosY >= thumbRect.bottom) {
                    sb_scroll(pSBElement,  sb_get_page_size(pSBElement));
                }
            }
        }
    }
}

void sb_on_mouse_button_up(easygui_element* pSBElement, int button, int relativeMousePosX, int relativeMousePosY)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;

    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (button == EASYGUI_MOUSE_BUTTON_LEFT)
    {
        if (pSB->thumbPressed && easygui_get_element_with_mouse_capture(pSBElement->pContext) == pSBElement)
        {
            easygui_release_mouse(pSBElement->pContext);
            pSB->thumbPressed = false;

            easygui_dirty(pSBElement, sb_get_thumb_rect(pSBElement));
        }
    }
}

void sb_on_mouse_wheel(easygui_element* pSBElement, int delta, int relativeMousePosX, int relativeMousePosY)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;

    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    sb_scroll(pSBElement, -delta * sb_get_mouse_wheel_scale(pSBElement));
}

void sb_on_paint(easygui_element* pSBElement, easygui_rect relativeClippingRect, void* pPaintData)
{
    (void)relativeClippingRect;

    const easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    easygui_rect thumbRect = sb_get_thumb_rect(pSBElement);

    if (sb_is_thumb_visible(pSBElement))
    {
        // The thumb is visible.

        // Track. We draw this in 4 seperate pieces so we can avoid overdraw with the thumb.
        easygui_draw_rect(pSBElement, easygui_make_rect(0, 0, easygui_get_width(pSBElement), thumbRect.top), pSB->trackColor, pPaintData);  // Top
        easygui_draw_rect(pSBElement, easygui_make_rect(0, thumbRect.bottom, easygui_get_width(pSBElement), easygui_get_height(pSBElement)), pSB->trackColor, pPaintData);  // Bottom
        easygui_draw_rect(pSBElement, easygui_make_rect(0, thumbRect.top, thumbRect.left, thumbRect.bottom), pSB->trackColor, pPaintData);  // Left
        easygui_draw_rect(pSBElement, easygui_make_rect(thumbRect.right, thumbRect.top, easygui_get_width(pSBElement), thumbRect.bottom), pSB->trackColor, pPaintData); // Right

        // Thumb.
        easygui_color thumbColor;
        if (pSB->thumbPressed) {
            thumbColor = pSB->thumbColorPressed;
        } else if (pSB->thumbHovered) {
            thumbColor = pSB->thumbColorHovered;
        } else {
            thumbColor = pSB->thumbColor;
        }

        easygui_draw_rect(pSBElement, thumbRect, thumbColor, pPaintData);
    }
    else
    {
        // The thumb is not visible - just draw the track as one quad.
        easygui_draw_rect(pSBElement, easygui_get_local_rect(pSBElement), pSB->trackColor, pPaintData);
    }
}



PRIVATE void sb_refresh_thumb(easygui_element* pSBElement)
{
    easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    easygui_rect oldThumbRect = sb_get_thumb_rect(pSBElement);

    pSB->thumbSize = sb_calculate_thumb_size(pSBElement);
    pSB->thumbPos  = sb_calculate_thumb_position(pSBElement);

    easygui_rect newThumbRect = sb_get_thumb_rect(pSBElement);
    if (!easygui_rect_equal(oldThumbRect, newThumbRect))
    {
        easygui_dirty(pSBElement, easygui_rect_union(oldThumbRect, newThumbRect));
    }
}

PRIVATE float sb_calculate_thumb_size(easygui_element* pSBElement)
{
    const easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    float trackSize = sb_get_track_size(pSBElement);
    float range = (float)(pSB->rangeMax - pSB->rangeMin + 1);
        
    float thumbSize = EASYGUI_MIN_SCROLLBAR_THUMB_SIZE;
    if (range > 0)
    {
        thumbSize = roundf((trackSize / range) * pSB->pageSize);
        thumbSize = sb_clampf(thumbSize, EASYGUI_MIN_SCROLLBAR_THUMB_SIZE, trackSize);
    }

    return thumbSize;
}

PRIVATE float sb_calculate_thumb_position(easygui_element* pSBElement)
{
    const easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    float trackSize = sb_get_track_size(pSBElement);
    float thumbSize = sb_calculate_thumb_size(pSBElement);
    float range = (float)(pSB->rangeMax - pSB->rangeMin + 1);

    float thumbPos = 0;
    if (range > pSB->pageSize)
    {
        thumbPos = roundf((trackSize / range) * pSB->scrollPos);
        thumbPos = sb_clampf(thumbPos, 0, trackSize - thumbSize);
    }

    return thumbPos;
}

PRIVATE float sb_get_track_size(easygui_element* pSBElement)
{
    const easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    if (pSB->orientation == sb_orientation_vertical) {
        return easygui_get_height(pSBElement) - (pSB->thumbPadding*2);
    } else {
        return easygui_get_width(pSBElement) - (pSB->thumbPadding*2);
    }
}

PRIVATE void sb_make_relative_to_thumb(easygui_element* pSBElement, float* pPosX, float* pPosY)
{
    easygui_rect thumbRect = sb_get_thumb_rect(pSBElement);

    if (pPosX != NULL) {
        *pPosX -= thumbRect.left;
    }

    if (pPosY != NULL) {
        *pPosY -= thumbRect.top;
    }
}

PRIVATE int sb_calculate_scroll_pos_from_thumb_pos(easygui_element* pSBElement, float thumbPos)
{
    const easygui_scrollbar* pSB = easygui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    float trackSize = sb_get_track_size(pSBElement);
    float range     = (float)(pSB->rangeMax - pSB->rangeMin + 1);

    return (int)roundf(thumbPos / (trackSize / range));
}


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