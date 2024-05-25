package main

import "core:c"
import "core:log"
import "core:container/lru"
import "core:mem"
import gl "vendor:OpenGL"
import stbtt "vendor:stb/truetype"

Glyph :: struct {
    bounding_box: rectangle2,
	advance: f32,
	lsb: f32,
    tex_rect: rectangle2,
}

Font :: struct {
    name: string,
    memory: ^[]u8,
    font_index: i32,
    size: f32,
    scale: f32,
    info: stbtt.fontinfo,
    ascent: f32,
    descent: f32,
    line_gap: f32,
    line_height: f32,
    row_height: f32,
    space_width: f32,
    first_character: rune,
    last_character: rune,
    num_of_characters: int,
    glyphs: []Glyph,
    texture_dim: v2s,
    texture_id: u32,
}

init_font :: proc(font: ^Font, font_mem: []u8, font_size: f32) -> (ok: bool) {
    font.size = font_size
    font.font_index = 0
    font.first_character = '!'
    font.last_character = 'z'
    font.num_of_characters = int((font.last_character - font.first_character) + 1)
    font_offset : i32 = 0
	if (!stbtt.InitFont(&font.info, &font_mem[0], font_offset)) {
        return false
	}

    font.scale = stbtt.ScaleForPixelHeight(&font.info, font.size)

	// Vertical metrics
    ascent, descent, line_gap : i32
	stbtt.GetFontVMetrics(&font.info, &ascent, &descent, &line_gap)
	font.ascent = font.scale * f32(ascent)
	font.descent = font.scale * f32(descent)
	font.line_gap = font.scale * f32(line_gap)
	font.line_height = font.ascent - font.descent
	font.row_height = font.line_height + font.line_gap

	memory, err := mem.alloc((font.num_of_characters) * size_of(stbtt.packedchar))
    if err != nil {
        log.errorf("couldn't allocate memory for packedchars array: {}", err)
        return
    }
    packed_chars := cast([^]stbtt.packedchar)memory
	defer mem.free(packed_chars)
	pack_context : stbtt.pack_context
	font.texture_dim = v2s{512, 512}
	tex_buffer := make([^]u8, font.texture_dim.x * font.texture_dim.y)
    defer free(tex_buffer)
	for {
		if (stbtt.PackBegin(&pack_context, tex_buffer, font.texture_dim.x, font.texture_dim.y, 0, 1, nil) == 0) {
			log.error("Failed to PackBegin!")
			return
		}
		if (stbtt.PackFontRange(&pack_context, &font_mem[0], font.font_index, font.size, i32(font.first_character), i32(font.num_of_characters), &packed_chars[0]) == 0) {
			old_size := int(font.texture_dim.x * font.texture_dim.y)
			font.texture_dim.x *= 2
            font.texture_dim.y *= 2
			new_size := int(font.texture_dim.x * font.texture_dim.y)
            new_mem, resize_err := mem.resize(tex_buffer, old_size, new_size)
            if resize_err != nil {
                log.errorf("couldn't resize memory for font texture: {}", resize_err)
                return
            }
			tex_buffer = cast([^]u8)new_mem
		} else {
			stbtt.PackEnd(&pack_context)
			break
		}
	}

	x0, y0, x1, y1, advance, lsb : i32
	font.glyphs = make([]Glyph, font.num_of_characters)
	for character in font.first_character..=font.last_character {
        glyph_idx := stbtt.FindGlyphIndex(&font.info, character)
		idx := u32(character) - u32(font.first_character)
		packed_char := packed_chars[idx]
		stbtt.GetGlyphHMetrics(&font.info, glyph_idx, &advance, &lsb)
		stbtt.GetGlyphBitmapBox(&font.info, glyph_idx, font.scale, font.scale, &x0, &y0, &x1, &y1)
        font.glyphs[idx].bounding_box = rect_ints_to_floats(rect_from_points_i32(x0, y0, x1, y1))
		font.glyphs[idx].advance = f32(advance) * font.scale
		font.glyphs[idx].lsb = f32(lsb) * font.scale
		tex_x0 := f32(packed_char.x0) / f32(font.texture_dim.x)
		tex_x1 := f32(packed_char.x1) / f32(font.texture_dim.x)
		tex_y0 := f32(packed_char.y0) / f32(font.texture_dim.y)
		tex_y1 := f32(packed_char.y1) / f32(font.texture_dim.y)
        font.glyphs[idx].tex_rect = rectangle2{v2{tex_x0, tex_y1}, v2{tex_x1, tex_y0}}
	}

	space_glyph := stbtt.FindGlyphIndex(&font.info, ' ')
	stbtt.GetGlyphHMetrics(&font.info, space_glyph, &advance, &lsb)
	stbtt.GetGlyphBitmapBox(&font.info, space_glyph, font.scale, font.scale, &x0, &y0, &x1, &y1)
	font.space_width = (f32(x1) - f32(x0)) //  * font.font_scale

    gl.GenTextures(1, &font.texture_id)
	gl.BindTexture(gl.TEXTURE_2D, font.texture_id)
	gl.TexImage2D(gl.TEXTURE_2D, 0, gl.RED, font.texture_dim.x, font.texture_dim.y, 0, gl.RED, gl.UNSIGNED_BYTE, tex_buffer)
	gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR)
    gl.BindTexture(gl.TEXTURE_2D, 0)

    ok = true
    return
}

measure_rune :: proc(font: ^Font, c: rune) -> (size: v2) {
    glyph_idx := i32(c) - i32(font.first_character)
    return rect_dim(font.glyphs[glyph_idx].bounding_box)
}

measure_text :: proc(font: ^Font, text: string) -> (size: v2) {
    for c in text {
        c_size := measure_rune(font, c)
        size.x += c_size.x
        size.y = max(c_size.y, size.y)
    }
	return
}
