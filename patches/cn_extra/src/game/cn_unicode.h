#ifndef CN_UNICODE_H
#define CN_UNICODE_H

#include "types.h"

// The three control bytes, repeated here so this header stays dependency-free
// (it is also compiled by the host-side test, which has no engine headers).
// They must match DIALOG_CHAR_* in ingame_menu.h.
#define CN_CHAR_BYTE_NEWLINE   0xFE
#define CN_CHAR_BYTE_SPACE     0x9E
#define CN_CHAR_BYTE_TERMINATOR 0xFF

// ---------------------------------------------------------------------------
// One text encoding for everything.
// ---------------------------------------------------------------------------
//
// Until now the pack spoke iQue: a dense two-byte stream (0x01-0x05 + low byte)
// that is byte-for-byte indistinguishable from the vanilla single-byte font
// (vanilla '1' is 0x01, CN 的 is 0x01 0x01). That ambiguity is why every walker
// had to be *told* which encoding it was looking at, why the pack needed its own
// dialog table, and why a Lua replacement could not be Chinese.
//
// All text is now UTF-8, terminated by 0xFF, with 0xFE as the line break:
//
//   0xFF                 end of string
//   0xFE                 new line      (== DIALOG_CHAR_NEWLINE)
//   0x20..0x7E           printable ASCII, drawn with the vanilla font
//   0xC2..0xF4 + cont.   UTF-8 sequence -> glyph from the iQue atlas
//   U+E000..U+E0FF       icon escape, see below
//   0xFA / 0xFC / 0xFD   legacy single-byte LUT glyphs (star icons), passed
//                        through to the vanilla renderer untouched
//
// 0xFF and 0xFE are not legal UTF-8 bytes, so the three spaces cannot collide.
// A UTF-8 character with no glyph in the atlas is drawn as '?', which is the
// only failure mode the renderer has: no string is ever silently skipped.
//
// One loop reads all three; what the *renderer* has to ask is a different and
// narrower question -- does this string contain a 16x16 CJK glyph (which needs
// the 20px pitch) -- see cn_string_has_wide(). And cn_utf8_to_engine() asks a
// third one: can the whole string be spelled with the vanilla font, in which
// case it is not Chinese text at all and goes back through the original
// renderer, punctuation and all.
//
// --- the icon escape -------------------------------------------------------
//
// The iQue stream spells *every* vanilla-font character as `0x00 nn`, and that
// includes the 16px tiles that are not characters at all: 0x50-0x53 are the
// arrows, 0x54-0x58 the bold A / B / C / Z / R button glyphs, 0xF9 the coin,
// 0xFA/0xFD the filled and open stars. Those bytes are all >= 0x50, which is
// exactly where the plain 8px ASCII range of the dialog font ends.
//
// Rendering them as ASCII text loses the glyph: the button letters came out as
// thin 8px "A"/"B" instead of the bold button tiles -- and a mod string holding
// "T" is indistinguishable from a bold-A tile if the byte is stored bare.
//
// So glyphs from 0x50 up travel as U+E000 + the glyph code (Private Use Area:
// no real character is ever mapped there, and a mod that pastes one in gets the
// glyph it asks for). The renderer draws them with the same vanilla
// render_generic_char() call and the same gDialogCharWidths[] entry the original
// game uses, so a button is exactly as wide and exactly as bold as before.
#define CN_GLYPH_ESCAPE_BASE  0xE000
#define CN_GLYPH_ESCAPE_LIMIT 0xE100
#define CN_GLYPH_ESCAPE_FIRST 0x50  // first vanilla byte stored as an escape

typedef enum {
    CN_CHAR_END = 0,   // 0xFF: end of string
    CN_CHAR_NEWLINE,   // 0xFE: line break
    CN_CHAR_NARROW,    // printable ASCII in `ascii`; 8px wide
    CN_CHAR_WIDE,      // CJK glyph, iQue code in `code`; 16px wide
    CN_CHAR_LEGACY,    // vanilla single-byte LUT glyph (star/coin icons)
    CN_CHAR_MISSING,   // no glyph; draw '?' instead
} CnCharKind;

typedef struct {
    CnCharKind kind;
    u16 code;   // CN_CHAR_WIDE: iQue code. CN_CHAR_LEGACY: vanilla glyph byte.
    u8 ascii;   // CN_CHAR_NARROW: the character
    s32 length; // bytes consumed (>= 1, always advances)
} CnChar;

// Decode one character at str[index]. `length` is never 0, so a walker always
// makes progress even on a malformed string.
CnChar cn_read_char(const u8 *str, s32 index);

// iQue code <-> Unicode. Both are pure table lookups.
u16 cn_unicode_to_code(u32 cp);      // 0 when the atlas has no such character
u32 cn_code_to_unicode(u16 code);    // 0 when the code is not in the atlas

// The stand-in for a codepoint the atlas cannot draw (『 -> “, é -> e,
// ！ -> !), or 0 when there is none. cn_read_char() applies it automatically;
// exposed so a verifier can enumerate what a mod will actually look like.
u32 cn_unicode_alias(u32 cp);

// Encode one code point as UTF-8 into `out`; returns the byte count (1-4).
s32 cn_utf8_encode(u32 cp, u8 *out);

// Convert an iQue-encoded string (the format the 神游 cartridge stores and the
// one text/cn_labels.inc.c is generated in) into the UTF-8 form above.
//
// `cap` is the size of `dst`; the result is always terminated. Returns the
// number of bytes written excluding the terminator, or -1 when it did not fit
// (in which case the output is still terminated, never truncated mid-character
// in a way that loses the terminator).
s32 cn_ique_to_utf8(const u8 *src, u8 *dst, s32 cap);

// Size in bytes of a UTF-8 string with the same content as the iQue one, plus
// the terminator. Used to size the conversion buffer.
s32 cn_ique_to_utf8_size(const u8 *src);

// Convert one printable ASCII character to the engine's single-byte dialog
// font index (what render_generic_char() and gDialogCharWidths[] expect).
// Unknown characters come back as the space glyph.
u8 cn_ascii_to_dialog_char(u8 ascii);

// Turn a C string (UTF-8, as every Lua mod writes it) into the storage format
// the renderers accept: either the engine's single-byte encoding, when the
// text is pure ASCII, or literal UTF-8 with 0xFE line breaks and a 0xFF
// terminator when it contains anything else.
//
// Returns a malloc'd buffer the caller owns, or NULL on allocation failure.
// This is what lets smlua_replace_dialog() and HOOK_ON_DIALOG hand over
// Chinese text without losing it to an ASCII-only conversion.
u8 *cn_text_from_utf8(const char *src);

// Does this string need that treatment at all? True when it holds any byte
// outside ASCII, i.e. when the vanilla single-byte font cannot draw it.
bool cn_utf8_has_multibyte(const char *src);

// The same conversion, into a buffer the caller already owns. Used where the
// result has to live in a fixed static buffer instead of a heap allocation --
// get_level_name_sm64() / get_star_name_sm64() hand out a romhack's own level
// and star names that way.
//
// When the text contains nothing only the atlas can draw (no CJK glyph, no
// kana), the result is the *vanilla* single-byte encoding and `dst` is NOT
// registered: an English string keeps the English font, its own punctuation
// and the US dialog layout. See cn_utf8_to_ascii().
//
// `dst` must outlive the call; otherwise it is registered with
// cn_register_utf8_string() so the renderers can recognise it (they only ever
// see a pointer). Returns the number of bytes written excluding the terminator,
// or -1 when it does not fit (dst is terminated either way).
s32 cn_utf8_to_engine(u8 *dst, s32 cap, const char *src);

// The vanilla-font spelling of a UTF-8 string, into `out` (NUL-terminated).
//
// Every character that only the atlas can draw makes this fail (-1): that is
// the test cn_utf8_to_engine() uses to decide whether a string is Chinese text
// at all. Everything else is replaced by the spelling sSm64CharMap knows --
// 『 -> <<, — -> -, ● -> *, é -> e -- so the result can go straight through
// convert_string_ascii_to_sm64().
//
// Exposed for the host-side test; nothing in the renderer calls it directly.
s32 cn_utf8_to_ascii(char *out, s32 cap, const char *src);

// Does this string contain a full-width CJK cell?
//
// Decides the dialog line pitch: a 16x16 CJK glyph needs 20px of line spacing
// so it does not touch the line below, while a string of 8px ASCII (however
// many vanilla icon tiles it also holds) is authored for the 16px pitch the
// game has always used. Answering with cn_is_cn_string() instead would give
// every romhack dialog 20px gaps, because a mod's string is UTF-8 too.
bool cn_string_has_wide(const u8 *str);

// Width in pixels of one decoded character, given the vanilla width table.
// (Defined here so every caller agrees; the table itself lives in
// ingame_menu.c.)
f32 cn_char_width_of(const CnChar *ch, const u8 *vanillaWidths);

#endif // CN_UNICODE_H
