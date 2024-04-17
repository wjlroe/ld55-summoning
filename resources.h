/* date = April 15th 2024 5:36 pm */

#ifndef RESOURCES_H

#ifdef _WIN32
#define ID_FONT 256
#define IM_FELL_FONT_ID 101
#define ID_SHADER 257
#define VERTEX_SHADER_SOURCE 102
#define FRAGMENT_SHADER_SOURCE 103
#endif

typedef struct File_Resource {
	char* filename;
	const char* contents;
	int length;
	
#ifdef _WIN32	
	int resource_type;
	int resource_id;
#endif
} File_Resource;

#ifdef _WIN32
#define RESOURCE(var, res_filename, win32_type, win32_id, unix_contents, unix_length) \
static File_Resource var = { \
.resource_type=win32_type, \
.resource_id=win32_id,     \
.filename=res_filename         \
};                         
#else
#define RESOURCE(var, res_filename, win32_type, win32_id, unix_contents, unix_length) \
static const char* unix_contents; \
static const int unix_length; \
static File_Resource var = { \
.filename=res_filename,          \
.contents=unix_contents,     \
.length=unix_length          \
};                                               
#endif

RESOURCE(vertex_shader_source, "vertex_shader.glsl", ID_SHADER, VERTEX_SHADER_SOURCE, vertex_shader_glsl, vertex_shader_glsl_len)
RESOURCE(fragment_shader_source, "fragment_shader.glsl", ID_SHADER, FRAGMENT_SHADER_SOURCE, fragment_shader_glsl, fragment_shader_glsl_len)

static void load_file_resource(File_Resource* resource) {
#ifdef _WIN32
	HMODULE handle = GetModuleHandle(NULL);
	HRSRC rc = FindResource(handle, MAKEINTRESOURCE(resource->resource_id), MAKEINTRESOURCE(resource->resource_type));
	HGLOBAL rc_data = LoadResource(handle, rc);
	assert(rc_data != NULL);
	DWORD size = SizeofResource(handle, rc);
	assert(size > 0);
	resource->length = size;
	resource->contents = rc_data;
#endif
}

#define RESOURCES_H

#endif //RESOURCES_H
