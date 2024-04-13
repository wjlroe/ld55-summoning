#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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

#define STRING(s) String{s, sizeof(s)}

typedef struct Color {
	int r, g, b, a;
} Color;

Color blue = {0, 0, 255, 255};
Color red = {255, 0, 0, 255};

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
} Input;

typedef struct Game_Window {
	bool quit; // zero-init means quit=false by default
	int window_width, window_height;
	SDL_Window* window;
	SDL_Renderer* renderer;
	TTF_Font* im_fell_font;
	SDL_Rect game_name_rect;
	SDL_Texture* game_name_texture;
	uint64_t last_frame_perf_counter;
	float dt;
	int frame_number;

	SDL_Rect rect2;
	SDL_Texture* runner_texture;
	Spritesheet runner1, runner2;
	
	Game_State state;
	
	Input input;
} Game_Window;

static Game_Window game_window = {0};

static bool key_first_down() {
	return (game_window.input.kbd_input.is_down) && !(game_window.input.kbd_input.was_down);
}

static bool key_is_down(Key key) {
	return (game_window.input.kbd_input.key == key) && (game_window.input.kbd_input.is_down);
}

#define DEFAULT_WINDOW_WIDTH  1280
#define DEFAULT_WINDOW_HEIGHT 800
#define KEYBOARD_VELOCITY 10

#ifndef __EMSCRIPTEN__
static float fps_cap = 60.0f;
static float fps_cap_in_ms;
#endif

static void init_the_game(void) {
	game_window.window_width = DEFAULT_WINDOW_WIDTH;
	game_window.window_height = DEFAULT_WINDOW_HEIGHT;
	game_window.last_frame_perf_counter = SDL_GetPerformanceCounter();
	SDL_Init(SDL_INIT_EVERYTHING);
	TTF_Init();
	game_window.window = SDL_CreateWindow(
										  "Ludum Dare 55: Summoning",
										  SDL_WINDOWPOS_UNDEFINED,
										  SDL_WINDOWPOS_UNDEFINED,
										  game_window.window_width, 
										  game_window.window_height, 
										  SDL_WINDOW_OPENGL);
	game_window.renderer = SDL_CreateRenderer(
											  game_window.window,
											  -1,
											  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	
	SDL_Surface *spritesheet_surface = IMG_Load("./assets/spritesheet.png");
	game_window.runner_texture = SDL_CreateTextureFromSurface(game_window.renderer, spritesheet_surface);
	SDL_FreeSurface(spritesheet_surface);
	
	TTF_Font* font = TTF_OpenFont("./assets/fonts/IM Fell English/FeENrm2.ttf", 120);
	if (font == NULL) {
		const char* error = SDL_GetError();
		#ifndef __EMSCRIPTEN__
		char buffer[1024] = {0};
		sprintf(&buffer[0], "Error loading a font: %s\n", error);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
								 "Error",
								 &buffer[0],
								 game_window.window);
		#endif
	}
	game_window.im_fell_font = font;
	SDL_Color OffWhite = {250, 250, 250};
	TTF_SizeText(game_window.im_fell_font, "Summoning", &game_window.game_name_rect.w, &game_window.game_name_rect.h);
	SDL_Surface* surface = TTF_RenderText_Solid(game_window.im_fell_font, "Summoning", OffWhite);
	game_window.game_name_texture = SDL_CreateTextureFromSurface(game_window.renderer, surface);

	{
		game_window.runner1 = new_spritesheet(game_window.runner_texture, 500, 500, 4, 4);
		game_window.runner1.dtPerFrame = 0.05;
		game_window.runner1.dest_rect.w = 100;
		game_window.runner1.dest_rect.h = 100;
		game_window.runner1.dest_rect.x = game_window.window_width - game_window.runner1.dest_rect.w;
		game_window.runner1.dest_rect.y = game_window.window_height - game_window.runner1.dest_rect.h;
	}
	{
		game_window.runner2 = new_spritesheet(game_window.runner_texture, 500, 500, 4, 4);
		game_window.runner2.dtPerFrame = 0.03;
		game_window.runner2.dest_rect.w = 150;
		game_window.runner2.dest_rect.h = 150;
		game_window.runner2.dest_rect.x = game_window.window_width - game_window.runner1.dest_rect.w - game_window.runner2.dest_rect.w;
		game_window.runner2.dest_rect.y = game_window.window_height - game_window.runner2.dest_rect.h;
	}

	game_window.rect2.x = 100;
	game_window.rect2.y = 20;
	game_window.rect2.w = 300;
	game_window.rect2.h = 300;
}

static void game_handle_input(void) {
	switch(game_window.state) {
		case STATE_PLAY: {
			if (key_is_down(KEY_ESCAPE) && key_first_down()) {
				game_window.state = STATE_PAUSE;
			} else if (key_is_down(KEY_UP)) {
				game_window.rect2.y -= KEYBOARD_VELOCITY;
			}
		} break;
		case STATE_PAUSE: {
			if (key_is_down(KEY_ESCAPE) && key_first_down()) {
				game_window.state = STATE_PLAY;
			}
		} break;
		case STATE_MENU: {
			if (key_is_down(KEY_RETURN) && key_first_down()) {
				game_window.state = STATE_PLAY;
			}
		} break;
		default: {}
	}
}

// This translates from SDL events into our Input structure
static void handle_inputs(void) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT: {
				game_window.quit = true;
			} break;
			case SDL_MOUSEMOTION: {
				game_window.input.mouse_input.posX = event.motion.x;
				game_window.input.mouse_input.posY = event.motion.y;
			} break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP: {
				if (event.button.button == SDL_BUTTON_LEFT) {
					game_window.input.mouse_input.left_was_down = (event.type == SDL_MOUSEBUTTONUP);
					game_window.input.mouse_input.left_is_down = (event.type == SDL_MOUSEBUTTONDOWN);
				} else if (event.button.button == SDL_BUTTON_RIGHT) {
			        game_window.input.mouse_input.right_was_down = (event.type == SDL_MOUSEBUTTONUP);
					game_window.input.mouse_input.right_is_down = (event.type == SDL_MOUSEBUTTONDOWN);
				}
			} break;
			case SDL_KEYDOWN: 
			case SDL_KEYUP: {
				game_window.input.kbd_input.is_down = (event.type == SDL_KEYDOWN);
				game_window.input.kbd_input.was_down = (event.type == SDL_KEYUP);
				switch (event.key.keysym.scancode) {
					case SDL_SCANCODE_W:
					case SDL_SCANCODE_UP: {
						game_window.input.kbd_input.key = KEY_UP;
					} break;
					case SDL_SCANCODE_A:
					case SDL_SCANCODE_LEFT: {
						game_window.input.kbd_input.key = KEY_LEFT;
					} break;
					case SDL_SCANCODE_S:
					case SDL_SCANCODE_DOWN: {
						game_window.input.kbd_input.key = KEY_DOWN;
					} break;
					case SDL_SCANCODE_D:
					case SDL_SCANCODE_RIGHT: {
						game_window.input.kbd_input.key = KEY_RIGHT;
					} break;
					case SDL_SCANCODE_ESCAPE: {
						game_window.input.kbd_input.key = KEY_ESCAPE;
					} break;
					case SDL_SCANCODE_RETURN: {
						game_window.input.kbd_input.key = KEY_RETURN;
					} break;
					default:
						break;
				}
			}
			default:
				break;
		}
		
		game_handle_input();
	}
}

float title_alpha = 0.0;

static void do_animation(void) {
	if (title_alpha < 1.0f) {
		title_alpha += game_window.dt / 10.f;
		title_alpha = title_alpha > 1.0 ? 1.0 : title_alpha;
	}
	update_sprite_animation(&game_window.runner1, game_window.dt);
	update_sprite_animation(&game_window.runner2, game_window.dt);
}

static void center_rect_horizontally(SDL_Rect* rect) {
	rect->x = (game_window.window_width / 2) - (rect->w / 2);
}

static void center_rect_veritcally(SDL_Rect* rect) {
	rect->y = (game_window.window_height / 2) - (rect->h / 2);
}

static void render_menu(void) {
	center_rect_horizontally(&game_window.game_name_rect);
	center_rect_veritcally(&game_window.game_name_rect);
	int alpha = title_alpha * 255;
	SDL_SetTextureAlphaMod(game_window.game_name_texture, alpha);
	SDL_RenderCopy(game_window.renderer, game_window.game_name_texture, NULL, &game_window.game_name_rect);
}

static void render_game(void) {
	SDL_SetRenderDrawColor(game_window.renderer, 255, 255, 255, 255);
	SDL_RenderFillRect(game_window.renderer, &game_window.rect2);
	SDL_RenderCopy(game_window.renderer,
				   game_window.runner1.texture,
				   &game_window.runner1.source_rect,
				   &game_window.runner1.dest_rect);
	SDL_RenderCopy(game_window.renderer,
				   game_window.runner2.texture,
				   &game_window.runner2.source_rect,
				   &game_window.runner2.dest_rect);
}

static void update_and_render(void) {
	SDL_SetRenderDrawColor(game_window.renderer, 9, 20, 33, 255);
	SDL_RenderClear(game_window.renderer);
	
	switch (game_window.state) {
		case STATE_MENU: {
			render_menu();
		} break;
		case STATE_PLAY: {
			render_game();
		} break;
		default: {}
	}
	
	SDL_RenderPresent(game_window.renderer);
}

static void main_loop(void) {
	if (game_window.quit) {
		#ifdef __EMSCRIPTEN__
		emscripten_cancel_main_loop();
		#else
		SDL_DestroyRenderer(game_window.renderer);
		SDL_DestroyWindow(game_window.window);
		exit(0);
		#endif
	}

	uint64_t this_frame_perf_counter = SDL_GetPerformanceCounter();
	game_window.dt = (float)(this_frame_perf_counter - game_window.last_frame_perf_counter)/(float)SDL_GetPerformanceFrequency();
	char buffer[1024] = {0};
	sprintf(&buffer[0], "Summoning. dt = %.5f", game_window.dt);
	SDL_SetWindowTitle(game_window.window, &buffer[0]);
	game_window.frame_number++;

	handle_inputs();
	do_animation();
	update_and_render();

	#ifndef __EMSCRIPTEN__
	uint64_t delta = game_window.dt * 1000.0f;
	if (delta < fps_cap_in_ms) {
		SDL_Delay(fps_cap_in_ms - delta);
	}
	#endif

	game_window.last_frame_perf_counter = this_frame_perf_counter;
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
