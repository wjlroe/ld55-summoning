package main

import "core:c"
import rl "vendor:raylib"

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
