package main

import "core:c"
import rl "vendor:raylib"
import stbtt "vendor:stb/truetype"

Font :: struct {
    raylib_font: rl.Font,
    size: f32,

    info: stbtt.fontinfo,
    scale: f32,
    ascent: f32,
    descent: f32,
    line_gap: f32,
    line_height: f32,
    row_height: f32,
}

init_font :: proc(font: ^Font, font_mem: []u8, font_size: f32) -> b32 {
    // We pass empty array of codepoints to bake the default ASCII range in
    font.raylib_font = load_font_from_memory(".ttf", &font_mem[0], i32(len(font_mem)), i32(font_size), nil, -1)
    assert(rl.IsFontReady(font.raylib_font))

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

FONT_TTF_DEFAULT_CHARS_PADDING :: 4

// LoadFontFromMemory copied from raylib and adjusted so we can use the packing
// mechanism that works rather than the busted broken default one
load_font_from_memory :: proc(fileType: cstring, fileData: rawptr, dataSize: c.int, fontSize: c.int, codepoints: [^]rune, codepointCount: c.int) -> rl.Font {
    font := rl.Font{}

    font.baseSize = fontSize;
    font.glyphCount = (codepointCount > 0)? codepointCount : 95;
    font.glyphPadding = 0;
    font.glyphs = rl.LoadFontData(fileData, dataSize, font.baseSize, codepoints, font.glyphCount, rl.FontType.DEFAULT);
    if (font.glyphs != nil) {
        font.glyphPadding = FONT_TTF_DEFAULT_CHARS_PADDING;

		// NOTE(will): we pass packingMethod=1 here so it'll work!!!
        atlas := rl.GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, font.baseSize, font.glyphPadding, 1);
        font.texture = rl.LoadTextureFromImage(atlas);

        // Update glyphs[i].image to use alpha, required to be used on ImageDrawText()
        for i in 0..<font.glyphCount {
            rl.UnloadImage(font.glyphs[i].image);
            font.glyphs[i].image = rl.ImageFromImage(atlas, font.recs[i]);
        }

        rl.UnloadImage(atlas);
    } else {
        font = rl.GetFontDefault();
    }

    return font;
}
