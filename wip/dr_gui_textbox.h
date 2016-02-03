// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - By default the cursor/caret does not blink automatically. Instead, the application must "step" the text box by
//   calling drgui_textbox_step().
//

#ifndef drgui_textbox_h
#define drgui_textbox_h

#include <dr_libs/dr_gui.h>

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
