// Public domain. See "unlicense" statement at the end of this file.

#include "easygui_textbox.h"
#include "easygui_text_layout.h"
#include <assert.h>

typedef struct
{
    /// The text layout.
    easygui_text_layout* pTL;


    /// The color of the border.
    easygui_color borderColor;

    /// The width of the border.
    float borderWidth;

    /// The amount of padding to apply the left and right of the text.
    float padding;


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data.
    char pExtraData[1];

} easygui_textbox;


/// Retrieves the offset to draw the text in the text box.
PRIVATE void easygui_textbox__get_text_offset(easygui_element* pTBElement, float* pOffsetXOut, float* pOffsetYOut);

/// Calculates the required size of the text layout.
PRIVATE void easygui_textbox__calculate_text_layout_container_size(easygui_element* pTBElement, float* pWidthOut, float* pHeightOut);

/// Retrieves the rectangle of the text layout's container.
PRIVATE easygui_rect easygui_textbox__get_text_rect(easygui_element* pTBElement);


/// on_paint_rect()
PRIVATE void easygui_textbox__on_text_layout_paint_rect(easygui_text_layout* pLayout, easygui_rect rect, easygui_color color, easygui_element* pTBElement, void* pPaintData);

/// on_paint_text()
PRIVATE void easygui_textbox__on_text_layout_paint_text(easygui_text_layout* pTL, easygui_text_run* pRun, easygui_element* pTBElement, void* pPaintData);

/// on_dirty()
PRIVATE void easygui_textbox__on_text_layout_dirty(easygui_text_layout* pLayout, easygui_rect rect);


easygui_element* easygui_create_textbox(easygui_context* pContext, easygui_element* pParent, size_t extraDataSize, const void* pExtraData)
{
    if (pContext == NULL) {
        return NULL;
    }

    easygui_element* pTBElement = easygui_create_element(pContext, pParent, sizeof(easygui_textbox) - sizeof(char) + extraDataSize);
    if (pTBElement == NULL) {
        return NULL;
    }

    easygui_set_on_size(pTBElement, easygui_textbox_on_size);
    easygui_set_on_mouse_move(pTBElement, easygui_textbox_on_mouse_move);
    easygui_set_on_mouse_button_down(pTBElement, easygui_textbox_on_mouse_button_down);
    easygui_set_on_mouse_button_up(pTBElement, easygui_textbox_on_mouse_button_up);
    easygui_set_on_mouse_button_dblclick(pTBElement, easygui_textbox_on_mouse_button_dblclick);
    easygui_set_on_key_down(pTBElement, easygui_textbox_on_key_down);
    easygui_set_on_printable_key_down(pTBElement, easygui_textbox_on_printable_key_down);
    easygui_set_on_paint(pTBElement, easygui_textbox_on_paint);
    easygui_set_on_capture_keyboard(pTBElement, easygui_textbox_on_capture_keyboard);
    easygui_set_on_release_keyboard(pTBElement, easygui_textbox_on_release_keyboard);
    easygui_set_on_capture_mouse(pTBElement, easygui_textbox_on_capture_mouse);
    easygui_set_on_release_mouse(pTBElement, easygui_textbox_on_release_mouse);

    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    pTB->pTL = easygui_create_text_layout(pContext, sizeof(&pTBElement), &pTBElement);
    if (pTB->pTL == NULL) {
        easygui_delete_element(pTBElement);
        return NULL;
    }

    easygui_text_layout_set_on_paint_rect(pTB->pTL, easygui_textbox__on_text_layout_paint_rect);
    easygui_text_layout_set_on_paint_text(pTB->pTL, easygui_textbox__on_text_layout_paint_text);
    easygui_text_layout_set_on_dirty(pTB->pTL, easygui_textbox__on_text_layout_dirty);
    easygui_text_layout_set_default_text_color(pTB->pTL, easygui_rgb(0, 0, 0));
    easygui_text_layout_set_cursor_color(pTB->pTL, easygui_rgb(0, 0, 0));
    easygui_text_layout_set_default_bg_color(pTB->pTL, easygui_rgb(255, 255, 255));
    easygui_text_layout_set_vertical_align(pTB->pTL, easygui_text_layout_alignment_center);

    pTB->borderColor = easygui_rgb(0, 0, 0);
    pTB->borderWidth = 1;
    pTB->padding     = 2;

    pTB->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pTB->pExtraData, pExtraData, extraDataSize);
    }

    return pTBElement;
}

void easygui_delete_textbox(easygui_element* pTBElement)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_delete_element(pTBElement);
}

size_t easygui_textbox_get_extra_data_size(easygui_element* pTBElement)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return pTB->extraDataSize;
}

void* easygui_textbox_get_extra_data(easygui_element* pTBElement)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    return pTB->pExtraData;
}


void easygui_textbox_set_font(easygui_element* pTBElement, easygui_font* pFont)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_set_default_font(pTB->pTL, pFont);
}

void easygui_textbox_set_text_color(easygui_element* pTBElement, easygui_color color)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_set_default_text_color(pTB->pTL, color);
}

void easygui_textbox_set_background_color(easygui_element* pTBElement, easygui_color color)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_set_default_bg_color(pTB->pTL, color);
}

void easygui_textbox_set_cursor_color(easygui_element* pTBElement, easygui_color color)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_set_cursor_color(pTB->pTL, color);
}

void easygui_textbox_set_border_color(easygui_element* pTBElement, easygui_color color)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->borderColor = color;
}

void easygui_textbox_set_border_width(easygui_element* pTBElement, float borderWidth)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->borderWidth = borderWidth;
}

void easygui_textbox_set_padding(easygui_element* pTBElement, float paddingX)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->padding = paddingX;
}


void easygui_textbox_set_text(easygui_element* pTBElement, const char* text)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_set_text(pTB->pTL, text);
}

size_t easygui_textbox_get_text(easygui_element* pTBElement, char* pTextOut, size_t textOutSize)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return easygui_text_layout_get_text(pTB->pTL, pTextOut, textOutSize);
}

void easygui_textbox_step(easygui_element* pTBElement, unsigned int milliseconds)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_step(pTB->pTL, milliseconds);
}

void easygui_textbox_set_cursor_blink_rate(easygui_element* pTBElement, unsigned int blinkRateInMilliseconds)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_set_cursor_blink_rate(pTB->pTL, blinkRateInMilliseconds);
}


void easygui_textbox_on_size(easygui_element* pTBElement, float newWidth, float newHeight)
{
    (void)newWidth;
    (void)newHeight;

    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    // The text layout needs to be resized.
    float containerWidth;
    float containerHeight;
    easygui_textbox__calculate_text_layout_container_size(pTBElement, &containerWidth, &containerHeight);
    easygui_text_layout_set_container_size(pTB->pTL, containerWidth, containerHeight);
}

void easygui_textbox_on_mouse_move(easygui_element* pTBElement, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (easygui_get_element_with_mouse_capture(pTBElement->pContext) == pTBElement)
    {
        float offsetX;
        float offsetY;
        easygui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

        easygui_text_layout_move_cursor_to_point(pTB->pTL, (float)relativeMousePosX - offsetX, (float)relativeMousePosY - offsetY);
    }
}

void easygui_textbox_on_mouse_button_down(easygui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (mouseButton == EASYGUI_MOUSE_BUTTON_LEFT)
    {
        // Focus the text editor.
        easygui_capture_keyboard(pTBElement);

        // If we are not in selection mode, make sure everything is deselected.
        if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) == 0) {
            easygui_text_layout_deselect_all(pTB->pTL);
        } else {
            easygui_text_layout_enter_selection_mode(pTB->pTL);
        }

        float offsetX;
        float offsetY;
        easygui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

        easygui_text_layout_move_cursor_to_point(pTB->pTL, (float)relativeMousePosX - offsetX, (float)relativeMousePosY - offsetY);

        // In order to support selection with the mouse we need to capture the mouse and enter selection mode.
        easygui_capture_mouse(pTBElement);

        // If we didn't previous enter selection mode we'll need to do that now so we can drag select.
        if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) == 0) {
            easygui_text_layout_enter_selection_mode(pTB->pTL);
        }
    }
}

void easygui_textbox_on_mouse_button_up(easygui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (mouseButton == EASYGUI_MOUSE_BUTTON_LEFT)
    {
        if (easygui_get_element_with_mouse_capture(pTBElement->pContext) == pTBElement)
        {
            // Releasing the mouse will leave selectionmode.
            easygui_release_mouse(pTBElement->pContext);
        }
    }
}

void easygui_textbox_on_mouse_button_dblclick(easygui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

}

void easygui_textbox_on_key_down(easygui_element* pTBElement, easygui_key key, int stateFlags)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    switch (key)
    {
        case EASYGUI_BACKSPACE:
        {
            bool wasTextChanged = false;
            easygui_text_layout_prepare_undo_point(pTB->pTL);
            {
                if (easygui_text_layout_is_anything_selected(pTB->pTL)) {
                    wasTextChanged = easygui_text_layout_delete_selected_text(pTB->pTL);
                } else {
                    wasTextChanged = easygui_text_layout_delete_character_to_left_of_cursor(pTB->pTL);
                }
            }
            if (wasTextChanged) { easygui_text_layout_commit_undo_point(pTB->pTL); }
            
            break;
        }

        case EASYGUI_DELETE:
        {
            bool wasTextChanged = false;
            easygui_text_layout_prepare_undo_point(pTB->pTL);
            {
                if (easygui_text_layout_is_anything_selected(pTB->pTL)) {
                    wasTextChanged = easygui_text_layout_delete_selected_text(pTB->pTL);
                } else {
                    wasTextChanged = easygui_text_layout_delete_character_to_right_of_cursor(pTB->pTL);
                }
            }
            if (wasTextChanged) { easygui_text_layout_commit_undo_point(pTB->pTL); }

            break;
        }


        case EASYGUI_ARROW_LEFT:
        {
            if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                easygui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (easygui_text_layout_is_anything_selected(pTB->pTL) && !easygui_text_layout_is_in_selection_mode(pTB->pTL)) {
                easygui_text_layout_move_cursor_to_start_of_selection(pTB->pTL);
                easygui_text_layout_deselect_all(pTB->pTL);
            } else {
                easygui_text_layout_move_cursor_left(pTB->pTL);
            }

            if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                easygui_text_layout_leave_selection_mode(pTB->pTL);
            }
            
            break;
        }

        case EASYGUI_ARROW_RIGHT:
        {
            if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                easygui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (easygui_text_layout_is_anything_selected(pTB->pTL) && !easygui_text_layout_is_in_selection_mode(pTB->pTL)) {
                easygui_text_layout_move_cursor_to_end_of_selection(pTB->pTL);
                easygui_text_layout_deselect_all(pTB->pTL);
            } else {
                easygui_text_layout_move_cursor_right(pTB->pTL);
            }

            if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                easygui_text_layout_leave_selection_mode(pTB->pTL);
            }

            break;
        }

        case EASYGUI_END:
        {
            if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                easygui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (easygui_text_layout_is_anything_selected(pTB->pTL) && !easygui_text_layout_is_in_selection_mode(pTB->pTL)) {
                easygui_text_layout_deselect_all(pTB->pTL);
            }

            if ((stateFlags & EASYGUI_KEY_STATE_CTRL_DOWN) != 0) {
                easygui_text_layout_move_cursor_to_end_of_text(pTB->pTL);
            } else {
                easygui_text_layout_move_cursor_to_end_of_line(pTB->pTL);
            }

            if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                easygui_text_layout_leave_selection_mode(pTB->pTL);
            }

            break;
        }

        case EASYGUI_HOME:
        {
            if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                easygui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (easygui_text_layout_is_anything_selected(pTB->pTL) && !easygui_text_layout_is_in_selection_mode(pTB->pTL)) {
                easygui_text_layout_deselect_all(pTB->pTL);
            }

            if ((stateFlags & EASYGUI_KEY_STATE_CTRL_DOWN) != 0) {
                easygui_text_layout_move_cursor_to_start_of_text(pTB->pTL);
            } else {
                easygui_text_layout_move_cursor_to_start_of_line(pTB->pTL);
            }

            if ((stateFlags & EASYGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                easygui_text_layout_leave_selection_mode(pTB->pTL);
            }

            break;
        }

        default: break;
    }
}

void easygui_textbox_on_printable_key_down(easygui_element* pTBElement, unsigned int utf32, int stateFlags)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_prepare_undo_point(pTB->pTL);
    {
        if (easygui_text_layout_is_anything_selected(pTB->pTL)) {
            easygui_text_layout_delete_selected_text(pTB->pTL);
        }

        easygui_text_layout_insert_character_at_cursor(pTB->pTL, utf32);
    }
    easygui_text_layout_commit_undo_point(pTB->pTL);
}


PRIVATE void easygui_textbox__on_text_layout_paint_rect(easygui_text_layout* pTL, easygui_rect rect, easygui_color color, easygui_element* pTBElement, void* pPaintData)
{
    float offsetX;
    float offsetY;
    easygui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

    easygui_draw_rect(pTBElement, easygui_offset_rect(rect, offsetX, offsetY), color, pPaintData);
}

PRIVATE void easygui_textbox__on_text_layout_paint_text(easygui_text_layout* pTL, easygui_text_run* pRun, easygui_element* pTBElement, void* pPaintData)
{
    float offsetX;
    float offsetY;
    easygui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

    easygui_draw_text(pTBElement, pRun->pFont, pRun->text, pRun->textLength, (float)pRun->posX + offsetX, (float)pRun->posY + offsetY, pRun->textColor, pRun->backgroundColor, pPaintData);
}

PRIVATE void easygui_textbox__on_text_layout_dirty(easygui_text_layout* pTL, easygui_rect rect)
{
    easygui_element* pTBElement = *(easygui_element**)easygui_text_layout_get_extra_data(pTL);
    if (pTBElement == NULL) {
        return;
    }

    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    float offsetX;
    float offsetY;
    easygui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

    easygui_dirty(pTBElement, easygui_offset_rect(rect, offsetX, offsetY));
}

void easygui_textbox_on_paint(easygui_element* pTBElement, easygui_rect relativeRect, void* pPaintData)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    // Border.
    easygui_rect borderRect = easygui_get_local_rect(pTBElement);
    easygui_draw_rect_outline(pTBElement, borderRect, pTB->borderColor, pTB->borderWidth, pPaintData);

    // Padding.
    easygui_rect paddingRect = easygui_grow_rect(borderRect, -pTB->borderWidth);
    easygui_draw_rect_outline(pTBElement, paddingRect, easygui_text_layout_get_default_bg_color(pTB->pTL), pTB->padding, pPaintData);

    // Text.
    easygui_set_clip(pTBElement, easygui_clamp_rect(easygui_textbox__get_text_rect(pTBElement), relativeRect), pPaintData);
    easygui_text_layout_paint(pTB->pTL, easygui_grow_rect(paddingRect, -pTB->padding), pTBElement, pPaintData);
}

void easygui_textbox_on_capture_keyboard(easygui_element* pTBElement)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_show_cursor(pTB->pTL);
}

void easygui_textbox_on_release_keyboard(easygui_element* pTBElement)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_hide_cursor(pTB->pTL);
}

void easygui_textbox_on_capture_mouse(easygui_element* pTBElement)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

}

void easygui_textbox_on_release_mouse(easygui_element* pTBElement)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    easygui_text_layout_leave_selection_mode(pTB->pTL);
}



PRIVATE void easygui_textbox__get_text_offset(easygui_element* pTBElement, float* pOffsetXOut, float* pOffsetYOut)
{
    float offsetX = 0;
    float offsetY = 0;

    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB != NULL)
    {
        offsetX = pTB->borderWidth + pTB->padding;
        offsetY = pTB->borderWidth;
    }


    if (pOffsetXOut != NULL) {
        *pOffsetXOut = offsetX;
    }
    if (pOffsetYOut != NULL) {
        *pOffsetYOut = offsetY;
    }
}

PRIVATE void easygui_textbox__calculate_text_layout_container_size(easygui_element* pTBElement, float* pWidthOut, float* pHeightOut)
{
    float width  = 0;
    float height = 0;

    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB != NULL)
    {
        width  = easygui_get_width(pTBElement) - (pTB->borderWidth + pTB->padding)*2;
        height = easygui_get_height(pTBElement) - pTB->borderWidth*2;
    }

    if (pWidthOut != NULL) {
        *pWidthOut = width;
    }
    if (pHeightOut != NULL) {
        *pHeightOut = height;
    }
}

PRIVATE easygui_rect easygui_textbox__get_text_rect(easygui_element* pTBElement)
{
    easygui_textbox* pTB = easygui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return easygui_make_rect(0, 0, 0, 0);
    }

    float offsetX;
    float offsetY;
    easygui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

    float width;
    float height;
    easygui_textbox__calculate_text_layout_container_size(pTBElement, &width, &height);

    return easygui_make_rect(offsetX, offsetY, offsetX + width, offsetY + height);
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
