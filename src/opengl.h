/* date = April 17th 2024 0:52 pm */
// vim: tabstop=4 shiftwidth=4 noexpandtab

#ifndef OPENGL_H
#define OPENGL_H

#ifdef _WIN32
#define GLDECL APIENTRY
#else
#define GLDECL
#endif

#if defined(_WIN64)
typedef signed   long long int khronos_ssize_t;
typedef unsigned long long int khronos_usize_t;
#else
typedef signed   long  int     khronos_ssize_t;
typedef unsigned long  int     khronos_usize_t;
#endif

typedef void GLvoid;
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef int GLint;
typedef char GLchar;
typedef uint8_t GLubyte;
typedef float GLfloat;
typedef float GLclampf;
typedef khronos_ssize_t GLsizeiptr;
typedef unsigned int GLbitfield;
typedef unsigned char GLboolean;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_ZERO 0
#define GL_ONE 1
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_RED 0x1903
#define GL_GREEN 0x1904
#define GL_BLUE 0x1905
#define GL_ALPHA 0x1906
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_TEXTURE0 0x84C0
#define GL_SAMPLER_2D 0x8B5E
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA_SATURATE 0x0308
#define GL_BLEND_DST 0x0BE0
#define GL_BLEND_SRC 0x0BE1
#define GL_BLEND 0x0BE2
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_OUT_OF_MEMORY 0x0505

#define GL_FUNCS \
GLE(void,   glActiveTexture,    GLenum texture) \
GLE(void,   glAttachShader,     GLuint program, GLuint shader) \
GLE(void,   glBindBuffer,       GLenum target, GLuint buffer) \
GLE(void,   glBindTexture,      GLenum target, GLuint texture) \
GLE(void,   glBindVertexArray,  GLuint array) \
GLE(void,   glBlendFunc,        GLenum sfactor, GLenum dfactor) \
GLE(void,   glBufferData,       GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) \
GLE(void,   glClear,            GLbitfield mask) \
GLE(void,   glClearColor,       GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) \
GLE(void,   glCompileShader,    GLuint shader) \
GLE(GLuint, glCreateProgram,    void) \
GLE(GLuint, glCreateShader,     GLenum type) \
GLE(void,   glDeleteShader,     GLuint shader) \
GLE(void,   glDisable,          GLenum cap) \
GLE(void,   glDisableVertexAttribArray, GLuint index) \
GLE(void,   glDrawElements,           GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) \
GLE(void,   glDrawElementsBaseVertex, GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLint basevertex) \
GLE(void,   glEnable,           GLenum cap) \
GLE(void,   glEnableVertexAttribArray, GLuint index) \
GLE(void,   glGenBuffers,       GLsizei n, GLuint* buffers) \
GLE(void,   glGenTextures,      GLsizei n, GLuint* textures) \
GLE(void,   glGetActiveAttrib,  GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) \
GLE(void,   glGetActiveUniform, GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) \
GLE(GLint,  glGetAttribLocation, GLuint program, const GLchar* name) \
GLE(GLenum, glGetError) \
GLE(void,   glGetProgramInfoLog, GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) \
GLE(void,   glGetProgramiv,     GLuint program, GLenum pname, GLint* params) \
GLE(void,   glGetShaderInfoLog, GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) \
GLE(void,   glGetShaderiv,      GLuint shader, GLenum pname, GLint *params) \
GLE(const GLubyte*, glGetString, GLenum name)                             \
GLE(GLint,  glGetUniformLocation, GLuint program, const GLchar* name) \
GLE(void,   glGenVertexArrays,  GLsizei n, GLuint* arrays) \
GLE(void,   glLinkProgram,      GLuint program) \
GLE(void,   glShaderSource,     GLuint shader, GLsizei count, const GLchar** string, const GLint* length) \
GLE(void,   glTexImage2D,       GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data) \
GLE(void,   glTexParameteri,    GLenum target, GLenum pname, GLint param) \
GLE(void,   glUniform1f,        GLint location, GLfloat v0) \
GLE(void,   glUniform1i,        GLint location, GLint v0) \
GLE(void,   glUniform1iv,       GLint location, GLsizei count, const GLint* value) \
GLE(void,   glUniform1uiv,      GLint location, GLsizei count, const GLuint* value) \
GLE(void,   glUniform2fv,       GLint location, GLsizei count, const GLfloat* value) \
GLE(void,   glUniform3fv,       GLint location, GLsizei count, const GLfloat* value) \
GLE(void,   glUniform4fv,       GLint location, GLsizei count, const GLfloat* value) \
GLE(void,   glUniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
GLE(void,   glUseProgram,       GLuint program) \
GLE(void,   glValidateProgram,      GLuint program) \
GLE(void,   glVertexAttribIPointer, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) \
GLE(void,   glVertexAttribPointer,  GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer) \
GLE(void,   glViewport,         GLint x, GLint y, GLsizei width, GLsizei height) \


#define GLE(ret, name, ...) typedef ret GLDECL name##proc(__VA_ARGS__); static name##proc * name;
GL_FUNCS
#undef GLE

static void* gl_get_proc_address(const char* proc) {
	return SDL_GL_GetProcAddress(proc);
}

// TODO: how to have this resolve to a wrapping function that'll call catch_gl_error() after everything?

static void load_gl_funcs(void) {
#define GLE(ret, name, ...) name = (name##proc *)gl_get_proc_address(#name);
	GL_FUNCS
#undef GLE
}

#endif //OPENGL_H
