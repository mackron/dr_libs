// Public Domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - Drawing must be done inside a easy2d_begin_draw() and easy2d_end_draw() pair. Rationale: 1) required for compatibility
//   with GDI's BeginPaint() and EndPaint() APIs; 2) gives implementations opportunity to save and restore state, such as
//   OpenGL state and whatnot.
// - This library is not thread safe.
//

//
// OPTIONS
//
// #define EASY2D_NO_GDI
// #define EASY2D_NO_CAIRO
//

#ifndef easy_draw
#define easy_draw

#include <stdlib.h>
#include <stdbool.h>

#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// No Cairo on Win32 builds.
#ifndef EASY2D_NO_CAIRO
#define EASY2D_NO_CAIRO
#endif
#else
// No GDI on non-Win32 builds.
#ifndef EASY2D_NO_GDI
#define EASY2D_NO_GDI
#endif
#endif


#ifdef __cplusplus
extern "C" {
#endif


/////////////////////////////////////////////////////////////////
//
// CORE 2D API
//
/////////////////////////////////////////////////////////////////

typedef unsigned char easy2d_byte;

typedef struct easy2d_context easy2d_context;
typedef struct easy2d_surface easy2d_surface;
typedef struct easy2d_color easy2d_color;
typedef struct easy2d_font_metrics easy2d_font_metrics;
typedef struct easy2d_glyph_metrics easy2d_glyph_metrics;
typedef struct easy2d_drawing_callbacks easy2d_drawing_callbacks;

typedef void* easy2d_font;


/// Structure representing an RGBA color. Color components are specified in the range of 0 - 255.
struct easy2d_color
{
    easy2d_byte r;
    easy2d_byte g;
    easy2d_byte b;
    easy2d_byte a;
};

struct easy2d_font_metrics
{
    int ascent;
    int descent;
    int lineHeight;
    int spaceWidth;
};

struct easy2d_glyph_metrics
{
    int width;
    int height;
};

typedef enum
{
    easy2d_weight_medium = 0,
    easy2d_weight_thin,
    easy2d_weight_extra_light,
    easy2d_weight_light,
    easy2d_weight_semi_bold,
    easy2d_weight_bold,
    easy2d_weight_extra_bold,
    easy2d_weight_heavy,

    easy2d_weight_normal  = easy2d_weight_medium,
    easy2d_weight_default = easy2d_weight_medium

} easy2d_font_weight;

typedef enum
{
    easy2d_slant_none = 0,
    easy2d_slant_italic,
    easy2d_slant_oblique

} easy2d_font_slant;


typedef bool        (* easy2d_on_create_context_proc)           (easy2d_context* pContext);
typedef void        (* easy2d_on_delete_context_proc)           (easy2d_context* pContext);
typedef bool        (* easy2d_on_create_surface_proc)           (easy2d_surface* pSurface, float width, float height);
typedef void        (* easy2d_on_delete_surface_proc)           (easy2d_surface* pSurface);
typedef void        (* easy2d_begin_draw_proc)                  (easy2d_surface* pSurface);
typedef void        (* easy2d_end_draw_proc)                    (easy2d_surface* pSurface);
typedef void        (* easy2d_clear_proc)                       (easy2d_surface* pSurface, easy2d_color color);
typedef void        (* easy2d_draw_rect_proc)                   (easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color);
typedef void        (* easy2d_draw_rect_outline_proc)           (easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth);
typedef void        (* easy2d_draw_rect_with_outline_proc)      (easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth, easy2d_color outlineColor);
typedef void        (* easy2d_draw_round_rect_proc)             (easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float width);
typedef void        (* easy2d_draw_round_rect_outline_proc)     (easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float width, float outlineWidth);
typedef void        (* easy2d_draw_round_rect_with_outline_proc)(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float width, float outlineWidth, easy2d_color outlineColor);
typedef void        (* easy2d_draw_text_proc)                   (easy2d_surface* pSurface, const char* text, unsigned int textSizeInBytes, float posX, float posY, easy2d_font font, easy2d_color color, easy2d_color backgroundColor);
typedef void        (* easy2d_set_clip_proc)                    (easy2d_surface* pSurface, float left, float top, float right, float bottom);
typedef void        (* easy2d_get_clip_proc)                    (easy2d_surface* pSurface, float* pLeftOut, float* pTopOut, float* pRightOut, float* pBottomOut);
typedef easy2d_font (* easy2d_create_font_proc)                 (easy2d_context* pContext, const char* family, unsigned int size, easy2d_font_weight weight, easy2d_font_slant slant, float rotation);
typedef void        (* easy2d_delete_font_proc)                 (easy2d_context* pContext, easy2d_font font);
typedef bool        (* easy2d_get_font_metrics_proc)            (easy2d_context* pContext, easy2d_font font, easy2d_font_metrics* pMetricsOut);
typedef bool        (* easy2d_get_glyph_metrics_proc)           (easy2d_context* pContext, unsigned int utf32, easy2d_font font, easy2d_glyph_metrics* pMetricsOut);
typedef bool        (* easy2d_measure_string_proc)              (easy2d_context* pContext, easy2d_font font, const char* text, unsigned int textSizeInBytes, float* pWidthOut, float* pHeightOut);



struct easy2d_drawing_callbacks
{
    easy2d_on_create_context_proc on_create_context;
    easy2d_on_delete_context_proc on_delete_context;
    easy2d_on_create_surface_proc on_create_surface;
    easy2d_on_delete_surface_proc on_delete_surface;

    easy2d_begin_draw_proc                   begin_draw;
    easy2d_end_draw_proc                     end_draw;
    easy2d_clear_proc                        clear;
    easy2d_draw_rect_proc                    draw_rect;
    easy2d_draw_rect_outline_proc            draw_rect_outline;
    easy2d_draw_rect_with_outline_proc       draw_rect_with_outline;
    easy2d_draw_round_rect_proc              draw_round_rect;
    easy2d_draw_round_rect_outline_proc      draw_round_rect_outline;
    easy2d_draw_round_rect_with_outline_proc draw_round_rect_with_outline;
    easy2d_draw_text_proc                    draw_text;
    easy2d_set_clip_proc                     set_clip;
    easy2d_get_clip_proc                     get_clip;

    easy2d_create_font_proc                  create_font;
    easy2d_delete_font_proc                  delete_font;
    easy2d_get_font_metrics_proc             get_font_metrics;
    easy2d_get_glyph_metrics_proc            get_glyph_metrics;
    easy2d_measure_string_proc               measure_string;
};

struct easy2d_surface
{
    /// A pointer to the context that owns the surface.
    easy2d_context* pContext;

    /// The width of the surface.
    float width;

    /// The height of the surface.
    float height;

    /// The extra bytes. The size of this buffer is equal to pContext->surfaceExtraBytes.
    easy2d_byte pExtraData[1];
};

struct easy2d_context
{
    /// The drawing callbacks.
    easy2d_drawing_callbacks drawingCallbacks;

    /// The number of extra bytes to allocate for each surface.
    size_t surfaceExtraBytes;

    /// The number of extra bytes to allocate for the context.
    size_t contextExtraBytes;

    /// The extra bytes.
    easy2d_byte pExtraData[1];
};



/// Creats a context.
easy2d_context* easy2d_create_context(easy2d_drawing_callbacks drawingCallbacks, size_t contextExtraBytes, size_t surfaceExtraBytes);

/// Deletes the given context.
void easy2d_delete_context(easy2d_context* pContext);

/// Retrieves a pointer to the given context's extra data buffer.
void* easy2d_get_context_extra_data(easy2d_context* pContext);


/// Creates a surface.
easy2d_surface* easy2d_create_surface(easy2d_context* pContext, float width, float height);

/// Deletes the given surface.
void easy2d_delete_surface(easy2d_surface* pSurface);

/// Retrieves the width of the surface.
float easy2d_get_surface_width(const easy2d_surface* pSurface);

/// Retrieves the height of the surface.
float easy2d_get_surface_height(const easy2d_surface* pSurface);

/// Retrieves a pointer to the given surface's extra data buffer.
void* easy2d_get_surface_extra_data(easy2d_surface* pSurface);



//// Drawing ////

/// Marks the beginning of a paint operation.
void easy2d_begin_draw(easy2d_surface* pSurface);

/// Marks the end of a paint operation.
void easy2d_end_draw(easy2d_surface* pSurface);

/// Clears the given surface with the given color.
void easy2d_clear(easy2d_surface* pSurface, easy2d_color color);

/// Draws a filled rectangle without an outline.
void easy2d_draw_rect(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color);

/// Draws the outline of the given rectangle.
void easy2d_draw_rect_outline(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth);

/// Draws a filled rectangle with an outline.
void easy2d_draw_rect_with_outline(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float outlineWidth, easy2d_color outlineColor);

/// Draws a filled rectangle without an outline with rounded corners.
void easy2d_draw_round_rect(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius);

/// Draws the outline of the given rectangle with rounded corners.
void easy2d_draw_round_rect_outline(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth);

/// Draws a filled rectangle with an outline.
void easy2d_draw_round_rect_with_outline(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color, float radius, float outlineWidth, easy2d_color outlineColor);

/// Draws a run of text.
void easy2d_draw_text(easy2d_surface* pSurface, const char* text, unsigned int textSizeInBytes, float posX, float posY, easy2d_font font, easy2d_color color, easy2d_color backgroundColor);

/// Sets the clipping rectangle.
void easy2d_set_clip(easy2d_surface* pSurface, float left, float top, float right, float bottom);

/// Retrieves the clipping rectangle.
void easy2d_get_clip(easy2d_surface* pSurface, float* pLeftOut, float* pTopOut, float* pRightOut, float* pBottomOut);

/// Creates a font that can be passed to easy2d_draw_text().
easy2d_font easy2d_create_font(easy2d_context* pContext, const char* family, unsigned int size, easy2d_font_weight weight, easy2d_font_slant slant, float rotation);

/// Deletes a font that was previously created with easy2d_create_font()
void easy2d_delete_font(easy2d_context* pContext, easy2d_font font);

/// Retrieves the metrics of the given font.
bool easy2d_get_font_metrics(easy2d_context* pContext, easy2d_font font, easy2d_font_metrics* pMetricsOut);

/// Retrieves the metrics of the glyph for the given character when rendered with the given font.
bool easy2d_get_glyph_metrics(easy2d_context* pContext, unsigned int utf32, easy2d_font font, easy2d_glyph_metrics* pGlyphMetrics);

/// Retrieves the dimensions of the given string when drawn with the given font.
bool easy2d_measure_string(easy2d_context* pContext, easy2d_font font, const char* text, unsigned int textSizeInBytes, float* pWidthOut, float* pHeightOut);



/////////////////////////////////////////////////////////////////
//
// UTILITY API
//
/////////////////////////////////////////////////////////////////

/// Creates a color object from a set of RGBA color components.
easy2d_color easy2d_rgba(easy2d_byte r, easy2d_byte g, easy2d_byte b, easy2d_byte a);

/// Creates a fully opaque color object from a set of RGB color components.
easy2d_color easy2d_rgb(easy2d_byte r, easy2d_byte g, easy2d_byte b);




/////////////////////////////////////////////////////////////////
//
// WINDOWS GDI 2D API
//
// When using GDI as the rendering back-end you will usually want to only call drawing functions in response to a WM_PAINT message.
//
/////////////////////////////////////////////////////////////////
#ifndef EASY2D_NO_GDI

/// Creates a 2D context with GDI as the backend.
easy2d_context* easy2d_create_context_gdi();

/// Creates a surface that draws directly to the given window.
///
/// @remarks
///     When using this kind of surface, the internal HBITMAP is not used.
easy2d_surface* easy2d_create_surface_gdi_HWND(easy2d_context* pContext, HWND hWnd);

/// Retrieves the internal HDC that we have been rendering to for the given surface.
///
/// @remarks
///     This assumes the given surface was created from a context that was created from easy2d_create_context_gdi()
HDC easy2d_get_HDC(easy2d_surface* pSurface);

/// Retrieves the internal HBITMAP object that we have been rendering to.
///
/// @remarks
///     This assumes the given surface was created from a context that was created from easy2d_create_context_gdi().
HBITMAP easy2d_get_HBITMAP(easy2d_surface* pSurface);

#endif  // GDI



/////////////////////////////////////////////////////////////////
//
// CAIRO 2D API
//
/////////////////////////////////////////////////////////////////
#ifndef EASY2D_NO_CAIRO
#include <cairo/cairo.h>

/// Creates a 2D context with Cairo as the backend.
easy2d_context* easy2d_create_context_cairo();

/// Retrieves the internal cairo_surface_t object from the given surface.
///
/// @remarks
///     This assumes the given surface was created from a context that was created with easy2d_create_context_cairo().
cairo_surface_t* easy2d_get_cairo_surface_t(easy2d_surface* pSurface);

/// Retrieves the internal cairo_t object from the given surface.
cairo_t* easy2d_get_cairo_t(easy2d_surface* pSurface);

#endif  // Cairo


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
