package main

import "core:c"
import gl "vendor:OpenGL"

Texture_Format :: enum {
	Red,
	Red_Green,
	Red_Green_Blue,
	Blue_Green_Red,
	Red_Green_Blue_Alpha,
	Blue_Green_Red_Alpha,
}

gl_format_for :: #force_inline proc(format: Texture_Format) -> i32 {
	switch format {
		case .Red:                  return gl.RED
		case .Red_Green:            return gl.RG
		case .Red_Green_Blue:       return gl.RGB
		case .Blue_Green_Red:       return gl.BGR
		case .Red_Green_Blue_Alpha: return gl.RGBA
		case .Blue_Green_Red_Alpha: return gl.BGRA
	}
	return -1
}

Texture :: struct {
	id:  u32,
	dim: v2s,
}

init_texture :: proc(texture: ^Texture, format: Texture_Format, memory: [^]u8) {
    gl.GenTextures(1, &texture.id)
	gl.BindTexture(gl.TEXTURE_2D, texture.id)
	gl_format := gl_format_for(format)
	assert(gl_format > 0)
	gl.TexImage2D(gl.TEXTURE_2D, 0, gl_format, texture.dim.x, texture.dim.y, 0, u32(gl_format), gl.UNSIGNED_BYTE, memory)
	gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR)
    gl.BindTexture(gl.TEXTURE_2D, 0)
}
