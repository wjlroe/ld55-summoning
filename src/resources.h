/* date = April 15th 2024 5:36 pm */

#ifndef RESOURCES_H

typedef struct File_Resource {
	char* filename;
	char* contents;
	int size;
	bool loaded;
} File_Resource;

#define RESOURCES_H

#endif //RESOURCES_H
