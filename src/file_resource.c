/* date = April 15th 2024 5:36 pm */
// vim: tabstop=4 shiftwidth=4 noexpandtab

typedef struct File_Resource {
	char* filename;
	uint8_t* contents;
	int size;
	bool loaded;
} File_Resource;

static void load_file_resource(File_Resource* resource) {
#ifdef _WIN32
	struct __stat64 file_stats;
	_stat64(resource->filename, &file_stats);
#else
	struct stat file_stats;
	stat(resource->filename, &file_stats);
#endif

	resource->size = file_stats.st_size;
	assert(resource->size > 0);
	resource->contents = malloc(resource->size+1); // allow for NULL termination
	assert(resource->contents != NULL);
	fread(resource->contents, 1, resource->size, fopen(resource->filename, "rb"));
	resource->contents[resource->size] = 0; // NULL termination
	DEBUG_MSG("Loaded file %s\n", resource->filename);
	resource->loaded = true;
}

