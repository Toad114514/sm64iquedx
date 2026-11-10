// CJK glyph table, provided at runtime from the player's 神游 (iQue) rom.
//
// This file used to hold 34304 bytes of extracted glyph bitmaps. They are now
// read straight out of the cartridge at startup (src/game/cn_rom_assets.c,
// segment 2 offset 0x16880) and expanded into the orientation the renderer
// expects -- the same transform, done once at load instead of at build time.
//
// Layout is unchanged: 16x16, 1bpp, row-major MSb-first, 32 bytes per glyph,
// addressed by CODEPOINT (index = code - 0x0100), every slot in range present.
// cn_font.c indexes it exactly as before; only the storage moved.

#ifndef CN_GLYPHS_16X16_H
#define CN_GLYPHS_16X16_H

#define CN_GLYPH_FIRST 0x0100
#define CN_GLYPH_LAST  0x052F
#define CN_GLYPH_COUNT 1072
#define CN_GLYPH_BYTES 34304

// NULL until cn_rom_assets_load() has succeeded. Callers cannot reach it
// before then: the pack is only marked active after a successful load.
extern const u8 *sCnGlyphs16x16;

#endif // CN_GLYPHS_16X16_H
