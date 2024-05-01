#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "opengl.h"
#include "resource_ids.h"
#include "resources.h"
#include "generated_resources.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// TODO
// * Switch to glDrawElements->glDrawArrays so we only have 1 vertex buffer
// * Remove SDL2 entirely
// * Port demonic sign rendering code to OpenGL and/or rendering to texture/file

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))
#define MIN(x,y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  i64;
typedef int32_t  i32;
typedef int16_t  i16;
typedef int8_t   i8;

#define KB(x) ((size_t) (x) << 10)
#define MB(x) ((size_t) (x) << 20)

#ifdef DEBUG
static FILE* debug_file;
#define DEBUG_MSG(...) fprintf(debug_file, __VA_ARGS__)
#else
// TODO: what about release mode - where do we get logs if that goes wrong?
#define DEBUG_MSG(...) do { printf(__VA_ARGS__); } while (0)
#endif

typedef struct Free_Entry Free_Entry;
struct Free_Entry {
	u8* start;
	ptrdiff_t size;
	Free_Entry* prev;
	Free_Entry* next;
};

typedef struct Arena {
	u8* start;
	u8* end;
	Free_Entry* first_free;
	u8* original_start;
} Arena;

static Arena new_memory(size_t size) {
	Arena arena = {0};
	arena.start = malloc(size);
	arena.original_start = arena.start;
	arena.end = arena.start + size;
	return arena;
}

static void reset_arena(Arena* arena) {
	arena->start = arena->original_start;
}

static u8* alloc(Arena* arena, size_t size, u32 count) {
	size_t available = arena->end - arena->start;
	size_t total = count * size;
	if (arena->first_free) {
		Free_Entry* last_free = arena->first_free;
		do {
			if (last_free->size >= total) {
				Free_Entry* new_next = last_free->next;
				Free_Entry* new_prev = last_free->prev;
				if (new_next) {
					new_next->prev = new_prev;
				}
				if (new_prev) {
					new_prev->next = new_next;
				}
				memset(last_free->start, 0, last_free->size);
				return last_free->start;
			}
			last_free = last_free->next;
		} while (last_free);
	}
	if (count > (available / size)) {
		abort();
	}
	u8* result = arena->start;
	arena->start += total;
	for (int i = 0; i < total; i++) {
		result[i] = 0;
	}
	return result;
}

static void free_mem(Arena* arena, size_t size, u8* start) {
	memset(start, 0, size);
	ptrdiff_t alloc_size = (ptrdiff_t)sizeof(Free_Entry);
	Free_Entry* entry = (Free_Entry*)alloc(arena, alloc_size, 1);
	entry->start = start;
	entry->size = size;
	entry->next = NULL;
	entry->prev = NULL;
	Free_Entry* last = arena->first_free;
	if (!last) {
		arena->first_free = entry;
		return;
	}
	while (last->next) {
		last = last->next;
	}
	entry->prev = last;
	last->next = entry;
}

typedef union vec2 {
	struct { float x, y; };
	float values[2];
} vec2;

static void vec2_add(vec2* a, vec2 b) {
	a->x += b.x;
	a->y += b.y;
}

typedef union vec3 {
	struct { float x, y, z; };
	struct { vec2 xy; float _z; };
	float values[3];
} vec3;

#define VEC2_3(v2, z) (vec3){v2.x, v2.y, z}

typedef union vec4 {
	struct { float x, y, z, w; };
	struct { float r, g, b, a; };
	float values[4];
} vec4;

typedef struct rectangle2 {
	vec2 min;
	vec2 max;
} rectangle2;

static float rect_width(rectangle2* rect) {
	return (rect->max.x - rect->min.x);
}

static float rect_height(rectangle2* rect) {
	return (rect->max.y - rect->min.y);
}

static vec2 rect_centre(rectangle2* rect) {
	return (vec2){
		rect->min.x + rect_width(rect) / 2.0f,
		rect->min.y + rect_height(rect) / 2.0f
	};
}

static rectangle2 rect_min_dim(vec2 min, vec2 dim) {
	vec2 max = min;
	vec2_add(&max, dim);
	return (rectangle2){
		min,
		max
	};
}

static void rect_add_vec2(rectangle2* rect, vec2 offset) {
	rect->min.x += offset.x;
	rect->max.x += offset.x;
	rect->min.y += offset.y;
	rect->max.y += offset.y;
}

static void debug_rect(char* prefix, rectangle2* rect) {
	DEBUG_MSG("%s: {{%8.4f, %8.4f}} -> {{%8.4f, %8.4f}}\n", 
			  prefix,
			  rect->min.x, rect->min.y,
			  rect->max.x, rect->max.y);
}

typedef vec4 Color;

typedef struct matrix4_4 {
	float values[4][4];
} matrix4_4;

#define COLOR(v) (float)v/255.0f
#define RGBA(r,g,b,a) {COLOR(r),COLOR(g),COLOR(b),COLOR(a)} 

typedef struct Shader {
	int program;
	
	GLuint vao;
	GLuint vbo;
	GLuint ebo;
	
	// attributes
	int position_loc;
	int texture_loc;
	
	// uniforms
	int position_offset_loc;
	int color_loc;
	int ortho_loc;
	int settings_loc;
	int radius_loc;
	int dimensions_loc;
	int origin_loc;
	
	// textures
	int font_texture_loc;
	int font_sampler_idx;
} Shader;

typedef struct Vertex {
	vec3 position;
	vec2 texture;
} Vertex;

#define NUM_VERTICES 4

typedef struct Quad {
	Vertex vertices[NUM_VERTICES];
} Quad;

static void setup_textured_quad(Quad* quad, rectangle2 pos, float z, rectangle2 tex) {
	quad->vertices[0] = (Vertex){.position={pos.min.x, pos.min.y, z}, .texture={tex.min.x, tex.min.y}}; // top-left
	quad->vertices[1] = (Vertex){.position={pos.min.x, pos.max.y, z}, .texture={tex.min.x, tex.max.y}}; // bottom-left
	quad->vertices[2] = (Vertex){.position={pos.max.x, pos.max.y, z}, .texture={tex.max.x, tex.max.y}}; // bottom-right
	quad->vertices[3] = (Vertex){.position={pos.max.x, pos.min.y, z}, .texture={tex.max.x, tex.min.y}}; // top-right
}

static void setup_textured_quad_full_texture(Quad* quad, rectangle2 pos, float z) {
	quad->vertices[0] = (Vertex){.position={pos.min.x, pos.min.y, z}, .texture={1.0, 1.0}}; // top-right
	quad->vertices[1] = (Vertex){.position={pos.min.x, pos.max.y, z}, .texture={1.0, 0.0}}; // bottom-right
	quad->vertices[2] = (Vertex){.position={pos.max.x, pos.max.y, z}, .texture={0.0, 0.0}}; // bottom-left
	quad->vertices[3] = (Vertex){.position={pos.max.x, pos.min.y, z}, .texture={0.0, 1.0}}; // top-left
}

typedef enum Shader_Setting {
	SHADER_NONE                  = 0,
	SHADER_SAMPLE_FONT_TEXTURE   = (1 << 0),
	SHADER_ROUNDED_RECT          = (1 << 1),
	SHADER_SAMPLE_BITMAP_TEXTURE = (1 << 2),
} Shader_Setting;

typedef enum Render_Setting {
	RENDER_NONE          = 0,
	RENDER_ALPHA_BLENDED = (1 << 0),
	RENDER_BLEND_REVERSE = (1 << 1),
} Render_Setting;

typedef struct Quad_Group {
	Quad* quads;
	int num_quads;
} Quad_Group;

typedef enum Render_Command_Type {
	COMMAND_NONE,
	COMMAND_QUAD,
	COMMAND_QUAD_GROUP,
	COMMAND_CLEAR,
} Render_Command_Type;

typedef enum Uniform_Type {
	UNIFORM_NONE,
	UNIFORM_FLOAT,
	UNIFORM_I32,
	UNIFORM_VEC2,
	UNIFORM_VEC3,
	UNIFORM_VEC4,
	UNIFORM_MATRIX4_4,
} Uniform_Type;

typedef struct Uniform_Data Uniform_Data;
struct Uniform_Data {
	Uniform_Type type;
	u32 location;
	union {
		float f;
		i32 inum;
		vec2 vec_2;
		vec3 vec_3;
		vec4 vec_4;
		matrix4_4 matrix;
	} data;
	
	Uniform_Data* next;
};

typedef struct Texture_Data Texture_Data;
struct Texture_Data {
	u32 shader_idx;
	u32 texture_id;
	
	Texture_Data* next;
};

typedef struct Clear_Command {
	bool clear_color;
	Color color;
} Clear_Command;

typedef struct Render_Command Render_Command;
struct Render_Command {
	Render_Command_Type type;
	u32 shader_id;
	u32 render_settings;
	
	union {
		Quad quad;
		Quad_Group quad_group;
		Clear_Command clear_command;
	} data;
	
	Uniform_Data* first_uniform;
	Uniform_Data* last_uniform;
	Texture_Data* first_texture;
	Texture_Data* last_texture;
	
	Render_Command* next;
};

static Uniform_Data* push_uniform(Arena* memory, Render_Command* command) {
	Uniform_Data* uniform = (Uniform_Data*)alloc(memory, sizeof(Uniform_Data), 1);
	if (!command->first_uniform) {
		command->first_uniform = uniform;
		command->last_uniform  = uniform;
	} else {
		command->last_uniform->next = uniform;
		command->last_uniform = uniform;
	}
	return uniform;
}

#define PUSH_UNIFORM(memory, command, s_location, u_type, field, value) {\
Uniform_Data* data = push_uniform(memory, command); \
data->type = u_type; \
data->location = s_location; \
data->data.field = value; \
}
#define PUSH_UNIFORM_I32(memory, command, location, value) PUSH_UNIFORM(memory, command, location, UNIFORM_I32, inum, value)
#define PUSH_UNIFORM_FLOAT(memory, command, location, value) PUSH_UNIFORM(memory, command, location, UNIFORM_FLOAT, f, value)
#define PUSH_UNIFORM_VEC2(memory, command, location, value) PUSH_UNIFORM(memory, command, location, UNIFORM_VEC2, vec_2, value)
#define PUSH_UNIFORM_VEC4(memory, command, location, value) PUSH_UNIFORM(memory, command, location, UNIFORM_VEC4, vec_4, value)
#define PUSH_UNIFORM_MATRIX(memory, command, location, value) PUSH_UNIFORM(memory, command, location, UNIFORM_MATRIX4_4, matrix, value)

static Texture_Data* push_texture(Arena* memory, Render_Command* command) {
	Texture_Data* texture = (Texture_Data*)alloc(memory, sizeof(Texture_Data), 1);
	if (!command->first_texture) {
		command->first_texture = texture;
		command->last_texture  = texture;
	} else {
		command->last_texture->next = texture;
		command->last_texture = texture;
	}
	return texture;
}

#define PUSH_TEXTURE(memory, command, t_shader_idx, t_texture_id) {\
Texture_Data* texture = push_texture(memory, command);\
texture->shader_idx = t_shader_idx;\
texture->texture_id = t_texture_id;\
}

typedef struct Command_Buffer {
	Arena memory;
	Render_Command* first;
	Render_Command* last;
} Command_Buffer;

static Render_Command* push_render_command(Command_Buffer* buffer) {
	Render_Command* cmd = (Render_Command*)alloc(&buffer->memory, sizeof(Render_Command), 1);
	if (!buffer->first) {
		buffer->first = cmd;
		buffer->last  = cmd;
	} else {
		buffer->last->next = cmd;
		buffer->last = cmd;
	}
	return cmd;
}

typedef struct String {
	char* str;
	int length;
} String;

static String new_string(char* str) {
	return (String){.str=str, .length=(int)strlen(str)};
}

static bool strings_equal(String one, String two) {
	if (one.length != two.length) { return false; }
	for (int i = 0; i < one.length; i++) {
		if (one.str[i] != two.str[i]) {
			return false;
		}
	}
	return true;
}

#define STRING(s) {s, sizeof(s)-1}

static String title = STRING("Summoning");
static String win = STRING("You've Won!");
static String lost = STRING("You've Lost!");

static String words[] = {
	STRING("summon"),
	STRING("incantation"),
	STRING("spell"),
	STRING("awaken"),
	STRING("cauldron"),
};

Color white = RGBA(255, 255, 255, 255);
Color blue = RGBA(10, 10, 255, 255);
Color red = RGBA(255, 10, 10, 255);
Color very_dark_blue = RGBA(4, 8, 13, 255);
Color green = RGBA(10, 255, 10, 255);
Color amber = RGBA(255, 191, 0, 255);

typedef enum Game_State {
	STATE_MENU,
	STATE_PLAY,
	STATE_PAUSE,
	STATE_WIN,
	STATE_LOSE,
} Game_State;

typedef enum Modifier {
	MODIFIER_NONE = 0,
	MODIFIER_ALT  = (1 << 0),
	MODIFIER_CTRL = (1 << 1),
} Modifier;

typedef enum Key {
	KEY_NONE,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_ESCAPE,
	KEY_RETURN,
	KEY_SPACE,
	KEY_F,
	KEY_Q,
} Key;

// TODO: This can only handle one key pressed at a time!
typedef struct Keyboard_Input {
	bool is_down, was_down, repeat;
	Key key;
	u32 mods_held;
} Keyboard_Input;

typedef struct Mouse_Input {
	int posX, posY;
	bool left_was_down, left_is_down;
	bool right_was_down, right_is_down;
} Mouse_Input;

typedef struct Input {
	Keyboard_Input kbd_input;
	Mouse_Input mouse_input;
	char character;
} Input;

typedef struct Glyph {
	char character;
	float x0;
	float x1;
	float y0;
	float y1;
	float width;
	float height;
	float advance;
	float lsb;
	
	float tex_x0;
	float tex_x1;
	float tex_y0;
	float tex_y1;
} Glyph;

#define NUM_GLYPHS 95 // 126-32 (inclusive)
#define GLYPH_INDEX(c) c-32

typedef struct Glyph_Cache {
	Glyph glyphs[NUM_GLYPHS];
	int first_glyph;
	int last_glyph;
	float font_size;
	float font_scale;
	stbtt_packedchar *packed_chars;
	void* texture;
	u32 texture_id;
	int texture_dim;
	float ascent;
	float descent;
	float line_gap;
	float line_height;
	float row_height;
} Glyph_Cache;

// This is effectively the number of font sizes for a font
#define NUM_GLYPH_CACHES 3

typedef struct Font {
	File_Resource* resource;
	stbtt_fontinfo font;
	i32 ascent;
	i32 descent;
	i32 line_gap;
	Glyph_Cache glyph_caches[NUM_GLYPH_CACHES];
	int num_glyph_caches;
} Font;

static void glyph_to_quad(Quad* quad, Glyph* glyph, Glyph_Cache* font_cache, rectangle2* bounding_box, vec3* starting_pos) {
	int x = (int)starting_pos->x;
	int y = (int)starting_pos->y;
	int baseline = y + font_cache->ascent;
	float g_x0 = (float)(x + glyph->lsb);
	float g_x1 = (float)(g_x0 + glyph->width);
	float g_y0 = (float)(baseline + glyph->y0);
	float g_y1 = (float)(baseline + glyph->y1);
	rectangle2 pos = {.min={g_x0, g_y0}, .max={g_x1, g_y1}};
	rectangle2 tex = {.min={glyph->tex_x0, glyph->tex_y0}, .max={glyph->tex_x1, glyph->tex_y1}};
	*bounding_box = pos;
	
	starting_pos->x += (float)glyph->advance;
	setup_textured_quad(quad, pos, starting_pos->z, tex);
}

typedef struct Text_Group {
	String text;
	u32 font_cache_id;
	
	// calculated from the above
	u32 num_glyphs;
	Quad* quads;
	rectangle2* glyph_bounding_boxes;
	rectangle2 bounding_box;
} Text_Group;

static void free_text_group(Text_Group* group) {
	free(group->quads);
	free(group->glyph_bounding_boxes);
}

typedef struct Type_Challenge {
	Text_Group text_group;
	bool* typed_correctly; // This could be tighter packed, but we don't care right now :)
	int position;
	float alpha;
	float alpha_fade_speed;
} Type_Challenge;

static void free_challenge(Type_Challenge* challenge) {
	free(challenge->typed_correctly);
	free_text_group(&challenge->text_group);
}

static bool is_challenge_done(Type_Challenge* challenge) {
	return (challenge->position >= challenge->text_group.text.length);
}

static bool has_typing_started(Type_Challenge* challenge) {
	return (challenge->position > 0);
}

#ifdef DEBUG
static void debug_enter_character(char character) {
	printf("%c", character);
}

static void debug_end_challenge() {
	printf("\n");
}
static void debug_reset_challenge() {
	printf(" !! reset!\n");
}
#endif

static void enter_challenge_character(Type_Challenge* challenge, char character) {
	if (!is_challenge_done(challenge)) {
#ifdef DEBUG
		debug_enter_character(character);
#endif
		challenge->typed_correctly[challenge->position] = (character == challenge->text_group.text.str[challenge->position]);
		challenge->position++;
	}
#ifdef DEBUG
	if (is_challenge_done(challenge)) {
		debug_end_challenge();
	}
#endif
}

static bool challenge_has_mistakes(Type_Challenge* challenge) {
	for (int i = 0; i < challenge->position; i++) {
		if (!challenge->typed_correctly[i]) {
			return true;
		}
	}
	return false;
}

static void update_challenge_alpha(Type_Challenge* challenge, float dt) {
	// TODO: LERP?
	float* alpha = &challenge->alpha;
	if (*alpha < 1.0f) {
		if (has_typing_started(challenge) || (challenge->alpha_fade_speed == 0.0f)) {
			// speed up fading in if the typing has started
			*alpha += dt;
		} else {
			*alpha += dt / challenge->alpha_fade_speed;
		}
		*alpha = (*alpha > 1.0) ? 1.0 : *alpha;
	}
}

static void reset_challenge(Type_Challenge* challenge) {
#ifdef DEBUG
	debug_reset_challenge();
#endif
	challenge->position = 0;
	challenge->alpha = 0.0;
}

#define MAX_NUM_CHALLENGES 8

typedef struct Level_Data {
	Type_Challenge challenges[MAX_NUM_CHALLENGES];
	int num_challenges;
	int current_challenge;
	int level_number;
	int points;
} Level_Data;

static void reset_level(Level_Data* level) {
	for (int i = 0; i < level->current_challenge+1; i++) {
		reset_challenge(&level->challenges[i]);
	}
	level->current_challenge = 0;
	level->points = 0;
	if (!SDL_IsTextInputActive()) {
		SDL_StartTextInput();
	}
}

static bool is_level_done(Level_Data* level) {
	if (level->current_challenge < level->num_challenges) {
		return false;
	}
	return true;
}

static void level_type_character(Level_Data* level, char character) {
	if (is_level_done(level)) { return; }
	Type_Challenge* challenge = &level->challenges[level->current_challenge];
	enter_challenge_character(challenge, character);
	if (is_challenge_done(challenge)) {
		level->current_challenge++;
		if (!challenge_has_mistakes(challenge)) {
			level->points++;
		}
	}
	if (is_level_done(level)) {
		SDL_StopTextInput();
	}
}

typedef struct Sized_Texture {
	SDL_Texture* texture;
	SDL_Rect rect;
} Sized_Texture;

#define MAX_SHADERS 8

typedef struct Game_Window {
	bool quit; // zero-init means quit=false by default
	int window_width, window_height;
	bool fullscreen;
	SDL_Window* window;
	SDL_GLContext gl_context;
	Shader shaders[MAX_SHADERS];
	u32 num_shaders;
	u32 quad_shader_id;
	matrix4_4 ortho_matrix;
	Font font;
	int title_font_cache_id;
	int challenge_font_cache_id;
	int demonic_font_cache_id;
	uint64_t last_frame_perf_counter;
	float dt;
	int frame_number;
	
	Command_Buffer command_buffer;
	
	Game_State state;
	Type_Challenge title_challenge; // initial title page challenge
	
	Input input;
	
	Level_Data level_data;
	
	Sized_Texture* demonic_word_textures;
	int demonic_word_i;
	
	Text_Group win_text_group;
	Text_Group lose_text_group;
	GLuint summoning_sign_tex_id;
} Game_Window;

static Game_Window* game_window = NULL;

static Render_Command* fill_rounded_rect(Command_Buffer* buffer, u32 shader_id, rectangle2 rect, Color color, float z, float radius) {
	Shader* shader = &game_window->shaders[shader_id];
	Render_Command* command = push_render_command(buffer);
	command->type = COMMAND_QUAD;
	command->shader_id = shader_id;
	command->render_settings = RENDER_ALPHA_BLENDED;
	PUSH_UNIFORM_VEC4(&buffer->memory, command, shader->color_loc, color);
	setup_textured_quad(&command->data.quad, rect, z, (rectangle2){0});
	
	return command;
}

static void setup_text_group(Text_Group* group) {
	Glyph_Cache* font_cache = &game_window->font.glyph_caches[group->font_cache_id];
	vec3 position = {0};
	group->bounding_box = (rectangle2){0};
	group->bounding_box.max.y = font_cache->ascent + font_cache->descent;
	group->num_glyphs = group->text.length;
	group->quads = (Quad*)calloc(group->num_glyphs, sizeof(Quad));
	group->glyph_bounding_boxes = (rectangle2*)calloc(group->num_glyphs, sizeof(rectangle2));
	for (int i = 0; i < group->text.length; i++) {
		char character = group->text.str[i];
		Glyph* glyph = &font_cache->glyphs[GLYPH_INDEX(character)];
		Quad* quad = &group->quads[i];
		rectangle2* glyph_bounding_box = &group->glyph_bounding_boxes[i];
		glyph_to_quad(quad, glyph, font_cache, glyph_bounding_box, &position);
		group->bounding_box.max.x += glyph->width;
	}
}

static Shader* push_shader(u32* shader_id) {
	assert(game_window->num_shaders < MAX_SHADERS);
	*shader_id = game_window->num_shaders;
	return &game_window->shaders[game_window->num_shaders++];
}

static void setup_challenge(Type_Challenge* challenge, int font_cache_id, String text) {
	challenge->text_group.text = text;
	challenge->text_group.font_cache_id = font_cache_id;
	setup_text_group(&challenge->text_group);
	
	challenge->typed_correctly = calloc(text.length, sizeof(bool));
}

static void setup_level(Level_Data* level, int font_cache_id) {
	for (int i = 0; i < ARRAY_LEN(words); i++) {
		if (i >= MAX_NUM_CHALLENGES) { break; }
		setup_challenge(&level->challenges[i], font_cache_id, words[i]);
		level->challenges[i].alpha_fade_speed = 1.5f;
		level->num_challenges++;
	}
	level->current_challenge = 0; // bit redundant!
	if (!SDL_IsTextInputActive()) {
		SDL_StartTextInput(); // so we can type 'into' the initial challenge text
	}
}

static bool key_first_down() {
	return (game_window->input.kbd_input.is_down) && !(game_window->input.kbd_input.was_down);
}

static bool key_is_down(Key key, u32 mods) {
	return (game_window->input.kbd_input.key == key) 
		&& (game_window->input.kbd_input.is_down)
		&& (game_window->input.kbd_input.mods_held == mods);
}

static bool key_repeat() {
	return (game_window->input.kbd_input.repeat);
}

#define DEFAULT_WINDOW_WIDTH  1280
#define DEFAULT_WINDOW_HEIGHT 800
#define KEYBOARD_VELOCITY 10

#ifndef __EMSCRIPTEN__
static float fps_cap = 60.0f;
static float fps_cap_in_ms;
#endif

static void ShowSDLError(char* message) {
	const char* sdl_error = SDL_GetError();
	char buffer[1024] = {0};
	sprintf("%s : %s", message, sdl_error);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
							 "Error",
							 &buffer[0],
							 game_window->window);
}

static void ShowGLError() {
	GLenum error = glGetError();
	assert(error == GL_NO_ERROR);
	while (error != GL_NO_ERROR) {
		// do something with each error that has occured
		error = glGetError();
	}
}

static void init_font(Font* font) {
	const unsigned char* data = (const unsigned char*)font->resource->contents;
	stbtt_InitFont(&font->font, data, stbtt_GetFontOffsetForIndex(data, 0));
	stbtt_GetFontVMetrics(&font->font, &font->ascent, &font->descent, &font->line_gap);
}

static int push_font_size(Font* font, float font_size) {
	int font_size_i = font->num_glyph_caches;
	assert(font_size_i < NUM_GLYPH_CACHES);
	
	Glyph_Cache* cache = &font->glyph_caches[font_size_i];
	int font_index = 0;
	cache->font_size = font_size;
	cache->first_glyph = 32;
	cache->last_glyph = 126;
	int num_chars = cache->last_glyph - cache->first_glyph;
	cache->packed_chars = calloc(num_chars+1, sizeof(stbtt_packedchar));
	stbtt_pack_context pack_context;
	cache->texture_dim = 512;
	cache->font_scale = stbtt_ScaleForPixelHeight(&font->font, cache->font_size);
	cache->ascent = (float)font->ascent * cache->font_scale;
	cache->descent = (float)font->descent * cache->font_scale;
	cache->line_gap = (float)font->line_gap * cache->font_scale;
	cache->line_height = cache->ascent - cache->descent;
	cache->row_height = cache->line_height + cache->line_gap;
	cache->texture = malloc(cache->texture_dim*cache->texture_dim);
	const unsigned char* data = (const unsigned char*)font->resource->contents;
	while (true) {
		if (!stbtt_PackBegin(&pack_context, cache->texture, cache->texture_dim, cache->texture_dim, 0, 1, NULL)) {
			// error
			return -1;
		}
		if (!stbtt_PackFontRange(&pack_context, data, font_index, font_size, cache->first_glyph, num_chars, cache->packed_chars)) {
			cache->texture_dim *= 2;
			cache->texture = realloc(cache->texture, cache->texture_dim*cache->texture_dim);
			assert(cache->texture != NULL);
		} else {
			stbtt_PackEnd(&pack_context);
			break; // we're done!
		}
	}
	
	glGenTextures(1, &cache->texture_id);
	glBindTexture(GL_TEXTURE_2D, cache->texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, cache->texture_dim, cache->texture_dim, 0, GL_RED, GL_UNSIGNED_BYTE, cache->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	i32 x0, y0, x1, y1, advance, lsb = 0;
	for (char character = cache->first_glyph; character <= cache->last_glyph; character++) {
		int idx = character - cache->first_glyph;
		assert(idx < NUM_GLYPHS);
		int glyph_idx = stbtt_FindGlyphIndex(&font->font, character);
		stbtt_packedchar packed_char = cache->packed_chars[idx];
		stbtt_GetGlyphHMetrics(&font->font, glyph_idx, &advance, &lsb);
		stbtt_GetGlyphBitmapBox(&font->font, glyph_idx, cache->font_scale, cache->font_scale, &x0, &y0, &x1, &y1);
		cache->glyphs[idx].character = character;
		cache->glyphs[idx].x0 = (float)x0;
		cache->glyphs[idx].x1 = (float)x1;
		cache->glyphs[idx].y0 = (float)y0;
		cache->glyphs[idx].y1 = (float)y1;
		cache->glyphs[idx].width = (float)(x1 - x0);
		cache->glyphs[idx].height = (float)(y1 - y0);
		cache->glyphs[idx].advance = (float)advance * cache->font_scale;
		cache->glyphs[idx].lsb = (float)lsb * cache->font_scale;
		cache->glyphs[idx].tex_x0 = (float)packed_char.x0 / (float)cache->texture_dim;
		cache->glyphs[idx].tex_x1 = (float)packed_char.x1 / (float)cache->texture_dim;
		cache->glyphs[idx].tex_y0 = (float)packed_char.y0 / (float)cache->texture_dim;
		cache->glyphs[idx].tex_y1 = (float)packed_char.y1 / (float)cache->texture_dim;
	}
	
	font->num_glyph_caches++;
	return font_size_i;
}

// This is from https://www.computerenhance.com/p/efficient-dda-circle-outlines
static void draw_circle(void* pixel_data, int radius, u32 color) {
	u32 (*pixels)[256] = pixel_data;
	int Cx = 256 / 2;
	int Cy = 256 / 2;
	int R = radius;
	{
		int R2 = R+R;
		
		int X = R;
		int Y = 0;
		int dY = -2;
		int dX = R2+R2 - 4;
		int D = R2 - 1;
		
		while (Y <= X) {
			pixels[Cy - Y][Cx - X] = color;
			pixels[Cy - Y][Cx + X] = color;
			pixels[Cy + Y][Cx - X] = color;
			pixels[Cy + Y][Cx + X] = color;
			pixels[Cy - X][Cx - Y] = color;
			pixels[Cy - X][Cx + Y] = color;
			pixels[Cy + X][Cx - Y] = color;
			pixels[Cy + X][Cx + Y] = color;
			
			D += dY;
			dY -= 4;
			++Y;
			
			int Mask = (D >> 31);
			D += dX & Mask;
			dX -= 4 & Mask;
			X += Mask;
		}
	}
}

#define SWAP(x, y, T) do { T SWAP = x; x = y; y = SWAP; } while (0)

static int ipart(float in) {
	return (int)floor(in);
}

static float fpart(float x) {
	return (x - (float)ipart(x));
}

static float rfpart(float x) {
	return (1.0 - fpart(x));
}

// This is from https://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm
static void draw_line(void* pixel_data, float x0, float y0, float x1, float y1, u32 color) {
	u32 (*pixels)[256] = pixel_data;
	bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
	
	if (steep) {
		SWAP(x0, y0, float);
		SWAP(x1, y1, float);
	}
	if (x0 > x1) {
		SWAP(x0, x1, float);
		SWAP(y0, y1, float);
	}
	
	float dx = x1 - x0;
	float dy = y1 - y0;
	
	float gradient;
	if (dx == 0.0) {
		gradient = 1.0f;
	} else {
		gradient = dy / dx;
	}
	
	float xend = round(x0);
	float yend = y0 + gradient * (xend - x0);
	float xgap = rfpart(x0 + 0.5f);
	int xpxl1 = xend;
	int ypxl1 = ipart(yend);
	if (steep) {
		pixels[xpxl1][ypxl1] = color;
		pixels[xpxl1][ypxl1+1] = color;
	} else {
		pixels[ypxl1][xpxl1] = color;
		pixels[ypxl1+1][xpxl1] = color;
	}
	float intery = yend + gradient;
	
	xend = round(x1);
	yend = y1 + gradient * (xend - x1);
	xgap = fpart(x1 + 0.5);
	int xpxl2 = xend;
	int ypxl2 = ipart(yend);
	if (steep) {
		pixels[xpxl2][ypxl2] = color;
		pixels[xpxl2][ypxl2+1] = color;
	} else {
		pixels[ypxl2][xpxl2] = color;
		pixels[ypxl2+1][xpxl2] = color;
	}
	
	if (steep) {
		for (int x = (xpxl1 + 1); x < (xpxl2 - 1); x++) {
			pixels[x][ipart(intery)] = color;
			pixels[x][ipart(intery)+1] = color;
			intery += gradient;
		}
	} else {
		for (int x = (xpxl1 + 1); x < (xpxl2 - 1); x++) {
			pixels[ipart(intery)][x] = color;
			pixels[ipart(intery)+1][x] = color;
			intery += gradient;
		}
	}
}

#if 0
static bool rendered_sign = false;
#endif

static GLuint render_demonic_sign_to_texture(String word, SDL_Rect* area) {
	area->w = area->h = 256;
	u8* pixels = (u8*)malloc(sizeof(u8) * area->w * area->h * 4); // 4 channels (r,g,b,a)
	for (int p = 0; p < 256*256; p++) {
		pixels[p] = 0;
	}
	u32 my_green = 0xff00ff00;
	draw_circle(pixels, 127, my_green);
	int inner_radius = 95;
	draw_circle(pixels, inner_radius, my_green);
	u32 my_red = 0xff0000ff;
	u32* my_pixels = (u32*)pixels;
	my_pixels[10] = my_red; //top-left
	
#if 0
	char first_char = toupper(word.str[0]);
	SDL_Surface* char_surface = TTF_RenderGlyph_Solid(game_window->demonic_font.font, first_char, green);
	SDL_Rect char_rect = {0};
	char_rect.w = char_surface->w;
	char_rect.h = char_surface->h;
	SDL_Rect dst_rect = char_rect;
	dst_rect.x = (area->w / 2) - (char_rect.w / 2);
	SDL_BlitSurface(char_surface, &char_rect,
					surface, &dst_rect);
	SDL_FreeSurface(char_surface);
	// Center of first char dest rect:
	int char_cx = (dst_rect.w / 2) + dst_rect.x;
	int char_cy = (dst_rect.h / 2) + dst_rect.y;
	// Center of area:
	int area_cx = area->w / 2;
	int area_cy = area->h / 2;
	assert(area_cx == char_cx);
	float angle_deg = 360.0 / (float)word.length;
	float theta = angle_deg * (M_PI / 180.0f);
	int new_cx, new_cy;
	//SDL_Point top = {char_cx, area_cy - inner_radius};
	SDL_Point* input_points = calloc(word.length+1, sizeof(SDL_Point));
	SDL_Point* restore_points = input_points;
	*input_points = (SDL_Point){char_cx, area_cy - inner_radius};
	input_points++;
	
	for (int i = 1; i < word.length; i++) {
		SDL_Surface* char_surface = TTF_RenderGlyph_Solid(game_window->demonic_font.font, word.str[i], green);
		SDL_Rect char_rect = {0};
		char_rect.w = char_surface->w;
		char_rect.h = char_surface->h;
		SDL_Rect dst_rect = char_rect;
		new_cx = cos(theta) * (char_cx - area_cx) - sin(theta) * (char_cy - area_cy) + area_cx;
		new_cy = sin(theta) * (char_cx - area_cx) + cos(theta) * (char_cy - area_cy) + area_cy;
		dst_rect.x = new_cx - (dst_rect.w / 2);
		dst_rect.y = new_cy - (dst_rect.h / 2);
		char_cx = new_cx;
		char_cy = new_cy;
		SDL_BlitSurface(char_surface, &char_rect,
						surface, &dst_rect);
		SDL_FreeSurface(char_surface);
		
		float vx = (float)char_cx - (float)area_cx;
		float vy = (float)char_cy - (float)area_cy;
		float magnitude = sqrt(vx*vx + vy*vy);
		vx /= magnitude;
		vy /= magnitude;
		int edge_x = (int)((float)area_cx + vx * (float)inner_radius);
		int edge_y = (int)((float)area_cy + vy * (float)inner_radius);
		*input_points = (SDL_Point){edge_x, edge_y};
		input_points++;
	}
	*input_points = *restore_points; // last point is the same as first
	input_points = restore_points;
	// Re-order points from input->output by doing spans...
	SDL_Point* points = calloc(word.length+1, sizeof(SDL_Point));
	SDL_Point* output_points = points;
	int input_i = 0;
	int half_word = word.length / 2;
	bool even_word_length = false;
	if ((word.length % 2) == 0) {
		half_word--;
		even_word_length = true;
	}
	// TODO: with even words, we need to +1 and go around again?
	// currently this is repeating lines I think
	for (int i = 0; i < word.length; i++) {
		output_points[i] = input_points[input_i];
		input_i = (input_i + half_word) % word.length;
	}
	output_points[word.length] = input_points[0];
	
	for (int i = 0; i < word.length; i++) {
		SDL_Point p0 = output_points[i];
		SDL_Point p1 = output_points[i+1];
		if (p0.x < p1.x) {
			draw_line(surface->pixels, p0.x, p0.y, p1.x, p1.y, my_green);
		} else {
			draw_line(surface->pixels, p1.x, p1.y, p0.x, p0.y, my_green);
		}
	}
	free(output_points);
	free(input_points);
#endif
	
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, area->w, area->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	free(pixels); // can we free this now?
	glBindTexture(GL_TEXTURE_2D, 0);
	
#if 0
	if (!rendered_sign) {
		SDL_SaveBMP(surface, "output.bmp");
		rendered_sign = true;
	}
#endif
	
	return texture_id;
}

#if 0
static void render_text_into_texture(SDL_Renderer* renderer, Sized_Texture* sized_texture, TTF_Font *font, String text) {
	TTF_SizeText(font, text.str, &sized_texture->rect.w, &sized_texture->rect.h);
	SDL_Surface* surface = TTF_RenderText_Solid(font, text.str, white);
	sized_texture->texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
}
#endif

static void update_ortho_matrix(void) {
	float minx = 0.0f;
	float maxx = (float)game_window->window_width;
	float maxy = 0.0f;
	float miny = (float)game_window->window_height; // swapped?
	float minz = -1.0f; // TODO: hard-coded Z
	float maxz = 1.0f;  // TODO: hard-coded Z
	
	float xdiff = maxx - minx;
	float ydiff = maxy - miny;
	float zdiff = maxz - minz;
	
	float ortho_matrix[4][4] = {
		{2.0f / xdiff,         0.0f,                 0.0f,                 0.0f},
		{0.0f,                 2.0f / ydiff,         0.0f,                 0.0f},
		{0.0f,                 0.0f,                 -2.0f / zdiff,        0.0f},
		{-((maxx+minx)/xdiff), -((maxy+miny)/ydiff), -((maxz+minz)/zdiff), 1.0f}
	};
	DEBUG_MSG("ortho matrix:\n");
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			DEBUG_MSG("%8.4f, ", ortho_matrix[i][j]);
		}
		DEBUG_MSG("\n");
	}
	//game_window->ortho_matrix.values = ortho_matrix;
	// NOTE: do we have to memcpy from one multi-dimensional array to another?
	memcpy(&game_window->ortho_matrix.values, ortho_matrix, 4*4*sizeof(GLfloat));
}

static void exec_command_buffer() {
	Command_Buffer* buffer = &game_window->command_buffer;
	Render_Command* command = buffer->first;
	Shader* shader = &game_window->shaders[command->shader_id];
	while (command) {
		if (command->render_settings & RENDER_ALPHA_BLENDED) {
			glEnable(GL_BLEND);
			if (command->render_settings & RENDER_BLEND_REVERSE) {
				glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
			} else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		} else {
			glDisable(GL_BLEND);
		}
		
		switch (command->type) {
			case COMMAND_CLEAR: {
				Clear_Command clear = command->data.clear_command;
				if (clear.clear_color) {
					Color c = clear.color;
					glClearColor(c.r, c.g, c.b, c.a);
					glClear(GL_COLOR_BUFFER_BIT);
				}
			} break;
			case COMMAND_QUAD_GROUP:
			case COMMAND_QUAD: {
				int num_quads = 0;
				Quad* quads;
				if (command->type == COMMAND_QUAD) {
					num_quads = 1;
					quads = &command->data.quad;
				} else {
					num_quads = command->data.quad_group.num_quads;
					quads = command->data.quad_group.quads;
				}
				assert(shader->program > 0);
				glUseProgram(shader->program);
				glBindVertexArray(shader->vao);
				
				Uniform_Data* uniform = command->first_uniform;
				while (uniform) {
					switch (uniform->type) {
						case UNIFORM_FLOAT: {
							glUniform1f(uniform->location, uniform->data.f);
						} break;
						case UNIFORM_I32: {
							glUniform1i(uniform->location, uniform->data.inum);
						} break;
						case UNIFORM_VEC2: {
							glUniform2fv(uniform->location, 1, &uniform->data.vec_2.values[0]);
						} break;
						case UNIFORM_VEC3: {
							glUniform3fv(uniform->location, 1, &uniform->data.vec_3.values[0]);
						} break;
						case UNIFORM_VEC4: {
							glUniform4fv(uniform->location, 1, &uniform->data.vec_4.values[0]);
						} break;
						case UNIFORM_MATRIX4_4: {
							glUniformMatrix4fv(uniform->location, 1, GL_FALSE, &uniform->data.matrix.values[0][0]);
						} break;
						case UNIFORM_NONE: {
							abort();
						} break;
					}
					uniform = uniform->next;
				}
				
				Texture_Data* texture = command->first_texture;
				while (texture) {
					glActiveTexture(GL_TEXTURE0 + texture->shader_idx);
					glBindTexture(GL_TEXTURE_2D, texture->texture_id);
					texture = texture->next;
				}
				
				int stride = sizeof(Vertex);
				
				glBindBuffer(GL_ARRAY_BUFFER, shader->vbo);
				while (num_quads > 0) {
					glBufferData(GL_ARRAY_BUFFER, 4 * stride, quads, GL_STATIC_DRAW);
					glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
					quads++;
					num_quads--;
				}
			} break;
			case COMMAND_NONE: {
				abort();
			} break;
		}
		command = command->next;
	}
	buffer->first = NULL;
	reset_arena(&buffer->memory);
}

static void init_shader(Shader* shader) {
	glUseProgram(shader->program);
	
	char name_buffer[1024] = {0};
	GLsizei name_size = 0;
	GLsizei attr_size = 0;
	GLuint attr_type = 0;
	
	int active_attributes = 0;
	glGetProgramiv(shader->program, GL_ACTIVE_ATTRIBUTES, &active_attributes);
	
	for (int i = 0; i < active_attributes; i++) {
		glGetActiveAttrib(shader->program, i, 1024, &name_size, &attr_size, &attr_type, &name_buffer[0]);
		int loc = glGetAttribLocation(shader->program, name_buffer);
		assert(loc >= 0);
		DEBUG_MSG("loc[%d] name: %s\n", loc, name_buffer);
		if (strcmp(name_buffer, "position") == 0) {
			shader->position_loc = loc;
		} else if (strcmp(name_buffer, "texture") == 0) {
			shader->texture_loc = loc;
		}
 	}
	
	int sampler_idx = 0;
	int active_uniforms = 0;
	glGetProgramiv(shader->program, GL_ACTIVE_UNIFORMS, &active_uniforms);
	
	for (int i = 0; i < active_uniforms; i++) {
		glGetActiveUniform(shader->program, i, 1024, &name_size, &attr_size, &attr_type, &name_buffer[0]);
		int loc = glGetUniformLocation(shader->program, name_buffer);
		assert(loc >= 0);
		DEBUG_MSG("loc[%d] name: %s\n", loc, name_buffer);
		if (strcmp(name_buffer, "position_offset") == 0) {
			shader->position_offset_loc = loc;
		} else if (strcmp(name_buffer, "ortho") == 0) {
			shader->ortho_loc = loc;
		} else if (strcmp(name_buffer, "fontTexture") == 0) {
			shader->font_texture_loc = loc;
			shader->font_sampler_idx = sampler_idx;
		} else if (strcmp(name_buffer, "settings") == 0) {
			shader->settings_loc = loc;
		} else if (strcmp(name_buffer, "color") == 0) {
			shader->color_loc = loc;
		} else if (strcmp(name_buffer, "radius") == 0) {
			shader->radius_loc = loc;
		} else if (strcmp(name_buffer, "dimensions") == 0) {
			shader->dimensions_loc = loc;
		} else if (strcmp(name_buffer, "origin") == 0) {
			shader->origin_loc = loc;
		}
		
		if (attr_type == GL_SAMPLER_2D) {
			sampler_idx++;
		}
	}	
}

static void check_shader(File_Resource* resource, GLuint shader) {
	int status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		int info_len = 0;
		char buffer[1024] = {0};
		glGetShaderInfoLog(shader, 1024, &info_len, &buffer[0]);
		DEBUG_MSG("Error in shader %s:\n%s\n", resource->filename, buffer);
		DEBUG_MSG("\n======= shader source: ====== \n");
		DEBUG_MSG("%s", resource->contents);
		DEBUG_MSG("\n======= END OF SOURCE  ====== \n");
	}
	assert(status == GL_TRUE);
}

static void check_program_info(GLuint program, GLenum param) {
	int status = 0;
	glGetProgramiv(program, param, &status);
	if (status != GL_TRUE) {
		int info_len = 0;
		char buffer[1024] = {0};
		glGetProgramInfoLog(program, 111024, &info_len, &buffer[0]);
		char* desc = "unknown";
		switch (param) {
			case GL_LINK_STATUS: { desc="GL_LINK_STATUS"; } break;
			case GL_VALIDATE_STATUS: { desc="GL_VALIDATE_STATUS"; } break;
		}
		DEBUG_MSG("Error in program (%s):\n%s\n", desc, buffer);
	}
	assert(status == GL_TRUE);
}

static void check_program_linking(GLuint program) {
	check_program_info(program, GL_LINK_STATUS);
}

static void check_program_valid(GLuint program) {
	glValidateProgram(program);
	check_program_info(program, GL_VALIDATE_STATUS);
}

static void init_gl(void) {
	DEBUG_MSG("gl version: %s\n", glGetString(GL_VERSION));
	DEBUG_MSG("gl shading language version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	GLuint program_id = glCreateProgram();
	DEBUG_MSG("program_id: %d\n", program_id);
	
	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	
	File_Resource* vertex_shader_src = &global_file_resources[RES_ID(VERTEX_SHADER_SOURCE)];
	glShaderSource(vertex_shader, 1, (const char**)&vertex_shader_src->contents, NULL);
	glCompileShader(vertex_shader);
	check_shader(vertex_shader_src, vertex_shader);
	
	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	File_Resource* fragment_shader_src = &global_file_resources[RES_ID(FRAGMENT_SHADER_SOURCE)];
	glShaderSource(fragment_shader, 1, (const char**)&fragment_shader_src->contents, NULL);
	glCompileShader(fragment_shader);
	check_shader(fragment_shader_src, fragment_shader);
	
	glAttachShader(program_id, vertex_shader);
	glAttachShader(program_id, fragment_shader);
	glLinkProgram(program_id);
	
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	
	// TODO: macOS may break if we validate linking before binding a VAO
	check_program_linking(program_id);
	check_program_valid(program_id);
	
	update_ortho_matrix();
	
	Shader* shader = push_shader(&game_window->quad_shader_id);
	shader->program = program_id;
	init_shader(shader);
	
	int stride = sizeof(Vertex);
	
	glGenVertexArrays(1, &shader->vao); ShowGLError();
	glBindVertexArray(shader->vao); ShowGLError();
	
	glGenBuffers(1, &shader->vbo);
	glGenBuffers(1, &shader->ebo);
	
	glBindBuffer(GL_ARRAY_BUFFER, shader->vbo); ShowGLError();
	glBufferData(GL_ARRAY_BUFFER, NUM_VERTICES * stride, NULL, GL_STATIC_DRAW); ShowGLError();
	
	glEnableVertexAttribArray(shader->position_loc); ShowGLError();
	glVertexAttribPointer(shader->position_loc, 3, GL_FLOAT, GL_FALSE, stride, NULL); ShowGLError();
	glEnableVertexAttribArray(shader->texture_loc); ShowGLError();
	glVertexAttribPointer(shader->texture_loc, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(sizeof(vec3))); ShowGLError();
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader->ebo);
	GLuint indexData[] = { 0, 1, 2, 2, 3, 0 };
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLuint), indexData, GL_STATIC_DRAW);
	
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void init_the_game(void) {
	game_window = (Game_Window*)malloc(sizeof(Game_Window));
	memset(game_window, 0, sizeof(Game_Window));
	game_window->command_buffer.memory = new_memory(MB(8));
	
#if defined(DEBUG) && defined(DEBUG_STDOUT)
	debug_file = stdout;
#elif defined(DEBUG)
	debug_file = fopen("summoning_debug_log.txt", "w");
	setvbuf(debug_file, NULL, _IONBF, 0);
#endif
	
	game_window->window_width = DEFAULT_WINDOW_WIDTH;
	game_window->window_height = DEFAULT_WINDOW_HEIGHT;
	game_window->last_frame_perf_counter = SDL_GetPerformanceCounter();
	SDL_Init(SDL_INIT_EVERYTHING);
	game_window->fullscreen = false;
	int window_flags = SDL_WINDOW_OPENGL;
	int num_displays = SDL_GetNumVideoDisplays();
	if (num_displays == 1) {
		SDL_DisplayMode display_mode = {0};
		SDL_GetCurrentDisplayMode(0, &display_mode);
		if ((display_mode.w == DEFAULT_WINDOW_WIDTH)
			&& (display_mode.h == DEFAULT_WINDOW_HEIGHT)) {
			game_window->fullscreen = true;
			window_flags |= SDL_WINDOW_FULLSCREEN;
		}
	}
	game_window->window = SDL_CreateWindow("Ludum Dare 55: Summoning",
										   SDL_WINDOWPOS_UNDEFINED,
										   SDL_WINDOWPOS_UNDEFINED,
										   game_window->window_width, 
										   game_window->window_height, 
										   window_flags);
	
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	game_window->gl_context = SDL_GL_CreateContext(game_window->window);
	if (game_window->gl_context == NULL) {
		const char* sdl_error = SDL_GetError();
		printf("failed to create GL context: %s\n", sdl_error);
	}
	assert(game_window->gl_context != NULL);
	int win_width, win_height = 0;
	SDL_GL_GetDrawableSize(game_window->window, &win_width, &win_height);
	if (SDL_GL_SetSwapInterval(1) != 0) {
		printf("Can't set VSync on GL context: %s\n", SDL_GetError());
		//abort();
	}
	if (SDL_GL_MakeCurrent(game_window->window, game_window->gl_context) != 0) {
		printf("Can't make GL context current: %s\n", SDL_GetError());
		abort();
	}
	load_gl_funcs();
	init_gl();
	glViewport(0, 0, win_width, win_height);
	
	game_window->font.resource = &global_file_resources[RES_ID(IM_FELL_FONT_ID)];
	init_font(&game_window->font);
	
	game_window->title_font_cache_id = push_font_size(&game_window->font, 120.0);
	game_window->challenge_font_cache_id = push_font_size(&game_window->font, 48.0);
	game_window->demonic_font_cache_id = push_font_size(&game_window->font, 24.0);
	
	game_window->win_text_group.text = win;
	game_window->win_text_group.font_cache_id = game_window->title_font_cache_id;
	setup_text_group(&game_window->win_text_group);
	game_window->lose_text_group.text = lost;
	game_window->lose_text_group.font_cache_id = game_window->title_font_cache_id;
	setup_text_group(&game_window->lose_text_group);
	
	game_window->title_challenge.alpha_fade_speed = 10.0f; // slow for the title
	setup_challenge(&game_window->title_challenge, game_window->title_font_cache_id, title);
	
	// SDL_StartTextInput(); // so we can type 'into' the initial challenge text
	
	SDL_Rect summoning_sign_rect = {0};
	game_window->summoning_sign_tex_id = render_demonic_sign_to_texture(title, &summoning_sign_rect);
#if 0
	game_window->demonic_word_textures = calloc(ARRAY_LEN(words), sizeof(Sized_Texture));
	for (int i = 0; i < ARRAY_LEN(words); i++) {
		game_window->demonic_word_textures[i].texture = render_demonic_sign_to_texture(words[i], &(game_window->demonic_word_textures[i].rect));
	}
#endif
	
	//if (SDL_IsTextInputActive()) {
	//SDL_StopTextInput();
	//}
}

static void game_handle_input(void) {
	if (key_is_down(KEY_F, MODIFIER_CTRL)) {
		game_window->fullscreen = !game_window->fullscreen;
		u32 window_flags = 0;
		if (game_window->fullscreen) {
			window_flags |= SDL_WINDOW_FULLSCREEN;
		}
		SDL_SetWindowFullscreen(game_window->window, window_flags);
		return;
	}
	if (key_is_down(KEY_Q, MODIFIER_CTRL)) {
		game_window->quit = true;
	}
	
	switch(game_window->state) {
		case STATE_PLAY: {
			if (game_window->input.character != 0) {
				level_type_character(&game_window->level_data, game_window->input.character);
				if (is_level_done(&game_window->level_data)) {
					if (game_window->level_data.points < game_window->level_data.num_challenges) {
						game_window->state = STATE_LOSE;
						DEBUG_MSG("Lose state!\n");
					} else {
						game_window->state = STATE_WIN;
						DEBUG_MSG("Win state!\n");
					}
				}
			} else if (key_is_down(KEY_ESCAPE, MODIFIER_NONE) && key_first_down()) {
				reset_level(&game_window->level_data);
			}
		} break;
		case STATE_PAUSE: {
			if (key_is_down(KEY_ESCAPE, MODIFIER_NONE) && key_first_down()) {
				game_window->state = STATE_PLAY;
			}
		} break;
		case STATE_MENU: {
			//if (key_is_down(KEY_SPACE, MODIFIER_NONE) && key_first_down()) {
			//game_window->demonic_word_i++;
			//game_window->demonic_word_i %= ARRAY_LEN(words);
			//return;
			//}
			if (game_window->input.character != 0) {
				enter_challenge_character(&game_window->title_challenge, game_window->input.character);
				if (is_challenge_done(&game_window->title_challenge)) {
					if (challenge_has_mistakes(&game_window->title_challenge)) {
						// TODO: show a message to indicate why this resets
						reset_challenge(&game_window->title_challenge);
					} else {
						if (SDL_IsTextInputActive()) {
							SDL_StopTextInput();
						}
						game_window->state = STATE_PLAY;
						setup_level(&game_window->level_data, game_window->challenge_font_cache_id);
					}
				}
			}
		} break;
		default: {}
	}
}

// This translates from SDL events into our Input structure
static void handle_inputs(void) {
	game_window->input.character = 0;
	game_window->input.kbd_input.key = KEY_NONE;
	int num_key_states = 0;
	const u8* keystates = SDL_GetKeyboardState(&num_key_states);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT: {
				game_window->quit = true;
				return;
			} break;
			case SDL_MOUSEMOTION: {
				game_window->input.mouse_input.posX = event.motion.x;
				game_window->input.mouse_input.posY = event.motion.y;
				return;
			} break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP: {
				if (event.button.button == SDL_BUTTON_LEFT) {
					game_window->input.mouse_input.left_was_down = (event.type == SDL_MOUSEBUTTONUP);
					game_window->input.mouse_input.left_is_down = (event.type == SDL_MOUSEBUTTONDOWN);
				} else if (event.button.button == SDL_BUTTON_RIGHT) {
			        game_window->input.mouse_input.right_was_down = (event.type == SDL_MOUSEBUTTONUP);
					game_window->input.mouse_input.right_is_down = (event.type == SDL_MOUSEBUTTONDOWN);
				}
				return;
			} break;
			case SDL_KEYDOWN: 
			case SDL_KEYUP: {
				int scancode = event.key.keysym.scancode;
				switch (scancode) {
					case SDL_SCANCODE_UP: {
						game_window->input.kbd_input.key = KEY_UP;
					} break;
					case SDL_SCANCODE_LEFT: {
						game_window->input.kbd_input.key = KEY_LEFT;
					} break;
					case SDL_SCANCODE_DOWN: {
						game_window->input.kbd_input.key = KEY_DOWN;
					} break;
					case SDL_SCANCODE_RIGHT: {
						game_window->input.kbd_input.key = KEY_RIGHT;
					} break;
					case SDL_SCANCODE_ESCAPE: {
						game_window->input.kbd_input.key = KEY_ESCAPE;
					} break;
					case SDL_SCANCODE_RETURN: {
						game_window->input.kbd_input.key = KEY_RETURN;
					} break;
					case SDL_SCANCODE_SPACE: {
						game_window->input.kbd_input.key = KEY_SPACE;
					} break;
					case SDL_SCANCODE_F: {
						game_window->input.kbd_input.key = KEY_F;
					} break;
					case SDL_SCANCODE_Q: {
						game_window->input.kbd_input.key = KEY_Q;
					} break;
					default: {
						return;
					} break;
				}
				game_window->input.kbd_input.was_down = (event.type == SDL_KEYUP);
				game_window->input.kbd_input.is_down = (event.type == SDL_KEYDOWN);
				game_window->input.kbd_input.repeat = event.key.repeat;
				game_window->input.kbd_input.mods_held = 0;
				int num_sdl_keys = 0;
				const u8* sdl_keys = SDL_GetKeyboardState(&num_sdl_keys);
				if (sdl_keys[SDL_SCANCODE_LCTRL]) {
					game_window->input.kbd_input.mods_held |= MODIFIER_CTRL;
				}
				if (sdl_keys[SDL_SCANCODE_LALT]) {
					game_window->input.kbd_input.mods_held |= MODIFIER_ALT;
				}
			} break;
			case SDL_TEXTINPUT: {
				if (SDL_IsTextInputActive()) {
					assert(strlen(event.text.text) == 1);
					game_window->input.character = event.text.text[0];
				} else {
					return;
				}
			} break;
			default: {
				return;
			} break;
		}
		
		game_handle_input();
	}
}

static void center_rect_horizontally(rectangle2* rect) {
	float width = rect_width(rect);
	float x_offset = ((float)game_window->window_width / 2.0f) - (width / 2.0f);
	rect->min.x = x_offset;
	rect->max.x = x_offset + width;
}

static void center_rect_vertically(rectangle2* rect) {
	float height = rect_height(rect);
	float y_offset = ((float)game_window->window_height / 2.0f) - (height / 2.0f);
	rect->min.y = y_offset;
	rect->max.y = y_offset + height;
}

static void render_text_group(Text_Group* group) {
	Command_Buffer* buffer = &game_window->command_buffer;
	Glyph_Cache* font_cache = &game_window->font.glyph_caches[group->font_cache_id];
	u32 shader_id = game_window->quad_shader_id;
	Shader* shader = &game_window->shaders[shader_id];
	Render_Command* command = push_render_command(buffer);
	command->type = COMMAND_QUAD_GROUP;
	command->shader_id = shader_id;
	command->render_settings = RENDER_ALPHA_BLENDED;
	PUSH_UNIFORM_I32(&buffer->memory, command, shader->settings_loc, SHADER_SAMPLE_FONT_TEXTURE);
	PUSH_UNIFORM_I32(&buffer->memory, command, shader->font_texture_loc, shader->font_sampler_idx);
	PUSH_TEXTURE(&buffer->memory, command, shader->font_sampler_idx, font_cache->texture_id);
	Color color = white;
	PUSH_UNIFORM_VEC4(&buffer->memory, command, shader->color_loc, color);
	PUSH_UNIFORM_VEC2(&buffer->memory, command, shader->position_offset_loc, group->bounding_box.min);
	PUSH_UNIFORM_MATRIX(&buffer->memory, command, shader->ortho_loc, game_window->ortho_matrix);
	command->data.quad_group.num_quads = group->num_glyphs;
	command->data.quad_group.quads = group->quads;
}

static void render_summoning_sign(vec2 pos_offset) {
	Command_Buffer* buffer = &game_window->command_buffer;
	Glyph_Cache* font_cache = &game_window->font.glyph_caches[game_window->demonic_font_cache_id];
	u32 shader_id = game_window->quad_shader_id;
	Shader* shader = &game_window->shaders[shader_id];
	Render_Command* command = push_render_command(buffer);
	command->type = COMMAND_QUAD;
	command->shader_id = shader_id;
	command->render_settings = RENDER_ALPHA_BLENDED;
	PUSH_UNIFORM_I32(&buffer->memory, command, shader->settings_loc, SHADER_SAMPLE_BITMAP_TEXTURE);
	PUSH_UNIFORM_I32(&buffer->memory, command, shader->font_texture_loc, shader->font_sampler_idx);
	PUSH_TEXTURE(&buffer->memory, command, shader->font_sampler_idx, game_window->summoning_sign_tex_id);
	PUSH_UNIFORM_VEC2(&buffer->memory, command, shader->position_offset_loc, pos_offset);
	PUSH_UNIFORM_MATRIX(&buffer->memory, command, shader->ortho_loc, game_window->ortho_matrix);
	rectangle2 pos = rect_min_dim((vec2){0.0f, 0.0f}, (vec2){256.0f, 256.0f});
	rectangle2 tex = rect_min_dim((vec2){0.0f, 0.0f}, (vec2){1.0f, 1.0f}); 
	setup_textured_quad(&command->data.quad, pos, 0.0f, tex);
}

static void render_challenge(Game_Window* game_window, Type_Challenge* challenge) {
	vec3 position = {0.0f, 0.0f, 0.4f};
	Command_Buffer* buffer = &game_window->command_buffer;
	Glyph_Cache* font_cache = &game_window->font.glyph_caches[challenge->text_group.font_cache_id];
	u32 shader_id = game_window->quad_shader_id;
	Shader* shader = &game_window->shaders[shader_id];
	float cursor_height = font_cache->ascent + font_cache->descent;
	for (int i = 0; i < challenge->text_group.text.length; i++) {
		Quad* quad = &challenge->text_group.quads[i];
		rectangle2* glyph_bounding_box = &challenge->text_group.glyph_bounding_boxes[i];
		char character = challenge->text_group.text.str[i];
		assert((character >= 32) && (character <= 126));
		Glyph* glyph = &font_cache->glyphs[GLYPH_INDEX(character)];
		
		Color color = white;
		if (challenge->position > i) {
			if (challenge->typed_correctly[i]) { // correct
				color = green;
			} else { // incorrect
				color = red;
			}
		} else if (challenge->position == i) {
			color = very_dark_blue;
			Color cursor_color = amber;
			cursor_color.a = challenge->alpha;
			float glyph_width = glyph->width;
			vec2 dimensions = {glyph_width, cursor_height};
			rectangle2 cursor_rect = rect_min_dim((vec2){glyph_bounding_box->min.x, 0.0f}, dimensions);
			vec2 origin = rect_centre(&cursor_rect);
			vec2_add(&origin, challenge->text_group.bounding_box.min);
			
			float radius = glyph_width / 4.0f;
			Render_Command* cursor_cmd = fill_rounded_rect(buffer, shader_id, cursor_rect, cursor_color, 0.3f, radius);
			PUSH_UNIFORM_I32(&buffer->memory, cursor_cmd, shader->settings_loc, SHADER_ROUNDED_RECT);
			PUSH_UNIFORM_VEC2(&buffer->memory, cursor_cmd, shader->position_offset_loc, challenge->text_group.bounding_box.min);
			PUSH_UNIFORM_MATRIX(&buffer->memory, cursor_cmd, shader->ortho_loc, game_window->ortho_matrix);
			PUSH_UNIFORM_FLOAT(&buffer->memory, cursor_cmd, shader->radius_loc, radius);
			PUSH_UNIFORM_VEC2(&buffer->memory, cursor_cmd, shader->dimensions_loc, dimensions);
			PUSH_UNIFORM_VEC2(&buffer->memory, cursor_cmd, shader->origin_loc, origin);
		}
		
		Render_Command* command = push_render_command(buffer);
		command->type = COMMAND_QUAD;
		command->shader_id = shader_id;
		command->render_settings = RENDER_ALPHA_BLENDED;
		color.a = challenge->alpha;
		PUSH_UNIFORM_I32(&buffer->memory, command, shader->settings_loc, SHADER_SAMPLE_FONT_TEXTURE);
		PUSH_UNIFORM_I32(&buffer->memory, command, shader->font_texture_loc, shader->font_sampler_idx);
		PUSH_UNIFORM_VEC4(&buffer->memory, command, shader->color_loc, color);
		PUSH_UNIFORM_VEC2(&buffer->memory, command, shader->position_offset_loc, challenge->text_group.bounding_box.min);
		PUSH_UNIFORM_MATRIX(&buffer->memory, command, shader->ortho_loc, game_window->ortho_matrix);
		PUSH_TEXTURE(&buffer->memory, command, shader->font_sampler_idx, font_cache->texture_id);
		memcpy(&command->data.quad, quad, sizeof(Quad));
	}
}

static void render_win() {
	center_rect_horizontally(&game_window->win_text_group.bounding_box);
	center_rect_vertically(&game_window->win_text_group.bounding_box);
	render_text_group(&game_window->win_text_group);
}

static void render_lose() {
	center_rect_horizontally(&game_window->lose_text_group.bounding_box);
	center_rect_vertically(&game_window->lose_text_group.bounding_box);
	render_text_group(&game_window->lose_text_group);
}

static void render_game(void) {
	Type_Challenge* challenge = &game_window->level_data.challenges[game_window->level_data.current_challenge];
	update_challenge_alpha(challenge, game_window->dt);
	center_rect_horizontally(&challenge->text_group.bounding_box);
	center_rect_vertically(&challenge->text_group.bounding_box);
	render_challenge(game_window, challenge);
}

static void render_menu(void) {
	update_challenge_alpha(&game_window->title_challenge, game_window->dt);
	center_rect_horizontally(&game_window->title_challenge.text_group.bounding_box);
	center_rect_vertically(&game_window->title_challenge.text_group.bounding_box);
	render_challenge(game_window, &game_window->title_challenge);
	rectangle2 sign_rect = rect_min_dim((vec2){0.0f, 0.0f}, (vec2){256.0f, 256.0f});
	center_rect_horizontally(&sign_rect);
	rect_add_vec2(&sign_rect, (vec2){0.5f, game_window->title_challenge.text_group.bounding_box.max.y + 10.5f});
	render_summoning_sign(sign_rect.min);
}

static void update_and_render(void) {
	Render_Command* clear = push_render_command(&game_window->command_buffer);
	clear->type = COMMAND_CLEAR;
	clear->data.clear_command.clear_color = true;
	clear->data.clear_command.color = very_dark_blue;
	
	switch (game_window->state) {
		case STATE_MENU: {
			//SDL_Rect dest_rect;
			//SDL_Texture* texture = render_demonic_sign_to_texture(words[1], &dest_rect);
			//center_rect_veritcally(&dest_rect);
			//center_rect_horizontally(&dest_rect);
			//SDL_RenderCopy(game_window->renderer, game_window->demonic_word_textures[game_window->demonic_word_i].texture, NULL, &(game_window->demonic_word_textures[game_window->demonic_word_i].rect));
			render_menu();
		} break;
		case STATE_PLAY: {
			render_game();
		} break;
		case STATE_WIN: {
			render_win();
		} break;
		case STATE_LOSE: {
			render_lose();
		} break;
		default: {}
	}
	
	exec_command_buffer();
	SDL_GL_SwapWindow(game_window->window);
}

static void main_loop(void) {
	if (game_window->quit) {
#ifdef __EMSCRIPTEN__
		emscripten_cancel_main_loop();
#else
		SDL_DestroyWindow(game_window->window);
		exit(0);
#endif
	}
	
	uint64_t this_frame_perf_counter = SDL_GetPerformanceCounter();
	game_window->dt = (float)(this_frame_perf_counter - game_window->last_frame_perf_counter)/(float)SDL_GetPerformanceFrequency();
	//char buffer[1024] = {0};
	//sprintf(&buffer[0], "Summoning. dt = %.5f | demonic_i = %d", game_window->dt, game_window->demonic_word_i);
	//SDL_SetWindowTitle(game_window->window, &buffer[0]);
	game_window->frame_number++;
	
	handle_inputs();
	update_and_render();
	
#ifndef __EMSCRIPTEN__
	uint64_t delta = game_window->dt * 1000.0f;
	if (delta < fps_cap_in_ms) {
		SDL_Delay(fps_cap_in_ms - delta);
	}
#endif
	
	game_window->last_frame_perf_counter = this_frame_perf_counter;
}

int main(int argc, char** argv) {
#ifdef UNIX
    // raw unbuffered output mode so we can debug without printing newlines
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif
	assert(sizeof(Vertex) == (sizeof(vec3)+sizeof(vec2))); // checking for no padding
	
	init_global_file_resources();
	init_the_game();
	
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(main_loop, 0, 1);
#else
	fps_cap_in_ms = 1000.0f / fps_cap;
	while (!game_window->quit) { main_loop(); }
#endif
	
#if defined(DEBUG) && !defined(DEBUG_STDOUT)
	fclose(debug_file);
#endif
	
#ifdef UNIX
    printf("\n");
#endif
	return 0;
}

#ifdef _WIN32
#include <windows.h>
int CALLBACK
WinMain(HINSTANCE Instance,
		HINSTANCE PrevInstance,
		LPSTR CommandLine,
		int ShowCode)  {
	return main(0, NULL);
}

int CALLBACK
wmain(int argc, wchar_t** argv) {
	return main(0, NULL);
}
#endif
