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

typedef void (* easygui_text_layout_run_iterator_proc)(easygui_text_layout* pLayout, easygui_text_run* pRun, void* pUserData);



/// Creates a new text layout object.
easygui_text_layout* easygui_create_text_layout(easygui_context* pContext, size_t extraDataSize, void* pExtraData);

/// Deletes the given text layout.
void easygui_delete_text_layout(easygui_text_layout* pTL);


/// Retrieves the size of the extra data associated with the given text layout.
size_t easygui_get_text_layout_extra_data_size(easygui_text_layout* pTL);

/// Retrieves a pointer to the extra data associated with the given text layout.
void* easygui_get_text_layout_extra_data(easygui_text_layout* pTL);


/// Sets the given text layout's text.
void easygui_set_text_layout_text(easygui_text_layout* pTL, const char* text);

/// Retrieves the given text layout's text.
///
/// @return The length of the string, not including the null terminator.
///
/// @remarks
///     Call this function with <textOut> set to NULL to retieve the required size of <textOut>.
size_t easygui_get_text_layout_text(easygui_text_layout* pTL, char* textOut, size_t textOutSize);


/// Sets the size of the container.
void easygui_set_text_layout_container_size(easygui_text_layout* pTL, float containerWidth, float containerHeight);

/// Retrieves the size of the container.
void easygui_get_text_layout_container_size(easygui_text_layout* pTL, float* pContainerWidthOut, float* pContainerHeightOut);


/// Sets the inner offset of the given text layout.
void easygui_set_text_layout_inner_offset(easygui_text_layout* pTL, float innerOffsetX, float innerOffsetY);

/// Sets the inner offset of the given text layout on the x axis.
void easygui_set_text_layout_inner_offset_x(easygui_text_layout* pTL, float innerOffsetX);

/// Sets the inner offset of the given text layout on the y axis.
void easygui_set_text_layout_inner_offset_y(easygui_text_layout* pTL, float innerOffsetY);

/// Retrieves the inner offset of the given text layout.
void easygui_get_text_layout_inner_offset(easygui_text_layout* pTL, float* pInnerOffsetX, float* pInnerOffsetY);


/// Sets the default font to use for text runs.
void easygui_set_text_layout_default_font(easygui_text_layout* pTL, easygui_font* pFont);

/// Retrieves the default font to use for text runs.
easygui_font* easygui_get_text_layout_default_font(easygui_text_layout* pTL);

/// Sets the default text color of the given text layout.
void easygui_set_text_layout_default_text_color(easygui_text_layout* pTL, easygui_color color);

/// Retrieves the default text color of the given text layout.
easygui_color easygui_get_text_layout_default_text_color(easygui_text_layout* pTL);

/// Sets the default background color of the given text layout.
void easygui_set_text_layout_default_bg_color(easygui_text_layout* pTL, easygui_color color);

/// Retrieves the default background color of the given text layout.
easygui_color easygui_get_text_layout_default_bg_color(easygui_text_layout* pTL);


/// Sets the size of a tab in spaces.
void easygui_set_text_layout_tab_size(easygui_text_layout* pTL, unsigned int sizeInSpaces);

/// Retrieves the size of a tab in spaces.
unsigned int easygui_get_text_layout_tab_size(easygui_text_layout* pTL);


/// Sets the horizontal alignment of the given text layout.
void easygui_set_text_layout_horizontal_align(easygui_text_layout* pTL, easygui_text_layout_alignment alignment);

/// Retrieves the horizontal aligment of the given text layout.
easygui_text_layout_alignment easygui_get_text_layout_horizontal_align(easygui_text_layout* pTL);

/// Sets the vertical alignment of the given text layout.
void easygui_set_text_layout_vertical_align(easygui_text_layout* pTL, easygui_text_layout_alignment alignment);

/// Retrieves the vertical aligment of the given text layout.
easygui_text_layout_alignment easygui_get_text_layout_vertical_align(easygui_text_layout* pTL);


/// Retrieves the rectangle of the text relative to the bounds, taking alignment into account.
easygui_rect easygui_get_text_layout_text_rect_relative_to_bounds(easygui_text_layout* pTL);


/// Sets the width of the text cursor.
void easygui_set_text_layout_cursor_width(easygui_text_layout* pTL, float cursorWidth);

/// Retrieves the width of the text cursor.
float easygui_get_text_layout_cursor_width(easygui_text_layout* pTL);

/// Sets the color of the text cursor.
void easygui_set_text_layout_cursor_color(easygui_text_layout* pTL, easygui_color cursorColor);

/// Retrieves the color of the text cursor.
easygui_color easygui_get_text_layout_cursor_color(easygui_text_layout* pTL);

/// Moves the cursor to the closest character based on the given input position.
void easygui_move_text_layout_cursor_to_point(easygui_text_layout* pTL, float posX, float posY);

/// Retrieves the position of the cursor, relative to the container.
void easygui_get_text_layout_cursor_position(easygui_text_layout* pTL, float* pPosXOut, float* pPosYOut);

/// Retrieves the rectangle of the cursor, relative to the container.
easygui_rect easygui_get_text_layout_cursor_rect(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout to the left by one character.
bool easygui_move_text_layout_cursor_left(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout to the right by one character.
bool easygui_move_text_layout_cursor_right(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout up one line.
bool easygui_move_text_layout_cursor_up(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout down one line.
bool easygui_move_text_layout_cursor_down(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout to the end of the line.
bool easygui_move_text_layout_cursor_to_end_of_line(easygui_text_layout* pTL);

/// Moves the cursor of the given text layout to the start of the line.
bool easygui_move_text_layout_cursor_to_start_of_line(easygui_text_layout* pTL);


/// Inserts a character into the given text layout.
void easygui_insert_character_into_text_layout(easygui_text_layout* pTL, unsigned int character, unsigned int insertIndex);

/// Inserts a character at the position of the cursor.
void easygui_insert_character_at_cursor(easygui_text_layout* pTL, unsigned int character);

/// Deletes the character to the left of the cursor.
void easygui_delete_character_to_left_of_cursor(easygui_text_layout* pTL);

/// Deletes the character to the right of the cursor.
void easygui_delete_character_to_right_of_cursor(easygui_text_layout* pTL);

/// Deletes the currently selected text.
void easygui_delete_selected_text(easygui_text_layout* pTL);


/// Enter's into selection mode.
///
/// @remarks
///     An application will typically enter selection mode when the Shift key is pressed, and then leave when the key is released.
///     @par
///     This will increment an internal counter, which is decremented with a corresponding call to easygui_leave_selection_mode().
///     Selection mode will be enabled so long as this counter is greater than 0. Thus, you must ensure you cleanly leave selection
///     mode.
void easygui_enter_selection_mode(easygui_text_layout* pTL);

/// Leaves selection mode.
///
/// @remarks
///     This decrements the internal counter. Selection mode will not be disabled while this reference counter is greater than 0. Always
///     ensure a leave is correctly matched with an enter.
void easygui_leave_selection_mode(easygui_text_layout* pTL);

/// Determines whether or not the given text layout is in selection mode.
bool easygui_is_text_layout_in_selection_mode(easygui_text_layout* pTL);

/// Determines whether or not anything is selected in the given text layout.
bool easygui_is_anything_selected_in_text_layout(easygui_text_layout* pTL);

/// Deselects everything in the given text layout.
void easygui_deselect_all_in_text_layout(easygui_text_layout* pTL);


/// Retrieves the number of lines in the given text layout.
unsigned int easygui_get_text_layout_line_count(easygui_text_layout* pTL);

/// Retrieves the number of lines that can fit on the visible portion of the layout, starting from the given line.
///
/// @remarks
///     Use this for controlling the page size for scrollbars.
unsigned int easygui_get_visible_line_count_starting_at(easygui_text_layout* pTL, unsigned int iFirstLine);

/// Retrieves the position of the line at the given index on the y axis.
///
/// @remarks
///     Use this for calculating the inner offset for scrolling on the y axis.
float easygui_get_text_layout_line_pos_y(easygui_text_layout* pTL, unsigned int iLine);


/// Iterates over every visible text run in the given text layout.
void easygui_iterate_visible_text_runs(easygui_text_layout* pTL, easygui_text_layout_run_iterator_proc callback, void* pUserData);


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
