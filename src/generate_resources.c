#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "resource_ids.h"

typedef struct Resource_Line Resource_Line;
typedef struct Resource_Line {
	char* name_id;
	char* type_id;
	char* filepath;
	Resource_Line* next;
} Resource_Line;

static void write_resource_to_output(FILE* output, Resource_Line* line) {
	if (line->type_id == "ICON") { return; } // skip the icon, Windows does this automatically
	
	
}

typedef struct File_Contents {
	int size;
	char* contents;
	char* filename;
} File_Contents;

//FIXME: we don't need to read the files in on Windows, only Unix

static void load_file_contents(File_Contents* file) {
#ifdef _WIN32
	struct __stat64 file_stats;
	_stat64(file->filename, &file_stats);
#else
	struct stat file_stats;
	stat(file->filename, &file_stats);
#endif
	
	file->size = file_stats.st_size;
	file->contents = malloc(file->size);
	fread(file->contents, 1, file->size, fopen(file->filename, "rb"));
}

typedef enum Resource_Token {
	R_UNKNOWN,
	R_ID,
	R_TYPE_ID,
	R_FILEPATH,
	R_SKIP,
} Resource_Token;

static char* buffer_to_string(char* buffer, int* buffer_size) {
	char* str = malloc(sizeof(char)*(*buffer_size+1));
	strncpy(str, buffer, *buffer_size);
	str[*buffer_size] = 0;
	*buffer_size = 0;
	return str;
}

static Resource_Line* consume_buffer(Resource_Line* line, Resource_Token current, char* buffer, int* buffer_i, int* num_resources) {
	switch (current) {
		case R_ID: {
			line->name_id = buffer_to_string(buffer, buffer_i);
			printf("name_id: '%s'\n", line->name_id);
		} break;
		case R_TYPE_ID: {
			line->type_id = buffer_to_string(buffer, buffer_i);
			printf("type_id: '%s'\n", line->type_id);
		} break;
		case R_FILEPATH: {
			line->filepath = buffer_to_string(buffer, buffer_i);
			printf("filepath: '%s'\n", line->filepath);
			Resource_Line* next = malloc(sizeof(Resource_Line));
			line->next = next;
			*num_resources++;
			return next;
		} break;
	}
	return line;
}

static Resource_Line* parse_resources_file(File_Contents* resources_file, int* num_resources) {
	char buffer[1024] = {0};
	int buffer_i = 0;
	*num_resources = 0;
	Resource_Line* first_line = malloc(sizeof(Resource_Line));
	Resource_Line* line = first_line;
	Resource_Token current = R_UNKNOWN;
	int i = 0;
	while (i < resources_file->size) {
		char c = resources_file->contents[i++];
		if (c == '\r') { continue; }
		if (current == R_UNKNOWN) {
			if (c == '#') {
				current = R_SKIP;
			} else if (isalpha(c)) {
				current = R_ID;
			}
		}
		if (((current != R_SKIP) && (c == ' ')) || (c == '\n')) {
			line = consume_buffer(line, current, buffer, &buffer_i, num_resources);
			if (c == '\n') {
				current = R_UNKNOWN;
			} else if ((current > R_UNKNOWN) && ((current+1) != R_SKIP)) {
				current++;
			}
		} else if (current == R_SKIP) {
			continue;
		} else {
			buffer[buffer_i++] = c;
		}
	}
	if (buffer_i > 0) {
		line = consume_buffer(line, current, buffer, &buffer_i, num_resources);
	}
	return first_line;
}

int main(int argc, char** argv) {
	File_Contents resources_file = {.filename="../resources.rc"};
	load_file_contents(&resources_file);
	int num_resources = 0;
	Resource_Line *line = parse_resources_file(&resources_file, &num_resources);
	FILE* output = fopen("generated_resources.h", "r");
	fprintf(output, "#define ID_BASE 100\n");
	fprintf(output, "#define NUM_RESOURCES %d\n", num_resources);
	fclose(output);
	return 0;
}