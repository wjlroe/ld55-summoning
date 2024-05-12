package main

import rl "vendor:raylib"

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

Text_Group :: struct {
	text: string,
	glyph_rects: []rl.Rectangle,
	rect: rl.Rectangle,
}

new_text_group :: proc(text: string) ->Text_Group {
	group := Text_Group {
		text = text,
		glyph_rects = make([]rl.Rectangle, len(text)),
	}


	for c, i in text {
		// group.glyph_rects = rl.GetGlyph,
	}

	return group
}

DEFAULT_CHALLENGE_FADE_SPEED :: 5.5

Type_Challenge :: struct {
    word: Text_Group,
    typed_correctly: []b32,
    position: u32,
    alpha: f32,
    alpha_fade_speed: f32,
}

type_challenge :: #force_inline proc(word: string, font_size: f32) -> Type_Challenge {
    return Type_Challenge{
        word = new_text_group(word, font_size),
        typed_correctly = make([]b32, len(word)),
        alpha_fade_speed = DEFAULT_CHALLENGE_FADE_SPEED,
    }
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

    dt: f32,
    frame_number: u64,
}

@(require)
game_window := Game_Window{
    title_challenge = type_challenge(title),
}

render_menu :: proc() {
    update_challenge_alpha(&game_window.title_challenge, game_window.dt)

    rl.BeginDrawing()
    rl.ClearBackground(VERY_DARK_BLUE)
    text_color := rl.ColorAlpha(WHITE, game_window.title_challenge.alpha)
    rl.DrawText(title, 190, 200, 20, text_color)
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

main :: proc() {
    woot := type_challenge("woot")
    rl.InitWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "Ludum Dare 55: Summoning")

    for !rl.WindowShouldClose() {
        game_window.dt = rl.GetFrameTime()
        update_and_render()
    }

    rl.CloseWindow()
}
