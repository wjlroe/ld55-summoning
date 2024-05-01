/* date = April 15th 2024 5:36 pm */
// vim: tabstop=4 shiftwidth=4 noexpandtab

#ifndef RESOURCES_H

typedef struct File_Resource {
	char* filename;
	uint8_t* contents;
	int size;
	bool loaded;
} File_Resource;

#define RESOURCES_H

#endif //RESOURCES_H
