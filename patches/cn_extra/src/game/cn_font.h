#ifndef CN_FONT_H
#define CN_FONT_H

#include <ultra64.h>
#include "ingame_menu.h" // DIALOG_CHAR_TERMINATOR / DIALOG_CHAR_SPACE

// In-game Chinese (iQue/CN) glyph rendering.
//
// CN text uses two-byte codes (0x01-0x05 prefix + low byte). These helpers
// decode the shipped RGBA32 CJK atlas into IA8 glyphs on demand and draw them
// through the normal in-game IA text pipeline.

// Cell size of one CN glyph, in pixels. Both the IA8 quad path and the
// rectangle path use this, and callers that need to convert between the
// menu-space Y axis (grows up) and the HUD/rectangle Y axis (grows down) need
// it to know how tall a glyph is -- so it lives here, not in cn_font.c.
#define CN_GLYPH_W 16
#define CN_GLYPH_H 16

u8 *cn_font_get_glyph(u16 code);
void render_cn_char(u16 code);
void cn_draw_glyph_rect(s32 x, s32 y, u16 code);

// Same, scaled into a `cell`-pixel square. For the fixed-pitch lists (the
// file-select score table is 12px per row) that cannot fit a 16px cell.
void cn_draw_glyph_rect_scaled(s32 x, s32 y, u16 code, s32 cell);
f32 cn_char_width(u16 code);

// ---------------------------------------------------------------------------
// Coloured CJK title font (iQue "HUD" font)
// ---------------------------------------------------------------------------
//
// A completely separate typeface from the IA8 dialog font above. The iQue ROM
// ships 124 16x16 RGBA16 cells (vertical gradients, drop shadows) addressed by
// code 0x3A-0xB5, and uses them for its fixed screen titles: 暂停, 恭喜发财,
// 最高分, 马里奥, 新建, 选择文件, 检查文件, 复制文件, 删除文件, 声音选择.
// Data: src/game/cn_data/cn_hud_glyphs_rgba16.h (gen_cn_hud_glyphs.py).
//
// The codes are SINGLE bytes, unrelated to the two-byte CN encoding used by
// dialogs -- do not feed these to cn_read_token().

// Stable pointer to a dense 16x16 RGBA16 (256 texels) copy of one coloured
// glyph, or NULL if `code` is outside 0x3A-0xB5. The buffer is owned by the
// font and stays valid for the process lifetime.
//
// The return type is u8*, NOT u16*, and that is deliberate: gfx_pc reads a
// texel as (addr[2*i] << 8) | addr[2*i + 1], so the cell must be stored
// big-endian. Reading the same bytes as a host-order u16 byte-swaps every
// colour channel and half the alphas.
//
// A code inside the range can legitimately return an all-zero cell: iQue pads
// its titles to a fixed 6-glyph line, and 4 of TEXT_PAUSE's 12 codes are fully
// transparent. That is a blank, not a failure.
u8 *cn_font_get_hud_glyph(u8 code);

// Draw one coloured glyph with a gSPTextureRectangle at (x, y), which is the
// TOP-left corner in HUD space (y grows downward -- see cn_draw_glyph_rect for
// why the two paths disagree).
void cn_draw_hud_glyph_rect(s32 x, s32 y, u8 code);

// Draw an iQue-style coloured title.
//
// `str` is a plain single-byte string: one glyph code per byte, 0xFE moves to
// the next line, 0xFF ends the string. Codes below 0x3A are the ASCII HUD font
// and are NOT handled here -- skip them in the input rather than relying on a
// fallback, so a title can never silently render as garbage.
//
// `x` is the left edge of the text and `y` the top of the first line; a glyph
// advances 16px right, and a newline is another 16px down. Returns the width
// actually drawn, so callers can centre the title.
s32 cn_draw_hud_title(s32 x, s32 y, const u8 *str);

// Draw an iQue *pseudo-glyph*: a character the ROM never stores as one cell but
// the official renderer assembles from several coloured cells. `x`/`y` are the
// top-left corner in HUD space (y grows downward). Returns the width drawn in
// pixels, or 0 when `code` is not a pseudo-glyph.
//
//   0xB0  "按" -- 2x2 block, cells 0x92/0x93 on top and 0x94/0x95 below, 32x32.
//                 (The 0xB0 slot itself holds only a fragment; the whole glyph
//                 exists only as these four quarters.)
//
// This must be called during the render pass -- it emits a display list -- and
// is meant for the text-label path (see render_text_labels()), where a queued
// 0xB0 byte is expanded into the block.
s32 cn_draw_hud_pseudo(s32 x, s32 y, u8 code);

// True when a CN glyph code belongs to the 0x00xx range that is identical to
// the vanilla single-byte font (digits, A-Z, arrows, [A]/[B]/[C]/[Z]/[R],
// quotes), or is one of the generator's bit-15 vanilla equivalents. Callers
// must draw those with render_generic_char(cn_glyph_vanilla_byte(code))
// instead of render_cn_char(), because the CJK atlas has no coverage for them.
bool cn_glyph_is_vanilla(u16 code);
u8 cn_glyph_vanilla_byte(u16 code);

// --- CN token reading -------------------------------------------------------
//
// iQue/CN text is a stream of *two-byte* tokens. The lead byte is 0x00 for an
// ASCII glyph and 0x01-0x05 for a CJK glyph, and 0xFF for every control token:
//
//   0x00 0x00..0xFD  ASCII glyph ('0'-'9', letters, punctuation)
//   0x01..0x05 xx    CJK glyph (every low byte 0x00-0xFF is used)
//   0xFF 0xFE  newline
//   0xFF 0x9E  (full-width) space
//   0xFF 0xFF  end of string
//   0xFF 0x00..0xFD  other control / escaped glyph
//
// The encoding is DENSE: 0x01-0x04 pair with every low byte 0x00-0xFF. That
// makes it *structurally impossible* to tell a CN string from a vanilla
// single-byte string by inspecting bytes alone -- vanilla writes a bare digit
// as {0x01} ("1") and {0x9E, 0x01, 0x9E} (" 1 ") which is byte-identical to
// the start of a CN CJK glyph. The distinction therefore has to be carried per
// string, not guessed per byte:
//
//   * strings the engine knows are CN (course/act name tables, imported dialog)
//     are walked with cn_read_token(..., true)
//   * everything else (vanilla HUD strings, star-icon LUT strings that mix
//     0xFA/0xFD into otherwise-CN data) is walked with `false`, which always
//     returns CN_TOKEN_NONE so the caller keeps its vanilla byte path.
//
// When `cn` is true the walker also honours the vanilla LUT glyphs 0xFA / 0xFD
// as pass-through single bytes, so a CN string that embeds star icons still
// renders those icons.

typedef enum {
    CN_TOKEN_NONE = 0, // not a CN token; caller should use the vanilla path
    CN_TOKEN_GLYPH,    // renderable CJK glyph, code in `code`
    CN_TOKEN_NEWLINE,  // honour as DIALOG_CHAR_NEWLINE
    CN_TOKEN_SPACE,    // honour as DIALOG_CHAR_SPACE
    CN_TOKEN_END       // 0xFF 0xFF, end of string
} CnTokenKind;

typedef struct {
    CnTokenKind kind;
    u16 code;  // valid when kind == CN_TOKEN_GLYPH
    s32 length;// bytes consumed: 2 for any CN token
} CnToken;

// Decode the token at str[index].
//
// `cn` must be true only when the caller knows `str` points at a CN-encoded
// string (see the explanation above). When it is false the function always
// returns kind == CN_TOKEN_NONE, and the caller keeps its vanilla single-byte
// handling -- which is the correct behaviour for every vanilla string in the
// game, including the 0xFA/0x9E star-icon strings passed to print_generic_string.
CnToken cn_read_token(const u8 *str, s32 index, bool cn);

// Returns true when `str` is one of the CN name-table strings installed by
// apply_cn_text_pack(). Walkers use this to decide the `cn` argument above,
// because the two encodings cannot be told apart from the bytes.
bool cn_is_cn_string(const u8 *str);

// Register a UTF-8 buffer that came from a mod (cn_text_from_utf8) so
// cn_is_cn_string() can recognise it later. The renderer needs this because it
// only has the pointer: a mod's Chinese string is not in any table the engine
// knows about.
//
// Bounded and replace-oldest: a long-running session that replaces thousands of
// dialogs must not grow without limit. Losing an entry only costs that string
// its UTF-8 fast path (it would be walked as vanilla bytes), never a crash.
void cn_register_utf8_string(const u8 *str);

// ---------------------------------------------------------------------------
// Fixed UI labels (pause menu, Lakitu hint box, HUD).
//
// Unlike courses.h / dialogs.h these are plain ROM strings in vanilla SM64 that
// INGAME_TEXT_COPY() copies onto the stack, and the iQue release never
// translated them -- so there is no official table to transcribe. Ours live in
// text/cn_labels.inc.c, generated by .workbuddy/gen_cn_labels.py.
//
// The lookup is by enum rather than by name because the call sites are macros:
// INGAME_TEXT_COPY(textStar, TEXT_STAR) has no room for a runtime branch, so it
// expands to cn_label_get(CN_LABEL_STAR) instead and the pointer to the shared
// read-only CN array replaces the stack copy.
// ---------------------------------------------------------------------------
typedef enum {
    CN_LABEL_PAUSE,
    CN_LABEL_COURSE,
    CN_LABEL_STAR,
    CN_LABEL_STAR_X,
    CN_LABEL_COIN_X,
    CN_LABEL_MY_SCORE,
    CN_LABEL_EXIT_COURSE,
    CN_LABEL_EXIT_TO_CASTLE,
    CN_LABEL_CLEAR,
    CN_LABEL_CAMERA_ANGLE_R,
    CN_LABEL_SAVE_AND_CONTINUE,
    CN_LABEL_CONTINUE,
    CN_LABEL_CONTINUE_WITHOUT_SAVING,
    CN_LABEL_LAKITU_MARIO,
    CN_LABEL_LAKITU_STOP,
    CN_LABEL_NORMAL_UPCLOSE,
    CN_LABEL_NORMAL_FIXED,
    CN_LABEL_HUD_HI_SCORE,
    CN_LABEL_HUD_CONGRATULATIONS,
    CN_LABEL_UNFILLED_STAR,
    CN_LABEL_CATCH,
    CN_LABEL_MARIO,
    CN_LABEL_COUNT
} CNLabelId;

// Returns the CN string for `id` when the pack is active, otherwise NULL.
// Never returns NULL for an in-range id while gCnTextPackActive is set.
const u8 *cn_label_get(CNLabelId id);

// Label id for a vanilla-only INGAME_TEXT_COPY() call site. Deliberately out of
// range so cn_label_get() returns NULL and the site keeps its ROM string; this
// is how the file-select and star-select labels stay English until they get a
// translation.
#define CN_LABEL_NONE CN_LABEL_COUNT

#endif // CN_FONT_H
