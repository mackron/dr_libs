// Public domain. See "unlicense" statement at the end of this file.

#ifndef drgui_scrollbar_h
#define drgui_scrollbar_h

#include <dr_libs/dr_gui.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    drgui_sb_orientation_none,
    drgui_sb_orientation_vertical,
    drgui_sb_orientation_horizontal

} drgui_sb_orientation;

typedef void (* drgui_sb_on_scroll_proc)(drgui_element* pSBElement, int scrollPos);


/// Creates a scrollbar element.
drgui_element* drgui_create_scrollbar(drgui_context* pContext, drgui_element* pParent, drgui_sb_orientation orientation, size_t extraDataSize, const void* pExtraData);

/// Deletes the given scrollbar element.
void drgui_delete_scrollbar(drgui_element* pSBElement);


/// Retrieves the size of the extra data associated with the scrollbar.
size_t drgui_sb_get_extra_data_size(drgui_element* pSBElement);

/// Retrieves a pointer to the extra data associated with the scrollbar.
void* drgui_sb_get_extra_data(drgui_element* pSBElement);


/// Retrieves the orientation of the given scrollbar.
drgui_sb_orientation drgui_sb_get_orientation(drgui_element* pSBElement);


/// Sets the given scrollbar's range.
void drgui_sb_set_range(drgui_element* pSBElement, int rangeMin, int rangeMax);

/// Retrieves the given scrollbar's range.
void drgui_sb_get_range(drgui_element* pSBElement, int* pRangeMinOut, int* pRangeMaxOut);


/// Sets the page size of the given scrollbar's page.
void drgui_sb_set_page_size(drgui_element* pSBElement, int pageSize);

/// Retrieves the page size of the given scrollbar's page.
int drgui_sb_get_page_size(drgui_element* pSBElement);


/// Sets the range and page size.
///
/// @remarks
///     Use this when both the range and page size need to be updated at the same time.
void drgui_sb_set_range_and_page_size(drgui_element* pSBElement, int rangeMin, int rangeMax, int pageSize);


/// Explicitly sets the scroll position.
///
/// @remarks
///     This will move the thumb, but not post the on_scroll event.
///     @par
///     The scroll position will be clamped to the current range, minus the page size.
void drgui_sb_set_scroll_position(drgui_element* pSBElement, int position);

/// Retrieves the scroll position.
int drgui_sb_get_scroll_position(drgui_element* pSBElement);


/// Scrolls by the given amount.
///
/// @remarks
///     If the resulting scroll position differs from the old one, the on on_scroll event will be posted.
void drgui_sb_scroll(drgui_element* pSBElement, int offset);

/// Scrolls to the given position.
///
/// @remarks
///     This differs from drgui_sb_set_scroll_position in that it will post the on_scroll event.
///     @par
///     Note that the actual maximum scrollable position is equal to the maximum range value minus the page size.
void drgui_sb_scroll_to(drgui_element* pSBElement, int newScrollPos);


/// Enables auto-hiding of the thumb.
void drgui_sb_enable_thumb_auto_hide(drgui_element* pSBElement);

/// Disables auto-hiding of the thumb.
void drgui_sb_disable_thumb_auto_hide(drgui_element* pSBElement);

/// Determines whether or not thumb auto-hiding is enabled.
bool drgui_sb_is_thumb_auto_hide_enabled(drgui_element* pSBElement);

/// Determines whether or not the thumb is visible.
///
/// @remarks
///     This is determined by whether or not the thumb is set to auto-hide and the current range and page size.
bool drgui_sb_is_thumb_visible(drgui_element* pSBElement);


/// Sets the mouse wheel scale.
///
/// @remarks
///     Set this to a negative value to reverse the direction.
void drgui_sb_set_mouse_wheel_scele(drgui_element* pSBElement, int scale);

/// Retrieves the mouse wheel scale.
int drgui_sb_get_mouse_wheel_scale(drgui_element* pSBElement);


/// Sets the color of the track.
void drgui_sb_set_track_color(drgui_element* pSBElement, drgui_color color);

/// Sets the default color of the thumb.
void drgui_sb_set_default_thumb_color(drgui_element* pSBElement, drgui_color color);

/// Sets the hovered color of the thumb.
void drgui_sb_set_hovered_thumb_color(drgui_element* pSBElement, drgui_color color);

/// Sets the pressed color of the thumb.
void drgui_sb_set_pressed_thumb_color(drgui_element* pSBElement, drgui_color color);


/// Sets the function to call when the given scrollbar is scrolled.
void drgui_sb_set_on_scroll(drgui_element* pSBElement, drgui_sb_on_scroll_proc onScroll);

/// Retrieves the function call when the given scrollbar is scrolled.
drgui_sb_on_scroll_proc drgui_sb_get_on_scroll(drgui_element* pSBElement);


/// Calculates the relative rectangle of the given scrollbar's thumb.
drgui_rect drgui_sb_get_thumb_rect(drgui_element* pSBElement);


/// Called when the size event needs to be processed for the given scrollbar.
void drgui_sb_on_size(drgui_element* pSBElement, float newWidth, float newHeight);

/// Called when the mouse leave event needs to be processed for the given scrollbar.
void drgui_sb_on_mouse_leave(drgui_element* pSBElement);

/// Called when the mouse move event needs to be processed for the given scrollbar.
void drgui_sb_on_mouse_move(drgui_element* pSBElement, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when the mouse button down event needs to be processed for the given scrollbar.
void drgui_sb_on_mouse_button_down(drgui_element* pSBElement, int button, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when the mouse button up event needs to be processed for the given scrollbar.
void drgui_sb_on_mouse_button_up(drgui_element* pSBElement, int button, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when the mouse wheel event needs to be processed for the given scrollbar.
void drgui_sb_on_mouse_wheel(drgui_element* pSBElement, int delta, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when the paint event needs to be processed.
void drgui_sb_on_paint(drgui_element* pSBElement, drgui_rect relativeClippingRect, void* pPaintData);


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