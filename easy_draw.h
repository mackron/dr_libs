// Public Domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
// - Drawing must be done inside a easy2d_begin_draw() and easy2d_end_draw() pair. Rationale: 1) required for compatibility
//   with GDI's BeginPaint() and EndPaint() APIs; 2) gives implementations opportunity to save and restore state, such as
//   OpenGL state and whatnot.
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

#define EASY2D_FALSE        0
#define EASY2D_TRUE         1


typedef unsigned char easy2d_byte;
typedef int           easy2d_bool;

typedef struct easy2d_context easy2d_context;
typedef struct easy2d_surface easy2d_surface;
typedef struct easy2d_color easy2d_color;
typedef struct easy2d_drawing_callbacks easy2d_drawing_callbacks;

/// Structure representing an RGBA color. Color components are specified in the range of 0 - 255.
struct easy2d_color
{
    easy2d_byte r;
    easy2d_byte g;
    easy2d_byte b;
    easy2d_byte a;
};


typedef easy2d_bool (* easy2d_on_create_context_proc)(easy2d_context* pContext);
typedef void        (* easy2d_on_delete_context_proc)(easy2d_context* pContext);
typedef easy2d_bool (* easy2d_on_create_surface_proc)(easy2d_surface* pSurface, float width, float height);
typedef void        (* easy2d_on_delete_surface_proc)(easy2d_surface* pSurface);
typedef void        (* easy2d_begin_draw_proc)(easy2d_surface* pSurface);
typedef void        (* easy2d_end_draw_proc)(easy2d_surface* pSurface);
typedef void        (* easy2d_draw_rect_proc)(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color);


struct easy2d_drawing_callbacks
{
    easy2d_on_create_context_proc on_create_context;
    easy2d_on_delete_context_proc on_delete_context;
    easy2d_on_create_surface_proc on_create_surface;
    easy2d_on_delete_surface_proc on_delete_surface;

    easy2d_begin_draw_proc     begin_draw;
    easy2d_end_draw_proc       end_draw;
    easy2d_draw_rect_proc      draw_rect;
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

/// Draws a rectangle.
void easy2d_draw_rect(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color);




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
// When GDI as the rendering back-end you will usually want to only call drawing in response to a WM_PAINT message.
//
/////////////////////////////////////////////////////////////////
#ifndef EASY2D_NO_GDI

/// Creates a 2D context with GDI as the backend.
easy2d_context* easy2d_create_context_gdi();

/// Retrieves the internal HDC that we have been rendering to for the given context.
///
/// @remarks
///     This assumes the given surface was created from a context that was created from easy2d_create_context_gdi()
HDC easy2d_get_HDC(easy2d_context* pContext);

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
