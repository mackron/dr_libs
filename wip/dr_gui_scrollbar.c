// Public domain. See "unlicense" statement at the end of this file.

#include "dr_gui_scrollbar.h"
#include <math.h>       // For round()
#include <string.h>
#include <assert.h>

#ifndef DRGUI_PRIVATE
#define DRGUI_PRIVATE static
#endif

#define DRGUI_MIN_SCROLLBAR_THUMB_SIZE    8

typedef struct
{
    /// The orientation.
    drgui_sb_orientation orientation;

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
    drgui_color trackColor;

    /// The color of the thumb while not hovered or pressed.
    drgui_color thumbColor;

    /// The color of the thumb while hovered.
    drgui_color thumbColorHovered;

    /// The color of the thumb while pressed.
    drgui_color thumbColorPressed;

    /// The function to call when the scroll position changes.
    drgui_sb_on_scroll_proc onScroll;


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

} drgui_scrollbar;


/// Refreshes the given scrollbar's thumb layout and redraws it.
DRGUI_PRIVATE void drgui_sb_refresh_thumb(drgui_element* pSBElement);

/// Calculates the size of the thumb. This does not change the state of the thumb.
DRGUI_PRIVATE float drgui_sb_calculate_thumb_size(drgui_element* pSBElement);

/// Calculates the position of the thumb. This does not change the state of the thumb.
DRGUI_PRIVATE float drgui_sb_calculate_thumb_position(drgui_element* pSBElement);

/// Retrieves the size of the given scrollbar's track. For vertical alignments, it's the height of the element, otherwise it's the width.
DRGUI_PRIVATE float drgui_sb_get_track_size(drgui_element* pSBElement);

/// Makes the given point that's relative to the given scrollbar relative to it's thumb.
DRGUI_PRIVATE void drgui_sb_make_relative_to_thumb(drgui_element* pSBElement, float* pPosX, float* pPosY);

/// Calculates the scroll position based on the current position of the thumb. This is used for scrolling while dragging the thumb.
DRGUI_PRIVATE int drgui_sb_calculate_scroll_pos_from_thumb_pos(drgui_element* pScrollba, float thumbPosr);

/// Simple clamp function.
DRGUI_PRIVATE float drgui_sb_clampf(float n, float lower, float upper)
{
    return n <= lower ? lower : n >= upper ? upper : n;
}

/// Simple clamp function.
DRGUI_PRIVATE int drgui_sb_clampi(int n, int lower, int upper)
{
    return n <= lower ? lower : n >= upper ? upper : n;
}

/// Simple max function.
DRGUI_PRIVATE int drgui_sb_maxi(int x, int y)
{
    return (x > y) ? x : y;
}


drgui_element* drgui_create_scrollbar(drgui_context* pContext, drgui_element* pParent, drgui_sb_orientation orientation, size_t extraDataSize, const void* pExtraData)
{
    if (pContext == NULL || orientation == drgui_sb_orientation_none) {
        return NULL;
    }

    drgui_element* pSBElement = drgui_create_element(pContext, pParent, sizeof(drgui_scrollbar) - sizeof(char) + extraDataSize, NULL);
    if (pSBElement == NULL) {
        return NULL;
    }

    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    pSB->orientation       = orientation;
    pSB->rangeMin          = 0;
    pSB->rangeMax          = 0;
    pSB->pageSize          = 0;
    pSB->scrollPos         = 0;
    pSB->autoHideThumb     = true;
    pSB->mouseWheelScale   = 1;
    pSB->trackColor        = drgui_rgb(80, 80, 80);
    pSB->thumbColor        = drgui_rgb(112, 112, 112);
    pSB->thumbColorHovered = drgui_rgb(144, 144, 144);
    pSB->thumbColorPressed = drgui_rgb(180, 180, 180);
    pSB->onScroll          = NULL;

    pSB->thumbSize         = DRGUI_MIN_SCROLLBAR_THUMB_SIZE;
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
    drgui_set_on_size(pSBElement, drgui_sb_on_size);
    drgui_set_on_mouse_leave(pSBElement, drgui_sb_on_mouse_leave);
    drgui_set_on_mouse_move(pSBElement, drgui_sb_on_mouse_move);
    drgui_set_on_mouse_button_down(pSBElement, drgui_sb_on_mouse_button_down);
    drgui_set_on_mouse_button_up(pSBElement, drgui_sb_on_mouse_button_up);
    drgui_set_on_mouse_wheel(pSBElement, drgui_sb_on_mouse_wheel);
    drgui_set_on_paint(pSBElement, drgui_sb_on_paint);


    return pSBElement;
}

void drgui_delete_scrollbar(drgui_element* pSBElement)
{
    if (pSBElement == NULL) {
        return;
    }

    drgui_delete_element(pSBElement);
}


size_t drgui_sb_get_extra_data_size(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return 0;
    }

    return pSB->extraDataSize;
}

void* drgui_sb_get_extra_data(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return NULL;
    }

    return pSB->pExtraData;
}


drgui_sb_orientation drgui_sb_get_orientation(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return drgui_sb_orientation_none;
    }

    return pSB->orientation;
}


void drgui_sb_set_range(drgui_element* pSBElement, int rangeMin, int rangeMax)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->rangeMin = rangeMin;
    pSB->rangeMax = rangeMax;


    // Make sure the scroll position is still valid.
    drgui_sb_scroll_to(pSBElement, drgui_sb_get_scroll_position(pSBElement));

    // The thumb may have changed, so refresh it.
    drgui_sb_refresh_thumb(pSBElement);
}

void drgui_sb_get_range(drgui_element* pSBElement, int* pRangeMinOut, int* pRangeMaxOut)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
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


void drgui_sb_set_page_size(drgui_element* pSBElement, int pageSize)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->pageSize = pageSize;


    // Make sure the scroll position is still valid.
    drgui_sb_scroll_to(pSBElement, drgui_sb_get_scroll_position(pSBElement));

    // The thumb may have changed, so refresh it.
    drgui_sb_refresh_thumb(pSBElement);
}

int drgui_sb_get_page_size(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return 0;
    }

    return pSB->pageSize;
}


void drgui_sb_set_range_and_page_size(drgui_element* pSBElement, int rangeMin, int rangeMax, int pageSize)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->rangeMin = rangeMin;
    pSB->rangeMax = rangeMax;
    pSB->pageSize = pageSize;


    // Make sure the scroll position is still valid.
    drgui_sb_scroll_to(pSBElement, drgui_sb_get_scroll_position(pSBElement));

    // The thumb may have changed, so refresh it.
    drgui_sb_refresh_thumb(pSBElement);
}


void drgui_sb_set_scroll_position(drgui_element* pSBElement, int position)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    int newScrollPos = drgui_sb_clampi(position, pSB->rangeMin, drgui_sb_maxi(0, pSB->rangeMax - pSB->pageSize + 1));
    if (newScrollPos != pSB->scrollPos)
    {
        pSB->scrollPos = newScrollPos;

        // The position of the thumb has changed, so refresh it.
        drgui_sb_refresh_thumb(pSBElement);
    }
}

int drgui_sb_get_scroll_position(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return 0;
    }

    return pSB->scrollPos;
}


void drgui_sb_scroll(drgui_element* pSBElement, int offset)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    drgui_sb_scroll_to(pSBElement, pSB->scrollPos + offset);
}

void drgui_sb_scroll_to(drgui_element* pSBElement, int newScrollPos)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    int oldScrollPos = pSB->scrollPos;
    drgui_sb_set_scroll_position(pSBElement, newScrollPos);

    if (oldScrollPos != pSB->scrollPos)
    {
        if (pSB->onScroll) {
            pSB->onScroll(pSBElement, pSB->scrollPos);
        }
    }
}


void drgui_sb_enable_thumb_auto_hide(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (pSB->autoHideThumb != true)
    {
        pSB->autoHideThumb = true;

        // The thumb needs to be refreshed in order to show the correct state.
        drgui_sb_refresh_thumb(pSBElement);
    }
}

void drgui_sb_disable_thumb_auto_hide(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (pSB->autoHideThumb != false)
    {
        pSB->autoHideThumb = false;

        // The thumb needs to be refreshed in order to show the correct state.
        drgui_sb_refresh_thumb(pSBElement);
    }
}

bool drgui_sb_is_thumb_auto_hide_enabled(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return false;
    }

    return pSB->autoHideThumb;
}

bool drgui_sb_is_thumb_visible(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return false;
    }

    // Always visible if auto-hiding is disabled.
    if (!pSB->autoHideThumb) {
        return true;
    }

    return pSB->pageSize < (pSB->rangeMax - pSB->rangeMin + 1) && pSB->pageSize > 0;
}


void drgui_sb_set_mouse_wheel_scele(drgui_element* pSBElement, int scale)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->mouseWheelScale = scale;
}

int drgui_sb_get_mouse_wheel_scale(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return 1;
    }

    return pSB->mouseWheelScale;
}


void drgui_sb_set_track_color(drgui_element* pSBElement, drgui_color color)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->trackColor = color;
}

void drgui_sb_set_default_thumb_color(drgui_element* pSBElement, drgui_color color)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->thumbColor = color;
}

void drgui_sb_set_hovered_thumb_color(drgui_element* pSBElement, drgui_color color)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->thumbColorHovered = color;
}

void drgui_sb_set_pressed_thumb_color(drgui_element* pSBElement, drgui_color color)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->thumbColorPressed = color;
}


void drgui_sb_set_on_scroll(drgui_element* pSBElement, drgui_sb_on_scroll_proc onScroll)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    pSB->onScroll = onScroll;
}

drgui_sb_on_scroll_proc drgui_sb_get_on_scroll(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return NULL;
    }

    return pSB->onScroll;
}


drgui_rect drgui_sb_get_thumb_rect(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return drgui_make_rect(0, 0, 0, 0);
    }

    drgui_rect rect = {0, 0, 0, 0};
    rect.left = pSB->thumbPadding;
    rect.top  = pSB->thumbPadding;

    if (pSB->orientation == drgui_sb_orientation_vertical)
    {
        // Vertical.
        rect.left   = pSB->thumbPadding;
        rect.right  = drgui_get_width(pSBElement) - pSB->thumbPadding;
        rect.top    = pSB->thumbPadding + pSB->thumbPos;
        rect.bottom = rect.top + pSB->thumbSize;
    }
    else
    {
        // Horizontal.
        rect.left   = pSB->thumbPadding + pSB->thumbPos;
        rect.right  = rect.left + pSB->thumbSize;
        rect.top    = pSB->thumbPadding;
        rect.bottom = drgui_get_height(pSBElement) - pSB->thumbPadding;
    }

    return rect;
}


void drgui_sb_on_size(drgui_element* pSBElement, float newWidth, float newHeight)
{
    (void)newWidth;
    (void)newHeight;

    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    drgui_sb_refresh_thumb(pSBElement);
}

void drgui_sb_on_mouse_leave(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
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
        drgui_dirty(pSBElement, drgui_sb_get_thumb_rect(pSBElement));
    }
}

void drgui_sb_on_mouse_move(drgui_element* pSBElement, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)stateFlags;

    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (pSB->thumbPressed)
    {
        // The thumb is pressed. Drag it.
        float thumbRelativeMousePosX = (float)relativeMousePosX;
        float thumbRelativeMousePosY = (float)relativeMousePosY;
        drgui_sb_make_relative_to_thumb(pSBElement, &thumbRelativeMousePosX, &thumbRelativeMousePosY);

        float dragOffsetX = thumbRelativeMousePosX - pSB->thumbClickPosX;
        float dragOffsetY = thumbRelativeMousePosY - pSB->thumbClickPosY;

        float destTrackPos = pSB->thumbPos;
        if (pSB->orientation == drgui_sb_orientation_vertical) {
            destTrackPos += dragOffsetY;
        } else {
            destTrackPos += dragOffsetX;
        }

        int destScrollPos = drgui_sb_calculate_scroll_pos_from_thumb_pos(pSBElement, destTrackPos);
        if (destScrollPos != pSB->scrollPos)
        {
            drgui_sb_scroll_to(pSBElement, destScrollPos);
        }
    }
    else
    {
        // The thumb is not pressed. We just need to check if the hovered state has change and redraw if required.
        if (drgui_sb_is_thumb_visible(pSBElement))
        {
            bool wasThumbHovered = pSB->thumbHovered;

            drgui_rect thumbRect = drgui_sb_get_thumb_rect(pSBElement);
            pSB->thumbHovered = drgui_rect_contains_point(thumbRect, (float)relativeMousePosX, (float)relativeMousePosY);

            if (wasThumbHovered != pSB->thumbHovered) {
                drgui_dirty(pSBElement, thumbRect);
            }
        }
    }
}

void drgui_sb_on_mouse_button_down(drgui_element* pSBElement, int button, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)stateFlags;

    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (button == DRGUI_MOUSE_BUTTON_LEFT)
    {
        if (drgui_sb_is_thumb_visible(pSBElement))
        {
            drgui_rect thumbRect = drgui_sb_get_thumb_rect(pSBElement);
            if (drgui_rect_contains_point(thumbRect, (float)relativeMousePosX, (float)relativeMousePosY))
            {
                if (!pSB->thumbPressed)
                {
                    drgui_capture_mouse(pSBElement);
                    pSB->thumbPressed = true;

                    pSB->thumbClickPosX = (float)relativeMousePosX;
                    pSB->thumbClickPosY = (float)relativeMousePosY;
                    drgui_sb_make_relative_to_thumb(pSBElement, &pSB->thumbClickPosX, &pSB->thumbClickPosY);

                    drgui_dirty(pSBElement, drgui_sb_get_thumb_rect(pSBElement));
                }
            }
            else
            {
                // If the click position is above the thumb we want to scroll up by a page. If it's below the thumb, we scroll down by a page.
                if (relativeMousePosY < thumbRect.top) {
                    drgui_sb_scroll(pSBElement, -drgui_sb_get_page_size(pSBElement));
                } else if (relativeMousePosY >= thumbRect.bottom) {
                    drgui_sb_scroll(pSBElement,  drgui_sb_get_page_size(pSBElement));
                }
            }
        }
    }
}

void drgui_sb_on_mouse_button_up(drgui_element* pSBElement, int button, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    if (button == DRGUI_MOUSE_BUTTON_LEFT)
    {
        if (pSB->thumbPressed && drgui_get_element_with_mouse_capture(pSBElement->pContext) == pSBElement)
        {
            drgui_release_mouse(pSBElement->pContext);
            pSB->thumbPressed = false;

            drgui_dirty(pSBElement, drgui_sb_get_thumb_rect(pSBElement));
        }
    }
}

void drgui_sb_on_mouse_wheel(drgui_element* pSBElement, int delta, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    drgui_sb_scroll(pSBElement, -delta * drgui_sb_get_mouse_wheel_scale(pSBElement));
}

void drgui_sb_on_paint(drgui_element* pSBElement, drgui_rect relativeClippingRect, void* pPaintData)
{
    (void)relativeClippingRect;

    const drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    drgui_rect thumbRect = drgui_sb_get_thumb_rect(pSBElement);

    if (drgui_sb_is_thumb_visible(pSBElement))
    {
        // The thumb is visible.

        // Track. We draw this in 4 seperate pieces so we can avoid overdraw with the thumb.
        drgui_draw_rect(pSBElement, drgui_make_rect(0, 0, drgui_get_width(pSBElement), thumbRect.top), pSB->trackColor, pPaintData);  // Top
        drgui_draw_rect(pSBElement, drgui_make_rect(0, thumbRect.bottom, drgui_get_width(pSBElement), drgui_get_height(pSBElement)), pSB->trackColor, pPaintData);  // Bottom
        drgui_draw_rect(pSBElement, drgui_make_rect(0, thumbRect.top, thumbRect.left, thumbRect.bottom), pSB->trackColor, pPaintData);  // Left
        drgui_draw_rect(pSBElement, drgui_make_rect(thumbRect.right, thumbRect.top, drgui_get_width(pSBElement), thumbRect.bottom), pSB->trackColor, pPaintData); // Right

        // Thumb.
        drgui_color thumbColor;
        if (pSB->thumbPressed) {
            thumbColor = pSB->thumbColorPressed;
        } else if (pSB->thumbHovered) {
            thumbColor = pSB->thumbColorHovered;
        } else {
            thumbColor = pSB->thumbColor;
        }

        drgui_draw_rect(pSBElement, thumbRect, thumbColor, pPaintData);
    }
    else
    {
        // The thumb is not visible - just draw the track as one quad.
        drgui_draw_rect(pSBElement, drgui_get_local_rect(pSBElement), pSB->trackColor, pPaintData);
    }
}



DRGUI_PRIVATE void drgui_sb_refresh_thumb(drgui_element* pSBElement)
{
    drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    drgui_rect oldThumbRect = drgui_sb_get_thumb_rect(pSBElement);

    pSB->thumbSize = drgui_sb_calculate_thumb_size(pSBElement);
    pSB->thumbPos  = drgui_sb_calculate_thumb_position(pSBElement);

    drgui_rect newThumbRect = drgui_sb_get_thumb_rect(pSBElement);
    if (!drgui_rect_equal(oldThumbRect, newThumbRect))
    {
        drgui_dirty(pSBElement, drgui_rect_union(oldThumbRect, newThumbRect));
    }
}

DRGUI_PRIVATE float drgui_sb_calculate_thumb_size(drgui_element* pSBElement)
{
    const drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    float trackSize = drgui_sb_get_track_size(pSBElement);
    float range = (float)(pSB->rangeMax - pSB->rangeMin + 1);

    float thumbSize = DRGUI_MIN_SCROLLBAR_THUMB_SIZE;
    if (range > 0)
    {
        thumbSize = roundf((trackSize / range) * pSB->pageSize);
        thumbSize = drgui_sb_clampf(thumbSize, DRGUI_MIN_SCROLLBAR_THUMB_SIZE, trackSize);
    }

    return thumbSize;
}

DRGUI_PRIVATE float drgui_sb_calculate_thumb_position(drgui_element* pSBElement)
{
    const drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    float trackSize = drgui_sb_get_track_size(pSBElement);
    float thumbSize = drgui_sb_calculate_thumb_size(pSBElement);
    float range = (float)(pSB->rangeMax - pSB->rangeMin + 1);

    float thumbPos = 0;
    if (range > pSB->pageSize)
    {
        thumbPos = roundf((trackSize / range) * pSB->scrollPos);
        thumbPos = drgui_sb_clampf(thumbPos, 0, trackSize - thumbSize);
    }

    return thumbPos;
}

DRGUI_PRIVATE float drgui_sb_get_track_size(drgui_element* pSBElement)
{
    const drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    if (pSB->orientation == drgui_sb_orientation_vertical) {
        return drgui_get_height(pSBElement) - (pSB->thumbPadding*2);
    } else {
        return drgui_get_width(pSBElement) - (pSB->thumbPadding*2);
    }
}

DRGUI_PRIVATE void drgui_sb_make_relative_to_thumb(drgui_element* pSBElement, float* pPosX, float* pPosY)
{
    drgui_rect thumbRect = drgui_sb_get_thumb_rect(pSBElement);

    if (pPosX != NULL) {
        *pPosX -= thumbRect.left;
    }

    if (pPosY != NULL) {
        *pPosY -= thumbRect.top;
    }
}

DRGUI_PRIVATE int drgui_sb_calculate_scroll_pos_from_thumb_pos(drgui_element* pSBElement, float thumbPos)
{
    const drgui_scrollbar* pSB = drgui_get_extra_data(pSBElement);
    assert(pSB != NULL);

    float trackSize = drgui_sb_get_track_size(pSBElement);
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
