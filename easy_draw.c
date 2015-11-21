// Public Domain. See "unlicense" statement at the end of this file.

#include "easy_draw.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

easy2d_context* easy2d_create_context(easy2d_drawing_callbacks drawingCallbacks, size_t contextExtraBytes, size_t surfaceExtraBytes)
{
    easy2d_context* pContext = (easy2d_context*)malloc(sizeof(easy2d_context) - sizeof(pContext->pExtraData) + contextExtraBytes);
    if (pContext != NULL)
    {
        pContext->drawingCallbacks  = drawingCallbacks;
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

void easy2d_draw_round_rect_with_outline(easy2d_surface * pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth, easy2d_color outlineColor)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_round_rect_with_outline != NULL) {
            pSurface->pContext->drawingCallbacks.draw_round_rect_with_outline(pSurface, left, top, right, bottom, color, radius, outlineWidth, outlineColor);
        }
    }
}

void easy2d_draw_text(easy2d_surface* pSurface, const char* text, unsigned int textSizeInBytes, float posX, float posY, easy2d_font font, easy2d_color color, easy2d_color backgroundColor)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_text != NULL) {
            pSurface->pContext->drawingCallbacks.draw_text(pSurface, text, textSizeInBytes, posX, posY, font, color, backgroundColor);
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

easy2d_font easy2d_create_font(easy2d_context* pContext, const char* family, unsigned int size, easy2d_font_weight weight, easy2d_font_slant slant, float rotation)
{
    if (pContext != NULL)
    {
        if (pContext->drawingCallbacks.create_font != NULL) {
            return pContext->drawingCallbacks.create_font(pContext, family, size, weight, slant, rotation);
        }
    }

    return NULL;
}

void easy2d_delete_font(easy2d_context* pContext, easy2d_font font)
{
    if (pContext != NULL)
    {
        if (pContext->drawingCallbacks.delete_font != NULL) {
            pContext->drawingCallbacks.delete_font(pContext, font);
        }
    }
}

bool easy2d_get_font_metrics(easy2d_context* pContext, easy2d_font font, easy2d_font_metrics* pMetricsOut)
{
    if (pContext != NULL)
    {
        if (pContext->drawingCallbacks.get_font_metrics != NULL) {
            return pContext->drawingCallbacks.get_font_metrics(pContext, font, pMetricsOut);
        }
    }

    return false;
}

bool easy2d_get_glyph_metrics(easy2d_context* pContext, unsigned int utf32, easy2d_font font, easy2d_glyph_metrics* pGlyphMetrics)
{
    if (pContext != NULL)
    {
        if (pContext->drawingCallbacks.get_glyph_metrics != NULL) {
            return pContext->drawingCallbacks.get_glyph_metrics(pContext, utf32, font, pGlyphMetrics);
        }
    }

    return false;
}

bool easy2d_measure_string(easy2d_context* pContext, easy2d_font font, const char* text, unsigned int textSizeInBytes, float* pWidthOut, float* pHeightOut)
{
    if (pContext != NULL)
    {
        if (pContext->drawingCallbacks.measure_string != NULL) {
            return pContext->drawingCallbacks.measure_string(pContext, font, text, textSizeInBytes, pWidthOut, pHeightOut);
        }
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

}gdi_context_data;

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


}gdi_surface_data;

bool easy2d_on_create_context_gdi(easy2d_context* pContext);
void easy2d_on_delete_context_gdi(easy2d_context* pContext);
bool easy2d_on_create_surface_gdi(easy2d_surface* pSurface, float width, float height);
void easy2d_on_delete_surface_gdi(easy2d_surface* pSurface);
void easy2d_begin_draw_gdi(easy2d_surface* pSurface);
void easy2d_end_draw_gdi(easy2d_surface* pSurface);
void easy2d_clear_gdi(easy2d_surface* pSurface, easy2d_color color);
void easy2d_draw_rect_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color);
void easy2d_draw_rect_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth);
void easy2d_draw_rect_with_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth, easy2d_color outlineColor);
void easy2d_draw_round_rect_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius);
void easy2d_draw_round_rect_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth);
void easy2d_draw_round_rect_with_outline_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth, easy2d_color outlineColor);
void easy2d_draw_text_gdi(easy2d_surface* pSurface, const char* text, unsigned int textSizeInBytes, float posX, float posY, easy2d_font font, easy2d_color color, easy2d_color backgroundColor);
void easy2d_set_clip_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom);
void easy2d_get_clip_gdi(easy2d_surface* pSurface, float* pLeftOut, float* pTopOut, float* pRightOut, float* pBottomOut);
easy2d_font easy2d_create_font_gdi(easy2d_context* pContext, const char* family, unsigned int size, easy2d_font_weight weight, easy2d_font_slant slant, float rotation);
void easy2d_delete_font_gdi(easy2d_context* pContext, easy2d_font font);
bool easy2d_get_font_metrics_gdi(easy2d_context* pContext, easy2d_font font, easy2d_font_metrics* pMetricsOut);
bool easy2d_get_glyph_metrics_gdi(easy2d_context* pContext, unsigned int utf32, easy2d_font font, easy2d_glyph_metrics* pGlyphMetrics);
bool easy2d_measure_string_gdi(easy2d_context* pContext, easy2d_font font, const char* text, unsigned int textSizeInBytes, float* pWidthOut, float* pHeightOut);

/// Converts a char* to a wchar_t* string.
wchar_t* easy2d_to_wchar_gdi(easy2d_context* pContext, const char* text, unsigned int textSizeInBytes, unsigned int* characterCountOut);

easy2d_context* easy2d_create_context_gdi()
{
    easy2d_drawing_callbacks callbacks;
    callbacks.on_create_context            = easy2d_on_create_context_gdi;
    callbacks.on_delete_context            = easy2d_on_delete_context_gdi;
    callbacks.on_create_surface            = easy2d_on_create_surface_gdi;
    callbacks.on_delete_surface            = easy2d_on_delete_surface_gdi;
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
    callbacks.create_font                  = easy2d_create_font_gdi;
    callbacks.delete_font                  = easy2d_delete_font_gdi;
    callbacks.get_font_metrics             = easy2d_get_font_metrics_gdi;
    callbacks.get_glyph_metrics            = easy2d_get_glyph_metrics_gdi;
    callbacks.measure_string               = easy2d_measure_string_gdi;

    return easy2d_create_context(callbacks, sizeof(gdi_context_data), sizeof(gdi_surface_data));
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

        SelectObject(hDC, GetStockObject(NULL_PEN));
        SelectObject(hDC, GetStockObject(DC_BRUSH));
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
            SelectObject(hDC, GetStockObject(NULL_BRUSH));
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
            SelectObject(hDC, GetStockObject(DC_BRUSH));
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

        SelectObject(hDC, GetStockObject(NULL_PEN));
        SelectObject(hDC, GetStockObject(DC_BRUSH));
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
            SelectObject(hDC, GetStockObject(NULL_BRUSH));
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
            SelectObject(hDC, GetStockObject(DC_BRUSH));
            SetDCBrushColor(hDC, RGB(color.r, color.g, color.b));

            RoundRect(hDC, (int)left, (int)top, (int)right, (int)bottom, (int)(radius*2), (int)(radius*2));

            DeleteObject(hPen);
        }
    }
}

void easy2d_draw_text_gdi(easy2d_surface* pSurface, const char* text, unsigned int textSizeInBytes, float posX, float posY, easy2d_font font, easy2d_color color, easy2d_color backgroundColor)
{
    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        HDC hDC = easy2d_get_HDC(pSurface);

        HFONT hFontGDI = (HFONT)font;
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

easy2d_font easy2d_create_font_gdi(easy2d_context* pContext, const char* family, unsigned int size, easy2d_font_weight weight, easy2d_font_slant slant, float rotation)
{
    (void)pContext;

    LONG weightGDI = FW_REGULAR;
    switch (weight)
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
    if (slant == easy2d_slant_italic || slant == easy2d_slant_oblique) {
        slantGDI = TRUE;
    }


	LOGFONTA logfont;
	memset(&logfont, 0, sizeof(logfont));


    
    logfont.lfHeight      = -(LONG)size;
	logfont.lfWeight      = weightGDI;
	logfont.lfItalic      = slantGDI;
	logfont.lfCharSet     = DEFAULT_CHARSET;
	logfont.lfQuality     = (size > 36) ? ANTIALIASED_QUALITY : CLEARTYPE_QUALITY;
    logfont.lfEscapement  = (LONG)rotation * 10;
    logfont.lfOrientation = (LONG)rotation * 10;
    
    size_t familyLength = strlen(family);
	memcpy(logfont.lfFaceName, family, (familyLength < 31) ? familyLength : 31);


	return (easy2d_font)CreateFontIndirectA(&logfont);
}

void easy2d_delete_font_gdi(easy2d_context* pContext, easy2d_font font)
{
    (void)pContext;

    DeleteObject((HFONT)font);
}

bool easy2d_get_font_metrics_gdi(easy2d_context* pContext, easy2d_font font, easy2d_font_metrics* pMetricsOut)
{
    assert(pMetricsOut != NULL);

    bool result = false;

    gdi_context_data* pGDIData = easy2d_get_context_extra_data(pContext);
    if (pGDIData == NULL) {
        return result;
    }

    HDC hDC = pGDIData->hDC;

    
    HGDIOBJ hPrevFont = SelectObject(hDC, (HFONT)font);
    {
        TEXTMETRIC metrics;
        GetTextMetrics(hDC, &metrics);

        pMetricsOut->ascent     = metrics.tmAscent;
        pMetricsOut->descent    = metrics.tmDescent;
        pMetricsOut->lineHeight = metrics.tmHeight;


        const MAT2 transform = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};        // <-- Identity matrix

        GLYPHMETRICS spaceMetrics;
        DWORD bitmapBufferSize = GetGlyphOutlineW(hDC, ' ', GGO_NATIVE, &spaceMetrics, 0, NULL, &transform);
        if (bitmapBufferSize != GDI_ERROR)
        {
			pMetricsOut->spaceWidth = spaceMetrics.gmBlackBoxX;
            result = true;
        }
    }
    SelectObject(hDC, hPrevFont);
    
    return result;
}

bool easy2d_get_glyph_metrics_gdi(easy2d_context* pContext, unsigned int utf32, easy2d_font font, easy2d_glyph_metrics* pGlyphMetrics)
{
    if (pGlyphMetrics != NULL) {
        return false;
    }

    gdi_context_data* pGDIData = easy2d_get_context_extra_data(pContext);
    if (pGDIData == NULL) {
        return false;
    }

    const MAT2 transform = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};        // <-- Identity matrix

    GLYPHMETRICS spaceMetrics;
    DWORD bitmapBufferSize = GetGlyphOutlineW(pGDIData->hDC, ' ', GGO_NATIVE, &spaceMetrics, 0, NULL, &transform);
    if (bitmapBufferSize != GDI_ERROR)
    {
        pGlyphMetrics->width  = spaceMetrics.gmBlackBoxX;
        pGlyphMetrics->height = spaceMetrics.gmBlackBoxY;

        return true;
    }

    return false;
}

bool easy2d_measure_string_gdi(easy2d_context* pContext, easy2d_font font, const char* text, unsigned int textSizeInBytes, float* pWidthOut, float* pHeightOut)
{
    gdi_context_data* pGDIData = easy2d_get_context_extra_data(pContext);
    if (pGDIData == NULL) {
        return false;
    }

    HDC hDC = pGDIData->hDC;


    BOOL result = FALSE;
    HGDIOBJ hPrevFont = SelectObject(hDC, (HFONT)font);
    {
        SIZE sizeWin32;
        result = GetTextExtentPoint32A(hDC, text, textSizeInBytes, &sizeWin32);

        if (result)
        {
            if (pWidthOut != NULL) {
                *pWidthOut = (float)sizeWin32.cx;
            }
            if (pHeightOut != NULL) {
                *pHeightOut = (float)sizeWin32.cy;
            }
        }
    }
    SelectObject(hDC, hPrevFont);


    return result;
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














