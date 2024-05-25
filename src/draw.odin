package main

import gl "vendor:OpenGL"
import "core:container/small_array"

Render_Command :: enum {
	Clear,
	Draw_With_Shader,
}

Render_Data :: union {
	Color,
	Textured_Single_Quad_Shader_Call,
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
					case Textured_Single_Quad_Shader_Call: {
						shader_call := group.data.(Textured_Single_Quad_Shader_Call)
						shader, ok := small_array.get_safe(global_shaders, shader_call.shader_id)
						assert(ok)
						if !ok {
							return
						}
						gl.UseProgram(shader.program_id)
						assert(shader.vao > 0)
						gl.BindVertexArray(shader.vao)

						for uniform in small_array.slice(&shader_call.uniforms) {
							switch &v in uniform.data {
								case f32: gl.Uniform1f(uniform.location, v)
								case u32: gl.Uniform1ui(uniform.location, v)
								case i32: gl.Uniform1i(uniform.location, v)
								case v2:  gl.Uniform2fv(uniform.location, 1, &(cast([]f32)v[:])[0])
								case v3:  gl.Uniform3fv(uniform.location, 1, &(cast([]f32)v[:])[0])
								case v4:  gl.Uniform4fv(uniform.location, 1, &(cast([]f32)v[:])[0])
								case matrix[4,4]f32: gl.UniformMatrix4fv(uniform.location, 1, gl.FALSE, &v[0][0])
							}
						}

						for texture in small_array.slice(&shader_call.textures) {
							gl.ActiveTexture(gl.TEXTURE0 + texture.shader_index)
							gl.BindTexture(gl.TEXTURE_2D, texture.id)
						}

						if small_array.len(shader_call.textures) == 0 {
							gl.BindTexture(gl.TEXTURE_2D, 0)
						}

						gl.BindBuffer(gl.ARRAY_BUFFER, shader.vbo)
						for &vertex_group in small_array.slice(&shader_call.vertices) {
							gl.BufferData(gl.ARRAY_BUFFER, size_of(vertex_group.vertices), &vertex_group.vertices, gl.STATIC_DRAW)
							// FIXME: hardcoded 6 for length of element buffer and type of them
							gl.DrawElements(gl.TRIANGLES, 6, gl.UNSIGNED_INT, nil)
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

draw_text :: proc(font: ^Font, position: v2, text: string, color: Color) -> (size: v2) {
	group := Render_Group { command = .Draw_With_Shader }
	shader_call := Textured_Single_Quad_Shader_Call{}
	shader_call.shader_id = global_quad_shader.shader_id
	push_uniform_binding(&shader_call, global_quad_shader.ortho, game_window.ortho_matrix)
	push_uniform_binding(&shader_call, global_quad_shader.color, v4(color))
	push_texture_binding(&shader_call, font.texture_id, 0)
	push_render_group(group)
	assert(false)
	return
}

draw_rune :: proc(font: ^Font, position: v2, c: rune, color: Color) -> (size: v2) {
	position := position
	group := Render_Group { command = .Draw_With_Shader, settings = { .Alpha_Blended } }
	shader_call := Textured_Single_Quad_Shader_Call{}
	shader_call.shader_id = global_quad_shader.shader_id
	settings : Shader_Settings = {.Sample_Font_Texture}
	push_uniform_binding(&shader_call, global_quad_shader.settings, i32(transmute(u8)settings))
	push_uniform_binding(&shader_call, global_quad_shader.ortho, game_window.ortho_matrix)
	push_uniform_binding(&shader_call, global_quad_shader.color, v4(color))
	push_texture_binding(&shader_call, font.texture_id, 0)
	glyph_idx := u32(c) - u32(font.first_character)
	glyph := font.glyphs[glyph_idx]
	glyph_dim := rect_dim(glyph.bounding_box)
	size = glyph_dim
	position.y -= glyph.bounding_box.max.y
	rect := rect_min_dim(position, glyph_dim)
	vertex_group := textured_quad(rect, 0.0, glyph.tex_rect)
	push_vertex_group(&shader_call, vertex_group)
	group.data = shader_call
	push_render_group(group)
	return
}

textured_quad :: proc(pos: rectangle2, z: f32, tex: rectangle2) -> Textured_Quad {
	return Textured_Quad{
		vertices = [?]Textured_Vertex{
			{position = {pos.min.x, pos.min.y, z}, texture = {tex.min.x, tex.min.y}}, // top-left
			{position = {pos.min.x, pos.max.y, z}, texture = {tex.min.x, tex.max.y}}, // bottom-left
			{position = {pos.max.x, pos.max.y, z}, texture = {tex.max.x, tex.max.y}}, // bottom-right
			{position = {pos.max.x, pos.min.y, z}, texture = {tex.max.x, tex.min.y}}, // top-right
		}
	}
}

draw_rect_filled :: proc(rect: rectangle2, color: Color) {
	group := Render_Group{ command = .Draw_With_Shader }
	shader_call := Textured_Single_Quad_Shader_Call{}
	shader_call.shader_id = global_quad_shader.shader_id
	push_uniform_binding(&shader_call, global_quad_shader.settings, i32(0))
	push_uniform_binding(&shader_call, global_quad_shader.ortho, game_window.ortho_matrix)
	push_uniform_binding(&shader_call, global_quad_shader.color, v4(color))
	tex_rect := rect_min_dim(v2{0.0, 0.0}, v2{1.0, 1.0})
	vertex_group := textured_quad(rect, 0.5, tex_rect)
	push_vertex_group(&shader_call, vertex_group)
	group.data = shader_call
	push_render_group(group)
}

draw_rect_outline :: proc(rect: rectangle2, color: Color, thickness: int) {

}

draw_rect_rounded_filled :: proc(rect: rectangle2, color: Color, roundness: f32) {

}

draw_rect_rounded_outline :: proc(rect: rectangle2, color: Color, roundness: f32, thickness: int) {

}
