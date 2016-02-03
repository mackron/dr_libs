// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - This control is only the tab bar itself - this does not handle tab pages and content switching and whatnot.
//

#ifndef drgui_tab_bar_h
#define drgui_tab_bar_h

#include <dr_libs/dr_gui.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRGUI_MAX_TAB_TEXT_LENGTH   256

typedef enum
{
    drgui_tabbar_orientation_top,
    drgui_tabbar_orientation_bottom,
    drgui_tabbar_orientation_left,
    drgui_tabbar_orientation_right
} drgui_tabbar_orientation;

typedef struct drgui_tab drgui_tab;

typedef void (* drgui_tabbar_on_measure_tab_proc)    (drgui_element* pTBElement, drgui_tab* pTab, float* pWidthOut, float* pHeightOut);
typedef void (* drgui_tabbar_on_paint_tab_proc)      (drgui_element* pTBElement, drgui_tab* pTab, drgui_rect relativeClippingRect, float offsetX, float offsetY, float width, float height, void* pPaintData);
typedef void (* drgui_tabbar_on_tab_activated_proc)  (drgui_element* pTBElement, drgui_tab* pTab);
typedef void (* drgui_tabbar_on_tab_deactivated_proc)(drgui_element* pTBElement, drgui_tab* pTab);
typedef void (* drgui_tabbar_on_tab_close_proc)      (drgui_element* pTBElement, drgui_tab* pTab);


///////////////////////////////////////////////////////////////////////////////
//
// Tab Bar
//
///////////////////////////////////////////////////////////////////////////////

/// Creates a new tab bar control.
drgui_element* drgui_create_tab_bar(drgui_context* pContext, drgui_element* pParent, drgui_tabbar_orientation orientation, size_t extraDataSize, const void* pExtraData);

/// Deletes the given tab bar control.
void drgui_delete_tab_bar(drgui_element* pTBElement);


/// Retrieves the size of the extra data associated with the scrollbar.
size_t drgui_tabbar_get_extra_data_size(drgui_element* pTBElement);

/// Retrieves a pointer to the extra data associated with the scrollbar.
void* drgui_tabbar_get_extra_data(drgui_element* pTBElement);

/// Retrieves the orientation of the given scrollbar.
drgui_tabbar_orientation drgui_tabbar_get_orientation(drgui_element* pTBElement);


/// Sets the default font to use for tabs.
void drgui_tabbar_set_font(drgui_element* pTBElement, drgui_font* pFont);

/// Retrieves the default font to use for tabs.
drgui_font* drgui_tabbar_get_font(drgui_element* pTBElement);

/// Sets the image to use for close buttons.
void drgui_tabbar_set_close_button_image(drgui_element* pTBElement, drgui_image* pImage);

/// Retrieves the image being used for the close buttons.
drgui_image* drgui_tabbar_get_close_button_image(drgui_element* pTBElement);


/// Sets the function to call when a tab needs to be measured.
void drgui_tabbar_set_on_measure_tab(drgui_element* pTBElement, drgui_tabbar_on_measure_tab_proc proc);

/// Sets the function to call when a tab needs to be painted.
void drgui_tabbar_set_on_paint_tab(drgui_element* pTBElement, drgui_tabbar_on_paint_tab_proc proc);

/// Sets the function to call when a tab is activated.
void drgui_tabbar_set_on_tab_activated(drgui_element* pTBElement, drgui_tabbar_on_tab_activated_proc proc);

/// Sets the function to call when a tab is deactivated.
void drgui_tabbar_set_on_tab_deactivated(drgui_element* pTBElement, drgui_tabbar_on_tab_deactivated_proc proc);

/// Sets the function to call when a tab is closed with the close button.
void drgui_tabbar_set_on_tab_closed(drgui_element* pTBElement, drgui_tabbar_on_tab_close_proc proc);


/// Measures the given tab.
void drgui_tabbar_measure_tab(drgui_element* pTBElement, drgui_tab* pTab, float* pWidthOut, float* pHeightOut);

/// Paints the given tab.
void drgui_tabbar_paint_tab(drgui_element* pTBElement, drgui_tab* pTab, drgui_rect relativeClippingRect, float offsetX, float offsetY, float width, float height, void* pPaintData);


/// Sets the width or height of the tab bar to that of it's tabs based on it's orientation.
///
/// @remarks
///     If the orientation is set to top or bottom, the height will be resized and the width will be left alone. If the orientation
///     is left or right, the width will be resized and the height will be left alone.
///     @par
///     If there is no tab measuring callback set, this will do nothing.
void drgui_tabbar_resize_by_tabs(drgui_element* pTBElement);

/// Enables auto-resizing based on tabs.
///
/// @remarks
///     This follows the same resizing rules as per drgui_tabbar_resize_by_tabs().
///
/// @see
///     drgui_tabbar_resize_by_tabs()
void drgui_tabbar_enable_auto_size(drgui_element* pTBElement);

/// Disables auto-resizing based on tabs.
void drgui_tabbar_disable_auto_size(drgui_element* pTBElement);

/// Determines whether or not auto-sizing is enabled.
bool drgui_tabbar_is_auto_size_enabled(drgui_element* pTBElement);


/// Activates the given tab.
void drgui_tabbar_activate_tab(drgui_element* pTBElement, drgui_tab* pTab);

/// Retrieves a pointer to the currently active tab.
drgui_tab* drgui_tabbar_get_active_tab(drgui_element* pTBElement);


/// Determines whether or not the given tab is in view.
bool drgui_tabbar_is_tab_in_view(drgui_element* pTBElement, drgui_tab* pTab);


/// Shows the close buttons on each tab.
void drgui_tabbar_show_close_buttons(drgui_element* pTBElement);

/// Hides the close buttons on each tab.
void drgui_tabbar_hide_close_buttons(drgui_element* pTBElement);

/// Enables the on_close event on middle click.
void drgui_tabbar_enable_close_on_middle_click(drgui_element* pTBElement);

/// Disables the on_close event on middle click.
void drgui_tabbar_disable_close_on_middle_click(drgui_element* pTBElement);

/// Determines whether or not close-on-middle-click is enabled.
bool drgui_tabbar_is_close_on_middle_click_enabled(drgui_element* pTBElement);


/// Called when the mouse leave event needs to be processed for the given tab bar control.
void drgui_tabbar_on_mouse_leave(drgui_element* pTBElement);

/// Called when the mouse move event needs to be processed for the given tab bar control.
void drgui_tabbar_on_mouse_move(drgui_element* pTBElement, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when the mouse button down event needs to be processed for the given tab bar control.
void drgui_tabbar_on_mouse_button_down(drgui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when the mouse button up event needs to be processed for the given tab bar control.
void drgui_tabbar_on_mouse_button_up(drgui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when the paint event needs to be processed for the given tab control.
void drgui_tabbar_on_paint(drgui_element* pTBElement, drgui_rect relativeClippingRect, void* pPaintData);




///////////////////////////////////////////////////////////////////////////////
//
// Tab
//
///////////////////////////////////////////////////////////////////////////////

/// Creates and appends a tab
drgui_tab* drgui_tabbar_create_and_append_tab(drgui_element* pTBElement, const char* text, size_t extraDataSize, const void* pExtraData);

/// Creates and prepends a tab.
drgui_tab* drgui_tabbar_create_and_prepend_tab(drgui_element* pTBElement, const char* text, size_t extraDataSize, const void* pExtraData);

/// Recursively deletes a tree view item.
void tab_delete(drgui_tab* pTab);

/// Retrieves the tree-view GUI element that owns the given item.
drgui_element* tab_get_tab_bar_element(drgui_tab* pTab);

/// Retrieves the size of the extra data associated with the given tree-view item.
size_t tab_get_extra_data_size(drgui_tab* pTab);

/// Retrieves a pointer to the extra data associated with the given tree-view item.
void* tab_get_extra_data(drgui_tab* pTab);


/// Sets the text of the given tab bar item.
void tab_set_text(drgui_tab* pTab, const char* text);

/// Retrieves the text of the given tab bar item.
const char* tab_get_text(drgui_tab* pTab);


/// Retrieves a pointer to the next tab in the tab bar.
drgui_tab* tab_get_next_tab(drgui_tab* pTab);

/// Retrieves a pointer to the previous tab in the tab bar.
drgui_tab* tab_get_prev_tab(drgui_tab* pTab);


/// Moves the given tab to the front of the tab bar that owns it.
void tab_move_to_front(drgui_tab* pTab);

/// Determines whether or not the given tab is in view.
bool tab_is_in_view(drgui_tab* pTab);


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
