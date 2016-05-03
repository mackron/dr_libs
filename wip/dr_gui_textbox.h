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

typedef void (* drgui_textbox_on_cursor_move_proc)(drgui_element* pTBElement);
typedef void (* drgui_textbox_on_undo_point_changed_proc)(drgui_element* pTBElement, unsigned int iUndoPoint);


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

/// Moves the caret to the beginning of the line at the given index.
void drgui_textbox_move_cursor_to_start_of_line_by_index(drgui_element* pTBElement, size_t iLine);

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

/// Retrieves the number of undo points remaining.
unsigned int drgui_textbox_get_undo_points_remaining_count(drgui_element* pTBElement);

/// Retrieves the number of redo points remaining.
unsigned int drgui_textbox_get_redo_points_remaining_count(drgui_element* pTBElement);

/// Retrieves the index of the line the cursor is current sitting on.
size_t drgui_textbox_get_cursor_line(drgui_element* pTBElement);

/// Retrieves the index of the column the cursor is current sitting on.
size_t drgui_textbox_get_cursor_column(drgui_element* pTBElement);

/// Retrieves the number of lines in the given text box.
size_t drgui_textbox_get_line_count(drgui_element* pTBElement);


/// Finds and selects the next occurance of the given string, starting from the cursor and looping back to the start.
bool drgui_textbox_find_and_select_next(drgui_element* pTBElement, const char* text);

/// Finds the next occurance of the given string and replaces it with another.
bool drgui_textbox_find_and_replace_next(drgui_element* pTBElement, const char* text, const char* replacement);

/// Finds every occurance of the given string and replaces it with another.
bool drgui_textbox_find_and_replace_all(drgui_element* pTBElement, const char* text, const char* replacement);


/// Shows the line numbers.
void drgui_textbox_show_line_numbers(drgui_element* pTBElement);

/// Hides the line numbers.
void drgui_textbox_hide_line_numbers(drgui_element* pTBElement);


/// Disables the vertical scrollbar.
void drgui_textbox_disable_vertical_scrollbar(drgui_element* pTBElement);

/// Enables the vertical scrollbar.
void drgui_textbox_enable_vertical_scrollbar(drgui_element* pTBElement);

/// Disables the horizontal scrollbar.
void drgui_textbox_disable_horizontal_scrollbar(drgui_element* pTBElement);

/// Enables the horizontal scrollbar.
void drgui_textbox_enable_horizontal_scrollbar(drgui_element* pTBElement);


/// Sets the function to call when the cursor moves.
void drgui_textbox_set_on_cursor_move(drgui_element* pTBElement, drgui_textbox_on_cursor_move_proc proc);

/// Sets the function to call when the undo point changes.
void drgui_textbox_set_on_undo_point_changed(drgui_element* pTBElement, drgui_textbox_on_undo_point_changed_proc proc);



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

/// on_mouse_wheel
void drgui_textbox_on_mouse_wheel(drgui_element* pTBElement, int delta, int relativeMousePosX, int relativeMousePosY, int stateFlags);

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

    /// The vertical scrollbar.
    drgui_element* pVertScrollbar;

    /// The horizontal scrollbar.
    drgui_element* pHorzScrollbar;

    /// The line numbers element.
    drgui_element* pLineNumbers;


    /// The color of the border.
    drgui_color borderColor;

    /// The width of the border.
    float borderWidth;

    /// The amount of padding to apply the left and right of the text.
    float padding;

    /// The padding to the right of the line numbers.
    float lineNumbersPaddingRight;


    /// The desired width of the vertical scrollbar.
    float vertScrollbarSize;

    /// The desired height of the horizontal scrollbar.
    float horzScrollbarSize;

    /// Whether or not the vertical scrollbar is enabled.
    bool isVertScrollbarEnabled;

    /// Whether or not the horizontal scrollbar is enabled.
    bool isHorzScrollbarEnabled;


    /// When selecting lines by clicking and dragging on the line numbers, keeps track of the line to anchor the selection to.
    size_t iLineSelectAnchor;


    /// The function to call when the text cursor/caret moves.
    drgui_textbox_on_cursor_move_proc onCursorMove;

    /// The function to call when the undo point changes.
    drgui_textbox_on_undo_point_changed_proc onUndoPointChanged;


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

/// Refreshes the range, page sizes and layouts of the scrollbars.
DRGUI_PRIVATE void drgui_textbox__refresh_scrollbars(drgui_element* pTBElement);

/// Refreshes the range and page sizes of the scrollbars.
DRGUI_PRIVATE void drgui_textbox__refresh_scrollbar_ranges(drgui_element* pTBElement);

/// Refreshes the size and position of the scrollbars.
DRGUI_PRIVATE void drgui_textbox__refresh_scrollbar_layouts(drgui_element* pTBElement);

/// Retrieves a rectangle representing the space between the edges of the two scrollbars.
DRGUI_PRIVATE drgui_rect drgui_textbox__get_scrollbar_dead_space_rect(drgui_element* pTBElement);


/// Called when a mouse button is pressed on the line numbers element.
DRGUI_PRIVATE void drgui_textbox__on_mouse_move_line_numbers(drgui_element* pLineNumbers, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when a mouse button is pressed on the line numbers element.
DRGUI_PRIVATE void drgui_textbox__on_mouse_button_down_line_numbers(drgui_element* pLineNumbers, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when a mouse button is pressed on the line numbers element.
DRGUI_PRIVATE void drgui_textbox__on_mouse_button_up_line_numbers(drgui_element* pLineNumbers, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// Called when the line numbers element needs to be drawn.
DRGUI_PRIVATE void drgui_textbox__on_paint_line_numbers(drgui_element* pLineNumbers, drgui_rect relativeRect, void* pPaintData);

/// Refreshes the line number of the given text editor.
DRGUI_PRIVATE void drgui_textbox__refresh_line_numbers(drgui_element* pTBElement);


/// on_paint_rect()
DRGUI_PRIVATE void drgui_textbox__on_text_layout_paint_rect(drgui_text_layout* pLayout, drgui_rect rect, drgui_color color, drgui_element* pTBElement, void* pPaintData);

/// on_paint_text()
DRGUI_PRIVATE void drgui_textbox__on_text_layout_paint_text(drgui_text_layout* pTL, drgui_text_run* pRun, drgui_element* pTBElement, void* pPaintData);

/// on_dirty()
DRGUI_PRIVATE void drgui_textbox__on_text_layout_dirty(drgui_text_layout* pTL, drgui_rect rect);

/// on_cursor_move()
DRGUI_PRIVATE void drgui_textbox__on_text_layout_cursor_move(drgui_text_layout* pTL);

/// on_text_changed()
DRGUI_PRIVATE void drgui_textbox__on_text_layout_text_changed(drgui_text_layout* pTL);

/// on_undo_point_changed()
DRGUI_PRIVATE void drgui_textbox__on_text_layout_undo_point_changed(drgui_text_layout* pTL, unsigned int iUndoPoint);


DRGUI_PRIVATE void drgui_textbox__on_vscroll(drgui_element* pSBElement, int scrollPos)
{
    drgui_element* pTBElement = *(drgui_element**)drgui_sb_get_extra_data(pSBElement);
    assert(pTBElement != NULL);

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    drgui_text_layout_set_inner_offset_y(pTB->pTL, -drgui_text_layout_get_line_pos_y(pTB->pTL, scrollPos));

    // The line numbers need to be redrawn.
    drgui_dirty(pTB->pLineNumbers, drgui_get_local_rect(pTB->pLineNumbers));
}

DRGUI_PRIVATE void drgui_textbox__on_hscroll(drgui_element* pSBElement, int scrollPos)
{
    drgui_element* pTBElement = *(drgui_element**)drgui_sb_get_extra_data(pSBElement);
    assert(pTBElement != NULL);

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    drgui_text_layout_set_inner_offset_x(pTB->pTL, (float)-scrollPos);
}



drgui_element* drgui_create_textbox(drgui_context* pContext, drgui_element* pParent, size_t extraDataSize, const void* pExtraData)
{
    if (pContext == NULL) {
        return NULL;
    }

    drgui_element* pTBElement = drgui_create_element(pContext, pParent, sizeof(drgui_textbox) - sizeof(char) + extraDataSize, NULL);
    if (pTBElement == NULL) {
        return NULL;
    }

    drgui_set_cursor(pTBElement, drgui_cursor_text);
    drgui_set_on_size(pTBElement, drgui_textbox_on_size);
    drgui_set_on_mouse_move(pTBElement, drgui_textbox_on_mouse_move);
    drgui_set_on_mouse_button_down(pTBElement, drgui_textbox_on_mouse_button_down);
    drgui_set_on_mouse_button_up(pTBElement, drgui_textbox_on_mouse_button_up);
    drgui_set_on_mouse_button_dblclick(pTBElement, drgui_textbox_on_mouse_button_dblclick);
    drgui_set_on_mouse_wheel(pTBElement, drgui_textbox_on_mouse_wheel);
    drgui_set_on_key_down(pTBElement, drgui_textbox_on_key_down);
    drgui_set_on_printable_key_down(pTBElement, drgui_textbox_on_printable_key_down);
    drgui_set_on_paint(pTBElement, drgui_textbox_on_paint);
    drgui_set_on_capture_keyboard(pTBElement, drgui_textbox_on_capture_keyboard);
    drgui_set_on_release_keyboard(pTBElement, drgui_textbox_on_release_keyboard);
    drgui_set_on_capture_mouse(pTBElement, drgui_textbox_on_capture_mouse);
    drgui_set_on_release_mouse(pTBElement, drgui_textbox_on_release_mouse);

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    pTB->pVertScrollbar = drgui_create_scrollbar(pContext, pTBElement, drgui_sb_orientation_vertical, sizeof(&pTBElement), &pTBElement);
    drgui_sb_set_on_scroll(pTB->pVertScrollbar, drgui_textbox__on_vscroll);
    drgui_sb_set_mouse_wheel_scele(pTB->pVertScrollbar, 3);

    pTB->pHorzScrollbar = drgui_create_scrollbar(pContext, pTBElement, drgui_sb_orientation_horizontal, sizeof(&pTBElement), &pTBElement);
    drgui_sb_set_on_scroll(pTB->pHorzScrollbar, drgui_textbox__on_hscroll);

    pTB->pLineNumbers = drgui_create_element(pContext, pTBElement, sizeof(&pTBElement), &pTBElement);
    drgui_hide(pTB->pLineNumbers);
    drgui_set_on_mouse_move(pTB->pLineNumbers, drgui_textbox__on_mouse_move_line_numbers);
    drgui_set_on_mouse_button_down(pTB->pLineNumbers, drgui_textbox__on_mouse_button_down_line_numbers);
    drgui_set_on_mouse_button_up(pTB->pLineNumbers, drgui_textbox__on_mouse_button_up_line_numbers);
    drgui_set_on_paint(pTB->pLineNumbers, drgui_textbox__on_paint_line_numbers);

    pTB->pTL = drgui_create_text_layout(pContext, sizeof(&pTBElement), &pTBElement);
    if (pTB->pTL == NULL) {
        drgui_delete_element(pTBElement);
        return NULL;
    }

    drgui_text_layout_set_on_paint_rect(pTB->pTL, drgui_textbox__on_text_layout_paint_rect);
    drgui_text_layout_set_on_paint_text(pTB->pTL, drgui_textbox__on_text_layout_paint_text);
    drgui_text_layout_set_on_dirty(pTB->pTL, drgui_textbox__on_text_layout_dirty);
    drgui_text_layout_set_on_cursor_move(pTB->pTL, drgui_textbox__on_text_layout_cursor_move);
    drgui_text_layout_set_on_text_changed(pTB->pTL, drgui_textbox__on_text_layout_text_changed);
    drgui_text_layout_set_on_undo_point_changed(pTB->pTL, drgui_textbox__on_text_layout_undo_point_changed);
    drgui_text_layout_set_default_text_color(pTB->pTL, drgui_rgb(0, 0, 0));
    drgui_text_layout_set_cursor_color(pTB->pTL, drgui_rgb(0, 0, 0));
    drgui_text_layout_set_default_bg_color(pTB->pTL, drgui_rgb(255, 255, 255));
    drgui_text_layout_set_active_line_bg_color(pTB->pTL, drgui_rgb(255, 255, 255));
    drgui_text_layout_set_vertical_align(pTB->pTL, drgui_text_layout_alignment_center);

    pTB->borderColor = drgui_rgb(0, 0, 0);
    pTB->borderWidth = 1;
    pTB->padding     = 2;
    pTB->lineNumbersPaddingRight = 16;
    pTB->vertScrollbarSize = 16;
    pTB->horzScrollbarSize = 16;
    pTB->isVertScrollbarEnabled = true;
    pTB->isHorzScrollbarEnabled = true;
    pTB->iLineSelectAnchor = 0;
    pTB->onCursorMove = NULL;
    pTB->onUndoPointChanged = NULL;

    pTB->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pTB->pExtraData, pExtraData, extraDataSize);
    }

    return pTBElement;
}

void drgui_delete_textbox(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_delete_element(pTBElement);
}

size_t drgui_textbox_get_extra_data_size(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return pTB->extraDataSize;
}

void* drgui_textbox_get_extra_data(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return NULL;
    }

    return pTB->pExtraData;
}


void drgui_textbox_set_font(drgui_element* pTBElement, drgui_font* pFont)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_default_font(pTB->pTL, pFont);
}

void drgui_textbox_set_text_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_default_text_color(pTB->pTL, color);
}

void drgui_textbox_set_background_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_default_bg_color(pTB->pTL, color);
}

void drgui_textbox_set_active_line_background_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_active_line_bg_color(pTB->pTL, color);
}

void drgui_textbox_set_cursor_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_cursor_color(pTB->pTL, color);
}

void drgui_textbox_set_border_color(drgui_element* pTBElement, drgui_color color)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->borderColor = color;
}

void drgui_textbox_set_border_width(drgui_element* pTBElement, float borderWidth)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->borderWidth = borderWidth;
}

void drgui_textbox_set_padding(drgui_element* pTBElement, float padding)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->padding = padding;
}

void drgui_textbox_set_vertical_align(drgui_element* pTBElement, drgui_text_layout_alignment align)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_vertical_align(pTB->pTL, align);
}

void drgui_textbox_set_horizontal_align(drgui_element* pTBElement, drgui_text_layout_alignment align)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_horizontal_align(pTB->pTL, align);
}


void drgui_textbox_set_text(drgui_element* pTBElement, const char* text)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_text(pTB->pTL, text);
}

size_t drgui_textbox_get_text(drgui_element* pTBElement, char* pTextOut, size_t textOutSize)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return drgui_text_layout_get_text(pTB->pTL, pTextOut, textOutSize);
}

void drgui_textbox_step(drgui_element* pTBElement, unsigned int milliseconds)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_step(pTB->pTL, milliseconds);
}

void drgui_textbox_set_cursor_blink_rate(drgui_element* pTBElement, unsigned int blinkRateInMilliseconds)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_set_cursor_blink_rate(pTB->pTL, blinkRateInMilliseconds);
}

void drgui_textbox_move_cursor_to_end_of_text(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_move_cursor_to_end_of_text(pTB->pTL);
}

void drgui_textbox_move_cursor_to_start_of_line_by_index(drgui_element* pTBElement, size_t iLine)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_move_cursor_to_start_of_line_by_index(pTB->pTL, iLine);
}


bool drgui_textbox_is_anything_selected(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return drgui_text_layout_is_anything_selected(pTB->pTL);
}

void drgui_textbox_select_all(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_select_all(pTB->pTL);
}

size_t drgui_textbox_get_selected_text(drgui_element* pTBElement, char* textOut, size_t textOutLength)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return drgui_text_layout_get_selected_text(pTB->pTL, textOut, textOutLength);
}

bool drgui_textbox_delete_character_to_right_of_cursor(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
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
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
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
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
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
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return drgui_text_layout_undo(pTB->pTL);
}

bool drgui_textbox_redo(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return drgui_text_layout_redo(pTB->pTL);
}

unsigned int drgui_textbox_get_undo_points_remaining_count(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return drgui_text_layout_get_undo_points_remaining_count(pTB->pTL);
}

unsigned int drgui_textbox_get_redo_points_remaining_count(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return false;
    }

    return drgui_text_layout_get_redo_points_remaining_count(pTB->pTL);
}


size_t drgui_textbox_get_cursor_line(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return drgui_text_layout_get_cursor_line(pTB->pTL);
}

size_t drgui_textbox_get_cursor_column(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return drgui_text_layout_get_cursor_column(pTB->pTL);
}

size_t drgui_textbox_get_line_count(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    return drgui_text_layout_get_line_count(pTB->pTL);
}


bool drgui_textbox_find_and_select_next(drgui_element* pTBElement, const char* text)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    size_t selectionStart;
    size_t selectionEnd;
    if (drgui_text_layout_find_next(pTB->pTL, text, &selectionStart, &selectionEnd))
    {
        drgui_text_layout_select(pTB->pTL, selectionStart, selectionEnd);
        drgui_text_layout_move_cursor_to_end_of_selection(pTB->pTL);

        return true;
    }

    return false;
}

bool drgui_textbox_find_and_replace_next(drgui_element* pTBElement, const char* text, const char* replacement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    bool wasTextChanged = false;
    drgui_text_layout_prepare_undo_point(pTB->pTL);
    {
        size_t selectionStart;
        size_t selectionEnd;
        if (drgui_text_layout_find_next(pTB->pTL, text, &selectionStart, &selectionEnd))
        {
            drgui_text_layout_select(pTB->pTL, selectionStart, selectionEnd);
            drgui_text_layout_move_cursor_to_end_of_selection(pTB->pTL);

            wasTextChanged = drgui_text_layout_delete_selected_text(pTB->pTL) || wasTextChanged;
            wasTextChanged = drgui_text_layout_insert_text_at_cursor(pTB->pTL, replacement) || wasTextChanged;
        }
    }
    if (wasTextChanged) { drgui_text_layout_commit_undo_point(pTB->pTL); }

    return wasTextChanged;
}

bool drgui_textbox_find_and_replace_all(drgui_element* pTBElement, const char* text, const char* replacement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return 0;
    }

    size_t originalCursorLine = drgui_text_layout_get_cursor_line(pTB->pTL);
    size_t originalCursorPos = drgui_text_layout_get_cursor_character(pTB->pTL) - drgui_text_layout_get_line_first_character(pTB->pTL, originalCursorLine);
    int originalScrollPosX = drgui_sb_get_scroll_position(pTB->pHorzScrollbar);
    int originalScrollPosY = drgui_sb_get_scroll_position(pTB->pVertScrollbar);

    bool wasTextChanged = false;
    drgui_text_layout_prepare_undo_point(pTB->pTL);
    {
        // It's important that we don't replace the replacement text. To handle this, we just move the cursor to the top of the text and find
        // and replace every occurance without looping.
        drgui_text_layout_move_cursor_to_start_of_text(pTB->pTL);

        size_t selectionStart;
        size_t selectionEnd;
        while (drgui_text_layout_find_next_no_loop(pTB->pTL, text, &selectionStart, &selectionEnd))
        {
            drgui_text_layout_select(pTB->pTL, selectionStart, selectionEnd);
            drgui_text_layout_move_cursor_to_end_of_selection(pTB->pTL);

            wasTextChanged = drgui_text_layout_delete_selected_text(pTB->pTL) || wasTextChanged;
            wasTextChanged = drgui_text_layout_insert_text_at_cursor(pTB->pTL, replacement) || wasTextChanged;
        }

        // The cursor may have moved so we'll need to restore it.
        size_t lineCharStart;
        size_t lineCharEnd;
        drgui_text_layout_get_line_character_range(pTB->pTL, originalCursorLine, &lineCharStart, &lineCharEnd);

        size_t newCursorPos = lineCharStart + originalCursorPos;
        if (newCursorPos > lineCharEnd) {
            newCursorPos = lineCharEnd;
        }
        drgui_text_layout_move_cursor_to_character(pTB->pTL, newCursorPos);
    }
    if (wasTextChanged) { drgui_text_layout_commit_undo_point(pTB->pTL); }


    // The scroll positions may have moved so we'll need to restore them.
    drgui_sb_scroll_to(pTB->pHorzScrollbar, originalScrollPosX);
    drgui_sb_scroll_to(pTB->pVertScrollbar, originalScrollPosY);

    return wasTextChanged;
}


void drgui_textbox_show_line_numbers(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_show(pTB->pLineNumbers);
    drgui_textbox__refresh_line_numbers(pTBElement);
}

void drgui_textbox_hide_line_numbers(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_hide(pTB->pLineNumbers);
    drgui_textbox__refresh_line_numbers(pTBElement);
}


void drgui_textbox_disable_vertical_scrollbar(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (pTB->isVertScrollbarEnabled) {
        pTB->isVertScrollbarEnabled = false;
        drgui_textbox__refresh_scrollbars(pTBElement);
    }
}

void drgui_textbox_enable_vertical_scrollbar(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (!pTB->isVertScrollbarEnabled) {
        pTB->isVertScrollbarEnabled = true;
        drgui_textbox__refresh_scrollbars(pTBElement);
    }
}

void drgui_textbox_disable_horizontal_scrollbar(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (pTB->isHorzScrollbarEnabled) {
        pTB->isHorzScrollbarEnabled = false;
        drgui_textbox__refresh_scrollbars(pTBElement);
    }
}

void drgui_textbox_enable_horizontal_scrollbar(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (!pTB->isHorzScrollbarEnabled) {
        pTB->isHorzScrollbarEnabled = true;
        drgui_textbox__refresh_scrollbars(pTBElement);
    }
}


void drgui_textbox_set_on_cursor_move(drgui_element* pTBElement, drgui_textbox_on_cursor_move_proc proc)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->onCursorMove = proc;
}

void drgui_textbox_set_on_undo_point_changed(drgui_element* pTBElement, drgui_textbox_on_undo_point_changed_proc proc)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    pTB->onUndoPointChanged = proc;
}


void drgui_textbox_on_size(drgui_element* pTBElement, float newWidth, float newHeight)
{
    (void)newWidth;
    (void)newHeight;

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    // The text layout needs to be resized.
    float containerWidth;
    float containerHeight;
    drgui_textbox__calculate_text_layout_container_size(pTBElement, &containerWidth, &containerHeight);
    drgui_text_layout_set_container_size(pTB->pTL, containerWidth, containerHeight);

    // Scrollbars need to be refreshed first.
    drgui_textbox__refresh_scrollbars(pTBElement);

    // Line numbers need to be refreshed.
    drgui_textbox__refresh_line_numbers(pTBElement);
}

void drgui_textbox_on_mouse_move(drgui_element* pTBElement, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)stateFlags;

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
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
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
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

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
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

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

}

void drgui_textbox_on_mouse_wheel(drgui_element* pTBElement, int delta, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_sb_scroll(pTB->pVertScrollbar, -delta * drgui_sb_get_mouse_wheel_scale(pTB->pVertScrollbar));
}

void drgui_textbox_on_key_down(drgui_element* pTBElement, drgui_key key, int stateFlags)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
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
        } break;

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
        } break;


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
        } break;

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
        } break;

        case DRGUI_ARROW_UP:
        {
            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (drgui_text_layout_is_anything_selected(pTB->pTL) && !drgui_text_layout_is_in_selection_mode(pTB->pTL)) {
                drgui_text_layout_deselect_all(pTB->pTL);
            }

            drgui_text_layout_move_cursor_up(pTB->pTL);

            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_leave_selection_mode(pTB->pTL);
            }
        } break;

        case DRGUI_ARROW_DOWN:
        {
            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (drgui_text_layout_is_anything_selected(pTB->pTL) && !drgui_text_layout_is_in_selection_mode(pTB->pTL)) {
                drgui_text_layout_deselect_all(pTB->pTL);
            }

            drgui_text_layout_move_cursor_down(pTB->pTL);

            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_leave_selection_mode(pTB->pTL);
            }
        } break;


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
        } break;

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
        } break;

        case DRGUI_PAGE_UP:
        {
            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (drgui_text_layout_is_anything_selected(pTB->pTL) && !drgui_text_layout_is_in_selection_mode(pTB->pTL)) {
                drgui_text_layout_deselect_all(pTB->pTL);
            }

            int scrollOffset = drgui_sb_get_page_size(pTB->pVertScrollbar);
            if ((stateFlags & DRGUI_KEY_STATE_CTRL_DOWN) == 0) {
                drgui_sb_scroll(pTB->pVertScrollbar, -scrollOffset);
            }

            drgui_text_layout_move_cursor_y(pTB->pTL, -drgui_sb_get_page_size(pTB->pVertScrollbar));

            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_leave_selection_mode(pTB->pTL);
            }
        } break;

        case DRGUI_PAGE_DOWN:
        {
            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_enter_selection_mode(pTB->pTL);
            }

            if (drgui_text_layout_is_anything_selected(pTB->pTL) && !drgui_text_layout_is_in_selection_mode(pTB->pTL)) {
                drgui_text_layout_deselect_all(pTB->pTL);
            }

            int scrollOffset = drgui_sb_get_page_size(pTB->pVertScrollbar);
            if (scrollOffset > (int)(drgui_text_layout_get_line_count(pTB->pTL) - drgui_text_layout_get_cursor_line(pTB->pTL))) {
                scrollOffset = 0;
            }

            if ((stateFlags & DRGUI_KEY_STATE_CTRL_DOWN) == 0) {
                drgui_sb_scroll(pTB->pVertScrollbar, scrollOffset);
            }

            drgui_text_layout_move_cursor_y(pTB->pTL, drgui_sb_get_page_size(pTB->pVertScrollbar));

            if ((stateFlags & DRGUI_KEY_STATE_SHIFT_DOWN) != 0) {
                drgui_text_layout_leave_selection_mode(pTB->pTL);
            }
        } break;

        default: break;
    }
}

void drgui_textbox_on_printable_key_down(drgui_element* pTBElement, unsigned int utf32, int stateFlags)
{
    (void)stateFlags;

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
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

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    float offsetX;
    float offsetY;
    drgui_textbox__get_text_offset(pTBElement, &offsetX, &offsetY);

    drgui_dirty(pTBElement, drgui_offset_rect(rect, offsetX, offsetY));
}

DRGUI_PRIVATE void drgui_textbox__on_text_layout_cursor_move(drgui_text_layout* pTL)
{
    // If the cursor is off the edge of the container we want to scroll it into position.
    drgui_element* pTBElement = *(drgui_element**)drgui_text_layout_get_extra_data(pTL);
    if (pTBElement == NULL) {
        return;
    }

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    // If the cursor is above or below the container, we need to scroll vertically.
    int iLine = (int)drgui_text_layout_get_cursor_line(pTB->pTL);
    if (iLine < drgui_sb_get_scroll_position(pTB->pVertScrollbar)) {
        drgui_sb_scroll_to(pTB->pVertScrollbar, iLine);
    }

    int iBottomLine = drgui_sb_get_scroll_position(pTB->pVertScrollbar) + drgui_sb_get_page_size(pTB->pVertScrollbar) - 1;
    if (iLine >= iBottomLine) {
        drgui_sb_scroll_to(pTB->pVertScrollbar, iLine - (drgui_sb_get_page_size(pTB->pVertScrollbar) - 1) + 1);
    }


    // If the cursor is to the left or right of the container we need to scroll horizontally.
    float cursorPosX;
    float cursorPosY;
    drgui_text_layout_get_cursor_position(pTB->pTL, &cursorPosX, &cursorPosY);

    if (cursorPosX < 0) {
        drgui_sb_scroll_to(pTB->pHorzScrollbar, (int)(cursorPosX - drgui_text_layout_get_inner_offset_x(pTB->pTL)) - 100);
    }
    if (cursorPosX >= drgui_text_layout_get_container_width(pTB->pTL)) {
        drgui_sb_scroll_to(pTB->pHorzScrollbar, (int)(cursorPosX - drgui_text_layout_get_inner_offset_x(pTB->pTL) - drgui_text_layout_get_container_width(pTB->pTL)) + 100);
    }


    if (pTB->onCursorMove) {
        pTB->onCursorMove(pTBElement);
    }
}

DRGUI_PRIVATE void drgui_textbox__on_text_layout_text_changed(drgui_text_layout* pTL)
{
    drgui_element* pTBElement = *(drgui_element**)drgui_text_layout_get_extra_data(pTL);
    if (pTBElement == NULL) {
        return;
    }

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    // Scrollbars need to be refreshed whenever text is changed.
    drgui_textbox__refresh_scrollbars(pTBElement);

    // The line numbers need to be redrawn.
    // TODO: This can probably be optimized a bit so that it is only redrawn if a line was inserted or deleted.
    drgui_dirty(pTB->pLineNumbers, drgui_get_local_rect(pTB->pLineNumbers));
}

DRGUI_PRIVATE void drgui_textbox__on_text_layout_undo_point_changed(drgui_text_layout* pTL, unsigned int iUndoPoint)
{
    drgui_element* pTBElement = *(drgui_element**)drgui_text_layout_get_extra_data(pTL);
    if (pTBElement == NULL) {
        return;
    }

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    if (pTB->onUndoPointChanged) {
        pTB->onUndoPointChanged(pTBElement, iUndoPoint);
    }
}


void drgui_textbox_on_paint(drgui_element* pTBElement, drgui_rect relativeRect, void* pPaintData)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_rect textRect = drgui_textbox__get_text_rect(pTBElement);

    // The dead space between the scrollbars should always be drawn with the default background color.
    drgui_draw_rect(pTBElement, drgui_textbox__get_scrollbar_dead_space_rect(pTBElement), drgui_text_layout_get_default_bg_color(pTB->pTL), pPaintData);

    // Border.
    drgui_rect borderRect = drgui_get_local_rect(pTBElement);
    drgui_draw_rect_outline(pTBElement, borderRect, pTB->borderColor, pTB->borderWidth, pPaintData);

    // Padding.
    drgui_rect paddingRect = drgui_grow_rect(textRect, pTB->padding);
    drgui_draw_rect_outline(pTBElement, paddingRect, drgui_text_layout_get_default_bg_color(pTB->pTL), pTB->padding, pPaintData);

    // Text.
    drgui_set_clip(pTBElement, drgui_clamp_rect(textRect, relativeRect), pPaintData);
    drgui_text_layout_paint(pTB->pTL, drgui_offset_rect(drgui_clamp_rect(textRect, relativeRect), -textRect.left, -textRect.top), pTBElement, pPaintData);
}

void drgui_textbox_on_capture_keyboard(drgui_element* pTBElement, drgui_element* pPrevCapturedElement)
{
    (void)pPrevCapturedElement;

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_show_cursor(pTB->pTL);
}

void drgui_textbox_on_release_keyboard(drgui_element* pTBElement, drgui_element* pNewCapturedElement)
{
    (void)pNewCapturedElement;

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_hide_cursor(pTB->pTL);
}

void drgui_textbox_on_capture_mouse(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

}

void drgui_textbox_on_release_mouse(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB == NULL) {
        return;
    }

    drgui_text_layout_leave_selection_mode(pTB->pTL);
}



DRGUI_PRIVATE void drgui_textbox__get_text_offset(drgui_element* pTBElement, float* pOffsetXOut, float* pOffsetYOut)
{
    float offsetX = 0;
    float offsetY = 0;

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB != NULL)
    {
        float lineNumbersWidth = 0;
        if (drgui_is_visible(pTB->pLineNumbers)) {
            lineNumbersWidth = drgui_get_width(pTB->pLineNumbers);
        }

        offsetX = pTB->borderWidth + pTB->padding + lineNumbersWidth;
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

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    if (pTB != NULL)
    {
        float horzScrollbarSize = 0;
        if (drgui_is_visible(pTB->pHorzScrollbar)) {
            horzScrollbarSize = drgui_get_height(pTB->pHorzScrollbar);
        }

        float vertScrollbarSize = 0;
        if (drgui_is_visible(pTB->pVertScrollbar)) {
            vertScrollbarSize = drgui_get_width(pTB->pVertScrollbar);
        }

        float lineNumbersWidth = 0;
        if (drgui_is_visible(pTB->pLineNumbers)) {
            lineNumbersWidth = drgui_get_width(pTB->pLineNumbers);
        }

        width  = drgui_get_width(pTBElement)  - (pTB->borderWidth + pTB->padding)*2 - vertScrollbarSize - lineNumbersWidth;
        height = drgui_get_height(pTBElement) - (pTB->borderWidth + pTB->padding)*2 - horzScrollbarSize;
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
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
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


DRGUI_PRIVATE void drgui_textbox__refresh_scrollbars(drgui_element* pTBElement)
{
    // The layout depends on the range because we may be dynamically hiding and showing the scrollbars depending on the range. Thus, we
    // refresh the range first. However, dynamically showing and hiding the scrollbars (which is done when the layout is refreshed) affects
    // the size of the text box, which in turn affects the range. Thus, we need to refresh the ranges a second time after the layouts.

    drgui_textbox__refresh_scrollbar_ranges(pTBElement);
    drgui_textbox__refresh_scrollbar_layouts(pTBElement);
    drgui_textbox__refresh_scrollbar_ranges(pTBElement);
}

DRGUI_PRIVATE void drgui_textbox__refresh_scrollbar_ranges(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    // The vertical scrollbar is based on the line count.
    size_t lineCount = drgui_text_layout_get_line_count(pTB->pTL);
    size_t pageSize  = drgui_text_layout_get_visible_line_count_starting_at(pTB->pTL, drgui_sb_get_scroll_position(pTB->pVertScrollbar));
    drgui_sb_set_range_and_page_size(pTB->pVertScrollbar, 0, (int)(lineCount + pageSize - 1 - 1), (int)pageSize);     // -1 to make the range 0 based. -1 to ensure at least one line is visible.

    if (drgui_sb_is_thumb_visible(pTB->pVertScrollbar)) {
        if (!drgui_is_visible(pTB->pVertScrollbar)) {
            drgui_show(pTB->pVertScrollbar);
        }
    } else {
        if (drgui_is_visible(pTB->pVertScrollbar)) {
            drgui_hide(pTB->pVertScrollbar);
        }
    }


    // The horizontal scrollbar is a per-pixel scrollbar, and is based on the width of the text versus the width of the container.
    drgui_rect textRect = drgui_text_layout_get_text_rect_relative_to_bounds(pTB->pTL);
    float containerWidth;
    drgui_text_layout_get_container_size(pTB->pTL, &containerWidth, NULL);
    drgui_sb_set_range_and_page_size(pTB->pHorzScrollbar, 0, (int)(textRect.right - textRect.left + (containerWidth/2)), (int)containerWidth);

    if (drgui_sb_is_thumb_visible(pTB->pHorzScrollbar)) {
        if (!drgui_is_visible(pTB->pHorzScrollbar)) {
            drgui_show(pTB->pHorzScrollbar);
            drgui_textbox__refresh_line_numbers(pTBElement);
        }
    } else {
        if (drgui_is_visible(pTB->pHorzScrollbar)) {
            drgui_hide(pTB->pHorzScrollbar);
            drgui_textbox__refresh_line_numbers(pTBElement);
        }
    }
}

DRGUI_PRIVATE void drgui_textbox__refresh_scrollbar_layouts(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    float offsetLeft   = pTB->borderWidth;
    float offsetTop    = pTB->borderWidth;
    float offsetRight  = pTB->borderWidth;
    float offsetBottom = pTB->borderWidth;

    float scrollbarSizeH = (drgui_sb_is_thumb_visible(pTB->pHorzScrollbar) && pTB->isHorzScrollbarEnabled) ? pTB->horzScrollbarSize : 0;
    float scrollbarSizeV = (drgui_sb_is_thumb_visible(pTB->pVertScrollbar) && pTB->isVertScrollbarEnabled) ? pTB->vertScrollbarSize : 0;

    drgui_set_size(pTB->pVertScrollbar, scrollbarSizeV, drgui_get_height(pTBElement) - scrollbarSizeH - (offsetTop + offsetBottom));
    drgui_set_size(pTB->pHorzScrollbar, drgui_get_width(pTBElement) - scrollbarSizeV - (offsetLeft + offsetRight), scrollbarSizeH);

    drgui_set_relative_position(pTB->pVertScrollbar, drgui_get_width(pTBElement) - scrollbarSizeV - offsetRight, offsetTop);
    drgui_set_relative_position(pTB->pHorzScrollbar, offsetLeft, drgui_get_height(pTBElement) - scrollbarSizeH - offsetBottom);


    // A change in the layout of the horizontal scrollbar will affect the layout of the line numbers.
    drgui_textbox__refresh_line_numbers(pTBElement);
}

DRGUI_PRIVATE drgui_rect drgui_textbox__get_scrollbar_dead_space_rect(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    float offsetLeft   = pTB->borderWidth;
    float offsetTop    = pTB->borderWidth;
    float offsetRight  = pTB->borderWidth;
    float offsetBottom = pTB->borderWidth;

    float scrollbarSizeH = (drgui_is_visible(pTB->pHorzScrollbar) && pTB->isHorzScrollbarEnabled) ? drgui_get_width(pTB->pHorzScrollbar) : 0;
    float scrollbarSizeV = (drgui_is_visible(pTB->pVertScrollbar) && pTB->isHorzScrollbarEnabled) ? drgui_get_height(pTB->pVertScrollbar) : 0;

    if (scrollbarSizeH == 0 && scrollbarSizeV == 0) {
        return drgui_make_rect(0, 0, 0, 0);
    }

    return drgui_make_rect(scrollbarSizeH + offsetLeft, scrollbarSizeV + offsetTop, drgui_get_width(pTBElement) - offsetRight, drgui_get_height(pTBElement) - offsetBottom);
}


DRGUI_PRIVATE void drgui_textbox__on_mouse_move_line_numbers(drgui_element* pLineNumbers, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;

    drgui_element* pTBElement = *(drgui_element**)drgui_get_extra_data(pLineNumbers);
    assert(pTBElement != NULL);

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    if ((stateFlags & DRGUI_MOUSE_BUTTON_LEFT_DOWN) != 0)
    {
        if (drgui_get_element_with_mouse_capture(pLineNumbers->pContext) == pLineNumbers)
        {
            // We just move the cursor around based on the line number we've moved over.
            drgui_text_layout_enter_selection_mode(pTB->pTL);
            {
                //float offsetX = pTextEditorData->padding;
                float offsetY = pTB->padding;
                size_t iLine = drgui_text_layout_get_line_at_pos_y(pTB->pTL, relativeMousePosY - offsetY);
                size_t iAnchorLine = pTB->iLineSelectAnchor;
                size_t lineCount = drgui_text_layout_get_line_count(pTB->pTL);

                size_t iSelectionFirstLine = drgui_text_layout_get_selection_first_line(pTB->pTL);
                size_t iSelectionLastLine = drgui_text_layout_get_selection_last_line(pTB->pTL);
                if (iSelectionLastLine != iSelectionFirstLine) {
                    iSelectionLastLine -= 1;
                }

                // If we're moving updwards we want to position the cursor at the start of the line. Otherwise we want to move the cursor to the start
                // of the next line, or the end of the text.
                bool movingUp = false;
                if (iLine < iAnchorLine) {
                    movingUp = true;
                }

                // If we're moving up the selection anchor needs to be placed at the end of the last line. Otherwise we need to move it to the start
                // of the first line.
                if (movingUp) {
                    if (iAnchorLine + 1 < lineCount) {
                        drgui_text_layout_move_selection_anchor_to_start_of_line(pTB->pTL, iAnchorLine + 1);
                    } else {
                        drgui_text_layout_move_selection_anchor_to_end_of_line(pTB->pTL, iAnchorLine);
                    }
                } else {
                    drgui_text_layout_move_selection_anchor_to_start_of_line(pTB->pTL, iAnchorLine);
                }


                // If we're moving up we want the cursor to be placed at the start of the selection range. Otherwise we want to place the cursor
                // at the end of the selection range.
                if (movingUp) {
                    drgui_text_layout_move_cursor_to_start_of_line_by_index(pTB->pTL, iLine);
                } else {
                    if (iLine + 1 < lineCount) {
                        drgui_text_layout_move_cursor_to_start_of_line_by_index(pTB->pTL, iLine + 1);
                    } else {
                        drgui_text_layout_move_cursor_to_end_of_line_by_index(pTB->pTL, iLine);
                    }
                }
            }
            drgui_text_layout_leave_selection_mode(pTB->pTL);
        }
    }
}

DRGUI_PRIVATE void drgui_textbox__on_mouse_button_down_line_numbers(drgui_element* pLineNumbers, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)stateFlags;

    drgui_element* pTBElement = *(drgui_element**)drgui_get_extra_data(pLineNumbers);
    assert(pTBElement != NULL);

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    if (mouseButton == DRGUI_MOUSE_BUTTON_LEFT)
    {
        //float offsetX = pTextEditorData->padding;
        float offsetY = pTB->padding;
        pTB->iLineSelectAnchor = drgui_text_layout_get_line_at_pos_y(pTB->pTL, relativeMousePosY - offsetY);

        drgui_text_layout_deselect_all(pTB->pTL);

        drgui_text_layout_move_cursor_to_start_of_line_by_index(pTB->pTL, pTB->iLineSelectAnchor);

        drgui_text_layout_enter_selection_mode(pTB->pTL);
        {
            if (pTB->iLineSelectAnchor + 1 < drgui_text_layout_get_line_count(pTB->pTL)) {
                drgui_text_layout_move_cursor_to_start_of_line_by_index(pTB->pTL, pTB->iLineSelectAnchor + 1);
            } else {
                drgui_text_layout_move_cursor_to_end_of_line(pTB->pTL);
            }
        }
        drgui_text_layout_leave_selection_mode(pTB->pTL);

        drgui_capture_mouse(pLineNumbers);
    }
}

DRGUI_PRIVATE void drgui_textbox__on_mouse_button_up_line_numbers(drgui_element* pLineNumbers, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags)
{
    (void)relativeMousePosX;
    (void)relativeMousePosY;
    (void)stateFlags;

    drgui_element* pTBElement = *(drgui_element**)drgui_get_extra_data(pLineNumbers);
    assert(pTBElement != NULL);

    if (mouseButton == DRGUI_MOUSE_BUTTON_LEFT)
    {
        drgui_release_mouse(pLineNumbers->pContext);
    }
}

DRGUI_PRIVATE void drgui_textbox__on_paint_rect_line_numbers(drgui_text_layout* pLayout, drgui_rect rect, drgui_color color, drgui_element* pTBElement, void* pPaintData)
{
    (void)pLayout;

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    float offsetX = pTB->padding;
    float offsetY = pTB->padding;

    drgui_draw_rect(pTB->pLineNumbers, drgui_offset_rect(rect, offsetX, offsetY), color, pPaintData);
}

DRGUI_PRIVATE void drgui_textbox__on_paint_text_line_numbers(drgui_text_layout* pLayout, drgui_text_run* pRun, drgui_element* pTBElement, void* pPaintData)
{
    (void)pLayout;

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    float offsetX = pTB->padding;
    float offsetY = pTB->padding;
    drgui_draw_text(pTB->pLineNumbers, pRun->pFont, pRun->text, (int)pRun->textLength, (float)pRun->posX + offsetX, (float)pRun->posY + offsetY, pRun->textColor, pRun->backgroundColor, pPaintData);
}

DRGUI_PRIVATE void drgui_textbox__on_paint_line_numbers(drgui_element* pLineNumbers, drgui_rect relativeRect, void* pPaintData)
{
    (void)relativeRect;

    drgui_element* pTBElement = *((drgui_element**)drgui_get_extra_data(pLineNumbers));
    assert(pTBElement != NULL);

    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    float lineNumbersWidth  = drgui_get_width(pLineNumbers) - (pTB->padding*2) - pTB->lineNumbersPaddingRight;
    float lineNumbersHeight = drgui_get_height(pLineNumbers) - (pTB->padding*2);

    drgui_text_layout_paint_line_numbers(pTB->pTL, lineNumbersWidth, lineNumbersHeight, drgui_rgb(80, 160, 192), drgui_textbox__on_paint_text_line_numbers, drgui_textbox__on_paint_rect_line_numbers, pTBElement, pPaintData);

    drgui_draw_rect_outline(pLineNumbers, drgui_get_local_rect(pLineNumbers), drgui_text_layout_get_default_bg_color(pTB->pTL), pTB->padding, pPaintData);

    // Right padding.
    drgui_rect rightPaddingRect = drgui_get_local_rect(pLineNumbers);
    rightPaddingRect.right -= pTB->padding;
    rightPaddingRect.left   = rightPaddingRect.right - pTB->lineNumbersPaddingRight;
    drgui_draw_rect(pLineNumbers, rightPaddingRect, drgui_text_layout_get_default_bg_color(pTB->pTL), pPaintData);
}

DRGUI_PRIVATE void drgui_textbox__refresh_line_numbers(drgui_element* pTBElement)
{
    drgui_textbox* pTB = (drgui_textbox*)drgui_get_extra_data(pTBElement);
    assert(pTB != NULL);

    float lineNumbersWidth = 0;
    if (drgui_is_visible(pTB->pLineNumbers)) {
        lineNumbersWidth = 64;
    }

    float scrollbarHeight = drgui_is_visible(pTB->pHorzScrollbar) ? drgui_get_height(pTB->pHorzScrollbar) : 0;
    drgui_set_size(pTB->pLineNumbers, lineNumbersWidth, drgui_get_height(pTBElement) - scrollbarHeight);


    // The size of the text container may have changed.
    float textEditorWidth;
    float textEditorHeight;
    drgui_textbox__calculate_text_layout_container_size(pTBElement, &textEditorWidth, &textEditorHeight);
    drgui_text_layout_set_container_size(pTB->pTL, textEditorWidth, textEditorHeight);


    // Force a redraw just to be sure everything is in a valid state.
    drgui_dirty(pTBElement, drgui_get_local_rect(pTBElement));
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
