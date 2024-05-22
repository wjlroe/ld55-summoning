package main

import "core:c"
import stbtt "vendor:stb/truetype"

Font :: struct {
    size: f32,

    info: stbtt.fontinfo,
    scale: f32,
    ascent: f32,
    descent: f32,
    line_gap: f32,
    line_height: f32,
    row_height: f32,
}

init_font :: proc(font: ^Font, font_mem: []u8, font_size: f32) -> bool {
    font.size = font_size

    font_offset : i32 = 0
	if (!stbtt.InitFont(&font.info, &font_mem[0], font_offset)) {
        return false
	}

	font.scale = stbtt.ScaleForPixelHeight(&font.info, font_size)

	// Vertical metrics
	ascent, descent, line_gap : i32
	stbtt.GetFontVMetrics(&font.info, &ascent, &descent, &line_gap)
	font.ascent = font.scale * f32(ascent)
	font.descent = font.scale * f32(descent)
	font.line_gap = font.scale * f32(line_gap)
	font.line_height = font.ascent - font.descent
	font.row_height = font.line_height + font.line_gap

    return true
}
