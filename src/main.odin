package main

import "core:fmt"
import "core:log"
import rl "vendor:raylib"

im_fell_font := #load("../assets/fonts/im_fell_roman.ttf")

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

words :: [?]string{
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

DEFAULT_CHALLENGE_FADE_SPEED :: 5.5

Type_Challenge :: struct {
    word: string,
    font: rl.Font,
    font_size: f32,
    dim: rl.Vector2,
    origin: rl.Vector2, // TODO: center this or whatever!
    typed_correctly: []b32,
    position: u32,
    alpha: f32,
    alpha_fade_speed: f32,
}

type_challenge :: #force_inline proc(word: string, font: rl.Font, font_size: f32) -> Type_Challenge {
    challenge := Type_Challenge{
        word = word,
        font = font,
        font_size = font_size,
        typed_correctly = make([]b32, len(word)),
        alpha_fade_speed = DEFAULT_CHALLENGE_FADE_SPEED,
    }

    c_str := fmt.ctprintf("%s", challenge.word)
    spacing : f32 = 0.0
    challenge.dim = rl.MeasureTextEx(font, c_str, font_size, spacing)

    return challenge
}

is_challenge_done :: proc(challenge: ^Type_Challenge) -> b32 {
    return challenge.position >= u32(len(challenge.word))
}

has_typing_started :: proc(challenge: ^Type_Challenge) -> b32 {
    return challenge.position > 0
}

enter_challenge_character :: proc(challenge: ^Type_Challenge, character: rune) {
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

// TODO: LERP?
update_challenge_alpha :: proc(challenge: ^Type_Challenge, dt: f32) {
    using challenge
    if alpha < 1.0 {
        if has_typing_started(challenge) || alpha_fade_speed == 0.0 {
            alpha += dt
        } else {
            alpha += dt / alpha_fade_speed
        }
        alpha = clamp(0.0, 1.0, alpha)
    }
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

    position := rl.Vector2{}

    for c, i in challenge.word {
        text_color : rl.Color = neutral_color
        if challenge.position > u32(i) {
            if challenge.typed_correctly[i] {
                text_color = correct_color
            } else {
                text_color = wrong_color
            }
        } else if challenge.position == u32(i) {
            text_color = under_cursor_color
        }
        rl.DrawTextCodepoint(
            challenge.font,
            c,
            position,
            challenge.font_size,
            text_color,
        )
        c_str := fmt.ctprintf("%c", c)
        spacing : f32 = 0.0
        glyph_size := rl.MeasureTextEx(
            challenge.font,
            c_str,
            challenge.font_size,
            spacing,
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

    title_font: rl.Font,

    dt: f32,
    frame_number: u64,
}

@(require)
game_window := Game_Window{}

render_menu :: proc() {
    update_challenge_alpha(&game_window.title_challenge, game_window.dt)

    rl.BeginDrawing()
    rl.ClearBackground(VERY_DARK_BLUE)

    render_challenge(&game_window.title_challenge)

    // text_color := rl.ColorAlpha(WHITE, game_window.title_challenge.alpha)
    // rl.DrawText(title, 190, 200, 20, text_color)
    rl.EndDrawing()
}

render_game :: proc() {}
render_win :: proc() {}
render_lose :: proc() {}

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

init_game :: proc() {
    codepoints : [NUM_GLYPHS]rune
    i := 0
    for glyph in FIRST_GLYPH..<LAST_GLYPH{
        codepoints[i] = rune(glyph)
        i += 1
    }
    font_file_size := i32(len(im_fell_font))
    game_window.title_font = rl.LoadFontFromMemory(".ttf", &im_fell_font[0], font_file_size, 120, &codepoints[0], NUM_GLYPHS)
    assert(rl.IsFontReady(game_window.title_font))
    game_window.title_challenge = type_challenge(title, game_window.title_font, 120.0)
}

main :: proc() {
    rl.InitWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "Ludum Dare 55: Summoning")
    init_game()

    for !rl.WindowShouldClose() {
		if err := free_all(context.temp_allocator); err != .None {
			log.errorf("temp_allocator.free_all err == {}", err);
        }
        game_window.dt = rl.GetFrameTime()
        update_and_render()
    }

    rl.CloseWindow()
}
