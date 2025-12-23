#pragma once

#include "lib/def.h"

// Include system OpenGL headers - they define types and some functions
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <gl/gl.h>
#include <windows.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

// On Windows, we need to manually load extension functions
// On Linux with GL_GLEXT_PROTOTYPES, functions are available but we still load
// them to support older drivers

// Additional OpenGL types not always defined
#ifndef GL_VERSION_2_0
typedef char GLchar;
#endif

#ifndef GL_VERSION_1_5
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
#endif

// OpenGL constants for features beyond GL 1.1
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#endif

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef GL_R8
#define GL_R8 0x8229
#endif

#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

// On Windows, we need our own function pointers
// On Linux, these are already defined in glext.h but we use different names
// to avoid conflicts and for consistency

// Use GL_ prefix to avoid conflicts with system headers
typedef void (*GL_PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void (*GL_PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
typedef void (*GL_PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (*GL_PFNGLBUFFERDATAPROC)(
    GLenum target,
    GLsizeiptr size,
    const void* data,
    GLenum usage
);
typedef void (*GL_PFNGLBUFFERSUBDATAPROC)(
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    const void* data
);

typedef void (*GL_PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void (*GL_PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void (*GL_PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (*GL_PFNGLVERTEXATTRIBPOINTERPROC)(
    GLuint index,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const void* pointer
);
typedef void (*GL_PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (*GL_PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);

typedef GLuint (*GL_PFNGLCREATESHADERPROC)(GLenum type);
typedef void (*GL_PFNGLDELETESHADERPROC)(GLuint shader);
typedef void (*GL_PFNGLSHADERSOURCEPROC)(
    GLuint shader,
    GLsizei count,
    const GLchar* const* string,
    const GLint* length
);
typedef void (*GL_PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (*GL_PFNGLGETSHADERIVPROC)(
    GLuint shader,
    GLenum pname,
    GLint* params
);
typedef void (*GL_PFNGLGETSHADERINFOLOGPROC)(
    GLuint shader,
    GLsizei maxLength,
    GLsizei* length,
    GLchar* infoLog
);

typedef GLuint (*GL_PFNGLCREATEPROGRAMPROC)(void);
typedef void (*GL_PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (*GL_PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (*GL_PFNGLDETACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (*GL_PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (*GL_PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (*GL_PFNGLGETPROGRAMIVPROC)(
    GLuint program,
    GLenum pname,
    GLint* params
);
typedef void (*GL_PFNGLGETPROGRAMINFOLOGPROC)(
    GLuint program,
    GLsizei maxLength,
    GLsizei* length,
    GLchar* infoLog
);
typedef GLint (*GL_PFNGLGETUNIFORMLOCATIONPROC)(
    GLuint program,
    const GLchar* name
);
typedef void (*GL_PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (*GL_PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (*GL_PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (*GL_PFNGLUNIFORM3FPROC)(
    GLint location,
    GLfloat v0,
    GLfloat v1,
    GLfloat v2
);
typedef void (*GL_PFNGLUNIFORM4FPROC)(
    GLint location,
    GLfloat v0,
    GLfloat v1,
    GLfloat v2,
    GLfloat v3
);

typedef void (*GL_PFNGLACTIVETEXTUREPROC)(GLenum texture);

typedef void (*GL_PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef void (*GL_PFNGLDELETEFRAMEBUFFERSPROC)(
    GLsizei n,
    const GLuint* framebuffers
);
typedef void (*GL_PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (*GL_PFNGLFRAMEBUFFERTEXTURE2DPROC)(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level
);
typedef GLenum (*GL_PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);

// Extension function pointers with gl_ prefix to avoid conflicts
extern GL_PFNGLGENBUFFERSPROC gl_GenBuffers;
extern GL_PFNGLDELETEBUFFERSPROC gl_DeleteBuffers;
extern GL_PFNGLBINDBUFFERPROC gl_BindBuffer;
extern GL_PFNGLBUFFERDATAPROC gl_BufferData;
extern GL_PFNGLBUFFERSUBDATAPROC gl_BufferSubData;

extern GL_PFNGLGENVERTEXARRAYSPROC gl_GenVertexArrays;
extern GL_PFNGLDELETEVERTEXARRAYSPROC gl_DeleteVertexArrays;
extern GL_PFNGLBINDVERTEXARRAYPROC gl_BindVertexArray;
extern GL_PFNGLVERTEXATTRIBPOINTERPROC gl_VertexAttribPointer;
extern GL_PFNGLENABLEVERTEXATTRIBARRAYPROC gl_EnableVertexAttribArray;
extern GL_PFNGLDISABLEVERTEXATTRIBARRAYPROC gl_DisableVertexAttribArray;

extern GL_PFNGLCREATESHADERPROC gl_CreateShader;
extern GL_PFNGLDELETESHADERPROC gl_DeleteShader;
extern GL_PFNGLSHADERSOURCEPROC gl_ShaderSource;
extern GL_PFNGLCOMPILESHADERPROC gl_CompileShader;
extern GL_PFNGLGETSHADERIVPROC gl_GetShaderiv;
extern GL_PFNGLGETSHADERINFOLOGPROC gl_GetShaderInfoLog;

extern GL_PFNGLCREATEPROGRAMPROC gl_CreateProgram;
extern GL_PFNGLDELETEPROGRAMPROC gl_DeleteProgram;
extern GL_PFNGLATTACHSHADERPROC gl_AttachShader;
extern GL_PFNGLDETACHSHADERPROC gl_DetachShader;
extern GL_PFNGLLINKPROGRAMPROC gl_LinkProgram;
extern GL_PFNGLUSEPROGRAMPROC gl_UseProgram;
extern GL_PFNGLGETPROGRAMIVPROC gl_GetProgramiv;
extern GL_PFNGLGETPROGRAMINFOLOGPROC gl_GetProgramInfoLog;
extern GL_PFNGLGETUNIFORMLOCATIONPROC gl_GetUniformLocation;
extern GL_PFNGLUNIFORM1IPROC gl_Uniform1i;
extern GL_PFNGLUNIFORM1FPROC gl_Uniform1f;
extern GL_PFNGLUNIFORM2FPROC gl_Uniform2f;
extern GL_PFNGLUNIFORM3FPROC gl_Uniform3f;
extern GL_PFNGLUNIFORM4FPROC gl_Uniform4f;

extern GL_PFNGLACTIVETEXTUREPROC gl_ActiveTexture;

extern GL_PFNGLGENFRAMEBUFFERSPROC gl_GenFramebuffers;
extern GL_PFNGLDELETEFRAMEBUFFERSPROC gl_DeleteFramebuffers;
extern GL_PFNGLBINDFRAMEBUFFERPROC gl_BindFramebuffer;
extern GL_PFNGLFRAMEBUFFERTEXTURE2DPROC gl_FramebufferTexture2D;
extern GL_PFNGLCHECKFRAMEBUFFERSTATUSPROC gl_CheckFramebufferStatus;

// Load extension functions
b32 gl_load_functions(void* (*get_proc_address)(const char*));
