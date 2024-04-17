/* date = April 17th 2024 0:52 pm */

#ifndef OPENGL_H
#define OPENGL_H

#ifdef _WIN32
#define GLDECL APIENTRY
#else
#define GLDECL
#endif

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef int GLint;
typedef char GLchar;
typedef uint8_t GLubyte;

#define GL_FALSE 0
#define GL_TRUE 1
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

#define GL_FUNCS \
GLE(void,   glAttachShader,     GLuint program, GLuint shader) \
GLE(void,   glCompileShader,    GLuint shader) \
GLE(GLuint, glCreateProgram,    void) \
GLE(GLuint, glCreateShader,     GLenum type) \
GLE(void,   glDeleteShader,     GLuint shader) \
GLE(void,   glGetProgramInfoLog, GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) \
GLE(void,   glGetProgramiv,     GLuint program, GLenum pname, GLint* params) \
GLE(void,   glGetShaderInfoLog, GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) \
GLE(void,   glGetShaderiv,      GLuint shader, GLenum pname, GLint *params) \
GLE(const GLubyte*, glGetString, GLenum name)                             \
GLE(void,   glLinkProgram,      GLuint program) \
GLE(void,   glShaderSource,     GLuint shader, GLsizei count, const GLchar** string, const GLint* length) \
GLE(void,   glValidateProgram,  GLuint program) \


#define GLE(ret, name, ...) typedef ret GLDECL name##proc(__VA_ARGS__); static name##proc * name;
GL_FUNCS
#undef GLE

static void load_gl_funcs(void) {
#define GLE(ret, name, ...) name = (name##proc *)SDL_GL_GetProcAddress(#name);
	GL_FUNCS
#undef GLE
}

#endif //OPENGL_H
