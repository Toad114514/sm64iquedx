// Coloured CJK HUD cells, provided at runtime from the player's 神游 (iQue) rom.
//
// This file used to hold a run-length-compressed copy of 124 16x16 RGBA16
// cells (about 8000 runs parsed out of the decomp). The cells are read straight
// out of segment 2 at startup instead, so there is nothing to compress and
// nothing to ship -- cn_font_get_hud_glyph() copies the one cell it needs into
// its fixed per-code slot.
//
// Where the anchor comes from
// ---------------------------
// main_hud_lut (vaddr 0x02008250) holds 58 ASCII entries (codes 0x00-0x39)
// followed by the coloured CJK run, which starts at code 0x3A and marches
// through segment 2 with a uniform 0x200 stride for 124 entries, starting at
// segment 2 offset 0x04A00. That boundary is not a guess: the official CN
// print_hud_lut_string() advances 12px below 0x3A and 16px from 0x3A on.
//
// Cell format: 16x16 RGBA16, big-endian texels, exactly what gfx_pc's
// import_texture_rgba16() unpacks.
//
// Blank cells are real and intentional: iQue pads its titles to a fixed 6-glyph
// line, and 4 of the 12 codes in TEXT_PAUSE (0x86, 0x88, 0x8F, 0x91) are
// entirely transparent. A cell of zeroes is a valid glyph, not a failure.

#ifndef CN_HUD_GLYPHS_RGBA16_H
#define CN_HUD_GLYPHS_RGBA16_H

#define CN_HUD_FIRST  0x3A
#define CN_HUD_LAST   0xB5
#define CN_HUD_COUNT  124
#define CN_HUD_TEXELS 256      // texels per cell (16x16)
#define CN_HUD_CELL_BYTES 512  // bytes per cell (RGBA16)

#endif // CN_HUD_GLYPHS_RGBA16_H
