package main

import "core:fmt"
import "core:math/linalg"
import "core:log"
import "core:math"
import "core:os"
import "core:container/small_array"
import "core:strings"
import "core:unicode"
import rl "vendor:raylib"

// TODO:
// * (macOS) Title is positioned at the bottom right corner rather than center!!
// * Newer (i.e. from Google Fonts) IM Font doesn't render at the correct scale
// * Older (i.e. from dafont) IM font renders ok but raylib miscalculates the height of it by about half

Font :: struct {
    font: rl.Font,
    size: f32,
}

when ODIN_DEBUG {
    im_fell_filename : cstring = "assets/fonts/IM_Fell_English/IMFellEnglish-Regular.ttf"

    load_im_fell :: proc(size: f32) -> (font: Font) {
        font.font = rl.LoadFontEx(im_fell_filename, i32(size), nil, -1)
        assert(rl.IsFontReady(font.font))
        font.size = size
        return
    }
} else {
    im_fell_font := #load("../assets/fonts/IM_Fell_English/IMFellEnglish-Regular.ttf")

    load_im_fell :: proc(size: f32) -> (font: Font) {
        font.font = rl.LoadFontFromMemory(".ttf", raw_data(im_fell_font), i32(len(im_fell_font)), i32(size), nil, -1)
        assert(rl.IsFontReady(font.font))
        font.size = size
        return
    }
}

WHITE          := rl.Color{255, 255, 255, 255}
BLUE           := rl.Color{10, 10, 255, 255}
RED            := rl.Color{255, 10, 10, 255}
VERY_DARK_BLUE := rl.Color{4, 8, 13, 255}
GREEN          := rl.Color{10, 255, 10, 255}
AMBER          := rl.Color{255, 191, 0, 255}

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
    STATE_TITLE,
    STATE_MENU,
    STATE_PLAY,
    STATE_PAUSE,
    STATE_WIN,
    STATE_LOSE,
}

Game_Action :: enum {
    Start,
    Quit,
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

get_time :: proc() -> f32 {
    return f32(rl.GetTime())
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
    word_c: cstring,
    font: ^Font,
    dim: rl.Vector2,
    origin: rl.Vector2,
    typed_correctly: []bool,
    position: u32,
    alpha: f32,
    alpha_animation: Animation,
}

setup_challenge :: proc(challenge: ^Type_Challenge, word: string, font: ^Font, fade_time: f32) {
    challenge.word = word
    challenge.word_c = strings.clone_to_cstring(word)
    challenge.font = font
    challenge.typed_correctly = make([]bool, len(word))
    challenge.dim = rl.MeasureTextEx(challenge.font.font, challenge.word_c, challenge.font.size, 0.0)
    challenge.alpha_animation = start_animation(fade_time)
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
    neutral_color      := rl.ColorAlpha(WHITE, challenge.alpha)
    correct_color      := rl.ColorAlpha(GREEN, challenge.alpha)
    wrong_color        := rl.ColorAlpha(RED, challenge.alpha)
    under_cursor_color := rl.ColorAlpha(VERY_DARK_BLUE, challenge.alpha)
    cursor_color       := rl.ColorAlpha(AMBER, challenge.alpha)

    position := challenge.origin

    for c, i in challenge.word {
        c_str := fmt.ctprintf("%c", c)
        spacing : f32 = 0.0
        glyph_size := rl.MeasureTextEx(
            challenge.font.font,
            c_str,
            challenge.font.size,
            spacing,
        )
        text_color := neutral_color
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
                height = challenge.font.size,
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
            challenge.font.font,
            c,
            position,
            challenge.font.size,
            text_color,
        )
        position.x += glyph_size.x
    }

    demonic_dim := rl.Vector2{256.0, 256.0}
    demonic_pos := rl.Vector2{0.0, game_window.dim.y - 256.0 - 10.0}
    center_horizontally(&demonic_pos, demonic_dim, game_window.dim)
    texture_color := rl.ColorAlpha(GREEN, challenge.alpha)
    demonic_rect := rl.Rectangle{
        demonic_pos.x, demonic_pos.y,
        demonic_dim.x, demonic_dim.y,
    }
    render_demonic_sign(demonic_rect, challenge.word, texture_color)
}

MAX_NUM_CHALLENGES :: 8

// TODO: should this be Exclusive_Choice ?
// you can't select more than one
Multiple_Choice_Challenge :: struct {
    challenges: small_array.Small_Array(MAX_NUM_CHALLENGES, Type_Challenge),
    active_challenges: small_array.Small_Array(MAX_NUM_CHALLENGES, bool),
    actions: small_array.Small_Array(MAX_NUM_CHALLENGES, Game_Action),
    font: ^Font,
}

setup_multiple_choice :: proc(mcc: ^Multiple_Choice_Challenge, choices: []string, actions: []Game_Action) {
    assert(len(choices) == len(actions))
    for choice, i in choices {
        challenge := Type_Challenge{}
        setup_challenge(&challenge, choice, mcc.font, 1.2)
        small_array.push_back(&mcc.challenges, challenge)
        small_array.push_back(&mcc.actions, actions[i])
        small_array.push_back(&mcc.active_challenges, true) // assume all active
    }
}

multiple_choice_type_character :: proc(mcc: ^Multiple_Choice_Challenge, character: rune) {
    any_active := false
    for i in 0..<mcc.challenges.len {
        challenge := small_array.get_ptr(&mcc.challenges, i)
        active    := small_array.get_ptr(&mcc.active_challenges, i)
        if !active^ { continue }
        enter_challenge_character(challenge, character)
        if challenge_has_mistakes(challenge) {
            active^ = false
        }
        any_active |= active^
    }
    if !any_active {
        reset_multiple_choice(mcc)
    }
}

reset_multiple_choice :: proc(mcc: ^Multiple_Choice_Challenge) {
    for i in 0..<mcc.challenges.len {
        challenge := small_array.get_ptr(&mcc.challenges, i)
        reset_challenge(challenge)
        active    := small_array.get_ptr(&mcc.active_challenges, i)
        active^ = true
    }
}

update_multiple_choice_alpha :: proc(mcc: ^Multiple_Choice_Challenge) {
    for i in 0..<mcc.challenges.len {
        challenge := small_array.get_ptr(&mcc.challenges, i)
        update_challenge_alpha(challenge)
    }
}

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
    title_menu_challenges: Multiple_Choice_Challenge,
    level_data: Level_Data,

    title_font: Font,
    menu_font: Font,
    challenge_font: Font,
    demonic_font: Font,

    dt: f32,
    dim: rl.Vector2,

    quit: bool,
}

@(require)
game_window := Game_Window{}

center_horizontally :: proc(position: ^rl.Vector2, dim: rl.Vector2, within: rl.Vector2) {
    position.x = (within.x / 2.0) - (dim.x / 2.0)
}

center_vertically :: proc(position: ^rl.Vector2, dim: rl.Vector2, within: rl.Vector2) {
    position.y = (within.y / 2.0) - (dim.y / 2.0)
}

render_title :: proc() {
    char := rl.GetCharPressed()
    for char != 0 {
        enter_challenge_character(&game_window.title_challenge, char)
        if is_challenge_done(&game_window.title_challenge) {
            if challenge_has_mistakes(&game_window.title_challenge) {
                reset_challenge(&game_window.title_challenge)
            } else {
                game_window.game_state = .STATE_MENU
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
    rl.EndDrawing()
}

render_menu :: proc() {
    char := rl.GetCharPressed()
    for char != 0 {
        multiple_choice_type_character(&game_window.title_menu_challenges, char)

        // if is_challenge_done(&game_window.title_challenge) {
        //     if challenge_has_mistakes(&game_window.title_challenge) {
        //         reset_challenge(&game_window.title_challenge)
        //     } else {
        //         game_window.game_state = .STATE_MENU
        //         setup_level(&game_window.level_data)
        //     }
        // }
        char = rl.GetCharPressed()
    }
    update_multiple_choice_alpha(&game_window.title_menu_challenges)
    center_horizontally(
        &game_window.title_challenge.origin,
        game_window.title_challenge.dim,
        game_window.dim,
    )
    vertical_position := rl.Vector2{}
    rl.BeginDrawing()
    rl.ClearBackground(VERY_DARK_BLUE)
    render_challenge(&game_window.title_challenge)
    rl.EndDrawing()
}

render_game :: proc() {
    char := rl.GetCharPressed()
    for char != 0 {
        level_type_character(&game_window.level_data, char)
        if is_level_done(&game_window.level_data) {
            if game_window.level_data.points < game_window.level_data.num_challenges {
                game_window.game_state = .STATE_LOSE
            } else {
                game_window.game_state = .STATE_WIN
            }
            return
        }
        char = rl.GetCharPressed()
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

    rl.BeginDrawing()
    rl.ClearBackground(VERY_DARK_BLUE)
    render_challenge(challenge)
    rl.EndDrawing()
}

render_big_text :: proc(text: string) {
    rl.BeginDrawing()

    rl.ClearBackground(VERY_DARK_BLUE)

    c_text := strings.clone_to_cstring(text, context.temp_allocator)
	text_size := rl.MeasureTextEx(game_window.title_font.font, c_text, game_window.title_font.size, 0.0)
    position := rl.Vector2{}
    center_horizontally(&position, text_size, game_window.dim)
    center_vertically(&position, text_size, game_window.dim)
    rl.DrawTextEx(game_window.title_font.font, c_text, position, game_window.title_font.size, 0.0, WHITE)

    rl.EndDrawing()
}

render_win :: proc() {
    render_big_text(win)
}

render_lose :: proc() {
    render_big_text(lost)
}

update_and_render :: proc() {
    #partial switch game_window.game_state {
        case .STATE_TITLE: render_title()
        case .STATE_MENU:  render_menu()
        case .STATE_PLAY:  render_game()
        case .STATE_WIN:   render_win()
        case .STATE_LOSE:  render_lose()
    }
}

render_demonic_sign :: proc(rect: rl.Rectangle, word: string, color: rl.Color) {
    using math

	font := &game_window.demonic_font

    area_center := rl.Vector2{rect.x + rect.width / 2.0, rect.y + rect.height / 2.0}

    rl.DrawCircleLinesV(area_center, 127.0, color)
    inner_radius : f32 = 95.0
    rl.DrawCircleLinesV(area_center, inner_radius, color)

    single_char := [2]u8{}
    first_char := unicode.to_upper(rune(word[0]))
    single_char[0] = u8(first_char)

    glyph_size := rl.MeasureTextEx(font.font, cstring(&single_char[0]), font.size, 0.0)
    pos := rl.Vector2{rect.x + rect.width / 2.0 - glyph_size.x / 2.0, rect.y}

    rl.DrawTextEx(
        font.font,
        cstring(&single_char[0]),
        pos,
        font.size,
        0.0,
        color
    )
	angle_deg := 360.0 / f32(len(word))
	theta     := angle_deg * (PI / 180.0)

    char_cx := pos.x + glyph_size.x / 2.0 // center of the space in width
    char_cy := pos.y + glyph_size.y / 2.0
    area_cx := area_center.x
    area_cy := area_center.y
    input_points := make([]rl.Vector2, len(word)+1, context.temp_allocator)
    input_points[0].x = char_cx
    input_points[0].y = area_cy - inner_radius

    for i in 1..<len(word) {
        new_cx := cos(theta) * (char_cx - area_cx) - sin(theta) * (char_cy - area_cy) + area_cx
        new_cy := sin(theta) * (char_cx - area_cx) + cos(theta) * (char_cy - area_cy) + area_cy
        char_cx = new_cx
        char_cy = new_cy

        single_char[0] = word[i]
        glyph_size = rl.MeasureTextEx(font.font, cstring(&single_char[0]), font.size, 0.0)
        pos := rl.Vector2{char_cx - (glyph_size.x / 2.0), char_cy - (glyph_size.y / 2.0)}
        rl.DrawTextEx(font.font, cstring(&single_char[0]), pos, font.size, 0.0, color)

        vx := char_cx - area_cx
        vy := char_cy - area_cy
        magnitude := sqrt(vx*vx + vy*vy)
        vx /= magnitude
        vy /= magnitude
        input_points[i].x = area_cx + vx * inner_radius
        input_points[i].y = area_cy + vy * inner_radius
    }
    input_points[len(word)] = input_points[0]

    half_word := len(word) / 2
    even_word_length := false
    if len(word) % 2 == 0 {
        half_word -= 1
        even_word_length = true
    }
    i0 := 0
    i1 := (i0 + half_word) % len(word)
    for i in 0..<len(word) {
        p0 := input_points[i0]
        p1 := input_points[i1]
        rl.DrawLineV(p0, p1, color)

        // Reset/rotate if we've gotten back to 0 before the end
        if i1 == 0 {
            i0 += 1
            i1 += 1
        } else {
            i0 = (i0 + half_word) % len(word)
            i1 = (i1 + half_word) % len(word)
        }
    }
}

init_game :: proc() -> bool {
    update_window_dim()

    game_window.title_font     = load_im_fell(120.0)
    game_window.menu_font      = load_im_fell(52.0)
    game_window.challenge_font = load_im_fell(48.0)
    game_window.demonic_font   = load_im_fell(38.0)

    setup_challenge(&game_window.title_challenge, title, &game_window.title_font, TITLE_CHALLENGE_FADE_TIME)
    game_window.level_data.font = &game_window.challenge_font
    game_window.title_menu_challenges.font = &game_window.menu_font
    menu_entries := []string{"Start game", "Quit"}
    menu_actions := []Game_Action{.Start, .Quit}
    setup_multiple_choice(&game_window.title_menu_challenges, menu_entries, menu_actions)

    return true
}

process_input :: proc() {
    if rl.IsKeyDown(rl.KeyboardKey.LEFT_CONTROL) && rl.IsKeyDown(rl.KeyboardKey.Q) {
        game_window.quit = true
    }
    if rl.WindowShouldClose() {
        game_window.quit = true
    }
}

update_window_dim :: proc() {
    game_window.dim.x = f32(rl.GetRenderWidth())
    game_window.dim.y = f32(rl.GetRenderHeight())
}

main :: proc() {
	lowest_level := log.Level.Info
	when ODIN_DEBUG {
		lowest_level = log.Level.Debug
	}
	context.logger = log.create_console_logger(lowest = lowest_level)

    rl.InitWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,  "Ludum Dare 55: Summoning")
    rl.SetExitKey(rl.KeyboardKey.KEY_NULL)

    if !init_game() {
        os.exit(1)
    }

    log.infof("going to start the render loop")

    for !game_window.quit {
		if err := free_all(context.temp_allocator); err != .None {
			log.errorf("temp_allocator.free_all err == {}", err);
        }

        game_window.dt = rl.GetFrameTime()
        process_input()
        update_and_render()
    }

    rl.CloseWindow()
}
