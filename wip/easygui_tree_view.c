// Public domain. See "unlicense" statement at the end of this file.

#include "easygui_tree_view.h"
#include <easy_gui/wip/easygui_tree_view.h>
#include <easy_gui/wip/easygui_scrollbar.h>
#include <assert.h>

typedef struct eg_tree_view eg_tree_view;

struct eg_tree_view
{
    /// The root tree-view item.
    eg_tree_view_item* pRootItem;

    /// The vertical scrollbar.
    easygui_element* pScrollbarV;

    /// The horizontal scrollbar.
    easygui_element* pScrollbarH;


    /// The default background color.
    easygui_color defaultBGColor;

    /// The hovered background color.
    easygui_color hoveredBGColor;
    
    /// The selected background color.
    easygui_color selectedBGColor;

    /// The amount of indentation to apply to each child item.
    float childOffsetX;


    /// The function to call when an item needs to handle a mouse movement event.
    tvi_on_mouse_move_proc onItemMouseMove;

    /// The function to call when an item needs to handle a mouse leave event.
    tvi_on_mouse_leave_proc onItemMouseLeave;

    /// The function to call when an item needs to be drawn.
    tvi_on_paint_proc onItemPaint;

    /// The function to call when an item needs to be measured.
    tvi_measure_proc onItemMeasure;

    /// The function to call when an item is picked.
    tvi_on_picked_proc onItemPicked;


    /// A pointer to the item the mouse is current hovered over.
    eg_tree_view_item* pHoveredItem;

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

struct eg_tree_view_item
{
    /// The tree-view control that owns this item.
    easygui_element* pTVElement;


    /// A pointer to the parent item.
    eg_tree_view_item* pParent;

    /// A pointer to the first child.
    eg_tree_view_item* pFirstChild;

    /// A pointer to the last child.
    eg_tree_view_item* pLastChild;

    /// A pointer to the next sibling.
    eg_tree_view_item* pNextSibling;

    /// A pointer to the prev sibling.
    eg_tree_view_item* pPrevSibling;


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
    eg_tree_view_item* pItem;

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

} eg_tree_view_iterator;

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

} eg_tree_view_item_metrics;

typedef struct
{
    /// A pointer to the tree-view control that owns the scrollbar.
    easygui_element* pTVElement;

} eg_tree_view_scrollbar_data;




///////////////////////////////////////////////////////////////////////////////
//
// Tree-View
//
///////////////////////////////////////////////////////////////////////////////

/// Refreshes the layout of the given tree-view control and schedules a redraw.
PRIVATE void tv_refresh_and_redraw(easygui_element* pTVElement);

/// Repositions and resizes the scrollbars of the given tree-view control.
PRIVATE void tv_refresh_scrollbar_layouts(easygui_element* pTVElement);

/// Refreshes the ranges and page sizes of the scrollbars of the given tree-view control.
PRIVATE void tv_refresh_scrollbar_ranges(easygui_element* pTVElement);

/// Retrieves the rectangle of the little space that sits between the two scrollbars.
PRIVATE easygui_rect tv_get_scrollbar_dead_space_rect(easygui_element* pTVElement);

/// Retrieves the rectangle region that does not include the scrollbars. This rectangle is used for clipping when drawing the tree-view.
PRIVATE easygui_rect tv_get_inner_rect(easygui_element* pTVElement);

/// Paints the items of the given tree-view control.
PRIVATE void tv_paint_items(easygui_element* pTVElement, easygui_rect relativeClippingRect, void* pPaintData, float* pItemsBottomOut);

/// Creates an iterator beginning at the given item.
PRIVATE bool tv_begin_at(eg_tree_view_item* pFirst, eg_tree_view_iterator* pIteratorOut);

/// Moves to the next item in the iterator.
PRIVATE bool tv_next_visible(eg_tree_view_iterator* pIterator);

/// Paints the given item.
PRIVATE void tv_paint_item(easygui_element* pTVElement, eg_tree_view_item* pItem, easygui_rect relativeClippingRect, float posX, float posY, float width, float height, void* pPaintData);

/// Finds the item under the given point.
PRIVATE eg_tree_view_item* tv_find_item_under_point(easygui_element* pTV, float relativePosX, float relativePosY, eg_tree_view_item_metrics* pMetricsOut);

/// Recursively deselects every item, including the given one.
PRIVATE void tv_deselect_all_items_recursive(eg_tree_view_item* pItem);

/// Called when the mouse enters a scrollbar. We use this to ensure there are no items marked as hovered as the use moves the
/// mouse from the tree-view to the scrollbars.
PRIVATE void tv_on_mouse_enter_scrollbar(easygui_element* pSBElement);

/// Called when the vertical scrollbar is scrolled.
PRIVATE void tv_on_scroll_v(easygui_element* pSBElement, int scrollPos);

/// Called when the horizontal scrollbar is scrolled.
PRIVATE void tv_on_scroll_h(easygui_element* pSBElement, int scrollPos);

/// Retrieves a pointer to the first visible item on the page, based on the scroll position.
PRIVATE eg_tree_view_item* tv_find_first_visible_item_on_page(easygui_element* pTVElement);


easygui_element* eg_create_tree_view(easygui_context* pContext, easygui_element* pParent, size_t extraDataSize, const void* pExtraData)
{
    easygui_element* pTVElement = easygui_create_element(pContext, pParent, sizeof(eg_tree_view) - sizeof(char) + extraDataSize);
    if (pTVElement == NULL) {
        return NULL;
    }

    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    pTV->pRootItem = tv_create_item(pTVElement, NULL, 0, NULL);
    if (pTV->pRootItem == NULL) {
        return NULL;
    }


    eg_tree_view_scrollbar_data sbdata;
    sbdata.pTVElement = pTVElement;

    pTV->pScrollbarV = easygui_create_scrollbar(pContext, pTVElement, sb_orientation_vertical, sizeof(sbdata), &sbdata);
    easygui_set_on_mouse_enter(pTV->pScrollbarV, tv_on_mouse_enter_scrollbar);
    sb_set_on_scroll(pTV->pScrollbarV, tv_on_scroll_v);
    
    pTV->pScrollbarH = easygui_create_scrollbar(pContext, pTVElement, sb_orientation_horizontal, sizeof(sbdata), &sbdata);
    easygui_set_on_mouse_enter(pTV->pScrollbarH, tv_on_mouse_enter_scrollbar);
    sb_set_on_scroll(pTV->pScrollbarH, tv_on_scroll_h);


    pTV->defaultBGColor    = easygui_rgb(96, 96, 96);
    pTV->hoveredBGColor    = easygui_rgb(112, 112, 112);
    pTV->selectedBGColor   = easygui_rgb(80, 160, 255);
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
    easygui_set_on_size(pTVElement, tv_on_size);
    easygui_set_on_mouse_leave(pTVElement, tv_on_mouse_leave);
    easygui_set_on_mouse_move(pTVElement, tv_on_mouse_move);
    easygui_set_on_mouse_button_down(pTVElement, tv_on_mouse_button_down);
    easygui_set_on_mouse_button_up(pTVElement, tv_on_mouse_button_up);
    easygui_set_on_mouse_button_dblclick(pTVElement, tv_on_mouse_button_dblclick);
    easygui_set_on_mouse_wheel(pTVElement, tv_on_mouse_wheel);
    easygui_set_on_paint(pTVElement, tv_on_paint);


    // Set the mouse wheel scale to 3 by default for the vertical scrollbar.
    sb_set_mouse_wheel_scele(pTV->pScrollbarV, 3);


    return pTVElement;
}

void eg_delete_tree_view(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    // Recursively delete the tree view items.
    tvi_delete(pTV->pRootItem);

    // Delete the element last.
    easygui_delete_element(pTVElement);
}


size_t tv_get_extra_data_size(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return 0;
    }

    return pTV->extraDataSize;
}

void* tv_get_extra_data(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    return pTV->pExtraData;
}

eg_tree_view_item* tv_get_root_item(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    return pTV->pRootItem;
}

easygui_element* tv_get_vertical_scrollbar(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    return pTV->pScrollbarV;
}

easygui_element* tv_get_horizontal_scrollbar(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    return pTV->pScrollbarH;
}


void tv_set_default_background_color(easygui_element* pTVElement, easygui_color color)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->defaultBGColor = color;
}

easygui_color tv_get_default_background_color(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return easygui_rgb(0, 0, 0);
    }

    return pTV->defaultBGColor;
}

void tv_set_hovered_background_color(easygui_element* pTVElement, easygui_color color)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->hoveredBGColor = color;
}

easygui_color tv_get_hovered_background_color(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return easygui_rgb(0, 0, 0);
    }

    return pTV->hoveredBGColor;
}

void tv_set_selected_background_color(easygui_element* pTVElement, easygui_color color)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->selectedBGColor = color;
}

easygui_color tv_get_selected_background_color(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return easygui_rgb(0, 0, 0);
    }

    return pTV->selectedBGColor;
}

void tv_set_child_offset_x(easygui_element* pTVElement, float childOffsetX)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->childOffsetX = childOffsetX;
}

float tv_get_child_offset_x(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return 0;
    }

    return pTV->childOffsetX;
}


bool tv_measure_item(easygui_element* pTVElement, eg_tree_view_item* pItem, float* pWidthOut, float* pHeightOut)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
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

void tv_deselect_all_items(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    tv_deselect_all_items_recursive(pTV->pRootItem);

    // TODO: Only redraw the region that actually changed.
    easygui_dirty(pTVElement, easygui_get_local_rect(pTVElement));
}


void tv_enable_multi_select(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->isMultiSelectEnabled = true;
}

void tv_disable_multi_select(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->isMultiSelectEnabled = false;
}

bool tv_is_multi_select_enabled(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return false;
    }

    return pTV->isMultiSelectEnabled;
}

eg_tree_view_item* tv_get_first_selected_item(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    eg_tree_view_iterator i;
    if (tv_begin_at(pTV->pRootItem->pFirstChild, &i))
    {
        do
        {
            if (tvi_is_selected(i.pItem)) {
                return i.pItem;
            }

        } while (tv_next_visible(&i));
    }

    return NULL;
}

eg_tree_view_item* tv_get_next_selected_item(easygui_element* pTVElement, eg_tree_view_item* pItem)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    eg_tree_view_iterator i;
    if (tv_begin_at(pItem, &i))
    {
        // Note that we're not including <pItem> in this iteration.
        while (tv_next_visible(&i))
        {
            if (tvi_is_selected(i.pItem)) {
                return i.pItem;
            }
        }
    }

    return NULL;
}


void tv_set_on_item_mouse_move(easygui_element* pTVElement, tvi_on_mouse_move_proc proc)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemMouseMove = proc;
}

void tv_set_on_item_mouse_leave(easygui_element* pTVElement, tvi_on_mouse_leave_proc proc)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemMouseLeave = proc;
}

void tv_set_on_item_paint(easygui_element* pTVElement, tvi_on_paint_proc proc)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemPaint = proc;
}

void tv_set_on_item_measure(easygui_element* pTVElement, tvi_measure_proc proc)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemMeasure = proc;
}

void tv_set_on_item_picked(easygui_element* pTVElement, tvi_on_picked_proc proc)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->onItemPicked = proc;
}


void tv_on_size(easygui_element* pTVElement, float newWidth, float newHeight)
{
    (void)newWidth;
    (void)newHeight;

    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    // Move the scrollbars.
    tv_refresh_scrollbar_layouts(pTVElement);

    // Refresh the scrollbar ranges.
    tv_refresh_scrollbar_ranges(pTVElement);
}

void tv_on_mouse_leave(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
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
        easygui_dirty(pTVElement, easygui_get_local_rect(pTVElement));
    }
}

void tv_on_mouse_move(easygui_element* pTVElement, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)stateFlags;

    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    pTV->isMouseOver       = true;
    pTV->relativeMousePosX = relativeMousePosX;
    pTV->relativeMousePosY = relativeMousePosY;

    // If the mouse has entered into the dead space between the scrollbars, we just pretend the mouse has left the tree-view
    // control entirely by posting a manual on_mouse_leave event and returning straight away.
    if (easygui_rect_contains_point(tv_get_scrollbar_dead_space_rect(pTVElement), (float)relativeMousePosX, (float)relativeMousePosY)) {
        tv_on_mouse_leave(pTVElement);
        return;
    }


    eg_tree_view_item_metrics newHoveredItemMetrics;
    eg_tree_view_item* pNewHoveredItem = tv_find_item_under_point(pTVElement, (float)relativeMousePosX, (float)relativeMousePosY, &newHoveredItemMetrics);
    eg_tree_view_item* pOldHoveredItem = pTV->pHoveredItem;

    bool wasMouseOverArrow = pTV->isMouseOverArrow;
    pTV->isMouseOverArrow = false;

    if (pNewHoveredItem != NULL)
    {
        if (pTV->onItemMouseMove)
        {
            float relativeMousePosXToItem = (float)relativeMousePosX - newHoveredItemMetrics.posX + sb_get_scroll_position(pTV->pScrollbarH);
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
        easygui_dirty(pTVElement, easygui_get_local_rect(pTVElement));
    }
}

void tv_on_mouse_button_down(easygui_element* pTVElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    if (mouseButton == EASYGUI_MOUSE_BUTTON_LEFT)
    {
        if (pTV->isMouseOverArrow)
        {
            if (tvi_is_expanded(pTV->pHoveredItem)) {
                tvi_collapse(pTV->pHoveredItem);
            } else {
                tvi_expand(pTV->pHoveredItem);
            }
        }
        else
        {
            if (pTV->isMultiSelectEnabled)
            {
                if (tvi_is_selected(pTV->pHoveredItem)) {
                    tvi_deselect(pTV->pHoveredItem);
                } else {
                    tvi_select(pTV->pHoveredItem);
                }
            }
            else
            {
                // TODO: Check if range selection is enabled and handle it here.

                tv_deselect_all_items(pTVElement);
                tvi_select(pTV->pHoveredItem);
            }
        }
    }
}

void tv_on_mouse_button_up(easygui_element* pTVElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)mouseButton;
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    // TODO: Implement me.
}

void tv_on_mouse_button_dblclick(easygui_element* pTVElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    if (mouseButton == EASYGUI_MOUSE_BUTTON_LEFT)
    {
        if (!pTV->isMouseOverArrow)
        {
            if (tvi_has_children(pTV->pHoveredItem))
            {
                // It is a parent item, so toggle it.
                if (tvi_is_expanded(pTV->pHoveredItem)) {
                    tvi_collapse(pTV->pHoveredItem);
                } else {
                    tvi_expand(pTV->pHoveredItem);
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

void tv_on_mouse_wheel(easygui_element* pTVElement, int delta, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    sb_scroll(pTV->pScrollbarV, -delta * sb_get_mouse_wheel_scale(pTV->pScrollbarV));
}

void tv_on_paint(easygui_element* pTVElement, easygui_rect relativeClippingRect, void* pPaintData)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    // The dead space between the scrollbars should always be drawn with the default background color.
    easygui_draw_rect(pTVElement, tv_get_scrollbar_dead_space_rect(pTVElement), pTV->defaultBGColor, pPaintData);

    // The clipping rectangle needs to be clamped to the local rectangle that is shrunk such that it does not
    // include the scrollbars. If we don't do this we'll end up drawing underneath the scrollbars which will
    // cause flickering.
    easygui_rect innerClippingRect = easygui_clamp_rect(tv_get_inner_rect(pTVElement), relativeClippingRect);
    easygui_set_clip(pTVElement, innerClippingRect, pPaintData);


    // The main content of the tree-view is drawn in two parts. The first part (the top part) contains all of
    // the tree-view items. The second part (the bottom part) is just the background region that is not covered
    // by items.

    // We draw the tree-view items (the top part) first. This will retrieve the position of the bottom of the
    // items which is used to determine how much empty space is remaining below it so we can draw a quad over
    // that part.
    float itemsBottom = 0;
    tv_paint_items(pTVElement, innerClippingRect, pPaintData, &itemsBottom);


    // At this point the items have been drawn. All that remains is the part of the background that is not
    // covered by items. We can determine this by looking at <itemsBottom>.
    if (itemsBottom < relativeClippingRect.bottom && itemsBottom < easygui_get_relative_position_y(pTV->pScrollbarH))
    {
        easygui_draw_rect(pTVElement, easygui_make_rect(0, itemsBottom, easygui_get_relative_position_x(pTV->pScrollbarV), easygui_get_relative_position_y(pTV->pScrollbarH)), pTV->defaultBGColor, pPaintData);
    }
}


PRIVATE void tv_refresh_and_redraw(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }


    // Refresh scrollbar ranges and page sizes.
    tv_refresh_scrollbar_ranges(pTVElement);

    // For now, just redraw the entire control.
    easygui_dirty(pTVElement, easygui_get_local_rect(pTVElement));
}

PRIVATE void tv_refresh_scrollbar_layouts(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }


    // Vertical scrollbar.
    easygui_set_size(pTV->pScrollbarV, 16, easygui_get_height(pTVElement) - 16);
    easygui_set_relative_position(pTV->pScrollbarV, easygui_get_width(pTVElement) - easygui_get_width(pTV->pScrollbarV), 0);
    
    // Horizontal scrollbar.
    easygui_set_size(pTV->pScrollbarH, easygui_get_width(pTVElement) - 16, 16);
    easygui_set_relative_position(pTV->pScrollbarH, 0, easygui_get_height(pTVElement) - easygui_get_height(pTV->pScrollbarH));
}

PRIVATE void tv_refresh_scrollbar_ranges(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    float innerWidth = 0;
    unsigned int totalItemCount = 0;
    unsigned int pageItemCount = 0;

    eg_tree_view_iterator i;
    if (tv_begin_at(pTV->pRootItem->pFirstChild, &i))
    {
        do
        {
            float itemRight = i.posX + i.width;
            if (itemRight > innerWidth) {
                innerWidth = itemRight;
            }

            float itemBottom = i.posY + i.height;
            if (itemBottom > 0 && itemBottom < easygui_get_relative_position_y(pTV->pScrollbarH)) {
                pageItemCount += 1;
            }

            totalItemCount += 1;

        } while (tv_next_visible(&i));
    }

    if (totalItemCount == 0)
    {
        // Vertical.
        sb_set_range(pTV->pScrollbarV, 0, 0);
        sb_set_page_size(pTV->pScrollbarV, 0);

        // Horizontal.
        sb_set_range(pTV->pScrollbarH, 0, 0);
        sb_set_page_size(pTV->pScrollbarH, 0);
    }
    else
    {
        // Vertical.
        sb_set_range(pTV->pScrollbarV, 0, (int)totalItemCount - 1); // - 1 because it's a 0-based range.
        sb_set_page_size(pTV->pScrollbarV, pageItemCount);

        // Horizontal.
        sb_set_range(pTV->pScrollbarH, 0, (int)innerWidth);
        sb_set_page_size(pTV->pScrollbarH, (int)easygui_get_relative_position_x(pTV->pScrollbarV));
    }
}

PRIVATE easygui_rect tv_get_scrollbar_dead_space_rect(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return easygui_make_rect(0, 0, 0, 0);
    }

    return easygui_make_rect(easygui_get_width(pTV->pScrollbarH), easygui_get_height(pTV->pScrollbarV), easygui_get_width(pTVElement), easygui_get_height(pTVElement));
}

PRIVATE easygui_rect tv_get_inner_rect(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return easygui_make_rect(0, 0, 0, 0);
    }

    easygui_rect result = easygui_get_local_rect(pTVElement);
    result.right  -= easygui_get_width(pTV->pScrollbarV);
    result.bottom -= easygui_get_height(pTV->pScrollbarH);

    return result;
}

PRIVATE void tv_paint_items(easygui_element* pTVElement, easygui_rect relativeClippingRect, void* pPaintData, float* pItemsBottomOut)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    float itemsBottom = 0;

    // For now we will begin at the root item, but later we want to begin at the first visible item which will be based on the
    // scroll position.
    eg_tree_view_iterator i;
    if (tv_begin_at(tv_find_first_visible_item_on_page(pTVElement), &i))
    {
        do
        {
            tv_paint_item(pTVElement, i.pItem, relativeClippingRect, i.posX, i.posY, i.width, i.height, pPaintData);
            
            // Restore the clipping rectangle in case the application changed the clipping rectangle.
            easygui_set_clip(pTVElement, relativeClippingRect, pPaintData);

            itemsBottom = i.posY + i.height;

        } while (itemsBottom < relativeClippingRect.bottom && tv_next_visible(&i));
    }


    if (pItemsBottomOut != NULL) {
        *pItemsBottomOut = itemsBottom;
    }
}

PRIVATE bool tv_begin_at(eg_tree_view_item* pFirst, eg_tree_view_iterator* pIteratorOut)
{
    if (pFirst == NULL || pIteratorOut == NULL) {
        return false;
    }

    if (!tv_measure_item(pFirst->pTVElement, pFirst, &pIteratorOut->width, &pIteratorOut->height)) {
        return false;
    }

    const int depth = tvi_get_depth(pFirst);

    pIteratorOut->pItem = pFirst;
    pIteratorOut->depth = depth;
    pIteratorOut->posX  = depth * tv_get_child_offset_x(pFirst->pTVElement);
    pIteratorOut->posY  = 0;
    
    return true;
}

PRIVATE bool tv_next_visible(eg_tree_view_iterator* pIterator)
{
    assert(pIterator != NULL);

    if (pIterator->pItem == NULL) {
        return false;
    }

    eg_tree_view* pTV = easygui_get_extra_data(pIterator->pItem->pTVElement);
    if (pTV == NULL) {
        return false;
    }

    if (tvi_has_children(pIterator->pItem) && tvi_is_expanded(pIterator->pItem))
    {
        pIterator->pItem = pIterator->pItem->pFirstChild;
        pIterator->depth += 1;
    }
    else
    {
        pIterator->pItem = tvi_next_visible_non_child(pIterator->pItem, &pIterator->depth);
    }


    if (pIterator->pItem == NULL) {
        return false;
    }

    pIterator->posX  = pIterator->depth * tv_get_child_offset_x(pIterator->pItem->pTVElement);
    pIterator->posY += pIterator->height;

    if (!tv_measure_item(pIterator->pItem->pTVElement, pIterator->pItem, &pIterator->width, &pIterator->height)) {
        return false;
    }

    return true;
}

PRIVATE void tv_paint_item(easygui_element* pTVElement, eg_tree_view_item* pItem, easygui_rect relativeClippingRect, float posX, float posY, float width, float height, void* pPaintData)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return;
    }

    if (pTV->onItemPaint)
    {
        // We draw an item in two main parts, with the first part being the background section to the left and right of the item and the
        // second part being the item itself. The first part we do ourselves, whereas the second part we pass off to the host application.
        
        // The background section to the left and right of the main content is done first, by us.
        easygui_color bgcolor;
        if (tvi_is_selected(pItem)) {
            bgcolor = pTV->selectedBGColor;
        } else if (pTV->pHoveredItem == pItem) {
            bgcolor = pTV->hoveredBGColor;
        } else {
            bgcolor = pTV->defaultBGColor;
        }

        float innerOffsetX = (float)-sb_get_scroll_position(pTV->pScrollbarH);

        // Left.
        if (posX + innerOffsetX > 0) {
            easygui_draw_rect(pTVElement, easygui_make_rect(0, posY, posX + innerOffsetX, posY + height), bgcolor, pPaintData);
        }
        
        // Right.
        if (posX + width + innerOffsetX < easygui_get_relative_position_x(pTV->pScrollbarV)) {
            easygui_draw_rect(pTVElement, easygui_make_rect(posX + width + innerOffsetX, posY, easygui_get_relative_position_x(pTV->pScrollbarV), posY + height), bgcolor, pPaintData);
        }


        // At this point if were to finish drawing we'd have a hole where the main content of the item should be. To fill this we need to
        // let the host application do it.
        pTV->onItemPaint(pTVElement, pItem, relativeClippingRect, bgcolor, posX + innerOffsetX, posY, width, height, pPaintData);
    }
}

PRIVATE eg_tree_view_item* tv_find_item_under_point(easygui_element* pTVElement, float relativePosX, float relativePosY, eg_tree_view_item_metrics* pMetricsOut)
{
    (void)relativePosX; // <-- Unused because we treat items as though they are infinitely wide.

    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    // For now we will begin at the root item, but later we want to begin at the first visible item which will be based on the
    // scroll position.
    eg_tree_view_iterator i;
    if (tv_begin_at(tv_find_first_visible_item_on_page(pTVElement), &i))
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

        } while ((i.posY + i.height < easygui_get_relative_position_y(pTV->pScrollbarH)) && tv_next_visible(&i));
    }

    return NULL;
}

PRIVATE void tv_deselect_all_items_recursive(eg_tree_view_item* pItem)
{
    pItem->isSelected = false;

    for (eg_tree_view_item* pChild = pItem->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling)
    {
        tv_deselect_all_items_recursive(pChild);
    }
}

PRIVATE void tv_on_mouse_enter_scrollbar(easygui_element* pSBElement)
{
    eg_tree_view_scrollbar_data* pSB = sb_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    // We just pretend the mouse has left the tree-view entirely. This will ensure any item marked as hovered is unmarked and redrawn.
    tv_on_mouse_leave(pSB->pTVElement);
}

PRIVATE void tv_on_scroll_v(easygui_element* pSBElement, int scrollPos)
{
    (void)scrollPos;

    eg_tree_view_scrollbar_data* pSB = sb_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    eg_tree_view* pTV = easygui_get_extra_data(pSB->pTVElement);
    if (pTV == NULL) {
        return;
    }

    // As we scroll, the mouse will be placed over a different item. We just post a manual mouse_move event to trigger a refresh.
    if (pTV->isMouseOver) {
        tv_on_mouse_move(pSB->pTVElement, pTV->relativeMousePosX, pTV->relativeMousePosY, 0);
    }

    // The paint routine is tied directly to the scrollbars, so all we need to do is mark it as dirty to trigger a redraw.
    easygui_dirty(pSB->pTVElement, easygui_get_local_rect(pSB->pTVElement));
}

PRIVATE void tv_on_scroll_h(easygui_element* pSBElement, int scrollPos)
{
    (void)scrollPos;

    eg_tree_view_scrollbar_data* pSB = sb_get_extra_data(pSBElement);
    if (pSB == NULL) {
        return;
    }

    eg_tree_view* pTV = easygui_get_extra_data(pSB->pTVElement);
    if (pTV == NULL) {
        return;
    }

    // The paint routine is tied directly to the scrollbars, so all we need to do is mark it as dirty to trigger a redraw.
    easygui_dirty(pSB->pTVElement, easygui_get_local_rect(pSB->pTVElement));
}

PRIVATE eg_tree_view_item* tv_find_first_visible_item_on_page(easygui_element* pTVElement)
{
    eg_tree_view* pTV = easygui_get_extra_data(pTVElement);
    if (pTV == NULL) {
        return NULL;
    }

    // We just keep iterating until we hit the index of the scroll position.
    int index = 0;

    eg_tree_view_iterator i;
    if (tv_begin_at(pTV->pRootItem->pFirstChild, &i))
    {
        do
        {
            if (index == sb_get_scroll_position(pTV->pScrollbarV)) {
                return i.pItem;
            }

            index += 1;

        } while (tv_next_visible(&i));
    }

    return NULL;
}



///////////////////////////////////////////////////////////////////////////////
//
// Tree-View Item
//
///////////////////////////////////////////////////////////////////////////////

/// Detaches the given tree-view item from it's parent and siblings.
PRIVATE void tvi_detach(eg_tree_view_item* pItem);

eg_tree_view_item* tv_create_item(easygui_element* pTVElement, eg_tree_view_item* pParent, size_t extraDataSize, const void* pExtraData)
{
    if (pTVElement == NULL) {
        return NULL;
    }

    if (pParent != NULL && pParent->pTVElement != pTVElement) {
        return NULL;
    }


    eg_tree_view_item* pItem = malloc(sizeof(*pItem) + extraDataSize - sizeof(pItem->pExtraData));
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
    tvi_append(pItem, pParent);

    return pItem;
}

void tvi_delete(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }


    // Children need to be deleted first.
    while (pItem->pFirstChild != NULL) {
        tvi_delete(pItem->pFirstChild);
    }

    // We need to grab a pointer to the main tree-view control so we can refresh and redraw it after we have detached the item.
    easygui_element* pTVElement = pItem->pTVElement;

    // The item needs to be completely detached first.
    tvi_detach(pItem);

    // Refresh the layout and redraw the tree-view control.
    tv_refresh_and_redraw(pTVElement);

    // Free the item last for safety.
    free(pItem);
}

easygui_element* tvi_get_tree_view_element(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pTVElement;
}

size_t tvi_get_extra_data_size(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return 0;
    }

    return pItem->extraDataSize;
}

void* tvi_get_extra_data(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pExtraData;
}


eg_tree_view_item* tvi_get_parent(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pParent;
}

eg_tree_view_item* tvi_get_first_child(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pFirstChild;
}

eg_tree_view_item* tvi_get_last_child(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pLastChild;
}

eg_tree_view_item* tvi_get_next_sibling(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pNextSibling;
}

eg_tree_view_item* tvi_get_prev_sibling(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return NULL;
    }

    return pItem->pPrevSibling;
}

void tvi_append(eg_tree_view_item* pItem, eg_tree_view_item* pParent)
{
    if (pItem == NULL) {
        return;
    }

    eg_tree_view* pTV = easygui_get_extra_data(pItem->pTVElement);
    if (pTV == NULL) {
        return;
    }


    // If a parent was not specified, append to the root item.
    if (pParent == NULL)
    {
        if (pTV->pRootItem != NULL) {
            tvi_append(pItem, pTV->pRootItem);
        }
    }
    else
    {
        assert(pItem->pTVElement == pParent->pTVElement);

        // Detach the child from it's current parent first.
        tvi_detach(pItem);

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
        tv_refresh_and_redraw(pItem->pTVElement);
    }
}

void tvi_prepend(eg_tree_view_item* pItem, eg_tree_view_item* pParent)
{
    if (pItem == NULL) {
        return;
    }

    eg_tree_view* pTV = easygui_get_extra_data(pItem->pTVElement);
    if (pTV == NULL) {
        return;
    }


    // If a parent was not specified, prepend to the root item.
    if (pParent == NULL)
    {
        if (pTV->pRootItem != NULL) {   
            tvi_prepend(pItem, pTV->pRootItem);
        }
    }
    else
    {
        assert(pItem->pTVElement == pParent->pTVElement);

        // Detach the child from it's current parent first.
        tvi_detach(pItem);

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
        tv_refresh_and_redraw(pItem->pTVElement);
    }
}

void tvi_append_sibling(eg_tree_view_item* pItemToAppend, eg_tree_view_item* pItemToAppendTo)
{
    if (pItemToAppend == NULL) {
        return;
    }

    eg_tree_view* pTV = easygui_get_extra_data(pItemToAppend->pTVElement);
    if (pTV == NULL) {
        return;
    }


    // If a parent was not specified, append to the root item.
    if (pItemToAppendTo == NULL)
    {
        if (pTV->pRootItem != NULL) {
            tvi_append(pItemToAppend, pTV->pRootItem);
        }
    }
    else
    {
        assert(pItemToAppend->pTVElement == pItemToAppendTo->pTVElement);

        // Detach the child from it's current parent first.
        tvi_detach(pItemToAppend);


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
        tv_refresh_and_redraw(pItemToAppend->pTVElement);
    }
}

void tvi_prepend_sibling(eg_tree_view_item* pItemToPrepend, eg_tree_view_item* pItemToPrependTo)
{
    if (pItemToPrepend == NULL) {
        return;
    }

    eg_tree_view* pTV = easygui_get_extra_data(pItemToPrepend->pTVElement);
    if (pTV == NULL) {
        return;
    }


    // If a parent was not specified, prepend to the root item.
    if (pItemToPrependTo == NULL)
    {
        if (pTV->pRootItem != NULL) {
            tvi_prepend(pItemToPrepend, pTV->pRootItem);
        }
    }
    else
    {
        assert(pItemToPrepend->pTVElement == pItemToPrependTo->pTVElement);

        // Detach the child from it's current parent first.
        tvi_detach(pItemToPrepend);


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
        tv_refresh_and_redraw(pItemToPrepend->pTVElement);
    }
}


bool tvi_has_children(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return false;
    }

    return pItem->pFirstChild != NULL;
}

int tvi_get_depth(eg_tree_view_item* pItem)
{
    if (pItem->pParent == NULL || pItem->pParent == tv_get_root_item(pItem->pTVElement)) {
        return 0;
    }

    return tvi_get_depth(pItem->pParent) + 1;
}

eg_tree_view_item* tvi_next_visible_non_child(eg_tree_view_item* pItem, int* pDepthInOut)
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
    
    return tvi_next_visible_non_child(pItem->pParent, pDepthInOut);
}


void tvi_select(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }

    if (!pItem->isSelected)
    {
        pItem->isSelected = true;
        easygui_dirty(pItem->pTVElement, easygui_get_local_rect(pItem->pTVElement));
    }
}

void tvi_deselect(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }

    if (pItem->isSelected)
    {
        pItem->isSelected = false;
        easygui_dirty(pItem->pTVElement, easygui_get_local_rect(pItem->pTVElement));
    }
}

bool tvi_is_selected(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return false;
    }

    return pItem->isSelected;
}

void tvi_expand(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }

    if (!pItem->isExpanded)
    {
        pItem->isExpanded = true;
        tv_refresh_and_redraw(pItem->pTVElement);
    }
}

void tvi_collapse(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return;
    }

    if (pItem->isExpanded)
    {
        pItem->isExpanded = false;
        tv_refresh_and_redraw(pItem->pTVElement);
    }
}

bool tvi_is_expanded(eg_tree_view_item* pItem)
{
    if (pItem == NULL) {
        return false;
    }

    return pItem->isExpanded;
}



PRIVATE void tvi_detach(eg_tree_view_item* pItem)
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
