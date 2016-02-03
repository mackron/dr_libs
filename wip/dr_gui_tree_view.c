// Public domain. See "unlicense" statement at the end of this file.

#include "dr_gui_tree_view.h"
#include "dr_gui_scrollbar.h"
#include <assert.h>

#ifndef PRIVATE
#define PRIVATE static
#endif

typedef struct drgui_tree_view drgui_tree_view;

struct drgui_tree_view
{
    /// The root tree-view item.
    drgui_tree_view_item* pRootItem;

    /// The vertical scrollbar.
    drgui_element* pScrollbarV;

    /// The horizontal scrollbar.
    drgui_element* pScrollbarH;


    /// The default background color.
    drgui_color defaultBGColor;

    /// The hovered background color.
    drgui_color hoveredBGColor;
    
    /// The selected background color.
    drgui_color selectedBGColor;

    /// The amount of indentation to apply to each child item.
    float childOffsetX;


    /// The function to call when an item needs to handle a mouse movement event.
    drgui_tvi_on_mouse_move_proc onItemMouseMove;

    /// The function to call when an item needs to handle a mouse leave event.
    drgui_tvi_on_mouse_leave_proc onItemMouseLeave;

    /// The function to call when an item needs to be drawn.
    drgui_tvi_on_paint_proc onItemPaint;

    /// The function to call when an item needs to be measured.
    drgui_tvi_measure_proc onItemMeasure;

    /// The function to call when an item is picked.
    drgui_tvi_on_picked_proc onItemPicked;


    /// A pointer to the item the mouse is current hovered over.
    drgui_tree_view_item* pHoveredItem;

    /// Whether or not the mouse is hovered over the arrow of pHoveredItem.
    bool isMouseOverArrow;

    /// Whether or not the mouse is over the given element.
    bool isMouseOver;

    /// The relative position of the mouse on the x axis. This is updated whenever the mouse_move event is received.
    int relativeMousePosX;

    /// The relative position of the mouse on the y axis. This is updated whenever the mouse_move event is received.
    int relativeMousePosY;


    /// Whether or not multi-select is enabled.
    bool isMultiSelectEnabled;

    /// Whether or not range-select is enabled.
    bool isRangeSelectEnabled;


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data buffer.
    char pExtraData[1];
};

struct drgui_tree_view_item
{
    /// The tree-view control that owns this item.
    drgui_element* pTVElement;


    /// A pointer to the parent item.
    drgui_tree_view_item* pParent;

    /// A pointer to the first child.
    drgui_tree_view_item* pFirstChild;

    /// A pointer to the last child.
    drgui_tree_view_item* pLastChild;

    /// A pointer to the next sibling.
    drgui_tree_view_item* pNextSibling;

    /// A pointer to the prev sibling.
    drgui_tree_view_item* pPrevSibling;


    /// Whether or not the item is select.
    bool isSelected;

    /// Whether or not the item is expanded.
    bool isExpanded;


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data buffer.
    char pExtraData[1];
};

typedef struct
{
    /// A pointer to the relevant item.
    drgui_tree_view_item* pItem;

    /// The width of the item.
    float width;

    /// The height of the item.
    float height;

    /// The position of the item on the x axis.
    float posX;

    /// Top position of the item on the y axis.
    float posY;

    /// The depth of the item. This is used to calculate the offset of the item.
    int depth;

} drgui_tree_view_iterator;

typedef struct
{
    /// The width of the item.
    float width;

    /// The height of the item.
    float height;

    /// The position of the item on the x axis.
    float posX;

    /// Top position of the item on the y axis.
    float posY;

} drgui_tree_view_item_metrics;

typedef struct
{
    /// A pointer to the tree-view control that owns the scrollbar.
    drgui_element* pTVElement;

} drgui_tree_view_scrollbar_data;




///////////////////////////////////////////////////////////////////////////////
//
// Tree-View
//
///////////////////////////////////////////////////////////////////////////////

/// Refreshes the layout of the given tree-view control and schedules a redraw.
PRIVATE void drgui_tv_refresh_and_redraw(drgui_element* pTVElement);

/// Repositions and resizes the scrollbars of the given tree-view control.
PRIVATE void drgui_tv_refresh_scrollbar_layouts(drgui_element* pTVElement);

/// Refreshes the ranges and page sizes of the scrollbars of the given tree-view control.
PRIVATE void drgui_tv_refresh_scrollbar_ranges(drgui_element* pTVElement);

/// Retrieves the rectangle of the little space that sits between the two scrollbars.
PRIVATE drgui_rect drgui_tv_get_scrollbar_dead_space_rect(drgui_element* pTVElement);

/// Retrieves the rectangle region that does not include the scrollbars. This rectangle is used for clipping when drawing the tree-view.
PRIVATE drgui_rect drgui_tv_get_inner_rect(drgui_element* pTVElement);

/// Paints the items of the given tree-view control.
PRIVATE void drgui_tv_paint_items(drgui_element* pTVElement, drgui_rect relativeClippingRect, void* pPaintData, float* pItemsBottomOut);

/// Creates an iterator beginning at the given item.
PRIVATE bool drgui_tv_begin_at(drgui_tree_view_item* pFirst, drgui_tree_view_iterator* pIteratorOut);

/// Moves to the next item in the iterator.
PRIVATE bool drgui_tv_next_visible(drgui_tree_view_iterator* pIterator);

/// Paints the given item.
PRIVATE void drgui_tv_paint_item(drgui_element* pTVElement, drgui_tree_view_item* pItem, drgui_rect relativeClippingRect, float posX, float posY, float width, float height, void* pPaintData);

/// Finds the item under the given point.
PRIVATE drgui_tree_view_item* drgui_tv_find_item_under_point(drgui_element* pTV, float relativePosX, float relativePosY, drgui_tree_view_item_metrics* pMetricsOut);

/// Recursively deselects every item, including the given one.
PRIVATE void drgui_tv_deselect_all_items_recursive(drgui_tree_view_item* pItem);

/// Called when the mouse enters a scrollbar. We use this to ensure there are no items marked as hovered as the use moves the
/// mouse from the tree-view to the scrollbars.
PRIVATE void drgui_tv_on_mouse_enter_scrollbar(drgui_element* pSBElement);

/// Called when the vertical scrollbar is scrolled.
PRIVATE void drgui_tv_on_scroll_v(drgui_element* pSBElement, int scrollPos);

/// Called when the horizontal scrollbar is scrolled.
PRIVATE void drgui_tv_on_scroll_h(drgui_element* pSBElement, int scrollPos);

/// Retrieves a pointer to the first visible item on the page, based on the scroll position.
PRIVATE drgui_tree_view_item* drgui_tv_find_first_visible_item_on_page(drgui_element* pTVElement);


drgui_element* drgui_create_tree_view(drgui_context* pContext, drgui_element* pParent, size_t extraDataSize, const void* pExtraData)
{
    drgui_element* pTVElement = drgui_create_element(pContext, pParent, sizeof(drgui_tree_view) - sizeof(char) + extraDataSize, NULL);
    if (pTVElement == NULL) {
        return NULL;
    }

    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    pTV->pRootItem = drgui_tv_create_item(pTVElement, NULL, 0, NULL);
    if (pTV->pRootItem == NULL) {
        return NULL;
    }


    drgui_tree_view_scrollbar_data sbdata;
    sbdata.pTVElement = pTVElement;

    pTV->pScrollbarV = drgui_create_scrollbar(pContext, pTVElement, drgui_sb_orientation_vertical, sizeof(sbdata), &sbdata);
    drgui_set_on_mouse_enter(pTV->pScrollbarV, drgui_tv_on_mouse_enter_scrollbar);
    drgui_sb_set_on_scroll(pTV->pScrollbarV, drgui_tv_on_scroll_v);
    
    pTV->pScrollbarH = drgui_create_scrollbar(pContext, pTVElement, drgui_sb_orientation_horizontal, sizeof(sbdata), &sbdata);
    drgui_set_on_mouse_enter(pTV->pScrollbarH, drgui_tv_on_mouse_enter_scrollbar);
    drgui_sb_set_on_scroll(pTV->pScrollbarH, drgui_tv_on_scroll_h);


    pTV->defaultBGColor    = drgui_rgb(96, 96, 96);
    pTV->hoveredBGColor    = drgui_rgb(112, 112, 112);
    pTV->selectedBGColor   = drgui_rgb(80, 160, 255);
    pTV->childOffsetX      = 16;

    pTV->onItemMouseMove   = NULL;
    pTV->onItemMouseLeave  = NULL;
    pTV->onItemPaint       = NULL;
    pTV->onItemMeasure     = NULL;
    pTV->onItemPicked      = NULL;

    pTV->pHoveredItem      = NULL;
    pTV->isMouseOverArrow  = false;
    pTV->isMouseOver       = false;
    pTV->relativeMousePosX = 0;
    pTV->relativeMousePosY = 0;

    pTV->isMultiSelectEnabled = false;
    pTV->isRangeSelectEnabled = false;

    pTV->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pTV->pExtraData, pExtraData, extraDataSize);
    }


    // Default event handlers.
    drgui_set_on_size(pTVElement, drgui_tv_on_size);
    drgui_set_on_mouse_leave(pTVElement, drgui_tv_on_mouse_leave);
    drgui_set_on_mouse_move(pTVElement, drgui_tv_on_mouse_move);
    drgui_set_on_mouse_button_down(pTVElement, drgui_tv_on_mouse_button_down);
    drgui_set_on_mouse_button_up(pTVElement, drgui_tv_on_mouse_button_up);
    drgui_set_on_mouse_button_dblclick(pTVElement, drgui_tv_on_mouse_button_dblclick);
    drgui_set_on_mouse_wheel(pTVElement, drgui_tv_on_mouse_wheel);
    drgui_set_on_paint(pTVElement, drgui_tv_on_paint);


    // Set the mouse wheel scale to 3 by default for the vertical scrollbar.
    drgui_sb_set_mouse_wheel_scele(pTV->pScrollbarV, 3);


    return pTVElement;
}

void drgui_delete_tree_view(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    // Recursively delete the tree view items.
    drgui_tvi_delete(pTV->pRootItem);

    // Delete the element last.
    drgui_delete_element(pTVElement);
}


size_t drgui_tv_get_extra_data_size(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return 0;
    }

    return pTV->extraDataSize;
}

void* drgui_tv_get_extra_data(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    return pTV->pExtraData;
}

drgui_tree_view_item* drgui_tv_get_root_item(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    return pTV->pRootItem;
}

drgui_element* drgui_tv_get_vertical_scrollbar(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    return pTV->pScrollbarV;
}

drgui_element* drgui_tv_get_horizontal_scrollbar(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    return pTV->pScrollbarH;
}


void drgui_tv_set_default_background_color(drgui_element* pTVElement, drgui_color color)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->defaultBGColor = color;
}

drgui_color drgui_tv_get_default_background_color(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return drgui_rgb(0, 0, 0);
    }

    return pTV->defaultBGColor;
}

void drgui_tv_set_hovered_background_color(drgui_element* pTVElement, drgui_color color)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->hoveredBGColor = color;
}

drgui_color drgui_tv_get_hovered_background_color(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return drgui_rgb(0, 0, 0);
    }

    return pTV->hoveredBGColor;
}

void drgui_tv_set_selected_background_color(drgui_element* pTVElement, drgui_color color)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->selectedBGColor = color;
}

drgui_color drgui_tv_get_selected_background_color(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return drgui_rgb(0, 0, 0);
    }

    return pTV->selectedBGColor;
}

void drgui_tv_set_child_offset_x(drgui_element* pTVElement, float childOffsetX)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->childOffsetX = childOffsetX;
}

float drgui_tv_get_child_offset_x(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return 0;
    }

    return pTV->childOffsetX;
}


bool drgui_tv_measure_item(drgui_element* pTVElement, drgui_tree_view_item* pItem, float* pWidthOut, float* pHeightOut)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return false;
    }

    if (pItem == NULL || pItem->pTVElement != pTVElement) {
        return false;
    }

    if (pTV->onItemMeasure)
    {
        pTV->onItemMeasure(pItem, pWidthOut, pHeightOut);
        return true;
    }

    return false;
}

void drgui_tv_deselect_all_items(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    drgui_tv_deselect_all_items_recursive(pTV->pRootItem);

    // TODO: Only redraw the region that actually changed.
    drgui_dirty(pTVElement, drgui_get_local_rect(pTVElement));
}


void drgui_tv_enable_multi_select(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->isMultiSelectEnabled = true;
}

void drgui_tv_disable_multi_select(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->isMultiSelectEnabled = false;
}

bool drgui_tv_is_multi_select_enabled(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return false;
    }

    return pTV->isMultiSelectEnabled;
}

drgui_tree_view_item* drgui_tv_get_first_selected_item(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    drgui_tree_view_iterator i;
    if (drgui_tv_begin_at(pTV->pRootItem->pFirstChild, &i))
    {
        do
        {
            if (drgui_tvi_is_selected(i.pItem)) {
                return i.pItem;
            }

        } while (drgui_tv_next_visible(&i));
    }

    return NULL;
}

drgui_tree_view_item* drgui_tv_get_next_selected_item(drgui_element* pTVElement, drgui_tree_view_item* pItem)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    drgui_tree_view_iterator i;
    if (drgui_tv_begin_at(pItem, &i))
    {
        // Note that we're not including <pItem> in this iteration.
        while (drgui_tv_next_visible(&i))
        {
            if (drgui_tvi_is_selected(i.pItem)) {
                return i.pItem;
            }
        }
    }

    return NULL;
}


void drgui_tv_set_on_item_mouse_move(drgui_element* pTVElement, drgui_tvi_on_mouse_move_proc proc)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemMouseMove = proc;
}

void drgui_tv_set_on_item_mouse_leave(drgui_element* pTVElement, drgui_tvi_on_mouse_leave_proc proc)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemMouseLeave = proc;
}

void drgui_tv_set_on_item_paint(drgui_element* pTVElement, drgui_tvi_on_paint_proc proc)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemPaint = proc;
}

void drgui_tv_set_on_item_measure(drgui_element* pTVElement, drgui_tvi_measure_proc proc)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemMeasure = proc;
}

void drgui_tv_set_on_item_picked(drgui_element* pTVElement, drgui_tvi_on_picked_proc proc)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemPicked = proc;
}


void drgui_tv_on_size(drgui_element* pTVElement, float newWidth, float newHeight)
{
    (void)newWidth;
    (void)newHeight;

    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    // Move the scrollbars.
    drgui_tv_refresh_scrollbar_layouts(pTVElement);

    // Refresh the scrollbar ranges.
    drgui_tv_refresh_scrollbar_ranges(pTVElement);
}

void drgui_tv_on_mouse_leave(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->isMouseOver = false;

    if (pTV->pHoveredItem != NULL || pTV->isMouseOverArrow)
    {
        if (pTV->onItemMouseLeave) {
            pTV->onItemMouseLeave(pTV->pHoveredItem);
        }

        pTV->pHoveredItem     = NULL;
        pTV->isMouseOverArrow = false;
        
        // For now just redraw the entire control, but should optimize this to only redraw the regions of the new and old hovered items.
        drgui_dirty(pTVElement, drgui_get_local_rect(pTVElement));
    }
}

void drgui_tv_on_mouse_move(drgui_element* pTVElement, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)stateFlags;

    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->isMouseOver       = true;
    pTV->relativeMousePosX = relativeMousePosX;
    pTV->relativeMousePosY = relativeMousePosY;

    // If the mouse has entered into the dead space between the scrollbars, we just pretend the mouse has left the tree-view
    // control entirely by posting a manual on_mouse_leave event and returning straight away.
    if (drgui_rect_contains_point(drgui_tv_get_scrollbar_dead_space_rect(pTVElement), (float)relativeMousePosX, (float)relativeMousePosY)) {
        drgui_tv_on_mouse_leave(pTVElement);
        return;
    }


    drgui_tree_view_item_metrics newHoveredItemMetrics;
    drgui_tree_view_item* pNewHoveredItem = drgui_tv_find_item_under_point(pTVElement, (float)relativeMousePosX, (float)relativeMousePosY, &newHoveredItemMetrics);
    drgui_tree_view_item* pOldHoveredItem = pTV->pHoveredItem;

    bool wasMouseOverArrow = pTV->isMouseOverArrow;
    pTV->isMouseOverArrow = false;

    if (pNewHoveredItem != NULL)
    {
        if (pTV->onItemMouseMove)
        {
            float relativeMousePosXToItem = (float)relativeMousePosX - newHoveredItemMetrics.posX + drgui_sb_get_scroll_position(pTV->pScrollbarH);
            float relativeMousePosYToItem = (float)relativeMousePosY - newHoveredItemMetrics.posY;

            if (relativeMousePosXToItem >= 0 && relativeMousePosXToItem < newHoveredItemMetrics.width &&
                relativeMousePosYToItem >= 0 && relativeMousePosYToItem < newHoveredItemMetrics.height)
            {
                pTV->onItemMouseMove(pNewHoveredItem, (int)relativeMousePosXToItem, (int)relativeMousePosYToItem, &pTV->isMouseOverArrow);
            }
        }
    }

    if (pNewHoveredItem != pOldHoveredItem || wasMouseOverArrow != pTV->isMouseOverArrow)
    {
        if (pNewHoveredItem != pOldHoveredItem && pOldHoveredItem != NULL)
        {
            if (pTV->onItemMouseLeave) {
                pTV->onItemMouseLeave(pOldHoveredItem);
            }
        }


        pTV->pHoveredItem = pNewHoveredItem;

        // TODO: Optimize this so that only the rectangle region encompassing the two relevant items is marked as dirty.
        drgui_dirty(pTVElement, drgui_get_local_rect(pTVElement));
    }
}

void drgui_tv_on_mouse_button_down(drgui_element* pTVElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    if (mouseButton == DRGUI_MOUSE_BUTTON_LEFT)
    {
        if (pTV->isMouseOverArrow)
        {
            if (drgui_tvi_is_expanded(pTV->pHoveredItem)) {
                drgui_tvi_collapse(pTV->pHoveredItem);
            } else {
                drgui_tvi_expand(pTV->pHoveredItem);
            }
        }
        else
        {
            if (pTV->isMultiSelectEnabled)
            {
                if (drgui_tvi_is_selected(pTV->pHoveredItem)) {
                    drgui_tvi_deselect(pTV->pHoveredItem);
                } else {
                    drgui_tvi_select(pTV->pHoveredItem);
                }
            }
            else
            {
                // TODO: Check if range selection is enabled and handle it here.

                drgui_tv_deselect_all_items(pTVElement);
                drgui_tvi_select(pTV->pHoveredItem);
            }
        }
    }
}

void drgui_tv_on_mouse_button_up(drgui_element* pTVElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)mouseButton;
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    // TODO: Implement me.
}

void drgui_tv_on_mouse_button_dblclick(drgui_element* pTVElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    if (mouseButton == DRGUI_MOUSE_BUTTON_LEFT)
    {
        if (!pTV->isMouseOverArrow)
        {
            if (drgui_tvi_has_children(pTV->pHoveredItem))
            {
                // It is a parent item, so toggle it.
                if (drgui_tvi_is_expanded(pTV->pHoveredItem)) {
                    drgui_tvi_collapse(pTV->pHoveredItem);
                } else {
                    drgui_tvi_expand(pTV->pHoveredItem);
                }
            }
            else
            {
                // It is a leaf item, so pick it.
                if (pTV->onItemPicked) {
                    pTV->onItemPicked(pTV->pHoveredItem);
                }
            }
        }
    }
}

void drgui_tv_on_mouse_wheel(drgui_element* pTVElement, int delta, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    drgui_sb_scroll(pTV->pScrollbarV, -delta * drgui_sb_get_mouse_wheel_scale(pTV->pScrollbarV));
}

void drgui_tv_on_paint(drgui_element* pTVElement, drgui_rect relativeClippingRect, void* pPaintData)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    // The dead space between the scrollbars should always be drawn with the default background color.
    drgui_draw_rect(pTVElement, drgui_tv_get_scrollbar_dead_space_rect(pTVElement), pTV->defaultBGColor, pPaintData);

    // The clipping rectangle needs to be clamped to the local rectangle that is shrunk such that it does not
    // include the scrollbars. If we don't do this we'll end up drawing underneath the scrollbars which will
    // cause flickering.
    drgui_rect innerClippingRect = drgui_clamp_rect(drgui_tv_get_inner_rect(pTVElement), relativeClippingRect);
    drgui_set_clip(pTVElement, innerClippingRect, pPaintData);


    // The main content of the tree-view is drawn in two parts. The first part (the top part) contains all of
    // the tree-view items. The second part (the bottom part) is just the background region that is not covered
    // by items.

    // We draw the tree-view items (the top part) first. This will retrieve the position of the bottom of the
    // items which is used to determine how much empty space is remaining below it so we can draw a quad over
    // that part.
    float itemsBottom = 0;
    drgui_tv_paint_items(pTVElement, innerClippingRect, pPaintData, &itemsBottom);


    // At this point the items have been drawn. All that remains is the part of the background that is not
    // covered by items. We can determine this by looking at <itemsBottom>.
    if (itemsBottom < relativeClippingRect.bottom && itemsBottom < drgui_get_relative_position_y(pTV->pScrollbarH))
    {
        drgui_draw_rect(pTVElement, drgui_make_rect(0, itemsBottom, drgui_get_relative_position_x(pTV->pScrollbarV), drgui_get_relative_position_y(pTV->pScrollbarH)), pTV->defaultBGColor, pPaintData);
    }
}


PRIVATE void drgui_tv_refresh_and_redraw(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }


    // Refresh scrollbar ranges and page sizes.
    drgui_tv_refresh_scrollbar_ranges(pTVElement);

    // For now, just redraw the entire control.
    drgui_dirty(pTVElement, drgui_get_local_rect(pTVElement));
}

PRIVATE void drgui_tv_refresh_scrollbar_layouts(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }


    // Vertical scrollbar.
    drgui_set_size(pTV->pScrollbarV, 16, drgui_get_height(pTVElement) - 16);
    drgui_set_relative_position(pTV->pScrollbarV, drgui_get_width(pTVElement) - drgui_get_width(pTV->pScrollbarV), 0);
    
    // Horizontal scrollbar.
    drgui_set_size(pTV->pScrollbarH, drgui_get_width(pTVElement) - 16, 16);
    drgui_set_relative_position(pTV->pScrollbarH, 0, drgui_get_height(pTVElement) - drgui_get_height(pTV->pScrollbarH));
}

PRIVATE void drgui_tv_refresh_scrollbar_ranges(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    float innerWidth = 0;
    unsigned int totalItemCount = 0;
    unsigned int pageItemCount = 0;

    drgui_tree_view_iterator i;
    if (drgui_tv_begin_at(pTV->pRootItem->pFirstChild, &i))
    {
        do
        {
            float itemRight = i.posX + i.width;
            if (itemRight > innerWidth) {
                innerWidth = itemRight;
            }

            float itemBottom = i.posY + i.height;
            if (itemBottom > 0 && itemBottom < drgui_get_relative_position_y(pTV->pScrollbarH)) {
                pageItemCount += 1;
            }

            totalItemCount += 1;

        } while (drgui_tv_next_visible(&i));
    }

    if (totalItemCount == 0)
    {
        // Vertical.
        drgui_sb_set_range(pTV->pScrollbarV, 0, 0);
        drgui_sb_set_page_size(pTV->pScrollbarV, 0);

        // Horizontal.
        drgui_sb_set_range(pTV->pScrollbarH, 0, 0);
        drgui_sb_set_page_size(pTV->pScrollbarH, 0);
    }
    else
    {
        // Vertical.
        drgui_sb_set_range(pTV->pScrollbarV, 0, (int)totalItemCount - 1); // - 1 because it's a 0-based range.
        drgui_sb_set_page_size(pTV->pScrollbarV, pageItemCount);

        // Horizontal.
        drgui_sb_set_range(pTV->pScrollbarH, 0, (int)innerWidth);
        drgui_sb_set_page_size(pTV->pScrollbarH, (int)drgui_get_relative_position_x(pTV->pScrollbarV));
    }
}

PRIVATE drgui_rect drgui_tv_get_scrollbar_dead_space_rect(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return drgui_make_rect(0, 0, 0, 0);
    }

    return drgui_make_rect(drgui_get_width(pTV->pScrollbarH), drgui_get_height(pTV->pScrollbarV), drgui_get_width(pTVElement), drgui_get_height(pTVElement));
}

PRIVATE drgui_rect drgui_tv_get_inner_rect(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return drgui_make_rect(0, 0, 0, 0);
    }

    drgui_rect result = drgui_get_local_rect(pTVElement);
    result.right  -= drgui_get_width(pTV->pScrollbarV);
    result.bottom -= drgui_get_height(pTV->pScrollbarH);

    return result;
}

PRIVATE void drgui_tv_paint_items(drgui_element* pTVElement, drgui_rect relativeClippingRect, void* pPaintData, float* pItemsBottomOut)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    float itemsBottom = 0;

    // For now we will begin at the root item, but later we want to begin at the first visible item which will be based on the
    // scroll position.
    drgui_tree_view_iterator i;
    if (drgui_tv_begin_at(drgui_tv_find_first_visible_item_on_page(pTVElement), &i))
    {
        do
        {
            drgui_tv_paint_item(pTVElement, i.pItem, relativeClippingRect, i.posX, i.posY, i.width, i.height, pPaintData);
            
            // Restore the clipping rectangle in case the application changed the clipping rectangle.
            drgui_set_clip(pTVElement, relativeClippingRect, pPaintData);

            itemsBottom = i.posY + i.height;

        } while (itemsBottom < relativeClippingRect.bottom && drgui_tv_next_visible(&i));
    }


    if (pItemsBottomOut != NULL) {
        *pItemsBottomOut = itemsBottom;
    }
}

PRIVATE bool drgui_tv_begin_at(drgui_tree_view_item* pFirst, drgui_tree_view_iterator* pIteratorOut)
{
    if (pFirst == NULL || pIteratorOut == NULL) {
        return false;
    }

    if (!drgui_tv_measure_item(pFirst->pTVElement, pFirst, &pIteratorOut->width, &pIteratorOut->height)) {
        return false;
    }

    const int depth = drgui_tvi_get_depth(pFirst);

    pIteratorOut->pItem = pFirst;
    pIteratorOut->depth = depth;
    pIteratorOut->posX  = depth * drgui_tv_get_child_offset_x(pFirst->pTVElement);
    pIteratorOut->posY  = 0;
    
    return true;
}

PRIVATE bool drgui_tv_next_visible(drgui_tree_view_iterator* pIterator)
{
    assert(pIterator != NULL);

    if (pIterator->pItem == NULL) {
        return false;
    }

    drgui_tree_view* pTV = drgui_get_extra_data(pIterator->pItem->pTVElement);
    if (pTV == NULL) {
        return false;
    }

    if (drgui_tvi_has_children(pIterator->pItem) && drgui_tvi_is_expanded(pIterator->pItem))
    {
        pIterator->pItem = pIterator->pItem->pFirstChild;
        pIterator->depth += 1;
    }
    else
    {
        pIterator->pItem = drgui_tvi_next_visible_non_child(pIterator->pItem, &pIterator->depth);
    }


    if (pIterator->pItem == NULL) {
        return false;
    }

    pIterator->posX  = pIterator->depth * drgui_tv_get_child_offset_x(pIterator->pItem->pTVElement);
    pIterator->posY += pIterator->height;

    if (!drgui_tv_measure_item(pIterator->pItem->pTVElement, pIterator->pItem, &pIterator->width, &pIterator->height)) {
        return false;
    }

    return true;
}

PRIVATE void drgui_tv_paint_item(drgui_element* pTVElement, drgui_tree_view_item* pItem, drgui_rect relativeClippingRect, float posX, float posY, float width, float height, void* pPaintData)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    if (pTV->onItemPaint)
    {
        // We draw an item in two main parts, with the first part being the background section to the left and right of the item and the
        // second part being the item itself. The first part we do ourselves, whereas the second part we pass off to the host application.
        
        // The background section to the left and right of the main content is done first, by us.
        drgui_color bgcolor;
        if (drgui_tvi_is_selected(pItem)) {
            bgcolor = pTV->selectedBGColor;
        } else if (pTV->pHoveredItem == pItem) {
            bgcolor = pTV->hoveredBGColor;
        } else {
            bgcolor = pTV->defaultBGColor;
        }

        float innerOffsetX = (float)-drgui_sb_get_scroll_position(pTV->pScrollbarH);

        // Left.
        if (posX + innerOffsetX > 0) {
            drgui_draw_rect(pTVElement, drgui_make_rect(0, posY, posX + innerOffsetX, posY + height), bgcolor, pPaintData);
        }
        
        // Right.
        if (posX + width + innerOffsetX < drgui_get_relative_position_x(pTV->pScrollbarV)) {
            drgui_draw_rect(pTVElement, drgui_make_rect(posX + width + innerOffsetX, posY, drgui_get_relative_position_x(pTV->pScrollbarV), posY + height), bgcolor, pPaintData);
        }


        // At this point if were to finish drawing we'd have a hole where the main content of the item should be. To fill this we need to
        // let the host application do it.
        pTV->onItemPaint(pTVElement, pItem, relativeClippingRect, bgcolor, posX + innerOffsetX, posY, width, height, pPaintData);
    }
}

PRIVATE drgui_tree_view_item* drgui_tv_find_item_under_point(drgui_element* pTVElement, float relativePosX, float relativePosY, drgui_tree_view_item_metrics* pMetricsOut)
{
    (void)relativePosX; // <-- Unused because we treat items as though they are infinitely wide.

    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    // For now we will begin at the root item, but later we want to begin at the first visible item which will be based on the
    // scroll position.
    drgui_tree_view_iterator i;
    if (drgui_tv_begin_at(drgui_tv_find_first_visible_item_on_page(pTVElement), &i))
    {
        do
        {
            if (relativePosY >= i.posY && relativePosY < i.posY + i.height)
            {
                if (pMetricsOut != NULL)
                {
                    pMetricsOut->posX   = i.posX;
                    pMetricsOut->posY   = i.posY;
                    pMetricsOut->width  = i.width;
                    pMetricsOut->height = i.height;
                }

                return i.pItem;
            }

        } while ((i.posY + i.height < drgui_get_relative_position_y(pTV->pScrollbarH)) && drgui_tv_next_visible(&i));
    }

    return NULL;
}

PRIVATE void drgui_tv_deselect_all_items_recursive(drgui_tree_view_item* pItem)
{
    pItem->isSelected = false;

    for (drgui_tree_view_item* pChild = pItem->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling)
    {
        drgui_tv_deselect_all_items_recursive(pChild);
    }
}

PRIVATE void drgui_tv_on_mouse_enter_scrollbar(drgui_element* pSBElement)
{
    drgui_tree_view_scrollbar_data* pSB = drgui_sb_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    // We just pretend the mouse has left the tree-view entirely. This will ensure any item marked as hovered is unmarked and redrawn.
    drgui_tv_on_mouse_leave(pSB->pTVElement);
}

PRIVATE void drgui_tv_on_scroll_v(drgui_element* pSBElement, int scrollPos)
{
    (void)scrollPos;

    drgui_tree_view_scrollbar_data* pSB = drgui_sb_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    drgui_tree_view* pTV = drgui_get_extra_data(pSB->pTVElement);
    if (pTV == NULL) {
        return;
    }

    // As we scroll, the mouse will be placed over a different item. We just post a manual mouse_move event to trigger a refresh.
    if (pTV->isMouseOver) {
        drgui_tv_on_mouse_move(pSB->pTVElement, pTV->relativeMousePosX, pTV->relativeMousePosY, 0);
    }

    // The paint routine is tied directly to the scrollbars, so all we need to do is mark it as dirty to trigger a redraw.
    drgui_dirty(pSB->pTVElement, drgui_get_local_rect(pSB->pTVElement));
}

PRIVATE void drgui_tv_on_scroll_h(drgui_element* pSBElement, int scrollPos)
{
    (void)scrollPos;

    drgui_tree_view_scrollbar_data* pSB = drgui_sb_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    drgui_tree_view* pTV = drgui_get_extra_data(pSB->pTVElement);
    if (pTV == NULL) {
        return;
    }

    // The paint routine is tied directly to the scrollbars, so all we need to do is mark it as dirty to trigger a redraw.
    drgui_dirty(pSB->pTVElement, drgui_get_local_rect(pSB->pTVElement));
}

PRIVATE drgui_tree_view_item* drgui_tv_find_first_visible_item_on_page(drgui_element* pTVElement)
{
    drgui_tree_view* pTV = drgui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    // We just keep iterating until we hit the index of the scroll position.
    int index = 0;

    drgui_tree_view_iterator i;
    if (drgui_tv_begin_at(pTV->pRootItem->pFirstChild, &i))
    {
        do
        {
            if (index == drgui_sb_get_scroll_position(pTV->pScrollbarV)) {
                return i.pItem;
            }

            index += 1;

        } while (drgui_tv_next_visible(&i));
    }

    return NULL;
}



///////////////////////////////////////////////////////////////////////////////
//
// Tree-View Item
//
///////////////////////////////////////////////////////////////////////////////

/// Detaches the given tree-view item from it's parent and siblings.
PRIVATE void drgui_tvi_detach(drgui_tree_view_item* pItem);

drgui_tree_view_item* drgui_tv_create_item(drgui_element* pTVElement, drgui_tree_view_item* pParent, size_t extraDataSize, const void* pExtraData)
{
    if (pTVElement == NULL) {
        return NULL;
    }

    if (pParent != NULL && pParent->pTVElement != pTVElement) {
        return NULL;
    }


    drgui_tree_view_item* pItem = malloc(sizeof(*pItem) + extraDataSize - sizeof(pItem->pExtraData));
    if (pItem == NULL) {
        return NULL;
    }

    pItem->pTVElement    = pTVElement;
    pItem->pParent       = NULL;
    pItem->pFirstChild   = NULL;
    pItem->pLastChild    = NULL;
    pItem->pNextSibling  = NULL;
    pItem->pPrevSibling  = NULL;
    pItem->isSelected    = false;
    pItem->isExpanded    = false;

    pItem->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pItem->pExtraData, pExtraData, extraDataSize);
    }


    // Append the item to the end of the parent item.
    drgui_tvi_append(pItem, pParent);

    return pItem;
}

void drgui_tvi_delete(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }


    // Children need to be deleted first.
    while (pItem->pFirstChild != NULL) {
        drgui_tvi_delete(pItem->pFirstChild);
    }

    // We need to grab a pointer to the main tree-view control so we can refresh and redraw it after we have detached the item.
    drgui_element* pTVElement = pItem->pTVElement;

    // The item needs to be completely detached first.
    drgui_tvi_detach(pItem);

    // Refresh the layout and redraw the tree-view control.
    drgui_tv_refresh_and_redraw(pTVElement);

    // Free the item last for safety.
    free(pItem);
}

drgui_element* drgui_tvi_get_tree_view_element(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pTVElement;
}

size_t drgui_tvi_get_extra_data_size(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return 0;
    }

    return pItem->extraDataSize;
}

void* drgui_tvi_get_extra_data(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pExtraData;
}


drgui_tree_view_item* drgui_tvi_get_parent(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pParent;
}

drgui_tree_view_item* drgui_tvi_get_first_child(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pFirstChild;
}

drgui_tree_view_item* drgui_tvi_get_last_child(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pLastChild;
}

drgui_tree_view_item* drgui_tvi_get_next_sibling(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pNextSibling;
}

drgui_tree_view_item* drgui_tvi_get_prev_sibling(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pPrevSibling;
}

void drgui_tvi_append(drgui_tree_view_item* pItem, drgui_tree_view_item* pParent)
{
    if (pItem == NULL) {
        return;
    }

    drgui_tree_view* pTV = drgui_get_extra_data(pItem->pTVElement);
    if (pTV == NULL) {
        return;
    }


    // If a parent was not specified, append to the root item.
    if (pParent == NULL)
    {
        if (pTV->pRootItem != NULL) {
            drgui_tvi_append(pItem, pTV->pRootItem);
        }
    }
    else
    {
        assert(pItem->pTVElement == pParent->pTVElement);

        // Detach the child from it's current parent first.
        drgui_tvi_detach(pItem);

        pItem->pParent = pParent;
        assert(pItem->pParent != NULL);

        if (pItem->pParent->pLastChild != NULL) {
            pItem->pPrevSibling = pItem->pParent->pLastChild;
            pItem->pPrevSibling->pNextSibling = pItem;
        }

        if (pItem->pParent->pFirstChild == NULL) {
            pItem->pParent->pFirstChild = pItem;
        }

        pItem->pParent->pLastChild = pItem;


        // Refresh the layout and redraw the tree-view control.
        drgui_tv_refresh_and_redraw(pItem->pTVElement);
    }
}

void drgui_tvi_prepend(drgui_tree_view_item* pItem, drgui_tree_view_item* pParent)
{
    if (pItem == NULL) {
        return;
    }

    drgui_tree_view* pTV = drgui_get_extra_data(pItem->pTVElement);
    if (pTV == NULL) {
        return;
    }


    // If a parent was not specified, prepend to the root item.
    if (pParent == NULL)
    {
        if (pTV->pRootItem != NULL) {   
            drgui_tvi_prepend(pItem, pTV->pRootItem);
        }
    }
    else
    {
        assert(pItem->pTVElement == pParent->pTVElement);

        // Detach the child from it's current parent first.
        drgui_tvi_detach(pItem);

        pItem->pParent = pParent;
        assert(pItem->pParent != NULL);

        if (pItem->pParent->pFirstChild != NULL) {
            pItem->pNextSibling = pItem->pParent->pFirstChild;
            pItem->pNextSibling->pPrevSibling = pItem;
        }

        if (pItem->pParent->pLastChild == NULL) {
            pItem->pParent->pLastChild = pItem;
        }

        pItem->pParent->pFirstChild = pItem;


        // Refresh the layout and redraw the tree-view control.
        drgui_tv_refresh_and_redraw(pItem->pTVElement);
    }
}

void drgui_tvi_append_sibling(drgui_tree_view_item* pItemToAppend, drgui_tree_view_item* pItemToAppendTo)
{
    if (pItemToAppend == NULL) {
        return;
    }

    drgui_tree_view* pTV = drgui_get_extra_data(pItemToAppend->pTVElement);
    if (pTV == NULL) {
        return;
    }


    // If a parent was not specified, append to the root item.
    if (pItemToAppendTo == NULL)
    {
        if (pTV->pRootItem != NULL) {
            drgui_tvi_append(pItemToAppend, pTV->pRootItem);
        }
    }
    else
    {
        assert(pItemToAppend->pTVElement == pItemToAppendTo->pTVElement);

        // Detach the child from it's current parent first.
        drgui_tvi_detach(pItemToAppend);


        pItemToAppend->pParent = pItemToAppendTo->pParent;
        assert(pItemToAppend->pParent != NULL);

        pItemToAppend->pNextSibling = pItemToAppendTo->pNextSibling;
        pItemToAppend->pPrevSibling = pItemToAppendTo;

        pItemToAppendTo->pNextSibling->pPrevSibling = pItemToAppend;
        pItemToAppendTo->pNextSibling = pItemToAppend;

        if (pItemToAppend->pParent->pLastChild == pItemToAppendTo) {
            pItemToAppend->pParent->pLastChild = pItemToAppend;
        }


        // Refresh the layout and redraw the tree-view control.
        drgui_tv_refresh_and_redraw(pItemToAppend->pTVElement);
    }
}

void drgui_tvi_prepend_sibling(drgui_tree_view_item* pItemToPrepend, drgui_tree_view_item* pItemToPrependTo)
{
    if (pItemToPrepend == NULL) {
        return;
    }

    drgui_tree_view* pTV = drgui_get_extra_data(pItemToPrepend->pTVElement);
    if (pTV == NULL) {
        return;
    }


    // If a parent was not specified, prepend to the root item.
    if (pItemToPrependTo == NULL)
    {
        if (pTV->pRootItem != NULL) {
            drgui_tvi_prepend(pItemToPrepend, pTV->pRootItem);
        }
    }
    else
    {
        assert(pItemToPrepend->pTVElement == pItemToPrependTo->pTVElement);

        // Detach the child from it's current parent first.
        drgui_tvi_detach(pItemToPrepend);


        pItemToPrepend->pParent = pItemToPrependTo->pParent;
        assert(pItemToPrepend->pParent != NULL);

        pItemToPrepend->pPrevSibling = pItemToPrependTo->pNextSibling;
        pItemToPrepend->pNextSibling = pItemToPrependTo;

        pItemToPrependTo->pPrevSibling->pNextSibling = pItemToPrepend;
        pItemToPrependTo->pNextSibling = pItemToPrepend;

        if (pItemToPrepend->pParent->pFirstChild == pItemToPrependTo) {
            pItemToPrepend->pParent->pFirstChild = pItemToPrepend;
        }


        // Refresh the layout and redraw the tree-view control.
        drgui_tv_refresh_and_redraw(pItemToPrepend->pTVElement);
    }
}


bool drgui_tvi_has_children(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return false;
    }

    return pItem->pFirstChild != NULL;
}

int drgui_tvi_get_depth(drgui_tree_view_item* pItem)
{
    if (pItem->pParent == NULL || pItem->pParent == drgui_tv_get_root_item(pItem->pTVElement)) {
        return 0;
    }

    return drgui_tvi_get_depth(pItem->pParent) + 1;
}

drgui_tree_view_item* drgui_tvi_next_visible_non_child(drgui_tree_view_item* pItem, int* pDepthInOut)
{
    if (pItem == NULL) {
        return NULL;
    }

    if (pItem->pNextSibling != NULL) {
        return pItem->pNextSibling;
    }


    if (pDepthInOut != NULL) {
        *pDepthInOut -= 1;
    }
    
    return drgui_tvi_next_visible_non_child(pItem->pParent, pDepthInOut);
}


void drgui_tvi_select(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }

    if (!pItem->isSelected)
    {
        pItem->isSelected = true;
        drgui_dirty(pItem->pTVElement, drgui_get_local_rect(pItem->pTVElement));
    }
}

void drgui_tvi_deselect(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }

    if (pItem->isSelected)
    {
        pItem->isSelected = false;
        drgui_dirty(pItem->pTVElement, drgui_get_local_rect(pItem->pTVElement));
    }
}

bool drgui_tvi_is_selected(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return false;
    }

    return pItem->isSelected;
}

void drgui_tvi_expand(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }

    if (!pItem->isExpanded)
    {
        pItem->isExpanded = true;
        drgui_tv_refresh_and_redraw(pItem->pTVElement);
    }
}

void drgui_tvi_collapse(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }

    if (pItem->isExpanded)
    {
        pItem->isExpanded = false;
        drgui_tv_refresh_and_redraw(pItem->pTVElement);
    }
}

bool drgui_tvi_is_expanded(drgui_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return false;
    }

    return pItem->isExpanded;
}



PRIVATE void drgui_tvi_detach(drgui_tree_view_item* pItem)
{
    assert(pItem != NULL);

    if (pItem->pParent != NULL)
    {
        if (pItem->pParent->pFirstChild == pItem) {
            pItem->pParent->pFirstChild = pItem->pNextSibling;
        }

        if (pItem->pParent->pLastChild == pItem) {
            pItem->pParent->pLastChild = pItem->pPrevSibling;
        }


        if (pItem->pPrevSibling != NULL) {
            pItem->pPrevSibling->pNextSibling = pItem->pNextSibling;
        }

        if (pItem->pNextSibling != NULL) {
            pItem->pNextSibling->pPrevSibling = pItem->pPrevSibling;
        }
    }

    pItem->pParent      = NULL;
    pItem->pPrevSibling = NULL;
    pItem->pNextSibling = NULL;
}
