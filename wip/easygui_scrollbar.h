// Public domain. See "unlicense" statement at the end of this file.

#ifndef easygui_scrollbar_h
#define easygui_scrollbar_h

#include <easy_gui/easy_gui.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    sb_orientation_none,
    sb_orientation_vertical,
    sb_orientation_horizontal

} sb_orientation;

typedef void (* sb_on_scroll_proc)(easygui_element* pScrollbar, int scrollPos);


/// Creates a scrollbar element.
easygui_element* eg_create_scrollbar(easygui_context* pContext, easygui_element* pParent, sb_orientation orientation, size_t extraDataSize, const void* pExtraData);

/// Deletes the given scrollbar element.
void eg_delete_scrollbar(easygui_element* pScrollbar);


/// Retrieves the size of the extra data associated with the scrollbar.
size_t sb_get_extra_data_size(easygui_element* pScrollbar);

/// Retrieves a pointer to the extra data associated with the scrollbar.
void* sb_get_extra_data(easygui_element* pScrollbar);


/// Retrieves the orientation of the given scrollbar.
sb_orientation sb_get_orientation(easygui_element* pScrollbar);


/// Sets the given scrollbar's range.
void sb_set_range(easygui_element* pScrollbar, int rangeMin, int rangeMax);

/// Retrieves the given scrollbar's range.
void sb_get_range(easygui_element* pScrollbar, int* pRangeMinOut, int* pRangeMaxOut);


/// Sets the page size of the given scrollbar's page.
void sb_set_page_size(easygui_element* pScrollbar, int pageSize);

/// Retrieves the page size of the given scrollbar's page.
int sb_get_page_size(easygui_element* pScrollbar);


/// Explicitly sets the scroll position.
///
/// @remarks
///     This will move the thumb, but not post the on_scroll event.
///     @par
///     The scroll position will be clamped to the current range, minus the page size.
void sb_set_scroll_position(easygui_element* pScrollbar, int position);

/// Retrieves the scroll position.
int sb_get_scroll_position(easygui_element* pScrollbar);


/// Scrolls by the given amount.
///
/// @remarks
///     If the resulting scroll position differs from the old one, the on on_scroll event will be posted.
void sb_scroll(easygui_element* pScrollbar, int offset);

/// Scrolls to the given position.
///
/// @remarks
///     This differs from sb_set_scroll_position in that it will post the on_scroll event.
///     @par
///     Note that the actual maximum scrollable position is equal to the maximum range value minus the page size.
void sb_scroll_to(easygui_element* pScrollbar, int newScrollPos);


/// Enables auto-hiding of the thumb.
void sb_enable_thumb_auto_hide(easygui_element* pScrollbar);

/// Disables auto-hiding of the thumb.
void sb_disable_thumb_auto_hide(easygui_element* pScrollbar);

/// Determines whether or not thumb auto-hiding is enabled.
bool sb_is_thumb_auto_hide_enabled(easygui_element* pScrollbar);

/// Determines whether or not the thumb is visible.
///
/// @remarks
///     This is determined by whether or not the thumb is set to auto-hide and the current range and page size.
bool sb_is_thumb_visible(easygui_element* pScrollbar);


/// Sets the mouse wheel scale.
///
/// @remarks
///     Set this to a negative value to reverse the direction.
void sb_set_mouse_wheel_scele(easygui_element* pScrollbar, int scale);

/// Retrieves the mouse wheel scale.
int sb_get_mouse_wheel_scale(easygui_element* pScrollbar);


/// Sets the color of the track.
void sb_set_track_color(easygui_element* pScrollbar, easygui_color color);

/// Sets the default color of the thumb.
void sb_set_default_thumb_color(easygui_element* pScrollbar, easygui_color color);

/// Sets the hovered color of the thumb.
void sb_set_hovered_thumb_color(easygui_element* pScrollbar, easygui_color color);

/// Sets the pressed color of the thumb.
void sb_set_pressed_thumb_color(easygui_element* pScrollbar, easygui_color color);


/// Sets the function to call when the given scrollbar is scrolled.
void sb_set_on_scroll(easygui_element* pScrollbar, sb_on_scroll_proc onScroll);

/// Retrieves the function call when the given scrollbar is scrolled.
sb_on_scroll_proc sb_get_on_scroll(easygui_element* pScrollbar);


/// Calculates the relative rectangle of the given scrollbar's thumb.
easygui_rect sb_get_thumb_rect(easygui_element* pScrollbar);


/// Called when the size event needs to be processed for the given scrollbar.
void sb_on_size(easygui_element* pScrollbar, float newWidth, float newHeight);

/// Called when the mouse leave event needs to be processed for the given scrollbar.
void sb_on_mouse_leave(easygui_element* pScrollbar);

/// Called when the mouse move event needs to be processed for the given scrollbar.
void sb_on_mouse_move(easygui_element* pScrollbar, int relativeMousePosX, int relativeMousePosY);

/// Called when the mouse button down event needs to be processed for the given scrollbar.
void sb_on_mouse_button_down(easygui_element* pScrollbar, int button, int relativeMousePosX, int relativeMousePosY);

/// Called when the mouse button up event needs to be processed for the given scrollbar.
void sb_on_mouse_button_up(easygui_element* pScrollbar, int button, int relativeMousePosX, int relativeMousePosY);

/// Called when the mouse wheel event needs to be processed for the given scrollbar.
void sb_on_mouse_wheel(easygui_element* pScrollbar, int delta, int relativeMousePosX, int relativeMousePosY);

/// Called when the paint event needs to be processed.
void sb_on_paint(easygui_element* pScrollbar, easygui_rect relativeClippingRect, void* pPaintData);


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