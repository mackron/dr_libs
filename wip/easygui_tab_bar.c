// Public domain. See "unlicense" statement at the end of this file.

#include "easygui_tab_bar.h"
#include <assert.h>

typedef struct easygui_tab_bar easygui_tab_bar;

struct easygui_tab_bar
{
    /// The orientation.
    tb_orientation orientation;


    /// A pointer to the first tab.
    easygui_tab* pFirstTab;

    /// A pointer to the last tab.
    easygui_tab* pLastTab;


    /// The default font to use for tab bar items.
    easygui_font* pFont;

    /// The default background color of tab bar items.
    easygui_color tabBackgroundColor;

    /// The background color of tab bar items while hovered.
    easygui_color tabBackgroundColorHovered;

    /// The background color of tab bar items while selected.
    easygui_color tabBackbroundColorSelected;


    /// The function to call when a tab needs to be measured.
    tb_on_measure_tab_proc onMeasureTab;

    /// The function to call when a tab needs to be painted.
    tb_on_paint_tab_proc onPaintTab;


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

easygui_element* eg_create_tab_bar(easygui_context* pContext, easygui_element* pParent, tb_orientation orientation, size_t extraDataSize, const void* pExtraData)
{
    if (pContext == NULL || orientation == tb_orientation_none) {
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

    pTB->pFont                      = NULL;
    pTB->tabBackgroundColor         = easygui_rgb(52, 52, 52);
    pTB->tabBackgroundColorHovered  = easygui_rgb(0, 128, 255);
    pTB->tabBackbroundColorSelected = easygui_rgb(80, 80, 80);

    pTB->onMeasureTab = NULL;
    pTB->onPaintTab   = NULL;

    pTB->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pTB->pExtraData, pExtraData, extraDataSize);
    }


    // Event handlers.
    easygui_set_on_paint(pTBElement, tb_on_paint);

    return pTBElement;
}

void eg_delete_tab_bar(easygui_element* pTBElement)
{
    if (pTBElement == NULL) {
        return;
    }

    easygui_delete_element(pTBElement);
}


size_t tb_get_extra_data_size(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return pTB->extraDataSize;
}

void* tb_get_extra_data(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    return pTB->pExtraData;
}

tb_orientation tb_get_orientation(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return tb_orientation_none;
    }

    return pTB->orientation;
}


void tb_set_font(easygui_element* pTBElement, easygui_font* pFont)
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

easygui_font* tb_get_font(easygui_element* pTBElement)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    return pTB->pFont;
}


void tb_set_on_measure_tab(easygui_element* pTBElement, tb_on_measure_tab_proc proc)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->onMeasureTab = proc;
}

void tb_set_on_paint_tab(easygui_element* pTBElement, tb_on_paint_tab_proc proc)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->onPaintTab = proc;
}


void tb_on_paint(easygui_element* pTBElement, easygui_rect relativeClippingRect, void* pPaintData)
{
    easygui_tab_bar* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_draw_rect(pTBElement, easygui_get_local_rect(pTBElement), pTB->tabBackgroundColor, pPaintData);
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

PRIVATE easygui_tab* tb_create_tab(easygui_element* pTBElement, size_t extraDataSize, const void* pExtraData)
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

    return pTab;
}

easygui_tab* tb_create_and_append_tab(easygui_element* pTBElement, size_t extraDataSize, const void* pExtraData)
{
    easygui_tab* pTab = tb_create_tab(pTBElement, extraDataSize, pExtraData);
    if (pTab != NULL)
    {
        tab_append(pTab, pTBElement);
    }

    return pTab;
}

easygui_tab* tb_create_and_prepend_tab(easygui_element* pTBElement, size_t extraDataSize, const void* pExtraData)
{
    easygui_tab* pTab = tb_create_tab(pTBElement, extraDataSize, pExtraData);
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


    // The content of the menu has changed so we'll need to schedule a redraw.
    easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
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


    // The content of the menu has changed so we'll need to schedule a redraw.
    easygui_dirty(pTBElement, easygui_get_local_rect(pTBElement));
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

    
    // The content of the menu has changed so we'll need to schedule a redraw.
    easygui_dirty(pTab->pTBElement, easygui_get_local_rect(pTab->pTBElement));
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
