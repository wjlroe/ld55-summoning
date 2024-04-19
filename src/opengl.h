/* date = April 17th 2024 0:52 pm */

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

#define GL_FUNCS \
GLE(void,   glAttachShader,     GLuint program, GLuint shader) \
GLE(void,   glBindBuffer,       GLenum target, GLuint buffer) \
GLE(void,   glBufferData,       GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) \
GLE(void,   glClear,            GLbitfield mask) \
GLE(void,   glClearColor,       GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) \
GLE(void,   glCompileShader,    GLuint shader) \
GLE(GLuint, glCreateProgram,    void) \
GLE(GLuint, glCreateShader,     GLenum type) \
GLE(void,   glDeleteShader,     GLuint shader) \
GLE(void,   glDisableVertexAttribArray, GLuint index) \
GLE(void,   glDrawElements,     GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) \
GLE(void,   glEnableVertexAttribArray, GLuint index) \
GLE(void,   glGenBuffers,       GLsizei n, GLuint* buffers) \
GLE(void,   glGetActiveAttrib,  GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) \
GLE(void,   glGetActiveUniform, GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) \
GLE(GLint,  glGetAttribLocation, GLuint program, const GLchar* name) \
GLE(void,   glGetProgramInfoLog, GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) \
GLE(void,   glGetProgramiv,     GLuint program, GLenum pname, GLint* params) \
GLE(void,   glGetShaderInfoLog, GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) \
GLE(void,   glGetShaderiv,      GLuint shader, GLenum pname, GLint *params) \
GLE(const GLubyte*, glGetString, GLenum name)                             \
GLE(GLint,  glGetUniformLocation, GLuint program, const GLchar* name) \
GLE(void,   glLinkProgram,      GLuint program) \
GLE(void,   glShaderSource,     GLuint shader, GLsizei count, const GLchar** string, const GLint* length) \
GLE(void,   glUniform1uiv,      GLint location, GLsizei count, const GLuint* value) \
GLE(void,   glUniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
GLE(void,   glUseProgram,       GLuint program) \
GLE(void,   glValidateProgram,      GLuint program) \
GLE(void,   glVertexAttribIPointer, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) \
GLE(void,   glVertexAttribPointer,  GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer) \


#define GLE(ret, name, ...) typedef ret GLDECL name##proc(__VA_ARGS__); static name##proc * name;
GL_FUNCS
#undef GLE

static void load_gl_funcs(void) {
#define GLE(ret, name, ...) name = (name##proc *)SDL_GL_GetProcAddress(#name);
	GL_FUNCS
#undef GLE
}

#endif //OPENGL_H
