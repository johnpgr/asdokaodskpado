#include "platform/opengl_loader.h"

// Define extension function pointers
GL_PFNGLGENBUFFERSPROC gl_GenBuffers = nullptr;
GL_PFNGLDELETEBUFFERSPROC gl_DeleteBuffers = nullptr;
GL_PFNGLBINDBUFFERPROC gl_BindBuffer = nullptr;
GL_PFNGLBUFFERDATAPROC gl_BufferData = nullptr;
GL_PFNGLBUFFERSUBDATAPROC gl_BufferSubData = nullptr;

GL_PFNGLGENVERTEXARRAYSPROC gl_GenVertexArrays = nullptr;
GL_PFNGLDELETEVERTEXARRAYSPROC gl_DeleteVertexArrays = nullptr;
GL_PFNGLBINDVERTEXARRAYPROC gl_BindVertexArray = nullptr;
GL_PFNGLVERTEXATTRIBPOINTERPROC gl_VertexAttribPointer = nullptr;
GL_PFNGLENABLEVERTEXATTRIBARRAYPROC gl_EnableVertexAttribArray = nullptr;
GL_PFNGLDISABLEVERTEXATTRIBARRAYPROC gl_DisableVertexAttribArray = nullptr;

GL_PFNGLCREATESHADERPROC gl_CreateShader = nullptr;
GL_PFNGLDELETESHADERPROC gl_DeleteShader = nullptr;
GL_PFNGLSHADERSOURCEPROC gl_ShaderSource = nullptr;
GL_PFNGLCOMPILESHADERPROC gl_CompileShader = nullptr;
GL_PFNGLGETSHADERIVPROC gl_GetShaderiv = nullptr;
GL_PFNGLGETSHADERINFOLOGPROC gl_GetShaderInfoLog = nullptr;

GL_PFNGLCREATEPROGRAMPROC gl_CreateProgram = nullptr;
GL_PFNGLDELETEPROGRAMPROC gl_DeleteProgram = nullptr;
GL_PFNGLATTACHSHADERPROC gl_AttachShader = nullptr;
GL_PFNGLDETACHSHADERPROC gl_DetachShader = nullptr;
GL_PFNGLLINKPROGRAMPROC gl_LinkProgram = nullptr;
GL_PFNGLUSEPROGRAMPROC gl_UseProgram = nullptr;
GL_PFNGLGETPROGRAMIVPROC gl_GetProgramiv = nullptr;
GL_PFNGLGETPROGRAMINFOLOGPROC gl_GetProgramInfoLog = nullptr;
GL_PFNGLGETUNIFORMLOCATIONPROC gl_GetUniformLocation = nullptr;
GL_PFNGLUNIFORM1IPROC gl_Uniform1i = nullptr;
GL_PFNGLUNIFORM1FPROC gl_Uniform1f = nullptr;
GL_PFNGLUNIFORM2FPROC gl_Uniform2f = nullptr;
GL_PFNGLUNIFORM3FPROC gl_Uniform3f = nullptr;
GL_PFNGLUNIFORM4FPROC gl_Uniform4f = nullptr;

GL_PFNGLACTIVETEXTUREPROC gl_ActiveTexture = nullptr;

GL_PFNGLGENFRAMEBUFFERSPROC gl_GenFramebuffers = nullptr;
GL_PFNGLDELETEFRAMEBUFFERSPROC gl_DeleteFramebuffers = nullptr;
GL_PFNGLBINDFRAMEBUFFERPROC gl_BindFramebuffer = nullptr;
GL_PFNGLFRAMEBUFFERTEXTURE2DPROC gl_FramebufferTexture2D = nullptr;
GL_PFNGLCHECKFRAMEBUFFERSTATUSPROC gl_CheckFramebufferStatus = nullptr;

b32 gl_load_functions(void* (*get_proc_address)(const char*)) {
    #define LOAD_GL(var, name) \
        var = (decltype(var))get_proc_address(name); \
        if (!var) return false;

    // Buffer functions (GL 1.5+)
    LOAD_GL(gl_GenBuffers, "glGenBuffers");
    LOAD_GL(gl_DeleteBuffers, "glDeleteBuffers");
    LOAD_GL(gl_BindBuffer, "glBindBuffer");
    LOAD_GL(gl_BufferData, "glBufferData");
    LOAD_GL(gl_BufferSubData, "glBufferSubData");

    // VAO functions (GL 3.0+)
    LOAD_GL(gl_GenVertexArrays, "glGenVertexArrays");
    LOAD_GL(gl_DeleteVertexArrays, "glDeleteVertexArrays");
    LOAD_GL(gl_BindVertexArray, "glBindVertexArray");
    LOAD_GL(gl_VertexAttribPointer, "glVertexAttribPointer");
    LOAD_GL(gl_EnableVertexAttribArray, "glEnableVertexAttribArray");
    LOAD_GL(gl_DisableVertexAttribArray, "glDisableVertexAttribArray");

    // Shader functions (GL 2.0+)
    LOAD_GL(gl_CreateShader, "glCreateShader");
    LOAD_GL(gl_DeleteShader, "glDeleteShader");
    LOAD_GL(gl_ShaderSource, "glShaderSource");
    LOAD_GL(gl_CompileShader, "glCompileShader");
    LOAD_GL(gl_GetShaderiv, "glGetShaderiv");
    LOAD_GL(gl_GetShaderInfoLog, "glGetShaderInfoLog");

    // Program functions (GL 2.0+)
    LOAD_GL(gl_CreateProgram, "glCreateProgram");
    LOAD_GL(gl_DeleteProgram, "glDeleteProgram");
    LOAD_GL(gl_AttachShader, "glAttachShader");
    LOAD_GL(gl_DetachShader, "glDetachShader");
    LOAD_GL(gl_LinkProgram, "glLinkProgram");
    LOAD_GL(gl_UseProgram, "glUseProgram");
    LOAD_GL(gl_GetProgramiv, "glGetProgramiv");
    LOAD_GL(gl_GetProgramInfoLog, "glGetProgramInfoLog");
    LOAD_GL(gl_GetUniformLocation, "glGetUniformLocation");
    LOAD_GL(gl_Uniform1i, "glUniform1i");
    LOAD_GL(gl_Uniform1f, "glUniform1f");
    LOAD_GL(gl_Uniform2f, "glUniform2f");
    LOAD_GL(gl_Uniform3f, "glUniform3f");
    LOAD_GL(gl_Uniform4f, "glUniform4f");

    // Texture functions (GL 1.3+)
    LOAD_GL(gl_ActiveTexture, "glActiveTexture");

    // Framebuffer functions (GL 3.0+)
    LOAD_GL(gl_GenFramebuffers, "glGenFramebuffers");
    LOAD_GL(gl_DeleteFramebuffers, "glDeleteFramebuffers");
    LOAD_GL(gl_BindFramebuffer, "glBindFramebuffer");
    LOAD_GL(gl_FramebufferTexture2D, "glFramebufferTexture2D");
    LOAD_GL(gl_CheckFramebufferStatus, "glCheckFramebufferStatus");

    #undef LOAD_GL

    return true;
}
