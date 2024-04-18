/* date = April 15th 2024 5:36 pm */

#ifndef RESOURCES_H

#ifndef _WIN32
#include "assets/vertex_shader_glsl.h"
#include "assets/fragment_shader_glsl.h"
#endif

typedef struct File_Resource {
	char* filename;
	const char* contents;
	int length;
	bool loaded;
} File_Resource;

static void load_file_resource(File_Resource* resource, 
							   char* filename,
#ifdef _WIN32
							   int win32_type, 
							   int win32_id
#else
							   char* unix_contents, 
							   int unix_length
#endif
							   
							   ) {
	resource->filename = filename;
#ifdef _WIN32
	HMODULE handle = GetModuleHandle(NULL);
	HRSRC rc = FindResource(handle, MAKEINTRESOURCE(win32_id), MAKEINTRESOURCE(win32_type));
	HGLOBAL rc_data = LoadResource(handle, rc);
	assert(rc_data != NULL);
	DWORD size = SizeofResource(handle, rc);
	assert(size > 0);
	resource->length = size;
	resource->contents = rc_data;
	resource->loaded = true;
#else
	resource->length = unix_length;
	resource->contents = unix_contents;
	resource->loaded = true;
#endif
}

#define MAX_FILE_RESOURCES 1024
static int num_file_resources;
static File_Resource global_file_resources[MAX_FILE_RESOURCES];

typedef struct Resources {
	File_Resource vertex_shader;
	File_Resource fragment_shader;
} Resources;

static Resources resources;

static void init_resources(void) {
	load_file_resource(&resources.vertex_shader, 
					   "vertex_shader.glsl",
#ifdef _WIN32
					   ID_SHADER,
					   VERTEX_SHADER_SOURCE
#else
					   vertex_shader_glsl,
					   vertex_shader_glsl_len
#endif
					   );
	load_file_resource(&resources.fragment_shader,
					   "fragment_shader.glsl",
#ifdef _WIN32
					   ID_SHADER,
					   FRAGMENT_SHADER_SOURCE
#else
					   fragment_shader_glsl,
					   fragment_shader_glsl_len
#endif
					   );
}

#define RESOURCES_H

#endif //RESOURCES_H
