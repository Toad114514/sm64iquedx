// cn_rom_assets.h - the Chinese (iQue) text pack, read out of the ROM at
// runtime instead of compiled into the binary.
//
// Why this exists
// ---------------
// An earlier revision baked the glyphs, the dialog text and the localised
// textures into the executable as C arrays. That works, but it means the binary
// *is* a copy of Nintendo/iQue assets: anyone who downloads a build owns the
// artwork and the translation without ever supplying the cartridge, and the
// project cannot honestly claim otherwise.
//
// So the pack is now streamed from the player's own 神游 dump:
//
//   * the engine still runs on the US image (that is where the game comes from)
//   * the Chinese pack is a *second* file, opened read-only, and every byte of
//     the pack comes out of it
//   * nothing here is derived data except the offsets below -- and an offset is
//     a fact about the cartridge's layout, not a copy of its contents
//
// What lives where
// ----------------
// Everything text-related is inside segment 2, which the ROM stores as one MIO0
// block at file offset 0x0E7500 and which expands to 201344 bytes. The layout
// was measured, not guessed, and each anchor below was confirmed by reproducing
// the previously-compiled arrays byte for byte:
//
//   0x16880  1072 CJK glyphs, two 16-byte ia1 tiles each (verified 1072/1072)
//   0x04A00  124 coloured HUD cells, 16x16 RGBA16, 0x200 bytes each (124/124)
//   0x28B8C  dialog table, 170 entries x 16 bytes (170/170 vs. linesPerBox)
//   0x29ABC  course name table, 26 pointers
//   0x2A1A8  act name table, 97 pointers
//
// .workbuddy/probe_ique_seg2.py and probe_ique_tables.py re-derive all of this
// from the ROM; run them if a future dump ever disagrees.

#ifndef CN_ROM_ASSETS_H
#define CN_ROM_ASSETS_H

#include "types.h"

// --- Segment 2 -------------------------------------------------------------
// File offset of the MIO0 block that holds segment 2, and its decompressed size.
#define CN_ROM_SEG2_MIO0_OFF 0x0E7500
#define CN_ROM_SEG2_SIZE     201344

// Segment 2 is loaded at 0x02000000, and the ROM's own tables store absolute
// addresses in that window. Converting is a subtract, nothing more.
#define CN_ROM_SEG_BASE      0x02000000

// --- Anchors inside segment 2 ---------------------------------------------
#define CN_ROM_GLYPH_OFF     0x16880
#define CN_ROM_GLYPH_COUNT   1072
#define CN_ROM_GLYPH_BYTES   (CN_ROM_GLYPH_COUNT * 32)

#define CN_ROM_HUD_OFF       0x04A00
#define CN_ROM_HUD_COUNT     124
#define CN_ROM_HUD_CELL_SIZE 512   // 16x16 RGBA16

#define CN_ROM_DIALOG_OFF    0x28B8C
#define CN_ROM_DIALOG_COUNT  170
#define CN_ROM_DIALOG_STRIDE 16
#define CN_ROM_DIALOG_LINES  4     // u8  linesPerBox
// The two geometry fields are u16, not u8, and they do NOT start at 5: byte 5
// is padding. Measured against the official source entry by entry
// (DIALOG_000 = left 30 / width 200, DIALOG_001 = 95 / 200), and pinned by
// .workbuddy/verify_cn_dialogs.py. Reading leftOffset as a byte at 5 yields a
// constant 0, which is why this used to look plausible and be wrong.
#define CN_ROM_DIALOG_LEFT   6     // u16 leftOffset
#define CN_ROM_DIALOG_WIDTH  8     // u16 width
#define CN_ROM_DIALOG_STR    12    // u32 address in the 0x02000000 window

#define CN_ROM_COURSE_OFF    0x29ABC
#define CN_ROM_COURSE_COUNT  26
#define CN_ROM_ACT_OFF       0x2A1A8
#define CN_ROM_ACT_COUNT     97

// The act-name table is 97 entries: 15 stages x 6 acts, then the seven extra
// texts the port keeps as vanilla ROM buffers (Lua's EXTRA_TEXT). Only the
// first 90 slots are Chinese; overlaying 90..96 would change what the Lua text
// layer sees for extra texts, so the loader stops at 90.
#define CN_ROM_STAGE_COUNT    15
#define CN_ROM_ACT_MAIN_COUNT (CN_ROM_STAGE_COUNT * 6)

// Scratch space for one name, now in UTF-8. The longest iQue entry is 30 bytes
// (an act name) = 15 CJK glyphs = 45 UTF-8 bytes, plus the 3-byte vanilla
// prefix, a newline and the terminator. Rounded up so a future dump with a
// slightly longer line still fits.
#define CN_ROM_NAME_BYTES    64

// --- Localised textures ----------------------------------------------------
// The twelve iQue textures that were redrawn *in place*: same segment, same
// offset, different pixels. They live inside three MIO0 blocks (the CN rom's
// file layout differs from the US one, so their US rom addresses do not work
// here), and are decompressed once at startup into one contiguous buffer laid
// out in the same order as cn_tex.c's slot table.
//
//   0 = power_meter_left     1 = power_meter_right
//   2 = menu_course_upper    3 = menu_course_lower
//   4..11 = title frames 0..7
#define CN_ROM_TEXTURE_COUNT 12
#define CN_ROM_TEXTURE_BYTES 41984   // 4 * 4096 + 8 * 3200

// Open the rom and pull segment 2 out of it. Returns false when the file is
// missing, is not the expected dump (wrong magic / wrong expanded size), or the
// anchors do not validate -- in which case the caller must leave the pack off
// rather than point at a buffer full of zeroes.
bool cn_rom_assets_load(const char *romPath);

// True once cn_rom_assets_load() has succeeded.
bool cn_rom_assets_ready(void);

// 1072 glyphs, 16x16 1bpp, row-major MSb-first, 32 bytes per glyph, addressed
// by codepoint (index = code - 0x0100). Already transformed into the orientation
// the renderer expects, exactly as the old generated table was.
const u8 *cn_rom_glyphs(void);

// Pointer to one RGBA16 cell inside the segment buffer (big-endian texels, the
// order gfx_pc's import_texture_rgba16() reads). No copy.
const u8 *cn_rom_hud_cell(u32 index);

// Dialog accessors; index is the DIALOG_NNN number.
u32       cn_rom_dialog_count(void);
u32       cn_rom_dialog_lines(u32 index);
u32       cn_rom_dialog_left(u32 index);
u32       cn_rom_dialog_width(u32 index);
const u8 *cn_rom_dialog_str(u32 index);

// Course name `slot` (0..25, the same order as seg2_course_name_table) and act
// name `index` (0..89).
//
// Both return a pointer to a private, mutable copy rather than into the
// segment buffer, because of the 3-byte vanilla prefix: every consumer of the
// course table either skips it with `+ 3` or expects the base to be a token
// boundary (cn_is_cn_string walks from base + 3). The iQue ROM's own strings do
// NOT carry that prefix -- only the previously-compiled arrays did -- so the
// loader re-creates it, byte for byte what text/cn_courses_cn.inc.c used to
// hold. Act names have no prefix and are copied verbatim.
//
// NULL when the rom is not loaded or the slot is out of range; callers must
// then leave the vanilla pointer in place.
const u8 *cn_rom_course_name(u32 slot);
const u8 *cn_rom_act_name(u32 index);

// True when `str` points anywhere inside one of the name buffers above.
//
// cn_is_cn_string() used to compare pointers for equality, which quietly broke
// every caller that walked a name forward -- skipping the " 1 " decoration in
// front of a course name lands six bytes in, not on the registered base or
// base+3, and the renderer then treated UTF-8 as vanilla bytes. A containment
// test is what those callers actually mean.
bool cn_rom_name_contains(const u8 *str);

// Raw pointer into the decompressed segment, for tables that are used as-is.
const u8 *cn_rom_seg(u32 offset);

// One of the twelve localised textures, or NULL if the rom is not loaded.
const u8 *cn_rom_texture(u32 index);

// Read `size` bytes from the rom file at `offset` into `dst`. Used for the
// localised textures, which sit at the same rom offsets as their US
// counterparts (that is the whole reason they can be overlaid in place).
bool cn_rom_read(u32 offset, void *dst, u32 size);

#endif // CN_ROM_ASSETS_H
