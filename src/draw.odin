package main

import gl "vendor:OpenGL"
import "core:container/small_array"

Font_Spec :: struct {
	font_id: int,
	size: f32,
}

// TODO: stack of Font_Spec (font_id, size - to key the glyph cache from)
MAX_FONT_STACK_SIZE :: 16
font_stack := small_array.Small_Array(MAX_FONT_STACK_SIZE, Font_Spec){}

push_font :: proc(font_id: int, size: f32) -> (ok: bool) {
	ok = small_array.push_back(&font_stack, Font_Spec{font_id, size})
	assert(ok)
	return ok
}

pop_font :: proc() {
	small_array.pop_back(&font_stack)
}

@(deferred_none=pop_font)
scoped_font :: proc(font_id: int, size: f32) {
	push_font(font_id, size)
}

get_font :: proc() -> Font_Spec {
	return font_stack.data[font_stack.len - 1]
}

Render_Command :: enum {
	Clear,
}

Render_Data :: union {
	Color,
}

Render_Group :: struct {
	shader_id: int,
	command: Render_Command,
	data: Render_Data,
}

MAX_RENDER_STACK_SIZE :: 1024
render_stack := small_array.Small_Array(MAX_RENDER_STACK_SIZE, Render_Group){}

push_render_group :: proc(group: Render_Group) {
	ok := small_array.push_back(&render_stack, group)
	assert(ok)
}

begin_drawing :: proc() {
	small_array.clear(&render_stack)
}

end_drawing :: proc() {
	for group in small_array.slice(&render_stack) {
		if group.shader_id >= 0 {
			gl.UseProgram(global_quad_shader.program_id)
		}
		switch group.command {
			case .Clear: {
				c := group.data.(Color)
				gl.ClearColor(c.r, c.g, c.b, c.a)
				gl.Clear(gl.COLOR_BUFFER_BIT)
			}
		}
	}
}

clear_background :: proc(color: Color) {
	push_render_group(Render_Group{ shader_id = -1, command = .Clear, data = color })
}

draw_text :: proc(position: v2, text: string, color: Color) -> (size: v2) {
	font := get_font()
	return
}

draw_rune :: proc(position: v2, c: rune, color: Color) -> (size: v2) {
	font := get_font()
	return
}

measure_text :: proc(text: string) -> (size: v2) {
	font := get_font()
	return
}

measure_rune :: proc(c: rune) -> (size: v2) {
	font := get_font()
	return
}

draw_rect_filled :: proc(rect: rectangle2, color: Color) {

}

draw_rect_outline :: proc(rect: rectangle2, color: Color, thickness: int) {

}

draw_rect_rounded_filled :: proc(rect: rectangle2, color: Color, roundness: f32) {

}

draw_rect_rounded_outline :: proc(rect: rectangle2, color: Color, roundness: f32, thickness: int) {

}
