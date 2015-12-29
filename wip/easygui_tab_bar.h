// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - This control is only the tab bar itself - this does not handle tab pages and content switching and whatnot.
//

#ifndef easygui_tab_bar_h
#define easygui_tab_bar_h

#include <easy_gui/easy_gui.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EASYGUI_MAX_TAB_TEXT_LENGTH   256

typedef enum
{
    tb_orientation_none,
    tb_orientation_vertical,
    tb_orientation_horizontal
} tb_orientation;

typedef struct easygui_tab easygui_tab;

typedef void (* tb_on_measure_tab_proc) (easygui_element* pTBElement, easygui_tab* pTab, float* pWidthOut, float* pHeightOut);
typedef void (* tb_on_paint_tab_proc)   (easygui_element* pTBElement, easygui_tab* pTab, easygui_rect relativeClippingRect, easygui_color backgroundColor, float offsetX, float offsetY, float width, float height, void* pPaintData);


///////////////////////////////////////////////////////////////////////////////
//
// Tab Bar
//
///////////////////////////////////////////////////////////////////////////////

/// Creates a new tab bar control.
easygui_element* eg_create_tab_bar(easygui_context* pContext, easygui_element* pParent, tb_orientation orientation, size_t extraDataSize, const void* pExtraData);

/// Deletes the given tab bar control.
void eg_delete_tab_bar(easygui_element* pTBElement);


/// Retrieves the size of the extra data associated with the scrollbar.
size_t tb_get_extra_data_size(easygui_element* pTBElement);

/// Retrieves a pointer to the extra data associated with the scrollbar.
void* tb_get_extra_data(easygui_element* pTBElement);

/// Retrieves the orientation of the given scrollbar.
tb_orientation tb_get_orientation(easygui_element* pTBElement);


/// Sets the default font to use for tabs.
void tb_set_font(easygui_element* pTBElement, easygui_font* pFont);

/// Retrieves the default font to use for tabs.
easygui_font* tb_get_font(easygui_element* pTBElement);


/// Sets the function to call when a tab needs to be measured.
void tb_set_on_measure_tab(easygui_element* pTBElement, tb_on_measure_tab_proc proc);

/// Sets the function to call when a tab needs to be painted.
void tb_set_on_paint_tab(easygui_element* pTBElement, tb_on_paint_tab_proc proc);


/// Called when the paint event needs to be processed for the given tab control.
void tb_on_paint(easygui_element* pTBElement, easygui_rect relativeClippingRect, void* pPaintData);




///////////////////////////////////////////////////////////////////////////////
//
// Tab
//
///////////////////////////////////////////////////////////////////////////////

/// Creates and appends a tab
easygui_tab* tb_create_and_append_tab(easygui_element* pTBElement, size_t extraDataSize, const void* pExtraData);

/// Creates and prepends a tab.
easygui_tab* tb_create_and_prepend_tab(easygui_element* pTBElement, size_t extraDataSize, const void* pExtraData);

/// Recursively deletes a tree view item.
void tab_delete(easygui_tab* pTab);

/// Retrieves the tree-view GUI element that owns the given item.
easygui_element* tab_get_tab_bar_element(easygui_tab* pTab);

/// Retrieves the size of the extra data associated with the given tree-view item.
size_t tab_get_extra_data_size(easygui_tab* pTab);

/// Retrieves a pointer to the extra data associated with the given tree-view item.
void* tab_get_extra_data(easygui_tab* pTab);



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
