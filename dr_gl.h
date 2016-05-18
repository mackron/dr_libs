// OpenGL API loader. Public Domain. See "unlicense" statement at the end of this file.

// !!!! THIS IS INCOMPLETE AND EXPERIMENTAL !!!!

// USAGE
//
// dr_gl is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_GL_IMPLEMENTATION
//   #include "dr_gl.h"
//
// You can then #include dr_gl.h in other parts of the program as you would with any other header file. This library is
// just an API loader, but what makes it slightly different is that it's all done in a single file, and you enable only
// the features you need by specifying #define options before including this file. The rationale for this design is that
// it avoids wasting time loading unneeded APIs and also saves a tiny amount of memory.
//
// By default, dr_gl will enable OpenGL 2.1 core APIs but no extensions. All extensions need to be explicitly enabled.
// Not every extension is supported. See below for a list of supported extensions.
//
// When using dr_gl, you will not need to link to OpenGL32.lib or libGL.so, but you will need to have gl/gl.h and family
// in your include paths. This should not be a problem in practice.
//
//
// OPTIONS
// #define these before including this file.
//
// #define DR_GL_VERSION <version>
//   The minimum version of OpenGL to support. Set the value to one of the values below. Defaults to 210.
//   110 - OpenGL 1.1
//   120 - OpenGL 1.2
//   130 - OpenGL 1.3
//   140 - OpenGL 1.4
//   150 - OpenGL 1.5
//   200 - OpenGL 2.0
//   210 - OpenGL 2.1
//
// #define DR_GL_ENABLE_EXT_swap_control
// #define DR_GL_ENABLE_EXT_framebuffer_blit
// #define DR_GL_ENABLE_EXT_framebuffer_multisample
// #define DR_GL_ENABLE_EXT_framebuffer_object

#ifndef dr_gl_h
#define dr_gl_h

#ifdef __cplusplus
extern "C" {
#endif

#include <gl/gl.h>
#include <gl/glext.h>

#ifdef _WIN32
#include <gl/wglext.h>
#endif

#ifndef DR_GL_VERSION
#define DR_GL_VERSION   210
#endif

typedef void (APIENTRYP PFNGLACCUM) (GLenum op, GLfloat value);
typedef void (APIENTRYP PFNGLALPHAFUNC) (GLenum func, GLclampf ref);
typedef GLboolean (APIENTRYP PFNGLARETEXTURESRESIDENT) (GLsizei n, const GLuint *textures, GLboolean *residences);
typedef void (APIENTRYP PFNGLARRAYELEMENT) (GLint i);
typedef void (APIENTRYP PFNGLBEGIN) (GLenum mode);
typedef void (APIENTRYP PFNGLBINDTEXTURE) (GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLBITMAP) (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
typedef void (APIENTRYP PFNGLBLENDFUNC) (GLenum sfactor, GLenum dfactor);
typedef void (APIENTRYP PFNGLCALLLIST) (GLuint list);
typedef void (APIENTRYP PFNGLCALLLISTS) (GLsizei n, GLenum type, const GLvoid *lists);
typedef void (APIENTRYP PFNGLCLEAR) (GLbitfield mask);
typedef void (APIENTRYP PFNGLCLEARACCUM) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNGLCLEARCOLOR) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (APIENTRYP PFNGLCLEARDEPTH) (GLclampd depth);
typedef void (APIENTRYP PFNGLCLEARINDEX) (GLfloat c);
typedef void (APIENTRYP PFNGLCLEARSTENCIL) (GLint s);
typedef void (APIENTRYP PFNGLCLIPPLANE) (GLenum plane, const GLdouble *equation);
typedef void (APIENTRYP PFNGLCOLOR3B) (GLbyte red, GLbyte green, GLbyte blue);
typedef void (APIENTRYP PFNGLCOLOR3BV) (const GLbyte *v);
typedef void (APIENTRYP PFNGLCOLOR3D) (GLdouble red, GLdouble green, GLdouble blue);
typedef void (APIENTRYP PFNGLCOLOR3DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLCOLOR3F) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (APIENTRYP PFNGLCOLOR3FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLCOLOR3I) (GLint red, GLint green, GLint blue);
typedef void (APIENTRYP PFNGLCOLOR3IV) (const GLint *v);
typedef void (APIENTRYP PFNGLCOLOR3S) (GLshort red, GLshort green, GLshort blue);
typedef void (APIENTRYP PFNGLCOLOR3SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLCOLOR3UB) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (APIENTRYP PFNGLCOLOR3UBV) (const GLubyte *v);
typedef void (APIENTRYP PFNGLCOLOR3UI) (GLuint red, GLuint green, GLuint blue);
typedef void (APIENTRYP PFNGLCOLOR3UIV) (const GLuint *v);
typedef void (APIENTRYP PFNGLCOLOR3US) (GLushort red, GLushort green, GLushort blue);
typedef void (APIENTRYP PFNGLCOLOR3USV) (const GLushort *v);
typedef void (APIENTRYP PFNGLCOLOR4B) (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
typedef void (APIENTRYP PFNGLCOLOR4BV) (const GLbyte *v);
typedef void (APIENTRYP PFNGLCOLOR4D) (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
typedef void (APIENTRYP PFNGLCOLOR4DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLCOLOR4F) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNGLCOLOR4FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLCOLOR4I) (GLint red, GLint green, GLint blue, GLint alpha);
typedef void (APIENTRYP PFNGLCOLOR4IV) (const GLint *v);
typedef void (APIENTRYP PFNGLCOLOR4S) (GLshort red, GLshort green, GLshort blue, GLshort alpha);
typedef void (APIENTRYP PFNGLCOLOR4SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLCOLOR4UB) (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
typedef void (APIENTRYP PFNGLCOLOR4UBV) (const GLubyte *v);
typedef void (APIENTRYP PFNGLCOLOR4UI) (GLuint red, GLuint green, GLuint blue, GLuint alpha);
typedef void (APIENTRYP PFNGLCOLOR4UIV) (const GLuint *v);
typedef void (APIENTRYP PFNGLCOLOR4US) (GLushort red, GLushort green, GLushort blue, GLushort alpha);
typedef void (APIENTRYP PFNGLCOLOR4USV) (const GLushort *v);
typedef void (APIENTRYP PFNGLCOLORMASK) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (APIENTRYP PFNGLCOLORMATERIAL) (GLenum face, GLenum mode);
typedef void (APIENTRYP PFNGLCOLORPOINTER) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP PFNGLCOPYPIXELS) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
typedef void (APIENTRYP PFNGLCOPYTEXIMAGE1D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (APIENTRYP PFNGLCOPYTEXIMAGE2D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (APIENTRYP PFNGLCOPYTEXSUBIMAGE1D) (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (APIENTRYP PFNGLCOPYTEXSUBIMAGE2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLCULLFACE) (GLenum mode);
typedef void (APIENTRYP PFNGLDELETELISTS) (GLuint list, GLsizei range);
typedef void (APIENTRYP PFNGLDELETETEXTURES) (GLsizei n, const GLuint *textures);
typedef void (APIENTRYP PFNGLDEPTHFUNC) (GLenum func);
typedef void (APIENTRYP PFNGLDEPTHMASK) (GLboolean flag);
typedef void (APIENTRYP PFNGLDEPTHRANGE) (GLclampd zNear, GLclampd zFar);
typedef void (APIENTRYP PFNGLDISABLE) (GLenum cap);
typedef void (APIENTRYP PFNGLDISABLECLIENTSTATE) (GLenum array);
typedef void (APIENTRYP PFNGLDRAWARRAYS) (GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP PFNGLDRAWBUFFER) (GLenum mode);
typedef void (APIENTRYP PFNGLDRAWELEMENTS) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (APIENTRYP PFNGLDRAWPIXELS) (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLEDGEFLAG) (GLboolean flag);
typedef void (APIENTRYP PFNGLEDGEFLAGPOINTER) (GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP PFNGLEDGEFLAGV) (const GLboolean *flag);
typedef void (APIENTRYP PFNGLENABLE) (GLenum cap);
typedef void (APIENTRYP PFNGLENABLECLIENTSTATE) (GLenum array);
typedef void (APIENTRYP PFNGLEND) (void);
typedef void (APIENTRYP PFNGLENDLIST) (void);
typedef void (APIENTRYP PFNGLEVALCOORD1D) (GLdouble u);
typedef void (APIENTRYP PFNGLEVALCOORD1DV) (const GLdouble *u);
typedef void (APIENTRYP PFNGLEVALCOORD1F) (GLfloat u);
typedef void (APIENTRYP PFNGLEVALCOORD1FV) (const GLfloat *u);
typedef void (APIENTRYP PFNGLEVALCOORD2D) (GLdouble u, GLdouble v);
typedef void (APIENTRYP PFNGLEVALCOORD2DV) (const GLdouble *u);
typedef void (APIENTRYP PFNGLEVALCOORD2F) (GLfloat u, GLfloat v);
typedef void (APIENTRYP PFNGLEVALCOORD2FV) (const GLfloat *u);
typedef void (APIENTRYP PFNGLEVALMESH1) (GLenum mode, GLint i1, GLint i2);
typedef void (APIENTRYP PFNGLEVALMESH2) (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
typedef void (APIENTRYP PFNGLEVALPOINT1) (GLint i);
typedef void (APIENTRYP PFNGLEVALPOINT2) (GLint i, GLint j);
typedef void (APIENTRYP PFNGLFEEDBACKBUFFER) (GLsizei size, GLenum type, GLfloat *buffer);
typedef void (APIENTRYP PFNGLFINISH) (void);
typedef void (APIENTRYP PFNGLFLUSH) (void);
typedef void (APIENTRYP PFNGLFOGF) (GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLFOGFV) (GLenum pname, const GLfloat *params);
typedef void (APIENTRYP PFNGLFOGI) (GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLFOGIV) (GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLFRONTFACE) (GLenum mode);
typedef void (APIENTRYP PFNGLFRUSTUM) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
typedef GLuint (APIENTRYP PFNGLGENLISTS) (GLsizei range);
typedef void (APIENTRYP PFNGLGENTEXTURES) (GLsizei n, GLuint *textures);
typedef void (APIENTRYP PFNGLGETBOOLEANV) (GLenum pname, GLboolean *params);
typedef void (APIENTRYP PFNGLGETCLIPPLANE) (GLenum plane, GLdouble *equation);
typedef void (APIENTRYP PFNGLGETDOUBLEV) (GLenum pname, GLdouble *params);
typedef GLenum (APIENTRYP PFNGLGETERROR) (void);
typedef void (APIENTRYP PFNGLGETFLOATV) (GLenum pname, GLfloat *params);
typedef void (APIENTRYP PFNGLGETINTEGERV) (GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETLIGHTFV) (GLenum light, GLenum pname, GLfloat *params);
typedef void (APIENTRYP PFNGLGETLIGHTIV) (GLenum light, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETMAPDV) (GLenum target, GLenum query, GLdouble *v);
typedef void (APIENTRYP PFNGLGETMAPFV) (GLenum target, GLenum query, GLfloat *v);
typedef void (APIENTRYP PFNGLGETMAPIV) (GLenum target, GLenum query, GLint *v);
typedef void (APIENTRYP PFNGLGETMATERIALFV) (GLenum face, GLenum pname, GLfloat *params);
typedef void (APIENTRYP PFNGLGETMATERIALIV) (GLenum face, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETPIXELMAPFV) (GLenum map, GLfloat *values);
typedef void (APIENTRYP PFNGLGETPIXELMAPUIV) (GLenum map, GLuint *values);
typedef void (APIENTRYP PFNGLGETPIXELMAPUSV) (GLenum map, GLushort *values);
typedef void (APIENTRYP PFNGLGETPOINTERV) (GLenum pname, GLvoid* *params);
typedef void (APIENTRYP PFNGLGETPOLYGONSTIPPLE) (GLubyte *mask);
typedef const GLubyte * (APIENTRYP PFNGLGETSTRING) (GLenum name);
typedef void (APIENTRYP PFNGLGETTEXENVFV) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP PFNGLGETTEXENVIV) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETTEXGENDV) (GLenum coord, GLenum pname, GLdouble *params);
typedef void (APIENTRYP PFNGLGETTEXGENFV) (GLenum coord, GLenum pname, GLfloat *params);
typedef void (APIENTRYP PFNGLGETTEXGENIV) (GLenum coord, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETTEXIMAGE) (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
typedef void (APIENTRYP PFNGLGETTEXLEVELPARAMETERFV) (GLenum target, GLint level, GLenum pname, GLfloat *params);
typedef void (APIENTRYP PFNGLGETTEXLEVELPARAMETERIV) (GLenum target, GLint level, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETTEXPARAMETERFV) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP PFNGLGETTEXPARAMETERIV) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLHINT) (GLenum target, GLenum mode);
typedef void (APIENTRYP PFNGLINDEXMASK) (GLuint mask);
typedef void (APIENTRYP PFNGLINDEXPOINTER) (GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP PFNGLINDEXD) (GLdouble c);
typedef void (APIENTRYP PFNGLINDEXDV) (const GLdouble *c);
typedef void (APIENTRYP PFNGLINDEXF) (GLfloat c);
typedef void (APIENTRYP PFNGLINDEXFV) (const GLfloat *c);
typedef void (APIENTRYP PFNGLINDEXI) (GLint c);
typedef void (APIENTRYP PFNGLINDEXIV) (const GLint *c);
typedef void (APIENTRYP PFNGLINDEXS) (GLshort c);
typedef void (APIENTRYP PFNGLINDEXSV) (const GLshort *c);
typedef void (APIENTRYP PFNGLINDEXUB) (GLubyte c);
typedef void (APIENTRYP PFNGLINDEXUBV) (const GLubyte *c);
typedef void (APIENTRYP PFNGLINITNAMES) (void);
typedef void (APIENTRYP PFNGLINTERLEAVEDARRAYS) (GLenum format, GLsizei stride, const GLvoid *pointer);
typedef GLboolean (APIENTRYP PFNGLISENABLED) (GLenum cap);
typedef GLboolean (APIENTRYP PFNGLISLIST) (GLuint list);
typedef GLboolean (APIENTRYP PFNGLISTEXTURE) (GLuint texture);
typedef void (APIENTRYP PFNGLLIGHTMODELF) (GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLLIGHTMODELFV) (GLenum pname, const GLfloat *params);
typedef void (APIENTRYP PFNGLLIGHTMODELI) (GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLLIGHTMODELIV) (GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLLIGHTF) (GLenum light, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLLIGHTFV) (GLenum light, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP PFNGLLIGHTI) (GLenum light, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLLIGHTIV) (GLenum light, GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLLINESTIPPLE) (GLint factor, GLushort pattern);
typedef void (APIENTRYP PFNGLLINEWIDTH) (GLfloat width);
typedef void (APIENTRYP PFNGLLISTBASE) (GLuint base);
typedef void (APIENTRYP PFNGLLOADIDENTITY) (void);
typedef void (APIENTRYP PFNGLLOADMATRIXD) (const GLdouble *m);
typedef void (APIENTRYP PFNGLLOADMATRIXF) (const GLfloat *m);
typedef void (APIENTRYP PFNGLLOADNAME) (GLuint name);
typedef void (APIENTRYP PFNGLLOGICOP) (GLenum opcode);
typedef void (APIENTRYP PFNGLMAP1D) (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);
typedef void (APIENTRYP PFNGLMAP1F) (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
typedef void (APIENTRYP PFNGLMAP2D) (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);
typedef void (APIENTRYP PFNGLMAP2F) (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
typedef void (APIENTRYP PFNGLMAPGRID1D) (GLint un, GLdouble u1, GLdouble u2);
typedef void (APIENTRYP PFNGLMAPGRID1F) (GLint un, GLfloat u1, GLfloat u2);
typedef void (APIENTRYP PFNGLMAPGRID2D) (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
typedef void (APIENTRYP PFNGLMAPGRID2F) (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP PFNGLMATERIALF) (GLenum face, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLMATERIALFV) (GLenum face, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP PFNGLMATERIALI) (GLenum face, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLMATERIALIV) (GLenum face, GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLMATRIXMODE) (GLenum mode);
typedef void (APIENTRYP PFNGLMULTMATRIXD) (const GLdouble *m);
typedef void (APIENTRYP PFNGLMULTMATRIXF) (const GLfloat *m);
typedef void (APIENTRYP PFNGLNEWLIST) (GLuint list, GLenum mode);
typedef void (APIENTRYP PFNGLNORMAL3B) (GLbyte nx, GLbyte ny, GLbyte nz);
typedef void (APIENTRYP PFNGLNORMAL3BV) (const GLbyte *v);
typedef void (APIENTRYP PFNGLNORMAL3D) (GLdouble nx, GLdouble ny, GLdouble nz);
typedef void (APIENTRYP PFNGLNORMAL3DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLNORMAL3F) (GLfloat nx, GLfloat ny, GLfloat nz);
typedef void (APIENTRYP PFNGLNORMAL3FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLNORMAL3I) (GLint nx, GLint ny, GLint nz);
typedef void (APIENTRYP PFNGLNORMAL3IV) (const GLint *v);
typedef void (APIENTRYP PFNGLNORMAL3S) (GLshort nx, GLshort ny, GLshort nz);
typedef void (APIENTRYP PFNGLNORMAL3SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLNORMALPOINTER) (GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP PFNGLORTHO) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
typedef void (APIENTRYP PFNGLPATHTHROUGH) (GLfloat token);
typedef void (APIENTRYP PFNGLPIXELMAPFV) (GLenum map, GLsizei mapsize, const GLfloat *values);
typedef void (APIENTRYP PFNGLPIXELMAPUIV) (GLenum map, GLsizei mapsize, const GLuint *values);
typedef void (APIENTRYP PFNGLPIXELMAPUSV) (GLenum map, GLsizei mapsize, const GLushort *values);
typedef void (APIENTRYP PFNGLPIXELSTOREF) (GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLPIXELSTOREI) (GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLPIXELTRANSFERF) (GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLPIXELTRANSGERI) (GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLPIXELZOOM) (GLfloat xfactor, GLfloat yfactor);
typedef void (APIENTRYP PFNGLPOINTSIZE) (GLfloat size);
typedef void (APIENTRYP PFNGLPOLYGONMODE) (GLenum face, GLenum mode);
typedef void (APIENTRYP PFNGLPOLYGONOFFSET) (GLfloat factor, GLfloat units);
typedef void (APIENTRYP PFNGLPOLYGONSTIPPLE) (const GLubyte *mask);
typedef void (APIENTRYP PFNGLPOPATTRIB) (void);
typedef void (APIENTRYP PFNGLPOPCLIENTATTRIB) (void);
typedef void (APIENTRYP PFNGLPOPMATRIX) (void);
typedef void (APIENTRYP PFNGLPOPNAME) (void);
typedef void (APIENTRYP PFNGLPRIORITIZETEXTURES) (GLsizei n, const GLuint *textures, const GLclampf *priorities);
typedef void (APIENTRYP PFNGLPUSHATTRIB) (GLbitfield mask);
typedef void (APIENTRYP PFNGLPUSHCLIENTATTRIB) (GLbitfield mask);
typedef void (APIENTRYP PFNGLPUSHMATRIX) (void);
typedef void (APIENTRYP PFNGLPUSHNAME) (GLuint name);
typedef void (APIENTRYP PFNGLRASTERPOS2D) (GLdouble x, GLdouble y);
typedef void (APIENTRYP PFNGLRASTERPOS2DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLRASTERPOS2F) (GLfloat x, GLfloat y);
typedef void (APIENTRYP PFNGLRASTERPOS2FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLRASTERPOS2I) (GLint x, GLint y);
typedef void (APIENTRYP PFNGLRASTERPOS2IV) (const GLint *v);
typedef void (APIENTRYP PFNGLRASTERPOS2S) (GLshort x, GLshort y);
typedef void (APIENTRYP PFNGLRASTERPOS2SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLRASTERPOS3D) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP PFNGLRASTERPOS3DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLRASTERPOS3F) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP PFNGLRASTERPOS3FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLRASTERPOS3I) (GLint x, GLint y, GLint z);
typedef void (APIENTRYP PFNGLRASTERPOS3IV) (const GLint *v);
typedef void (APIENTRYP PFNGLRASTERPOS3S) (GLshort x, GLshort y, GLshort z);
typedef void (APIENTRYP PFNGLRASTERPOS3SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLRASTERPOS4D) (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP PFNGLRASTERPOS4DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLRASTERPOS4F) (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRYP PFNGLRASTERPOS4FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLRASTERPOS4I) (GLint x, GLint y, GLint z, GLint w);
typedef void (APIENTRYP PFNGLRASTERPOS4IV) (const GLint *v);
typedef void (APIENTRYP PFNGLRASTERPOS4S) (GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRYP PFNGLRASTERPOS4SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLREADBUFFER) (GLenum mode);
typedef void (APIENTRYP PFNGLREADPIXELS) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
typedef void (APIENTRYP PFNGLRECTD) (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
typedef void (APIENTRYP PFNGLRECTDV) (const GLdouble *v1, const GLdouble *v2);
typedef void (APIENTRYP PFNGLRECTF) (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
typedef void (APIENTRYP PFNGLRECTFV) (const GLfloat *v1, const GLfloat *v2);
typedef void (APIENTRYP PFNGLRECTI) (GLint x1, GLint y1, GLint x2, GLint y2);
typedef void (APIENTRYP PFNGLRECTIV) (const GLint *v1, const GLint *v2);
typedef void (APIENTRYP PFNGLRECTS) (GLshort x1, GLshort y1, GLshort x2, GLshort y2);
typedef void (APIENTRYP PFNGLRECTSV) (const GLshort *v1, const GLshort *v2);
typedef GLint (APIENTRYP PFNGLRENDERMODE) (GLenum mode);
typedef void (APIENTRYP PFNGLROTATED) (GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP PFNGLROTATEF) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP PFNGLSCALED) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP PFNGLSCALEF) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP PFNGLSCISSOR) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLSELECTBUFFER) (GLsizei size, GLuint *buffer);
typedef void (APIENTRYP PFNGLSHADEMODEL) (GLenum mode);
typedef void (APIENTRYP PFNGLSTENCILFUNC) (GLenum func, GLint ref, GLuint mask);
typedef void (APIENTRYP PFNGLSTENCILMASK) (GLuint mask);
typedef void (APIENTRYP PFNGLSTENCILOP) (GLenum fail, GLenum zfail, GLenum zpass);
typedef void (APIENTRYP PFNGLTEXCOORD1D) (GLdouble s);
typedef void (APIENTRYP PFNGLTEXCOORD1DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLTEXCOORD1F) (GLfloat s);
typedef void (APIENTRYP PFNGLTEXCOORD1FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLTEXCOORD1I) (GLint s);
typedef void (APIENTRYP PFNGLTEXCOORD1IV) (const GLint *v);
typedef void (APIENTRYP PFNGLTEXCOORD1S) (GLshort s);
typedef void (APIENTRYP PFNGLTEXCOORD1SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLTEXCOORD2D) (GLdouble s, GLdouble t);
typedef void (APIENTRYP PFNGLTEXCOORD2DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLTEXCOORD2F) (GLfloat s, GLfloat t);
typedef void (APIENTRYP PFNGLTEXCOORD2FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLTEXCOORD2I) (GLint s, GLint t);
typedef void (APIENTRYP PFNGLTEXCOORD2IV) (const GLint *v);
typedef void (APIENTRYP PFNGLTEXCOORD2S) (GLshort s, GLshort t);
typedef void (APIENTRYP PFNGLTEXCOORD2SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLTEXCOORD3D) (GLdouble s, GLdouble t, GLdouble r);
typedef void (APIENTRYP PFNGLTEXCOORD3DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLTEXCOORD3F) (GLfloat s, GLfloat t, GLfloat r);
typedef void (APIENTRYP PFNGLTEXCOORD3FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLTEXCOORD3I) (GLint s, GLint t, GLint r);
typedef void (APIENTRYP PFNGLTEXCOORD3IV) (const GLint *v);
typedef void (APIENTRYP PFNGLTEXCOORD3S) (GLshort s, GLshort t, GLshort r);
typedef void (APIENTRYP PFNGLTEXCOORD3SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLTEXCOORD4D) (GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (APIENTRYP PFNGLTEXCOORD4DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLTEXCOORD4F) (GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (APIENTRYP PFNGLTEXCOORD4FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLTEXCOORD4I) (GLint s, GLint t, GLint r, GLint q);
typedef void (APIENTRYP PFNGLTEXCOORD4IV) (const GLint *v);
typedef void (APIENTRYP PFNGLTEXCOORD4S) (GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (APIENTRYP PFNGLTEXCOORD4SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLTEXCOORDPOINTER) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP PFNGLTEXENVF) (GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLTEXENVFV) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP PFNGLTEXENVI) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLTEXENVIV) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLTEXGEND) (GLenum coord, GLenum pname, GLdouble param);
typedef void (APIENTRYP PFNGLTEXGENDV) (GLenum coord, GLenum pname, const GLdouble *params);
typedef void (APIENTRYP PFNGLTEXGENF) (GLenum coord, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLTEXGENFV) (GLenum coord, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP PFNGLTEXGENI) (GLenum coord, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLTEXGENIV) (GLenum coord, GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLTEXIMAGE1D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLTEXIMAGE2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLTEXPARAMETERF) (GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLTEXPARAMETERFV) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP PFNGLTEXPARAMETERI) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLTEXPARAMETERIV) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLTEXSUBIMAGE1D) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLTEXSUBIMAGE2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLTRANSLATED) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP PFNGLTRANSLATEF) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP PFNGLVERTEX2D) (GLdouble x, GLdouble y);
typedef void (APIENTRYP PFNGLVERTEX2DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLVERTEX2F) (GLfloat x, GLfloat y);
typedef void (APIENTRYP PFNGLVERTEX2FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLVERTEX2I) (GLint x, GLint y);
typedef void (APIENTRYP PFNGLVERTEX2IV) (const GLint *v);
typedef void (APIENTRYP PFNGLVERTEX2S) (GLshort x, GLshort y);
typedef void (APIENTRYP PFNGLVERTEX2SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLVERTEX3D) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP PFNGLVERTEX3DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLVERTEX3F) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP PFNGLVERTEX3FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLVERTEX3I) (GLint x, GLint y, GLint z);
typedef void (APIENTRYP PFNGLVERTEX3IV) (const GLint *v);
typedef void (APIENTRYP PFNGLVERTEX3S) (GLshort x, GLshort y, GLshort z);
typedef void (APIENTRYP PFNGLVERTEX3SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLVERTEX4D) (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP PFNGLVERTEX4DV) (const GLdouble *v);
typedef void (APIENTRYP PFNGLVERTEX4F) (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRYP PFNGLVERTEX4FV) (const GLfloat *v);
typedef void (APIENTRYP PFNGLVERTEX4I) (GLint x, GLint y, GLint z, GLint w);
typedef void (APIENTRYP PFNGLVERTEX4IV) (const GLint *v);
typedef void (APIENTRYP PFNGLVERTEX4S) (GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRYP PFNGLVERTEX4SV) (const GLshort *v);
typedef void (APIENTRYP PFNGLVERTEXPOINTER) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP PFNGLVIEWPORT) (GLint x, GLint y, GLsizei width, GLsizei height);

#ifdef _WIN32
typedef BOOL  (WINAPI * PFNWGLCOPYCONTEXT) (HGLRC, HGLRC, UINT);
typedef HGLRC (WINAPI * PFNWGLCREATECONTEXT) (HDC);
typedef HGLRC (WINAPI * PFNWGLCREATELAYERCONTEXT) (HDC, int);
typedef BOOL  (WINAPI * PFNWGLDELETECONTEXT) (HGLRC);
typedef HGLRC (WINAPI * PFNWGLGETCURRENTCONTEXT) (VOID);
typedef HDC   (WINAPI * PFNWGLGETCURRENTDC) (VOID);
typedef PROC  (WINAPI * PFNWGLGETPROCADDRESS) (LPCSTR);
typedef BOOL  (WINAPI * PFNWGLMAKECURRENT) (HDC, HGLRC);
typedef BOOL  (WINAPI * PFNWGLSHARELISTS) (HGLRC, HGLRC);
#endif

typedef struct
{
// Platform Specific.
#if _WIN32
    // A handle to the OpenGL32 DLL.
    HANDLE hOpenGL32;

    // The dummy window for creating the main rendering context.
    HWND hDummyHWND;

    // The dummy DC for creating the main rendering context.
    HDC hDummyDC;

    // The OpenGL rendering context.
    HGLRC hRC;

    // The pixel format for use with SetPixelFormat()
    int pixelFormat;

    // The pixel format descriptor for use with SetPixelFormat()
    PIXELFORMATDESCRIPTOR pfd;

    // Win32-specific APIs for context management.
    PFNWGLCREATECONTEXT CreateContext;
    PFNWGLDELETECONTEXT DeleteContext;
    PFNWGLGETCURRENTCONTEXT GetCurrentContext;
    PFNWGLGETCURRENTDC GetCurrentDC;
    PFNWGLGETPROCADDRESS GetProcAddress;
    PFNWGLMAKECURRENT MakeCurrent;
#endif

    // OpenGL 1.1
#if DR_GL_VERSION >= 110
    PFNGLACCUM Accum;
    PFNGLALPHAFUNC AlphaFunc;
    PFNGLARETEXTURESRESIDENT AreTexturesResident;
    PFNGLARRAYELEMENT ArrayElement;
    PFNGLBEGIN Begin;
    PFNGLBINDTEXTURE BindTexture;
    PFNGLBITMAP Bitmap;
    PFNGLBLENDFUNC BlendFunc;
    PFNGLCALLLIST CallList;
    PFNGLCALLLISTS CallLists;
    PFNGLCLEAR Clear;
    PFNGLCLEARACCUM ClearAccum;
    PFNGLCLEARCOLOR ClearColor;
    PFNGLCLEARDEPTH ClearDepth;
    PFNGLCLEARINDEX ClearIndex;
    PFNGLCLEARSTENCIL ClearStencil;
    PFNGLCLIPPLANE ClipPlane;
    PFNGLCOLOR3B Color3b;
    PFNGLCOLOR3BV Color3bv;
    PFNGLCOLOR3D Color3d;
    PFNGLCOLOR3DV Color3dv;
    PFNGLCOLOR3F Color3f;
    PFNGLCOLOR3FV Color3fv;
    PFNGLCOLOR3I Color3i;
    PFNGLCOLOR3IV Color3iv;
    PFNGLCOLOR3S Color3s;
    PFNGLCOLOR3SV Color3sv;
    PFNGLCOLOR3UB Color3ub;
    PFNGLCOLOR3UBV Color3ubv;
    PFNGLCOLOR3UI Color3ui;
    PFNGLCOLOR3UIV Color3uiv;
    PFNGLCOLOR3US Color3us;
    PFNGLCOLOR3USV Color3usv;
    PFNGLCOLOR4B Color4b;
    PFNGLCOLOR4BV Color4bv;
    PFNGLCOLOR4D Color4d;
    PFNGLCOLOR4DV Color4dv;
    PFNGLCOLOR4F Color4f;
    PFNGLCOLOR4FV Color4fv;
    PFNGLCOLOR4I Color4i;
    PFNGLCOLOR4IV Color4iv;
    PFNGLCOLOR4S Color4s;
    PFNGLCOLOR4SV Color4sv;
    PFNGLCOLOR4UB Color4ub;
    PFNGLCOLOR4UBV Color4ubv;
    PFNGLCOLOR4UI Color4ui;
    PFNGLCOLOR4UIV Color4uiv;
    PFNGLCOLOR4US Color4us;
    PFNGLCOLOR4USV Color4usv;
    PFNGLCOLORMASK ColorMask;
    PFNGLCOLORMATERIAL ColorMaterial;
    PFNGLCOLORPOINTER ColorPointer;
    PFNGLCOPYPIXELS CopyPixels;
    PFNGLCOPYTEXIMAGE1D CopyTexImage1D;
    PFNGLCOPYTEXIMAGE2D CopyTexImage2D;
    PFNGLCOPYTEXSUBIMAGE1D CopyTexSubImage1D;
    PFNGLCOPYTEXSUBIMAGE2D CopyTexSubImage2D;
    PFNGLCULLFACE CullFace;
    PFNGLDELETELISTS DeleteLists;
    PFNGLDELETETEXTURES DeleteTextures;
    PFNGLDEPTHFUNC DepthFunc;
    PFNGLDEPTHMASK DepthMask;
    PFNGLDEPTHRANGE DepthRange;
    PFNGLDISABLE Disable;
    PFNGLDISABLECLIENTSTATE DisableClientState;
    PFNGLDRAWARRAYS DrawArrays;
    PFNGLDRAWBUFFER DrawBuffer;
    PFNGLDRAWELEMENTS DrawElements;
    PFNGLDRAWPIXELS DrawPixels;
    PFNGLEDGEFLAG EdgeFlag;
    PFNGLEDGEFLAGPOINTER EdgeFlagPointer;
    PFNGLEDGEFLAGV EdgeFlagv;
    PFNGLENABLE Enable;
    PFNGLENABLECLIENTSTATE EnableClientState;
    PFNGLEND End;
    PFNGLENDLIST EndList;
    PFNGLEVALCOORD1D EvalCoord1d;
    PFNGLEVALCOORD1DV EvalCoord1dv;
    PFNGLEVALCOORD1F EvalCoord1f;
    PFNGLEVALCOORD1FV EvalCoord1fv;
    PFNGLEVALCOORD2D EvalCoord2d;
    PFNGLEVALCOORD2DV EvalCoord2dv;
    PFNGLEVALCOORD2F EvalCoord2f;
    PFNGLEVALCOORD2FV EvalCoord2fv;
    PFNGLEVALMESH1 EvalMesh1;
    PFNGLEVALMESH2 EvalMesh2;
    PFNGLEVALPOINT1 EvalPoint1;
    PFNGLEVALPOINT2 EvalPoint2;
    PFNGLFEEDBACKBUFFER FeedbackBuffer;
    PFNGLFINISH Finish;
    PFNGLFLUSH Flush;
    PFNGLFOGF Fogf;
    PFNGLFOGFV Fogfv;
    PFNGLFOGI Fogi;
    PFNGLFOGIV Fogiv;
    PFNGLFRONTFACE FrontFace;
    PFNGLFRUSTUM Frustum;
    PFNGLGENLISTS GenLists;
    PFNGLGENTEXTURES GenTextures;
    PFNGLGETBOOLEANV GetBooleanv;
    PFNGLGETCLIPPLANE GetClipPlane;
    PFNGLGETDOUBLEV GetDoublev;
    PFNGLGETERROR GetError;
    PFNGLGETFLOATV GetFloatv;
    PFNGLGETINTEGERV GetIntegerv;
    PFNGLGETLIGHTFV GetLightfv;
    PFNGLGETLIGHTIV GetLightiv;
    PFNGLGETMAPDV GetMapdv;
    PFNGLGETMAPFV GetMapfv;
    PFNGLGETMAPIV GetMapiv;
    PFNGLGETMATERIALFV GetMaterialfv;
    PFNGLGETMATERIALIV GetMaterialiv;
    PFNGLGETPIXELMAPFV GetPixelMapfv;
    PFNGLGETPIXELMAPUIV GetPixelMapuiv;
    PFNGLGETPIXELMAPUSV GetPixelMapusv;
    PFNGLGETPOINTERV GetPointerv;
    PFNGLGETPOLYGONSTIPPLE GetPolygonStipple;
    PFNGLGETSTRING GetString;
    PFNGLGETTEXENVFV GetTexEnvfv;
    PFNGLGETTEXENVIV GetTexEnviv;
    PFNGLGETTEXGENDV GetTexGendv;
    PFNGLGETTEXGENFV GetTexGenfv;
    PFNGLGETTEXGENIV GetTexGeniv;
    PFNGLGETTEXIMAGE GetTexImage;
    PFNGLGETTEXLEVELPARAMETERFV GetTexLevelParameterfv;
    PFNGLGETTEXLEVELPARAMETERIV GetTexLevelParameteriv;
    PFNGLGETTEXPARAMETERFV GetTexParameterfv;
    PFNGLGETTEXPARAMETERIV GetTexParameteriv;
    PFNGLHINT Hint;
    PFNGLINDEXMASK IndexMask;
    PFNGLINDEXPOINTER IndexPointer;
    PFNGLINDEXD Indexd;
    PFNGLINDEXDV Indexdv;
    PFNGLINDEXF Indexf;
    PFNGLINDEXFV Indexfv;
    PFNGLINDEXI Indexi;
    PFNGLINDEXIV Indexiv;
    PFNGLINDEXS Indexs;
    PFNGLINDEXSV Indexsv;
    PFNGLINDEXUB Indexub;
    PFNGLINDEXUBV Indexubv;
    PFNGLINITNAMES InitNames;
    PFNGLINTERLEAVEDARRAYS InterleavedArrays;
    PFNGLISENABLED IsEnabled;
    PFNGLISLIST IsList;
    PFNGLISTEXTURE IsTexture;
    PFNGLLIGHTMODELF LightModelf;
    PFNGLLIGHTMODELFV LightModelfv;
    PFNGLLIGHTMODELI LightModeli;
    PFNGLLIGHTMODELIV LightModeliv;
    PFNGLLIGHTF Lightf;
    PFNGLLIGHTFV Lightfv;
    PFNGLLIGHTI Lighti;
    PFNGLLIGHTIV Lightiv;
    PFNGLLINESTIPPLE LineStipple;
    PFNGLLINEWIDTH LineWidth;
    PFNGLLISTBASE ListBase;
    PFNGLLOADIDENTITY LoadIdentity;
    PFNGLLOADMATRIXD LoadMatrixd;
    PFNGLLOADMATRIXF LoadMatrixf;
    PFNGLLOADNAME LoadName;
    PFNGLLOGICOP LogicOp;
    PFNGLMAP1D Map1d;
    PFNGLMAP1F Map1f;
    PFNGLMAP2D Map2d;
    PFNGLMAP2F Map2f;
    PFNGLMAPGRID1D MapGrid1d;
    PFNGLMAPGRID1F MapGrid1f;
    PFNGLMAPGRID2D MapGrid2d;
    PFNGLMAPGRID2F MapGrid2f;
    PFNGLMATERIALF Materialf;
    PFNGLMATERIALFV Materialfv;
    PFNGLMATERIALI Materiali;
    PFNGLMATERIALIV Materialiv;
    PFNGLMATRIXMODE MatrixMode;
    PFNGLMULTMATRIXD MultMatrixd;
    PFNGLMULTMATRIXF MultMatrixf;
    PFNGLNEWLIST NewList;
    PFNGLNORMAL3B Normal3b;
    PFNGLNORMAL3BV Normal3bv;
    PFNGLNORMAL3D Normal3d;
    PFNGLNORMAL3DV Normal3dv;
    PFNGLNORMAL3F Normal3f;
    PFNGLNORMAL3FV Normal3fv;
    PFNGLNORMAL3I Normal3i;
    PFNGLNORMAL3IV Normal3iv;
    PFNGLNORMAL3S Normal3s;
    PFNGLNORMAL3SV Normal3sv;
    PFNGLNORMALPOINTER NormalPointer;
    PFNGLORTHO Ortho;
    PFNGLPATHTHROUGH PassThrough;
    PFNGLPIXELMAPFV PixelMapfv;
    PFNGLPIXELMAPUIV PixelMapuiv;
    PFNGLPIXELMAPUSV PixelMapusv;
    PFNGLPIXELSTOREF PixelStoref;
    PFNGLPIXELSTOREI PixelStorei;
    PFNGLPIXELTRANSFERF PixelTransferf;
    PFNGLPIXELTRANSGERI PixelTransferi;
    PFNGLPIXELZOOM PixelZoom;
    PFNGLPOINTSIZE PointSize;
    PFNGLPOLYGONMODE PolygonMode;
    PFNGLPOLYGONOFFSET PolygonOffset;
    PFNGLPOLYGONSTIPPLE PolygonStipple;
    PFNGLPOPATTRIB PopAttrib;
    PFNGLPOPCLIENTATTRIB PopClientAttrib;
    PFNGLPOPMATRIX PopMatrix;
    PFNGLPOPNAME PopName;
    PFNGLPRIORITIZETEXTURES PrioritizeTextures;
    PFNGLPUSHATTRIB PushAttrib;
    PFNGLPUSHCLIENTATTRIB PushClientAttrib;
    PFNGLPUSHMATRIX PushMatrix;
    PFNGLPUSHNAME PushName;
    PFNGLRASTERPOS2D RasterPos2d;
    PFNGLRASTERPOS2DV RasterPos2dv;
    PFNGLRASTERPOS2F RasterPos2f;
    PFNGLRASTERPOS2FV RasterPos2fv;
    PFNGLRASTERPOS2I RasterPos2i;
    PFNGLRASTERPOS2IV RasterPos2iv;
    PFNGLRASTERPOS2S RasterPos2s;
    PFNGLRASTERPOS2SV RasterPos2sv;
    PFNGLRASTERPOS3D RasterPos3d;
    PFNGLRASTERPOS3DV RasterPos3dv;
    PFNGLRASTERPOS3F RasterPos3f;
    PFNGLRASTERPOS3FV RasterPos3fv;
    PFNGLRASTERPOS3I RasterPos3i;
    PFNGLRASTERPOS3IV RasterPos3iv;
    PFNGLRASTERPOS3S RasterPos3s;
    PFNGLRASTERPOS3SV RasterPos3sv;
    PFNGLRASTERPOS4D RasterPos4d;
    PFNGLRASTERPOS4DV RasterPos4dv;
    PFNGLRASTERPOS4F RasterPos4f;
    PFNGLRASTERPOS4FV RasterPos4fv;
    PFNGLRASTERPOS4I RasterPos4i;
    PFNGLRASTERPOS4IV RasterPos4iv;
    PFNGLRASTERPOS4S RasterPos4s;
    PFNGLRASTERPOS4SV RasterPos4sv;
    PFNGLREADBUFFER ReadBuffer;
    PFNGLREADPIXELS ReadPixels;
    PFNGLRECTD Rectd;
    PFNGLRECTDV Rectdv;
    PFNGLRECTF Rectf;
    PFNGLRECTFV Rectfv;
    PFNGLRECTI Recti;
    PFNGLRECTIV Rectiv;
    PFNGLRECTS Rects;
    PFNGLRECTSV Rectsv;
    PFNGLRENDERMODE RenderMode;
    PFNGLROTATED Rotated;
    PFNGLROTATEF Rotatef;
    PFNGLSCALED Scaled;
    PFNGLSCALEF Scalef;
    PFNGLSCISSOR Scissor;
    PFNGLSELECTBUFFER SelectBuffer;
    PFNGLSHADEMODEL ShadeModel;
    PFNGLSTENCILFUNC StencilFunc;
    PFNGLSTENCILMASK StencilMask;
    PFNGLSTENCILOP StencilOp;
    PFNGLTEXCOORD1D TexCoord1d;
    PFNGLTEXCOORD1DV TexCoord1dv;
    PFNGLTEXCOORD1F TexCoord1f;
    PFNGLTEXCOORD1FV TexCoord1fv;
    PFNGLTEXCOORD1I TexCoord1i;
    PFNGLTEXCOORD1IV TexCoord1iv;
    PFNGLTEXCOORD1S TexCoord1s;
    PFNGLTEXCOORD1SV TexCoord1sv;
    PFNGLTEXCOORD2D TexCoord2d;
    PFNGLTEXCOORD2DV TexCoord2dv;
    PFNGLTEXCOORD2F TexCoord2f;
    PFNGLTEXCOORD2FV TexCoord2fv;
    PFNGLTEXCOORD2I TexCoord2i;
    PFNGLTEXCOORD2IV TexCoord2iv;
    PFNGLTEXCOORD2S TexCoord2s;
    PFNGLTEXCOORD2SV TexCoord2sv;
    PFNGLTEXCOORD3D TexCoord3d;
    PFNGLTEXCOORD3DV TexCoord3dv;
    PFNGLTEXCOORD3F TexCoord3f;
    PFNGLTEXCOORD3FV TexCoord3fv;
    PFNGLTEXCOORD3I TexCoord3i;
    PFNGLTEXCOORD3IV TexCoord3iv;
    PFNGLTEXCOORD3S TexCoord3s;
    PFNGLTEXCOORD3SV TexCoord3sv;
    PFNGLTEXCOORD4D TexCoord4d;
    PFNGLTEXCOORD4DV TexCoord4dv;
    PFNGLTEXCOORD4F TexCoord4f;
    PFNGLTEXCOORD4FV TexCoord4fv;
    PFNGLTEXCOORD4I TexCoord4i;
    PFNGLTEXCOORD4IV TexCoord4iv;
    PFNGLTEXCOORD4S TexCoord4s;
    PFNGLTEXCOORD4SV TexCoord4sv;
    PFNGLTEXCOORDPOINTER TexCoordPointer;
    PFNGLTEXENVF TexEnvf;
    PFNGLTEXENVFV TexEnvfv;
    PFNGLTEXENVI TexEnvi;
    PFNGLTEXENVIV TexEnviv;
    PFNGLTEXGEND TexGend;
    PFNGLTEXGENDV TexGendv;
    PFNGLTEXGENF TexGenf;
    PFNGLTEXGENFV TexGenfv;
    PFNGLTEXGENI TexGeni;
    PFNGLTEXGENIV TexGeniv;
    PFNGLTEXIMAGE1D TexImage1D;
    PFNGLTEXIMAGE2D TexImage2D;
    PFNGLTEXPARAMETERF TexParameterf;
    PFNGLTEXPARAMETERFV TexParameterfv;
    PFNGLTEXPARAMETERI TexParameteri;
    PFNGLTEXPARAMETERIV TexParameteriv;
    PFNGLTEXSUBIMAGE1D TexSubImage1D;
    PFNGLTEXSUBIMAGE2D TexSubImage2D;
    PFNGLTRANSLATED Translated;
    PFNGLTRANSLATEF Translatef;
    PFNGLVERTEX2D Vertex2d;
    PFNGLVERTEX2DV Vertex2dv;
    PFNGLVERTEX2F Vertex2f;
    PFNGLVERTEX2FV Vertex2fv;
    PFNGLVERTEX2I Vertex2i;
    PFNGLVERTEX2IV Vertex2iv;
    PFNGLVERTEX2S Vertex2s;
    PFNGLVERTEX2SV Vertex2sv;
    PFNGLVERTEX3D Vertex3d;
    PFNGLVERTEX3DV Vertex3dv;
    PFNGLVERTEX3F Vertex3f;
    PFNGLVERTEX3FV Vertex3fv;
    PFNGLVERTEX3I Vertex3i;
    PFNGLVERTEX3IV Vertex3iv;
    PFNGLVERTEX3S Vertex3s;
    PFNGLVERTEX3SV Vertex3sv;
    PFNGLVERTEX4D Vertex4d;
    PFNGLVERTEX4DV Vertex4dv;
    PFNGLVERTEX4F Vertex4f;
    PFNGLVERTEX4FV Vertex4fv;
    PFNGLVERTEX4I Vertex4i;
    PFNGLVERTEX4IV Vertex4iv;
    PFNGLVERTEX4S Vertex4s;
    PFNGLVERTEX4SV Vertex4sv;
    PFNGLVERTEXPOINTER VertexPointer;
    PFNGLVIEWPORT Viewport;
#endif

    // OpenGL 1.2
#if DR_GL_VERSION >= 120
    PFNGLDRAWRANGEELEMENTSPROC DrawRangeElements;
    PFNGLTEXIMAGE3DPROC TexImage3D;
    PFNGLTEXSUBIMAGE3DPROC TexSubImage3D;
    PFNGLCOPYTEXSUBIMAGE3DPROC CopyTexSubImage3D;
#endif

    // OpenGL 1.3
#if DR_GL_VERSION >= 130
    PFNGLACTIVETEXTUREPROC ActiveTexture;
    PFNGLSAMPLECOVERAGEPROC SampleCoverage;
    PFNGLCOMPRESSEDTEXIMAGE3DPROC CompressedTexImage3D;
    PFNGLCOMPRESSEDTEXIMAGE2DPROC CompressedTexImage2D;
    PFNGLCOMPRESSEDTEXIMAGE1DPROC CompressedTexImage1D;
    PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC CompressedTexSubImage3D;
    PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC CompressedTexSubImage2D;
    PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC CompressedTexSubImage1D;
    PFNGLGETCOMPRESSEDTEXIMAGEPROC GetCompressedTexImage;
    PFNGLCLIENTACTIVETEXTUREPROC ClientActiveTexture;
    PFNGLMULTITEXCOORD1DPROC MultiTexCoord1d;
    PFNGLMULTITEXCOORD1DVPROC MultiTexCoord1dv;
    PFNGLMULTITEXCOORD1FPROC MultiTexCoord1f;
    PFNGLMULTITEXCOORD1FVPROC MultiTexCoord1fv;
    PFNGLMULTITEXCOORD1IPROC MultiTexCoord1i;
    PFNGLMULTITEXCOORD1IVPROC MultiTexCoord1iv;
    PFNGLMULTITEXCOORD1SPROC MultiTexCoord1s;
    PFNGLMULTITEXCOORD1SVPROC MultiTexCoord1sv;
    PFNGLMULTITEXCOORD2DPROC MultiTexCoord2d;
    PFNGLMULTITEXCOORD2DVPROC MultiTexCoord2dv;
    PFNGLMULTITEXCOORD2FPROC MultiTexCoord2f;
    PFNGLMULTITEXCOORD2FVPROC MultiTexCoord2fv;
    PFNGLMULTITEXCOORD2IPROC MultiTexCoord2i;
    PFNGLMULTITEXCOORD2IVPROC MultiTexCoord2iv;
    PFNGLMULTITEXCOORD2SPROC MultiTexCoord2s;
    PFNGLMULTITEXCOORD2SVPROC MultiTexCoord2sv;
    PFNGLMULTITEXCOORD3DPROC MultiTexCoord3d;
    PFNGLMULTITEXCOORD3DVPROC MultiTexCoord3dv;
    PFNGLMULTITEXCOORD3FPROC MultiTexCoord3f;
    PFNGLMULTITEXCOORD3FVPROC MultiTexCoord3fv;
    PFNGLMULTITEXCOORD3IPROC MultiTexCoord3i;
    PFNGLMULTITEXCOORD3IVPROC MultiTexCoord3iv;
    PFNGLMULTITEXCOORD3SPROC MultiTexCoord3s;
    PFNGLMULTITEXCOORD3SVPROC MultiTexCoord3sv;
    PFNGLMULTITEXCOORD4DPROC MultiTexCoord4d;
    PFNGLMULTITEXCOORD4DVPROC MultiTexCoord4dv;
    PFNGLMULTITEXCOORD4FPROC MultiTexCoord4f;
    PFNGLMULTITEXCOORD4FVPROC MultiTexCoord4fv;
    PFNGLMULTITEXCOORD4IPROC MultiTexCoord4i;
    PFNGLMULTITEXCOORD4IVPROC MultiTexCoord4iv;
    PFNGLMULTITEXCOORD4SPROC MultiTexCoord4s;
    PFNGLMULTITEXCOORD4SVPROC MultiTexCoord4sv;
    PFNGLLOADTRANSPOSEMATRIXFPROC LoadTransposeMatrixf;
    PFNGLLOADTRANSPOSEMATRIXDPROC LoadTransposeMatrixd;
    PFNGLMULTTRANSPOSEMATRIXFPROC MultTransposeMatrixf;
    PFNGLMULTTRANSPOSEMATRIXDPROC MultTransposeMatrixd;
#endif

    // OpenGL 1.4
#if DR_GL_VERSION >= 140
    PFNGLBLENDFUNCSEPARATEPROC BlendFuncSeparate;
    PFNGLMULTIDRAWARRAYSPROC MultiDrawArrays;
    PFNGLMULTIDRAWELEMENTSPROC MultiDrawElements;
    PFNGLPOINTPARAMETERFPROC PointParameterf;
    PFNGLPOINTPARAMETERFVPROC PointParameterfv;
    PFNGLPOINTPARAMETERIPROC PointParameteri;
    PFNGLPOINTPARAMETERIVPROC PointParameteriv;
    PFNGLFOGCOORDFPROC FogCoordf;
    PFNGLFOGCOORDFVPROC FogCoordfv;
    PFNGLFOGCOORDDPROC FogCoordd;
    PFNGLFOGCOORDDVPROC FogCoorddv;
    PFNGLFOGCOORDPOINTERPROC FogCoordPointer;
    PFNGLSECONDARYCOLOR3BPROC SecondaryColor3b;
    PFNGLSECONDARYCOLOR3BVPROC SecondaryColor3bv;
    PFNGLSECONDARYCOLOR3DPROC SecondaryColor3d;
    PFNGLSECONDARYCOLOR3DVPROC SecondaryColor3dv;
    PFNGLSECONDARYCOLOR3FPROC SecondaryColor3f;
    PFNGLSECONDARYCOLOR3FVPROC SecondaryColor3fv;
    PFNGLSECONDARYCOLOR3IPROC SecondaryColor3i;
    PFNGLSECONDARYCOLOR3IVPROC SecondaryColor3iv;
    PFNGLSECONDARYCOLOR3SPROC SecondaryColor3s;
    PFNGLSECONDARYCOLOR3SVPROC SecondaryColor3sv;
    PFNGLSECONDARYCOLOR3UBPROC SecondaryColor3ub;
    PFNGLSECONDARYCOLOR3UBVPROC SecondaryColor3ubv;
    PFNGLSECONDARYCOLOR3UIPROC SecondaryColor3ui;
    PFNGLSECONDARYCOLOR3UIVPROC SecondaryColor3uiv;
    PFNGLSECONDARYCOLOR3USPROC SecondaryColor3us;
    PFNGLSECONDARYCOLOR3USVPROC SecondaryColor3usv;
    PFNGLSECONDARYCOLORPOINTERPROC SecondaryColorPointer;
    PFNGLWINDOWPOS2DPROC WindowPos2d;
    PFNGLWINDOWPOS2DVPROC WindowPos2dv;
    PFNGLWINDOWPOS2FPROC WindowPos2f;
    PFNGLWINDOWPOS2FVPROC WindowPos2fv;
    PFNGLWINDOWPOS2IPROC WindowPos2i;
    PFNGLWINDOWPOS2IVPROC WindowPos2iv;
    PFNGLWINDOWPOS2SPROC WindowPos2s;
    PFNGLWINDOWPOS2SVPROC WindowPos2sv;
    PFNGLWINDOWPOS3DPROC WindowPos3d;
    PFNGLWINDOWPOS3DVPROC WindowPos3dv;
    PFNGLWINDOWPOS3FPROC WindowPos3f;
    PFNGLWINDOWPOS3FVPROC WindowPos3fv;
    PFNGLWINDOWPOS3IPROC WindowPos3i;
    PFNGLWINDOWPOS3IVPROC WindowPos3iv;
    PFNGLWINDOWPOS3SPROC WindowPos3s;
    PFNGLWINDOWPOS3SVPROC WindowPos3sv;
    PFNGLBLENDCOLORPROC BlendColor;
    PFNGLBLENDEQUATIONPROC BlendEquation;
#endif

    // OpenGL 1.5
#if DR_GL_VERSION >= 150
    PFNGLGENQUERIESPROC GenQueries;
    PFNGLDELETEQUERIESPROC DeleteQueries;
    PFNGLISQUERYPROC IsQuery;
    PFNGLBEGINQUERYPROC BeginQuery;
    PFNGLENDQUERYPROC EndQuery;
    PFNGLGETQUERYIVPROC GetQueryiv;
    PFNGLGETQUERYOBJECTIVPROC GetQueryObjectiv;
    PFNGLGETQUERYOBJECTUIVPROC GetQueryObjectuiv;
    PFNGLBINDBUFFERPROC BindBuffer;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;
    PFNGLGENBUFFERSPROC GenBuffers;
    PFNGLISBUFFERPROC IsBuffer;
    PFNGLBUFFERDATAPROC BufferData;
    PFNGLBUFFERSUBDATAPROC BufferSubData;
    PFNGLGETBUFFERSUBDATAPROC GetBufferSubData;
    PFNGLMAPBUFFERPROC MapBuffer;
    PFNGLUNMAPBUFFERPROC UnmapBuffer;
    PFNGLGETBUFFERPARAMETERIVPROC GetBufferParameteriv;
    PFNGLGETBUFFERPOINTERVPROC GetBufferPointerv;
#endif

    // OpenGL 2.0
#if DR_GL_VERSION >= 200
    PFNGLBLENDEQUATIONSEPARATEPROC BlendEquationSeparate;
    PFNGLDRAWBUFFERSPROC DrawBuffers;
    PFNGLSTENCILOPSEPARATEPROC StencilOpSeparate;
    PFNGLSTENCILFUNCSEPARATEPROC StencilFuncSeparate;
    PFNGLSTENCILMASKSEPARATEPROC StencilMaskSeparate;
    PFNGLATTACHSHADERPROC AttachShader;
    PFNGLBINDATTRIBLOCATIONPROC BindAttribLocation;
    PFNGLCOMPILESHADERPROC CompileShader;
    PFNGLCREATEPROGRAMPROC CreateProgram;
    PFNGLCREATESHADERPROC CreateShader;
    PFNGLDELETEPROGRAMPROC DeleteProgram;
    PFNGLDELETESHADERPROC DeleteShader;
    PFNGLDETACHSHADERPROC DetachShader;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
    PFNGLGETACTIVEATTRIBPROC GetActiveAttrib;
    PFNGLGETACTIVEUNIFORMPROC GetActiveUniform;
    PFNGLGETATTACHEDSHADERSPROC GetAttachedShaders;
    PFNGLGETATTRIBLOCATIONPROC GetAttribLocation;
    PFNGLGETPROGRAMIVPROC GetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
    PFNGLGETSHADERIVPROC GetShaderiv;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
    PFNGLGETSHADERSOURCEPROC GetShaderSource;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
    PFNGLGETUNIFORMFVPROC GetUniformfv;
    PFNGLGETUNIFORMIVPROC GetUniformiv;
    PFNGLGETVERTEXATTRIBDVPROC GetVertexAttribdv;
    PFNGLGETVERTEXATTRIBFVPROC GetVertexAttribfv;
    PFNGLGETVERTEXATTRIBIVPROC GetVertexAttribiv;
    PFNGLGETVERTEXATTRIBPOINTERVPROC GetVertexAttribPointerv;
    PFNGLISPROGRAMPROC IsProgram;
    PFNGLISSHADERPROC IsShader;
    PFNGLLINKPROGRAMPROC LinkProgram;
    PFNGLSHADERSOURCEPROC ShaderSource;
    PFNGLUSEPROGRAMPROC UseProgram;
    PFNGLUNIFORM1FPROC Uniform1f;
    PFNGLUNIFORM2FPROC Uniform2f;
    PFNGLUNIFORM3FPROC Uniform3f;
    PFNGLUNIFORM4FPROC Uniform4f;
    PFNGLUNIFORM1IPROC Uniform1i;
    PFNGLUNIFORM2IPROC Uniform2i;
    PFNGLUNIFORM3IPROC Uniform3i;
    PFNGLUNIFORM4IPROC Uniform4i;
    PFNGLUNIFORM1FVPROC Uniform1fv;
    PFNGLUNIFORM2FVPROC Uniform2fv;
    PFNGLUNIFORM3FVPROC Uniform3fv;
    PFNGLUNIFORM4FVPROC Uniform4fv;
    PFNGLUNIFORM1IVPROC Uniform1iv;
    PFNGLUNIFORM2IVPROC Uniform2iv;
    PFNGLUNIFORM3IVPROC Uniform3iv;
    PFNGLUNIFORM4IVPROC Uniform4iv;
    PFNGLUNIFORMMATRIX2FVPROC UniformMatrix2fv;
    PFNGLUNIFORMMATRIX3FVPROC UniformMatrix3fv;
    PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv;
    PFNGLVALIDATEPROGRAMPROC ValidateProgram;
    PFNGLVERTEXATTRIB1DPROC VertexAttrib1d;
    PFNGLVERTEXATTRIB1DVPROC VertexAttrib1dv;
    PFNGLVERTEXATTRIB1FPROC VertexAttrib1f;
    PFNGLVERTEXATTRIB1FVPROC VertexAttrib1fv;
    PFNGLVERTEXATTRIB1SPROC VertexAttrib1s;
    PFNGLVERTEXATTRIB1SVPROC VertexAttrib1sv;
    PFNGLVERTEXATTRIB2DPROC VertexAttrib2d;
    PFNGLVERTEXATTRIB2DVPROC VertexAttrib2dv;
    PFNGLVERTEXATTRIB2FPROC VertexAttrib2f;
    PFNGLVERTEXATTRIB2FVPROC VertexAttrib2fv;
    PFNGLVERTEXATTRIB2SPROC VertexAttrib2s;
    PFNGLVERTEXATTRIB2SVPROC VertexAttrib2sv;
    PFNGLVERTEXATTRIB3DPROC VertexAttrib3d;
    PFNGLVERTEXATTRIB3DVPROC VertexAttrib3dv;
    PFNGLVERTEXATTRIB3FPROC VertexAttrib3f;
    PFNGLVERTEXATTRIB3FVPROC VertexAttrib3fv;
    PFNGLVERTEXATTRIB3SPROC VertexAttrib3s;
    PFNGLVERTEXATTRIB3SVPROC VertexAttrib3sv;
    PFNGLVERTEXATTRIB4NBVPROC VertexAttrib4Nbv;
    PFNGLVERTEXATTRIB4NIVPROC VertexAttrib4Niv;
    PFNGLVERTEXATTRIB4NSVPROC VertexAttrib4Nsv;
    PFNGLVERTEXATTRIB4NUBPROC VertexAttrib4Nub;
    PFNGLVERTEXATTRIB4NUBVPROC VertexAttrib4Nubv;
    PFNGLVERTEXATTRIB4NUIVPROC VertexAttrib4Nuiv;
    PFNGLVERTEXATTRIB4NUSVPROC VertexAttrib4Nusv;
    PFNGLVERTEXATTRIB4BVPROC VertexAttrib4bv;
    PFNGLVERTEXATTRIB4DPROC VertexAttrib4d;
    PFNGLVERTEXATTRIB4DVPROC VertexAttrib4dv;
    PFNGLVERTEXATTRIB4FPROC VertexAttrib4f;
    PFNGLVERTEXATTRIB4FVPROC VertexAttrib4fv;
    PFNGLVERTEXATTRIB4IVPROC VertexAttrib4iv;
    PFNGLVERTEXATTRIB4SPROC VertexAttrib4s;
    PFNGLVERTEXATTRIB4SVPROC VertexAttrib4sv;
    PFNGLVERTEXATTRIB4UBVPROC VertexAttrib4ubv;
    PFNGLVERTEXATTRIB4UIVPROC VertexAttrib4uiv;
    PFNGLVERTEXATTRIB4USVPROC VertexAttrib4usv;
    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
#endif

    // OpenGL 2.1
#if DR_GL_VERSION >= 210
    PFNGLUNIFORMMATRIX2X3FVPROC UniformMatrix2x3fv;
    PFNGLUNIFORMMATRIX3X2FVPROC UniformMatrix3x2fv;
    PFNGLUNIFORMMATRIX2X4FVPROC UniformMatrix2x4fv;
    PFNGLUNIFORMMATRIX4X2FVPROC UniformMatrix4x2fv;
    PFNGLUNIFORMMATRIX3X4FVPROC UniformMatrix3x4fv;
    PFNGLUNIFORMMATRIX4X3FVPROC UniformMatrix4x3fv;
#endif



    // Extensions
#ifdef DR_GL_ENABLE_EXT_framebuffer_blit
    PFNGLBLITFRAMEBUFFEREXTPROC BlitFramebufferEXT;
#endif

#ifdef DR_GL_ENABLE_EXT_framebuffer_multisample
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC RenderbufferStorageMultisampleEXT;
#endif

#ifdef DR_GL_ENABLE_EXT_framebuffer_object
    PFNGLISRENDERBUFFEREXTPROC IsRenderbufferEXT;
    PFNGLBINDRENDERBUFFEREXTPROC BindRenderbufferEXT;
    PFNGLDELETERENDERBUFFERSEXTPROC DeleteRenderbuffersEXT;
    PFNGLGENRENDERBUFFERSEXTPROC GenRenderbuffersEXT;
    PFNGLRENDERBUFFERSTORAGEEXTPROC RenderbufferStorageEXT;
    PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC GetRenderbufferParameterivEXT;
    PFNGLISFRAMEBUFFEREXTPROC IsFramebufferEXT;
    PFNGLBINDFRAMEBUFFEREXTPROC BindFramebufferEXT;
    PFNGLDELETEFRAMEBUFFERSEXTPROC DeleteFramebuffersEXT;
    PFNGLGENFRAMEBUFFERSEXTPROC GenFramebuffersEXT;
    PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC CheckFramebufferStatusEXT;
    PFNGLFRAMEBUFFERTEXTURE1DEXTPROC FramebufferTexture1DEXT;
    PFNGLFRAMEBUFFERTEXTURE2DEXTPROC FramebufferTexture2DEXT;
    PFNGLFRAMEBUFFERTEXTURE3DEXTPROC FramebufferTexture3DEXT;
    PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC FramebufferRenderbufferEXT;
    PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC GetFramebufferAttachmentParameterivEXT;
    PFNGLGENERATEMIPMAPEXTPROC GenerateMipmapEXT;
#endif

#ifdef DR_GL_ENABLE_EXT_swap_control
    PFNWGLSWAPINTERVALEXTPROC SwapIntervalEXT;
    PFNWGLGETSWAPINTERVALEXTPROC GetSwapIntervalEXT;
#endif



} drgl;


// drgl_init()
bool drgl_init(drgl* pGL);

// drgl_uninit()
void drgl_uninit(drgl* pGL);



#ifdef __cplusplus
}
#endif
#endif  //dr_util_h


///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DR_GL_IMPLEMENTATION

#ifdef _WIN32
static LRESULT DummyWindowProcWin32(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void* drgl__get_proc_address(drgl* pGL, const char* name)
{
    assert(pGL != NULL);
    assert(pGL->GetProcAddress != NULL);

    void* func = pGL->GetProcAddress(name);
    if (func != NULL) {
        return func;
    }

    return GetProcAddress(pGL->hOpenGL32, name);
}
#else
void* drgl__get_gl_proc_address(const char* name)
{
    // TODO: Implement Me.
    return NULL;
}
#endif




bool drgl_init(drgl* pGL)
{
    if (pGL == NULL) {
        return false;
    }

    memset(pGL, 0, sizeof(*pGL));

#ifdef _WIN32
    pGL->hOpenGL32 = LoadLibraryW(L"OpenGL32.dll");
    if (pGL->hOpenGL32 == NULL) {
        goto on_error;
    }

    pGL->CreateContext     = (PFNWGLCREATECONTEXT    )GetProcAddress(pGL->hOpenGL32, "wglCreateContext");
    pGL->DeleteContext     = (PFNWGLDELETECONTEXT    )GetProcAddress(pGL->hOpenGL32, "wglDeleteContext");
    pGL->GetCurrentContext = (PFNWGLGETCURRENTCONTEXT)GetProcAddress(pGL->hOpenGL32, "wglGetCurrentContext");
    pGL->GetCurrentDC      = (PFNWGLGETCURRENTDC     )GetProcAddress(pGL->hOpenGL32, "wglGetCurrentDC");
    pGL->GetProcAddress    = (PFNWGLGETPROCADDRESS   )GetProcAddress(pGL->hOpenGL32, "wglGetProcAddress");
    pGL->MakeCurrent       = (PFNWGLMAKECURRENT      )GetProcAddress(pGL->hOpenGL32, "wglMakeCurrent");

    if (pGL->CreateContext     == NULL ||
        pGL->DeleteContext     == NULL ||
        pGL->GetCurrentContext == NULL ||
        pGL->GetCurrentDC      == NULL ||
        pGL->GetProcAddress    == NULL ||
        pGL->MakeCurrent       == NULL) {
        goto on_error;
    }


    WNDCLASSEXW dummyWC;
    memset(&dummyWC, 0, sizeof(dummyWC));
    dummyWC.cbSize        = sizeof(dummyWC);
    dummyWC.lpfnWndProc   = (WNDPROC)DummyWindowProcWin32;
    dummyWC.lpszClassName = L"DR_GL_DummyHWND";
    dummyWC.style         = CS_OWNDC;
    if (!RegisterClassExW(&dummyWC)) {
        goto on_error;
    }

    pGL->hDummyHWND = CreateWindowExW(0, L"DR_GL_DummyHWND", L"", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);
    pGL->hDummyDC   = GetDC(pGL->hDummyHWND);

    memset(&pGL->pfd, 0, sizeof(pGL->pfd));
    pGL->pfd.nSize        = sizeof(pGL->pfd);
    pGL->pfd.nVersion     = 1;
    pGL->pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pGL->pfd.iPixelType   = PFD_TYPE_RGBA;
    pGL->pfd.cStencilBits = 8;
    pGL->pfd.cDepthBits   = 24;
    pGL->pfd.cColorBits   = 32;
    pGL->pixelFormat = ChoosePixelFormat(pGL->hDummyDC, &pGL->pfd);
    if (pGL->pixelFormat == 0) {
        goto on_error;
    }

    if (!SetPixelFormat(pGL->hDummyDC, pGL->pixelFormat,  &pGL->pfd)) {
        goto on_error;
    }

    pGL->hRC = pGL->CreateContext(pGL->hDummyDC);
    if (pGL->hRC == NULL) {
        goto on_error;
    }

    pGL->MakeCurrent(pGL->hDummyDC, pGL->hRC);
#else
    // TODO: Linux.
#endif



    // Core APIs
#if DR_GL_VERSION >= 110
    pGL->Accum = (PFNGLACCUM)drgl__get_proc_address(pGL, "glAccum");
    pGL->AlphaFunc = (PFNGLALPHAFUNC)drgl__get_proc_address(pGL, "glAlphaFunc");
    pGL->AreTexturesResident = (PFNGLARETEXTURESRESIDENT)drgl__get_proc_address(pGL, "glAreTexturesResident");
    pGL->ArrayElement = (PFNGLARRAYELEMENT)drgl__get_proc_address(pGL, "glArrayElement");
    pGL->Begin = (PFNGLBEGIN)drgl__get_proc_address(pGL, "glBegin");
    pGL->BindTexture = (PFNGLBINDTEXTURE)drgl__get_proc_address(pGL, "glBindTexture");
    pGL->Bitmap = (PFNGLBITMAP)drgl__get_proc_address(pGL, "glBitmap");
    pGL->BlendFunc = (PFNGLBLENDFUNC)drgl__get_proc_address(pGL, "glBlendFunc");
    pGL->CallList = (PFNGLCALLLIST)drgl__get_proc_address(pGL, "glCallList");
    pGL->CallLists = (PFNGLCALLLISTS)drgl__get_proc_address(pGL, "glCallLists");
    pGL->Clear = (PFNGLCLEAR)drgl__get_proc_address(pGL, "glClear");
    pGL->ClearAccum = (PFNGLCLEARACCUM)drgl__get_proc_address(pGL, "glClearAccum");
    pGL->ClearColor = (PFNGLCLEARCOLOR)drgl__get_proc_address(pGL, "glClearColor");
    pGL->ClearDepth = (PFNGLCLEARDEPTH)drgl__get_proc_address(pGL, "glClearDepth");
    pGL->ClearIndex = (PFNGLCLEARINDEX)drgl__get_proc_address(pGL, "glClearIndex");
    pGL->ClearStencil = (PFNGLCLEARSTENCIL)drgl__get_proc_address(pGL, "glClearStencil");
    pGL->ClipPlane = (PFNGLCLIPPLANE)drgl__get_proc_address(pGL, "glClipPlane");
    pGL->Color3b = (PFNGLCOLOR3B)drgl__get_proc_address(pGL, "glColor3b");
    pGL->Color3bv = (PFNGLCOLOR3BV)drgl__get_proc_address(pGL, "glColor3bv");
    pGL->Color3d = (PFNGLCOLOR3D)drgl__get_proc_address(pGL, "glColor3d");
    pGL->Color3dv = (PFNGLCOLOR3DV)drgl__get_proc_address(pGL, "glColor3dv");
    pGL->Color3f = (PFNGLCOLOR3F)drgl__get_proc_address(pGL, "glColor3f");
    pGL->Color3fv = (PFNGLCOLOR3FV)drgl__get_proc_address(pGL, "glColor3fv");
    pGL->Color3i = (PFNGLCOLOR3I)drgl__get_proc_address(pGL, "glColor3i");
    pGL->Color3iv = (PFNGLCOLOR3IV)drgl__get_proc_address(pGL, "glColor3iv");
    pGL->Color3s = (PFNGLCOLOR3S)drgl__get_proc_address(pGL, "glColor3s");
    pGL->Color3sv = (PFNGLCOLOR3SV)drgl__get_proc_address(pGL, "glColor3sv");
    pGL->Color3ub = (PFNGLCOLOR3UB)drgl__get_proc_address(pGL, "glColor3ub");
    pGL->Color3ubv = (PFNGLCOLOR3UBV)drgl__get_proc_address(pGL, "glColor3ubv");
    pGL->Color3ui = (PFNGLCOLOR3UI)drgl__get_proc_address(pGL, "glColor3ui");
    pGL->Color3uiv = (PFNGLCOLOR3UIV)drgl__get_proc_address(pGL, "glColor3uiv");
    pGL->Color3us = (PFNGLCOLOR3US)drgl__get_proc_address(pGL, "glColor3us");
    pGL->Color3usv = (PFNGLCOLOR3USV)drgl__get_proc_address(pGL, "glColor3usv");
    pGL->Color4b = (PFNGLCOLOR4B)drgl__get_proc_address(pGL, "glColor4b");
    pGL->Color4bv = (PFNGLCOLOR4BV)drgl__get_proc_address(pGL, "glColor4bv");
    pGL->Color4d = (PFNGLCOLOR4D)drgl__get_proc_address(pGL, "glColor4d");
    pGL->Color4dv = (PFNGLCOLOR4DV)drgl__get_proc_address(pGL, "glColor4dv");
    pGL->Color4f = (PFNGLCOLOR4F)drgl__get_proc_address(pGL, "glColor4f");
    pGL->Color4fv = (PFNGLCOLOR4FV)drgl__get_proc_address(pGL, "glColor4fv");
    pGL->Color4i = (PFNGLCOLOR4I)drgl__get_proc_address(pGL, "glColor4i");
    pGL->Color4iv = (PFNGLCOLOR4IV)drgl__get_proc_address(pGL, "glColor4iv");
    pGL->Color4s = (PFNGLCOLOR4S)drgl__get_proc_address(pGL, "glColor4s");
    pGL->Color4sv = (PFNGLCOLOR4SV)drgl__get_proc_address(pGL, "glColor4sv");
    pGL->Color4ub = (PFNGLCOLOR4UB)drgl__get_proc_address(pGL, "glColor4ub");
    pGL->Color4ubv = (PFNGLCOLOR4UBV)drgl__get_proc_address(pGL, "glColor4ubv");
    pGL->Color4ui = (PFNGLCOLOR4UI)drgl__get_proc_address(pGL, "glColor4ui");
    pGL->Color4uiv = (PFNGLCOLOR4UIV)drgl__get_proc_address(pGL, "glColor4uiv");
    pGL->Color4us = (PFNGLCOLOR4US)drgl__get_proc_address(pGL, "glColor4us");
    pGL->Color4usv = (PFNGLCOLOR4USV)drgl__get_proc_address(pGL, "glColor4usv");
    pGL->ColorMask = (PFNGLCOLORMASK)drgl__get_proc_address(pGL, "glColorMask");
    pGL->ColorMaterial = (PFNGLCOLORMATERIAL)drgl__get_proc_address(pGL, "glColorMaterial");
    pGL->ColorPointer = (PFNGLCOLORPOINTER)drgl__get_proc_address(pGL, "glColorPointer");
    pGL->CopyPixels = (PFNGLCOPYPIXELS)drgl__get_proc_address(pGL, "glCopyPixels");
    pGL->CopyTexImage1D = (PFNGLCOPYTEXIMAGE1D)drgl__get_proc_address(pGL, "glCopyTexImage1D");
    pGL->CopyTexImage2D = (PFNGLCOPYTEXIMAGE2D)drgl__get_proc_address(pGL, "glCopyTexImage2D");
    pGL->CopyTexSubImage1D = (PFNGLCOPYTEXSUBIMAGE1D)drgl__get_proc_address(pGL, "glCopyTexSubImage1D");
    pGL->CopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2D)drgl__get_proc_address(pGL, "glCopyTexSubImage2D");
    pGL->CullFace = (PFNGLCULLFACE)drgl__get_proc_address(pGL, "glCullFace");
    pGL->DeleteLists = (PFNGLDELETELISTS)drgl__get_proc_address(pGL, "glDeleteLists");
    pGL->DeleteTextures = (PFNGLDELETETEXTURES)drgl__get_proc_address(pGL, "glDeleteTextures");
    pGL->DepthFunc = (PFNGLDEPTHFUNC)drgl__get_proc_address(pGL, "glDepthFunc");
    pGL->DepthMask = (PFNGLDEPTHMASK)drgl__get_proc_address(pGL, "glDepthMask");
    pGL->DepthRange = (PFNGLDEPTHRANGE)drgl__get_proc_address(pGL, "glDepthRange");
    pGL->Disable = (PFNGLDISABLE)drgl__get_proc_address(pGL, "glDisable");
    pGL->DisableClientState = (PFNGLDISABLECLIENTSTATE)drgl__get_proc_address(pGL, "glDisableClientState");
    pGL->DrawArrays = (PFNGLDRAWARRAYS)drgl__get_proc_address(pGL, "glDrawArrays");
    pGL->DrawBuffer = (PFNGLDRAWBUFFER)drgl__get_proc_address(pGL, "glDrawBuffer");
    pGL->DrawElements = (PFNGLDRAWELEMENTS)drgl__get_proc_address(pGL, "glDrawElements");
    pGL->DrawPixels = (PFNGLDRAWPIXELS)drgl__get_proc_address(pGL, "glDrawPixels");
    pGL->EdgeFlag = (PFNGLEDGEFLAG)drgl__get_proc_address(pGL, "glEdgeFlag");
    pGL->EdgeFlagPointer = (PFNGLEDGEFLAGPOINTER)drgl__get_proc_address(pGL, "glEdgeFlagPointer");
    pGL->EdgeFlagv = (PFNGLEDGEFLAGV)drgl__get_proc_address(pGL, "glEdgeFlagv");
    pGL->Enable = (PFNGLENABLE)drgl__get_proc_address(pGL, "glEnable");
    pGL->EnableClientState = (PFNGLENABLECLIENTSTATE)drgl__get_proc_address(pGL, "glEnableClientState");
    pGL->End = (PFNGLEND)drgl__get_proc_address(pGL, "glEnd");
    pGL->EndList = (PFNGLENDLIST)drgl__get_proc_address(pGL, "glEndList");
    pGL->EvalCoord1d = (PFNGLEVALCOORD1D)drgl__get_proc_address(pGL, "glEvalCoord1d");
    pGL->EvalCoord1dv = (PFNGLEVALCOORD1DV)drgl__get_proc_address(pGL, "glEvalCoord1dv");
    pGL->EvalCoord1f = (PFNGLEVALCOORD1F)drgl__get_proc_address(pGL, "glEvalCoord1f");
    pGL->EvalCoord1fv = (PFNGLEVALCOORD1FV)drgl__get_proc_address(pGL, "glEvalCoord1fv");
    pGL->EvalCoord2d = (PFNGLEVALCOORD2D)drgl__get_proc_address(pGL, "glEvalCoord2d");
    pGL->EvalCoord2dv = (PFNGLEVALCOORD2DV)drgl__get_proc_address(pGL, "glEvalCoord2dv");
    pGL->EvalCoord2f = (PFNGLEVALCOORD2F)drgl__get_proc_address(pGL, "glEvalCoord2f");
    pGL->EvalCoord2fv = (PFNGLEVALCOORD2FV)drgl__get_proc_address(pGL, "glEvalCoord2fv");
    pGL->EvalMesh1 = (PFNGLEVALMESH1)drgl__get_proc_address(pGL, "glEvalMesh1");
    pGL->EvalMesh2 = (PFNGLEVALMESH2)drgl__get_proc_address(pGL, "glEvalMesh2");
    pGL->EvalPoint1 = (PFNGLEVALPOINT1)drgl__get_proc_address(pGL, "glEvalPoint1");
    pGL->EvalPoint2 = (PFNGLEVALPOINT2)drgl__get_proc_address(pGL, "glEvalPoint2");
    pGL->FeedbackBuffer = (PFNGLFEEDBACKBUFFER)drgl__get_proc_address(pGL, "glFeedbackBuffer");
    pGL->Finish = (PFNGLFINISH)drgl__get_proc_address(pGL, "glFinish");
    pGL->Flush = (PFNGLFLUSH)drgl__get_proc_address(pGL, "glFlush");
    pGL->Fogf = (PFNGLFOGF)drgl__get_proc_address(pGL, "glFogf");
    pGL->Fogfv = (PFNGLFOGFV)drgl__get_proc_address(pGL, "glFogfv");
    pGL->Fogi = (PFNGLFOGI)drgl__get_proc_address(pGL, "glFogi");
    pGL->Fogiv = (PFNGLFOGIV)drgl__get_proc_address(pGL, "glFogiv");
    pGL->FrontFace = (PFNGLFRONTFACE)drgl__get_proc_address(pGL, "glFrontFace");
    pGL->Frustum = (PFNGLFRUSTUM)drgl__get_proc_address(pGL, "glFrustum");
    pGL->GenLists = (PFNGLGENLISTS)drgl__get_proc_address(pGL, "glGenLists");
    pGL->GenTextures = (PFNGLGENTEXTURES)drgl__get_proc_address(pGL, "glGenTextures");
    pGL->GetBooleanv = (PFNGLGETBOOLEANV)drgl__get_proc_address(pGL, "glGetBooleanv");
    pGL->GetClipPlane = (PFNGLGETCLIPPLANE)drgl__get_proc_address(pGL, "glGetClipPlane");
    pGL->GetDoublev = (PFNGLGETDOUBLEV)drgl__get_proc_address(pGL, "glGetDoublev");
    pGL->GetError = (PFNGLGETERROR)drgl__get_proc_address(pGL, "glGetError");
    pGL->GetFloatv = (PFNGLGETFLOATV)drgl__get_proc_address(pGL, "glGetFloatv");
    pGL->GetIntegerv = (PFNGLGETINTEGERV)drgl__get_proc_address(pGL, "glGetIntegerv");
    pGL->GetLightfv = (PFNGLGETLIGHTFV)drgl__get_proc_address(pGL, "glGetLightfv");
    pGL->GetLightiv = (PFNGLGETLIGHTIV)drgl__get_proc_address(pGL, "glGetLightiv");
    pGL->GetMapdv = (PFNGLGETMAPDV)drgl__get_proc_address(pGL, "glGetMapdv");
    pGL->GetMapfv = (PFNGLGETMAPFV)drgl__get_proc_address(pGL, "glGetMapfv");
    pGL->GetMapiv = (PFNGLGETMAPIV)drgl__get_proc_address(pGL, "glGetMapiv");
    pGL->GetMaterialfv = (PFNGLGETMATERIALFV)drgl__get_proc_address(pGL, "glGetMaterialfv");
    pGL->GetMaterialiv = (PFNGLGETMATERIALIV)drgl__get_proc_address(pGL, "glGetMaterialiv");
    pGL->GetPixelMapfv = (PFNGLGETPIXELMAPFV)drgl__get_proc_address(pGL, "glGetPixelMapfv");
    pGL->GetPixelMapuiv = (PFNGLGETPIXELMAPUIV)drgl__get_proc_address(pGL, "glGetPixelMapuiv");
    pGL->GetPixelMapusv = (PFNGLGETPIXELMAPUSV)drgl__get_proc_address(pGL, "glGetPixelMapusv");
    pGL->GetPointerv = (PFNGLGETPOINTERV)drgl__get_proc_address(pGL, "glGetPointerv");
    pGL->GetPolygonStipple = (PFNGLGETPOLYGONSTIPPLE)drgl__get_proc_address(pGL, "glGetPolygonStipple");
    pGL->GetString = (PFNGLGETSTRING)drgl__get_proc_address(pGL, "glGetString");
    pGL->GetTexEnvfv = (PFNGLGETTEXENVFV)drgl__get_proc_address(pGL, "glGetTexEnvfv");
    pGL->GetTexEnviv = (PFNGLGETTEXENVIV)drgl__get_proc_address(pGL, "glGetTexEnviv");
    pGL->GetTexGendv = (PFNGLGETTEXGENDV)drgl__get_proc_address(pGL, "glGetTexGendv");
    pGL->GetTexGenfv = (PFNGLGETTEXGENFV)drgl__get_proc_address(pGL, "glGetTexGenfv");
    pGL->GetTexGeniv = (PFNGLGETTEXGENIV)drgl__get_proc_address(pGL, "glGetTexGeniv");
    pGL->GetTexImage = (PFNGLGETTEXIMAGE)drgl__get_proc_address(pGL, "glGetTexImage");
    pGL->GetTexLevelParameterfv = (PFNGLGETTEXLEVELPARAMETERFV)drgl__get_proc_address(pGL, "glGetTexLevelParameterfv");
    pGL->GetTexLevelParameteriv = (PFNGLGETTEXLEVELPARAMETERIV)drgl__get_proc_address(pGL, "glGetTexLevelParameteriv");
    pGL->GetTexParameterfv = (PFNGLGETTEXPARAMETERFV)drgl__get_proc_address(pGL, "glGetTexParameterfv");
    pGL->GetTexParameteriv = (PFNGLGETTEXPARAMETERIV)drgl__get_proc_address(pGL, "glGetTexParameteriv");
    pGL->Hint = (PFNGLHINT)drgl__get_proc_address(pGL, "glHint");
    pGL->IndexMask = (PFNGLINDEXMASK)drgl__get_proc_address(pGL, "glIndexMask");
    pGL->IndexPointer = (PFNGLINDEXPOINTER)drgl__get_proc_address(pGL, "glIndexPointer");
    pGL->Indexd = (PFNGLINDEXD)drgl__get_proc_address(pGL, "glIndexd");
    pGL->Indexdv = (PFNGLINDEXDV)drgl__get_proc_address(pGL, "glIndexdv");
    pGL->Indexf = (PFNGLINDEXF)drgl__get_proc_address(pGL, "glIndexf");
    pGL->Indexfv = (PFNGLINDEXFV)drgl__get_proc_address(pGL, "glIndexfv");
    pGL->Indexi = (PFNGLINDEXI)drgl__get_proc_address(pGL, "glIndexi");
    pGL->Indexiv = (PFNGLINDEXIV)drgl__get_proc_address(pGL, "glIndexiv");
    pGL->Indexs = (PFNGLINDEXS)drgl__get_proc_address(pGL, "glIndexs");
    pGL->Indexsv = (PFNGLINDEXSV)drgl__get_proc_address(pGL, "glIndexsv");
    pGL->Indexub = (PFNGLINDEXUB)drgl__get_proc_address(pGL, "glIndexub");
    pGL->Indexubv = (PFNGLINDEXUBV)drgl__get_proc_address(pGL, "glIndexubv");
    pGL->InitNames = (PFNGLINITNAMES)drgl__get_proc_address(pGL, "glInitNames");
    pGL->InterleavedArrays = (PFNGLINTERLEAVEDARRAYS)drgl__get_proc_address(pGL, "glInterleavedArrays");
    pGL->IsEnabled = (PFNGLISENABLED)drgl__get_proc_address(pGL, "glIsEnabled");
    pGL->IsList = (PFNGLISLIST)drgl__get_proc_address(pGL, "glIsList");
    pGL->IsTexture = (PFNGLISTEXTURE)drgl__get_proc_address(pGL, "glIsTexture");
    pGL->LightModelf = (PFNGLLIGHTMODELF)drgl__get_proc_address(pGL, "glLightModelf");
    pGL->LightModelfv = (PFNGLLIGHTMODELFV)drgl__get_proc_address(pGL, "glLightModelfv");
    pGL->LightModeli = (PFNGLLIGHTMODELI)drgl__get_proc_address(pGL, "glLightModeli");
    pGL->LightModeliv = (PFNGLLIGHTMODELIV)drgl__get_proc_address(pGL, "glLightModeliv");
    pGL->Lightf = (PFNGLLIGHTF)drgl__get_proc_address(pGL, "glLightf");
    pGL->Lightfv = (PFNGLLIGHTFV)drgl__get_proc_address(pGL, "glLightfv");
    pGL->Lighti = (PFNGLLIGHTI)drgl__get_proc_address(pGL, "glLighti");
    pGL->Lightiv = (PFNGLLIGHTIV)drgl__get_proc_address(pGL, "glLightiv");
    pGL->LineStipple = (PFNGLLINESTIPPLE)drgl__get_proc_address(pGL, "glLineStipple");
    pGL->LineWidth = (PFNGLLINEWIDTH)drgl__get_proc_address(pGL, "glLineWidth");
    pGL->ListBase = (PFNGLLISTBASE)drgl__get_proc_address(pGL, "glListBase");
    pGL->LoadIdentity = (PFNGLLOADIDENTITY)drgl__get_proc_address(pGL, "glLoadIdentity");
    pGL->LoadMatrixd = (PFNGLLOADMATRIXD)drgl__get_proc_address(pGL, "glLoadMatrixd");
    pGL->LoadMatrixf = (PFNGLLOADMATRIXF)drgl__get_proc_address(pGL, "glLoadMatrixf");
    pGL->LoadName = (PFNGLLOADNAME)drgl__get_proc_address(pGL, "glLoadName");
    pGL->LogicOp = (PFNGLLOGICOP)drgl__get_proc_address(pGL, "glLogicOp");
    pGL->Map1d = (PFNGLMAP1D)drgl__get_proc_address(pGL, "glMap1d");
    pGL->Map1f = (PFNGLMAP1F)drgl__get_proc_address(pGL, "glMap1f");
    pGL->Map2d = (PFNGLMAP2D)drgl__get_proc_address(pGL, "glMap2d");
    pGL->Map2f = (PFNGLMAP2F)drgl__get_proc_address(pGL, "glMap2f");
    pGL->MapGrid1d = (PFNGLMAPGRID1D)drgl__get_proc_address(pGL, "glMapGrid1d");
    pGL->MapGrid1f = (PFNGLMAPGRID1F)drgl__get_proc_address(pGL, "glMapGrid1f");
    pGL->MapGrid2d = (PFNGLMAPGRID2D)drgl__get_proc_address(pGL, "glMapGrid2d");
    pGL->MapGrid2f = (PFNGLMAPGRID2F)drgl__get_proc_address(pGL, "glMapGrid2f");
    pGL->Materialf = (PFNGLMATERIALF)drgl__get_proc_address(pGL, "glMaterialf");
    pGL->Materialfv = (PFNGLMATERIALFV)drgl__get_proc_address(pGL, "glMaterialfv");
    pGL->Materiali = (PFNGLMATERIALI)drgl__get_proc_address(pGL, "glMateriali");
    pGL->Materialiv = (PFNGLMATERIALIV)drgl__get_proc_address(pGL, "glMaterialiv");
    pGL->MatrixMode = (PFNGLMATRIXMODE)drgl__get_proc_address(pGL, "glMatrixMode");
    pGL->MultMatrixd = (PFNGLMULTMATRIXD)drgl__get_proc_address(pGL, "glMultMatrixd");
    pGL->MultMatrixf = (PFNGLMULTMATRIXF)drgl__get_proc_address(pGL, "glMultMatrixf");
    pGL->NewList = (PFNGLNEWLIST)drgl__get_proc_address(pGL, "glNewList");
    pGL->Normal3b = (PFNGLNORMAL3B)drgl__get_proc_address(pGL, "glNormal3b");
    pGL->Normal3bv = (PFNGLNORMAL3BV)drgl__get_proc_address(pGL, "glNormal3bv");
    pGL->Normal3d = (PFNGLNORMAL3D)drgl__get_proc_address(pGL, "glNormal3d");
    pGL->Normal3dv = (PFNGLNORMAL3DV)drgl__get_proc_address(pGL, "glNormal3dv");
    pGL->Normal3f = (PFNGLNORMAL3F)drgl__get_proc_address(pGL, "glNormal3f");
    pGL->Normal3fv = (PFNGLNORMAL3FV)drgl__get_proc_address(pGL, "glNormal3fv");
    pGL->Normal3i = (PFNGLNORMAL3I)drgl__get_proc_address(pGL, "glNormal3i");
    pGL->Normal3iv = (PFNGLNORMAL3IV)drgl__get_proc_address(pGL, "glNormal3iv");
    pGL->Normal3s = (PFNGLNORMAL3S)drgl__get_proc_address(pGL, "glNormal3s");
    pGL->Normal3sv = (PFNGLNORMAL3SV)drgl__get_proc_address(pGL, "glNormal3sv");
    pGL->NormalPointer = (PFNGLNORMALPOINTER)drgl__get_proc_address(pGL, "glNormalPointer");
    pGL->Ortho = (PFNGLORTHO)drgl__get_proc_address(pGL, "glOrtho");
    pGL->PassThrough = (PFNGLPATHTHROUGH)drgl__get_proc_address(pGL, "glPassThrough");
    pGL->PixelMapfv = (PFNGLPIXELMAPFV)drgl__get_proc_address(pGL, "glPixelMapfv");
    pGL->PixelMapuiv = (PFNGLPIXELMAPUIV)drgl__get_proc_address(pGL, "glPixelMapuiv");
    pGL->PixelMapusv = (PFNGLPIXELMAPUSV)drgl__get_proc_address(pGL, "glPixelMapusv");
    pGL->PixelStoref = (PFNGLPIXELSTOREF)drgl__get_proc_address(pGL, "glPixelStoref");
    pGL->PixelStorei = (PFNGLPIXELSTOREI)drgl__get_proc_address(pGL, "glPixelStorei");
    pGL->PixelTransferf = (PFNGLPIXELTRANSFERF)drgl__get_proc_address(pGL, "glPixelTransferf");
    pGL->PixelTransferi = (PFNGLPIXELTRANSGERI)drgl__get_proc_address(pGL, "glPixelTransferi");
    pGL->PixelZoom = (PFNGLPIXELZOOM)drgl__get_proc_address(pGL, "glPixelZoom");
    pGL->PointSize = (PFNGLPOINTSIZE)drgl__get_proc_address(pGL, "glPointSize");
    pGL->PolygonMode = (PFNGLPOLYGONMODE)drgl__get_proc_address(pGL, "glPolygonMode");
    pGL->PolygonOffset = (PFNGLPOLYGONOFFSET)drgl__get_proc_address(pGL, "glPolygonOffset");
    pGL->PolygonStipple = (PFNGLPOLYGONSTIPPLE)drgl__get_proc_address(pGL, "glPolygonStipple");
    pGL->PopAttrib = (PFNGLPOPATTRIB)drgl__get_proc_address(pGL, "glPopAttrib");
    pGL->PopClientAttrib = (PFNGLPOPCLIENTATTRIB)drgl__get_proc_address(pGL, "glPopClientAttrib");
    pGL->PopMatrix = (PFNGLPOPMATRIX)drgl__get_proc_address(pGL, "glPopMatrix");
    pGL->PopName = (PFNGLPOPNAME)drgl__get_proc_address(pGL, "glPopName");
    pGL->PrioritizeTextures = (PFNGLPRIORITIZETEXTURES)drgl__get_proc_address(pGL, "glPrioritizeTextures");
    pGL->PushAttrib = (PFNGLPUSHATTRIB)drgl__get_proc_address(pGL, "glPushAttrib");
    pGL->PushClientAttrib = (PFNGLPUSHCLIENTATTRIB)drgl__get_proc_address(pGL, "glPushClientAttrib");
    pGL->PushMatrix = (PFNGLPUSHMATRIX)drgl__get_proc_address(pGL, "glPushMatrix");
    pGL->PushName = (PFNGLPUSHNAME)drgl__get_proc_address(pGL, "glPushName");
    pGL->RasterPos2d = (PFNGLRASTERPOS2D)drgl__get_proc_address(pGL, "glRasterPos2d");
    pGL->RasterPos2dv = (PFNGLRASTERPOS2DV)drgl__get_proc_address(pGL, "glRasterPos2dv");
    pGL->RasterPos2f = (PFNGLRASTERPOS2F)drgl__get_proc_address(pGL, "glRasterPos2f");
    pGL->RasterPos2fv = (PFNGLRASTERPOS2FV)drgl__get_proc_address(pGL, "glRasterPos2fv");
    pGL->RasterPos2i = (PFNGLRASTERPOS2I)drgl__get_proc_address(pGL, "glRasterPos2i");
    pGL->RasterPos2iv = (PFNGLRASTERPOS2IV)drgl__get_proc_address(pGL, "glRasterPos2iv");
    pGL->RasterPos2s = (PFNGLRASTERPOS2S)drgl__get_proc_address(pGL, "glRasterPos2s");
    pGL->RasterPos2sv = (PFNGLRASTERPOS2SV)drgl__get_proc_address(pGL, "glRasterPos2sv");
    pGL->RasterPos3d = (PFNGLRASTERPOS3D)drgl__get_proc_address(pGL, "glRasterPos3d");
    pGL->RasterPos3dv = (PFNGLRASTERPOS3DV)drgl__get_proc_address(pGL, "glRasterPos3dv");
    pGL->RasterPos3f = (PFNGLRASTERPOS3F)drgl__get_proc_address(pGL, "glRasterPos3f");
    pGL->RasterPos3fv = (PFNGLRASTERPOS3FV)drgl__get_proc_address(pGL, "glRasterPos3fv");
    pGL->RasterPos3i = (PFNGLRASTERPOS3I)drgl__get_proc_address(pGL, "glRasterPos3i");
    pGL->RasterPos3iv = (PFNGLRASTERPOS3IV)drgl__get_proc_address(pGL, "glRasterPos3iv");
    pGL->RasterPos3s = (PFNGLRASTERPOS3S)drgl__get_proc_address(pGL, "glRasterPos3s");
    pGL->RasterPos3sv = (PFNGLRASTERPOS3SV)drgl__get_proc_address(pGL, "glRasterPos3sv");
    pGL->RasterPos4d = (PFNGLRASTERPOS4D)drgl__get_proc_address(pGL, "glRasterPos4d");
    pGL->RasterPos4dv = (PFNGLRASTERPOS4DV)drgl__get_proc_address(pGL, "glRasterPos4dv");
    pGL->RasterPos4f = (PFNGLRASTERPOS4F)drgl__get_proc_address(pGL, "glRasterPos4f");
    pGL->RasterPos4fv = (PFNGLRASTERPOS4FV)drgl__get_proc_address(pGL, "glRasterPos4fv");
    pGL->RasterPos4i = (PFNGLRASTERPOS4I)drgl__get_proc_address(pGL, "glRasterPos4i");
    pGL->RasterPos4iv = (PFNGLRASTERPOS4IV)drgl__get_proc_address(pGL, "glRasterPos4iv");
    pGL->RasterPos4s = (PFNGLRASTERPOS4S)drgl__get_proc_address(pGL, "glRasterPos4s");
    pGL->RasterPos4sv = (PFNGLRASTERPOS4SV)drgl__get_proc_address(pGL, "glRasterPos4sv");
    pGL->ReadBuffer = (PFNGLREADBUFFER)drgl__get_proc_address(pGL, "glReadBuffer");
    pGL->ReadPixels = (PFNGLREADPIXELS)drgl__get_proc_address(pGL, "glReadPixels");
    pGL->Rectd = (PFNGLRECTD)drgl__get_proc_address(pGL, "glRectd");
    pGL->Rectdv = (PFNGLRECTDV)drgl__get_proc_address(pGL, "glRectdv");
    pGL->Rectf = (PFNGLRECTF)drgl__get_proc_address(pGL, "glRectf");
    pGL->Rectfv = (PFNGLRECTFV)drgl__get_proc_address(pGL, "glRectfv");
    pGL->Recti = (PFNGLRECTI)drgl__get_proc_address(pGL, "glRecti");
    pGL->Rectiv = (PFNGLRECTIV)drgl__get_proc_address(pGL, "glRectiv");
    pGL->Rects = (PFNGLRECTS)drgl__get_proc_address(pGL, "glRects");
    pGL->Rectsv = (PFNGLRECTSV)drgl__get_proc_address(pGL, "glRectsv");
    pGL->RenderMode = (PFNGLRENDERMODE)drgl__get_proc_address(pGL, "glRenderMode");
    pGL->Rotated = (PFNGLROTATED)drgl__get_proc_address(pGL, "glRotated");
    pGL->Rotatef = (PFNGLROTATEF)drgl__get_proc_address(pGL, "glRotatef");
    pGL->Scaled = (PFNGLSCALED)drgl__get_proc_address(pGL, "glScaled");
    pGL->Scalef = (PFNGLSCALEF)drgl__get_proc_address(pGL, "glScalef");
    pGL->Scissor = (PFNGLSCISSOR)drgl__get_proc_address(pGL, "glScissor");
    pGL->SelectBuffer = (PFNGLSELECTBUFFER)drgl__get_proc_address(pGL, "glSelectBuffer");
    pGL->ShadeModel = (PFNGLSHADEMODEL)drgl__get_proc_address(pGL, "glShadeModel");
    pGL->StencilFunc = (PFNGLSTENCILFUNC)drgl__get_proc_address(pGL, "glStencilFunc");
    pGL->StencilMask = (PFNGLSTENCILMASK)drgl__get_proc_address(pGL, "glStencilMask");
    pGL->StencilOp = (PFNGLSTENCILOP)drgl__get_proc_address(pGL, "glStencilOp");
    pGL->TexCoord1d = (PFNGLTEXCOORD1D)drgl__get_proc_address(pGL, "glTexCoord1d");
    pGL->TexCoord1dv = (PFNGLTEXCOORD1DV)drgl__get_proc_address(pGL, "glTexCoord1dv");
    pGL->TexCoord1f = (PFNGLTEXCOORD1F)drgl__get_proc_address(pGL, "glTexCoord1f");
    pGL->TexCoord1fv = (PFNGLTEXCOORD1FV)drgl__get_proc_address(pGL, "glTexCoord1fv");
    pGL->TexCoord1i = (PFNGLTEXCOORD1I)drgl__get_proc_address(pGL, "glTexCoord1i");
    pGL->TexCoord1iv = (PFNGLTEXCOORD1IV)drgl__get_proc_address(pGL, "glTexCoord1iv");
    pGL->TexCoord1s = (PFNGLTEXCOORD1S)drgl__get_proc_address(pGL, "glTexCoord1s");
    pGL->TexCoord1sv = (PFNGLTEXCOORD1SV)drgl__get_proc_address(pGL, "glTexCoord1sv");
    pGL->TexCoord2d = (PFNGLTEXCOORD2D)drgl__get_proc_address(pGL, "glTexCoord2d");
    pGL->TexCoord2dv = (PFNGLTEXCOORD2DV)drgl__get_proc_address(pGL, "glTexCoord2dv");
    pGL->TexCoord2f = (PFNGLTEXCOORD2F)drgl__get_proc_address(pGL, "glTexCoord2f");
    pGL->TexCoord2fv = (PFNGLTEXCOORD2FV)drgl__get_proc_address(pGL, "glTexCoord2fv");
    pGL->TexCoord2i = (PFNGLTEXCOORD2I)drgl__get_proc_address(pGL, "glTexCoord2i");
    pGL->TexCoord2iv = (PFNGLTEXCOORD2IV)drgl__get_proc_address(pGL, "glTexCoord2iv");
    pGL->TexCoord2s = (PFNGLTEXCOORD2S)drgl__get_proc_address(pGL, "glTexCoord2s");
    pGL->TexCoord2sv = (PFNGLTEXCOORD2SV)drgl__get_proc_address(pGL, "glTexCoord2sv");
    pGL->TexCoord3d = (PFNGLTEXCOORD3D)drgl__get_proc_address(pGL, "glTexCoord3d");
    pGL->TexCoord3dv = (PFNGLTEXCOORD3DV)drgl__get_proc_address(pGL, "glTexCoord3dv");
    pGL->TexCoord3f = (PFNGLTEXCOORD3F)drgl__get_proc_address(pGL, "glTexCoord3f");
    pGL->TexCoord3fv = (PFNGLTEXCOORD3FV)drgl__get_proc_address(pGL, "glTexCoord3fv");
    pGL->TexCoord3i = (PFNGLTEXCOORD3I)drgl__get_proc_address(pGL, "glTexCoord3i");
    pGL->TexCoord3iv = (PFNGLTEXCOORD3IV)drgl__get_proc_address(pGL, "glTexCoord3iv");
    pGL->TexCoord3s = (PFNGLTEXCOORD3S)drgl__get_proc_address(pGL, "glTexCoord3s");
    pGL->TexCoord3sv = (PFNGLTEXCOORD3SV)drgl__get_proc_address(pGL, "glTexCoord3sv");
    pGL->TexCoord4d = (PFNGLTEXCOORD4D)drgl__get_proc_address(pGL, "glTexCoord4d");
    pGL->TexCoord4dv = (PFNGLTEXCOORD4DV)drgl__get_proc_address(pGL, "glTexCoord4dv");
    pGL->TexCoord4f = (PFNGLTEXCOORD4F)drgl__get_proc_address(pGL, "glTexCoord4f");
    pGL->TexCoord4fv = (PFNGLTEXCOORD4FV)drgl__get_proc_address(pGL, "glTexCoord4fv");
    pGL->TexCoord4i = (PFNGLTEXCOORD4I)drgl__get_proc_address(pGL, "glTexCoord4i");
    pGL->TexCoord4iv = (PFNGLTEXCOORD4IV)drgl__get_proc_address(pGL, "glTexCoord4iv");
    pGL->TexCoord4s = (PFNGLTEXCOORD4S)drgl__get_proc_address(pGL, "glTexCoord4s");
    pGL->TexCoord4sv = (PFNGLTEXCOORD4SV)drgl__get_proc_address(pGL, "glTexCoord4sv");
    pGL->TexCoordPointer = (PFNGLTEXCOORDPOINTER)drgl__get_proc_address(pGL, "glTexCoordPointer");
    pGL->TexEnvf = (PFNGLTEXENVF)drgl__get_proc_address(pGL, "glTexEnvf");
    pGL->TexEnvfv = (PFNGLTEXENVFV)drgl__get_proc_address(pGL, "glTexEnvfv");
    pGL->TexEnvi = (PFNGLTEXENVI)drgl__get_proc_address(pGL, "glTexEnvi");
    pGL->TexEnviv = (PFNGLTEXENVIV)drgl__get_proc_address(pGL, "glTexEnviv");
    pGL->TexGend = (PFNGLTEXGEND)drgl__get_proc_address(pGL, "glTexGend");
    pGL->TexGendv = (PFNGLTEXGENDV)drgl__get_proc_address(pGL, "glTexGendv");
    pGL->TexGenf = (PFNGLTEXGENF)drgl__get_proc_address(pGL, "glTexGenf");
    pGL->TexGenfv = (PFNGLTEXGENFV)drgl__get_proc_address(pGL, "glTexGenfv");
    pGL->TexGeni = (PFNGLTEXGENI)drgl__get_proc_address(pGL, "glTexGeni");
    pGL->TexGeniv = (PFNGLTEXGENIV)drgl__get_proc_address(pGL, "glTexGeniv");
    pGL->TexImage1D = (PFNGLTEXIMAGE1D)drgl__get_proc_address(pGL, "glTexImage1D");
    pGL->TexImage2D = (PFNGLTEXIMAGE2D)drgl__get_proc_address(pGL, "glTexImage2D");
    pGL->TexParameterf = (PFNGLTEXPARAMETERF)drgl__get_proc_address(pGL, "glTexParameterf");
    pGL->TexParameterfv = (PFNGLTEXPARAMETERFV)drgl__get_proc_address(pGL, "glTexParameterfv");
    pGL->TexParameteri = (PFNGLTEXPARAMETERI)drgl__get_proc_address(pGL, "glTexParameteri");
    pGL->TexParameteriv = (PFNGLTEXPARAMETERIV)drgl__get_proc_address(pGL, "glTexParameteriv");
    pGL->TexSubImage1D = (PFNGLTEXSUBIMAGE1D)drgl__get_proc_address(pGL, "glTexSubImage1D");
    pGL->TexSubImage2D = (PFNGLTEXSUBIMAGE2D)drgl__get_proc_address(pGL, "glTexSubImage2D");
    pGL->Translated = (PFNGLTRANSLATED)drgl__get_proc_address(pGL, "glTranslated");
    pGL->Translatef = (PFNGLTRANSLATEF)drgl__get_proc_address(pGL, "glTranslatef");
    pGL->Vertex2d = (PFNGLVERTEX2D)drgl__get_proc_address(pGL, "glVertex2d");
    pGL->Vertex2dv = (PFNGLVERTEX2DV)drgl__get_proc_address(pGL, "glVertex2dv");
    pGL->Vertex2f = (PFNGLVERTEX2F)drgl__get_proc_address(pGL, "glVertex2f");
    pGL->Vertex2fv = (PFNGLVERTEX2FV)drgl__get_proc_address(pGL, "glVertex2fv");
    pGL->Vertex2i = (PFNGLVERTEX2I)drgl__get_proc_address(pGL, "glVertex2i");
    pGL->Vertex2iv = (PFNGLVERTEX2IV)drgl__get_proc_address(pGL, "glVertex2iv");
    pGL->Vertex2s = (PFNGLVERTEX2S)drgl__get_proc_address(pGL, "glVertex2s");
    pGL->Vertex2sv = (PFNGLVERTEX2SV)drgl__get_proc_address(pGL, "glVertex2sv");
    pGL->Vertex3d = (PFNGLVERTEX3D)drgl__get_proc_address(pGL, "glVertex3d");
    pGL->Vertex3dv = (PFNGLVERTEX3DV)drgl__get_proc_address(pGL, "glVertex3dv");
    pGL->Vertex3f = (PFNGLVERTEX3F)drgl__get_proc_address(pGL, "glVertex3f");
    pGL->Vertex3fv = (PFNGLVERTEX3FV)drgl__get_proc_address(pGL, "glVertex3fv");
    pGL->Vertex3i = (PFNGLVERTEX3I)drgl__get_proc_address(pGL, "glVertex3i");
    pGL->Vertex3iv = (PFNGLVERTEX3IV)drgl__get_proc_address(pGL, "glVertex3iv");
    pGL->Vertex3s = (PFNGLVERTEX3S)drgl__get_proc_address(pGL, "glVertex3s");
    pGL->Vertex3sv = (PFNGLVERTEX3SV)drgl__get_proc_address(pGL, "glVertex3sv");
    pGL->Vertex4d = (PFNGLVERTEX4D)drgl__get_proc_address(pGL, "glVertex4d");
    pGL->Vertex4dv = (PFNGLVERTEX4DV)drgl__get_proc_address(pGL, "glVertex4dv");
    pGL->Vertex4f = (PFNGLVERTEX4F)drgl__get_proc_address(pGL, "glVertex4f");
    pGL->Vertex4fv = (PFNGLVERTEX4FV)drgl__get_proc_address(pGL, "glVertex4fv");
    pGL->Vertex4i = (PFNGLVERTEX4I)drgl__get_proc_address(pGL, "glVertex4i");
    pGL->Vertex4iv = (PFNGLVERTEX4IV)drgl__get_proc_address(pGL, "glVertex4iv");
    pGL->Vertex4s = (PFNGLVERTEX4S)drgl__get_proc_address(pGL, "glVertex4s");
    pGL->Vertex4sv = (PFNGLVERTEX4SV)drgl__get_proc_address(pGL, "glVertex4sv");
    pGL->VertexPointer = (PFNGLVERTEXPOINTER)drgl__get_proc_address(pGL, "glVertexPointer");
    pGL->Viewport = (PFNGLVIEWPORT)drgl__get_proc_address(pGL, "glViewport");
#endif

    // OpenGL 1.2
#if DR_GL_VERSION >= 120
    pGL->DrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC)drgl__get_proc_address(pGL, "glDrawRangeElements");
    pGL->TexImage3D = (PFNGLTEXIMAGE3DPROC)drgl__get_proc_address(pGL, "glTexImage3D");
    pGL->TexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)drgl__get_proc_address(pGL, "glTexSubImage3D");
    pGL->CopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC)drgl__get_proc_address(pGL, "glCopyTexSubImage3D");
#endif

    // OpenGL 1.3
#if DR_GL_VERSION >= 130
    pGL->ActiveTexture = (PFNGLACTIVETEXTUREPROC)drgl__get_proc_address(pGL, "glActiveTexture");
    pGL->SampleCoverage = (PFNGLSAMPLECOVERAGEPROC)drgl__get_proc_address(pGL, "glSampleCoverage");
    pGL->CompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC)drgl__get_proc_address(pGL, "glCompressedTexImage3D");
    pGL->CompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)drgl__get_proc_address(pGL, "glCompressedTexImage2D");
    pGL->CompressedTexImage1D = (PFNGLCOMPRESSEDTEXIMAGE1DPROC)drgl__get_proc_address(pGL, "glCompressedTexImage1D");
    pGL->CompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)drgl__get_proc_address(pGL, "glCompressedTexSubImage3D");
    pGL->CompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)drgl__get_proc_address(pGL, "glCompressedTexSubImage2D");
    pGL->CompressedTexSubImage1D = (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)drgl__get_proc_address(pGL, "glCompressedTexSubImage1D");
    pGL->GetCompressedTexImage = (PFNGLGETCOMPRESSEDTEXIMAGEPROC)drgl__get_proc_address(pGL, "glGetCompressedTexImage");
    pGL->ClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)drgl__get_proc_address(pGL, "glClientActiveTexture");
    pGL->MultiTexCoord1d = (PFNGLMULTITEXCOORD1DPROC)drgl__get_proc_address(pGL, "glMultiTexCoord1d");
    pGL->MultiTexCoord1dv = (PFNGLMULTITEXCOORD1DVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord1dv");
    pGL->MultiTexCoord1f = (PFNGLMULTITEXCOORD1FPROC)drgl__get_proc_address(pGL, "glMultiTexCoord1f");
    pGL->MultiTexCoord1fv = (PFNGLMULTITEXCOORD1FVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord1fv");
    pGL->MultiTexCoord1i = (PFNGLMULTITEXCOORD1IPROC)drgl__get_proc_address(pGL, "glMultiTexCoord1i");
    pGL->MultiTexCoord1iv = (PFNGLMULTITEXCOORD1IVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord1iv");
    pGL->MultiTexCoord1s = (PFNGLMULTITEXCOORD1SPROC)drgl__get_proc_address(pGL, "glMultiTexCoord1s");
    pGL->MultiTexCoord1sv = (PFNGLMULTITEXCOORD1SVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord1sv");
    pGL->MultiTexCoord2d = (PFNGLMULTITEXCOORD2DPROC)drgl__get_proc_address(pGL, "glMultiTexCoord2d");
    pGL->MultiTexCoord2dv = (PFNGLMULTITEXCOORD2DVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord2dv");
    pGL->MultiTexCoord2f = (PFNGLMULTITEXCOORD2FPROC)drgl__get_proc_address(pGL, "glMultiTexCoord2f");
    pGL->MultiTexCoord2fv = (PFNGLMULTITEXCOORD2FVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord2fv");
    pGL->MultiTexCoord2i = (PFNGLMULTITEXCOORD2IPROC)drgl__get_proc_address(pGL, "glMultiTexCoord2i");
    pGL->MultiTexCoord2iv = (PFNGLMULTITEXCOORD2IVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord2iv");
    pGL->MultiTexCoord2s = (PFNGLMULTITEXCOORD2SPROC)drgl__get_proc_address(pGL, "glMultiTexCoord2s");
    pGL->MultiTexCoord2sv = (PFNGLMULTITEXCOORD2SVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord2sv");
    pGL->MultiTexCoord3d = (PFNGLMULTITEXCOORD3DPROC)drgl__get_proc_address(pGL, "glMultiTexCoord3d");
    pGL->MultiTexCoord3dv = (PFNGLMULTITEXCOORD3DVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord3dv");
    pGL->MultiTexCoord3f = (PFNGLMULTITEXCOORD3FPROC)drgl__get_proc_address(pGL, "glMultiTexCoord3f");
    pGL->MultiTexCoord3fv = (PFNGLMULTITEXCOORD3FVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord3fv");
    pGL->MultiTexCoord3i = (PFNGLMULTITEXCOORD3IPROC)drgl__get_proc_address(pGL, "glMultiTexCoord3i");
    pGL->MultiTexCoord3iv = (PFNGLMULTITEXCOORD3IVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord3iv");
    pGL->MultiTexCoord3s = (PFNGLMULTITEXCOORD3SPROC)drgl__get_proc_address(pGL, "glMultiTexCoord3s");
    pGL->MultiTexCoord3sv = (PFNGLMULTITEXCOORD3SVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord3sv");
    pGL->MultiTexCoord4d = (PFNGLMULTITEXCOORD4DPROC)drgl__get_proc_address(pGL, "glMultiTexCoord4d");
    pGL->MultiTexCoord4dv = (PFNGLMULTITEXCOORD4DVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord4dv");
    pGL->MultiTexCoord4f = (PFNGLMULTITEXCOORD4FPROC)drgl__get_proc_address(pGL, "glMultiTexCoord4f");
    pGL->MultiTexCoord4fv = (PFNGLMULTITEXCOORD4FVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord4fv");
    pGL->MultiTexCoord4i = (PFNGLMULTITEXCOORD4IPROC)drgl__get_proc_address(pGL, "glMultiTexCoord4i");
    pGL->MultiTexCoord4iv = (PFNGLMULTITEXCOORD4IVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord4iv");
    pGL->MultiTexCoord4s = (PFNGLMULTITEXCOORD4SPROC)drgl__get_proc_address(pGL, "glMultiTexCoord4s");
    pGL->MultiTexCoord4sv = (PFNGLMULTITEXCOORD4SVPROC)drgl__get_proc_address(pGL, "glMultiTexCoord4sv");
    pGL->LoadTransposeMatrixf = (PFNGLLOADTRANSPOSEMATRIXFPROC)drgl__get_proc_address(pGL, "glLoadTransposeMatrixf");
    pGL->LoadTransposeMatrixd = (PFNGLLOADTRANSPOSEMATRIXDPROC)drgl__get_proc_address(pGL, "glLoadTransposeMatrixd");
    pGL->MultTransposeMatrixf = (PFNGLMULTTRANSPOSEMATRIXFPROC)drgl__get_proc_address(pGL, "glMultTransposeMatrixf");
    pGL->MultTransposeMatrixd = (PFNGLMULTTRANSPOSEMATRIXDPROC)drgl__get_proc_address(pGL, "glMultTransposeMatrixd");
#endif

    // OpenGL 1.4
#if DR_GL_VERSION >= 140
    pGL->BlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)drgl__get_proc_address(pGL, "glBlendFuncSeparate");
    pGL->MultiDrawArrays = (PFNGLMULTIDRAWARRAYSPROC)drgl__get_proc_address(pGL, "glMultiDrawArrays");
    pGL->MultiDrawElements = (PFNGLMULTIDRAWELEMENTSPROC)drgl__get_proc_address(pGL, "glMultiDrawElements");
    pGL->PointParameterf = (PFNGLPOINTPARAMETERFPROC)drgl__get_proc_address(pGL, "glPointParameterf");
    pGL->PointParameterfv = (PFNGLPOINTPARAMETERFVPROC)drgl__get_proc_address(pGL, "glPointParameterfv");
    pGL->PointParameteri = (PFNGLPOINTPARAMETERIPROC)drgl__get_proc_address(pGL, "glPointParameteri");
    pGL->PointParameteriv = (PFNGLPOINTPARAMETERIVPROC)drgl__get_proc_address(pGL, "glPointParameteriv");
    pGL->FogCoordf = (PFNGLFOGCOORDFPROC)drgl__get_proc_address(pGL, "glFogCoordf");
    pGL->FogCoordfv = (PFNGLFOGCOORDFVPROC)drgl__get_proc_address(pGL, "glFogCoordfv");
    pGL->FogCoordd = (PFNGLFOGCOORDDPROC)drgl__get_proc_address(pGL, "glFogCoordd");
    pGL->FogCoorddv = (PFNGLFOGCOORDDVPROC)drgl__get_proc_address(pGL, "glFogCoorddv");
    pGL->FogCoordPointer = (PFNGLFOGCOORDPOINTERPROC)drgl__get_proc_address(pGL, "glFogCoordPointer");
    pGL->SecondaryColor3b = (PFNGLSECONDARYCOLOR3BPROC)drgl__get_proc_address(pGL, "glSecondaryColor3b");
    pGL->SecondaryColor3bv = (PFNGLSECONDARYCOLOR3BVPROC)drgl__get_proc_address(pGL, "glSecondaryColor3bv");
    pGL->SecondaryColor3d = (PFNGLSECONDARYCOLOR3DPROC)drgl__get_proc_address(pGL, "glSecondaryColor3d");
    pGL->SecondaryColor3dv = (PFNGLSECONDARYCOLOR3DVPROC)drgl__get_proc_address(pGL, "glSecondaryColor3dv");
    pGL->SecondaryColor3f = (PFNGLSECONDARYCOLOR3FPROC)drgl__get_proc_address(pGL, "glSecondaryColor3f");
    pGL->SecondaryColor3fv = (PFNGLSECONDARYCOLOR3FVPROC)drgl__get_proc_address(pGL, "glSecondaryColor3fv");
    pGL->SecondaryColor3i = (PFNGLSECONDARYCOLOR3IPROC)drgl__get_proc_address(pGL, "glSecondaryColor3i");
    pGL->SecondaryColor3iv = (PFNGLSECONDARYCOLOR3IVPROC)drgl__get_proc_address(pGL, "glSecondaryColor3iv");
    pGL->SecondaryColor3s = (PFNGLSECONDARYCOLOR3SPROC)drgl__get_proc_address(pGL, "glSecondaryColor3s");
    pGL->SecondaryColor3sv = (PFNGLSECONDARYCOLOR3SVPROC)drgl__get_proc_address(pGL, "glSecondaryColor3sv");
    pGL->SecondaryColor3ub = (PFNGLSECONDARYCOLOR3UBPROC)drgl__get_proc_address(pGL, "glSecondaryColor3ub");
    pGL->SecondaryColor3ubv = (PFNGLSECONDARYCOLOR3UBVPROC)drgl__get_proc_address(pGL, "glSecondaryColor3ubv");
    pGL->SecondaryColor3ui = (PFNGLSECONDARYCOLOR3UIPROC)drgl__get_proc_address(pGL, "glSecondaryColor3ui");
    pGL->SecondaryColor3uiv = (PFNGLSECONDARYCOLOR3UIVPROC)drgl__get_proc_address(pGL, "glSecondaryColor3uiv");
    pGL->SecondaryColor3us = (PFNGLSECONDARYCOLOR3USPROC)drgl__get_proc_address(pGL, "glSecondaryColor3us");
    pGL->SecondaryColor3usv = (PFNGLSECONDARYCOLOR3USVPROC)drgl__get_proc_address(pGL, "glSecondaryColor3usv");
    pGL->SecondaryColorPointer = (PFNGLSECONDARYCOLORPOINTERPROC)drgl__get_proc_address(pGL, "glSecondaryColorPointer");
    pGL->WindowPos2d = (PFNGLWINDOWPOS2DPROC)drgl__get_proc_address(pGL, "glWindowPos2d");
    pGL->WindowPos2dv = (PFNGLWINDOWPOS2DVPROC)drgl__get_proc_address(pGL, "glWindowPos2dv");
    pGL->WindowPos2f = (PFNGLWINDOWPOS2FPROC)drgl__get_proc_address(pGL, "glWindowPos2f");
    pGL->WindowPos2fv = (PFNGLWINDOWPOS2FVPROC)drgl__get_proc_address(pGL, "glWindowPos2fv");
    pGL->WindowPos2i = (PFNGLWINDOWPOS2IPROC)drgl__get_proc_address(pGL, "glWindowPos2i");
    pGL->WindowPos2iv = (PFNGLWINDOWPOS2IVPROC)drgl__get_proc_address(pGL, "glWindowPos2iv");
    pGL->WindowPos2s = (PFNGLWINDOWPOS2SPROC)drgl__get_proc_address(pGL, "glWindowPos2s");
    pGL->WindowPos2sv = (PFNGLWINDOWPOS2SVPROC)drgl__get_proc_address(pGL, "glWindowPos2sv");
    pGL->WindowPos3d = (PFNGLWINDOWPOS3DPROC)drgl__get_proc_address(pGL, "glWindowPos3d");
    pGL->WindowPos3dv = (PFNGLWINDOWPOS3DVPROC)drgl__get_proc_address(pGL, "glWindowPos3dv");
    pGL->WindowPos3f = (PFNGLWINDOWPOS3FPROC)drgl__get_proc_address(pGL, "glWindowPos3f");
    pGL->WindowPos3fv = (PFNGLWINDOWPOS3FVPROC)drgl__get_proc_address(pGL, "glWindowPos3fv");
    pGL->WindowPos3i = (PFNGLWINDOWPOS3IPROC)drgl__get_proc_address(pGL, "glWindowPos3i");
    pGL->WindowPos3iv = (PFNGLWINDOWPOS3IVPROC)drgl__get_proc_address(pGL, "glWindowPos3iv");
    pGL->WindowPos3s = (PFNGLWINDOWPOS3SPROC)drgl__get_proc_address(pGL, "glWindowPos3s");
    pGL->WindowPos3sv = (PFNGLWINDOWPOS3SVPROC)drgl__get_proc_address(pGL, "glWindowPos3sv");
    pGL->BlendColor = (PFNGLBLENDCOLORPROC)drgl__get_proc_address(pGL, "glBlendColor");
    pGL->BlendEquation = (PFNGLBLENDEQUATIONPROC)drgl__get_proc_address(pGL, "glBlendEquation");
#endif

    // OpenGL 1.5
#if DR_GL_VERSION >= 150
    pGL->GenQueries = (PFNGLGENQUERIESPROC)drgl__get_proc_address(pGL, "glGenQueries");
    pGL->DeleteQueries = (PFNGLDELETEQUERIESPROC)drgl__get_proc_address(pGL, "glDeleteQueries");
    pGL->IsQuery = (PFNGLISQUERYPROC)drgl__get_proc_address(pGL, "glIsQuery");
    pGL->BeginQuery = (PFNGLBEGINQUERYPROC)drgl__get_proc_address(pGL, "glBeginQuery");
    pGL->EndQuery = (PFNGLENDQUERYPROC)drgl__get_proc_address(pGL, "glEndQuery");
    pGL->GetQueryiv = (PFNGLGETQUERYIVPROC)drgl__get_proc_address(pGL, "glGetQueryiv");
    pGL->GetQueryObjectiv = (PFNGLGETQUERYOBJECTIVPROC)drgl__get_proc_address(pGL, "glGetQueryObjectiv");
    pGL->GetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC)drgl__get_proc_address(pGL, "glGetQueryObjectuiv");
    pGL->BindBuffer = (PFNGLBINDBUFFERPROC)drgl__get_proc_address(pGL, "glBindBuffer");
    pGL->DeleteBuffers = (PFNGLDELETEBUFFERSPROC)drgl__get_proc_address(pGL, "glDeleteBuffers");
    pGL->GenBuffers = (PFNGLGENBUFFERSPROC)drgl__get_proc_address(pGL, "glGenBuffers");
    pGL->IsBuffer = (PFNGLISBUFFERPROC)drgl__get_proc_address(pGL, "glIsBuffer");
    pGL->BufferData = (PFNGLBUFFERDATAPROC)drgl__get_proc_address(pGL, "glBufferData");
    pGL->BufferSubData = (PFNGLBUFFERSUBDATAPROC)drgl__get_proc_address(pGL, "glBufferSubData");
    pGL->GetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC)drgl__get_proc_address(pGL, "glGetBufferSubData");
    pGL->MapBuffer = (PFNGLMAPBUFFERPROC)drgl__get_proc_address(pGL, "glMapBuffer");
    pGL->UnmapBuffer = (PFNGLUNMAPBUFFERPROC)drgl__get_proc_address(pGL, "glUnmapBuffer");
    pGL->GetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC)drgl__get_proc_address(pGL, "glGetBufferParameteriv");
    pGL->GetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC)drgl__get_proc_address(pGL, "glGetBufferPointerv");
#endif

    // OpenGL 2.0
#if DR_GL_VERSION >= 200
    pGL->BlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC)drgl__get_proc_address(pGL, "glBlendEquationSeparate");
    pGL->DrawBuffers = (PFNGLDRAWBUFFERSPROC)drgl__get_proc_address(pGL, "glDrawBuffers");
    pGL->StencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC)drgl__get_proc_address(pGL, "glStencilOpSeparate");
    pGL->StencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC)drgl__get_proc_address(pGL, "glStencilFuncSeparate");
    pGL->StencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC)drgl__get_proc_address(pGL, "glStencilMaskSeparate");
    pGL->AttachShader = (PFNGLATTACHSHADERPROC)drgl__get_proc_address(pGL, "glAttachShader");
    pGL->BindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)drgl__get_proc_address(pGL, "glBindAttribLocation");
    pGL->CompileShader = (PFNGLCOMPILESHADERPROC)drgl__get_proc_address(pGL, "glCompileShader");
    pGL->CreateProgram = (PFNGLCREATEPROGRAMPROC)drgl__get_proc_address(pGL, "glCreateProgram");
    pGL->CreateShader = (PFNGLCREATESHADERPROC)drgl__get_proc_address(pGL, "glCreateShader");
    pGL->DeleteProgram = (PFNGLDELETEPROGRAMPROC)drgl__get_proc_address(pGL, "glDeleteProgram");
    pGL->DeleteShader = (PFNGLDELETESHADERPROC)drgl__get_proc_address(pGL, "glDeleteShader");
    pGL->DetachShader = (PFNGLDETACHSHADERPROC)drgl__get_proc_address(pGL, "glDetachShader");
    pGL->DisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)drgl__get_proc_address(pGL, "glDisableVertexAttribArray");
    pGL->EnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)drgl__get_proc_address(pGL, "glEnableVertexAttribArray");
    pGL->GetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC)drgl__get_proc_address(pGL, "glGetActiveAttrib");
    pGL->GetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)drgl__get_proc_address(pGL, "glGetActiveUniform");
    pGL->GetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC)drgl__get_proc_address(pGL, "glGetAttachedShaders");
    pGL->GetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)drgl__get_proc_address(pGL, "glGetAttribLocation");
    pGL->GetProgramiv = (PFNGLGETPROGRAMIVPROC)drgl__get_proc_address(pGL, "glGetProgramiv");
    pGL->GetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)drgl__get_proc_address(pGL, "glGetProgramInfoLog");
    pGL->GetShaderiv = (PFNGLGETSHADERIVPROC)drgl__get_proc_address(pGL, "glGetShaderiv");
    pGL->GetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)drgl__get_proc_address(pGL, "glGetShaderInfoLog");
    pGL->GetShaderSource = (PFNGLGETSHADERSOURCEPROC)drgl__get_proc_address(pGL, "glGetShaderSource");
    pGL->GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)drgl__get_proc_address(pGL, "glGetUniformLocation");
    pGL->GetUniformfv = (PFNGLGETUNIFORMFVPROC)drgl__get_proc_address(pGL, "glGetUniformfv");
    pGL->GetUniformiv = (PFNGLGETUNIFORMIVPROC)drgl__get_proc_address(pGL, "glGetUniformiv");
    pGL->GetVertexAttribdv = (PFNGLGETVERTEXATTRIBDVPROC)drgl__get_proc_address(pGL, "glGetVertexAttribdv");
    pGL->GetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC)drgl__get_proc_address(pGL, "glGetVertexAttribfv");
    pGL->GetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)drgl__get_proc_address(pGL, "glGetVertexAttribiv");
    pGL->GetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC)drgl__get_proc_address(pGL, "glGetVertexAttribPointerv");
    pGL->IsProgram = (PFNGLISPROGRAMPROC)drgl__get_proc_address(pGL, "glIsProgram");
    pGL->IsShader = (PFNGLISSHADERPROC)drgl__get_proc_address(pGL, "glIsShader");
    pGL->LinkProgram = (PFNGLLINKPROGRAMPROC)drgl__get_proc_address(pGL, "glLinkProgram");
    pGL->ShaderSource = (PFNGLSHADERSOURCEPROC)drgl__get_proc_address(pGL, "glShaderSource");
    pGL->UseProgram = (PFNGLUSEPROGRAMPROC)drgl__get_proc_address(pGL, "glUseProgram");
    pGL->Uniform1f = (PFNGLUNIFORM1FPROC)drgl__get_proc_address(pGL, "glUniform1f");
    pGL->Uniform2f = (PFNGLUNIFORM2FPROC)drgl__get_proc_address(pGL, "glUniform2f");
    pGL->Uniform3f = (PFNGLUNIFORM3FPROC)drgl__get_proc_address(pGL, "glUniform3f");
    pGL->Uniform4f = (PFNGLUNIFORM4FPROC)drgl__get_proc_address(pGL, "glUniform4f");
    pGL->Uniform1i = (PFNGLUNIFORM1IPROC)drgl__get_proc_address(pGL, "glUniform1i");
    pGL->Uniform2i = (PFNGLUNIFORM2IPROC)drgl__get_proc_address(pGL, "glUniform2i");
    pGL->Uniform3i = (PFNGLUNIFORM3IPROC)drgl__get_proc_address(pGL, "glUniform3i");
    pGL->Uniform4i = (PFNGLUNIFORM4IPROC)drgl__get_proc_address(pGL, "glUniform4i");
    pGL->Uniform1fv = (PFNGLUNIFORM1FVPROC)drgl__get_proc_address(pGL, "glUniform1fv");
    pGL->Uniform2fv = (PFNGLUNIFORM2FVPROC)drgl__get_proc_address(pGL, "glUniform2fv");
    pGL->Uniform3fv = (PFNGLUNIFORM3FVPROC)drgl__get_proc_address(pGL, "glUniform3fv");
    pGL->Uniform4fv = (PFNGLUNIFORM4FVPROC)drgl__get_proc_address(pGL, "glUniform4fv");
    pGL->Uniform1iv = (PFNGLUNIFORM1IVPROC)drgl__get_proc_address(pGL, "glUniform1iv");
    pGL->Uniform2iv = (PFNGLUNIFORM2IVPROC)drgl__get_proc_address(pGL, "glUniform2iv");
    pGL->Uniform3iv = (PFNGLUNIFORM3IVPROC)drgl__get_proc_address(pGL, "glUniform3iv");
    pGL->Uniform4iv = (PFNGLUNIFORM4IVPROC)drgl__get_proc_address(pGL, "glUniform4iv");
    pGL->UniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC)drgl__get_proc_address(pGL, "glUniformMatrix2fv");
    pGL->UniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)drgl__get_proc_address(pGL, "glUniformMatrix3fv");
    pGL->UniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)drgl__get_proc_address(pGL, "glUniformMatrix4fv");
    pGL->ValidateProgram = (PFNGLVALIDATEPROGRAMPROC)drgl__get_proc_address(pGL, "glValidateProgram");
    pGL->VertexAttrib1d = (PFNGLVERTEXATTRIB1DPROC)drgl__get_proc_address(pGL, "glVertexAttrib1d");
    pGL->VertexAttrib1dv = (PFNGLVERTEXATTRIB1DVPROC)drgl__get_proc_address(pGL, "glVertexAttrib1dv");
    pGL->VertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)drgl__get_proc_address(pGL, "glVertexAttrib1f");
    pGL->VertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC)drgl__get_proc_address(pGL, "glVertexAttrib1fv");
    pGL->VertexAttrib1s = (PFNGLVERTEXATTRIB1SPROC)drgl__get_proc_address(pGL, "glVertexAttrib1s");
    pGL->VertexAttrib1sv = (PFNGLVERTEXATTRIB1SVPROC)drgl__get_proc_address(pGL, "glVertexAttrib1sv");
    pGL->VertexAttrib2d = (PFNGLVERTEXATTRIB2DPROC)drgl__get_proc_address(pGL, "glVertexAttrib2d");
    pGL->VertexAttrib2dv = (PFNGLVERTEXATTRIB2DVPROC)drgl__get_proc_address(pGL, "glVertexAttrib2dv");
    pGL->VertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC)drgl__get_proc_address(pGL, "glVertexAttrib2f");
    pGL->VertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC)drgl__get_proc_address(pGL, "glVertexAttrib2fv");
    pGL->VertexAttrib2s = (PFNGLVERTEXATTRIB2SPROC)drgl__get_proc_address(pGL, "glVertexAttrib2s");
    pGL->VertexAttrib2sv = (PFNGLVERTEXATTRIB2SVPROC)drgl__get_proc_address(pGL, "glVertexAttrib2sv");
    pGL->VertexAttrib3d = (PFNGLVERTEXATTRIB3DPROC)drgl__get_proc_address(pGL, "glVertexAttrib3d");
    pGL->VertexAttrib3dv = (PFNGLVERTEXATTRIB3DVPROC)drgl__get_proc_address(pGL, "glVertexAttrib3dv");
    pGL->VertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC)drgl__get_proc_address(pGL, "glVertexAttrib3f");
    pGL->VertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC)drgl__get_proc_address(pGL, "glVertexAttrib3fv");
    pGL->VertexAttrib3s = (PFNGLVERTEXATTRIB3SPROC)drgl__get_proc_address(pGL, "glVertexAttrib3s");
    pGL->VertexAttrib3sv = (PFNGLVERTEXATTRIB3SVPROC)drgl__get_proc_address(pGL, "glVertexAttrib3sv");
    pGL->VertexAttrib4Nbv = (PFNGLVERTEXATTRIB4NBVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4Nbv");
    pGL->VertexAttrib4Niv = (PFNGLVERTEXATTRIB4NIVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4Niv");
    pGL->VertexAttrib4Nsv = (PFNGLVERTEXATTRIB4NSVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4Nsv");
    pGL->VertexAttrib4Nub = (PFNGLVERTEXATTRIB4NUBPROC)drgl__get_proc_address(pGL, "glVertexAttrib4Nub");
    pGL->VertexAttrib4Nubv = (PFNGLVERTEXATTRIB4NUBVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4Nubv");
    pGL->VertexAttrib4Nuiv = (PFNGLVERTEXATTRIB4NUIVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4Nuiv");
    pGL->VertexAttrib4Nusv = (PFNGLVERTEXATTRIB4NUSVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4Nusv");
    pGL->VertexAttrib4bv = (PFNGLVERTEXATTRIB4BVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4bv");
    pGL->VertexAttrib4d = (PFNGLVERTEXATTRIB4DPROC)drgl__get_proc_address(pGL, "glVertexAttrib4d");
    pGL->VertexAttrib4dv = (PFNGLVERTEXATTRIB4DVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4dv");
    pGL->VertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC)drgl__get_proc_address(pGL, "glVertexAttrib4f");
    pGL->VertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4fv");
    pGL->VertexAttrib4iv = (PFNGLVERTEXATTRIB4IVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4iv");
    pGL->VertexAttrib4s = (PFNGLVERTEXATTRIB4SPROC)drgl__get_proc_address(pGL, "glVertexAttrib4s");
    pGL->VertexAttrib4sv = (PFNGLVERTEXATTRIB4SVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4sv");
    pGL->VertexAttrib4ubv = (PFNGLVERTEXATTRIB4UBVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4ubv");
    pGL->VertexAttrib4uiv = (PFNGLVERTEXATTRIB4UIVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4uiv");
    pGL->VertexAttrib4usv = (PFNGLVERTEXATTRIB4USVPROC)drgl__get_proc_address(pGL, "glVertexAttrib4usv");
    pGL->VertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)drgl__get_proc_address(pGL, "glVertexAttribPointer");
#endif

    // OpenGL 2.1
#if DR_GL_VERSION >= 210
    pGL->UniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC)drgl__get_proc_address(pGL, "glUniformMatrix2x3fv");
    pGL->UniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC)drgl__get_proc_address(pGL, "glUniformMatrix3x2fv");
    pGL->UniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC)drgl__get_proc_address(pGL, "glUniformMatrix2x4fv");
    pGL->UniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC)drgl__get_proc_address(pGL, "glUniformMatrix4x2fv");
    pGL->UniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC)drgl__get_proc_address(pGL, "glUniformMatrix3x4fv");
    pGL->UniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC)drgl__get_proc_address(pGL, "glUniformMatrix4x3fv");
#endif


    // Cross-platform extensions
#ifdef DR_GL_ENABLE_EXT_framebuffer_blit
    pGL->BlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)drgl__get_proc_address(pGL, "glBlitFramebufferEXT");
#endif

#ifdef DR_GL_ENABLE_EXT_framebuffer_multisample
    pGL->RenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)drgl__get_proc_address(pGL, "glRenderbufferStorageMultisampleEXT");
#endif

#ifdef DR_GL_ENABLE_EXT_framebuffer_object
    pGL->IsRenderbufferEXT = (PFNGLISRENDERBUFFEREXTPROC)drgl__get_proc_address(pGL, "glIsRenderbufferEXT");
    pGL->BindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC)drgl__get_proc_address(pGL, "glBindRenderbufferEXT");
    pGL->DeleteRenderbuffersEXT = (PFNGLDELETERENDERBUFFERSEXTPROC)drgl__get_proc_address(pGL, "glDeleteRenderbuffersEXT");
    pGL->GenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)drgl__get_proc_address(pGL, "glGenRenderbuffersEXT");
    pGL->RenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)drgl__get_proc_address(pGL, "glRenderbufferStorageEXT");
    pGL->GetRenderbufferParameterivEXT = (PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC)drgl__get_proc_address(pGL, "glGetRenderbufferParameterivEXT");
    pGL->IsFramebufferEXT = (PFNGLISFRAMEBUFFEREXTPROC)drgl__get_proc_address(pGL, "glIsFramebufferEXT");
    pGL->BindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)drgl__get_proc_address(pGL, "glBindFramebufferEXT");
    pGL->DeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSEXTPROC)drgl__get_proc_address(pGL, "glDeleteFramebuffersEXT");
    pGL->GenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)drgl__get_proc_address(pGL, "glGenFramebuffersEXT");
    pGL->CheckFramebufferStatusEXT = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)drgl__get_proc_address(pGL, "glCheckFramebufferStatusEXT");
    pGL->FramebufferTexture1DEXT = (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC)drgl__get_proc_address(pGL, "glFramebufferTexture1DEXT");
    pGL->FramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)drgl__get_proc_address(pGL, "glFramebufferTexture2DEXT");
    pGL->FramebufferTexture3DEXT = (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC)drgl__get_proc_address(pGL, "glFramebufferTexture3DEXT");
    pGL->FramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)drgl__get_proc_address(pGL, "glFramebufferRenderbufferEXT");
    pGL->GetFramebufferAttachmentParameterivEXT = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC)drgl__get_proc_address(pGL, "glGetFramebufferAttachmentParameterivEXT");
    pGL->GenerateMipmapEXT = (PFNGLGENERATEMIPMAPEXTPROC)drgl__get_proc_address(pGL, "glGenerateMipmapEXT");
#endif


    // Platform-specific extensions
#ifdef _WIN32
#ifdef DR_GL_ENABLE_EXT_swap_control
    pGL->SwapIntervalEXT    = (PFNWGLSWAPINTERVALEXTPROC   )drgl__get_proc_address(pGL, "wglSwapIntervalEXT");
    pGL->GetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)drgl__get_proc_address(pGL, "wglGetSwapIntervalEXT");
#endif
#else
    // TODO: GLX extensions.
#endif


    return true;


on_error:
    drgl_uninit(pGL);
    return false;
}

void drgl_uninit(drgl* pGL)
{
    if (pGL == NULL) {
        return;
    }


#ifdef _WIN32
    if (pGL->hDummyHWND) {
        DestroyWindow(pGL->hDummyHWND);
    }

    if (pGL->hRC) {
        pGL->DeleteContext(pGL->hRC);
    }

    if (pGL->hOpenGL32 != NULL) {
        FreeLibrary(pGL->hOpenGL32);
    }
#else
    // TODO: Linux
#endif
}

#endif  //DR_GL_IMPLEMENTATION


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
