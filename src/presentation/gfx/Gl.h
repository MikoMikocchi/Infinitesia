#pragma once

#include <string>

#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

// OpenGL 3.3 Core functions that are not part of GL 1.1. The 1.1 entry points
// (glClear, glViewport, glBindTexture, glDrawElements, ...) are linked from the
// system OpenGL library; everything newer is resolved through SDL.
namespace Infinitesia::Gfx::Gl
{
// Resolves every pointer below. Returns false and fills `error` with the list
// of missing entry points when the driver does not provide the full set.
bool LoadFunctions(std::string *error = nullptr);

extern PFNGLGENBUFFERSPROC GenBuffers;
extern PFNGLBINDBUFFERPROC BindBuffer;
extern PFNGLBUFFERDATAPROC BufferData;
extern PFNGLBUFFERSUBDATAPROC BufferSubData;
extern PFNGLDELETEBUFFERSPROC DeleteBuffers;

extern PFNGLGENVERTEXARRAYSPROC GenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC BindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;

extern PFNGLCREATESHADERPROC CreateShader;
extern PFNGLSHADERSOURCEPROC ShaderSource;
extern PFNGLCOMPILESHADERPROC CompileShader;
extern PFNGLGETSHADERIVPROC GetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
extern PFNGLDELETESHADERPROC DeleteShader;

extern PFNGLCREATEPROGRAMPROC CreateProgram;
extern PFNGLATTACHSHADERPROC AttachShader;
extern PFNGLLINKPROGRAMPROC LinkProgram;
extern PFNGLGETPROGRAMIVPROC GetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
extern PFNGLDELETEPROGRAMPROC DeleteProgram;
extern PFNGLUSEPROGRAMPROC UseProgram;
extern PFNGLBINDATTRIBLOCATIONPROC BindAttribLocation;
extern PFNGLGETATTRIBLOCATIONPROC GetAttribLocation;

extern PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
extern PFNGLUNIFORM1IPROC Uniform1i;
extern PFNGLUNIFORM1FPROC Uniform1f;
extern PFNGLUNIFORM2FPROC Uniform2f;
extern PFNGLUNIFORM3FPROC Uniform3f;
extern PFNGLUNIFORM4FPROC Uniform4f;
extern PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv;

extern PFNGLACTIVETEXTUREPROC ActiveTexture;
extern PFNGLGENERATEMIPMAPPROC GenerateMipmap;

extern PFNGLBLENDFUNCSEPARATEPROC BlendFuncSeparate;
extern PFNGLBLENDEQUATIONPROC BlendEquation;
extern PFNGLBLENDEQUATIONSEPARATEPROC BlendEquationSeparate;
extern PFNGLDRAWBUFFERSPROC DrawBuffers;

extern PFNGLGENFRAMEBUFFERSPROC GenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC BindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers;

extern PFNGLGENRENDERBUFFERSPROC GenRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC BindRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer;
extern PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers;
extern PFNGLBLITFRAMEBUFFERPROC BlitFramebuffer;
} // namespace Infinitesia::Gfx::Gl
