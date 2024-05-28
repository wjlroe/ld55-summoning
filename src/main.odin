package main

import "core:fmt"
import "core:math/linalg"
import "core:log"
import "core:math"
import "core:os"
import "core:unicode"
import stbtt "vendor:stb/truetype"

// TODO:
// * Remove raylib (i.e. straight Odin port of the C code, without without SDL)
// * Newer (i.e. from Google Fonts) IM Font doesn't render at the correct scale
// * Older (i.e. from dafont) IM font renders ok but raylib miscalculates the height of it by about half

// Google Fonts
im_fell_font := #load("../assets/fonts/IM_Fell_English/IMFellEnglish-Regular.ttf")
// Dafont version
im_fell_dafont_font := #load("../assets/fonts/im_fell_roman.ttf")

DEFAULT_WINDOW_WIDTH  :: 1280
DEFAULT_WINDOW_HEIGHT :: 800

title :: "Summoning"
win   :: "You've Won!"
lost  :: "You've Lost!"

words := [?]string{
    "summon",
    "incantation",
    "spell",
    "awaken",
    "cauldron",
}

Game_State :: enum {
    STATE_MENU,
    STATE_PLAY,
    STATE_PAUSE,
    STATE_WIN,
    STATE_LOSE,
}

TITLE_CHALLENGE_FADE_TIME :: 10.0 // seconds

Animation :: struct {
    start: f32,
    end: f32,
    duration: f32,
}

start_animation :: proc(duration: f32) -> Animation {
    now := get_time()
    end := now + duration
    return Animation {
        start = now,
        end = end,
        duration = duration,
    }
}

animate :: proc(animation: ^Animation) -> f32 {
    using animation

    t := (get_time() - start) / duration
    t = clamp(t, 0.0, 1.0)
    return linalg.lerp(f32(0.0), f32(1.0), t)
}

shorten_animation :: proc(animation: ^Animation, new_time: f32) {
    using animation
    duration = new_time
    end = start + new_time
}

Type_Challenge :: struct {
    word: string,
    font: ^Font,
    dim: v2,
    origin: v2,
    typed_correctly: []bool,
    position: u32,
    alpha: f32,
    alpha_animation: Animation,
    demonic_sign: Texture,
}

setup_challenge :: proc(challenge: ^Type_Challenge, word: string, font: ^Font, fade_time: f32) {
    challenge.word = word
    challenge.font = font
    challenge.typed_correctly = make([]bool, len(word))
    challenge.dim = measure_text(challenge.font, challenge.word)
    challenge.alpha_animation = start_animation(fade_time)
    render_demonic_sign(&challenge.demonic_sign, word)
}

is_challenge_done :: proc(challenge: ^Type_Challenge) -> bool {
    return challenge.position >= u32(len(challenge.word))
}

has_typing_started :: proc(challenge: ^Type_Challenge) -> bool {
    return challenge.position > 0
}

enter_challenge_character :: proc(challenge: ^Type_Challenge, character: rune) {
    if challenge.position == 0 {
        shorten_animation(&challenge.alpha_animation, challenge.alpha_animation.duration / 3.0)
    }
    if !is_challenge_done(challenge) {
        challenge.typed_correctly[challenge.position] = character == rune(challenge.word[challenge.position])
        challenge.position += 1
    }
}

challenge_has_mistakes :: proc(challenge: ^Type_Challenge) -> bool {
    for correct in challenge.typed_correctly {
        if !correct { return true }
    }
    return  false
}

update_challenge_alpha :: proc(challenge: ^Type_Challenge) {
    challenge.alpha = animate(&challenge.alpha_animation)
}

reset_challenge :: proc(challenge: ^Type_Challenge) {
    challenge.position = 0
    challenge.alpha = 0.0
}

render_challenge :: proc(challenge: ^Type_Challenge) {
    neutral_color      := color_with_alpha(WHITE, challenge.alpha)
    correct_color      := color_with_alpha(GREEN, challenge.alpha)
    wrong_color        := color_with_alpha(RED, challenge.alpha)
    under_cursor_color := color_with_alpha(VERY_DARK_BLUE, challenge.alpha)
    cursor_color       := color_with_alpha(AMBER, challenge.alpha)

    {
        debug_msg := "top of screen"
        text_size := measure_text(&game_window.challenge_font, debug_msg)

        position := v2{0.0, text_size.y}
        center_horizontally(&position, text_size, game_window.dim)
        draw_text(&game_window.challenge_font, position, debug_msg, RED)
    }

    position := challenge.origin

    for c, i in challenge.word {
        text_color := neutral_color
        if challenge.position > u32(i) {
            if challenge.typed_correctly[i] {
                text_color = correct_color
            } else {
                text_color = wrong_color
            }
        } else if challenge.position == u32(i) {
            glyph_size, glyph := measure_rune(challenge.font, c)
            assert(glyph_size.x > 0.0)
            // cursor_height := challenge.font.ascent + abs(challenge.font.descent)
            cursor_pos := position
            cursor_pos.y += challenge.font.bounding_box.min.y
            cursor_rect := rect_min_dim(cursor_pos, v2{glyph_size.x, challenge.font.font_height})
            rect_sub_floats(&cursor_rect, v2{0.0, abs(challenge.font.descent)})
            draw_rect_rounded_filled(cursor_rect, cursor_color, glyph_size.x / 4.0)
            text_color = under_cursor_color
        }
        character_size, glyph := draw_rune(challenge.font, position, c, text_color)
        position.x += glyph.advance
    }

    demonic_pos := v2{0.0, game_window.dim.y - 256.0 - 10.0}
    demonic_dim := vec_ints_to_floats(challenge.demonic_sign.dim)
    center_horizontally(&demonic_pos, demonic_dim, game_window.dim)
    texture_color := color_with_alpha(GREEN, challenge.alpha)
    rect := rect_min_dim(demonic_pos, demonic_dim)
    draw_texture(&challenge.demonic_sign, rect, texture_color)
}

MAX_NUM_CHALLENGES :: 8

Level_Data :: struct {
    challenges: [MAX_NUM_CHALLENGES]Type_Challenge,
    num_challenges: int,
    current_challenge: int,
    level_number: int,
    points: int,
    font: ^Font,
}

setup_level :: proc(level: ^Level_Data) {
    for _, i in words {
        if i >= MAX_NUM_CHALLENGES { break }
        setup_challenge(&level.challenges[i], words[i], level.font, 1.5)
        level.num_challenges += 1
    }
    level.current_challenge = 0
}

reset_level :: proc(level: ^Level_Data) {
    for _,i in level.challenges {
        reset_challenge(&level.challenges[i])
    }
    level.current_challenge = 0
    level.points = 0
}

is_level_done :: proc(level: ^Level_Data) -> bool {
    if level.current_challenge < level.num_challenges {
        return false
    }
    return true
}

level_type_character :: proc(level: ^Level_Data, character: rune) {
    if is_level_done(level) { return }
    challenge := &level.challenges[level.current_challenge]
    enter_challenge_character(challenge, character)
    if is_challenge_done(challenge) {
        level.current_challenge += 1
        if !challenge_has_mistakes(challenge) {
            level.points += 1
        }
    }
}

Game_Window :: struct {
    game_state: Game_State,
    title_challenge: Type_Challenge,
    level_data: Level_Data,

    im_fell_id: int,
    title_font: Font,
    dafont_title_font: Font,
    challenge_font: Font,
    demonic_font: Font,

    dt: f32,
    frame_number: u64,
    dim: v2,
    ortho_matrix: matrix[4,4]f32,
}

@(require)
game_window := Game_Window{}

update_ortho_matrix :: proc() {
    min := v3{0.0, game_window.dim.y, -1.0}
    max := v3{game_window.dim.x, 0.0, 1.0}
    game_window.ortho_matrix = ortho_matrix(min, max)
}

center_horizontally :: proc(position: ^v2, dim: v2, within: v2) {
    position.x = (within.x / 2.0) - (dim.x / 2.0)
}

center_vertically :: proc(position: ^v2, dim: v2, within: v2) {
    position.y = (within.y / 2.0) - (dim.y / 2.0)
}

render_menu :: proc() {
    char := get_char_pressed()
    for char != 0 {
        enter_challenge_character(&game_window.title_challenge, char)
        if is_challenge_done(&game_window.title_challenge) {
            if challenge_has_mistakes(&game_window.title_challenge) {
                reset_challenge(&game_window.title_challenge)
            } else {
                game_window.game_state = .STATE_PLAY
                setup_level(&game_window.level_data)
            }
        }
        char = get_char_pressed()
    }
    update_challenge_alpha(&game_window.title_challenge)
    center_horizontally(
        &game_window.title_challenge.origin,
        game_window.title_challenge.dim,
        game_window.dim,
    )
    center_vertically(
        &game_window.title_challenge.origin,
        game_window.title_challenge.dim,
        game_window.dim,
    )

    begin_drawing()
    clear_background(VERY_DARK_BLUE)
    render_challenge(&game_window.title_challenge)
    end_drawing()
}

render_game :: proc() {
    char := get_char_pressed()
    for char != 0 {
        level_type_character(&game_window.level_data, char)
        if is_level_done(&game_window.level_data) {
            if game_window.level_data.points < game_window.level_data.num_challenges {
                game_window.game_state = .STATE_LOSE
            } else {
                game_window.game_state = .STATE_WIN
            }
        }
        char = get_char_pressed()
    }
    challenge := &game_window.level_data.challenges[game_window.level_data.current_challenge]
    update_challenge_alpha(challenge)
    center_horizontally(
        &challenge.origin,
        challenge.dim,
        game_window.dim,
    )
    center_vertically(
        &challenge.origin,
        challenge.dim,
        game_window.dim,
    )

    begin_drawing()
    clear_background(VERY_DARK_BLUE)
    render_challenge(challenge)
    end_drawing()
}

render_big_text :: proc(text: string) {
    begin_drawing()

    clear_background(VERY_DARK_BLUE)

	text_size := measure_text(&game_window.title_font, text)
    position := v2{}
    center_horizontally(&position, text_size, game_window.dim)
    center_vertically(&position, text_size, game_window.dim)
    draw_text(&game_window.title_font, position, text, WHITE)

    end_drawing()
}

render_win :: proc() {
    render_big_text(win)
}

render_lose :: proc() {
    render_big_text(lost)
}

update_and_render :: proc() {
    #partial switch game_window.game_state {
        case .STATE_MENU: render_menu()
        case .STATE_PLAY: render_game()
        case .STATE_WIN:  render_win()
        case .STATE_LOSE: render_lose()
    }
}

// FIRST_GLYPH :: 32
// LAST_GLYPH  :: 127
// NUM_GLYPHS  :: LAST_GLYPH - FIRST_GLYPH

// #define SWAP(x, y, T) do { T SWAP = x; x = y; y = SWAP; } while (0)
swap :: #force_inline proc($T: typeid, a, b: ^T) {
    tmp := a
    a^ = b^
    b^ = tmp^
}

ipart :: #force_inline proc(x: f32) -> i32 {
	return i32(math.floor(x))
}

fpart :: #force_inline proc(x: f32) -> f32 {
	return x - f32(ipart(x))
}

rfpart :: #force_inline proc(x: f32) -> f32 {
	return 1.0 - fpart(x)
}

// This is from https://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm
draw_line :: proc(pixels: []u8, row_size: i32, x0, y0, x1, y1: f32) {
    x0 := x0
    y0 := y0
    x1 := x1
    y1 := y1
	steep := abs(y1 - y0) > abs(x1 - x0)

	if (steep) {
		swap(f32, &x0, &y0)
		swap(f32, &x1, &y1)
	}
	if (x0 > x1) {
		swap(f32, &x0, &x1)
		swap(f32, &y0, &y1)
	}

	dx := x1 - x0
	dy := y1 - y0

	gradient : f32
	if dx == 0.0 {
		gradient = 1.0
	} else {
		gradient = dy / dx
	}

	xend := math.round(x0)
	yend := y0 + gradient * (xend - x0)
	xgap := rfpart(x0 + 0.5)
	xpxl1 : i32 = i32(xend)
	ypxl1 : i32 = ipart(yend)
	if steep {
		pixels[row_size * xpxl1     + ypxl1]     = 255
		pixels[row_size * xpxl1     + (ypxl1+1)] = 255
	} else {
		pixels[row_size * ypxl1     + xpxl1]     = 255
		pixels[row_size * (ypxl1+1) + xpxl1]     = 255
	}
	intery := yend + gradient

	xend = math.round(x1)
	yend = y1 + gradient * (xend - x1)
	xgap = fpart(x1 + 0.5)
	xpxl2 : i32 = i32(xend)
	ypxl2 : i32 = ipart(yend)
	if steep {
		pixels[row_size * xpxl2     + ypxl2]     = 255
		pixels[row_size * xpxl2     + (ypxl2+1)] = 255
	} else {
		pixels[row_size * ypxl2     + xpxl2]     = 255
		pixels[row_size * (ypxl2+1) + xpxl2]     = 255
	}

	if steep {
        for x in (xpxl1 + 1)..<(xpxl2 - 1) {
			pixels[row_size * x + ipart(intery)]     = 255
			pixels[row_size * x + (ipart(intery)+1)] = 255
			intery += gradient
		}
	} else {
        for x in (xpxl1 + 1)..<(xpxl2 - 1) {
			pixels[row_size * ipart(intery)     + x] = 255
			pixels[row_size * (ipart(intery)+1) + x] = 255
			intery += gradient
		}
	}
}

// This is from https://www.computerenhance.com/p/efficient-dda-circle-outlines
draw_circle :: proc(pixels: []u8, row_size: i32, radius: i32) {
	Cx : i32 = 256 / 2
	Cy : i32 = 256 / 2
	R  : i32 = radius
	{
		R2 : i32 = R+R

		X  := R
		Y  : i32 = 0
		dY : i32 = -2
		dX : i32 = R2+R2 - 4
		D  : i32 = R2 - 1

		for (Y <= X) {
			pixels[row_size * (Cy - Y) + (Cx - X)] = 255
			pixels[row_size * (Cy - Y) + (Cx + X)] = 255
			pixels[row_size * (Cy + Y) + (Cx - X)] = 255
			pixels[row_size * (Cy + Y) + (Cx + X)] = 255
			pixels[row_size * (Cy - X) + (Cx - Y)] = 255
			pixels[row_size * (Cy - X) + (Cx + Y)] = 255
			pixels[row_size * (Cy + X) + (Cx - Y)] = 255
			pixels[row_size * (Cy + X) + (Cx + Y)] = 255

			D += dY
			dY -= 4
			Y += 1

			Mask := (D >> 31)
			D += dX & Mask
			dX -= 4 & Mask
			X += Mask
		}
	}
}

render_demonic_sign :: proc(texture: ^Texture, word: string) {
    using math

	font := &game_window.demonic_font
    info := &font.info

    texture.dim = v2s{256, 256}
    // Single channel memory since we're drawing just alpha channel values
    memory := make([]u8, texture.dim.x * texture.dim.y)
    defer delete(memory)

    area_center := rect_centre(rect_min_dim(v2s{}, texture.dim))

    draw_circle(memory, texture.dim.x, 127)
    inner_radius : i32 = 95
    draw_circle(memory, texture.dim.x, inner_radius)

    codepoint := unicode.to_upper(rune(word[0]))
    // glyph_size := rl.MeasureTextEx(game_window.demonic_font.raylib_font, cstring(&single_char[0]), f32(game_window.demonic_font.raylib_font.baseSize), 0.0)
    glyph_size, glyph := measure_rune(font, codepoint)
    pos := vec_floats_to_ints(v2{256.0 / 2.0 - glyph_size.x / 2.0, 0.0})
    pos_idx := pos.y * texture.dim.x + pos.x

    // STBTT_DEF void stbtt_MakeCodepointBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int codepoint);
    before := memory[pos_idx]
    stbtt.MakeCodepointBitmap(info, &memory[pos_idx], i32(math.round(glyph_size.x)), i32(math.round(glyph_size.y)), texture.dim.x, font.scale, font.scale, codepoint)
    after := memory[pos_idx]
    fmt.printf("before: %d, after: %d\n", before, after)
    // rl.DrawTextEx(
    //     game_window.demonic_font.raylib_font,
    //     cstring(&single_char[0]),
    //     pos,
    //     f32(game_window.demonic_font.raylib_font.baseSize),
    //     0.0,
    //     GREEN
    // )
	angle_deg := 360.0 / f32(len(word))
	theta     := angle_deg * (PI / 180.0)

    char_cx := f32(pos.x) + glyph_size.x / 2.0 // center of the space in width
    char_cy := f32(pos.y) + glyph_size.y / 2.0
    area_cx := f32(area_center.x)
    area_cy := f32(area_center.y)
    input_points := make([]v2, len(word)+1, context.temp_allocator)
    input_points[0].x = char_cx
    input_points[0].y = area_cy - f32(inner_radius)

    for i in 1..<len(word) {
        new_cx := cos(theta) * (char_cx - area_cx) - sin(theta) * (char_cy - area_cy) + area_cx
        new_cy := sin(theta) * (char_cx - area_cx) + cos(theta) * (char_cy - area_cy) + area_cy
        char_cx = new_cx
        char_cy = new_cy

        codepoint := rune(word[i])
        // glyph_size = rl.MeasureTextEx(game_window.demonic_font.raylib_font, cstring(&single_char[0]), f32(game_window.demonic_font.raylib_font.baseSize), 0.0)
        glyph_size, glyph = measure_rune(font, codepoint)
        pos = vec_floats_to_ints(v2{
            char_cx - (glyph_size.x / 2.0),
            char_cy - (glyph_size.y / 2.0),
        })
        pos_idx = pos.y * texture.dim.y + pos.x
        stbtt.MakeCodepointBitmap(info, &memory[pos_idx], i32(math.round(glyph_size.x)), i32(math.round(glyph_size.y)), texture.dim.x, font.scale, font.scale, codepoint)
        // rl.DrawTextEx(
        //     game_window.demonic_font.raylib_font,
        //     cstring(&single_char[0]),
        //     pos,
        //     f32(game_window.demonic_font.raylib_font.baseSize),
        //     0.0,
        //     GREEN
        // )

        vx := char_cx - area_cx
        vy := char_cy - area_cy
        magnitude := sqrt(vx*vx + vy*vy)
        vx /= magnitude
        vy /= magnitude
        input_points[i].x = area_cx + vx * f32(inner_radius)
        input_points[i].y = area_cy + vy * f32(inner_radius)
    }
    input_points[len(word)] = input_points[0]

    output_points := make([]v2, len(word)+1, context.temp_allocator)
    input_i := 0
    half_word := len(word) / 2
    even_word_length := false
    if len(word) % 2 == 0 {
        half_word -= 1
        even_word_length = true
    }
    for i in 0..<len(word) {
        output_points[i] = input_points[input_i]
        input_i = (input_i + half_word) % len(word)
    }
    output_points[len(word)] = input_points[0]

    for i in 0..<len(word) {
        p0 := output_points[i]
        p1 := output_points[i+1]
        if p0.x < p1.x {
            draw_line(memory, texture.dim.x, p0.x, p0.y, p1.x, p1.y)
        } else {
            draw_line(memory, texture.dim.x, p1.x, p1.y, p0.x, p0.y)
        }
    }

    init_texture(texture, .Red, raw_data(memory))
}

init_game :: proc() -> bool {
    update_window_dim()
    update_ortho_matrix()

    if !init_font(&game_window.title_font, im_fell_font, 120.0) {
        log.error("Failed to init the title font!")
        return false
    }
    if !init_font(&game_window.dafont_title_font, im_fell_dafont_font, 120.0) {
        log.error("Failed to init the dafont title font!")
        return false
    }
    if !init_font(&game_window.challenge_font, im_fell_font, 48.0) {
        log.error("Failed to init the challenge font!")
        return false
    }
    if !init_font(&game_window.demonic_font, im_fell_font, 38.0) {
        log.error("Failed to init the demonic font!")
        return false
    }

    setup_challenge(&game_window.title_challenge, title, &game_window.title_font, TITLE_CHALLENGE_FADE_TIME)
    game_window.level_data.font = &game_window.challenge_font

    return true
}

Window :: struct {
    width: i32,
    height: i32,
    title: string,
    quit: bool,
}

global_window := Window{}

process_input :: proc() {
    if is_key_down(control_key(.Q)) {
        // TODO: we may want to prompt about quitting first
        // TODO: we may want to save game state first
        global_window.quit = true
    }
}

main :: proc() {
	lowest_level := log.Level.Info
	when ODIN_DEBUG {
		lowest_level = log.Level.Debug
	}
	context.logger = log.create_console_logger(lowest = lowest_level)

    global_window.width = DEFAULT_WINDOW_WIDTH
    global_window.height = DEFAULT_WINDOW_HEIGHT
    global_window.title = "Ludum Dare 55: Summoning"

    {
        // Platform-specific Window initialization
        ok := init_window()
        assert(ok)
        if !ok {
            log.errorf("init_window failed!")
        }
    }

    {
        ok : bool
        vertex_shader_source : []u8
        vertex_shader_source, ok = os.read_entire_file("assets/shaders/vertex_shader.glsl")
        assert(ok)
        if !ok {
            log.errorf("failed to read vertex shader file")
        }
        fragment_shader_source : []u8
        fragment_shader_source, ok = os.read_entire_file("assets/shaders/fragment_shader.glsl")
        assert(ok)
        if !ok {
            log.errorf("failed to read fragment shader file")
        }
        shader_idx : int
        shader_idx, ok = compile_shader_program(string(vertex_shader_source), string(fragment_shader_source))
        assert(ok)
        if !ok {
            log.errorf("shaders failed to compile!")
        }
        ok = init_quad_shader(shader_idx)
        assert(ok)
        if !ok {
            log.error("shaders failed to init!")
        }
    }

    if !init_game() {
        os.exit(1)
    }

    log.infof("going to start the render loop")
    for !global_window.quit {
		if err := free_all(context.temp_allocator); err != .None {
			log.errorf("temp_allocator.free_all err == {}", err);
        }

        reset_frame_inputs()
        capture_input()
        process_input()
        update_and_render()
        swap_window()
    }
    // for !rl.WindowShouldClose() {
    //     game_window.dt = rl.GetFrameTime()
    // }

    // rl.CloseWindow()
}
