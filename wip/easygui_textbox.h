// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - By default the cursor/caret does not blink automatically. Instead, the application must "step" the text box by
//   calling easygui_textbox_step().
//

#ifndef easygui_textbox_h
#define easygui_textbox_h

#include <easy_gui/easy_gui.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Creates a new text box control.
easygui_element* easygui_create_textbox(easygui_context* pContext, easygui_element* pParent, size_t extraDataSize, const void* pExtraData);

/// Deletest the given text box control.
void easygui_delete_textbox(easygui_element* pTBElement);


/// Retrieves the size of the extra data associated with the given text box.
size_t easygui_textbox_get_extra_data_size(easygui_element* pTBElement);

/// Retrieves a pointer to the extra data associated with the given text box.
void* easygui_textbox_get_extra_data(easygui_element* pTBElement);


/// Sets the font to use with the given text box.
void easygui_textbox_set_font(easygui_element* pTBElement, easygui_font* pFont);

/// Sets the color of the text in teh given text box.
void easygui_textbox_set_text_color(easygui_element* pTBElement, easygui_color color);

/// Sets the background color of the given text box.
void easygui_textbox_set_background_color(easygui_element* pTBElement, easygui_color color);

/// Sets the background color for the line the caret is currently sitting on.
void easygui_textbox_set_active_line_background_color(easygui_element* pTBElement, easygui_color color);

/// Sets the color of the cursor of the given text box.
void easygui_textbox_set_cursor_color(easygui_element* pTBElement, easygui_color color);

/// Sets the border color of the given text box.
void easygui_textbox_set_border_color(easygui_element* pTBElement, easygui_color color);

/// Sets the border width of the given text box.
void easygui_textbox_set_border_width(easygui_element* pTBElement, float borderWidth);

/// Sets the amount of padding to apply to given text box.
void easygui_textbox_set_padding(easygui_element* pTBElement, float padding);


/// Sets the text of the given text box.
void easygui_textbox_set_text(easygui_element* pTBElement, const char* text);

/// Retrieves the text of the given text box.
size_t easygui_textbox_get_text(easygui_element* pTBElement, char* pTextOut, size_t textOutSize);

/// Steps the text box to allow it to blink the cursor.
void easygui_textbox_step(easygui_element* pTBElement, unsigned int milliseconds);

/// Sets the blink rate of the cursor in milliseconds.
void easygui_textbox_set_cursor_blink_rate(easygui_element* pTBElement, unsigned int blinkRateInMilliseconds);


/// on_size.
void easygui_textbox_on_size(easygui_element* pTBElement, float newWidth, float newHeight);

/// on_mouse_move.
void easygui_textbox_on_mouse_move(easygui_element* pTBElement, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// on_mouse_button_down.
void easygui_textbox_on_mouse_button_down(easygui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// on_mouse_button_up.
void easygui_textbox_on_mouse_button_up(easygui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// on_mouse_button_dblclick.
void easygui_textbox_on_mouse_button_dblclick(easygui_element* pTBElement, int mouseButton, int relativeMousePosX, int relativeMousePosY, int stateFlags);

/// on_key_down.
void easygui_textbox_on_key_down(easygui_element* pTBElement, easygui_key key, int stateFlags);

/// on_printable_key_down.
void easygui_textbox_on_printable_key_down(easygui_element* pTBElement, unsigned int utf32, int stateFlags);

/// on_paint.
void easygui_textbox_on_paint(easygui_element* pTBElement, easygui_rect relativeRect, void* pPaintData);

/// on_capture_keyboard
void easygui_textbox_on_capture_keyboard(easygui_element* pTBElement, easygui_element* pPrevCapturedElement);

/// on_release_keyboard
void easygui_textbox_on_release_keyboard(easygui_element* pTBElement, easygui_element* pNewCapturedElement);

/// on_capture_mouse
void easygui_textbox_on_capture_mouse(easygui_element* pTBElement);

/// on_release_mouse
void easygui_textbox_on_release_mouse(easygui_element* pTBElement);



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
