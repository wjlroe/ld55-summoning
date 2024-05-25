package main

import "core:container/small_array"
import "core:log"

Mod :: enum{
	Control,
	Super, // Cmd on Mac
	Alt,
	Shift,
}

Mods :: bit_set[Mod]

when ODIN_OS == .Windows {
	cmd_or_control : Mod = .Control
}

when ODIN_OS == .Darwin {
	cmd_or_control : Mod = .Super
}

when ODIN_OS == .Linux {
	cmd_or_control : Mod = .Control
}

Key :: enum{
	LeftArrow,
	RightArrow,
	UpArrow,
	DownArrow,
	Home,
	End,
	PageUp,
	PageDown,
	Backspace,
	Escape,
	Enter,
	Q,
}

Key_Or_Char :: union {
	Key,
	rune,
}

Keyboard_Input :: struct {
	key: Key_Or_Char,
	mods: Mods,
	is_down: bool,
	was_down: bool,
}

Mouse_Button :: enum{
	LeftMouse,
	RightMouse,
	MiddleMouse,
	ScrollWheel,
}

Mouse_Input :: struct {
	position: v2,
	scroll: v2,
	mods: Mods,
	button: Mouse_Button,
	is_down: bool,
	was_down: bool,
}

Input :: struct {
	for_window: int,
	processed: bool,
	data: union {
		Keyboard_Input,
		Mouse_Input,
	},
}

is_keyboard_input :: proc(input: ^Input) -> (ki: Keyboard_Input, is_ki: bool) {
	ki, is_ki = input.data.(Keyboard_Input)
	return
}

is_character_input :: proc(input: ^Input) -> (c: rune, is_char: bool) {
	ki, ok := is_keyboard_input(input)
	if !ok { return }
	c, is_char = ki.key.(rune)
	return
}

keyboard_input_is_control_char :: proc(keyboard_input: ^Keyboard_Input, key: Key_Or_Char) -> (is_command: bool) {
	if !keyboard_input.is_down {
		return
	}
	if keyboard_input.was_down {
		return
	}
	if keyboard_input.mods != {cmd_or_control} {
		return
	}

	if keyboard_input.key == key {
		is_command = true
	}

	return
}

MAX_FRAME_INPUTS :: 32
Frame_Inputs :: struct {
	inputs: small_array.Small_Array(MAX_FRAME_INPUTS, Input)
}

global_frame_inputs := Frame_Inputs{}

push_input :: proc(input: Input) -> (ok: bool) {
	log.debugf("input: {}", input)
	ok = small_array.push_back(&global_frame_inputs.inputs, input)
	assert(ok)
	return
}

reset_frame_inputs :: proc() {
	small_array.clear(&global_frame_inputs.inputs)
}

get_char_pressed :: proc() -> (c: rune) {
    for &input in small_array.slice(&global_frame_inputs.inputs) {
		this_c, is_char := is_character_input(&input)
		if !input.processed && is_char {
			input.processed = true
			c = this_c
			return
		}
	}
	return
}

control_key :: proc(key: Key) -> (ki: Keyboard_Input) {
	ki.key = key
	ki.mods = {cmd_or_control}
	ki.is_down = true
	ki.was_down = false
	return
}

is_key_down :: proc(keyboard_input: Keyboard_Input) -> (down: bool) {
    for &input in small_array.slice(&global_frame_inputs.inputs) {
		ki, is_ki := is_keyboard_input(&input)
		if is_ki && !input.processed && ki == keyboard_input {
			input.processed = true
			down = true
			return
		}
	}
	return
}
