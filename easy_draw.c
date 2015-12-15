// Public Domain. See "unlicense" statement at the end of this file.

#include "easy_draw.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef PRIVATE
#define PRIVATE
#endif


easy2d_context* easy2d_create_context(easy2d_drawing_callbacks drawingCallbacks, size_t contextExtraBytes, size_t surfaceExtraBytes, size_t fontExtraBytes)
{
    easy2d_context* pContext = (easy2d_context*)malloc(sizeof(easy2d_context) - sizeof(pContext->pExtraData) + contextExtraBytes);
    if (pContext != NULL)
    {
        pContext->drawingCallbacks  = drawingCallbacks;
        pContext->fontExtraBytes    = fontExtraBytes;
        pContext->surfaceExtraBytes = surfaceExtraBytes;
        pContext->contextExtraBytes = contextExtraBytes;
        memset(pContext->pExtraData, 0, contextExtraBytes);

        // The create_context callback is optional. If it is set to NULL, we just finish up here and return successfully. Otherwise
        // we want to call the create_context callback and check it's return value. If it's return value if false it means there
        // was an error and we need to return null.
        if (pContext->drawingCallbacks.on_create_context != NULL)
        {
            if (!pContext->drawingCallbacks.on_create_context(pContext))
            {
                // An error was thrown from the callback.
                free(pContext);
                pContext = NULL;
            }
        }
    }

    return pContext;
}

void easy2d_delete_context(easy2d_context* pContext)
{
    if (pContext != NULL) {
        if (pContext->drawingCallbacks.on_delete_context != NULL) {
            pContext->drawingCallbacks.on_delete_context(pContext);
        }

        free(pContext);
    }
}

void* easy2d_get_context_extra_data(easy2d_context* pContext)
{
    return pContext->pExtraData;
}


easy2d_surface* easy2d_create_surface(easy2d_context* pContext, float width, float height)
{
    if (pContext != NULL)
    {
        easy2d_surface* pSurface = (easy2d_surface*)malloc(sizeof(easy2d_surface) - sizeof(pContext->pExtraData) + pContext->surfaceExtraBytes);
        if (pSurface != NULL)
        {
            pSurface->pContext = pContext;
            pSurface->width    = width;
            pSurface->height   = height;
            memset(pSurface->pExtraData, 0, pContext->surfaceExtraBytes);

            // The create_surface callback is optional. If it is set to NULL, we just finish up here and return successfully. Otherwise
            // we want to call the create_surface callback and check it's return value. If it's return value if false it means there
            // was an error and we need to return null.
            if (pContext->drawingCallbacks.on_create_surface != NULL)
            {
                if (!pContext->drawingCallbacks.on_create_surface(pSurface, width, height))
                {
                    free(pSurface);
                    pSurface = NULL;
                }
            }
        }

        return pSurface;
    }

    return NULL;
}

void easy2d_delete_surface(easy2d_surface* pSurface)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.on_delete_surface != NULL) {
            pSurface->pContext->drawingCallbacks.on_delete_surface(pSurface);
        }

        free(pSurface);
    }
}

float easy2d_get_surface_width(const easy2d_surface* pSurface)
{
    if (pSurface != NULL) {
        return pSurface->width;
    }

    return 0;
}

float easy2d_get_surface_height(const easy2d_surface* pSurface)
{
    if (pSurface != NULL) {
        return pSurface->height;
    }

    return 0;
}

void* easy2d_get_surface_extra_data(easy2d_surface* pSurface)
{
    if (pSurface != NULL) {
        return pSurface->pExtraData;
    }

    return NULL;
}


void easy2d_begin_draw(easy2d_surface* pSurface)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.begin_draw != NULL) {
            pSurface->pContext->drawingCallbacks.begin_draw(pSurface);
        }
    }
}

void easy2d_end_draw(easy2d_surface* pSurface)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.end_draw != NULL) {
            pSurface->pContext->drawingCallbacks.end_draw(pSurface);
        }
    }
}

void easy2d_clear(easy2d_surface * pSurface, easy2d_color color)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.clear != NULL) {
            pSurface->pContext->drawingCallbacks.clear(pSurface, color);
        }
    }
}

void easy2d_draw_rect(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_rect != NULL) {
            pSurface->pContext->drawingCallbacks.draw_rect(pSurface, left, top, right, bottom, color);
        }
    }
}

void easy2d_draw_rect_outline(easy2d_surface * pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_rect_outline != NULL) {
            pSurface->pContext->drawingCallbacks.draw_rect_outline(pSurface, left, top, right, bottom, color, outlineWidth);
        }
    }
}

void easy2d_draw_rect_with_outline(easy2d_surface * pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth, easy2d_color outlineColor)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_rect_with_outline != NULL) {
            pSurface->pContext->drawingCallbacks.draw_rect_with_outline(pSurface, left, top, right, bottom, color, outlineWidth, outlineColor);
        }
    }
}

void easy2d_draw_round_rect(easy2d_surface * pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_round_rect != NULL) {
            pSurface->pContext->drawingCallbacks.draw_round_rect(pSurface, left, top, right, bottom, color, radius);
        }
    }
}

void easy2d_draw_round_rect_outline(easy2d_surface * pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_round_rect_outline != NULL) {
            pSurface->pContext->drawingCallbacks.draw_round_rect_outline(pSurface, left, top, right, bottom, color, radius, outlineWidth);
        }
    }
}

void easy2d_draw_round_rect_with_outline(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth, easy2d_color outlineColor)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_round_rect_with_outline != NULL) {
            pSurface->pContext->drawingCallbacks.draw_round_rect_with_outline(pSurface, left, top, right, bottom, color, radius, outlineWidth, outlineColor);
        }
    }
}

void easy2d_draw_text(easy2d_surface* pSurface, easy2d_font* pFont, const char* text, unsigned int textSizeInBytes, float posX, float posY, easy2d_color color, easy2d_color backgroundColor)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_text != NULL) {
            pSurface->pContext->drawingCallbacks.draw_text(pSurface, pFont, text, textSizeInBytes, posX, posY, color, backgroundColor);
        }
    }
}

void easy2d_set_clip(easy2d_surface* pSurface, float left, float top, float right, float bottom)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.set_clip != NULL) {
            pSurface->pContext->drawingCallbacks.set_clip(pSurface, left, top, right, bottom);
        }
    }
}

void easy2d_get_clip(easy2d_surface* pSurface, float* pLeftOut, float* pTopOut, float* pRightOut, float* pBottomOut)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.get_clip != NULL) {
            pSurface->pContext->drawingCallbacks.get_clip(pSurface, pLeftOut, pTopOut, pRightOut, pBottomOut);
        }
    }
}

easy2d_font* easy2d_create_font(easy2d_context* pContext, const char* family, unsigned int size, easy2d_font_weight weight, easy2d_font_slant slant, float rotation)
{
    if (pContext == NULL) {
        return NULL;
    }

    easy2d_font* pFont = malloc(sizeof(easy2d_font) - sizeof(pFont->pExtraData) + pContext->fontExtraBytes);
    if (pFont == NULL) {
        return NULL;
    }

    pFont->pContext  = pContext;
    pFont->family[0] = '\0';
    pFont->size      = size;
    pFont->weight    = weight;
    pFont->slant     = slant;
    pFont->rotation  = rotation;

    if (family != NULL) {
        strcpy_s(pFont->family, sizeof(pFont->family), family);
    }

    if (pContext->drawingCallbacks.on_create_font != NULL) {
        if (!pContext->drawingCallbacks.on_create_font(pFont)) {
            free(pFont);
            return NULL;
        }
    }

    return pFont;
}

void easy2d_delete_font(easy2d_font* pFont)
{
    if (pFont == NULL) {
        return;
    }

    assert(pFont->pContext != NULL);

    if (pFont->pContext->drawingCallbacks.on_delete_font != NULL) {
        pFont->pContext->drawingCallbacks.on_delete_font(pFont);
    }

    free(pFont);
}

void* easy2d_get_font_extra_data(easy2d_font* pFont)
{
    if (pFont == NULL) {
        return NULL;
    }

    return pFont->pExtraData;
}

bool easy2d_get_font_metrics(easy2d_font* pFont, easy2d_font_metrics* pMetricsOut)
{
    if (pFont == NULL) {
        return false;
    }

    assert(pFont->pContext != NULL);

    if (pFont->pContext->drawingCallbacks.get_font_metrics != NULL) {
        return pFont->pContext->drawingCallbacks.get_font_metrics(pFont, pMetricsOut);
    }

    return false;
}

bool easy2d_get_glyph_metrics(easy2d_font* pFont, unsigned int utf32, easy2d_glyph_metrics* pMetricsOut)
{
    if (pFont == NULL || pMetricsOut == NULL) {
        return false;
    }

    assert(pFont->pContext != NULL);

    if (pFont->pContext->drawingCallbacks.get_glyph_metrics != NULL) {
        return pFont->pContext->drawingCallbacks.get_glyph_metrics(pFont, utf32, pMetricsOut);
    }

    return false;
}

bool easy2d_measure_string(easy2d_font* pFont, const char* text, unsigned int textSizeInBytes, float* pWidthOut, float* pHeightOut)
{
    if (pFont == NULL) {
        return false;
    }

    assert(pFont->pContext != NULL);

    if (pFont->pContext->drawingCallbacks.measure_string != NULL) {
        return pFont->pContext->drawingCallbacks.measure_string(pFont, text, textSizeInBytes, pWidthOut, pHeightOut);
    }

    return false;
}




/////////////////////////////////////////////////////////////////
//
// UTILITY API
//
/////////////////////////////////////////////////////////////////

easy2d_color easy2d_rgba(easy2d_byte r, easy2d_byte g, easy2d_byte b, easy2d_byte a)
{
    easy2d_color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;

    return color;
}

easy2d_color easy2d_rgb(easy2d_byte r, easy2d_byte g, easy2d_byte b)
{
    easy2d_color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = 255;

    return color;
}




/////////////////////////////////////////////////////////////////
//
// WINDOWS GDI 2D API
//
/////////////////////////////////////////////////////////////////
#ifndef EASY2D_NO_GDI

typedef struct
{
    /// The device context that owns every surface HBITMAP object. All drawing is done through this DC.
    HDC hDC;

    /// The buffer used to store wchar strings when converting from char* to wchar_t* strings. We just use a global buffer for
    /// this so we can avoid unnecessary allocations.
    wchar_t* wcharBuffer;

    /// The size of wcharBuffer (including the null terminator).
    unsigned int wcharBufferLength;

} gdi_context_data;

typedef struct
{
    /// The window to draw to. The can be null, which is the case when creating the surface with easy2d_create_surface(). When this
    /// is non-null the size of the surface is always tied to the window.
    HWND hWnd;

    /// The HDC to use when drawing to the surface.
    HDC hDC;

    /// The PAINTSTRUCT object that is filled by BeginPaint(). Only used when hWnd is non-null.
    PAINTSTRUCT ps;

    /// The bitmap to render to. This is created with GDI's CreateDIBSection() API, using the DC above. This is only used
    /// when hDC is NULL, which is the default.
    HBITMAP hBitmap;

    /// A pointer to the raw bitmap data. This is allocated CreateDIBSection().
    void* pBitmapData;


    /// The stock DC brush.
    HGDIOBJ hStockDCBrush;

    /// The stock null brush.
    HGDIOBJ hStockNullBrush;

    /// The stock DC pen.
    HGDIOBJ hStockDCPen;

    /// The stock null pen.
    HGDIOBJ hStockNullPen;


    /// The pen that was active at the start of drawing. This is restored at the end of drawing.
    HGDIOBJ hPrevPen;

    /// The brush that was active at the start of drawing.
    HGDIOBJ hPrevBrush;

    /// The brush color at the start of drawing.
    COLORREF prevBrushColor;

    /// The previous font.
    HGDIOBJ hPrevFont;

    /// The previous text background mode.
    int prevBkMode;

    /// The previous text background color.
    COLORREF prevBkColor;


} gdi_surface_data;

typedef struct
{
    /// A handle to the Win32 font.
    HFONT hFont;

} gdi_font_data;


bool easy2d_on_create_context_gdi(easy2d_context* pContext);
void easy2d_on_delete_context_gdi(easy2d_context* pContext);
bool easy2d_on_create_surface_gdi(easy2d_surface* pSurface, float width, float height);
void easy2d_on_delete_surface_gdi(easy2d_surface* pSurface);
bool easy2d_on_create_font_gdi(easy2d_font* pFont);
void easy2d_on_delete_font_gdi(easy2d_font* pFont);

void easy2d_begin_draw_gdi(easy2d_surface* pSurface);
void easy2d_end_draw_gdi(easy2d_surface* pSurface);
void easy2d_clear_gdi(easy2d_surface* pSurface, easy2d_color color);
void easy2d_draw_rect_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color);
void easy2d_draw_rect_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth);
void easy2d_draw_rect_with_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth, easy2d_color outlineColor);
void easy2d_draw_round_rect_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius);
void easy2d_draw_round_rect_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth);
void easy2d_draw_round_rect_with_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth, easy2d_color outlineColor);
void easy2d_draw_text_gdi(easy2d_surface* pSurface, easy2d_font* pFont, const char* text, unsigned int textSizeInBytes, float posX, float posY, easy2d_color color, easy2d_color backgroundColor);
void easy2d_set_clip_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom);
void easy2d_get_clip_gdi(easy2d_surface* pSurface, float* pLeftOut, float* pTopOut, float* pRightOut, float* pBottomOut);

bool easy2d_get_font_metrics_gdi(easy2d_font* pFont, easy2d_font_metrics* pMetricsOut);
bool easy2d_get_glyph_metrics_gdi(easy2d_font* pFont, unsigned int utf32, easy2d_glyph_metrics* pGlyphMetrics);
bool easy2d_measure_string_gdi(easy2d_font* pFont, const char* text, unsigned int textSizeInBytes, float* pWidthOut, float* pHeightOut);

/// Converts a char* to a wchar_t* string.
wchar_t* easy2d_to_wchar_gdi(easy2d_context* pContext, const char* text, unsigned int textSizeInBytes, unsigned int* characterCountOut);

/// Converts a UTF-32 character to a UTF-16.
static int easy2d_utf32_to_utf16(unsigned int utf32, unsigned short utf16[2])
{
    if (utf16 == NULL) {
        return 0;
    }

    if (utf32 < 0xD800 || (utf32 >= 0xE000 && utf32 <= 0xFFFF))
    {
        utf16[0] = (unsigned short)utf32;
        utf16[1] = 0;
        return 1;
    }
    else
    {
        if (utf32 >= 0x10000 && utf32 <= 0x10FFFF)
        {
            utf16[0] = (unsigned short)(0xD7C0 + (unsigned short)(utf32 >> 10));
            utf16[1] = (unsigned short)(0xDC00 + (unsigned short)(utf32 & 0x3FF));
            return 2;
        }
        else
        {
            // Invalid.
            utf16[0] = 0;
            utf16[0] = 0;
            return 0;
        }
    }
}

easy2d_context* easy2d_create_context_gdi()
{
    easy2d_drawing_callbacks callbacks;
    callbacks.on_create_context            = easy2d_on_create_context_gdi;
    callbacks.on_delete_context            = easy2d_on_delete_context_gdi;
    callbacks.on_create_surface            = easy2d_on_create_surface_gdi;
    callbacks.on_delete_surface            = easy2d_on_delete_surface_gdi;
    callbacks.on_create_font               = easy2d_on_create_font_gdi;
    callbacks.on_delete_font               = easy2d_on_delete_font_gdi;

    callbacks.begin_draw                   = easy2d_begin_draw_gdi;
    callbacks.end_draw                     = easy2d_end_draw_gdi;
    callbacks.clear                        = easy2d_clear_gdi;
    callbacks.draw_rect                    = easy2d_draw_rect_gdi;
    callbacks.draw_rect_outline            = easy2d_draw_rect_outline_gdi;
    callbacks.draw_rect_with_outline       = easy2d_draw_rect_with_outline_gdi;
    callbacks.draw_round_rect              = easy2d_draw_round_rect_gdi;
    callbacks.draw_round_rect_outline      = easy2d_draw_round_rect_outline_gdi;
    callbacks.draw_round_rect_with_outline = easy2d_draw_round_rect_with_outline_gdi;
    callbacks.draw_text                    = easy2d_draw_text_gdi;
    callbacks.set_clip                     = easy2d_set_clip_gdi;
    callbacks.get_clip                     = easy2d_get_clip_gdi;

    callbacks.get_font_metrics             = easy2d_get_font_metrics_gdi;
    callbacks.get_glyph_metrics            = easy2d_get_glyph_metrics_gdi;
    callbacks.measure_string               = easy2d_measure_string_gdi;

    return easy2d_create_context(callbacks, sizeof(gdi_context_data), sizeof(gdi_surface_data), sizeof(gdi_font_data));
}

easy2d_surface* easy2d_create_surface_gdi_HWND(easy2d_context* pContext, HWND hWnd)
{
    easy2d_surface* pSurface = easy2d_create_surface(pContext, 0, 0);
    if (pSurface != NULL) {
        gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
        if (pGDIData != NULL) {
            pGDIData->hWnd = hWnd;
        }
    }

    return pSurface;
}

HDC easy2d_get_HDC(easy2d_surface* pSurface)
{
    if (pSurface != NULL) {
        gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
        if (pGDIData != NULL) {
            return pGDIData->hDC;
        }
    }

    return NULL;
}

HBITMAP easy2d_get_HBITMAP(easy2d_surface* pSurface)
{
    if (pSurface != NULL) {
        gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
        if (pGDIData != NULL) {
            return pGDIData->hBitmap;
        }
    }

    return NULL;
}

HFONT easy2d_get_HFONT(easy2d_font* pFont)
{
    gdi_font_data* pGDIData = easy2d_get_font_extra_data(pFont);
    if (pGDIData == NULL) {
        return NULL;
    }

    return pGDIData->hFont;
}


bool easy2d_on_create_context_gdi(easy2d_context* pContext)
{
    assert(pContext != NULL);

    // We need to create the DC that all of our rendering commands will be drawn to.
    gdi_context_data* pGDIData = easy2d_get_context_extra_data(pContext);
    if (pGDIData == NULL) {
        return false;
    }

    pGDIData->hDC = CreateCompatibleDC(GetDC(GetDesktopWindow()));
    if (pGDIData->hDC == NULL) {
        return false;
    }


    // We want to use the advanced graphics mode so that GetTextExtentPoint32() performs the conversions for font rotation for us.
    SetGraphicsMode(pGDIData->hDC, GM_ADVANCED);


    pGDIData->wcharBuffer       = NULL;
    pGDIData->wcharBufferLength = 0;


    return true;
}

void easy2d_on_delete_context_gdi(easy2d_context* pContext)
{
    assert(pContext != NULL);

    gdi_context_data* pGDIData = easy2d_get_context_extra_data(pContext);
    if (pGDIData != NULL)
    {
        free(pGDIData->wcharBuffer);
        pGDIData->wcharBuffer       = 0;
        pGDIData->wcharBufferLength = 0;

        DeleteDC(pGDIData->hDC);
        pGDIData->hDC = NULL;
    }
}

bool easy2d_on_create_surface_gdi(easy2d_surface* pSurface, float width, float height)
{
    assert(pSurface != NULL);

    gdi_context_data* pGDIContextData = easy2d_get_context_extra_data(pSurface->pContext);
    if (pGDIContextData == NULL) {
        return false;
    }

    gdi_surface_data* pGDISurfaceData = easy2d_get_surface_extra_data(pSurface);
    if (pGDISurfaceData == NULL) {
        return false;
    }

    HDC hDC = pGDIContextData->hDC;
    if (hDC == NULL) {
        return false;
    }


    pGDISurfaceData->hWnd = NULL;
    

    if (width != 0 && height != 0)
    {
        pGDISurfaceData->hDC  = hDC;

        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth       = (LONG)width;
        bmi.bmiHeader.biHeight      = (LONG)height;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        pGDISurfaceData->hBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, &pGDISurfaceData->pBitmapData, NULL, 0);
        if (pGDISurfaceData->hBitmap == NULL) {
            return false;
        }
    }
    else
    {
        pGDISurfaceData->hBitmap = NULL;
        pGDISurfaceData->hDC     = NULL;
    }


    return true;
}

void easy2d_on_delete_surface_gdi(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        DeleteObject(pGDIData->hBitmap);
        pGDIData->hBitmap = NULL;
    }
}

bool easy2d_on_create_font_gdi(easy2d_font* pFont)
{
    assert(pFont != NULL);

    gdi_font_data* pGDIData = easy2d_get_font_extra_data(pFont);
    if (pGDIData == NULL) {
        return false;
    }


    LONG weightGDI = FW_REGULAR;
    switch (pFont->weight)
    {
    case easy2d_weight_medium:      weightGDI = FW_MEDIUM;     break;
    case easy2d_weight_thin:        weightGDI = FW_THIN;       break;
    case easy2d_weight_extra_light: weightGDI = FW_EXTRALIGHT; break;
    case easy2d_weight_light:       weightGDI = FW_LIGHT;      break;
    case easy2d_weight_semi_bold:   weightGDI = FW_SEMIBOLD;   break;
    case easy2d_weight_bold:        weightGDI = FW_BOLD;       break;
    case easy2d_weight_extra_bold:  weightGDI = FW_EXTRABOLD;  break;
    case easy2d_weight_heavy:       weightGDI = FW_HEAVY;      break;
    default: break;
    }

	BYTE slantGDI = FALSE;
    if (pFont->slant == easy2d_slant_italic || pFont->slant == easy2d_slant_oblique) {
        slantGDI = TRUE;
    }


	LOGFONTA logfont;
	memset(&logfont, 0, sizeof(logfont));


    
    logfont.lfHeight      = -(LONG)pFont->size;
	logfont.lfWeight      = weightGDI;
	logfont.lfItalic      = slantGDI;
	logfont.lfCharSet     = DEFAULT_CHARSET;
	logfont.lfQuality     = (pFont->size > 36) ? ANTIALIASED_QUALITY : CLEARTYPE_QUALITY;
    logfont.lfEscapement  = (LONG)pFont->rotation * 10;
    logfont.lfOrientation = (LONG)pFont->rotation * 10;
    
    size_t familyLength = strlen(pFont->family);
	memcpy(logfont.lfFaceName, pFont->family, (familyLength < 31) ? familyLength : 31);


	pGDIData->hFont = CreateFontIndirectA(&logfont);
    if (pGDIData->hFont == NULL) {
        return false;
    }

    return true;
}

void easy2d_on_delete_font_gdi(easy2d_font* pFont)
{
    assert(pFont != NULL);

    gdi_font_data* pGDIData = easy2d_get_font_extra_data(pFont);
    if (pGDIData == NULL) {
        return;
    }

    DeleteObject(pGDIData->hFont);
}


void easy2d_begin_draw_gdi(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL) {
        if (pGDIData->hWnd != NULL) {
            pGDIData->hDC = BeginPaint(pGDIData->hWnd, &pGDIData->ps);
        } else {
            SelectObject(easy2d_get_HDC(pSurface), pGDIData->hBitmap);
        }

        HDC hDC = easy2d_get_HDC(pSurface);

        pGDIData->hStockDCBrush   = GetStockObject(DC_BRUSH);
        pGDIData->hStockNullBrush = GetStockObject(NULL_BRUSH);
        pGDIData->hStockDCPen     = GetStockObject(DC_PEN);
        pGDIData->hStockNullPen   = GetStockObject(NULL_PEN);
        
        // Retrieve the defaults so they can be restored later.
        pGDIData->hPrevPen       = GetCurrentObject(hDC, OBJ_PEN);
        pGDIData->hPrevBrush     = GetCurrentObject(hDC, OBJ_BRUSH);
        pGDIData->prevBrushColor = GetDCBrushColor(hDC);
        pGDIData->hPrevFont      = GetCurrentObject(hDC, OBJ_FONT);
        pGDIData->prevBkMode     = GetBkMode(hDC);
        pGDIData->prevBkColor    = GetBkColor(hDC);
    }
}

void easy2d_end_draw_gdi(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL) {
        HDC hDC = easy2d_get_HDC(pSurface);

        SelectClipRgn(hDC, NULL);

        SelectObject(hDC, pGDIData->hPrevPen);
        SelectObject(hDC, pGDIData->hPrevBrush);
        SetDCBrushColor(hDC, pGDIData->prevBrushColor);
        SelectObject(hDC, pGDIData->hPrevFont);
        SetBkMode(hDC, pGDIData->prevBkMode);
        SetBkColor(hDC, pGDIData->prevBkColor);

        if (pGDIData->hWnd != NULL) {
            EndPaint(pGDIData->hWnd, &pGDIData->ps);
        }
    }
}

void easy2d_clear_gdi(easy2d_surface* pSurface, easy2d_color color)
{
    assert(pSurface != NULL);

    easy2d_draw_rect_gdi(pSurface, 0, 0, pSurface->width, pSurface->height, color);
}

void easy2d_draw_rect_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        HDC hDC = easy2d_get_HDC(pSurface);

        SelectObject(hDC, pGDIData->hStockNullPen);
        SelectObject(hDC, pGDIData->hStockDCBrush);
        SetDCBrushColor(hDC, RGB(color.r, color.g, color.b));

        // Now draw the rectangle. The documentation for this says that the width and height is 1 pixel less when the pen is null. Therefore we will
        // increase the width and height by 1 since we have got the pen set to null.
        Rectangle(hDC, (int)left, (int)top, (int)right + 1, (int)bottom + 1);
    }
}

void easy2d_draw_rect_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        HDC hDC = easy2d_get_HDC(pSurface);

        HPEN hPen = CreatePen(PS_SOLID | PS_INSIDEFRAME, (int)outlineWidth, RGB(color.r, color.g, color.b));
        if (hPen != NULL)
        {
            SelectObject(hDC, pGDIData->hStockNullBrush);
            SelectObject(hDC, hPen);

            Rectangle(hDC, (int)left, (int)top, (int)right, (int)bottom);

            DeleteObject(hPen);
        }
    }
}

void easy2d_draw_rect_with_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth, easy2d_color outlineColor)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        HDC hDC = easy2d_get_HDC(pSurface);

        HPEN hPen = CreatePen(PS_SOLID | PS_INSIDEFRAME, (int)outlineWidth, RGB(outlineColor.r, outlineColor.g, outlineColor.b));
        if (hPen != NULL)
        {
            SelectObject(hDC, hPen);
            SelectObject(hDC, pGDIData->hStockDCBrush);
            SetDCBrushColor(hDC, RGB(color.r, color.g, color.b));
            
            Rectangle(hDC, (int)left, (int)top, (int)right, (int)bottom);

            DeleteObject(hPen);
        }
    }
}

void easy2d_draw_round_rect_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        HDC hDC = easy2d_get_HDC(pSurface);

        SelectObject(hDC, pGDIData->hStockNullPen);
        SelectObject(hDC, pGDIData->hStockDCBrush);
        SetDCBrushColor(hDC, RGB(color.r, color.g, color.b));

        RoundRect(hDC, (int)left, (int)top, (int)right + 1, (int)bottom + 1, (int)(radius*2), (int)(radius*2));
    }
}

void easy2d_draw_round_rect_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        HDC hDC = easy2d_get_HDC(pSurface);

        HPEN hPen = CreatePen(PS_SOLID | PS_INSIDEFRAME, (int)outlineWidth, RGB(color.r, color.g, color.b));
        if (hPen != NULL)
        {
            SelectObject(hDC, pGDIData->hStockNullBrush);
            SelectObject(hDC, hPen);

            RoundRect(hDC, (int)left, (int)top, (int)right, (int)bottom, (int)(radius*2), (int)(radius*2));

            DeleteObject(hPen);
        }
    }
}

void easy2d_draw_round_rect_with_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth, easy2d_color outlineColor)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        HDC hDC = easy2d_get_HDC(pSurface);

        HPEN hPen = CreatePen(PS_SOLID | PS_INSIDEFRAME, (int)outlineWidth, RGB(outlineColor.r, outlineColor.g, outlineColor.b));
        if (hPen != NULL)
        {
            SelectObject(hDC, hPen);
            SelectObject(hDC, pGDIData->hStockDCBrush);
            SetDCBrushColor(hDC, RGB(color.r, color.g, color.b));

            RoundRect(hDC, (int)left, (int)top, (int)right, (int)bottom, (int)(radius*2), (int)(radius*2));

            DeleteObject(hPen);
        }
    }
}

void easy2d_draw_text_gdi(easy2d_surface* pSurface, easy2d_font* pFont, const char* text, unsigned int textSizeInBytes, float posX, float posY, easy2d_color color, easy2d_color backgroundColor)
{
    gdi_font_data* pGDIFontData = easy2d_get_font_extra_data(pFont);
    if (pGDIFontData == NULL) {
        return;
    }


    HDC hDC = easy2d_get_HDC(pSurface);

    HFONT hFontGDI = pGDIFontData->hFont;
    if (hFontGDI != NULL)
    {
        // We actually want to use the W version of TextOut because otherwise unicode doesn't work properly.

        unsigned int textWLength;
        wchar_t* textW = easy2d_to_wchar_gdi(pSurface->pContext, text, textSizeInBytes, &textWLength);
        if (textW != NULL)
        {
            SelectObject(hDC, hFontGDI);

            UINT options = 0;
            RECT rect = {0, 0, 0, 0};

            if (backgroundColor.a == 0) {
                SetBkMode(hDC, TRANSPARENT);
            } else {
                SetBkMode(hDC, OPAQUE);
                SetBkColor(hDC, RGB(backgroundColor.r, backgroundColor.g, backgroundColor.b));

                // There is an issue with the way GDI draws the background of a string of text. When ClearType is enabled, the rectangle appears
                // to be wider than it is supposed to be. As a result, drawing text right next to each other results in the most recent one being
                // drawn slightly on top of the previous one. To fix this we need to use ExtTextOut() with the ETO_CLIPPED parameter enabled.
                options |= ETO_CLIPPED;

                SIZE textSize = {0, 0};
                GetTextExtentPoint32W(hDC, textW, textWLength, &textSize);
                rect.left   = (LONG)posX;
                rect.top    = (LONG)posY;
                rect.right  = rect.left + textSize.cx;
                rect.bottom = rect.top + textSize.cy;
            }
                
            SetTextColor(hDC, RGB(color.r, color.g, color.b));

            ExtTextOutW(hDC, (int)posX, (int)posY, options, &rect, textW, textWLength, NULL);
        }
    }
}

void easy2d_set_clip_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        HDC hDC = easy2d_get_HDC(pSurface);

        SelectClipRgn(hDC, NULL);
        IntersectClipRect(hDC, (int)left, (int)top, (int)right, (int)bottom);
    }
}

void easy2d_get_clip_gdi(easy2d_surface* pSurface, float* pLeftOut, float* pTopOut, float* pRightOut, float* pBottomOut)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        RECT rect;
        GetClipBox(easy2d_get_HDC(pSurface), &rect);

        if (pLeftOut != NULL) {
            *pLeftOut = (float)rect.left;
        }
        if (pTopOut != NULL) {
            *pTopOut = (float)rect.top;
        }
        if (pRightOut != NULL) {
            *pRightOut = (float)rect.right;
        }
        if (pBottomOut != NULL) {
            *pBottomOut = (float)rect.bottom;
        }
    }
}



bool easy2d_get_font_metrics_gdi(easy2d_font* pFont, easy2d_font_metrics* pMetricsOut)
{
    assert(pFont != NULL);
    assert(pMetricsOut != NULL);

    gdi_font_data* pGDIFontData = easy2d_get_font_extra_data(pFont);
    if (pGDIFontData == NULL) {
        return false;
    }

    gdi_context_data* pGDIContextData = easy2d_get_context_extra_data(pFont->pContext);
    if (pGDIContextData == NULL) {
        return false;
    }


    bool result = false;
    HGDIOBJ hPrevFont = SelectObject(pGDIContextData->hDC, pGDIFontData->hFont);
    {
        TEXTMETRIC metrics;
        GetTextMetrics(pGDIContextData->hDC, &metrics);

        pMetricsOut->ascent     = metrics.tmAscent;
        pMetricsOut->descent    = metrics.tmDescent;
        pMetricsOut->lineHeight = metrics.tmHeight;


        const MAT2 transform = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};        // <-- Identity matrix

        GLYPHMETRICS spaceMetrics;
        DWORD bitmapBufferSize = GetGlyphOutlineW(pGDIContextData->hDC, ' ', GGO_NATIVE, &spaceMetrics, 0, NULL, &transform);
        if (bitmapBufferSize != GDI_ERROR)
        {
			pMetricsOut->spaceWidth = spaceMetrics.gmBlackBoxX;
            result = true;
        }
    }
    SelectObject(pGDIContextData->hDC, hPrevFont);
    
    return result;
}

bool easy2d_get_glyph_metrics_gdi(easy2d_font* pFont, unsigned int utf32, easy2d_glyph_metrics* pGlyphMetrics)
{
    assert(pFont != NULL);
    assert(pGlyphMetrics != NULL);

    gdi_font_data* pGDIFontData = easy2d_get_font_extra_data(pFont);
    if (pGDIFontData == NULL) {
        return false;
    }

    gdi_context_data* pGDIContextData = easy2d_get_context_extra_data(pFont->pContext);
    if (pGDIContextData == NULL) {
        return false;
    }


    SelectObject(pGDIContextData->hDC, pGDIFontData->hFont);


    const MAT2 transform = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};        // <-- Identity matrix

    unsigned short utf16[2];
    int utf16Len = easy2d_utf32_to_utf16(utf32, utf16);

    WCHAR glyphIndices[2];

    GCP_RESULTSW glyphResults;
    ZeroMemory(&glyphResults, sizeof(glyphResults));
    glyphResults.lStructSize = sizeof(glyphResults);
    glyphResults.lpGlyphs = glyphIndices;
    glyphResults.nGlyphs  = 2;
    if (GetCharacterPlacementW(pGDIContextData->hDC, utf16, utf16Len, 0, &glyphResults, 0) != 0)
    {
        GLYPHMETRICS metrics;
        DWORD bitmapBufferSize = GetGlyphOutlineW(pGDIContextData->hDC, glyphIndices[0], GGO_NATIVE | GGO_GLYPH_INDEX, &metrics, 0, NULL, &transform);
        if (bitmapBufferSize != GDI_ERROR)
        {
            pGlyphMetrics->width    = metrics.gmBlackBoxX;
            pGlyphMetrics->height   = metrics.gmBlackBoxY;
            pGlyphMetrics->originX  = metrics.gmptGlyphOrigin.x;
            pGlyphMetrics->originY  = metrics.gmptGlyphOrigin.y;
            pGlyphMetrics->advanceX = metrics.gmCellIncX;
            pGlyphMetrics->advanceY = metrics.gmCellIncY;

            return true;
        }
    }
    
    return false;
}

bool easy2d_measure_string_gdi(easy2d_font* pFont, const char* text, unsigned int textSizeInBytes, float* pWidthOut, float* pHeightOut)
{
    assert(pFont != NULL);

    gdi_font_data* pGDIFontData = easy2d_get_font_extra_data(pFont);
    if (pGDIFontData == NULL) {
        return false;
    }

    gdi_context_data* pGDIContextData = easy2d_get_context_extra_data(pFont->pContext);
    if (pGDIContextData == NULL) {
        return false;
    }


    SelectObject(pGDIContextData->hDC, pGDIFontData->hFont);

    unsigned int textWLength;
    wchar_t* textW = easy2d_to_wchar_gdi(pFont->pContext, text, textSizeInBytes, &textWLength);
    if (textW != NULL)
    {
        SIZE sizeWin32;
        if (GetTextExtentPoint32W(pGDIContextData->hDC, textW, textWLength, &sizeWin32))
        {
            if (pWidthOut != NULL) {
                *pWidthOut = (float)sizeWin32.cx;
            }
            if (pHeightOut != NULL) {
                *pHeightOut = (float)sizeWin32.cy;
            }

            return true;
        }
    }

    return false;
}


wchar_t* easy2d_to_wchar_gdi(easy2d_context* pContext, const char* text, unsigned int textSizeInBytes, unsigned int* characterCountOut)
{
    if (pContext == NULL || text == NULL) {
        return NULL;
    }

    gdi_context_data* pGDIData = easy2d_get_context_extra_data(pContext);
    if (pGDIData == NULL) {
        return NULL;
    }


    int wcharCount = MultiByteToWideChar(CP_UTF8, 0, text, textSizeInBytes, NULL, 0);
    if (wcharCount == 0) {
        return NULL;
    }

    if (pGDIData->wcharBufferLength < (unsigned int)wcharCount + 1) {
        free(pGDIData->wcharBuffer);
        pGDIData->wcharBuffer       = malloc(sizeof(wchar_t) * (wcharCount + 1));
        pGDIData->wcharBufferLength = wcharCount + 1;
    }

    wcharCount = MultiByteToWideChar(CP_UTF8, 0, text, textSizeInBytes, pGDIData->wcharBuffer, pGDIData->wcharBufferLength);
    if (wcharCount == 0) {
        return NULL;
    }


    if (characterCountOut != NULL) {
        *characterCountOut = wcharCount;
    }

    return pGDIData->wcharBuffer;
}

#endif  // GDI


/////////////////////////////////////////////////////////////////
//
// CAIRO 2D API
//
/////////////////////////////////////////////////////////////////
#ifndef EASY2D_NO_CAIRO

typedef struct
{
    cairo_surface_t* pCairoSurface;
    cairo_t* pCairoContext;

}cairo_surface_data;

bool easy2d_on_create_context_cairo(easy2d_context* pContext);
void easy2d_on_delete_context_cairo(easy2d_context* pContext);
bool easy2d_on_create_surface_cairo(easy2d_surface* pSurface, float width, float height);
void easy2d_on_delete_surface_cairo(easy2d_surface* pSurface);
void easy2d_begin_draw_cairo(easy2d_surface* pSurface);
void easy2d_end_draw_cairo(easy2d_surface* pSurface);
void easy2d_draw_rect_cairo(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color);


easy2d_context* easy2d_create_context_cairo()
{
    easy2d_drawing_callbacks callbacks;
    callbacks.on_create_context = easy2d_on_create_context_cairo;
    callbacks.on_delete_context = easy2d_on_delete_context_cairo;
    callbacks.on_create_surface = easy2d_on_create_surface_cairo;
    callbacks.on_delete_surface = easy2d_on_delete_surface_cairo;
    callbacks.begin_draw        = easy2d_begin_draw_cairo;
    callbacks.end_draw          = easy2d_end_draw_cairo;
    callbacks.draw_rect         = easy2d_draw_rect_cairo;

    return easy2d_create_context(callbacks, 0, sizeof(cairo_surface_data));
}

cairo_surface_t* easy2d_get_cairo_surface_t(easy2d_surface* pSurface)
{
    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL) {
        return pCairoData->pCairoSurface;
    }

    return NULL;
}

cairo_t* easy2d_get_cairo_t(easy2d_surface* pSurface)
{
    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL) {
        return pCairoData->pCairoContext;
    }

    return NULL;
}


bool easy2d_on_create_context_cairo(easy2d_context* pContext)
{
    assert(pContext != NULL);
    (void)pContext;

    return true;
}

void easy2d_on_delete_context_cairo(easy2d_context* pContext)
{
    assert(pContext != NULL);
    (void)pContext;
}

bool easy2d_on_create_surface_cairo(easy2d_surface* pSurface, float width, float height)
{
    assert(pSurface != NULL);

    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData == NULL) {
        return false;
    }

    pCairoData->pCairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)width, (int)height);
    if (pCairoData->pCairoSurface == NULL) {
        return false;
    }

    pCairoData->pCairoContext = cairo_create(pCairoData->pCairoSurface);
    if (pCairoData->pCairoContext == NULL) {
        cairo_surface_destroy(pCairoData->pCairoSurface);
        return false;
    }


    return true;
}

void easy2d_on_delete_surface_cairo(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);

    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL)
    {
        cairo_destroy(pCairoData->pCairoContext);
        cairo_surface_destroy(pCairoData->pCairoSurface);
    }
}


void easy2d_begin_draw_cairo(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);

    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL)
    {
    }
}

void easy2d_end_draw_cairo(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);
    (void)pSurface;
}

void easy2d_draw_rect_cairo(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color)
{
    assert(pSurface != NULL);

    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL)
    {
        cairo_set_source_rgba(pCairoData->pCairoContext, color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0);
        cairo_rectangle(pCairoData->pCairoContext, left, top, (right - left), (bottom - top));
        cairo_fill(pCairoData->pCairoContext);
    }
}


#endif  // Cairo














