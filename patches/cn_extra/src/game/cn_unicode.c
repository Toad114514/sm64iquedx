#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "cn_unicode.h"
#include "cn_data/cn_unicode_map.h"
#include "cn_data/cn_unicode_alias.h"

// Declared locally rather than pulled in from the engine headers: this file is
// also compiled by the host-side tests (.workbuddy/test_cn_unicode.sh,
// .workbuddy/test_cn_rom_assets.sh), which have no engine headers at all.
u8 *convert_string_ascii_to_sm64(u8 *str64, const char *strAscii, bool menu);
char *sm64_char_to_ascii(char *out, u8 c);
u8 ascii_char_to_sm64(char c, bool menu);
void cn_register_utf8_string(const u8 *str);

#define CN_CHAR_NEWLINE_E   CN_CHAR_BYTE_NEWLINE
#define CN_CHAR_SPACE_E     CN_CHAR_BYTE_SPACE
#define CN_CHAR_TERM_E      CN_CHAR_BYTE_TERMINATOR
#define CN_GLYPH_W_E        16
#define CN_GLYPH_FIRST_E    0x0100

static s32 cn_vanilla_glyph_for(u32 cp);
static bool cn_apply_alias(u32 cp, CnChar *ch);

// ---------------------------------------------------------------------------
// UTF-8 walker
// ---------------------------------------------------------------------------
//
// One character per call, and the returned `length` is always >= 1, so even a
// string full of garbage terminates. Anything that is not printable ASCII, a
// valid UTF-8 sequence or one of the two control bytes becomes CN_CHAR_MISSING
// and the renderer draws '?': a wrong glyph is a visible bug, a silently
// skipped character is an invisible one.
//
// The legacy single-byte glyph codes (0xFA filled star, 0xFC interpunct,
// 0xFD empty star, and the 0x54-0x58 button letters) are passed through as
// CN_CHAR_LEGACY: the vanilla font already has them, and they can legitimately
// appear inside an otherwise-UTF-8 string (a mod writing "★ 3 stars").

CnChar cn_read_char(const u8 *str, s32 index) {
    CnChar ch;
    u8 b;

    ch.kind = CN_CHAR_END;
    ch.code = 0;
    ch.ascii = 0;
    ch.length = 1;

    if (str == NULL) { return ch; }

    b = str[index];

    if (b == CN_CHAR_TERM_E) {
        ch.kind = CN_CHAR_END;
        return ch;
    }
    if (b == CN_CHAR_NEWLINE_E) {
        ch.kind = CN_CHAR_NEWLINE;
        return ch;
    }
    if (b == CN_CHAR_SPACE_E) {
        // Legacy space token. Plain ' ' also works, this is for the iQue-era
        // strings that still carry 0x9E.
        ch.kind = CN_CHAR_NARROW;
        ch.ascii = ' ';
        return ch;
    }

    if (b < 0x80) {
        // Controls other than the two above cannot occur in text we produce;
        // showing '?' beats emitting a random glyph.
        if (b < 0x20) {
            ch.kind = CN_CHAR_MISSING;
            return ch;
        }
        ch.kind = CN_CHAR_NARROW;
        ch.ascii = b;
        return ch;
    }

    // Legacy LUT glyphs. These are >= 0x80 but are not UTF-8 lead bytes, so
    // without this branch they would fall through to CN_CHAR_MISSING and a
    // "★ 3 stars" string would lose its star. Everything from 0x20 to 0x7E is
    // printable ASCII and is decoded above -- the button glyphs are 0x54..0x58
    // in the *iQue* stream (as `00 54`), never bare, so they do not belong here.
    if (b == 0xFA || b == 0xFC || b == 0xFD) {
        ch.kind = CN_CHAR_LEGACY;
        ch.code = b;
        return ch;
    }

    {
        u32 cp = 0;
        s32 need = 0;
        s32 i;

        if ((b & 0xE0) == 0xC0) {
            need = 1; cp = b & 0x1F;
        } else if ((b & 0xF0) == 0xE0) {
            need = 2; cp = b & 0x0F;
        } else if ((b & 0xF8) == 0xF0) {
            need = 3; cp = b & 0x07;
        } else {
            // Continuation byte or invalid lead: one byte, '?'.
            ch.kind = CN_CHAR_MISSING;
            return ch;
        }

        for (i = 1; i <= need; i++) {
            u8 c = str[index + i];
            if ((c & 0xC0) != 0x80) {
                ch.kind = CN_CHAR_MISSING;
                return ch; // consume the bad lead byte only
            }
            cp = (cp << 6) | (c & 0x3F);
        }

        ch.length = need + 1;

        if (cp < 0x20) {
            ch.kind = CN_CHAR_MISSING;
            return ch;
        }
        if (cp < 0x80) {
            // Overlong encoding of an ASCII character.
            ch.kind = CN_CHAR_NARROW;
            ch.ascii = (u8) cp;
            return ch;
        }

        ch.code = cn_unicode_to_code(cp);
        if (ch.code == 0) {
            s32 glyph = cn_vanilla_glyph_for(cp);

            // A character the *vanilla* font already has (the filled/empty star
            // and the interpunct). Passing it through as a legacy glyph keeps
            // a mod's "★★★★★ ..." title readable instead of a row of '?'.
            if (glyph >= 0) {
                ch.kind = CN_CHAR_LEGACY;
                ch.code = (u16) glyph;
            }
            // The icon escape: 0x50 and up are the vanilla font's 16px tiles
            // (arrows, the bold button letters, the coin and star icons). They
            // are one glyph, not text, so they carry the raw glyph byte.
            else if (cp >= CN_GLYPH_ESCAPE_BASE && cp < CN_GLYPH_ESCAPE_LIMIT) {
                ch.kind = CN_CHAR_LEGACY;
                ch.code = (u16) (cp - CN_GLYPH_ESCAPE_BASE);
            }
            // Last chance before '?': a character the atlas has no glyph for,
            // but whose *meaning* the atlas can draw -- 『 as “, é as e, ！ as
            // !. See cn_data/cn_unicode_alias.h.
            else if (cn_apply_alias(cp, &ch)) {
                // filled in by the helper
            } else {
                ch.kind = CN_CHAR_MISSING;
            }
        } else {
            ch.kind = CN_CHAR_WIDE;
        }
        return ch;
    }
}

// --- Vanilla glyphs that are not ASCII -------------------------------------

// Non-ASCII characters the vanilla single-byte font can already draw.
//
// sSm64CharMap (level_info.c) is the authority: convert_string_ascii_to_sm64()
// consults it, which is why a romhack's "★★★★★★★★" renders correctly on the
// US build. The UTF-8 path has to reproduce that mapping or the very same
// string reaches the screen as a row of '?' -- the atlas lookup below only
// knows iQue codes, and U+2605 is not one of them.
//
// Keep in step with the multi-byte entries of sSm64CharMap;
// .workbuddy/test_cn_unicode.sh cross-checks the two tables so a new one cannot
// be added there and forgotten here.
static s32 cn_vanilla_glyph_for(u32 cp) {
    switch (cp) {
        case 0x2605: return 0xFA; // ★ filled star
        case 0x2606: return 0xFD; // ☆ open star
        case 0x2022: return 0xFC; // • interpunct
    }
    return -1;
}

// --- Map: one character per call -------------------------------------------

u16 cn_unicode_to_code(u32 cp) {
    s32 lo = 0;
    s32 hi = CN_UNICODE_TO_CODE_COUNT - 1;

    while (lo <= hi) {
        s32 mid = (lo + hi) / 2;
        u32 midCp = sCnUnicodeToCode[mid].cp;

        if (midCp == cp) { return sCnUnicodeToCode[mid].code; }
        if (midCp < cp) { lo = mid + 1; } else { hi = mid - 1; }
    }

    return 0;
}

u32 cn_code_to_unicode(u16 code) {
    u32 index;

    if (code < CN_GLYPH_FIRST_E) { return 0; }
    index = (u32) (code - CN_GLYPH_FIRST_E);
    if (index >= sizeof(sCnCodeToUnicode) / sizeof(sCnCodeToUnicode[0])) { return 0; }
    return sCnCodeToUnicode[index];
}

// --- Alias: the same glyph, spelled differently ----------------------------
//
// Last stage of the lookup, after the atlas and the vanilla tiles both miss.
// A mod writes 『SM64 ROM Hacks』 (U+300E/U+300F), "Bergentrückung", "！",
// "3 × 3", "1 ↔ 1" -- none of those codepoints exist in the 1072-glyph atlas,
// but every one of them has a stand-in that does, and a stand-in is always
// better than '?'. The table is generated (cn_data/cn_unicode_alias.h) so it
// covers whole blocks (all of Latin-1 Supplement and Latin Extended-A/B, all
// of full-width ASCII) instead of the handful of characters a particular mod
// happened to use.
//
// What is deliberately *not* here: kana and the kanji outside the 1072. There
// is no glyph that means "の", so those still draw '?' -- the honest answer,
// and the only one that does not lie about the text.
//
// The helper only fills in kind / code / ascii; `length` stays the number of
// bytes the *source* character occupies in the UTF-8 string, because that is
// what the caller has to skip.
u32 cn_unicode_alias(u32 cp) {
    s32 lo = 0;
    s32 hi = CN_UNICODE_ALIAS_COUNT - 1;

    while (lo <= hi) {
        s32 mid = (lo + hi) / 2;
        u32 midCp = sCnUnicodeAlias[mid].cp;

        if (midCp == cp) { return sCnUnicodeAlias[mid].to; }
        if (midCp < cp) { lo = mid + 1; } else { hi = mid - 1; }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// The vanilla-font spelling of one codepoint
// ---------------------------------------------------------------------------
//
// sCnAsciiForm (cn_data/cn_unicode_alias.h) is generated, not hand-written,
// because every entry has to be reproducible by sSm64CharMap alone: the
// vanilla renderer knows nothing else.
//
// Why a second table at all, when sCnUnicodeAlias already maps 『 to a glyph?
// Because "the glyph the atlas has" and "the glyph the vanilla font has" are
// different answers, and using the first one for English text is what put a
// 16px Chinese quote, and a 16px em dash, in the middle of 8px ASCII -- it
// reads as a different font because it is one. The atlas answer is for Chinese
// text; this one is for everything else.
static const char *cn_ascii_form(u32 cp) {
    s32 lo = 0;
    s32 hi = CN_ASCII_FORM_COUNT - 1;

    while (lo <= hi) {
        s32 mid = (lo + hi) / 2;
        u32 midCp = sCnAsciiForm[mid].cp;

        if (midCp == cp) { return sCnAsciiForm[mid].ascii; }
        if (midCp < cp) { lo = mid + 1; } else { hi = mid - 1; }
    }

    return NULL;
}

// Resolve one alias target into a drawable character. False when the target
// is not drawable either -- the generator refuses to emit such an entry, so
// this is belt and braces, never a normal path.
static bool cn_apply_alias(u32 cp, CnChar *ch) {
    u32 alt = cn_unicode_alias(cp);
    u16 code;

    if (alt == 0) {
        return false;
    }

    // ASCII draws from the vanilla single-byte font.
    if (alt < 0x80) {
        ch->kind = CN_CHAR_NARROW;
        ch->ascii = (u8) alt;
        ch->code = 0;
        return true;
    }

    code = cn_unicode_to_code(alt);
    if (code != 0) {
        ch->kind = CN_CHAR_WIDE;
        ch->code = code;
        return true;
    }

    {
        s32 glyph = cn_vanilla_glyph_for(alt);
        if (glyph >= 0) {
            ch->kind = CN_CHAR_LEGACY;
            ch->code = (u16) glyph;
            return true;
        }
    }

    return false;
}

s32 cn_utf8_encode(u32 cp, u8 *out) {
    if (cp < 0x80) {
        out[0] = (u8) cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (u8) (0xC0 | (cp >> 6));
        out[1] = (u8) (0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (u8) (0xE0 | (cp >> 12));
        out[1] = (u8) (0x80 | ((cp >> 6) & 0x3F));
        out[2] = (u8) (0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (u8) (0xF0 | (cp >> 18));
    out[1] = (u8) (0x80 | ((cp >> 12) & 0x3F));
    out[2] = (u8) (0x80 | ((cp >> 6) & 0x3F));
    out[3] = (u8) (0x80 | (cp & 0x3F));
    return 4;
}

// --- iQue (cartridge) text -> UTF-8 ----------------------------------------

// The 神游 ROM stores text in its own encoding: 0x00 xx is a vanilla glyph,
// 0x01-0x05 xx a CJK glyph, 0xFF 0xFE a newline and 0xFF 0xFF the end. Every
// string that comes out of the cartridge goes through here exactly once, when
// it is installed, so the renderer only ever sees the format in cn_unicode.h.
s32 cn_ique_to_utf8(const u8 *src, u8 *dst, s32 cap) {
    s32 out = 0;

    if (dst == NULL || cap <= 0) { return -1; }
    if (src == NULL) { dst[0] = CN_CHAR_TERM_E; return 0; }

    for (;;) {
        u8 lead = src[0];
        u8 buf[4];
        s32 n = 0;

        // 0xFF 0xFF ends the string; every other 0xFF is a two-byte control.
        if (lead == CN_CHAR_TERM_E) {
            if (src[1] == CN_CHAR_TERM_E) { break; }
        }

        if (lead == CN_CHAR_NEWLINE_E) {
            buf[0] = CN_CHAR_NEWLINE_E;
            n = 1;
            src += 1;
        } else if (lead == 0x00) {
            u8 glyph = src[1];

            // 0x50 and up is not text: those are the 16px tiles of the vanilla
            // font -- the arrows, the bold A/B/C/Z/R button letters, the coin
            // and the two star icons. Turning them into ASCII gave a thin 8px
            // "A" where the game draws a bold button, so they are carried as
            // the U+E0xx escape instead (see cn_unicode.h).
            //
            // sm64_char_to_ascii(), not convert_string_sm64_to_ascii(): the
            // latter asks cn_is_cn_string() about its input, which calls
            // cn_label_get(), which calls back into this function. That
            // recursion is what segfaulted on the first UI label drawn.
            if (glyph >= CN_GLYPH_ESCAPE_FIRST && glyph != CN_CHAR_SPACE_E) {
                n = cn_utf8_encode(CN_GLYPH_ESCAPE_BASE + glyph, buf);
            } else if (glyph == CN_CHAR_SPACE_E) {
                // The vanilla space glyph: an empty 8px cell.
                buf[0] = ' ';
                n = 1;
            } else {
                char ascii[8]; // SM64_TO_ASCII_MAX_CHAR_SIZE is 4; this file has no engine headers

                sm64_char_to_ascii(ascii, glyph);
                n = (s32) strlen(ascii);
                if (n > 4) { n = 4; }
                if (n == 0) {
                    buf[0] = '?';
                    n = 1;
                } else {
                    memcpy(buf, ascii, (size_t) n);
                }
            }
            src += 2;
        } else if (lead >= 0x01 && lead <= 0x05) {
            u32 cp = cn_code_to_unicode((u16) ((lead << 8) | src[1]));
            if (cp == 0) {
                buf[0] = '?';
                n = 1;
            } else {
                n = cn_utf8_encode(cp, buf);
            }
            src += 2;
        } else if (lead == 0xFF) {
            u8 second = src[1];
            if (second == 0xFE) {
                buf[0] = CN_CHAR_NEWLINE_E;
                n = 1;
            } else if (second == 0x9E) {
                buf[0] = ' ';
                n = 1;
            } else {
                // Unknown escape: skip it rather than leaking a stray byte
                // into a UTF-8 string.
                n = 0;
            }
            src += 2;
        } else {
            // Legacy single-byte glyph; keep it as-is (the renderer passes it
            // straight to the vanilla font).
            buf[0] = lead;
            n = 1;
            src += 1;
        }

        if (n > 0) {
            if (out + n + 1 > cap) {
                dst[out] = CN_CHAR_TERM_E;
                return -1;
            }
            memcpy(dst + out, buf, (size_t) n);
            out += n;
        }
    }

    dst[out] = CN_CHAR_TERM_E;
    return out;
}

s32 cn_ique_to_utf8_size(const u8 *src) {
    s32 size = 0;

    if (src == NULL) { return 1; }

    for (;;) {
        u8 lead = *src;

        if (lead == CN_CHAR_TERM_E && src[1] == CN_CHAR_TERM_E) {
            break;
        }

        if (lead == CN_CHAR_NEWLINE_E) {
            size += 1;
            src += 1;
        } else if (lead == 0xFF) {
            // A two-byte control (FF FE / FF 9E / an unused escape).
            size += 1;
            src += 2;
        } else if (lead == 0x00) {
            // ASCII glyphs are one byte, an icon escape is three. No single
            // 0x00-lead token produces more.
            size += 3;
            src += 2;
        } else if (lead >= 0x01 && lead <= 0x05) {
            size += 3; // CJK is 3 bytes in UTF-8
            src += 2;
        } else {
            size += 1;
            src += 1;
        }
    }

    return size + 1;
}

// --- ASCII -> vanilla dialog font ------------------------------------------

u8 cn_ascii_to_dialog_char(u8 ascii) {
    return ascii_char_to_sm64((char) ascii, false);
}

// --- Mod text (UTF-8) -> storage format ------------------------------------

// Does this C string contain anything outside ASCII? That is exactly the
// question "can the vanilla single-byte font draw it", and the answer decides
// whether a name has to go through cn_utf8_to_engine() instead of
// convert_string_ascii_to_sm64().
bool cn_utf8_has_multibyte(const char *src) {
    s32 i;

    if (src == NULL) { return false; }

    for (i = 0; src[i] != '\0'; i++) {
        if ((u8) src[i] >= 0x80) { return true; }
    }
    return false;
}

// Copy a UTF-8 C string into the storage format, in a caller-provided buffer:
// 0xFE for a line break, 0xFF to end, literal UTF-8 bytes in between.
//
// This is the only conversion for text the vanilla charmap cannot express (a
// romhack's own level / star name, a mod's dialog). The engine's
// convert_string_ascii_to_sm64() maps every byte through the single-byte font
// table, which turns each UTF-8 byte into a random glyph -- that is why a
// Chinese course name used to reach the screen as garbage or as nothing at all.
//
// `dst` must outlive the call (a static buffer, or the malloc in
// cn_text_from_utf8) and is registered here: the renderers only ever see a
// pointer, and cn_is_cn_string() recognises engine-format text by identity.
// Returns the number of bytes written excluding the terminator, or -1 when it
// does not fit (dst is still terminated).
s32 cn_utf8_to_engine(u8 *dst, s32 cap, const char *src) {
    s32 n = 0;
    s32 i;

    if (dst == NULL || cap < 1 || src == NULL) { return -1; }

    // Fast path: the string holds nothing the atlas alone can draw, so it is
    // not Chinese text at all -- an English romhack dialog, an author credit,
    // a level name. Handing it back to the vanilla renderer is not a
    // short-circuit for speed, it is the only way its punctuation looks
    // right: the atlas' “ ” — … are 16px wide, and among 8px letters they
    // read as a second font. See cn_utf8_to_ascii().
    //
    // The result is deliberately NOT registered with cn_register_utf8_string():
    // it is in the engine's own encoding, and cn_is_cn_string() must keep
    // saying "no" so the renderers use the vanilla walker and the vanilla
    // layout (16px pitch, the dialog's own linesPerBox).
    {
        s32 const srcLen = (s32) strlen(src);
        char *ascii = (char *) malloc((size_t) srcLen + 1);

        if (ascii != NULL) {
            s32 const asciiLen = cn_utf8_to_ascii(ascii, srcLen + 1, src);

            if (asciiLen >= 0 && asciiLen + 2 <= cap) {
                convert_string_ascii_to_sm64(dst, ascii, false);
                free(ascii);
                while (dst[n] != CN_CHAR_TERM_E) { n++; }
                return n;
            }
            free(ascii);
        }
    }

    for (i = 0; src[i] != '\0'; i++) {
        u8 const c = (u8) src[i];

        if (c == '\r') { continue; }

        if (n + 1 >= cap) {
            dst[0] = CN_CHAR_TERM_E;
            return -1;
        }
        dst[n++] = (c == '\n') ? CN_CHAR_NEWLINE_E : c;
    }

    dst[n] = CN_CHAR_TERM_E;
    cn_register_utf8_string(dst);
    return n;
}

// Decode one UTF-8 sequence. Returns the codepoint, or 0xFFFFFFFF when the
// bytes are not a well-formed sequence (in which case `*len` is 1 and the
// caller treats it as undrawable).
static u32 cn_decode_utf8(const char *src, s32 *len) {
    u8 const b = (u8) src[0];
    u32 cp;
    s32 need;
    s32 i;

    if (b < 0x80) { *len = 1; return b; }
    if ((b & 0xE0) == 0xC0) { need = 1; cp = b & 0x1F; }
    else if ((b & 0xF0) == 0xE0) { need = 2; cp = b & 0x0F; }
    else if ((b & 0xF8) == 0xF0) { need = 3; cp = b & 0x07; }
    else { *len = 1; return 0xFFFFFFFF; }

    for (i = 1; i <= need; i++) {
        u8 const c = (u8) src[i];

        if ((c & 0xC0) != 0x80) { *len = 1; return 0xFFFFFFFF; }
        cp = (cp << 6) | (c & 0x3F);
    }

    *len = need + 1;
    return cp;
}

// Rebuild `src` as text the *vanilla* font can draw, into `out`.
//
// Returns the length in bytes, or -1 when at least one character only exists
// in the atlas (a CJK glyph, kana). Every non-ASCII character that does have a
// vanilla spelling is replaced by it: 『 -> <<, — -> -, ● -> *. A character the
// vanilla font can draw as-is keeps its own bytes, because sSm64CharMap has
// multi-byte entries for ★ / ☆ / • and convert_string_ascii_to_sm64() below
// resolves those.
//
// Exposed (cn_utf8_to_ascii) so the host-side test can assert the mapping
// without linking the renderer.
s32 cn_utf8_to_ascii(char *out, s32 cap, const char *src) {
    s32 n = 0;
    s32 i = 0;

    if (out == NULL || cap < 1 || src == NULL) { return -1; }

    while (src[i] != '\0') {
        u32 cp;
        s32 len;

        if (src[i] == '\r') { i++; continue; }

        cp = cn_decode_utf8(src + i, &len);

        if (cp < 0x80) {
            if (n + 1 >= cap) { out[n] = '\0'; return -1; }
            out[n++] = src[i++];
            continue;
        }

        if (cp != 0xFFFFFFFF && cn_vanilla_glyph_for(cp) >= 0) {
            // Keep the UTF-8 spelling: the charmap has it as one entry.
            if (n + len >= cap) { out[n] = '\0'; return -1; }
            memcpy(out + n, src + i, (size_t) len);
            n += len;
            i += len;
            continue;
        }

        {
            const char *form = (cp == 0xFFFFFFFF) ? NULL : cn_ascii_form(cp);
            s32 fl;

            if (form == NULL) { out[n] = '\0'; return -1; }

            fl = (s32) strlen(form);
            if (n + fl >= cap) { out[n] = '\0'; return -1; }
            memcpy(out + n, form, (size_t) fl);
            n += fl;
            i += len;
        }
    }

    out[n] = '\0';
    return n;
}

// Everything a mod hands us is UTF-8. Pure ASCII text still goes through the
// engine's own conversion so it behaves exactly as before (that path also maps
// "\n" to 0xFE and knows about the multi-character glyphs); anything with a
// high byte is stored as literal UTF-8, which cn_read_char() understands.
u8 *cn_text_from_utf8(const char *src) {
    u8 *out;

    if (src == NULL) { return NULL; }

    if (!cn_utf8_has_multibyte(src)) {
        return convert_string_ascii_to_sm64(NULL, src, false);
    }

    out = (u8 *) malloc((size_t) strlen(src) + 2);
    if (out == NULL) { return NULL; }

    // Registers `out` for cn_is_cn_string(); nothing else to do here.
    if (cn_utf8_to_engine(out, (s32) strlen(src) + 2, src) < 0) {
        free(out);
        return NULL;
    }

    return out;
}

bool cn_string_has_wide(const u8 *str) {
    s32 i = 0;

    if (str == NULL) { return false; }

    for (;;) {
        CnChar ch = cn_read_char(str, i);

        if (ch.kind == CN_CHAR_END) { return false; }
        if (ch.kind == CN_CHAR_WIDE) {
            // Only an ideograph (or a kana) fills its 16x16 cell from top to
            // bottom and therefore needs the 20px line pitch. The full-width
            // punctuation the atlas does have -- “ ”, —, …, 、, 。 -- is 16px
            // *wide* but only a few pixels tall, and so is anything the alias
            // table substituted in (『 -> “). Counting those would give an
            // English romhack dialog Chinese line spacing, which is exactly
            // the bug the two-layout split was introduced to fix.
            if (cn_code_to_unicode(ch.code) >= 0x2E80) { return true; }
        }
        i += ch.length;
    }
}

f32 cn_char_width_of(const CnChar *ch, const u8 *vanillaWidths) {
    switch (ch->kind) {
        case CN_CHAR_WIDE:
            return (f32) CN_GLYPH_W_E;
        case CN_CHAR_NARROW:
            return (f32) vanillaWidths[cn_ascii_to_dialog_char(ch->ascii)];
        case CN_CHAR_MISSING:
            return (f32) vanillaWidths[cn_ascii_to_dialog_char('?')];
        case CN_CHAR_LEGACY:
            return (f32) vanillaWidths[ch->code];
        default:
            return 0.0f;
    }
}
