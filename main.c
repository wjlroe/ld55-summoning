#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2_gfxPrimitives.c>
#include <SDL2_rotozoom.c>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

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

static String words[] = {
	STRING("summon"),
	STRING("incantation"),
	STRING("spell"),
	STRING("awaken"),
	STRING("cauldron"),
};

SDL_Color white = {255, 255, 255, 255};
SDL_Color blue = {10, 10, 255, 255};
SDL_Color red = {255, 10, 10, 255};
SDL_Color very_dark_blue = {4, 8, 13, 255};
SDL_Color green = {10, 255, 10};
SDL_Color amber = {255, 191, 0, 255};

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
	SDL_Rect bounding_box;
	int minx;
	int maxx;
	int miny;
	int maxy;
	int advance;
	SDL_Texture* texture;
} Glyph;

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

#define NUM_GLYPHS 94 //126-32
#define GLYPH_INDEX(c) c-32

typedef struct Glyph_Cache {
	Glyph glyphs[NUM_GLYPHS];
	int first_glyph;
	int last_glyph;
} Glyph_Cache;

typedef struct Font {
	TTF_Font* font;
	Glyph_Cache glyph_cache;
} Font;

typedef struct Type_Challenge {
	Font* font;
	String text;
	bool* typed_correctly; // This could be tighter packed, but we don't care right now :)
	SDL_Rect* cursor_rects;
	int position;
	SDL_Rect bounding_box;
	float alpha;
	float alpha_fade_speed;
} Type_Challenge;

static void setup_challenge(Type_Challenge* challenge, Font* font, String text) {
	challenge->text = text;
	challenge->font = font;
	challenge->typed_correctly = calloc(text.length, sizeof(bool));
	challenge->cursor_rects = calloc(text.length, sizeof(SDL_Rect));
	
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

static void setup_level(Level_Data* level, Font* font) {
	for (int i = 0; i < ARRAY_LEN(words); i++) {
		if (i >= MAX_NUM_CHALLENGES) { break; }
		setup_challenge(&level->challenges[i], font, words[i]);
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
		// TODO: score points!!!
		level->current_challenge++;
	}
	if (is_level_done(level)) {
		SDL_StopTextInput();
	}
}

typedef struct Sized_Texture {
	SDL_Texture* texture;
	SDL_Rect rect;
} Sized_Texture;

typedef struct Game_Window {
	bool quit; // zero-init means quit=false by default
	int window_width, window_height;
	SDL_Window* window;
	SDL_Renderer* renderer;
	Font title_font;
	Font challenge_font;
	Font demonic_font;
	uint64_t last_frame_perf_counter;
	float dt;
	int frame_number;

	SDL_Rect rect2;
	SDL_Texture* runner_texture;
	Spritesheet runner1, runner2;
	
	Game_State state;
	Type_Challenge title_challenge; // initial title page challenge
	
	Input input;
	
	Level_Data level_data;
	
	Sized_Texture* demonic_word_textures;
	int demonic_word_i;
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

static bool setup_font(Game_Window* game_window, Font* font, char* filename, int size) {
	TTF_Font* sdl_font = TTF_OpenFont(filename, size);
	if (sdl_font == NULL) {
		const char* error = SDL_GetError();
#ifndef __EMSCRIPTEN__
		char buffer[1024] = {0};
		sprintf(&buffer[0], "Error loading a font: %s\n", error);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
								 "Error",
								 &buffer[0],
								 game_window->window);
#endif
		return false;
	}
	font->font = sdl_font;
	font->glyph_cache.first_glyph = 32;
	font->glyph_cache.last_glyph = 126;
	for (int i = 0; i < NUM_GLYPHS; i++) {
		font->glyph_cache.glyphs[i] = new_glyph(game_window->renderer, sdl_font, i+32);
	}
	return true;
}

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

static void draw_line(void* pixel_data, float x0, float y0, float x1, float y1, u32 color) {
	u32 (*pixels)[256] = pixel_data;
	bool steep = abs(y1 - y0) > abs(x1 - x0);
	
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
#if 0
	if (!rendered_sign) {
		SDL_SaveBMP(surface, "output.bmp");
		rendered_sign = true;
	}
#endif
	SDL_FreeSurface(surface);
	return texture;
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
	game_window->renderer = SDL_CreateRenderer(game_window->window,
											   -1, // initialize the first one supporting the requested flags
											   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	
	SDL_Surface *spritesheet_surface = IMG_Load("./assets/spritesheet.png");
	game_window->runner_texture = SDL_CreateTextureFromSurface(game_window->renderer, spritesheet_surface);
	SDL_FreeSurface(spritesheet_surface);
	
	char* font_filename = "./assets/fonts/im_fell_roman.ttf";
	if (!setup_font(game_window, &game_window->title_font, font_filename, 120)) {
		abort();
		return;
	}
	if (!setup_font(game_window, &game_window->challenge_font, font_filename, 48)) {
		abort();
		return;
	}
	if (!setup_font(game_window, &game_window->demonic_font, font_filename, 24)) {
		abort();
		return;
	}

	{
		game_window->runner1 = new_spritesheet(game_window->runner_texture, 500, 500, 4, 4);
		game_window->runner1.dtPerFrame = 0.05;
		game_window->runner1.dest_rect.w = 100;
		game_window->runner1.dest_rect.h = 100;
		game_window->runner1.dest_rect.x = game_window->window_width - game_window->runner1.dest_rect.w;
		game_window->runner1.dest_rect.y = game_window->window_height - game_window->runner1.dest_rect.h;
	}
	{
		game_window->runner2 = new_spritesheet(game_window->runner_texture, 500, 500, 4, 4);
		game_window->runner2.dtPerFrame = 0.03;
		game_window->runner2.dest_rect.w = 150;
		game_window->runner2.dest_rect.h = 150;
		game_window->runner2.dest_rect.x = game_window->window_width - game_window->runner1.dest_rect.w - game_window->runner2.dest_rect.w;
		game_window->runner2.dest_rect.y = game_window->window_height - game_window->runner2.dest_rect.h;
	}

	game_window->rect2.x = 100;
	game_window->rect2.y = 20;
	game_window->rect2.w = 300;
	game_window->rect2.h = 300;
	
	setup_challenge(&game_window->title_challenge, &game_window->title_font, title);
	game_window->title_challenge.alpha_fade_speed = 10.0f; // slow for the title
	// SDL_StartTextInput(); // so we can type 'into' the initial challenge text
	
	game_window->demonic_word_textures = calloc(ARRAY_LEN(words), sizeof(Sized_Texture));
	for (int i = 0; i < ARRAY_LEN(words); i++) {
		game_window->demonic_word_textures[i].texture = render_demonic_sign_to_texture(words[i], &(game_window->demonic_word_textures[i].rect));
	}
	
	if (SDL_IsTextInputActive()) {
		SDL_StopTextInput();
	}
}

static void game_handle_input(void) {
	switch(game_window->state) {
		case STATE_PLAY: {
			if (game_window->input.character != 0) {
				level_type_character(&game_window->level_data, game_window->input.character);
				// TODO: if level is done!
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
			if (key_is_down(KEY_SPACE) && key_first_down()) {
				game_window->demonic_word_i++;
				game_window->demonic_word_i %= ARRAY_LEN(words);
				return;
			}
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
						setup_level(&game_window->level_data, &game_window->challenge_font);
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

static void center_rect_veritcally(SDL_Rect* rect) {
	rect->y = (game_window->window_height / 2) - (rect->h / 2);
}

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

static void render_menu(void) {
	center_rect_horizontally(&game_window->title_challenge.bounding_box);
	center_rect_veritcally(&game_window->title_challenge.bounding_box);
	render_challenge(game_window, &game_window->title_challenge);
}

static void render_game(void) {
	Type_Challenge* challenge = &game_window->level_data.challenges[game_window->level_data.current_challenge];
	update_challenge_alpha(challenge, game_window->dt);
	center_rect_horizontally(&challenge->bounding_box);
	center_rect_veritcally(&challenge->bounding_box);
	render_challenge(game_window, challenge);
}

static void update_and_render(void) {
	render_draw_color(game_window->renderer, very_dark_blue);
	SDL_RenderClear(game_window->renderer);
	
	switch (game_window->state) {
		case STATE_MENU: {
			//SDL_Rect dest_rect;
			//SDL_Texture* texture = render_demonic_sign_to_texture(words[1], &dest_rect);
			//center_rect_veritcally(&dest_rect);
			//center_rect_horizontally(&dest_rect);
			SDL_RenderCopy(game_window->renderer, game_window->demonic_word_textures[game_window->demonic_word_i].texture, NULL, &(game_window->demonic_word_textures[game_window->demonic_word_i].rect));
			//title_screen_do_animation();
			//render_menu();
		} break;
		case STATE_PLAY: {
			play_screen_do_animation();
			render_game();
		} break;
		default: {}
	}
	
	SDL_RenderPresent(game_window->renderer);
}

static void main_loop(void) {
	if (game_window->quit) {
		#ifdef __EMSCRIPTEN__
		emscripten_cancel_main_loop();
		#else
		SDL_DestroyRenderer(game_window->renderer);
		SDL_DestroyWindow(game_window->window);
		exit(0);
		#endif
	}

	uint64_t this_frame_perf_counter = SDL_GetPerformanceCounter();
	game_window->dt = (float)(this_frame_perf_counter - game_window->last_frame_perf_counter)/(float)SDL_GetPerformanceFrequency();
	char buffer[1024] = {0};
	sprintf(&buffer[0], "Summoning. dt = %.5f | demonic_i = %d", game_window->dt, game_window->demonic_word_i);
	SDL_SetWindowTitle(game_window->window, &buffer[0]);
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
