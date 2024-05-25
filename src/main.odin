package main

import "core:math/linalg"
import "core:log"
import "core:os"

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
    // demonic_sign: rl.RenderTexture2D,
}

setup_challenge :: proc(challenge: ^Type_Challenge, word: string, font: ^Font, fade_time: f32) {
    challenge.word = word
    challenge.font = font
    challenge.typed_correctly = make([]bool, len(word))
    challenge.dim = measure_text(challenge.font, challenge.word)
    challenge.alpha_animation = start_animation(fade_time)
    // render_demonic_sign(&challenge.demonic_sign, word)
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
            glyph_size := measure_rune(challenge.font, c)
            assert(glyph_size.x > 0.0)
            cursor_height := challenge.font.ascent + abs(challenge.font.descent)
            cursor_rect := rect_min_dim(position, v2{glyph_size.x, cursor_height})
            rect_sub_floats(&cursor_rect, v2{0.0, abs(challenge.font.descent)})
            // draw_rect_rounded_filled(cursor_rect, cursor_color, 0.5)
            draw_rect_filled(cursor_rect, cursor_color)
            text_color = under_cursor_color
        }
        character_size := draw_rune(challenge.font, position, c, text_color)
        position.x += character_size.x
    }

    // texture_color := color_with_alpha(WHITE, challenge.alpha)
    // demonic_dim := rl.Vector2{256, 256}
    // demonic_pos := rl.Vector2{0.0, game_window.dim.y - 256 - 10}
    // center_horizontally(
    //     &demonic_pos,
    //     demonic_dim,
    //     game_window.dim,
    // )
    // source_rect := rl.Rectangle{0, 0, 256, -256}
    // dest_rect   := rl.Rectangle{demonic_pos.x, demonic_pos.y, demonic_dim.x, demonic_dim.y}
    // origin      := rl.Vector2{0, 0}
    // rotation    : f32 = 0.0
    // rl.DrawTexturePro(challenge.demonic_sign.texture, source_rect, dest_rect, origin, rotation, texture_color)
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
    min := v3{0.0, 0.0, -1.0}
    max := v3{game_window.dim.x, game_window.dim.y, 1.0}
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

// render_demonic_sign :: proc(texture: ^rl.RenderTexture2D, word: string) {
//     using math
//     width   : f32 = 256.0
//     height  : f32 = 256.0
//     area_cx : f32 = width / 2.0
//     area_cy : f32 = height / 2.0
//     area_center := rl.Vector2{area_cx, area_cy}
//     texture^ = rl.LoadRenderTexture(i32(width), i32(height))

//     rl.BeginTextureMode(texture^)

//     rl.DrawCircleLinesV(area_center, 127.0, GREEN)
//     inner_radius : f32 = 95.0
//     rl.DrawCircleLinesV(area_center, inner_radius, GREEN)

//     single_char := [2]u8{}
//     first_char := unicode.to_upper(rune(word[0]))
//     single_char[0] = u8(first_char)
//     glyph_size := rl.MeasureTextEx(game_window.demonic_font.raylib_font, cstring(&single_char[0]), f32(game_window.demonic_font.raylib_font.baseSize), 0.0)
//     pos := rl.Vector2{256.0 / 2.0 - glyph_size.x / 2.0, 0.0}
//     rl.DrawTextEx(
//         game_window.demonic_font.raylib_font,
//         cstring(&single_char[0]),
//         pos,
//         f32(game_window.demonic_font.raylib_font.baseSize),
//         0.0,
//         GREEN
//     )
// 	angle_deg := 360.0 / f32(len(word))
// 	theta     := angle_deg * (PI / 180.0)

//     char_cx : f32 = pos.x + glyph_size.x/ 2.0 // center of the space in width
//     char_cy : f32 = pos.y + glyph_size.y / 2.0
//     input_points := make([]rl.Vector2, len(word)+1, context.temp_allocator)
//     input_points[0].x = char_cx
//     input_points[0].y = area_cy - inner_radius

//     for i in 1..<len(word) {
//         new_cx := cos(theta) * (char_cx - area_cx) - sin(theta) * (char_cy - area_cy) + area_cx
//         new_cy := sin(theta) * (char_cx - area_cx) + cos(theta) * (char_cy - area_cy) + area_cy
//         char_cx = new_cx
//         char_cy = new_cy

//         single_char[0] = word[i]
//         glyph_size = rl.MeasureTextEx(game_window.demonic_font.raylib_font, cstring(&single_char[0]), f32(game_window.demonic_font.raylib_font.baseSize), 0.0)
//         pos.x = char_cx - (glyph_size.x / 2.0)
//         pos.y = char_cy - (glyph_size.y / 2.0)
//         rl.DrawTextEx(
//             game_window.demonic_font.raylib_font,
//             cstring(&single_char[0]),
//             pos,
//             f32(game_window.demonic_font.raylib_font.baseSize),
//             0.0,
//             GREEN
//         )

//         vx := char_cx - area_cx
//         vy := char_cy - area_cy
//         magnitude := sqrt(vx*vx + vy*vy)
//         vx /= magnitude
//         vy /= magnitude
//         input_points[i].x = area_cx + vx * inner_radius
//         input_points[i].y = area_cy + vy * inner_radius
//     }
//     input_points[len(word)] = input_points[0]

//     output_points := make([]rl.Vector2, len(word)+1, context.temp_allocator)
//     input_i := 0
//     half_word := len(word) / 2
//     even_word_length := false
//     if len(word) % 2 == 0 {
//         half_word -= 1
//         even_word_length = true
//     }
//     for i in 0..<len(word) {
//         output_points[i] = input_points[input_i]
//         input_i = (input_i + half_word) % len(word)
//     }
//     output_points[len(word)] = input_points[0]

//     for i in 0..<len(word) {
//         p0 := output_points[i]
//         p1 := output_points[i+1]
//         if p0.x < p1.x {
//             rl.DrawLineV(p0, p1, GREEN)
//         } else {
//             rl.DrawLineV(p1, p0, GREEN)
//         }
//     }

//     rl.EndTextureMode()
// }

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
