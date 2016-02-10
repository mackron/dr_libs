// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - By default the cursor/caret does not blink automatically. Instead, the application must "step" the text box by
//   calling drgui_textbox_step().
//

#ifndef drgui_textbox_h
#define drgui_textbox_h

#ifdef __cplusplus
extern "C" {
#endif

/// Creates a new text box control.
drgui_element* drgui_create_textbox(drgui_context* pContext, drgui_element* pParent, size_t extraDataSize, const void* pExtraData);

/// Deletest the given text box control.
void drgui_delete_textbox(drgui_element* pTBElement);


/// Retrieves the size of the extra data associated with the given text box.
size_t drgui_textbox_get_extra_data_size(drgui_element* pTBElement);

/// Retrieves a pointer to the extra data associated with the given text box.
void* drgui_textbox_get_extra_data(drgui_element* pTBElement);


/// Sets the font to use with the given text box.
void drgui_textbox_set_font(drgui_element* pTBElement, drgui_font* pFont);

/// Sets the color of the text in teh given text box.
void drgui_textbox_set_text_color(drgui_element* pTBElement, drgui_color color);

/// Sets the background color of the given text box.
void drgui_textbox_set_background_color(drgui_element* pTBElement, drgui_color color);

/// Sets the background color for the line the caret is currently sitting on.
void drgui_textbox_set_active_line_background_color(drgui_element* pTBElement, drgui_color color);

/// Sets the color of the cursor of the given text box.
void drgui_textbox_set_cursor_color(drgui_element* pTBElement, drgui_color color);

/// Sets the border color of the given text box.
void drgui_textbox_set_border_color(drgui_element* pTBElement, drgui_color color);

/// Sets the border width of the given text box.
void drgui_textbox_set_border_width(drgui_element* pTBElement, float borderWidth);

/// Sets the amount of padding to apply to given text box.
void drgui_textbox_set_padding(drgui_element* pTBElement, float padding);

/// Sets the vertical alignment of the given text box.
void drgui_textbox_set_vertical_align(drgui_element* pTBElement, drgui_text_layout_alignment align);

/// Sets the horizontal alignment of the given text box.
void drgui_textbox_set_horizontal_align(drgui_element* pTBElement, drgui_text_layout_alignment align);


/// Sets the text of the given text box.
void drgui_textbox_set_text(drgui_element* pTBElement, const char* text);

/// Retrieves the text of the given text box.
size_t drgui_textbox_get_text(drgui_element* pTBElement, char* pTextOut, size_t textOutSize);

/// Steps the text box to allow it to blink the cursor.
void drgui_textbox_step(drgui_element* pTBElement, unsigned int milliseconds);

/// Sets the blink rate of the cursor in milliseconds.
void drgui_textbox_set_cursor_blink_rate(drgui_element* pTBElement, unsigned int blinkRateInMilliseconds);

/// Moves the caret to the end of the text.
void drgui_textbox_move_cursor_to_end_of_text(drgui_element* pTBElement);

/// Determines whether or not anything is selected in the given text box.
bool drgui_textbox_is_anything_selected(drgui_element* pTBElement);

/// Selects all of the text inside the text box.
void drgui_textbox_select_all(drgui_element* pTBElement);

/// Retrieves a copy of the selected text.
///
/// @remarks
///     This returns the length of the selected text. Call this once with <textOut> set to NULL to calculate the required size of the
///     buffer.
///     @par
///     If the output buffer is not larger enough, the string will be truncated.
size_t drgui_textbox_get_selected_text(drgui_element* pTBElement, char* textOut, size_t textOutLength);

/// Deletes the character to the right of the cursor.
///
/// @return True if the text within the text layout has changed.
bool drgui_textbox_delete_character_to_right_of_cursor(drgui_element* pTBElement);

/// Deletes the currently selected text.
///
/// @return True if the text within the text layout has changed.
bool drgui_textbox_delete_selected_text(drgui_element* pTBElement);

/// Inserts a character at the position of the cursor.
///
/// @return True if the text within the text layout has changed.
bool drgui_textbox_insert_text_at_cursor(drgui_element* pTBElement, const char* text);

/// Performs an undo operation.
bool drgui_textbox_undo(drgui_element* pTBElement);

/// Performs a redo operation.
bool drgui_textbox_redo(drgui_element* pTBElement);



/// on_size.
void drgui_textbox_on_size(drgui_element* pTBElement, float newWidth, float newHeight);

/// on_mouse_move.
void drgui_textbox_on_mouse_move(drgui_element* pTBElement, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// on_mouse_button_down.
void drgui_textbox_on_mouse_button_down(drgui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// on_mouse_button_up.
void drgui_textbox_on_mouse_button_up(drgui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// on_mouse_button_dblclick.
void drgui_textbox_on_mouse_button_dblclick(drgui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// on_key_down.
void drgui_textbox_on_key_down(drgui_element* pTBElement, drgui_key key, int stateFlags);

/// on_printable_key_down.
void drgui_textbox_on_printable_key_down(drgui_element* pTBElement, unsigned int utf32, int stateFlags);

/// on_paint.
void drgui_textbox_on_paint(drgui_element* pTBElement, drgui_rect relativeRect, void* pPaintData);

/// on_capture_keyboard
void drgui_textbox_on_capture_keyboard(drgui_element* pTBElement, drgui_element* pPrevCapturedElement);

/// on_release_keyboard
void drgui_textbox_on_release_keyboard(drgui_element* pTBElement, drgui_element* pNewCapturedElement);

/// on_capture_mouse
void drgui_textbox_on_capture_mouse(drgui_element* pTBElement);

/// on_release_mouse
void drgui_textbox_on_release_mouse(drgui_element* pTBElement);



#ifdef __cplusplus
}
#endif

#endif  //drgui_textbox_h


#ifdef DR_GUI_IMPLEMENTATION
typedef struct
{
    /// The text layout.
    drgui_text_layout* pTL;


    /// The color of the border.
    drgui_color borderColor;

    /// The width of the border.
    float borderWidth;

    /// The amount of padding to apply the left and right of the text.
    float padding;


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data.
    char pExtraData[1];

} drgui_textbox;


/// Retrieves the offset to draw the text in the text box.
DRGUI_PRIVATE void drgui_textbox__get_text_offset(drgui_element* pTBElement, float* pOffsetXOut, float* pOffsetYOut);

/// Calculates the required size of the text layout.
DRGUI_PRIVATE void drgui_textbox__calculate_text_layout_container_size(drgui_element* pTBElement, float* pWidthOut, float* pHeightOut);

/// Retrieves the rectangle of the text layout's container.
DRGUI_PRIVATE drgui_rect drgui_textbox__get_text_rect(drgui_element* pTBElement);


/// on_paint_rect()
DRGUI_PRIVATE void drgui_textbox__on_text_layout_paint_rect(drgui_text_layout* pLayout, drgui_rect rect, drgui_color color, drgui_element* pTBElement, void* pPaintData);

/// on_paint_text()
DRGUI_PRIVATE void drgui_textbox__on_text_layout_paint_text(drgui_text_layout* pTL, drgui_text_run* pRun, drgui_element* pTBElement, void* pPaintData);

/// on_dirty()
DRGUI_PRIVATE void drgui_textbox__on_text_layout_dirty(drgui_text_layout* pLayout, drgui_rect rect);


drgui_element* drgui_create_textbox(drgui_context* pContext, drgui_element* pParent, size_t extraDataSize, const void* pExtraData)
{
    if (pContext == NULL) {
        return NULL;
    }

    drgui_element* pTBElement = drgui_create_element(pContext, pParent, sizeof(drgui_textbox) - sizeof(char) + extraDataSize, NULL);
    if (pTBElement == NULL) {
        return NULL;
    }

    drgui_set_on_size(pTBElement, drgui_textbox_on_size);
    drgui_set_on_mouse_move(pTBElement, drgui_textbox_on_mouse_move);
    drgui_set_on_mouse_button_down(pTBElement, drgui_textbox_on_mouse_button_down);
    drgui_set_on_mouse_button_up(pTBElement, drgui_textbox_on_mouse_button_up);
    drgui_set_on_mouse_button_dblclick(pTBElement, drgui_textbox_on_mouse_button_dblclick);
    drgui_set_on_key_down(pTBElement, drgui_textbox_on_key_down);
    drgui_set_on_printable_key_down(pTBElement, drgui_textbox_on_printable_key_down);
    drgui_set_on_paint(pTBElement, drgui_textbox_on_paint);
    drgui_set_on_capture_keyboard(pTBElement, drgui_textbox_on_capture_keyboard);
    drgui_set_on_release_keyboard(pTBElement, drgui_textbox_on_release_keyboard);
    drgui_set_on_capture_mouse(pTBElement, drgui_textbox_on_capture_mouse);
    drgui_set_on_release_mouse(pTBElement, drgui_textbox_on_release_mouse);

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    pTB->pTL = drgui_create_text_layout(pContext, sizeof(&pTBElement), &pTBElement);
    if (pTB->pTL == NULL) {
        drgui_delete_element(pTBElement);
        return NULL;
    }

    drgui_text_layout_set_on_paint_rect(pTB->pTL, drgui_textbox__on_text_layout_paint_rect);
    drgui_text_layout_set_on_paint_text(pTB->pTL, drgui_textbox__on_text_layout_paint_text);
    drgui_text_layout_set_on_dirty(pTB->pTL, drgui_textbox__on_text_layout_dirty);
    drgui_text_layout_set_default_text_color(pTB->pTL, drgui_rgb(0, 0, 0));
    drgui_text_layout_set_cursor_color(pTB->pTL, drgui_rgb(0, 0, 0));
    drgui_text_layout_set_default_bg_color(pTB->pTL, drgui_rgb(255, 255, 255));
    drgui_text_layout_set_active_line_bg_color(pTB->pTL, drgui_rgb(255, 255, 255));
    drgui_text_layout_set_vertical_align(pTB->pTL, drgui_text_layout_alignment_center);

    pTB->borderColor = drgui_rgb(0, 0, 0);
    pTB->borderWidth = 1;
    pTB->padding     = 2;

    pTB->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pTB->pExtraData, pExtraData, extraDataSize);
    }

    return pTBElement;
}

void drgui_delete_textbox(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_delete_element(pTBElement);
}

size_t drgui_textbox_get_extra_data_size(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return pTB->extraDataSize;
}

void* drgui_textbox_get_extra_data(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    return pTB->pExtraData;
}


void drgui_textbox_set_font(drgui_element* pTBElement, drgui_font* pFont)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_default_font(pTB->pTL, pFont);
}

void drgui_textbox_set_text_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_default_text_color(pTB->pTL, color);
}

void drgui_textbox_set_background_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_default_bg_color(pTB->pTL, color);
}

void drgui_textbox_set_active_line_background_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_active_line_bg_color(pTB->pTL, color);
}

void drgui_textbox_set_cursor_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_cursor_color(pTB->pTL, color);
}

void drgui_textbox_set_border_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->borderColor = color;
}

void drgui_textbox_set_border_width(drgui_element* pTBElement, float borderWidth)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->borderWidth = borderWidth;
}

void drgui_textbox_set_padding(drgui_element* pTBElement, float padding)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->padding = padding;
}

void drgui_textbox_set_vertical_align(drgui_element* pTBElement, drgui_text_layout_alignment align)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_vertical_align(pTB->pTL, align);
}

void drgui_textbox_set_horizontal_align(drgui_element* pTBElement, drgui_text_layout_alignment align)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_horizontal_align(pTB->pTL, align);
}


void drgui_textbox_set_text(drgui_element* pTBElement, const char* text)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_text(pTB->pTL, text);
}

size_t drgui_textbox_get_text(drgui_element* pTBElement, char* pTextOut, size_t textOutSize)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return drgui_text_layout_get_text(pTB->pTL, pTextOut, textOutSize);
}

void drgui_textbox_step(drgui_element* pTBElement, unsigned int milliseconds)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_step(pTB->pTL, milliseconds);
}

void drgui_textbox_set_cursor_blink_rate(drgui_element* pTBElement, unsigned int blinkRateInMilliseconds)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_cursor_blink_rate(pTB->pTL, blinkRateInMilliseconds);
}

void drgui_textbox_move_cursor_to_end_of_text(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_move_cursor_to_end_of_text(pTB->pTL);
}


bool drgui_textbox_is_anything_selected(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return drgui_text_layout_is_anything_selected(pTB->pTL);
}

void drgui_textbox_select_all(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_select_all(pTB->pTL);
}

size_t drgui_textbox_get_selected_text(drgui_element* pTBElement, char* textOut, size_t textOutLength)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return drgui_text_layout_get_selected_text(pTB->pTL, textOut, textOutLength);
}

bool drgui_textbox_delete_character_to_right_of_cursor(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    bool wasTextChanged = false;
    drgui_text_layout_prepare_undo_point(pTB->pTL);
    {
        wasTextChanged = drgui_text_layout_delete_character_to_right_of_cursor(pTB->pTL);
    }
    if (wasTextChanged) { drgui_text_layout_commit_undo_point(pTB->pTL); }

    return wasTextChanged;
}

bool drgui_textbox_delete_selected_text(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    bool wasTextChanged = false;
    drgui_text_layout_prepare_undo_point(pTB->pTL);
    {
        wasTextChanged = drgui_text_layout_delete_selected_text(pTB->pTL);
    }
    if (wasTextChanged) { drgui_text_layout_commit_undo_point(pTB->pTL); }

    return wasTextChanged;
}

bool drgui_textbox_insert_text_at_cursor(drgui_element* pTBElement, const char* text)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;;
    }

    bool wasTextChanged = false;
    drgui_text_layout_prepare_undo_point(pTB->pTL);
    {
        wasTextChanged = drgui_text_layout_insert_text_at_cursor(pTB->pTL, text);
    }
    if (wasTextChanged) { drgui_text_layout_commit_undo_point(pTB->pTL); }

    return wasTextChanged;
}

bool drgui_textbox_undo(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return drgui_text_layout_undo(pTB->pTL);
}

bool drgui_textbox_redo(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return drgui_text_layout_redo(pTB->pTL);
}


void drgui_textbox_on_size(drgui_element* pTBElement, float newWidth, float newHeight)
{
    (void)newWidth;
    (void)newHeight;

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    // The text layout needs to be resized.
    float containerWidth;
    float containerHeight;
    drgui_textbox__calculate_text_layout_container_size(pTBElement, &containerWidth, &containerHeight);
    drgui_text_layout_set_container_size(pTB->pTL, containerWidth, containerHeight);
}

void drgui_textbox_on_mouse_move(drgui_element* pTBElement, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)stateFlags;

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (drgui_get_element_with_mouse_capture(pTBElement->pContext) == pTBElement)
    {
        float offsetX;
        float offsetY;
        drgui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

        drgui_text_layout_move_cursor_to_point(pTB->pTL, (float)relativeMousePosX - offsetX, (float)relativeMousePosY - offsetY);
    }
}

void drgui_textbox_on_mouse_button_down(drgui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (mouseButton == DRGUI_MOUSE_BUTTON_LEFT)
    {
        // Focus the text editor.
        drgui_capture_keyboard(pTBElement);

        // If we are not in selection mode, make sure everything is deselected.
        if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) == 0) {
            drgui_text_layout_deselect_all(pTB->pTL);
        } else {
            drgui_text_layout_enter_selection_mode(pTB->pTL);
        }

        float offsetX;
        float offsetY;
        drgui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

        drgui_text_layout_move_cursor_to_point(pTB->pTL, (float)relativeMousePosX - offsetX, (float)relativeMousePosY - offsetY);

        // In order to support selection with the mouse we need to capture the mouse and enter selection mode.
        drgui_capture_mouse(pTBElement);

        // If we didn't previous enter selection mode we'll need to do that now so we can drag select.
        if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) == 0) {
            drgui_text_layout_enter_selection_mode(pTB->pTL);
        }
    }
}

void drgui_textbox_on_mouse_button_up(drgui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (mouseButton == DRGUI_MOUSE_BUTTON_LEFT)
    {
        if (drgui_get_element_with_mouse_capture(pTBElement->pContext) == pTBElement)
        {
            // Releasing the mouse will leave selectionmode.
            drgui_release_mouse(pTBElement->pContext);
        }
    }
}

void drgui_textbox_on_mouse_button_dblclick(drgui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)mouseButton;
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

}

void drgui_textbox_on_key_down(drgui_element* pTBElement, drgui_key key, int stateFlags)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    switch (key)
    {
        case DRGUI_BACKSPACE:
        {
            bool wasTextChanged = false;
            drgui_text_layout_prepare_undo_point(pTB->pTL);
            {
                if (drgui_text_layout_is_anything_selected(pTB->pTL)) {
                    wasTextChanged = drgui_text_layout_delete_selected_text(pTB->pTL);
                } else {
                    wasTextChanged = drgui_text_layout_delete_character_to_left_of_cursor(pTB->pTL);
                }
            }
            if (wasTextChanged) { drgui_text_layout_commit_undo_point(pTB->pTL); }

            break;
        }

        case DRGUI_DELETE:
        {
            bool wasTextChanged = false;
            drgui_text_layout_prepare_undo_point(pTB->pTL);
            {
                if (drgui_text_layout_is_anything_selected(pTB->pTL)) {
                    wasTextChanged = drgui_text_layout_delete_selected_text(pTB->pTL);
                } else {
                    wasTextChanged = drgui_text_layout_delete_character_to_right_of_cursor(pTB->pTL);
                }
            }
            if (wasTextChanged) { drgui_text_layout_commit_undo_point(pTB->pTL); }

            break;
        }


        case DRGUI_ARROW_LEFT:
        {
            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (drgui_text_layout_is_anything_selected(pTB->pTL) && !drgui_text_layout_is_in_selection_mode(pTB->pTL)) {
                drgui_text_layout_move_cursor_to_start_of_selection(pTB->pTL);
                drgui_text_layout_deselect_all(pTB->pTL);
            } else {
                drgui_text_layout_move_cursor_left(pTB->pTL);
            }

            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_leave_selection_mode(pTB->pTL);
            }

            break;
        }

        case DRGUI_ARROW_RIGHT:
        {
            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (drgui_text_layout_is_anything_selected(pTB->pTL) && !drgui_text_layout_is_in_selection_mode(pTB->pTL)) {
                drgui_text_layout_move_cursor_to_end_of_selection(pTB->pTL);
                drgui_text_layout_deselect_all(pTB->pTL);
            } else {
                drgui_text_layout_move_cursor_right(pTB->pTL);
            }

            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_leave_selection_mode(pTB->pTL);
            }

            break;
        }

        case DRGUI_END:
        {
            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (drgui_text_layout_is_anything_selected(pTB->pTL) && !drgui_text_layout_is_in_selection_mode(pTB->pTL)) {
                drgui_text_layout_deselect_all(pTB->pTL);
            }

            if ((stateFlags & DRGUI_KEY_STATE_CTRL_DOWN) != 0) {
                drgui_text_layout_move_cursor_to_end_of_text(pTB->pTL);
            } else {
                drgui_text_layout_move_cursor_to_end_of_line(pTB->pTL);
            }

            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_leave_selection_mode(pTB->pTL);
            }

            break;
        }

        case DRGUI_HOME:
        {
            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (drgui_text_layout_is_anything_selected(pTB->pTL) && !drgui_text_layout_is_in_selection_mode(pTB->pTL)) {
                drgui_text_layout_deselect_all(pTB->pTL);
            }

            if ((stateFlags & DRGUI_KEY_STATE_CTRL_DOWN) != 0) {
                drgui_text_layout_move_cursor_to_start_of_text(pTB->pTL);
            } else {
                drgui_text_layout_move_cursor_to_start_of_line(pTB->pTL);
            }

            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_leave_selection_mode(pTB->pTL);
            }

            break;
        }

        default: break;
    }
}

void drgui_textbox_on_printable_key_down(drgui_element* pTBElement, unsigned int utf32, int stateFlags)
{
    (void)stateFlags;

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_prepare_undo_point(pTB->pTL);
    {
        if (drgui_text_layout_is_anything_selected(pTB->pTL)) {
            drgui_text_layout_delete_selected_text(pTB->pTL);
        }

        drgui_text_layout_insert_character_at_cursor(pTB->pTL, utf32);
    }
    drgui_text_layout_commit_undo_point(pTB->pTL);
}


DRGUI_PRIVATE void drgui_textbox__on_text_layout_paint_rect(drgui_text_layout* pTL, drgui_rect rect, drgui_color color, drgui_element* pTBElement, void* pPaintData)
{
    (void)pTL;

    float offsetX;
    float offsetY;
    drgui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

    drgui_draw_rect(pTBElement, drgui_offset_rect(rect, offsetX, offsetY), color, pPaintData);
}

DRGUI_PRIVATE void drgui_textbox__on_text_layout_paint_text(drgui_text_layout* pTL, drgui_text_run* pRun, drgui_element* pTBElement, void* pPaintData)
{
    (void)pTL;

    float offsetX;
    float offsetY;
    drgui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

    drgui_draw_text(pTBElement, pRun->pFont, pRun->text, (int)pRun->textLength, (float)pRun->posX + offsetX, (float)pRun->posY + offsetY, pRun->textColor, pRun->backgroundColor, pPaintData);
}

DRGUI_PRIVATE void drgui_textbox__on_text_layout_dirty(drgui_text_layout* pTL, drgui_rect rect)
{
    drgui_element* pTBElement = *(drgui_element**)drgui_text_layout_get_extra_data(pTL);
    if (pTBElement == NULL) {
        return;
    }

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    float offsetX;
    float offsetY;
    drgui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

    drgui_dirty(pTBElement, drgui_offset_rect(rect, offsetX, offsetY));
}

void drgui_textbox_on_paint(drgui_element* pTBElement, drgui_rect relativeRect, void* pPaintData)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    // Border.
    drgui_rect borderRect = drgui_get_local_rect(pTBElement);
    drgui_draw_rect_outline(pTBElement, borderRect, pTB->borderColor, pTB->borderWidth, pPaintData);

    // Padding.
    drgui_rect paddingRect = drgui_grow_rect(borderRect, -pTB->borderWidth);
    drgui_draw_rect_outline(pTBElement, paddingRect, drgui_text_layout_get_default_bg_color(pTB->pTL), pTB->padding, pPaintData);

    // Text.
    drgui_rect textRect = drgui_clamp_rect(drgui_textbox__get_text_rect(pTBElement), relativeRect);
    drgui_set_clip(pTBElement, textRect, pPaintData);
    drgui_text_layout_paint(pTB->pTL, drgui_grow_rect(textRect, -pTB->padding), pTBElement, pPaintData);
}

void drgui_textbox_on_capture_keyboard(drgui_element* pTBElement, drgui_element* pPrevCapturedElement)
{
    (void)pPrevCapturedElement;

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_show_cursor(pTB->pTL);
}

void drgui_textbox_on_release_keyboard(drgui_element* pTBElement, drgui_element* pNewCapturedElement)
{
    (void)pNewCapturedElement;

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_hide_cursor(pTB->pTL);
}

void drgui_textbox_on_capture_mouse(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

}

void drgui_textbox_on_release_mouse(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_leave_selection_mode(pTB->pTL);
}



DRGUI_PRIVATE void drgui_textbox__get_text_offset(drgui_element* pTBElement, float* pOffsetXOut, float* pOffsetYOut)
{
    float offsetX = 0;
    float offsetY = 0;

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB != NULL)
    {
        offsetX = pTB->borderWidth + pTB->padding;
        offsetY = pTB->borderWidth + pTB->padding;
    }


    if (pOffsetXOut != NULL) {
        *pOffsetXOut = offsetX;
    }
    if (pOffsetYOut != NULL) {
        *pOffsetYOut = offsetY;
    }
}

DRGUI_PRIVATE void drgui_textbox__calculate_text_layout_container_size(drgui_element* pTBElement, float* pWidthOut, float* pHeightOut)
{
    float width  = 0;
    float height = 0;

    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB != NULL)
    {
        width  = drgui_get_width(pTBElement)  - (pTB->borderWidth + pTB->padding)*2;
        height = drgui_get_height(pTBElement) - (pTB->borderWidth + pTB->padding)*2;
    }

    if (pWidthOut != NULL) {
        *pWidthOut = width;
    }
    if (pHeightOut != NULL) {
        *pHeightOut = height;
    }
}

DRGUI_PRIVATE drgui_rect drgui_textbox__get_text_rect(drgui_element* pTBElement)
{
    drgui_textbox* pTB = drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return drgui_make_rect(0, 0, 0, 0);
    }

    float offsetX;
    float offsetY;
    drgui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

    float width;
    float height;
    drgui_textbox__calculate_text_layout_container_size(pTBElement, &width, &height);

    return drgui_make_rect(offsetX, offsetY, offsetX + width, offsetY + height);
}
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
