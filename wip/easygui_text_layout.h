// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - Text layouts are used to make it easier to manage the layout of a block of text.
// - Text layouts support basic editing which requires inbound events to be posted from the higher
//   level application.
// - Text layouts are not GUI elements. They are lower level objects that are used by higher level
//   GUI elements.
// - Text layouts normalize line endings to \n format. Keep this in mind when retrieving the text of
//   a layout.
// - Text layouts use the notion of a container which is used for determining which text runs are
//   visible.
//

#ifndef easygui_text_layout_h
#define easygui_text_layout_h

#include <easy_gui/easy_gui.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct easygui_text_layout easygui_text_layout;

typedef enum
{
    easygui_text_layout_alignment_left,
    easygui_text_layout_alignment_top,
    easygui_text_layout_alignment_center,
    easygui_text_layout_alignment_right,
    easygui_text_layout_alignment_bottom,
} easygui_text_layout_alignment;

typedef struct
{
    /// A pointer to the start of the string. This is NOT null terminated.
    const char* text;

    /// The length of the string, in bytes.
    size_t textLength;


    /// The font.
    easygui_font* pFont;

    /// The foreground color of the text.
    easygui_color textColor;

    /// The backgorund color of the text.
    easygui_color backgroundColor;


    /// The position to draw the text on the x axis.
    float posX;

    /// The position to draw the text on the y axis.
    float posY;

    /// The width of the run.
    float width;

    /// The height of the run.
    float height;


    // PROPERTIES BELOW ARE FOR INTERNAL USE ONLY

    /// Index of the line the run is placed on. For runs that are new line characters, this will represent the number of lines that came before it. For
    /// example, if this run represents the new-line character for the first line, this will be 0 and so on.
    PRIVATE unsigned int iLine;

    /// Index in the main text string of the first character of the run.
    PRIVATE unsigned int iChar;

    /// Index in the main text string of the character just past the last character in the run.
    PRIVATE unsigned int iCharEnd;

} easygui_text_run;

typedef void (* easygui_text_layout_on_paint_text_proc)        (easygui_text_layout* pTL, easygui_text_run* pRun, easygui_element* pElement, void* pPaintData);
typedef void (* easygui_text_layout_on_paint_rect_proc)        (easygui_text_layout* pTL, easygui_rect rect, easygui_color color, easygui_element* pElement, void* pPaintData);
typedef void (* easygui_text_layout_on_cursor_move_proc)       (easygui_text_layout* pTL);
typedef void (* easygui_text_layout_on_dirty_proc)             (easygui_text_layout* pTL, easygui_rect rect);
typedef void (* easygui_text_layout_on_text_changed_proc)      (easygui_text_layout* pTL);
typedef void (* easygui_text_layout_on_undo_point_changed_proc)(easygui_text_layout* pTL, unsigned int iUndoPoint);


/// Creates a new text layout object.
easygui_text_layout* easygui_create_text_layout(easygui_context* pContext, size_t extraDataSize, void* pExtraData);

/// Deletes the given text layout.
void easygui_delete_text_layout(easygui_text_layout* pTL);


/// Retrieves the size of the extra data associated with the given text layout.
size_t easygui_text_layout_get_extra_data_size(easygui_text_layout* pTL);

/// Retrieves a pointer to the extra data associated with the given text layout.
void* easygui_text_layout_get_extra_data(easygui_text_layout* pTL);


/// Sets the given text layout's text.
void easygui_text_layout_set_text(easygui_text_layout* pTL, const char* text);

/// Retrieves the given text layout's text.
///
/// @return The length of the string, not including the null terminator.
///
/// @remarks
///     Call this function with <textOut> set to NULL to retieve the required size of <textOut>.
size_t easygui_text_layout_get_text(easygui_text_layout* pTL, char* textOut, size_t textOutSize);


/// Sets the function to call when a region of the text layout needs to be redrawn.
void easygui_text_layout_set_on_dirty(easygui_text_layout* pTL, easygui_text_layout_on_dirty_proc proc);

/// Sets the function to call when the content of the given text layout has changed.
void easygui_text_layout_set_on_text_changed(easygui_text_layout* pTL, easygui_text_layout_on_text_changed_proc proc);

/// Sets the function to call when the content of the given text layout's current undo point has moved.
void easygui_text_layout_set_on_undo_point_changed(easygui_text_layout* pTL, easygui_text_layout_on_undo_point_changed_proc proc);


/// Sets the size of the container.
void easygui_text_layout_set_container_size(easygui_text_layout* pTL, float containerWidth, float containerHeight);

/// Retrieves the size of the container.
void easygui_text_layout_get_container_size(easygui_text_layout* pTL, float* pContainerWidthOut, float* pContainerHeightOut);

/// Retrieves the width of the container.
float easygui_text_layout_get_container_width(easygui_text_layout* pTL);

/// Retrieves the height of the container.
float easygui_text_layout_get_container_height(easygui_text_layout* pTL);


/// Sets the inner offset of the given text layout.
void easygui_text_layout_set_inner_offset(easygui_text_layout* pTL, float innerOffsetX, float innerOffsetY);

/// Sets the inner offset of the given text layout on the x axis.
void easygui_text_layout_set_inner_offset_x(easygui_text_layout* pTL, float innerOffsetX);

/// Sets the inner offset of the given text layout on the y axis.
void easygui_text_layout_set_inner_offset_y(easygui_text_layout* pTL, float innerOffsetY);

/// Retrieves the inner offset of the given text layout.
void easygui_text_layout_get_inner_offset(easygui_text_layout* pTL, float* pInnerOffsetX, float* pInnerOffsetY);

/// Retrieves the inner offset of the given text layout on the x axis.
float easygui_text_layout_get_inner_offset_x(easygui_text_layout* pTL);

/// Retrieves the inner offset of the given text layout on the x axis.
float easygui_text_layout_get_inner_offset_y(easygui_text_layout* pTL);


/// Sets the default font to use for text runs.
void easygui_text_layout_set_default_font(easygui_text_layout* pTL, easygui_font* pFont);

/// Retrieves the default font to use for text runs.
easygui_font* easygui_text_layout_get_default_font(easygui_text_layout* pTL);

/// Sets the default text color of the given text layout.
void easygui_text_layout_set_default_text_color(easygui_text_layout* pTL, easygui_color color);

/// Retrieves the default text color of the given text layout.
easygui_color easygui_text_layout_get_default_text_color(easygui_text_layout* pTL);

/// Sets the default background color of the given text layout.
void easygui_text_layout_set_default_bg_color(easygui_text_layout* pTL, easygui_color color);

/// Retrieves the default background color of the given text layout.
easygui_color easygui_text_layout_get_default_bg_color(easygui_text_layout* pTL);

/// Sets the background color of the line the cursor is sitting on.
void easygui_text_layout_set_active_line_bg_color(easygui_text_layout* pTL, easygui_color color);

/// Retrieves the background color of the line the cursor is sitting on.
easygui_color easygui_text_layout_get_active_line_bg_color(easygui_text_layout* pTL);


/// Sets the size of a tab in spaces.
void easygui_text_layout_set_tab_size(easygui_text_layout* pTL, unsigned int sizeInSpaces);

/// Retrieves the size of a tab in spaces.
unsigned int easygui_text_layout_get_tab_size(easygui_text_layout* pTL);


/// Sets the horizontal alignment of the given text layout.
void easygui_text_layout_set_horizontal_align(easygui_text_layout* pTL, easygui_text_layout_alignment alignment);

/// Retrieves the horizontal aligment of the given text layout.
easygui_text_layout_alignment easygui_text_layout_get_horizontal_align(easygui_text_layout* pTL);

/// Sets the vertical alignment of the given text layout.
void easygui_text_layout_set_vertical_align(easygui_text_layout* pTL, easygui_text_layout_alignment alignment);

/// Retrieves the vertical aligment of the given text layout.
easygui_text_layout_alignment easygui_text_layout_get_vertical_align(easygui_text_layout* pTL);


/// Retrieves the rectangle of the text relative to the bounds, taking alignment into account.
easygui_rect easygui_text_layout_get_text_rect_relative_to_bounds(easygui_text_layout* pTL);


/// Sets the width of the text cursor.
void easygui_text_layout_set_cursor_width(easygui_text_layout* pTL, float cursorWidth);

/// Retrieves the width of the text cursor.
float easygui_text_layout_get_cursor_width(easygui_text_layout* pTL);

/// Sets the color of the text cursor.
void easygui_text_layout_set_cursor_color(easygui_text_layout* pTL, easygui_color cursorColor);

/// Retrieves the color of the text cursor.
easygui_color easygui_text_layout_get_cursor_color(easygui_text_layout* pTL);

/// Sets the blink rate of the cursor in milliseconds.
void easygui_text_layout_set_cursor_blink_rate(easygui_text_layout* pTL, unsigned int blinkRateInMilliseconds);

/// Retrieves the blink rate of the cursor in milliseconds.
unsigned int easygui_text_layout_get_cursor_blink_rate(easygui_text_layout* pTL);

/// Shows the cursor.
void easygui_text_layout_show_cursor(easygui_text_layout* pTL);

/// Hides the cursor.
void easygui_text_layout_hide_cursor(easygui_text_layout* pTL);

/// Determines whether or not the cursor is visible.
bool easygui_text_layout_is_showing_cursor(easygui_text_layout* pTL);

/// Moves the cursor to the closest character based on the given input position.
void easygui_text_layout_move_cursor_to_point(easygui_text_layout* pTL, float posX, float posY);

/// Retrieves the position of the cursor, relative to the container.
void easygui_text_layout_get_cursor_position(easygui_text_layout* pTL, float* pPosXOut, float* pPosYOut);

/// Retrieves the rectangle of the cursor, relative to the container.
easygui_rect easygui_text_layout_get_cursor_rect(easygui_text_layout* pTL);

/// Retrieves the index of the line the cursor is currently sitting on.
unsigned int easygui_text_layout_get_cursor_line(easygui_text_layout* pTL);

/// Retrieves the index of the column the cursor is currently sitting on.
unsigned int easygui_text_layout_get_cursor_column(easygui_text_layout* pTL);

/// Retrieves the index of the character the cursor is currently sitting on.
unsigned int easygui_text_layout_get_cursor_character(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout to the left by one character.
bool easygui_text_layout_move_cursor_left(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout to the right by one character.
bool easygui_text_layout_move_cursor_right(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout up one line.
bool easygui_text_layout_move_cursor_up(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout down one line.
bool easygui_text_layout_move_cursor_down(easygui_text_layout* pTL);

/// Moves the cursor up or down the given number of lines.
bool easygui_text_layout_move_cursor_y(easygui_text_layout* pTL, int amount);

/// Moves the cursor of the given text layout to the end of the line.
bool easygui_text_layout_move_cursor_to_end_of_line(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout to the start of the line.
bool easygui_text_layout_move_cursor_to_start_of_line(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout to the end of the line at the given index.
bool easygui_text_layout_move_cursor_to_end_of_line_by_index(easygui_text_layout* pTL, unsigned int iLine);

/// Moves the cursor of the given text layout to the start of the line at the given index.
bool easygui_text_layout_move_cursor_to_start_of_line_by_index(easygui_text_layout* pTL, unsigned int iLine);

/// Moves the cursor of the given text layout to the end of the text.
bool easygui_text_layout_move_cursor_to_end_of_text(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout to the end of the text.
bool easygui_text_layout_move_cursor_to_start_of_text(easygui_text_layout* pTL);

/// Moves the cursor to the start of the selected text.
void easygui_text_layout_move_cursor_to_start_of_selection(easygui_text_layout* pTL);

/// Moves the cursor to the end of the selected text.
void easygui_text_layout_move_cursor_to_end_of_selection(easygui_text_layout* pTL);

/// Moves the cursor to the given character index.
void easygui_text_layout_move_cursor_to_character(easygui_text_layout* pTL, unsigned int characterIndex);

/// Determines whether or not the cursor is sitting at the start of the selection.
bool easygui_text_layout_is_cursor_at_start_of_selection(easygui_text_layout* pTL);

/// Determines whether or not the cursor is sitting at the end fo the selection.
bool easygui_text_layout_is_cursor_at_end_of_selection(easygui_text_layout* pTL);

/// Swaps the position of the cursor based on the current selection.
void easygui_text_layout_swap_selection_markers(easygui_text_layout* pTL);

/// Sets the function to call when the cursor in the given text layout is mvoed.
void easygui_text_layout_set_on_cursor_move(easygui_text_layout* pTL, easygui_text_layout_on_cursor_move_proc proc);


/// Inserts a character into the given text layout.
///
/// @return True if the text within the text layout has changed.
bool easygui_text_layout_insert_character(easygui_text_layout* pTL, unsigned int character, unsigned int insertIndex);

/// Inserts the given string at the given character index.
///
/// @return True if the text within the text layout has changed.
bool easygui_text_layout_insert_text(easygui_text_layout* pTL, const char* text, unsigned int insertIndex);

/// Deletes a range of text in the given text layout.
///
/// @return True if the text within the text layout has changed.
bool easygui_text_layout_delete_text_range(easygui_text_layout* pTL, unsigned int iFirstCh, unsigned int iLastChPlus1);

/// Inserts a character at the position of the cursor.
///
/// @return True if the text within the text layout has changed.
bool easygui_text_layout_insert_character_at_cursor(easygui_text_layout* pTL, unsigned int character);

/// Inserts a character at the position of the cursor.
///
/// @return True if the text within the text layout has changed.
bool easygui_text_layout_insert_text_at_cursor(easygui_text_layout* pTL, const char* text);

/// Deletes the character to the left of the cursor.
///
/// @return True if the text within the text layout has changed.
bool easygui_text_layout_delete_character_to_left_of_cursor(easygui_text_layout* pTL);

/// Deletes the character to the right of the cursor.
///
/// @return True if the text within the text layout has changed.
bool easygui_text_layout_delete_character_to_right_of_cursor(easygui_text_layout* pTL);

/// Deletes the currently selected text.
///
/// @return True if the text within the text layout has changed.
bool easygui_text_layout_delete_selected_text(easygui_text_layout* pTL);


/// Enter's into selection mode.
///
/// @remarks
///     An application will typically enter selection mode when the Shift key is pressed, and then leave when the key is released.
///     @par
///     This will increment an internal counter, which is decremented with a corresponding call to easygui_text_layout_leave_selection_mode().
///     Selection mode will be enabled so long as this counter is greater than 0. Thus, you must ensure you cleanly leave selection
///     mode.
void easygui_text_layout_enter_selection_mode(easygui_text_layout* pTL);

/// Leaves selection mode.
///
/// @remarks
///     This decrements the internal counter. Selection mode will not be disabled while this reference counter is greater than 0. Always
///     ensure a leave is correctly matched with an enter.
void easygui_text_layout_leave_selection_mode(easygui_text_layout* pTL);

/// Determines whether or not the given text layout is in selection mode.
bool easygui_text_layout_is_in_selection_mode(easygui_text_layout* pTL);

/// Determines whether or not anything is selected in the given text layout.
bool easygui_text_layout_is_anything_selected(easygui_text_layout* pTL);

/// Deselects everything in the given text layout.
void easygui_text_layout_deselect_all(easygui_text_layout* pTL);

/// Selects everything in the given text layout.
void easygui_text_layout_select_all(easygui_text_layout* pTL);

/// Selects the given range of text.
void easygui_text_layout_select(easygui_text_layout* pTL, unsigned int firstCharacter, unsigned int lastCharacter);

/// Retrieves a copy of the selected text.
///
/// @remarks
///     This returns the length of the selected text. Call this once with <textOut> set to NULL to calculate the required size of the
///     buffer.
///     @par
///     If the output buffer is not larger enough, the string will be truncated.
size_t easygui_text_layout_get_selected_text(easygui_text_layout* pTL, char* textOut, size_t textOutLength);

/// Retrieves the index of the first line of the current selection.
unsigned int easygui_text_layout_get_selection_first_line(easygui_text_layout* pTL);

/// Retrieves the index of the last line of the current selection.
unsigned int easygui_text_layout_get_selection_last_line(easygui_text_layout* pTL);

/// Moves the selection anchor to the end of the given line.
void easygui_text_layout_move_selection_anchor_to_end_of_line(easygui_text_layout* pTL, unsigned int iLine);

/// Moves the selection anchor to the start of the given line.
void easygui_text_layout_move_selection_anchor_to_start_of_line(easygui_text_layout* pTL, unsigned int iLine);

/// Retrieves the line the selection anchor is sitting on.
unsigned int easygui_text_layout_get_selection_anchor_line(easygui_text_layout* pTL);


/// Prepares the next undo/redo point.
///
/// @remarks
///     This captures the state that will be applied when the undo/redo point is undone.
bool easygui_text_layout_prepare_undo_point(easygui_text_layout* pTL);

/// Creates a snapshot of the current state of the text layout and pushes it to the top of the undo/redo stack.
bool easygui_text_layout_commit_undo_point(easygui_text_layout* pTL);

/// Performs an undo operation.
bool easygui_text_layout_undo(easygui_text_layout* pTL);

/// Performs a redo operation.
bool easygui_text_layout_redo(easygui_text_layout* pTL);

/// Retrieves the number of undo points remaining in the stack.
unsigned int easygui_text_layout_get_undo_points_remaining_count(easygui_text_layout* pTL);

/// Retrieves the number of redo points remaining in the stack.
unsigned int easygui_text_layout_get_redo_points_remaining_count(easygui_text_layout* pTL);

/// Clears the undo stack.
void easygui_text_layout_clear_undo_stack(easygui_text_layout* pTL);



/// Retrieves the number of lines in the given text layout.
unsigned int easygui_text_layout_get_line_count(easygui_text_layout* pTL);

/// Retrieves the number of lines that can fit on the visible portion of the layout, starting from the given line.
///
/// @remarks
///     Use this for controlling the page size for scrollbars.
unsigned int easygui_text_layout_get_visible_line_count_starting_at(easygui_text_layout* pTL, unsigned int iFirstLine);

/// Retrieves the position of the line at the given index on the y axis.
///
/// @remarks
///     Use this for calculating the inner offset for scrolling on the y axis.
float easygui_text_layout_get_line_pos_y(easygui_text_layout* pTL, unsigned int iLine);

/// Finds the line under the given point on the y axis relative to the container.
unsigned int easygui_text_layout_get_line_at_pos_y(easygui_text_layout* pTL, float posY);


/// Sets the function to call when a run of text needs to be painted for the given text layout.
void easygui_text_layout_set_on_paint_text(easygui_text_layout* pTL, easygui_text_layout_on_paint_text_proc proc);

/// Sets the function to call when a quad needs to the be painted for the given text layout.
void easygui_text_layout_set_on_paint_rect(easygui_text_layout* pTL, easygui_text_layout_on_paint_rect_proc proc);

/// Paints the given text layout by calling the appropriate painting callbacks.
///
/// @remarks
///     Typically a text layout will be painted to a GUI element. A pointer to an element can be passed to this function
///     which will be passed to the callback functions. This is purely for convenience and nothing is actually drawn to
///     the element outside of the callback functions.
void easygui_text_layout_paint(easygui_text_layout* pTL, easygui_rect rect, easygui_element* pElement, void* pPaintData);


/// Steps the given text layout by the given number of milliseconds.
///
/// @remarks
///     This will trigger the on_dirty callback when the cursor switches it's blink states.
void easygui_text_layout_step(easygui_text_layout* pTL, unsigned int milliseconds);


/// Calls the given painting callbacks for the line numbers of the given text layout.
void easygui_text_layout_paint_line_numbers(easygui_text_layout* pTL, float lineNumbersWidth, float lineNumbersHeight, easygui_color textColor, easygui_text_layout_on_paint_text_proc onPaintText, easygui_text_layout_on_paint_rect_proc onPaintRect, easygui_element* pElement, void* pPaintData);


/// Finds the given string starting from the cursor and then looping back.
bool easygui_text_layout_find_next(easygui_text_layout* pTL, const char* text, unsigned int* pSelectionStartOut, unsigned int* pSelectionEndOut);

/// Finds the given string starting from the cursor, but does not loop back.
bool easygui_text_layout_find_next_no_loop(easygui_text_layout* pTL, const char* text, unsigned int* pSelectionStartOut, unsigned int* pSelectionEndOut);



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
