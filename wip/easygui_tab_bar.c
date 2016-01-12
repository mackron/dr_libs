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

    /// The tab whose close button is currently pressed, if any.
    easygui_tab* pTabWithCloseButtonPressed;


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

    /// The image to use for the close button.
    easygui_image* pCloseButtonImage;

    /// The width of the close button when drawn on the tab. This is independant of the actual image's width.
    float closeButtonWidth;

    /// The height of the close button when drawn on the tab. This is independant of the actual image's height.
    float closeButtonHeight;

    /// The padding to the left of the close button.
    float closeButtonPaddingLeft;

    /// The default color of the close button.
    easygui_color closeButtonColorDefault;

    /// The color of the close button when the tab is hovered, but not the close button itself.
    easygui_color closeButtonColorTabHovered;

    /// The color of the close button when it is hovered.
    easygui_color closeButtonColorHovered;

    /// The color of the close button when it is pressed.
    easygui_color closeButtonColorPressed;


    /// Whether or not auto-sizing is enabled. Disabled by default.
    bool isAutoSizeEnabled;

    /// Whether or not the close buttons are being shown.
    bool isShowingCloseButton;

    /// Whether or not close-on-middle-click is enabled.
    bool isCloseOnMiddleClickEnabled;

    /// Whether or not the close button is hovered.
    bool isCloseButtonHovered;


    /// The function to call when a tab needs to be measured.
    tabbar_on_measure_tab_proc onMeasureTab;

    /// The function to call when a tab needs to be painted.
    tabbar_on_paint_tab_proc onPaintTab;

    /// The function to call when a tab is activated.
    tabbar_on_tab_activated_proc onTabActivated;

    /// The function to call when a tab is deactivated.
    tabbar_on_tab_deactivated_proc onTabDeactivated;

    /// The function to call when a tab is closed via the close button.
    tabbar_on_tab_close_proc onTabClose;


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
PRIVATE easygui_tab* tabbar_find_tab_under_point(easygui_element* pTBElement, float relativePosX, float relativePosY, bool* pIsOverCloseButtonOut);

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

    pTB->orientation                 = orientation;
    pTB->pFirstTab                   = NULL;
    pTB->pLastTab                    = NULL;
    pTB->pHoveredTab                 = NULL;
    pTB->pActiveTab                  = NULL;
    pTB->pTabWithCloseButtonPressed  = NULL;

    pTB->pFont                       = NULL;
    pTB->tabTextColor                = easygui_rgb(224, 224, 224);
    pTB->tabBackgroundColor          = easygui_rgb(58, 58, 58);
    pTB->tabBackgroundColorHovered   = easygui_rgb(32, 128, 192);
    pTB->tabBackbroundColorActivated = easygui_rgb(80, 80, 80);
    pTB->tabPadding                  = 4;
    pTB->pCloseButtonImage           = NULL;
    pTB->closeButtonWidth            = 16;
    pTB->closeButtonHeight           = 16;
    pTB->closeButtonPaddingLeft      = 6;
    pTB->closeButtonColorDefault     = pTB->tabBackgroundColor;
    pTB->closeButtonColorTabHovered  = easygui_rgb(192, 192, 192);
    pTB->closeButtonColorHovered     = easygui_rgb(255, 96, 96);
    pTB->closeButtonColorPressed     = easygui_rgb(192, 32, 32);
    pTB->isAutoSizeEnabled           = false;
    pTB->isShowingCloseButton        = false;
    pTB->isCloseOnMiddleClickEnabled = false;
    pTB->isCloseButtonHovered        = false;

    pTB->onMeasureTab                = tabbar_on_measure_tab_default;
    pTB->onPaintTab                  = tabbar_on_paint_tab_default;
    pTB->onTabActivated              = NULL;
    pTB->onTabDeactivated            = NULL;
    pTB->onTabClose                  = NULL;


    pTB->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pTB->pExtraData, pExtraData, extraDataSize);
    }


    // Event handlers.
    easygui_set_on_mouse_leave(pTBElement, tabbar_on_mouse_leave);
    easygui_set_on_mouse_move(pTBElement, tabbar_on_mouse_move);
    easygui_set_on_mouse_button_down(pTBElement, tabbar_on_mouse_button_down);
    easygui_set_on_mouse_button_up(pTBElement, tabbar_on_mouse_button_up);
    easygui_set_on_paint(pTBElement, tabbar_on_paint);

    return pTBElement;
}

void easygui_delete_tab_bar(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    while (pTB->pFirstTab != NULL) {
        tab_delete(pTB->pFirstTab);
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


void tabbar_set_close_button_image(easygui_element* pTBElement, easygui_image* pImage)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->pCloseButtonImage = pImage;

    if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
        easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
    }
}

easygui_image* tabbar_get_close_button_image(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    return pTB->pCloseButtonImage;
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

void tabbar_set_on_tab_closed(easygui_element* pTBElement, tabbar_on_tab_close_proc proc)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->onTabClose = proc;
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

easygui_tab* tabbar_get_active_tab(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    return pTB->pActiveTab;
}


bool tabbar_is_tab_in_view(easygui_element* pTBElement, easygui_tab* pTabIn)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
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

        if (pTab == pTabIn) {
            return runningPosX + tabWidth <= tabbarWidth && runningPosY + tabHeight <= tabbarHeight;
        }


        if (pTB->orientation == tabbar_orientation_top || pTB->orientation == tabbar_orientation_bottom) {
            runningPosX += tabWidth;
        } else {
            runningPosY += tabHeight;
        }
    }

    return false;
}


void tabbar_show_close_buttons(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->isShowingCloseButton = true;

    if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
        easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
    }
}

void tabbar_hide_close_buttons(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->isShowingCloseButton = false;

    if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
        easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
    }
}


void tabbar_enable_close_on_middle_click(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->isCloseOnMiddleClickEnabled = true;
}

void tabbar_disable_close_on_middle_click(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->isCloseOnMiddleClickEnabled = false;
}

bool tabbar_is_close_on_middle_click_enabled(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return pTB->isCloseOnMiddleClickEnabled;
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
        pTB->isCloseButtonHovered = false;

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

    bool isCloseButtonHovered = false;

    easygui_tab* pOldHoveredTab = pTB->pHoveredTab;
    easygui_tab* pNewHoveredTab = tabbar_find_tab_under_point(pTBElement, (float)relativeMousePosX, (float)relativeMousePosY, &isCloseButtonHovered);

    if (pOldHoveredTab != pNewHoveredTab || pTB->isCloseButtonHovered != isCloseButtonHovered)
    {
        pTB->pHoveredTab = pNewHoveredTab;
        pTB->isCloseButtonHovered = isCloseButtonHovered;

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
        bool isOverCloseButton = false;

        easygui_tab* pOldActiveTab = pTB->pActiveTab;
        easygui_tab* pNewActiveTab = tabbar_find_tab_under_point(pTBElement, (float)relativeMousePosX, (float)relativeMousePosY, &isOverCloseButton);

        if (pNewActiveTab != NULL && pOldActiveTab != pNewActiveTab && !isOverCloseButton) {
            tabbar_activate_tab(pTBElement, pNewActiveTab);
        }

        if (isOverCloseButton)
        {   
            pTB->pTabWithCloseButtonPressed = pNewActiveTab;

            if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
                easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
            }
        }
    }
    else if (mouseButton == EASYGUI_MOUSE_BUTTON_MIDDLE)
    {
        if (pTB->isCloseOnMiddleClickEnabled)
        {
            easygui_tab* pHoveredTab = tabbar_find_tab_under_point(pTBElement, (float)relativeMousePosX, (float)relativeMousePosY, NULL);
            if (pHoveredTab != NULL)
            {
                if (pTB->onTabClose) {
                    pTB->onTabClose(pTBElement, pHoveredTab);
                }
            }
        }
    }
}

void tabbar_on_mouse_button_up(easygui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (mouseButton == EASYGUI_MOUSE_BUTTON_LEFT)
    {
        if (pTB->pTabWithCloseButtonPressed)
        {
            // We need to check if the button was released while over the close button, and if so post the event.
            bool releasedOverCloseButton = false;
            easygui_tab* pTabUnderMouse = tabbar_find_tab_under_point(pTBElement, (float)relativeMousePosX, (float)relativeMousePosY, &releasedOverCloseButton);

            if (releasedOverCloseButton && pTabUnderMouse == pTB->pTabWithCloseButtonPressed)
            {
                if (pTB->onTabClose) {
                    pTB->onTabClose(pTBElement, pTB->pTabWithCloseButtonPressed);
                }
            }


            pTB->pTabWithCloseButtonPressed = NULL;

            if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
                easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
            }
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

        // If a part of the tab is out of bounds, stop drawing.
        if (runningPosX + tabWidth > tabbarWidth || runningPosY + tabHeight > tabbarHeight) {
            break;
        }


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


    float closeButtonWidth  = 0;
    float closeButtonHeight = 0;
    if (pTB->isShowingCloseButton && pTB->pCloseButtonImage != NULL)
    {
        closeButtonWidth  = pTB->closeButtonWidth + pTB->closeButtonPaddingLeft;
        closeButtonHeight = pTB->closeButtonHeight;
    }

 
    if (pWidthOut) {
        *pWidthOut = textWidth + closeButtonWidth + pTB->tabPadding*2;
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
    easygui_color closeButtonColor = pTB->closeButtonColorDefault;

    if (pTB->pHoveredTab == pTab) {
        bgcolor = pTB->tabBackgroundColorHovered;
        closeButtonColor = pTB->closeButtonColorTabHovered;
    }
    if (pTB->pActiveTab == pTab) {
        bgcolor = pTB->tabBackbroundColorActivated;
        closeButtonColor = pTB->closeButtonColorTabHovered;
    }

    if (pTB->pHoveredTab == pTab && pTB->isCloseButtonHovered) {
        closeButtonColor = pTB->closeButtonColorHovered;

        if (pTB->pTabWithCloseButtonPressed == pTB->pHoveredTab) {
            closeButtonColor = pTB->closeButtonColorPressed;
        }
    }

    easygui_draw_rect_outline(pTBElement, easygui_make_rect(offsetX, offsetY, offsetX + width, offsetY + height), bgcolor, pTB->tabPadding, pPaintData);


    // Text.
    float textPosX = offsetX + pTB->tabPadding;
    float textPosY = offsetY + pTB->tabPadding;
    if (pTab != NULL)
    {
        easygui_draw_text(pTBElement, pTB->pFont, pTab->text, strlen(pTab->text), textPosX, textPosY, pTB->tabTextColor, bgcolor, pPaintData);
    }


    // Close button.
    if (pTB->isShowingCloseButton && pTB->pCloseButtonImage != NULL)
    {
        float textWidth  = 0;
        float textHeight = 0;
        if (pTab != NULL) {
            easygui_measure_string_by_element(pTB->pFont, pTab->text, strlen(pTab->text), pTBElement, &textWidth, &textHeight);
        }

        float closeButtonPosX = textPosX + textWidth + pTB->closeButtonPaddingLeft;
        float closeButtonPosY = textPosY;

        unsigned int iconWidth;
        unsigned int iconHeight;
        easygui_get_image_size(pTB->pCloseButtonImage, &iconWidth, &iconHeight);

        easygui_draw_image_args args;
        args.dstX            = closeButtonPosX;
        args.dstY            = closeButtonPosY;
        args.dstWidth        = pTB->closeButtonWidth;
        args.dstHeight       = pTB->closeButtonHeight;
        args.srcX            = 0;
        args.srcY            = 0;
        args.srcWidth        = (float)iconWidth;
        args.srcHeight       = (float)iconHeight;
        args.dstBoundsX      = args.dstX;
        args.dstBoundsY      = args.dstY;
        args.dstBoundsWidth  = pTB->closeButtonWidth;
        args.dstBoundsHeight = height - (pTB->tabPadding*2);
        args.foregroundTint  = closeButtonColor;
        args.backgroundColor = bgcolor;
        args.boundsColor     = bgcolor;
        args.options         = EASYGUI_IMAGE_DRAW_BACKGROUND | EASYGUI_IMAGE_DRAW_BOUNDS | EASYGUI_IMAGE_CLIP_BOUNDS | EASYGUI_IMAGE_ALIGN_CENTER;
        easygui_draw_image(pTBElement, pTB->pCloseButtonImage, &args, pPaintData);


        /// Space between the text and the padding.
        easygui_draw_rect(pTBElement, easygui_make_rect(textPosX + textWidth, textPosY, closeButtonPosX, textPosY + textHeight), bgcolor, pPaintData);
    }
}

PRIVATE easygui_tab* tabbar_find_tab_under_point(easygui_element* pTBElement, float relativePosX, float relativePosY, bool* pIsOverCloseButtonOut)
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

        if (relativePosX >= runningPosX && relativePosX < runningPosX + tabWidth && relativePosY >= runningPosY && relativePosY < runningPosY + tabHeight)
        {
            if (pIsOverCloseButtonOut)
            {
                if (relativePosX >= runningPosX + tabWidth  - (pTB->tabPadding + pTB->closeButtonWidth)  && relativePosX < runningPosX + tabWidth  - pTB->tabPadding &&
                    relativePosY >= runningPosY + tabHeight - (pTB->tabPadding + pTB->closeButtonHeight) && relativePosY < runningPosY + tabHeight - pTB->tabPadding)
                {
                    *pIsOverCloseButtonOut = true;
                }
                else
                {
                    *pIsOverCloseButtonOut = false;
                }
            }

            return pTab;
        }

        if (pTB->orientation == tabbar_orientation_top || pTB->orientation == tabbar_orientation_bottom) {
            runningPosX += tabWidth;
        } else {
            runningPosY += tabHeight;
        }
    }


    if (pIsOverCloseButtonOut) {
        *pIsOverCloseButtonOut = false;
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

/// Detaches the given tab bar from it's tab bar element's hierarchy.
///
/// @remarks
///     This does not deactivate the tab or what - it only detaches the tab from the hierarchy.
PRIVATE void tab_detach_from_hierarchy(easygui_tab* pTab);

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


easygui_tab* tab_get_next_tab(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return NULL;
    }

    return pTab->pNextTab;
}

easygui_tab* tab_get_prev_tab(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return NULL;
    }

    return pTab->pPrevTab;
}


void tab_move_to_front(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return;
    }

    easygui_element* pTBElement = pTab->pTBElement;
    
    tab_detach_from_hierarchy(pTab);
    tab_prepend(pTab, pTBElement);
}

bool tab_is_in_view(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return false;
    }

    return tabbar_is_tab_in_view(pTab->pTBElement, pTab);
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

PRIVATE void tab_detach_from_hierarchy(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return;
    }

    easygui_element* pTBElement = pTab->pTBElement;
    if (pTBElement == NULL) {
        return;
    }

    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
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
}

PRIVATE void tab_detach(easygui_tab* pTab)
{
    if (pTab == NULL) {
        return;
    }

    easygui_element* pTBElement = pTab->pTBElement;
    if (pTBElement == NULL) {
        return;
    }

    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    if (pTB->pHoveredTab == pTab) {
        pTB->pHoveredTab = NULL;
        pTB->isCloseButtonHovered = false;
    }

    if (pTB->pActiveTab == pTab) {
        pTB->pActiveTab = NULL;
    }

    if (pTB->pTabWithCloseButtonPressed == pTab) {
        pTB->pTabWithCloseButtonPressed = NULL;
    }


    tab_detach_from_hierarchy(pTab);


    if (pTB->isAutoSizeEnabled) {
        tabbar_resize_by_tabs(pTBElement);
    }
    
    // The content of the menu has changed so we'll need to schedule a redraw.
    if (easygui_is_auto_dirty_enabled(pTBElement->pContext)) {
        easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
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
