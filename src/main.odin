package main

import "core:c"
import "core:fmt"
import "core:math"
import "core:math/linalg"
import "core:log"
import "core:os"
import "core:unicode"
import rl "vendor:raylib"
import stbtt "vendor:stb/truetype"

im_fell_font := #load("../assets/fonts/IM_Fell_English/IMFellEnglish-Regular.ttf")

DEFAULT_WINDOW_WIDTH  :: 1280
DEFAULT_WINDOW_HEIGHT :: 800

WHITE          :: rl.Color{255, 255, 255, 255}
BLUE           :: rl.Color{10, 10, 255, 255}
RED            :: rl.Color{255, 10, 10, 255}
VERY_DARK_BLUE :: rl.Color{4, 8, 13, 255}
GREEN          :: rl.Color{10, 255, 10, 255}
AMBER          :: rl.Color{255, 191, 0, 255}

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
    now := f32(rl.GetTime())
    end := now + duration
    return Animation {
        start = now,
        end = end,
        duration = duration,
    }
}

animate :: proc(animation: ^Animation) -> f32 {
    using animation

    t := (f32(rl.GetTime()) - start) / duration
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
    dim: rl.Vector2,
    origin: rl.Vector2,
    typed_correctly: []b32,
    position: u32,
    alpha: f32,
    alpha_animation: Animation,
}

setup_challenge :: proc(challenge: ^Type_Challenge, word: string, font: ^Font, fade_time: f32) {
    challenge.word = word
    challenge.font = font
    challenge.typed_correctly = make([]b32, len(word))
    c_str := fmt.ctprintf("%s", challenge.word)
    spacing : f32 = 0.0
    challenge.dim = rl.MeasureTextEx(font.raylib_font, c_str, f32(font.raylib_font.baseSize), spacing)
    challenge.dim.y = font.ascent - font.descent
    challenge.alpha_animation = start_animation(fade_time)
}

is_challenge_done :: proc(challenge: ^Type_Challenge) -> b32 {
    return challenge.position >= u32(len(challenge.word))
}

has_typing_started :: proc(challenge: ^Type_Challenge) -> b32 {
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

challenge_has_mistakes :: proc(challenge: ^Type_Challenge) -> b32 {
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
    neutral_color    := rl.ColorAlpha(WHITE, challenge.alpha)
    correct_color := rl.ColorAlpha(GREEN, challenge.alpha)
    wrong_color   := rl.ColorAlpha(RED, challenge.alpha)
    under_cursor_color := rl.ColorAlpha(VERY_DARK_BLUE, challenge.alpha)
    cursor_color       := rl.ColorAlpha(AMBER, challenge.alpha)

    position := challenge.origin

    for c, i in challenge.word {
        c_str := fmt.ctprintf("%c", c)
        spacing : f32 = 0.0
        glyph_size := rl.MeasureTextEx(
            challenge.font.raylib_font,
            c_str,
            f32(challenge.font.raylib_font.baseSize),
            spacing,
        )

        text_color : rl.Color = neutral_color
        if challenge.position > u32(i) {
            if challenge.typed_correctly[i] {
                text_color = correct_color
            } else {
                text_color = wrong_color
            }
        } else if challenge.position == u32(i) {
            cursor_rect := rl.Rectangle{
                x = position.x,
                y = position.y,
                width = glyph_size.x,
                height = challenge.font.ascent - challenge.font.descent,
            }
            rl.DrawRectangleRounded(
                cursor_rect,
                0.5, // roundness
                0, // segments
                cursor_color,
            )
            text_color = under_cursor_color
        }
        rl.DrawTextCodepoint(
            challenge.font.raylib_font,
            c,
            position,
            f32(challenge.font.raylib_font.baseSize),
            text_color,
        )
        position.x += glyph_size.x
    }
}

MAX_NUM_CHALLENGES :: 8

Level_Data :: struct {
    challenges: [MAX_NUM_CHALLENGES]Type_Challenge,
    num_challenges: int,
    current_challenge: int,
    level_number: int,
    points: int,
}

setup_level :: proc(level: ^Level_Data) {
    for _, i in words {
        if i >= MAX_NUM_CHALLENGES { break }
        setup_challenge(&level.challenges[i], words[i], &game_window.challenge_font, 1.5)
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

is_level_done :: proc(level: ^Level_Data) -> b32 {
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

    title_font: Font,
    challenge_font: Font,
    demonic_font: Font,

    dt: f32,
    frame_number: u64,
    dim: rl.Vector2,

    demonic_sign_texture: rl.RenderTexture2D,
}

@(require)
game_window := Game_Window{}

update_window_dim :: proc() {
    game_window.dim.x = f32(rl.GetRenderWidth())
    game_window.dim.y = f32(rl.GetRenderHeight())
}

center_horizontally :: proc(position: ^rl.Vector2, dim: rl.Vector2, within: rl.Vector2) {
    position.x = (within.x / 2.0) - (dim.x / 2.0)
}

center_vertically :: proc(position: ^rl.Vector2, dim: rl.Vector2, within: rl.Vector2) {
    position.y = (within.y / 2.0) - (dim.y / 2.0)
}

render_menu :: proc() {
    char := rl.GetCharPressed()
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
        char = rl.GetCharPressed()
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

    rl.BeginDrawing()
    rl.ClearBackground(VERY_DARK_BLUE)

    render_challenge(&game_window.title_challenge)

    offset_y := game_window.title_challenge.origin.y + game_window.title_challenge.dim.y + 10.0
    demonic_pos := rl.Vector2{0.0, offset_y}
    demonic_dim := rl.Vector2{256, 256}
    center_horizontally(
        &demonic_pos,
        demonic_dim,
        game_window.dim,
    )
    source_rect := rl.Rectangle{0, 0, 256, -256}
    dest_rect   := rl.Rectangle{demonic_pos.x, demonic_pos.y, demonic_dim.x, demonic_dim.y}
    origin      := rl.Vector2{0, 0}
    rotation    : f32 = 0.0
    rl.DrawTexturePro(game_window.demonic_sign_texture.texture, source_rect, dest_rect, origin, rotation, WHITE)

    rl.EndDrawing()
}

render_game :: proc() {
    challenge := &game_window.level_data.challenges[game_window.level_data.current_challenge]
    char := rl.GetCharPressed()
    for char != 0 {
        level_type_character(&game_window.level_data, char)
        if is_level_done(&game_window.level_data) {
            if game_window.level_data.points < game_window.level_data.num_challenges {
                game_window.game_state = .STATE_LOSE
            } else {
                game_window.game_state = .STATE_WIN
            }
        }
        char = rl.GetCharPressed()
    }
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

    rl.BeginDrawing()
    rl.ClearBackground(VERY_DARK_BLUE)

    render_challenge(challenge)

    rl.EndDrawing()
}

render_win :: proc() {
    rl.BeginDrawing()
    rl.ClearBackground(VERY_DARK_BLUE)

    text := fmt.ctprintf("%s", win)
    text_size := rl.MeasureTextEx(
        game_window.title_font.raylib_font,
        text,
        f32(game_window.title_font.raylib_font.baseSize),
        0.0,
    )
    position := rl.Vector2{}
    center_horizontally(
        &position,
        text_size,
        game_window.dim,
    )
    center_vertically(
        &position,
        text_size,
        game_window.dim,
    )
    rl.DrawTextEx(
        game_window.title_font.raylib_font,
        text,
        position,
        f32(game_window.title_font.raylib_font.baseSize),
        0.0,
        WHITE,
    )

    rl.EndDrawing()
}

render_lose :: proc() {
    rl.BeginDrawing()
    rl.ClearBackground(VERY_DARK_BLUE)

    text := fmt.ctprintf("%s", lost)
    text_size := rl.MeasureTextEx(
        game_window.title_font.raylib_font,
        text,
        f32(game_window.title_font.raylib_font.baseSize),
        0.0,
    )
    position := rl.Vector2{}
    center_horizontally(
        &position,
        text_size,
        game_window.dim,
    )
    center_vertically(
        &position,
        text_size,
        game_window.dim,
    )
    rl.DrawTextEx(
        game_window.title_font.raylib_font,
        text,
        position,
        f32(game_window.title_font.raylib_font.baseSize),
        0.0,
        WHITE,
    )

    rl.EndDrawing()
}

update_and_render :: proc() {
    #partial switch game_window.game_state {
        case .STATE_MENU: render_menu()
        case .STATE_PLAY: render_game()
        case .STATE_WIN:  render_win()
        case .STATE_LOSE: render_lose()
    }
}

FIRST_GLYPH :: 32
LAST_GLYPH  :: 127
NUM_GLYPHS  :: LAST_GLYPH - FIRST_GLYPH

render_demonic_sign :: proc(word: string) {
    using math
    width   : f32 = 256.0
    height  : f32 = 256.0
    area_cx : f32 = width / 2.0
    area_cy : f32 = height / 2.0
    area_center := rl.Vector2{area_cx, area_cy}
    game_window.demonic_sign_texture = rl.LoadRenderTexture(i32(width), i32(height))
    scale := game_window.demonic_font.scale

    rl.BeginTextureMode(game_window.demonic_sign_texture)

    rl.DrawPixelV(area_center, RED) // nocheckin: debugging
    rl.DrawCircleLinesV(area_center, 127.0, GREEN)
    inner_radius : f32 = 95.0
    rl.DrawCircleLinesV(area_center, inner_radius, GREEN)

    single_char := [2]u8{}
    first_char := unicode.to_upper(rune(word[0]))
    single_char[0] = u8(first_char)
    // glyph_rect := rl.GetGlyphAtlasRec(game_window.demonic_font.raylib_font, first_char)
    x0, x1, y0, y1 : c.int
    stbtt.GetCodepointBox(&game_window.demonic_font.info, rune(word[0]), &x0, &y0, &x1, &y1)
    glyph_size := rl.Vector2{f32(x1 - x0)*scale, f32(y1 - y0)*scale}
    // glyph_size := rl.MeasureTextEx(game_window.demonic_font.raylib_font, cstring(&single_char[0]), f32(game_window.demonic_font.raylib_font.baseSize), 0.0)
    pos := rl.Vector2{256.0 / 2.0 - glyph_size.x / 2.0, 0.0}
    rl.DrawTextEx(
        game_window.demonic_font.raylib_font,
        cstring(&single_char[0]),
        pos,
        f32(game_window.demonic_font.raylib_font.baseSize),
        0.0,
        GREEN
    )
    // TODO: render the rest of the characters and the lines
	angle_deg := 360.0 / f32(len(word))
	theta     := angle_deg * (PI / 180.0)

    char_cx : f32 = pos.x + glyph_size.x/ 2.0 // center of the space in width
    char_cy : f32 = pos.y + glyph_size.y / 2.0
    rl.DrawPixel(i32(char_cx), i32(char_cy), RED)
    input_points := make([]rl.Vector2, len(word)+1, context.temp_allocator)
    input_points[0].x = char_cx
    input_points[0].y = char_cy

    for i in 1..<len(word) {
        new_cx := cos(theta) * (char_cx - area_cx) - sin(theta) * (char_cy - area_cy) + area_cx
        new_cy := sin(theta) * (char_cx - area_cx) + cos(theta) * (char_cy - area_cy) + area_cy
        char_cx = new_cx
        char_cy = new_cy

        single_char[0] = word[i]
        // glyph_rect = rl.GetGlyphAtlasRec(game_window.demonic_font.raylib_font, rune(word[i]))
        stbtt.GetCodepointBox(&game_window.demonic_font.info, rune(word[i]), &x0, &y0, &x1, &y1)
        glyph_size = rl.Vector2{f32(x1 - x0)*scale, f32(y1 - y0)*scale}
        // glyph_size = rl.MeasureTextEx(game_window.demonic_font.raylib_font, cstring(&single_char[0]), f32(game_window.demonic_font.raylib_font.baseSize), 0.0)
        pos.x = char_cx - (glyph_size.x / 2.0)
        pos.y = char_cy - (glyph_size.y / 2.0)
        rl.DrawTextEx(
            game_window.demonic_font.raylib_font,
            cstring(&single_char[0]),
            pos,
            f32(game_window.demonic_font.raylib_font.baseSize),
            0.0,
            GREEN
        )
        rl.DrawRectangleV(
            pos,
            glyph_size,
            BLUE,
        )
        rl.DrawPixel(i32(char_cx), i32(char_cy), RED)

        vx := char_cx - area_cx
        vy := char_cy - area_cy
        magnitude := sqrt(vx*vx + vy*vy)
        vx /= magnitude
        vy /= magnitude
        input_points[i].x = area_cx + vx * inner_radius
        input_points[i].y = area_cy + vy * inner_radius
    }
    input_points[len(word)] = input_points[0]

    rl.EndTextureMode()
}

init_game :: proc() -> b32 {
    update_window_dim()
    if !init_font(&game_window.title_font, im_fell_font, 120.0) {
        log.error("Failed to init the title font!")
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
    render_demonic_sign(title)
    return true
}

main :: proc() {
    rl.InitWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "Ludum Dare 55: Summoning")
    if !init_game() {
        os.exit(1)
    }

    for !rl.WindowShouldClose() {
		if err := free_all(context.temp_allocator); err != .None {
			log.errorf("temp_allocator.free_all err == {}", err);
        }
        game_window.dt = rl.GetFrameTime()
        update_and_render()
    }

    rl.CloseWindow()
}
