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

static String words[] = {
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
} Key;

// TODO: This can only handle one key pressed at a time!
typedef struct Keyboard_Input {
	bool is_down, was_down;
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

static void enter_challenge_character(Type_Challenge* challenge, char character) {
	if (!is_challenge_done(challenge)) {
		challenge->typed_correctly[challenge->position] = (character == challenge->text.str[challenge->position]);
		challenge->position++;
	}
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

typedef struct Game_Window {
	bool quit; // zero-init means quit=false by default
	int window_width, window_height;
	SDL_Window* window;
	SDL_Renderer* renderer;
	Font title_font;
	Font challenge_font;
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
} Game_Window;

static Game_Window* game_window = NULL;

static bool key_first_down() {
	return (game_window->input.kbd_input.is_down) && !(game_window->input.kbd_input.was_down);
}

static bool key_is_down(Key key) {
	return (game_window->input.kbd_input.key == key) && (game_window->input.kbd_input.is_down);
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
	
	String challenge_text = new_string("Summoning");
	setup_challenge(&game_window->title_challenge, &game_window->title_font, challenge_text);
	game_window->title_challenge.alpha_fade_speed = 10.0f; // slow for the title
	SDL_StartTextInput(); // so we can type 'into' the initial challenge text
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
			if (game_window->input.character != 0) {
				enter_challenge_character(&game_window->title_challenge, game_window->input.character);
				if (is_challenge_done(&game_window->title_challenge)) {
					if (challenge_has_mistakes(&game_window->title_challenge)) {
						// TODO: show a message to indicate why this resets
						reset_challenge(&game_window->title_challenge);
					} else {
						SDL_StopTextInput();
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
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT: {
				game_window->quit = true;
			} break;
			case SDL_MOUSEMOTION: {
				game_window->input.mouse_input.posX = event.motion.x;
				game_window->input.mouse_input.posY = event.motion.y;
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
			} break;
			case SDL_KEYDOWN: 
			case SDL_KEYUP: {
				switch (event.key.keysym.scancode) {
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
					default: {
						game_window->input.kbd_input.key = KEY_NONE;
					} break;
				}
				game_window->input.kbd_input.was_down = (event.type == SDL_KEYUP);
				game_window->input.kbd_input.is_down = (event.type == SDL_KEYDOWN);
			} break;
			case SDL_TEXTINPUT: {
				game_window->input.character = event.text.text[0];
			} break;
			default:
				break;
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
			title_screen_do_animation();
			render_menu();
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
	sprintf(&buffer[0], "Summoning. dt = %.5f", game_window->dt);
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
	init_the_game();

	#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(main_loop, 0, 1);
	#else
	fps_cap_in_ms = 1000.0f / fps_cap;
	while (true) { main_loop(); }
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
	main(0, NULL);
}
#endif
