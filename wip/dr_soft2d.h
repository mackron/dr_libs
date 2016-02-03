// Public domain. See "unlicense" statement at the end of this file.

// ABOUT
// 
// This is just a very work-in-progress research project which implements a basic 2D rasterizer. If you've stumbled across this you
// don't want to be using this library.
//
//
//
// USAGE
//
// This is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_SOFT2D_IMPLEMENTATION
//   #include "dr_soft2d.h"
//
// You can then #include this file in other parts of the program as you would with any other header file.

#ifndef dr_soft2d_h
#define dr_soft2d_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct soft2d_context soft2d_context;
typedef struct soft2d_surface soft2d_surface;
typedef struct soft2d_color soft2d_color;
typedef struct soft2d_rect soft2d_rect;

struct soft2d_color
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct soft2d_rect
{
    int left;
    int top;
    int right;
    int bottom;
};


/// soft2d_create_context()
soft2d_context* soft2d_create_context();

/// soft2d_delete_context()
void soft2d_delete_context(soft2d_context* pContext);


/// soft2d_create_surface()
///
/// @remarks
///     <stride> and <pDstBuffer> can be 0 and null respectively, in which case the memory will be managed internally.
soft2d_surface* soft2d_create_surface(soft2d_context* pContext, int width, int height, int stride, void* pDstBuffer);

/// soft2d_delete_surface()
void soft2d_delete_surface(soft2d_surface* pSurface);

/// soft2d_get_surface_width()
int soft2d_get_surface_width(soft2d_surface* pSurface);

/// soft2d_get_surface_height()
int soft2d_get_surface_height(soft2d_surface* pSurface);

/// soft2d_get_surface_stride()
int soft2d_get_surface_stride(soft2d_surface* pSurface);

/// soft2d_get_surface_data()
void* soft2d_get_surface_data(soft2d_surface* pSurface);


/// soft2d_clear()
void soft2d_clear(soft2d_surface* pSurface, soft2d_color color);

/// soft2d_draw_rect()
void soft2d_draw_rect(soft2d_surface* pSurface, soft2d_rect rect, soft2d_color color);

/// soft2d_draw_line()
void soft2d_draw_line(soft2d_surface* pSurface, int p0X, int p0Y, int p1X, int p1Y, soft2d_color color);

/// soft2d_draw_point()
void soft2d_draw_point(soft2d_surface* pSurface, int pX, int pY, soft2d_color color);


/// Creates a soft2d_color object.
soft2d_color soft2d_rgb(unsigned char r, unsigned int g, unsigned int b);

/// Creates a soft2d_color object.
soft2d_color soft2d_rgba(unsigned char r, unsigned int g, unsigned int b, unsigned int a);


/// Creates a rectangle.
soft2d_rect soft2d_make_rect(int left, int top, int right, int bottom);


#ifdef __cplusplus
}
#endif

#endif  //dr_soft2d_h


///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DR_SOFT2D_IMPLEMENTATION
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


float soft2d_absf(float x) {
    return (x < 0) ? -x : x;
}


// At the moment the context is not actually used. In the future we may use it for things like caching threads.
struct soft2d_context
{
    int unused;
};

struct soft2d_surface
{
    /// A pointer to the context that owns the surface.
    soft2d_context* pContext;

    /// The width of the surface.
    int width;

    /// The height of the surface.
    int height;

    /// The size in bytes of each row in the surface. This is different from the width because we may want to do alignment.
    int stride;

    /// A pointer to the raw color data of the surface.
    unsigned char* pData;

    /// Whether or not we own the surface's data buffer.
    bool ownsData;
};


soft2d_context* soft2d_create_context()
{
    soft2d_context* pContext = malloc(sizeof(*pContext));
    if (pContext == NULL) {
        return NULL;
    }

    pContext->unused = 0;

    return pContext;
}

void soft2d_delete_context(soft2d_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    free(pContext);
}


soft2d_surface* soft2d_create_surface(soft2d_context* pContext, int width, int height, int stride, void* pDstBuffer)
{
    if (pContext == NULL || width == 0 || height == 0) {
        return NULL;
    }

    if (stride == 0)
    {
        if (pDstBuffer != NULL) {
            return NULL;
        }

        stride = width*4;
    }


    soft2d_surface* pSurface = malloc(sizeof(*pSurface));
    if (pSurface == NULL) {
        return NULL;
    }

    pSurface->width    = width;
    pSurface->height   = height;
    pSurface->stride   = stride;
    pSurface->pData    = pDstBuffer;
    pSurface->ownsData = false;

    if (pSurface->pData == NULL) {
        pSurface->pData = malloc(pSurface->height * pSurface->stride);
        if (pSurface->pData == NULL) {
            free(pSurface);
            return NULL;
        }

        pSurface->ownsData = true;
    }

    return pSurface;
}

void soft2d_delete_surface(soft2d_surface* pSurface)
{
    if (pSurface == NULL) {
        return;
    }

    if (pSurface->ownsData) {
        free(pSurface->pData);
    }

    free(pSurface);
}

int soft2d_get_surface_width(soft2d_surface* pSurface)
{
    if (pSurface == NULL) {
        return 0;
    }

    return pSurface->width;
}

int soft2d_get_surface_height(soft2d_surface* pSurface)
{
    if (pSurface == NULL) {
        return 0;
    }

    return pSurface->height;
}

int soft2d_get_surface_stride(soft2d_surface* pSurface)
{
    if (pSurface == NULL) {
        return 0;
    }

    return pSurface->stride;
}

void* soft2d_get_surface_data(soft2d_surface* pSurface)
{
    if (pSurface == NULL) {
        return NULL;
    }

    return pSurface->pData;
}


void soft2d_clear(soft2d_surface* pSurface, soft2d_color color)
{
    if (pSurface == NULL) {
        return;
    }

    const uint32_t src = (color.a << 24 | color.r << 16 | color.g << 8 | color.b);

    // Naive implementation for now.
    for (int iRow = 0; iRow < pSurface->height; ++iRow)
    {
        uint32_t* pRow = (uint32_t*)(pSurface->pData + (iRow * pSurface->stride));

        for (int iCol = 0; iCol < pSurface->width; ++iCol)
        {
            pRow[iCol] = src;
        }
    }
}

void soft2d_draw_rect(soft2d_surface* pSurface, soft2d_rect rect, soft2d_color color)
{
    if (pSurface == NULL) {
        return;
    }

    // The rectangle needs to be clamped.
    soft2d_rect rect2 = rect;
    if (rect2.left < 0) {
        rect2.left = 0;
    }
    if (rect2.top < 0) {
        rect2.top = 0;
    }
    if (rect2.right > (int)pSurface->width) {
        rect2.right = (int)pSurface->width;
    }
    if (rect2.bottom > (int)pSurface->height) {
        rect2.bottom = (int)pSurface->height;
    }


    const uint32_t srcColor = (color.a << 24 | color.r << 16 | color.g << 8 | color.b);
    for (int iRow = rect.top; iRow < rect.bottom; ++iRow)
    {
        uint32_t* pRow = (uint32_t*)(pSurface->pData + (iRow * pSurface->stride));

        for (int iCol = rect.left; iCol < rect.right; ++iCol)
        {
            pRow[iCol] = srcColor;
        }
    }
}

void soft2d_draw_line(soft2d_surface* pSurface, int p0X, int p0Y, int p1X, int p1Y, soft2d_color color)
{
    if (pSurface == NULL) {
        return;
    }

    const uint32_t srcColor = (color.a << 24 | color.r << 16 | color.g << 8 | color.b);

    // Stright vertical line.
    if (p0X == p1X)
    {
        if (p0Y > p1Y)
        {
            int temp = p0Y;
            p0Y = p1Y;
            p1Y = temp;
        }

        if (p0Y > pSurface->height) {
            return;
        }
        if (p1Y < 0) {
            return;
        }
        if (p0X > pSurface->width) {
            return;
        }
        if (p0X < 0) {
            return;
        }

        // Clip the line.
        if (p0Y < 0) {
            p0Y = 0;
        }
        if (p1Y > pSurface->height) {
            p1Y = pSurface->height;
        }
        

        unsigned char* pRow = pSurface->pData + (p0Y * pSurface->stride);
        for (int iRow = p0Y; iRow < p1Y; ++iRow)
        {
            ((uint32_t*)pRow)[p0X] = srcColor;
            pRow += pSurface->stride;
        }

        return;
    }


    // Stright horizontal line.
    if (p0Y == p1Y)
    {
        if (p0X > p1X)
        {
            int temp = p0X;
            p0X = p1X;
            p1X = temp;
        }

        if (p0X > pSurface->width) {
            return;
        }
        if (p1X < 0) {
            return;
        }
        if (p0Y > pSurface->height) {
            return;
        }
        if (p0Y < 0) {
            return;
        }

        // Clip the line.
        if (p0X < 0) {
            p0X = 0;
        }
        if (p1X > pSurface->width) {
            p1X = pSurface->width;
        }

        unsigned char* pRow = pSurface->pData + (p0Y * pSurface->stride);
        for (int iCol = p0X; iCol < p1X; ++iCol)
        {
            ((uint32_t*)pRow)[iCol] = srcColor;
        }

        return;
    }


    // Diagonal line. This is the complex case.
    // TEST: Try improving efficiency by pre-clipping the line against the rectangle.

    {
        if (p0X > p1X) {
            int temp = p0X;
            p0X = p1X;
            p1X = temp;

            temp = p0Y;
            p0Y = p1Y;
            p1Y = temp;
        }

        float deltax   = (float)p1X - p0X;
        float deltay   = (float)p1Y - p0Y;
        float error    = 0;
        float deltaerr = soft2d_absf(deltay / deltax);

        int ysign = ((p1Y - p0Y) < 0) ? -1 : 1;
        int y = p0Y;
        for (int x = p0X; x < p1X; ++x)
        {
            soft2d_draw_point(pSurface, x, y, color);

            error += deltaerr;
            while (error >= 0.5f)
            {
                soft2d_draw_point(pSurface, x, y, color);
                y += ysign;
                error -= 1;
            }
        }
    }
}

void soft2d_draw_point(soft2d_surface* pSurface, int pX, int pY, soft2d_color color)
{
    if (pSurface == NULL) {
        return;
    }

    if (pX < 0 || pX >= pSurface->width) {
        return;
    }
    if (pY < 0 || pY >= pSurface->height) {
        return;
    }


    const uint32_t srcColor = (color.a << 24 | color.r << 16 | color.g << 8 | color.b);
    if (color.a < 255)
    {
        /// TODO: Transparency.
    }
    
    uint32_t* pRow = (uint32_t*)(pSurface->pData + (pY * pSurface->stride));
    pRow[pX] = srcColor;
}


soft2d_color soft2d_rgb(unsigned char r, unsigned int g, unsigned int b)
{
    return (soft2d_color){r, g, b, 255};
}

soft2d_color soft2d_rgba(unsigned char r, unsigned int g, unsigned int b, unsigned int a)
{
    return (soft2d_color){r, g, b, a};
}


soft2d_rect soft2d_make_rect(int left, int top, int right, int bottom)
{
    return (soft2d_rect){left, top, right, bottom};
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