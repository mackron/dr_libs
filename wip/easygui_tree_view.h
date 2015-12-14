// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// Tree-View Controls
// - A tree-view control is a complex control with a hierarchy of items. They are typically used for file explorers.
// - 

#ifndef easygui_tree_view_h
#define easygui_tree_view_h

#include <easy_gui/easy_gui.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EG_MAX_TREE_VIEW_ITEM_TEXT_LENGTH   256

typedef struct eg_tree_view_item eg_tree_view_item;

typedef void (* tvi_on_mouse_move_proc)  (eg_tree_view_item* pItem, int relativeMousePosX, int relativeMousePosY, bool* pIsOverArrow);
typedef void (* tvi_on_mouse_leave_proc) (eg_tree_view_item* pItem);
typedef void (* tvi_on_paint_proc)       (easygui_element* pTVElement, eg_tree_view_item* pItem, easygui_rect relativeClippingRect, easygui_color backgroundColor, float offsetX, float offsetY, float width, float height, void* pPaintData);
typedef void (* tvi_measure_proc)        (eg_tree_view_item* pItem, float* pWidthOut, float* pHeightOut);
typedef void (* tvi_on_picked_proc)      (eg_tree_view_item* pItem);

///////////////////////////////////////////////////////////////////////////////
//
// Tree-View
//
///////////////////////////////////////////////////////////////////////////////

/// Creates a tree-view control.
easygui_element* eg_create_tree_view(easygui_context* pContext, easygui_element* pParent, size_t extraDataSize, const void* pExtraData);

/// Deletes the given tree-view control.
void eg_delete_tree_view(easygui_element* pTVElement);


/// Retrieves the size of the extra data associated with the given tree-view control.
size_t tv_get_extra_data_size(easygui_element* pTVElement);

/// Retrieves a pointer to the buffer containing the given tree-view's extra data.
void* tv_get_extra_data(easygui_element* pTVElement);

/// Retrieves a pointer to the root element of the given tree view control.
eg_tree_view_item* tv_get_root_item(easygui_element* pTVElement);

/// Retrieves a pointer to the vertical scrollbar.
easygui_element* tv_get_vertical_scrollbar(easygui_element* pTVElement);

/// Retrieves a pointer to the horizontal scrollbar.
easygui_element* tv_get_horizontal_scrollbar(easygui_element* pTVElement);


/// Sets the default background color.
void tv_set_default_background_color(easygui_element* pTVElement, easygui_color color);

/// Retrieves the default background color.
easygui_color tv_get_default_background_color(easygui_element* pTVElement);

/// Sets the default background color of hovered items.
void tv_set_hovered_background_color(easygui_element* pTVElement, easygui_color color);

/// Retrieves the default background color of hovered items.
easygui_color tv_get_hovered_background_color(easygui_element* pTVElement);

/// Sets the default background color of selected items.
void tv_set_selected_background_color(easygui_element* pTVElement, easygui_color color);

/// Retrieves the default background color of selected items.
easygui_color tv_get_selected_background_color(easygui_element* pTVElement);

/// Sets the amount of indentation to apply to each child item in the given tree-view.
void tv_set_child_offset_x(easygui_element* pTVElement, float childOffsetX);

/// Retrieves the amount of indentation to apply to each child item in the given tree-view.
float tv_get_child_offset_x(easygui_element* pTVElement);


/// Measures the given item.
bool tv_measure_item(easygui_element* pTVElement, eg_tree_view_item* pItem, float* pWidthOut, float* pHeightOut);

/// Deselects every tree-view item.
void tv_deselect_all_items(easygui_element* pTVElement);


/// Sets the function to call when the mouse is moved while over a tree-view item.
void tv_set_on_item_mouse_move(easygui_element* pTVElement, tvi_on_mouse_move_proc proc);

/// Sets the function call when the mouse leaves a tree-view item.
void tv_set_on_item_mouse_leave(easygui_element* pTVElement, tvi_on_mouse_leave_proc proc);

/// Sets the function to call when a tree-view item needs to be drawn.
void tv_set_on_item_paint(easygui_element* pTVElement, tvi_on_paint_proc proc);

/// Sets the function to call when a tree-view item needs to be measured.
void tv_set_on_item_measure(easygui_element* pTVElement, tvi_measure_proc proc);

/// Sets the function to call when a tree-view item is picked.
///
/// @remarks
///     An item is "picked" when it is a leaf item (has no children) and is double-clicked.
void tv_set_on_item_picked(easygui_element* pTVElement, tvi_on_picked_proc proc);


/// Called when the size event needs to be processed for the given tree-view control.
void tv_on_size(easygui_element* pTVElement, float newWidth, float newHeight);

/// Called when the mouse leave event needs to be processed for the given tree-view control.
void tv_on_mouse_leave(easygui_element* pTVElement);

/// Called when the mouse move event needs to be processed for the given tree-view control.
void tv_on_mouse_move(easygui_element* pTVElement, int relativeMousePosX, int relativeMousePosY);

/// Called when the mouse button down event needs to be processed for the given tree-view control.
void tv_on_mouse_button_down(easygui_element* pTVElement, int mouseButton, int relativeMousePosX, int relativeMousePosY);

/// Called when the mouse button up event needs to be processed for the given tree-view control.
void tv_on_mouse_button_up(easygui_element* pTVElement, int mouseButton, int relativeMousePosX, int relativeMousePosY);

/// Called when the mouse button double-click event needs to be processed for the given tree-view control.
void tv_on_mouse_button_dblclick(easygui_element* pTVElement, int mouseButton, int relativeMousePosX, int relativeMousePosY);

/// Called when the mouse wheel event needs to be processed for the given tree-view control.
void tv_on_mouse_wheel(easygui_element* pTVElement, int delta, int relativeMousePosX, int relativeMousePosY);

/// Called when the paint event needs to be processed for the given tree-view control.
void tv_on_paint(easygui_element* pTVElement, easygui_rect relativeClippingRect, void* pPaintData);


///////////////////////////////////////////////////////////////////////////////
//
// Tree-View Item
//
///////////////////////////////////////////////////////////////////////////////

/// Creates a tree view item.
///
/// @remarks
///     When pParent is non-null, the tree-view control must match that of the tree-view control that owns the
///     parent item.
eg_tree_view_item* tv_create_item(easygui_element* pTVElement, eg_tree_view_item* pParent, size_t extraDataSize, const void* pExtraData);

/// Recursively deletes a tree view item.
void tvi_delete(eg_tree_view_item* pItem);

/// Retrieves the tree-view GUI element that owns the given item.
easygui_element* tvi_get_tree_view_element(eg_tree_view_item* pItem);

/// Retrieves the size of the extra data associated with the given tree-view item.
size_t tvi_get_extra_data_size(eg_tree_view_item* pItem);

/// Retrieves a pointer to the extra data associated with the given tree-view item.
void* tvi_get_extra_data(eg_tree_view_item* pItem);


/// Retrieves the parent tree-view item.
eg_tree_view_item* tvi_get_parent(eg_tree_view_item* pItem);

/// Retrieves a pointer to the first child of the given tree-view item.
eg_tree_view_item* tvi_get_first_child(eg_tree_view_item* pItem);

/// Retrieves a pointer to the last child of the given tree-view item.
eg_tree_view_item* tvi_get_last_child(eg_tree_view_item* pItem);

/// Retrieves a pointer to the next sibling of the given tree-view item.
eg_tree_view_item* tvi_get_next_sibling(eg_tree_view_item* pItem);

/// Retrieves a pointer to the previous sibling of the given tree-view item.
eg_tree_view_item* tvi_get_prev_sibling(eg_tree_view_item* pItem);

/// Appends a tree view item as a child of the given parent item.
void tvi_append(eg_tree_view_item* pItem, eg_tree_view_item* pParent);

/// Prepends a tree view item as a child of the given parent item.
void tvi_prepend(eg_tree_view_item* pItem, eg_tree_view_item* pParent);

/// Appends the given tree view item to the given sibling.
void tvi_append_sibling(eg_tree_view_item* pItemToAppend, eg_tree_view_item* pItemToAppendTo);

/// Prepends the given tree view item to the given sibling.
void tvi_prepend_sibling(eg_tree_view_item* pItemToPrepend, eg_tree_view_item* pItemToPrependTo);


/// Determines whether or not the given item has any children.
bool tvi_has_children(eg_tree_view_item* pItem);

/// Retrieves the depth of the item.
///
/// @remarks
///     This is a recursive call and runs in linear time.
int tvi_get_depth(eg_tree_view_item* pItem);

/// Retrieves a pointer to the next visible item in the hierarchy that is not a child.
///
/// @remarks
///     This is used for iterating.
///     @par
///     <pDepthInOut> is an input and output parameter that is decremented whenver the next item is an ancestor.
eg_tree_view_item* tvi_next_visible_non_child(eg_tree_view_item* pItem, int* pDepthInOut);


/// Selects the given item.
void tvi_select(eg_tree_view_item* pItem);

/// Deselects the given item.
void tvi_deselect(eg_tree_view_item* pItem);

/// Determines whether or not the given tree view item is selected.
bool tvi_is_selected(eg_tree_view_item* pItem);

/// Expands the given item.
void tvi_expand(eg_tree_view_item* pItem);

/// Collapses the given item.
void tvi_collapse(eg_tree_view_item* pItem);

/// Determines whether or not the given item is expanded.
bool tvi_is_expanded(eg_tree_view_item* pItem);



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