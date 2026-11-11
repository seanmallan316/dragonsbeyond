// gles.h - the OpenGL ES 3.0 calls the port uses, looked up at start-up through SDL (the same on Android and on a PC).
#ifndef KK_GLES_H
#define KK_GLES_H
#include <stddef.h>
#include <stdint.h>

typedef unsigned int GLenum; typedef unsigned char GLboolean; typedef unsigned int GLbitfield; typedef int GLint; typedef int GLsizei;
typedef unsigned int GLuint; typedef float GLfloat; typedef char GLchar; typedef ptrdiff_t GLsizeiptr; typedef ptrdiff_t GLintptr; typedef unsigned char GLubyte;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_FUNC_ADD 0x8006
#define GL_BLEND 0x0BE2
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_SCISSOR_TEST 0x0C11
#define GL_BACK 0x0405
#define GL_CCW 0x0901
#define GL_LESS 0x0201
#define GL_LEQUAL 0x0203
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STREAM_DRAW 0x88E0
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_NONE 0
#define GL_MAX_SAMPLES 0x8D57
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_EXTENSIONS 0x1F03
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01

#define KK_GL_FUNCS \
    X(void, glClear, (GLbitfield)) X(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) X(void, glClearDepthf, (GLfloat)) \
    X(void, glEnable, (GLenum)) X(void, glDisable, (GLenum)) X(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum)) \
    X(void, glBlendEquation, (GLenum)) X(void, glDepthFunc, (GLenum)) X(void, glDepthMask, (GLboolean)) X(void, glColorMask, (GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(void, glCullFace, (GLenum)) X(void, glFrontFace, (GLenum)) X(void, glViewport, (GLint, GLint, GLsizei, GLsizei)) X(void, glScissor, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, glGenTextures, (GLsizei, GLuint *)) X(void, glDeleteTextures, (GLsizei, const GLuint *)) X(void, glBindTexture, (GLenum, GLuint)) \
    X(void, glActiveTexture, (GLenum)) X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    X(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) X(void, glTexParameteri, (GLenum, GLenum, GLint)) \
    X(void, glTexParameterf, (GLenum, GLenum, GLfloat)) X(void, glGenerateMipmap, (GLenum)) X(void, glPixelStorei, (GLenum, GLint)) \
    X(void, glCopyTexSubImage2D, (GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei)) \
    X(void, glGenBuffers, (GLsizei, GLuint *)) X(void, glDeleteBuffers, (GLsizei, const GLuint *)) X(void, glBindBuffer, (GLenum, GLuint)) \
    X(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum)) X(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void *)) \
    X(void, glGenVertexArrays, (GLsizei, GLuint *)) X(void, glBindVertexArray, (GLuint)) X(void, glDeleteVertexArrays, (GLsizei, const GLuint *)) \
    X(void, glEnableVertexAttribArray, (GLuint)) X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
    X(void, glDrawArrays, (GLenum, GLint, GLsizei)) \
    X(GLuint, glCreateShader, (GLenum)) X(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) X(void, glCompileShader, (GLuint)) \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint *)) X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) X(GLuint, glCreateProgram, (void)) \
    X(void, glAttachShader, (GLuint, GLuint)) X(void, glBindAttribLocation, (GLuint, GLuint, const GLchar *)) X(void, glLinkProgram, (GLuint)) \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint *)) X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) X(void, glUseProgram, (GLuint)) \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar *)) X(void, glUniform1i, (GLint, GLint)) X(void, glUniform1f, (GLint, GLfloat)) X(void, glUniform4fv, (GLint, GLsizei, const GLfloat *)) \
    X(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat *)) \
    X(void, glGenFramebuffers, (GLsizei, GLuint *)) X(void, glBindFramebuffer, (GLenum, GLuint)) X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(void, glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint)) X(GLenum, glCheckFramebufferStatus, (GLenum)) X(void, glDeleteFramebuffers, (GLsizei, const GLuint *)) \
    X(void, glGenRenderbuffers, (GLsizei, GLuint *)) X(void, glBindRenderbuffer, (GLenum, GLuint)) X(void, glDeleteRenderbuffers, (GLsizei, const GLuint *)) \
    X(void, glRenderbufferStorageMultisample, (GLenum, GLsizei, GLenum, GLsizei, GLsizei)) \
    X(void, glBlitFramebuffer, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
    X(void, glDrawBuffers, (GLsizei, const GLenum *)) X(void, glReadBuffer, (GLenum)) X(void, glGetIntegerv, (GLenum, GLint *)) \
    X(const GLubyte *, glGetString, (GLenum)) X(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) X(GLenum, glGetError, (void)) \
    X(void, glFinish, (void))

#define X(ret, name, args) typedef ret (*PFN_##name) args; extern PFN_##name kk_##name;
KK_GL_FUNCS
#undef X
#define X(ret, name, args)
int kk_gl_load(void *(*getproc)(const char *));   /* 0 if a call is missing */
#endif

#ifndef KK_GLES_NO_RENAME
#define glClear kk_glClear
#define glClearColor kk_glClearColor
#define glClearDepthf kk_glClearDepthf
#define glEnable kk_glEnable
#define glDisable kk_glDisable
#define glBlendFuncSeparate kk_glBlendFuncSeparate
#define glBlendEquation kk_glBlendEquation
#define glDepthFunc kk_glDepthFunc
#define glDepthMask kk_glDepthMask
#define glColorMask kk_glColorMask
#define glCullFace kk_glCullFace
#define glFrontFace kk_glFrontFace
#define glViewport kk_glViewport
#define glScissor kk_glScissor
#define glGenTextures kk_glGenTextures
#define glDeleteTextures kk_glDeleteTextures
#define glBindTexture kk_glBindTexture
#define glActiveTexture kk_glActiveTexture
#define glTexImage2D kk_glTexImage2D
#define glTexSubImage2D kk_glTexSubImage2D
#define glTexParameteri kk_glTexParameteri
#define glTexParameterf kk_glTexParameterf
#define glGenerateMipmap kk_glGenerateMipmap
#define glPixelStorei kk_glPixelStorei
#define glCopyTexSubImage2D kk_glCopyTexSubImage2D
#define glGenBuffers kk_glGenBuffers
#define glDeleteBuffers kk_glDeleteBuffers
#define glBindBuffer kk_glBindBuffer
#define glBufferData kk_glBufferData
#define glBufferSubData kk_glBufferSubData
#define glGenVertexArrays kk_glGenVertexArrays
#define glBindVertexArray kk_glBindVertexArray
#define glDeleteVertexArrays kk_glDeleteVertexArrays
#define glEnableVertexAttribArray kk_glEnableVertexAttribArray
#define glVertexAttribPointer kk_glVertexAttribPointer
#define glDrawArrays kk_glDrawArrays
#define glCreateShader kk_glCreateShader
#define glShaderSource kk_glShaderSource
#define glCompileShader kk_glCompileShader
#define glGetShaderiv kk_glGetShaderiv
#define glGetShaderInfoLog kk_glGetShaderInfoLog
#define glCreateProgram kk_glCreateProgram
#define glAttachShader kk_glAttachShader
#define glBindAttribLocation kk_glBindAttribLocation
#define glLinkProgram kk_glLinkProgram
#define glGetProgramiv kk_glGetProgramiv
#define glGetProgramInfoLog kk_glGetProgramInfoLog
#define glUseProgram kk_glUseProgram
#define glGetUniformLocation kk_glGetUniformLocation
#define glUniform1i kk_glUniform1i
#define glUniform1f kk_glUniform1f
#define glUniform4fv kk_glUniform4fv
#define glUniformMatrix4fv kk_glUniformMatrix4fv
#define glGenFramebuffers kk_glGenFramebuffers
#define glBindFramebuffer kk_glBindFramebuffer
#define glFramebufferTexture2D kk_glFramebufferTexture2D
#define glFramebufferRenderbuffer kk_glFramebufferRenderbuffer
#define glCheckFramebufferStatus kk_glCheckFramebufferStatus
#define glDeleteFramebuffers kk_glDeleteFramebuffers
#define glGenRenderbuffers kk_glGenRenderbuffers
#define glBindRenderbuffer kk_glBindRenderbuffer
#define glDeleteRenderbuffers kk_glDeleteRenderbuffers
#define glRenderbufferStorageMultisample kk_glRenderbufferStorageMultisample
#define glBlitFramebuffer kk_glBlitFramebuffer
#define glDrawBuffers kk_glDrawBuffers
#define glReadBuffer kk_glReadBuffer
#define glGetIntegerv kk_glGetIntegerv
#define glGetString kk_glGetString
#define glReadPixels kk_glReadPixels
#define glGetError kk_glGetError
#define glFinish kk_glFinish
#endif
