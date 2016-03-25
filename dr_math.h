// Public Domain. See "unlicense" statement at the end of this file.

// NOTE: This is still very much work in progress and is only being updated as I need it. You don't want to be using this library
//       in it's current state.

// QUICK NOTES
// - This library does not use SSE for it's basic types (vec4, etc.). Rationale: 1) It keeps things simple; 2) SSE is not always
//   faster than the FPU(s) on modern CPUs; 3) The library can always implement functions that work on __m128 variables directly
//   in the future if the need arises.
// - Use DISABLE_SSE to disable SSE implementations.
// - Angles are always specified in degrees, unless otherwise noted. Rationale: Degrees are easier to use for humans which means
//   the API is easier to use.
//   - Use radians() and degrees() to convert between the two.

#ifndef dr_math_h
#define dr_math_h

#include <math.h>

#if defined(_MSC_VER)
#define DR_MATHCALL static __forceinline
#else
#define DR_MATHCALL static inline
#endif

#define DR_PI       3.14159265358979323846

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float x;
    float y;
    float z;
    float w;
} vec4;

typedef struct
{
    float x;
    float y;
    float z;
} vec3;

typedef struct
{
    float x;
    float y;
} vec2;

typedef struct
{
    vec4 col0;
    vec4 col1;
    vec4 col2;
    vec4 col3;
} mat4;

typedef struct
{
    float x;
    float y;
    float z;
    float w; 
} quat;


// Radians to degrees.
DR_MATHCALL float degrees(float radians)
{
    return radians * 57.29577951308232087685f;
}

// Degrees to radians.
DR_MATHCALL float radians(float degrees)
{
    return degrees * 0.01745329251994329577f;
}



///////////////////////////////////////////////
//
// VEC4
//
///////////////////////////////////////////////

DR_MATHCALL vec4 vec4f(float x, float y, float z, float w)
{
    vec4 result;
    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;

    return result;
}
DR_MATHCALL vec4 vec4v(const float* v)
{
    return vec4f(v[0], v[1], v[2], v[3]);
}
DR_MATHCALL vec4 vec4_zero()
{
    return vec4f(0, 0, 0, 0);
}
DR_MATHCALL vec4 vec4_one()
{
    return vec4f(1, 1, 1, 1);
}



DR_MATHCALL vec4 vec4_add(vec4 a, vec4 b)
{
    return vec4f(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

DR_MATHCALL vec4 vec4_sub(vec4 a, vec4 b)
{
    return vec4f(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}


DR_MATHCALL vec4 vec4_mul(vec4 a, vec4 b)
{
    return vec4f(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}
DR_MATHCALL vec4 vec4_mul_1f(vec4 a, float x)
{
    return vec4f(a.x * x, a.y * x, a.z * x, a.w * x);
}


DR_MATHCALL vec4 vec4_div(vec4 a, vec4 b)
{
    return vec4f(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}



///////////////////////////////////////////////
//
// VEC3
//
///////////////////////////////////////////////

DR_MATHCALL vec3 vec3f(float x, float y, float z)
{
    vec3 result;
    result.x = x;
    result.y = y;
    result.z = z;

    return result;
}
DR_MATHCALL vec3 vec3v(const float* v)
{
    return vec3f(v[0], v[1], v[2]);
}
DR_MATHCALL vec3 vec3_zero()
{
    return vec3f(0, 0, 0);
}
DR_MATHCALL vec3 vec3_one()
{
    return vec3f(1, 1, 1);
}


DR_MATHCALL vec3 vec3_add(vec3 a, vec3 b)
{
    return vec3f(a.x + b.x, a.y + b.y, a.z + b.z);
}

DR_MATHCALL vec3 vec3_sub(vec3 a, vec3 b)
{
    return vec3f(a.x - b.x, a.y - b.y, a.z - b.z);
}


DR_MATHCALL vec3 vec3_mul(vec3 a, vec3 b)
{
    return vec3f(a.x * b.x, a.y * b.y, a.z * b.z);
}
DR_MATHCALL vec3 vec3_mul_1f(vec3 a, float x)
{
    return vec3f(a.x * x, a.y * x, a.z * x);
}


DR_MATHCALL vec3 vec3_div(vec3 a, vec3 b)
{
    return vec3f(a.x / b.x, a.y / b.y, a.z / b.z);
}



///////////////////////////////////////////////
//
// MAT4
//
///////////////////////////////////////////////

DR_MATHCALL mat4 mat4f(vec4 col0, vec4 col1, vec4 col2, vec4 col3)
{
    mat4 result;
    result.col0 = col0;
    result.col1 = col1;
    result.col2 = col2;
    result.col3 = col3;

    return result;
}

DR_MATHCALL mat4 mat4_identity()
{
    mat4 result;
    result.col0 = vec4f(1, 0, 0, 0);
    result.col1 = vec4f(0, 1, 0, 0);
    result.col2 = vec4f(0, 0, 1, 0);
    result.col3 = vec4f(0, 0, 0, 1);

    return result;
}

DR_MATHCALL mat4 mat4_ortho(float left, float right, float bottom, float top, float znear, float zfar)
{
    float rml = right - left;
    float tmb = top - bottom;
    float fmn = zfar - znear;

    float rpl = right + left;
    float tpb = top + bottom;
    float fpn = zfar + znear;

    mat4 result;
    result.col0 = vec4f(2/rml, 0, 0,  0);
    result.col1 = vec4f(0, 2/tmb, 0,  0);
    result.col2 = vec4f(0, 0, -2/fmn, 0);
    result.col3 = vec4f(-(rpl/rml), -(tpb/tmb), -(fpn/fmn), 1);

    return result;
}



///////////////////////////////////////////////
//
// QUAT
//
///////////////////////////////////////////////

DR_MATHCALL quat quatf(float x, float y, float z, float w)
{
    quat result;
    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;

    return result;
}
DR_MATHCALL quat quatv(const float* v)
{
    return quatf(v[0], v[1], v[2], v[3]);
}

DR_MATHCALL quat quat_identity()
{
    return quatf(0, 0, 0, 1);
}





///////////////////////////////////////////////
//
// TRANSFORM
//
///////////////////////////////////////////////

typedef struct
{
    vec3 position;
    quat rotation;
    vec3 scale;
}transform_t;

DR_MATHCALL transform_t transform_init(vec3 position, quat rotation, vec3 scale)
{
    transform_t result;
    result.position = position;
    result.rotation = rotation;
    result.scale    = scale;

    return result;
}

DR_MATHCALL transform_t transform_identity()
{
    transform_t result;
    result.position = vec3_zero();
    result.rotation = quat_identity();
    result.scale    = vec3_one();

    return result;
}


DR_MATHCALL transform_t transform_translate(transform_t transform, vec3 offset)
{
    transform_t result = transform;
    result.position = vec3_add(transform.position, offset);

    return result;
}




///////////////////////////////////////////////
//
// SSE IMPLEMENTATION
//
///////////////////////////////////////////////

// Not supporting SSE on x86/MSVC due to pass-by-value errors with aligned types.
#if (defined(_MSC_VER) && defined(_M_X64)) || defined(__SSE2__)
#define SUPPORTS_SSE
#endif

#if !defined(DISABLE_SSE) && defined(SUPPORTS_SSE)
#define ENABLE_SSE
#endif

#ifdef ENABLE_SSE
#if defined(__MINGW32__)
#include <intrin.h>
#endif
#include <emmintrin.h>
#endif





#ifdef __cplusplus
}
#endif

#endif  //dr_math_h

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
