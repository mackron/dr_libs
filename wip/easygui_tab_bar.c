// Public domain. See "unlicense" statement at the end of this file.

#include "easygui_tab_bar.h"
#include <assert.h>

typedef struct easygui_tab_bar easygui_tab_bar;

struct easygui_tab_bar
{
    /// The orientation.
    tabbar_orientation orientation;


    /// A pointer to the first tab.
    easygui_tab* pFirstTab;

    /// A pointer to the last tab.
    easygui_tab* pLastTab;


    /// A pointer to the hovered tab.
    easygui_tab* pHoveredTab;

    /// A pointer to the active tab.
    easygui_tab* pActiveTab;


    /// The default font to use for tab bar items.
    easygui_font* pFont;

    /// The default color to use for tab bar item text.
    easygui_color tabTextColor;

    /// The default background color of tab bar items.
    easygui_color tabBackgroundColor;

    /// The background color of tab bar items while hovered.
    easygui_color tabBackgroundColorHovered;

    /// The background color of tab bar items while selected.
    easygui_color tabBackbroundColorActivated;

    /// The padding to apply to the text of tabs.
    float tabPadding;

    /// Whether or not auto-sizing is enabled. Disabled by default.
    bool isAutoSizeEnabled;


    /// The function to call when a tab needs to be measured.
    tabbar_on_measure_tab_proc onMeasureTab;

    /// The function to call when a tab needs to be painted.
    tabbar_on_paint_tab_proc onPaintTab;

    /// The function to call when a tab is activated.
    tabbar_on_tab_activated_proc onTabActivated;

    /// The function to call when a tab is deactivated.
    tabbar_on_tab_deactivated_proc onTabDeactivated;


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data.
    char pExtraData[1];
};

struct easygui_tab
{
    /// The tab bar that owns the tab.
    easygui_element* pTBElement;

    /// A pointer to the next tab in the tab bar.
    easygui_tab* pNextTab;

    /// A pointer to the previous tab in the tab bar.
    easygui_tab* pPrevTab;


    /// The tab bar's text.
    char text[EASYGUI_MAX_TAB_TEXT_LENGTH];


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data buffer.
    char pExtraData[1];
};


///////////////////////////////////////////////////////////////////////////////
//
// Tab
//
///////////////////////////////////////////////////////////////////////////////

/// Default implementation of the item measure event.
PRIVATE void tabbar_on_measure_tab_default(easygui_element* pTBElement, easygui_tab* pTab, float* pWidthOut, float* pHeightOut);

/// Paints the given menu item.
PRIVATE void tabbar_on_paint_tab_default(easygui_element* pTBElement, easygui_tab* pTab, easygui_rect relativeClippingRect, float offsetX, float offsetY, float width, float height, void* pPaintData);

/// Finds the tab sitting under the given point, if any.
PRIVATE easygui_tab* tabbar_find_tab_under_point(easygui_element* pTBElement, float relativePosX, float relativePosY);

easygui_element* easygui_create_tab_bar(easygui_context* pContext, easygui_element* pParent, tabbar_orientation orientation, size_t extraDataSize, const void* pExtraData)
{
    if (pContext == NULL) {
        return NULL;
    }

    easygui_element* pTBElement = easygui_create_element(pContext, pParent, sizeof(easygui_tab_bar) - sizeof(char) + extraDataSize);
    if (pTBElement == NULL) {
        return NULL;
    }

    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    pTB->orientation = orientation;
    pTB->pFirstTab   = NULL;
    pTB->pLastTab    = NULL;
    pTB->pHoveredTab = NULL;
    pTB->pActiveTab  = NULL;

    pTB->pFont                       = NULL;
    pTB->tabTextColor                = easygui_rgb(224, 224, 224);
    pTB->tabBackgroundColor          = easygui_rgb(52, 52, 52);
    pTB->tabBackgroundColorHovered   = easygui_rgb(0, 128, 255);
    pTB->tabBackbroundColorActivated = easygui_rgb(80, 80, 80);
    pTB->tabPadding                  = 4;
    pTB->isAutoSizeEnabled           = false;

    pTB->onMeasureTab                = tabbar_on_measure_tab_default;
    pTB->onPaintTab                  = tabbar_on_paint_tab_default;
    pTB->onTabActivated              = NULL;
    pTB->onTabDeactivated            = NULL;


    pTB->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pTB->pExtraData, pExtraData, extraDataSize);
    }


    // Event handlers.
    easygui_set_on_mouse_leave(pTBElement, tabbar_on_mouse_leave);
    easygui_set_on_mouse_move(pTBElement, tabbar_on_mouse_move);
    easygui_set_on_mouse_button_down(pTBElement, tabbar_on_mouse_button_down);
    easygui_set_on_paint(pTBElement, tabbar_on_paint);

    return pTBElement;
}

void easygui_delete_tab_bar(easygui_element* pTBElement)
{
    if (pTBElement == NULL) {
        return;
    }

    easygui_delete_element(pTBElement);
}


size_t tabbar_get_extra_data_size(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return pTB->extraDataSize;
}

void* tabbar_get_extra_data(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    return pTB->pExtraData;
}

tabbar_orientation tabbar_get_orientation(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return tabbar_orientation_top;
    }

    return pTB->orientation;
}


void tabbar_set_font(easygui_element* pTBElement, easygui_font* pFont)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->pFont = pFont;

    if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
        easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
    }
}

easygui_font* tabbar_get_font(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    return pTB->pFont;
}


void tabbar_set_on_measure_tab(easygui_element* pTBElement, tabbar_on_measure_tab_proc proc)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->onMeasureTab = proc;
}

void tabbar_set_on_paint_tab(easygui_element* pTBElement, tabbar_on_paint_tab_proc proc)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->onPaintTab = proc;
}

void tabbar_set_on_tab_activated(easygui_element* pTBElement, tabbar_on_tab_activated_proc proc)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->onTabActivated = proc;
}

void tabbar_set_on_tab_deactivated(easygui_element* pTBElement, tabbar_on_tab_deactivated_proc proc)
{
     easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->onTabDeactivated = proc;
}


void tabbar_measure_tab(easygui_element* pTBElement, easygui_tab* pTab, float* pWidthOut, float* pHeightOut)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (pTB->onMeasureTab) {
        pTB->onMeasureTab(pTBElement, pTab, pWidthOut, pHeightOut);
    }
}

void tabbar_paint_tab(easygui_element* pTBElement, easygui_tab* pTab, easygui_rect relativeClippingRect, float offsetX, float offsetY, float width, float height, void* pPaintData)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (pTB->onPaintTab) {
        pTB->onPaintTab(pTBElement, pTab, relativeClippingRect, offsetX, offsetY, width, height, pPaintData);
    }
}


void tabbar_resize_by_tabs(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (pTB->onMeasureTab == NULL) {
        return;
    }

    float maxWidth  = 0;
    float maxHeight = 0;
    for (easygui_tab* pTab = pTB->pFirstTab; pTab != NULL; pTab = pTab->pNextTab)
    {
        float tabWidth  = 0;
        float tabHeight = 0;
        tabbar_measure_tab(pTBElement, pTab, &tabWidth, &tabHeight);

        maxWidth  = (tabWidth  > maxWidth)  ? tabWidth  : maxWidth;
        maxHeight = (tabHeight > maxHeight) ? tabHeight : maxHeight;
    }


    if (pTB->orientation == tabbar_orientation_top || pTB->orientation == tabbar_orientation_bottom) {
        easygui_set_size(pTBElement, easygui_get_width(pTBElement), maxHeight);
    } else {
        easygui_set_size(pTBElement, maxWidth, easygui_get_height(pTBElement));
    }
}

void tabbar_enable_auto_size(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->isAutoSizeEnabled = true;
}

void tabbar_disable_auto_size(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->isAutoSizeEnabled = false;
}

bool tabbar_is_auto_size_enabled(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return pTB->isAutoSizeEnabled;
}


void tabbar_activate_tab(easygui_element* pTBElement, easygui_tab* pTab)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_tab* pOldActiveTab = pTB->pActiveTab;
    easygui_tab* pNewActiveTab = pTab;

    if (pOldActiveTab == pNewActiveTab) {
        return;     // The tab is already active - nothing to do.
    }


    pTB->pActiveTab = pNewActiveTab;

    if (pTB->onTabDeactivated && pOldActiveTab != NULL) {
        pTB->onTabDeactivated(pTBElement, pOldActiveTab);
    }

    if (pTB->onTabActivated && pNewActiveTab != NULL) {
        pTB->onTabActivated(pTBElement, pNewActiveTab);
    }


    if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
        easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
    }
}


void tabbar_on_mouse_leave(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (pTB->pHoveredTab != NULL)
    {
        pTB->pHoveredTab = NULL;

        if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
            easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
        }
    }
}

void tabbar_on_mouse_move(easygui_element* pTBElement, int relativeMousePosX, int relativeMousePosY)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_tab* pOldHoveredTab = pTB->pHoveredTab;
    easygui_tab* pNewHoveredTab = tabbar_find_tab_under_point(pTBElement, (float)relativeMousePosX, (float)relativeMousePosY);

    if (pOldHoveredTab != pNewHoveredTab)
    {
        pTB->pHoveredTab = pNewHoveredTab;

        if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
            easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
        }
    }
}

void tabbar_on_mouse_button_down(easygui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (mouseButton == EASYGUI_MOUSE_BUTTON_LEFT)
    {
        easygui_tab* pOldActiveTab = pTB->pActiveTab;
        easygui_tab* pNewActiveTab = tabbar_find_tab_under_point(pTBElement, (float)relativeMousePosX, (float)relativeMousePosY);

        if (pNewActiveTab != NULL && pOldActiveTab != pNewActiveTab) {
            tabbar_activate_tab(pTBElement, pNewActiveTab);
        }
    }
}

void tabbar_on_paint(easygui_element* pTBElement, easygui_rect relativeClippingRect, void* pPaintData)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }


    float tabbarWidth  = 0;
    float tabbarHeight = 0;
    easygui_get_size(pTBElement, &tabbarWidth, &tabbarHeight);


    // Each tab.
    float runningPosX = 0;
    float runningPosY = 0;
    for (easygui_tab* pTab = pTB->pFirstTab; pTab != NULL; pTab = pTab->pNextTab)
    {
        float tabWidth  = 0;
        float tabHeight = 0;
        tabbar_measure_tab(pTBElement, pTab, &tabWidth, &tabHeight);

        tabbar_paint_tab(pTBElement, pTab, relativeClippingRect, runningPosX, runningPosY, tabWidth, tabHeight, pPaintData);

        // After painting the tab, there may be a region of the background that was not drawn by the tab painting callback. We'll need to
        // draw that here.
        if (pTB->orientation == tabbar_orientation_top || pTB->orientation == tabbar_orientation_bottom) {
            easygui_draw_rect(pTBElement, easygui_make_rect(runningPosX, runningPosY + tabHeight, runningPosX + tabWidth, tabbarHeight), pTB->tabBackgroundColor, pPaintData);
        } else {
            easygui_draw_rect(pTBElement, easygui_make_rect(runningPosX + tabWidth, runningPosY, tabbarWidth, runningPosY + tabHeight), pTB->tabBackgroundColor, pPaintData);
        }



        if (pTB->orientation == tabbar_orientation_top || pTB->orientation == tabbar_orientation_bottom) {
            runningPosX += tabWidth;
        } else {
            runningPosY += tabHeight;
        }
    }


    // Background. We just draw a quad around the region that is not covered by items.
    easygui_draw_rect(pTBElement, easygui_make_rect(runningPosX, runningPosY, tabbarWidth, tabbarHeight), pTB->tabBackgroundColor, pPaintData);
}


PRIVATE void tabbar_on_measure_tab_default(easygui_element* pTBElement, easygui_tab* pTab, float* pWidthOut, float* pHeightOut)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    float textWidth  = 0;
    float textHeight = 0;

    if (pTab != NULL) {
        easygui_measure_string_by_element(pTB->pFont, pTab->text, strlen(pTab->text), pTBElement, &textWidth, &textHeight);
    }

 
    if (pWidthOut) {
        *pWidthOut = textWidth + pTB->tabPadding*2;
    }
    if (pHeightOut) {
        *pHeightOut = textHeight + pTB->tabPadding*2;
    }
}

PRIVATE void tabbar_on_paint_tab_default(easygui_element* pTBElement, easygui_tab* pTab, easygui_rect relativeClippingRect, float offsetX, float offsetY, float width, float height, void* pPaintData)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    // Background.
    easygui_color bgcolor = pTB->tabBackgroundColor;
    if (pTB->pHoveredTab == pTab) {
        bgcolor = pTB->tabBackgroundColorHovered;
    }
    if (pTB->pActiveTab == pTab) {
        bgcolor = pTB->tabBackbroundColorActivated;
    }

    easygui_draw_rect_outline(pTBElement, easygui_make_rect(offsetX, offsetY, offsetX + width, offsetY + height), bgcolor, pTB->tabPadding, pPaintData);


    // Text.
    if (pTab != NULL)
    {
        float textPosX = offsetX + pTB->tabPadding;
        float textPosY = offsetY + pTB->tabPadding;
        easygui_draw_text(pTBElement, pTB->pFont, pTab->text, strlen(pTab->text), textPosX, textPosY, pTB->tabTextColor, bgcolor, pPaintData);
    }
}

PRIVATE easygui_tab* tabbar_find_tab_under_point(easygui_element* pTBElement, float relativePosX, float relativePosY)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    float runningPosX = 0;
    float runningPosY = 0;
    for (easygui_tab* pTab = pTB->pFirstTab; pTab != NULL; pTab = pTab->pNextTab)
    {
        float tabWidth  = 0;
        float tabHeight = 0;
        tabbar_measure_tab(pTBElement, pTab, &tabWidth, &tabHeight);

        if (relativePosX >= runningPosX && relativePosX < runningPosX + tabWidth && relativePosY >= runningPosY && relativePosY < runningPosY + tabHeight) {
            return pTab;
        }

        if (pTB->orientation == tabbar_orientation_top || pTB->orientation == tabbar_orientation_bottom) {
            runningPosX += tabWidth;
        } else {
            runningPosY += tabHeight;
        }
    }

    return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//
// Tab
//
///////////////////////////////////////////////////////////////////////////////

/// Appends the given tab to the given tab bar.
PRIVATE void tab_append(easygui_tab* pTab, easygui_element* pTBElement);

/// Prepends the given tab to the given tab bar.
PRIVATE void tab_prepend(easygui_tab* pTab, easygui_element* pTBElement);

/// Detaches the given tab bar from it's tab bar element.
PRIVATE void tab_detach(easygui_tab* pTab);

PRIVATE easygui_tab* tb_create_tab(easygui_element* pTBElement, const char* text, size_t extraDataSize, const void* pExtraData)
{
    if (pTBElement == NULL) {
        return NULL;
    }

    easygui_tab* pTab = malloc(sizeof(*pTab) + extraDataSize - sizeof(pTab->pExtraData));
    if (pTab == NULL) {
        return NULL;
    }

    pTab->pTBElement = NULL;
    pTab->pNextTab   = NULL;
    pTab->pPrevTab   = NULL;
    pTab->text[0]    = '\0';
    
    pTab->extraDataSize = extraDataSize;
    if (pExtraData) {
        memcpy(pTab->pExtraData, pExtraData, extraDataSize);
    }

    if (text != NULL) {
        strncpy_s(pTab->text, sizeof(pTab->text), text, _TRUNCATE);
    }

    return pTab;
}

easygui_tab* tabbar_create_and_append_tab(easygui_element* pTBElement, const char* text, size_t extraDataSize, const void* pExtraData)
{
    easygui_tab* pTab = tb_create_tab(pTBElement, text, extraDataSize, pExtraData);
    if (pTab != NULL)
    {
        tab_append(pTab, pTBElement);
    }

    return pTab;
}

easygui_tab* tabbar_create_and_prepend_tab(easygui_element* pTBElement, const char* text, size_t extraDataSize, const void* pExtraData)
{
    easygui_tab* pTab = tb_create_tab(pTBElement, text, extraDataSize, pExtraData);
    if (pTab != NULL)
    {
        tab_prepend(pTab, pTBElement);
    }

    return pTab;
}

void tab_delete(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return;
    }

    tab_detach(pTab);
    free(pTab);
}

easygui_element* tab_get_tab_bar_element(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return NULL;
    }

    return pTab->pTBElement;
}

size_t tab_get_extra_data_size(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return 0;
    }

    return pTab->extraDataSize;
}

void* tab_get_extra_data(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return NULL;
    }

    return pTab->pExtraData;
}


void tab_set_text(easygui_tab* pTab, const char* text)
{
    if (pTab == NULL) {
        return;
    }

    if (text != NULL) {
        strncpy_s(pTab->text, sizeof(pTab->text), text, _TRUNCATE);
    } else {
        pTab->text[0] = '\0';
    }

    // The content of the menu has changed so we'll need to schedule a redraw.
    if (easygui_is_auto_dirty_enabled(pTab->pTBElement->pContext)) {
        easygui_dirty(pTab->pTBElement, easygui_get_local_rect(pTab->pTBElement));
    }
}

const char* tab_get_text(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return NULL;
    }

    return pTab->text;
}



PRIVATE void tab_append(easygui_tab* pTab, easygui_element* pTBElement)
{
    if (pTab == NULL || pTBElement == NULL) {
        return;
    }

    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    pTab->pTBElement = pTBElement;
    if (pTB->pFirstTab == NULL)
    {
        assert(pTB->pLastTab == NULL);

        pTB->pFirstTab = pTab;
        pTB->pLastTab  = pTab;
    }
    else
    {
        assert(pTB->pLastTab != NULL);

        pTab->pPrevTab = pTB->pLastTab;

        pTB->pLastTab->pNextTab = pTab;
        pTB->pLastTab = pTab;
    }


    if (pTB->isAutoSizeEnabled) {
        tabbar_resize_by_tabs(pTBElement);
    }

    // The content of the menu has changed so we'll need to schedule a redraw.
    if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
        easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
    }
}

PRIVATE void tab_prepend(easygui_tab* pTab, easygui_element* pTBElement)
{
    if (pTab == NULL || pTBElement == NULL) {
        return;
    }

    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    pTab->pTBElement = pTBElement;
    if (pTB->pFirstTab == NULL)
    {
        assert(pTB->pLastTab == NULL);

        pTB->pFirstTab = pTab;
        pTB->pLastTab  = pTab;
    }
    else
    {
        assert(pTB->pLastTab != NULL);

        pTab->pNextTab = pTB->pFirstTab;

        pTB->pFirstTab->pPrevTab = pTab;
        pTB->pFirstTab = pTab;
    }


    if (pTB->isAutoSizeEnabled) {
        tabbar_resize_by_tabs(pTBElement);
    }

    // The content of the menu has changed so we'll need to schedule a redraw.
    if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
        easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
    }
}

PRIVATE void tab_detach(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return;
    }

    easygui_tab_bar* pTB = easygui_get_extra_data(pTab->pTBElement);
    assert(pTB != NULL);


    if (pTab->pNextTab != NULL) {
        pTab->pNextTab->pPrevTab = pTab->pPrevTab;
    }

    if (pTab->pPrevTab != NULL) {
        pTab->pPrevTab->pNextTab = pTab->pNextTab;
    }


    if (pTab == pTB->pFirstTab) {
        pTB->pFirstTab = pTab->pNextTab;
    }

    if (pTab == pTB->pLastTab) {
        pTB->pLastTab = pTab->pPrevTab;
    }


    pTab->pNextTab   = NULL;
    pTab->pPrevTab   = NULL;
    pTab->pTBElement = NULL;


    if (pTB->isAutoSizeEnabled) {
        tabbar_resize_by_tabs(pTab->pTBElement);
    }
    
    // The content of the menu has changed so we'll need to schedule a redraw.
    if (easygui_is_auto_dirty_enabled(pTab->pTBElement->pContext)) {
        easygui_dirty(pTab->pTBElement, easygui_get_local_rect(pTab->pTBElement));
    }
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
