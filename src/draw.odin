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
	Draw_With_Shader,
}

Render_Data :: union {
	Color,
	Quad_Shader_Call,
}

Render_Setting :: enum {
	Alpha_Blended,
}

Render_Settings :: bit_set[Render_Setting]

Render_Group :: struct {
	command: Render_Command,
	data: Render_Data,
	settings: Render_Settings,
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
		if .Alpha_Blended in group.settings {
			gl.Enable(gl.BLEND)
			gl.BlendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA)
		}

		switch group.command {
			case .Clear: {
				c := group.data.(Color)
				gl.ClearColor(c.r, c.g, c.b, c.a)
				gl.Clear(gl.COLOR_BUFFER_BIT)
			}
			case .Draw_With_Shader: {
				#partial switch v in group.data {
					case Quad_Shader_Call: {
						shader_call := group.data.(Quad_Shader_Call)
						shader, ok := small_array.get_safe(global_shaders, shader_call.shader_id)
						assert(ok)
						if !ok {
							return
						}
						gl.UseProgram(shader.program_id)

						for texture in small_array.slice(&shader_call.textures) {
							gl.ActiveTexture(gl.TEXTURE0 + texture.shader_index)
							gl.BindTexture(gl.TEXTURE_2D, texture.id)
						}
					}
				}
			}
		}
	}
}

clear_background :: proc(color: Color) {
	push_render_group(Render_Group{ command = .Clear, data = color })
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
	group := Render_Group{ command = .Draw_With_Shader }
	shader_call := Quad_Shader_Call{}
	shader_call.shader_id = global_quad_shader.shader_id
	group.data = shader_call
	push_render_group(group)
}

draw_rect_outline :: proc(rect: rectangle2, color: Color, thickness: int) {

}

draw_rect_rounded_filled :: proc(rect: rectangle2, color: Color, roundness: f32) {

}

draw_rect_rounded_outline :: proc(rect: rectangle2, color: Color, roundness: f32, thickness: int) {

}
