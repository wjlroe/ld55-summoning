#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2_gfxPrimitives.c>
#include <SDL2_rotozoom.c>

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

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  i64;
typedef int32_t  i32;
typedef int16_t  i16;
typedef int8_t   i8;

typedef union vec2 {
	struct { float x, y; };
	float values[2];
} vec2;

typedef union vec3 {
	struct { float x, y, z; };
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

typedef vec4 Color;

#define COLOR(v) (float)v/255.0f
#define RGBA(r,g,b,a) {COLOR(r),COLOR(g),COLOR(b),COLOR(a)} 

typedef struct Vertex {
	vec3 position;
	vec2 texture;
	vec4 color;
} Vertex;

typedef struct Quad {
	Vertex vertices[4];
} Quad;

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

static void render_draw_color(SDL_Renderer* renderer, SDL_Color color) {
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void texture_color_mod(SDL_Texture* texture, SDL_Color color) {
	SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
}

static void fill_rounded_rect(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color, int radius) {
	roundedBoxRGBA(renderer, rect.x, rect.y, rect.x+rect.w, rect.y+rect.h, radius, color.r, color.g, color.b, color.a);
}

typedef struct Spritesheet {
	int rows, cols;
	int numFrames, frame;
	float dtPerFrame, dtThisFrame;
	int sheet_width, sheet_height;
	SDL_Texture *texture;
	SDL_Rect source_rect;
	SDL_Rect dest_rect;
} Spritesheet;

static void set_spritesheet_source_rect(Spritesheet* sprite) {
	int current_col = sprite->frame % sprite->cols;
	sprite->source_rect.x = current_col * sprite->source_rect.w;
	int current_row = sprite->frame / sprite->cols;
	sprite->source_rect.y = current_row * sprite->source_rect.h;
}

static Spritesheet new_spritesheet(SDL_Texture *texture, int tex_width, int tex_height, int rows, int cols) {
	Spritesheet sheet = {0};
	sheet.texture = texture;
	sheet.rows = rows;
	sheet.cols = cols;
	sheet.numFrames = rows * cols;
	sheet.sheet_width = tex_width;
	sheet.sheet_height = tex_height;
	sheet.source_rect.w = sheet.sheet_width / sheet.cols;
	sheet.source_rect.h = sheet.sheet_height / sheet.rows;
	set_spritesheet_source_rect(&sheet);
	return sheet;
}

static void update_sprite_animation(Spritesheet* sprite, float dt) {
	sprite->dtThisFrame += dt;
	float dtDiff = sprite->dtThisFrame - sprite->dtPerFrame;
	if (dtDiff >= 0.0f) {
		sprite->frame++;
		sprite->frame %= sprite->numFrames;
		sprite->dtThisFrame = fmodf(dtDiff, sprite->dtPerFrame);
		set_spritesheet_source_rect(sprite);
	}
}

typedef enum Game_State {
	STATE_MENU,
	STATE_PLAY,
	STATE_PAUSE,
	STATE_WIN,
	STATE_LOSE,
} Game_State;

typedef enum Key {
	KEY_NONE,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_ESCAPE,
	KEY_RETURN,
	KEY_SPACE
} Key;

// TODO: This can only handle one key pressed at a time!
typedef struct Keyboard_Input {
	bool is_down, was_down, repeat;
	Key key;
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
	
	// FIXME: SDL stuff, obsolete
	SDL_Rect bounding_box;
	int minx;
	int maxx;
	int miny;
	int maxy;
	//int advance;
	SDL_Texture* texture;
	// FIXME: end of SDL stuff
} Glyph;

#if 0
static Glyph new_glyph(SDL_Renderer* renderer, TTF_Font* font, char character) {
	Glyph glyph = {0};
	glyph.character = character;
	TTF_GlyphMetrics(font, character,
					 &glyph.minx, &glyph.maxx,
					 &glyph.miny, &glyph.maxy, &glyph.advance);
	SDL_Surface* surface = TTF_RenderGlyph_Solid(font, character, white);
	glyph.bounding_box.w = surface->w;
	glyph.bounding_box.h = surface->h;
	glyph.texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	return glyph;
}
#endif

#define NUM_GLYPHS 95 //126-32 (inclusive)
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

typedef struct Type_Challenge {
	int font_cache_id;
	String text;
	bool* typed_correctly; // This could be tighter packed, but we don't care right now :)
	SDL_Rect* cursor_rects;
	int position;
	SDL_Rect bounding_box;
	float alpha;
	float alpha_fade_speed;
} Type_Challenge;

static void setup_challenge(Type_Challenge* challenge, int font_cache_id, String text) {
	challenge->text = text;
	challenge->font_cache_id = font_cache_id;
	challenge->typed_correctly = calloc(text.length, sizeof(bool));
	challenge->cursor_rects = calloc(text.length, sizeof(SDL_Rect));
	
	#if 0
	TTF_SizeText(font->font, text.str, &challenge->bounding_box.w, &challenge->bounding_box.h);
	
	int font_ascent = TTF_FontAscent(font->font);
	int font_descent = TTF_FontDescent(font->font);
	int cursor_height = font_ascent + font_descent;
	int x = 0;
	for (int i = 0; i < challenge->text.length; i++) {
		char character = challenge->text.str[i];
		assert((character >= 32) && (character <= 126));
		Glyph* glyph = &font->glyph_cache.glyphs[GLYPH_INDEX(character)];
		SDL_Rect* rect = &challenge->cursor_rects[i];
		rect->w = glyph->bounding_box.w;
		rect->h = cursor_height;
		rect->x = glyph->bounding_box.x + x;
		rect->y = glyph->bounding_box.y;
		
		x += glyph->advance;
	}
	#endif
}

static void free_challenge(Type_Challenge* challenge) {
	free(challenge->typed_correctly);
	free(challenge->cursor_rects);
}

static bool is_challenge_done(Type_Challenge* challenge) {
	return (challenge->position >= challenge->text.length);
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
		challenge->typed_correctly[challenge->position] = (character == challenge->text.str[challenge->position]);
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
	if (challenge->alpha < 1.0f) {
		if (has_typing_started(challenge) || (challenge->alpha_fade_speed == 0.0f)) {
			// speed up fading in if the typing has started
			challenge->alpha += dt;
		} else {
			challenge->alpha += dt / challenge->alpha_fade_speed;
		}
		challenge->alpha = (challenge->alpha > 1.0) ? 1.0 : challenge->alpha;
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
	SDL_Rect positions[MAX_NUM_CHALLENGES];
	int num_challenges;
	int current_challenge;
	int level_number;
	int points;
} Level_Data;

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

typedef struct Shader {
	int program;
	
	// attributes
	int position_loc;
	int texture_loc;
	int in_color_loc;
	
	// uniforms
	int position_offset_loc;
	int ortho_loc;
	int settings_loc;
	
	// textures
	int font_texture_loc;
	int font_sampler_idx;
} Shader;

typedef struct Game_Window {
	bool quit; // zero-init means quit=false by default
	int window_width, window_height;
	SDL_Window* window;
	//SDL_Renderer* renderer;
	SDL_GLContext gl_context;
	Shader shader;
	GLuint vao;
	GLuint vbo;
	GLuint ebo;
	float ortho_matrix[4][4];
	Font font;
	int title_font_cache_id;
	int challenge_font_cache_id;
	int demonic_font_cache_id;
	uint64_t last_frame_perf_counter;
	float dt;
	int frame_number;

	Game_State state;
	Type_Challenge title_challenge; // initial title page challenge
	
	Input input;
	
	Level_Data level_data;
	
	Sized_Texture* demonic_word_textures;
	int demonic_word_i;
	
	Sized_Texture win_text;
	Sized_Texture lose_text;
	// TODO: points text
} Game_Window;

static Game_Window* game_window = NULL;

static bool key_first_down() {
	return (game_window->input.kbd_input.is_down) && !(game_window->input.kbd_input.was_down);
}

static bool key_is_down(Key key) {
	return (game_window->input.kbd_input.key == key) && (game_window->input.kbd_input.is_down);
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

#if 0
static SDL_Texture* render_demonic_sign_to_texture(String word, SDL_Rect* area) {
	area->w = area->h = 256;
	SDL_Surface* surface = SDL_CreateRGBSurface(0, 256, 256, 32, 0xff000000, 0x00ff0000, 0x000000ff00, 0x000000ff);
	assert(surface->format->BitsPerPixel == 32);
	u32 my_green = 0x00ff00ff;
	draw_circle(surface->pixels, 127, my_green);
	int inner_radius = 95;
	draw_circle(surface->pixels, inner_radius, my_green);
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
	
	SDL_Texture* texture = SDL_CreateTextureFromSurface(game_window->renderer, surface);
	assert(texture != NULL);
	if (!rendered_sign) {
		SDL_SaveBMP(surface, "output.bmp");
		rendered_sign = true;
	}
	SDL_FreeSurface(surface);
	return texture;
}
#endif

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
	// NOTE: do we have to memcpy from one multi-dimensional array to another?
	memcpy(&game_window->ortho_matrix, ortho_matrix, 4*4*sizeof(GLfloat));
}

static void render_gl_test(void) {
	Color background = very_dark_blue;
	glClearColor(background.r, background.g, background.b, background.a);
	glClear(GL_COLOR_BUFFER_BIT);
	
	glUseProgram(game_window->shader.program);
	
	glUniformMatrix4fv(game_window->shader.ortho_loc, 
					   1,
					   GL_FALSE,
					   &game_window->ortho_matrix[0][0]);
	
	float position_offset[] = {20.0f, 20.0f, 0.0f};
	glUniform3fv(game_window->shader.position_offset_loc, 1, position_offset);
	
	i32 settings = 1; // sample texture
	glUniform1iv(game_window->shader.settings_loc,
				 1,
				 &settings);
	
	glBindVertexArray(game_window->vao);
	
	Color color = white;
	Glyph_Cache* font_cache = &game_window->font.glyph_caches[0];
	
	glUniform1i(game_window->shader.font_texture_loc, game_window->shader.font_sampler_idx);
	glActiveTexture(GL_TEXTURE0 + game_window->shader.font_sampler_idx);
	glBindTexture(GL_TEXTURE_2D, font_cache->texture_id);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	Glyph* glyph = &font_cache->glyphs[GLYPH_INDEX('S')];
	float z = 0.5;
	int x = 0;
	int y = 0;
	int baseline = y + font_cache->ascent;
	int g_x0 = x + glyph->lsb;
	int g_x1 = g_x0 + glyph->width;
	int g_y0 = baseline + glyph->y0;
	int g_y1 = baseline + glyph->y1;
	rectangle2 pos = {.min={g_x0, g_y0}, .max={g_x1, g_y1}};
	rectangle2 tex = {.min={glyph->tex_x0, glyph->tex_y0}, .max={glyph->tex_x1, glyph->tex_y1}};
	Quad quad;
	quad.vertices[0] = (Vertex){.position={pos.min.x, pos.min.y, z}, .texture={tex.min.x, tex.min.y}, .color=color}; // top-left
	quad.vertices[1] = (Vertex){.position={pos.min.x, pos.max.y, z}, .texture={tex.min.x, tex.max.y}, .color=color}; // bottom-left
	quad.vertices[2] = (Vertex){.position={pos.max.x, pos.max.y, z}, .texture={tex.max.x, tex.max.y}, .color=color}; // bottom-right
	quad.vertices[3] = (Vertex){.position={pos.max.x, pos.min.y, z}, .texture={tex.max.x, tex.min.y}, .color=color}; // top-right
	
	int stride = sizeof(Vertex);
	
	glBindBuffer(GL_ARRAY_BUFFER, game_window->vbo);
	glBufferData(GL_ARRAY_BUFFER, 4 * stride, quad.vertices, GL_STATIC_DRAW);
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
	glBindVertexArray(0);
	glUseProgram(0);
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
		printf("loc[%d] name: %s\n", loc, name_buffer);
		if (strcmp(name_buffer, "position") == 0) {
			shader->position_loc = loc;
		} else if (strcmp(name_buffer, "texture") == 0) {
			shader->texture_loc = loc;
		} else if (strcmp(name_buffer, "in_color") == 0) {
			shader->in_color_loc = loc;
		}
	}
	
	int sampler_idx = 0;
	int active_uniforms = 0;
	glGetProgramiv(shader->program, GL_ACTIVE_UNIFORMS, &active_uniforms);
	
	for (int i = 0; i < active_uniforms; i++) {
		glGetActiveUniform(shader->program, i, 1024, &name_size, &attr_size, &attr_type, &name_buffer[0]);
		int loc = glGetUniformLocation(shader->program, name_buffer);
		assert(loc >= 0);
		printf("loc[%d] name: %s\n", loc, name_buffer);
		if (strcmp(name_buffer, "position_offset") == 0) {
			shader->position_offset_loc = loc;
		} else if (strcmp(name_buffer, "ortho") == 0) {
			shader->ortho_loc = loc;
		} else if (strcmp(name_buffer, "fontTexture") == 0) {
			shader->font_texture_loc = loc;
			shader->font_sampler_idx = sampler_idx;
		} else if (strcmp(name_buffer, "settings") == 0) {
			shader->settings_loc = loc;
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
		printf("Error in shader %s:\n%s\n", resource->filename, buffer);
		printf("\n======= shader source: ====== \n");
		printf("%s", resource->contents);
		printf("\n======= END OF SOURCE  ====== \n");
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
		printf("Error in program (%s):\n%s\n", desc, buffer);
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
	printf("gl version: %s\n", glGetString(GL_VERSION));
	printf("gl shading language version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	GLuint program_id = glCreateProgram();
	printf("program_id: %d\n", program_id);
	
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
	
	Shader shader = {0};
	shader.program = program_id;
	init_shader(&shader);
	
	game_window->shader = shader;
	
	int stride = 9 * sizeof(GLfloat);
	
	glGenVertexArrays(1, &game_window->vao);
	glBindVertexArray(game_window->vao);
	
	glGenBuffers(1, &game_window->vbo);
	glGenBuffers(1, &game_window->ebo);
	
	glBindBuffer(GL_ARRAY_BUFFER, game_window->vbo);
	glBufferData(GL_ARRAY_BUFFER, 4 * stride, NULL, GL_STATIC_DRAW);
	
	glEnableVertexAttribArray(game_window->shader.position_loc);
	glVertexAttribPointer(game_window->shader.position_loc, 3, GL_FLOAT, GL_FALSE, stride, NULL);
	glEnableVertexAttribArray(game_window->shader.texture_loc);
	glVertexAttribPointer(game_window->shader.texture_loc, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(3*sizeof(GLfloat)));
	glEnableVertexAttribArray(game_window->shader.in_color_loc);
	glVertexAttribPointer(game_window->shader.in_color_loc, 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(5*sizeof(GLfloat)));
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, game_window->ebo);
	GLuint indexData[] = { 0, 1, 2, 2, 3, 0 };
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLuint), indexData, GL_STATIC_DRAW);
	
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void init_the_game(void) {
	game_window = (Game_Window*)malloc(sizeof(Game_Window));
	memset(game_window, 0, sizeof(Game_Window));
	game_window->window_width = DEFAULT_WINDOW_WIDTH;
	game_window->window_height = DEFAULT_WINDOW_HEIGHT;
	game_window->last_frame_perf_counter = SDL_GetPerformanceCounter();
	SDL_Init(SDL_INIT_EVERYTHING);
	TTF_Init();
	game_window->window = SDL_CreateWindow("Ludum Dare 55: Summoning",
										   SDL_WINDOWPOS_UNDEFINED,
										   SDL_WINDOWPOS_UNDEFINED,
										   game_window->window_width, 
										   game_window->window_height, 
										   SDL_WINDOW_OPENGL);
	//game_window->renderer = SDL_CreateRenderer(game_window->window,
											   //-1, // initialize the first one supporting the requested flags
											   //SDL_RENDERER_PRESENTVSYNC);
	
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	game_window->gl_context = SDL_GL_CreateContext(game_window->window);
	if (game_window->gl_context == NULL) {
		const char* sdl_error = SDL_GetError();
		printf("failed to create GL context: %s\n", sdl_error);
	}
	assert(game_window->gl_context != NULL);
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
	
	game_window->font.resource = &global_file_resources[RES_ID(IM_FELL_FONT_ID)];
	init_font(&game_window->font);
	
	game_window->title_font_cache_id = push_font_size(&game_window->font, 120.0);
	game_window->challenge_font_cache_id = push_font_size(&game_window->font, 48.0);
	game_window->demonic_font_cache_id = push_font_size(&game_window->font, 24.0);
	
	//render_text_into_texture(game_window->renderer, &game_window->win_text, game_window->title_font.font, win);
	//render_text_into_texture(game_window->renderer, &game_window->lose_text, game_window->title_font.font, lost);
	
	#if 0
	setup_challenge(&game_window->title_challenge, game_window->title_font_cache_id, title);
	game_window->title_challenge.alpha_fade_speed = 10.0f; // slow for the title
	// SDL_StartTextInput(); // so we can type 'into' the initial challenge text
	
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
	switch(game_window->state) {
		case STATE_PLAY: {
			if (game_window->input.character != 0) {
				level_type_character(&game_window->level_data, game_window->input.character);
				if (is_level_done(&game_window->level_data)) {
					if (game_window->level_data.points < game_window->level_data.num_challenges) {
						game_window->state = STATE_LOSE;
					} else {
						game_window->state = STATE_WIN;
					}
				}
			} else if (key_is_down(KEY_ESCAPE) && key_first_down()) {
				reset_level(&game_window->level_data);
			}
		} break;
		case STATE_PAUSE: {
			if (key_is_down(KEY_ESCAPE) && key_first_down()) {
				game_window->state = STATE_PLAY;
			}
		} break;
		case STATE_MENU: {
			//if (key_is_down(KEY_SPACE) && key_first_down()) {
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
					default: {
						return;
					} break;
				}
				game_window->input.kbd_input.was_down = (event.type == SDL_KEYUP);
				game_window->input.kbd_input.is_down = (event.type == SDL_KEYDOWN);
				game_window->input.kbd_input.repeat = event.key.repeat;
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

static void play_screen_do_animation(void) {
	// update_sprite_animation(&game_window->runner1, game_window->dt);
	// update_sprite_animation(&game_window->runner2, game_window->dt);
}

static void title_screen_do_animation(void) {
	update_challenge_alpha(&game_window->title_challenge, game_window->dt);
}

static void center_rect_horizontally(SDL_Rect* rect) {
	rect->x = (game_window->window_width / 2) - (rect->w / 2);
}

static void center_rect_vertically(SDL_Rect* rect) {
	rect->y = (game_window->window_height / 2) - (rect->h / 2);
}

#if 0
static void render_challenge(Game_Window* game_window, Type_Challenge* challenge) {
	int x = 0;
	for (int i = 0; i < challenge->text.length; i++) {
		char character = challenge->text.str[i];
		assert((character >= 32) && (character <= 126));
		Glyph* glyph = &challenge->font->glyph_cache.glyphs[GLYPH_INDEX(character)];
		SDL_Texture* texture = glyph->texture;
		int alpha = challenge->alpha * 255;
		SDL_SetTextureAlphaMod(texture, alpha);
		if (challenge->position > i) {
			if (challenge->typed_correctly[i]) { // correct
				texture_color_mod(texture, green);
			} else { // incorrect
				texture_color_mod(texture, red);
			}
		} else if (challenge->position == i) {
			SDL_Rect cursor_rect = challenge->cursor_rects[i];
			cursor_rect.x += challenge->bounding_box.x;
			cursor_rect.y += challenge->bounding_box.y;
			fill_rounded_rect(game_window->renderer, cursor_rect, amber, cursor_rect.w/4);
			texture_color_mod(texture, very_dark_blue);
		} else {
			// untyped character - so keep original color
			texture_color_mod(texture, white);
		}
		SDL_Rect pos = glyph->bounding_box;
		pos.x += challenge->bounding_box.x + x;
		pos.y += challenge->bounding_box.y;
		
		SDL_RenderCopy(game_window->renderer, texture, NULL, &pos);
		x += glyph->advance;
	}
}

static void render_win() {
	center_rect_horizontally(&game_window->win_text.rect);
	center_rect_vertically(&game_window->win_text.rect);
	SDL_RenderCopy(game_window->renderer, game_window->win_text.texture, NULL, &game_window->win_text.rect);
}

static void render_lose() {
	center_rect_horizontally(&game_window->lose_text.rect);
	center_rect_vertically(&game_window->lose_text.rect);
	SDL_RenderCopy(game_window->renderer, game_window->lose_text.texture, NULL, &game_window->lose_text.rect);
}

static void render_menu(void) {
	center_rect_horizontally(&game_window->title_challenge.bounding_box);
	center_rect_vertically(&game_window->title_challenge.bounding_box);
	render_challenge(game_window, &game_window->title_challenge);
}

static void render_game(void) {
	Type_Challenge* challenge = &game_window->level_data.challenges[game_window->level_data.current_challenge];
	update_challenge_alpha(challenge, game_window->dt);
	center_rect_horizontally(&challenge->bounding_box);
	center_rect_veritcally(&challenge->bounding_box);
	render_challenge(game_window, challenge);
}
#endif

static void update_and_render(void) {
	//render_draw_color(game_window->renderer, very_dark_blue);
	//SDL_RenderClear(game_window->renderer);
	
	render_gl_test();
	
#if 0
	switch (game_window->state) {
		case STATE_MENU: {
			//SDL_Rect dest_rect;
			//SDL_Texture* texture = render_demonic_sign_to_texture(words[1], &dest_rect);
			//center_rect_veritcally(&dest_rect);
			//center_rect_horizontally(&dest_rect);
			//SDL_RenderCopy(game_window->renderer, game_window->demonic_word_textures[game_window->demonic_word_i].texture, NULL, &(game_window->demonic_word_textures[game_window->demonic_word_i].rect));
			title_screen_do_animation();
			render_menu();
		} break;
		case STATE_PLAY: {
			play_screen_do_animation();
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
#endif
	
	SDL_GL_SwapWindow(game_window->window);
	//SDL_RenderPresent(game_window->renderer);
}

static void main_loop(void) {
	if (game_window->quit) {
		#ifdef __EMSCRIPTEN__
		emscripten_cancel_main_loop();
		#else
		//SDL_DestroyRenderer(game_window->renderer);
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
	printf("size of Vertex: %zu\n", sizeof(Vertex));
	printf("size of 3+2+4 floats: %zu\n", (3+2+4)*sizeof(float));
	assert(sizeof(Vertex) == (3+2+4)*sizeof(float)); // checking for no padding
	
	init_global_file_resources();
	init_the_game();

	#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(main_loop, 0, 1);
	#else
	fps_cap_in_ms = 1000.0f / fps_cap;
	while (true) { main_loop(); }
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
