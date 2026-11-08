#include <ultra64.h>
#include <string.h>

#include "actors/common1.h"
#include "area.h"
#include "audio/external.h"
#include "behavior_data.h"
#include "camera.h"
#include "course_table.h"
#include "dialog_ids.h"
#include "engine/math_util.h"
#include "eu_translation.h"
#include "game_init.h"
#include "gfx_dimensions.h"
#include "ingame_menu.h"
#include "level_update.h"
#include "levels/castle_grounds/header.h"
#include "memory.h"
#include "object_helpers.h"
#include "print.h"
#include "save_file.h"
#include "segment2.h"
#include "segment7.h"
#include "seq_ids.h"
#include "sm64.h"
#include "types.h"
#include "macros.h"
#include "hardcoded.h"
#include "pc/network/network.h"
#include "pc/djui/djui.h"
#include "pc/djui/djui_panel.h"
#include "pc/djui/djui_panel_pause.h"
#include "pc/utils/misc.h"
#include "data/dynos_mgr_builtin_externs.h"
#include "hud.h"
#include "pc/lua/smlua_hooks.h"
#include "game/camera.h"
#include "level_info.h"
#include "pc/lua/utils/smlua_text_utils.h"
#include "menu/ingame_text.h"
#include "pc/dialog_table.h"
#include "cn_font.h"

// CN-aware course/act name lookups. Declared in ingame_menu.h (star_select.c
// needs them too) and defined further down in this file.

// Runtime CN text-pack switch. It has to be visible from the dialog geometry
// further down (render_dialog_box_type), not just from the name lookups: the
// iQue dialog layout is a line taller and spaces its lines 20px apart, so
// switching only the strings would leave the text overflowing the US box.
#if defined(SM64COOPDX_CN_TEXT)
#define CN_PACK_ACTIVE() (gCnTextPackActive != 0)
#else
#define CN_PACK_ACTIVE() false
#endif

u16 gDialogColorFadeTimer;
s8 gLastDialogLineNum;
s32 gDialogVariable;
u16 gDialogTextAlpha;
#if defined(VERSION_EU)
s16 gDialogX; // D_8032F69A
s16 gDialogY; // D_8032F69C
#endif
s16 gCutsceneMsgXOffset;
s16 gCutsceneMsgYOffset;
s16 gDialogMinWidth = 0;
s16 gDialogOverrideX = 0;
s16 gDialogOverrideY = 0;
u8 gOverrideDialogPos = 0;
u8 gOverrideDialogColor = 0;
u8 gDialogBgColorR = 0;
u8 gDialogBgColorG = 0;
u8 gDialogBgColorB = 0;
u8 gDialogBgColorA = 0;
u8 gDialogTextColorR = 0;
u8 gDialogTextColorG = 0;
u8 gDialogTextColorB = 0;
u8 gDialogTextColorA = 0;

extern u8 gLastCompletedCourseNum;
extern u8 gLastCompletedStarNum;
u8 gLastCollectedStarOrKey = 0;

enum DialogBoxState {
    DIALOG_STATE_OPENING,
    DIALOG_STATE_VERTICAL,
    DIALOG_STATE_HORIZONTAL,
    DIALOG_STATE_CLOSING
};

enum DialogBoxPageState {
    DIALOG_PAGE_STATE_NONE,
    DIALOG_PAGE_STATE_SCROLL,
    DIALOG_PAGE_STATE_END
};

enum DialogBoxType {
    DIALOG_TYPE_ROTATE, // used in NPCs and level messages
    DIALOG_TYPE_ZOOM    // used in signposts and wall signs and etc
};

enum DialogMark { DIALOG_MARK_NONE = 0, DIALOG_MARK_DAKUTEN = 1, DIALOG_MARK_HANDAKUTEN = 2 };

#define DEFAULT_DIALOG_BOX_ANGLE 90.0f
#define DEFAULT_DIALOG_BOX_SCALE 19.0f

#if defined(VERSION_US) || defined(VERSION_EU)
u8 gDialogCharWidths[256] = { // TODO: Is there a way to auto generate this?
    7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  6,  6,  6,  6,  6,  6,
    6,  6,  5,  6,  6,  5,  8,  8,  6,  6,  6,  6,  6,  5,  6,  6,
    8,  7,  6,  6,  6,  5,  5,  6,  5,  5,  6,  5,  4,  5,  5,  3,
    7,  5,  5,  5,  6,  5,  5,  5,  5,  5,  7,  7,  5,  5,  4,  4,
    8,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    8,  8,  8,  8,  7,  7,  6,  7,  7,  0,  0,  0,  0,  0,  0,  0,
#ifdef VERSION_EU
    6,  6,  6,  0,  6,  6,  6,  0,  0,  0,  0,  0,  0,  0,  0,  4,
    5,  5,  5,  5,  6,  6,  6,  6,  0,  0,  0,  0,  0,  0,  0,  0,
    5,  5,  5,  0,  6,  6,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  5,  5,  0,  0,  6,  6,  0,  0,  0,  0,  0,  0,  0,  5,  6,
    0,  4,  4,  0,  0,  5,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0,
#else
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5,  6,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
#endif
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
#ifdef VERSION_EU
    7,  5, 10,  5,  9,  8,  4,  0,  0,  0,  0,  5,  5,  6,  5,  0,
#else
    7,  5, 10,  5,  9,  8,  4,  0,  0,  0,  0,  0,  0,  0,  0,  0,
#endif
    0,  0,  5,  7,  7,  6,  6,  8,  0,  8, 10,  6,  4, 10,  0,  0
};
#endif

s8 gDialogBoxState = DIALOG_STATE_OPENING;
f32 gDialogBoxOpenTimer = DEFAULT_DIALOG_BOX_ANGLE;
f32 gDialogBoxScale = DEFAULT_DIALOG_BOX_SCALE;
s16 gDialogScrollOffsetY = 0;
s8 gDialogBoxType = DIALOG_TYPE_ROTATE;
s32 gDialogID = DIALOG_NONE;
s16 gLastDialogPageStrPos = 0;
s16 gDialogTextPos = 0;
#ifdef VERSION_EU
s32 gInGameLanguage = 0;
#endif
s8 gDialogLineNum = 1;
s8 gLastDialogResponse = 0;
u8 gMenuHoldKeyIndex = 0;
u8 gMenuHoldKeyTimer = 0;
s32 gDialogResponse = 0;

bool gPauseMenuHidden = false;

#if defined(VERSION_JP) || defined(VERSION_SH) || defined(VERSION_EU)
#ifdef VERSION_EU
#define CHCACHE_BUFLEN (8 * 8)  // EU only converts 8x8
#else
#define CHCACHE_BUFLEN (8 * 16) // JP only converts 8x16 or 16x8 characters
#endif
// stores char data unpacked from ia1 to ia8 or ia4
// so that it won't be reconverted every time a character is rendered
static struct CachedChar { u8 used; u8 data[CHCACHE_BUFLEN]; } charCache[256];
#endif // VERSION

static Gfx *sDialogOffsetPos;
static Gfx *sDialogRotationPos;
static Gfx *sDialogZoomPos;

static f32 sDialogOffset;
static f32 sDialogOffsetPrev;
static f32 sDialogScale;
static f32 sDialogScalePrev;
static f32 sDialogRotation;
static f32 sDialogRotationPrev;

void patch_dialog_before(void) {
    sDialogOffsetPos   = NULL;
    sDialogRotationPos = NULL;
    sDialogZoomPos     = NULL;
}

void patch_dialog_interpolated(f32 delta) {
    Mtx *matrix;

    if (sDialogOffsetPos != NULL) {
        matrix = (Mtx *) alloc_display_list(sizeof(Mtx));
        if (matrix == NULL) { return; }
        f32 interpOffset = delta_interpolate_f32(sDialogOffsetPrev, sDialogOffset, delta);
        guTranslate(matrix, 0, interpOffset, 0);
        gSPMatrix(sDialogOffsetPos, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    }

    if (sDialogRotationPos != NULL) {
        matrix = (Mtx *) alloc_display_list(sizeof(Mtx));
        if (matrix == NULL) { return; }

        f32 interpScale = delta_interpolate_f32(sDialogScalePrev, sDialogScale, delta);
        guScale(matrix, 1.0 / interpScale, 1.0 / interpScale, 1.0f);
        gSPMatrix(sDialogRotationPos, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);

        matrix = (Mtx *) alloc_display_list(sizeof(Mtx));
        if (matrix == NULL) { return; }

        f32 interpRotation = delta_interpolate_f32(sDialogRotationPrev, sDialogRotation, delta);
        guRotate(matrix, interpRotation * 4.0f, 0, 0, 1.0f);
        gSPMatrix((sDialogRotationPos + 1), VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    }

    if (sDialogZoomPos != NULL) {
        matrix = (Mtx *) alloc_display_list(sizeof(Mtx));
        if (matrix == NULL) { return; }

        f32 interpScale = delta_interpolate_f32(sDialogScalePrev, sDialogScale, delta);
        guTranslate(matrix, 65.0 - (65.0 / interpScale), (40.0 / interpScale) - 40, 0);
        gSPMatrix(sDialogZoomPos, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);

        matrix = (Mtx *) alloc_display_list(sizeof(Mtx));
        if (matrix == NULL) { return; }

        guScale(matrix, 1.0 / interpScale, 1.0 / interpScale, 1.0f);
        gSPMatrix((sDialogZoomPos + 1), VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    }
}

void create_dl_identity_matrix(void) {
    Mtx *matrix = (Mtx *) alloc_display_list(sizeof(Mtx));

    if (matrix == NULL) {
        return;
    }

    guMtxIdent(matrix);

    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
}

void create_dl_translation_matrix(s8 pushOp, f32 x, f32 y, f32 z) {
    Mtx *matrix = (Mtx *) alloc_display_list(sizeof(Mtx));

    if (matrix == NULL) {
        return;
    }

    guTranslate(matrix, x, y, z);

    if (pushOp == MENU_MTX_PUSH)
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);

    if (pushOp == MENU_MTX_NOPUSH)
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
}

void create_dl_rotation_matrix(s8 pushOp, f32 a, f32 x, f32 y, f32 z) {
    Mtx *matrix = (Mtx *) alloc_display_list(sizeof(Mtx));

    if (matrix == NULL) {
        return;
    }

    guRotate(matrix, a, x, y, z);

    if (pushOp == MENU_MTX_PUSH)
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);

    if (pushOp == MENU_MTX_NOPUSH)
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
}

void create_dl_scale_matrix(s8 pushOp, f32 x, f32 y, f32 z) {
    Mtx *matrix = (Mtx *) alloc_display_list(sizeof(Mtx));

    if (matrix == NULL) {
        return;
    }

    guScale(matrix, x, y, z);

    if (pushOp == MENU_MTX_PUSH)
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);

    if (pushOp == MENU_MTX_NOPUSH)
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
}

void create_dl_ortho_matrix(void) {
    Mtx *matrix = (Mtx *) alloc_display_list(sizeof(Mtx));

    if (matrix == NULL) {
        return;
    }

    create_dl_identity_matrix();

    guOrtho(matrix, 0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -10.0f, 10.0f, 1.0f);

    // Should produce G_RDPHALF_1 in Fast3D
    gSPPerspNormalize(gDisplayListHead++, 0xFFFF);

    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH)
}

#if defined(VERSION_JP) || defined(VERSION_SH)
static inline void alloc_ia8_text_from_i1(u8 *out, u16 *in, s16 width, s16 height) {
    s32 inPos;
    u16 bitMask;
    u16 inWord;
    s16 outPos = 0;

    for (inPos = 0; inPos < (width * height) / 16; inPos++) {
        inWord = BE_TO_HOST16(in[inPos]);
        bitMask = 0x8000;

        while (bitMask != 0) {
            if (inWord & bitMask) {
                out[outPos] = 0xFF;
            } else {
                out[outPos] = 0x00;
            }

            bitMask /= 2;
            outPos++;
        }
    }
}

static inline u8 *convert_ia8_char(u8 c, u16 *tex, s16 w, s16 h) {
    if (!tex) return NULL;
    if (!charCache[c].used) {
        charCache[c].used = 1;
        alloc_ia8_text_from_i1(charCache[c].data, tex, w, h);
    }
    return charCache[c].data;
}
#endif

void render_generic_char(u8 c) {
    void **fontLUT;
    void *packedTexture;
#if defined(VERSION_JP) || defined(VERSION_SH)
    void *unpackedTexture;
#endif

    fontLUT = segmented_to_virtual(main_font_lut);
    packedTexture = segmented_to_virtual(fontLUT[c]);

#if defined(VERSION_JP) || defined(VERSION_SH)
    unpackedTexture = convert_ia8_char(c, packedTexture, 8, 16);

    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_8b, 1, VIRTUAL_TO_PHYSICAL(unpackedTexture));
#else
#ifdef VERSION_US
    gDPPipeSync(gDisplayListHead++);
#endif
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_16b, 1, VIRTUAL_TO_PHYSICAL(packedTexture));
#endif
    gSPDisplayList(gDisplayListHead++, dl_ia_text_tex_settings);
#ifdef VERSION_EU
    gSPTextureRectangleFlip(gDisplayListHead++, gDialogX << 2, (gDialogY - 16) << 2,
                            (gDialogX + 8) << 2, gDialogY << 2, G_TX_RENDERTILE, 8 << 6, 4 << 6, 1 << 10, 1 << 10);
#endif
}

#ifdef VERSION_EU
static void alloc_ia4_tex_from_i1(u8 *out, u8 *in, s16 width, s16 height) {
    u32 size = (u32) width * (u32) height;
    s32 inPos;
    s16 outPos = 0;
    u8 bitMask;

    for (inPos = 0; inPos < (width * height) / 4; inPos++) {
        bitMask = 0x80;
        while (bitMask != 0) {
            out[outPos] = (in[inPos] & bitMask) ? 0xF0 : 0x00;
            bitMask /= 2;
            out[outPos] = (in[inPos] & bitMask) ? out[outPos] + 0x0F : out[outPos];
            bitMask /= 2;
            outPos++;
        }
    }
}

static u8 *convert_ia4_char(u8 c, u8 *tex, s16 w, s16 h) {
    if (!tex) return NULL;
    if (!charCache[c].used) {
        charCache[c].used = 1;
        alloc_ia4_tex_from_i1(charCache[c].data, tex, w, h);
    }
    return charCache[c].data;
}

void render_generic_char_at_pos(s16 xPos, s16 yPos, u8 c) {
    void **fontLUT;
    void *packedTexture;
    void *unpackedTexture;

    fontLUT = segmented_to_virtual(main_font_lut);
    packedTexture = segmented_to_virtual(fontLUT[c]);
    unpackedTexture = convert_ia4_char(c, packedTexture, 8, 8);

    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_16b, 1, VIRTUAL_TO_PHYSICAL(unpackedTexture));
    gSPDisplayList(gDisplayListHead++, dl_ia_text_tex_settings);
    gSPTextureRectangleFlip(gDisplayListHead++, xPos << 2, (yPos - 16) << 2, (xPos + 8) << 2, yPos << 2,
                            G_TX_RENDERTILE, 8 << 6, 4 << 6, 1 << 10, 1 << 10);
}

void render_lowercase_diacritic(s16 *xPos, s16 *yPos, u8 letter, u8 diacritic) {
    render_generic_char_at_pos(*xPos, *yPos, letter);
    render_generic_char_at_pos(*xPos, *yPos, diacritic + 0xE7);
    *xPos += gDialogCharWidths[letter];
}

void render_uppercase_diacritic(s16 *xPos, s16 *yPos, u8 letter, u8 diacritic) {
    render_generic_char_at_pos(*xPos, *yPos, letter);
    render_generic_char_at_pos(*xPos, *yPos - 4, diacritic + 0xE3);
    *xPos += gDialogCharWidths[letter];
}
#endif // VERSION_EU

#if !defined(VERSION_JP) && !defined(VERSION_SH)
struct MultiTextEntry {
    u8 length;
    u8 str[4];
};

#define TEXT_THE_RAW ASCII_TO_DIALOG('t'), ASCII_TO_DIALOG('h'), ASCII_TO_DIALOG('e'), 0x00
#define TEXT_YOU_RAW ASCII_TO_DIALOG('y'), ASCII_TO_DIALOG('o'), ASCII_TO_DIALOG('u'), 0x00

enum MultiStringIDs { STRING_THE, STRING_YOU };

/*
 * Place the multi-text string according to the ID passed. (US, EU)
 * 0: 'the'
 * 1: 'you'
 */
#ifdef VERSION_US
void render_multi_text_string(s8 multiTextID)
#elif defined(VERSION_EU)
void render_multi_text_string(s16 *xPos, s16 *yPos, s8 multiTextID)
#endif
{
    s8 i;
    struct MultiTextEntry textLengths[2] = {
        { 3, { TEXT_THE_RAW } },
        { 3, { TEXT_YOU_RAW } },
    };

    for (i = 0; i < textLengths[multiTextID].length; i++) {
#ifdef VERSION_US
        render_generic_char(textLengths[multiTextID].str[i]);
        create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[textLengths[multiTextID].str[i]]), 0.0f, 0.0f);
#elif defined(VERSION_EU)
        render_generic_char_at_pos(*xPos, *yPos, textLengths[multiTextID].str[i]);
        *xPos += gDialogCharWidths[textLengths[multiTextID].str[i]];
#endif
    }
}
#endif

f32 get_generic_dialog_width(u8* dialog) {
#ifdef VERSION_JP
    return 0;
#else
    f32 largestWidth = 0;
    f32 width = 0;
    u8* d = dialog;
    s32 pos = 0;
    // The CN-ness cannot change during the walk, and cn_is_cn_string() is a
    // table scan (it now also walks tokens). Hoist it out of the loop.
    bool cn = cn_is_cn_string(d);

    // Token-wise walk: a CN glyph is two bytes and CN control tokens also
    // start with 0xFF, so the byte-wise `*d != TERMINATOR` test would stop at
    // the first CN newline (0xFF 0xFE). Iteration is capped so a vanilla
    // buffer can never drive this off the end of its array.
    for (s32 cap = 0; cap < 4096; cap++) {
        CnToken tok = cn_read_token(d, pos, cn);

        if (tok.kind == CN_TOKEN_END) {
            break;
        }

        if (tok.kind == CN_TOKEN_NEWLINE || (!tok.kind && d[pos] == DIALOG_CHAR_NEWLINE)) {
            width = 0;
            pos += tok.kind ? tok.length : 1;
            continue;
        }

        if (tok.kind == CN_TOKEN_SPACE) {
            // A CN full-width space advances the cursor but is not visible.
            width += (f32)gDialogCharWidths[DIALOG_CHAR_SPACE];
            largestWidth = MAX(width, largestWidth);
            pos += tok.length;
            continue;
        }

        if (tok.kind == CN_TOKEN_GLYPH) {
            // Two-byte CN glyph: one full-width advance, not two byte widths.
            width += cn_char_width(tok.code);
            largestWidth = MAX(width, largestWidth);
            pos += tok.length;
            continue;
        }

        // CN_TOKEN_NONE: a vanilla single-byte font code (e.g. the 0xFA star
        // icon). Fall through to the byte-wise handling below.
        if (d[pos] == DIALOG_CHAR_TERMINATOR) {
            break;
        }

        if (d[pos] == DIALOG_CHAR_NEWLINE) {
            width = 0;
            pos++;
            continue;
        }

        width += (f32)gDialogCharWidths[d[pos]];
        largestWidth = MAX(width, largestWidth);
        pos++;
    }

    return largestWidth;
#endif
}

f32 get_generic_ascii_string_width(const char* ascii) {
    u8 *str = convert_string_ascii_to_sm64(NULL, ascii, false);
    f32 width = get_generic_dialog_width(str);
    free(str);
    return width;
}

f32 get_generic_dialog_height(u8* dialog) {
    s32 lines = 0;
    u8* d = dialog;
    s32 pos = 0;
    bool cn = cn_is_cn_string(d);

    // Token-wise: count CN newlines (0xFF 0xFE) as well as vanilla 0xFE.
    // Iteration capped as above.
    for (s32 cap = 0; cap < 4096; cap++) {
        CnToken tok = cn_read_token(d, pos, cn);

        if (tok.kind == CN_TOKEN_END) {
            break;
        }
        if (tok.kind == CN_TOKEN_NEWLINE) {
            lines++;
            pos += tok.length;
            continue;
        }
        if (tok.kind) {
            pos += tok.length;
            continue;
        }
        if (d[pos] == DIALOG_CHAR_TERMINATOR) {
            break;
        }
        if (d[pos] == DIALOG_CHAR_NEWLINE) {
            lines++;
        }
        pos++;
    }
    return lines * 14;
}

f32 get_generic_ascii_string_height(const char* ascii) {
    u8 *str = convert_string_ascii_to_sm64(NULL, ascii, false);
    f32 height = get_generic_dialog_height(str);
    free(str);
    return height;
}

void print_generic_ascii_string(s16 x, s16 y, const char* ascii) {
    u8 *str = convert_string_ascii_to_sm64(NULL, ascii, false);
    print_generic_string(x, y, str);
    free(str);
}

#if defined(VERSION_JP) || defined(VERSION_SH)
#define MAX_STRING_WIDTH 18
#else
#define MAX_STRING_WIDTH 16
#endif

// Line advance for a CN newline token. CN cells are CN_GLYPH_H (16) tall and
// are drawn upwards from the cursor, so this has to be taller than 16 or
// consecutive lines touch. The iQue build advances 18 for exactly this reason.
#define CN_LINE_STEP (CN_GLYPH_H + 2)

/**
 * Prints a generic white string.
 * In JP/EU a IA1 texture is used but in US a IA4 texture is used.
 */
void print_generic_string(s16 x, s16 y, const u8 *str) {
    UNUSED s8 mark = DIALOG_MARK_NONE; // unused in EU
    s32 strPos = 0;
    u8 lineNum = 1;
    // Whether `str` uses the CN encoding cannot change while we walk it, and
    // cn_is_cn_string() is a table scan. Hoisting it out of the loop below
    // keeps this from rescanning the CN tables once per glyph.
    bool cn = cn_is_cn_string(str);
#ifdef VERSION_EU
    s16 xCoord = x;
    s16 yCoord = 240 - y;
#endif

#ifndef VERSION_EU
    create_dl_translation_matrix(MENU_MTX_PUSH, x, y, 0.0f);
#endif

    // NOTE: the loop test cannot be `str[strPos] != DIALOG_CHAR_TERMINATOR`
    // when CN text is in play: every CN control token starts with 0xFF, so a
    // CN newline (0xFF 0xFE) or space (0xFF 0x9E) would look like the end of
    // the string. The CN branch below decodes the full two-byte token and only
    // 0xFF 0xFF terminates.
    //
    // The iteration count is capped: if `str` ever turns out to be a vanilla
    // buffer (e.g. the ROM {0xFF, 0x00...} stubs), the CN walker would read
    // 0xFF 0x00 as a glyph and never find 0xFF 0xFF, running off the end of the
    // array. The cap turns that into a truncated string instead of a segfault.
    for (s32 strCap = 0; strCap < 4096; strCap++) {
        CnToken cnTok = cn_read_token(str, strPos, cn);

        if (cnTok.kind == CN_TOKEN_END) {
            break;
        }

        if (cnTok.kind == CN_TOKEN_NEWLINE) {
#ifdef VERSION_EU
            yCoord += 16;
            xCoord = x;
            lineNum++;
#else
            // Same re-anchor the vanilla 0xFE case does below: on non-EU the
            // cursor is the modelview matrix, so a newline has to pop the
            // current line and push a fresh one a line lower. Without this the
            // CN newline was simply swallowed and a whole multi-line CN string
            // (e.g. Peach's letter, DIALOG_020) ran off the right edge.
            //
            // The CN step is 18px, not MAX_STRING_WIDTH (16): a CN cell is
            // CN_GLYPH_H (16) tall and is drawn *upwards* from the cursor, so
            // 16 would butt each line against the one above it. 18 is what the
            // iQue build uses for its own newline (VERSION_CN's
            // MAX_STRING_WIDTH in n64decomp-cn src/game/ingame_menu.c).
            gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
            create_dl_translation_matrix(MENU_MTX_PUSH, x, y - (lineNum * CN_LINE_STEP), 0.0f);
            lineNum++;
#endif
            strPos += cnTok.length;
            continue;
        }

        if (cnTok.kind == CN_TOKEN_SPACE) {
#ifdef VERSION_EU
            xCoord += 5;
#else
            create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[DIALOG_CHAR_SPACE]), 0.0f, 0.0f);
#endif
            strPos += cnTok.length;
            continue;
        }

        if (cnTok.kind == CN_TOKEN_GLYPH) {
            render_cn_char(cnTok.code);
            create_dl_translation_matrix(MENU_MTX_NOPUSH, cn_char_width(cnTok.code), 0.0f, 0.0f);
            strPos += cnTok.length;
            continue;
        }

        // Not a CN token: fall through to the vanilla single-byte handling.
        // Re-check the terminator here because the loop test no longer does.
        if (str[strPos] == DIALOG_CHAR_TERMINATOR) {
            break;
        }

        switch (str[strPos]) {
#ifdef VERSION_EU
            case DIALOG_CHAR_SPACE:
                xCoord += 5;
                break;
            case DIALOG_CHAR_NEWLINE:
                yCoord += 16;
                xCoord = x;
                lineNum++;
                break;
            case DIALOG_CHAR_LOWER_A_GRAVE:
            case DIALOG_CHAR_LOWER_A_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_A_UMLAUT:
                render_lowercase_diacritic(&xCoord, &yCoord, ASCII_TO_DIALOG('a'), str[strPos] & 0xF);
                break;
            case DIALOG_CHAR_UPPER_A_UMLAUT: // @bug grave and circumflex (0x64-0x65) are absent here
                render_uppercase_diacritic(&xCoord, &yCoord, ASCII_TO_DIALOG('A'), str[strPos] & 0xF);
                break;
            case DIALOG_CHAR_LOWER_E_GRAVE:
            case DIALOG_CHAR_LOWER_E_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_E_UMLAUT:
            case DIALOG_CHAR_LOWER_E_ACUTE:
                render_lowercase_diacritic(&xCoord, &yCoord, ASCII_TO_DIALOG('e'), str[strPos] & 0xF);
                break;
            case DIALOG_CHAR_UPPER_E_GRAVE:
            case DIALOG_CHAR_UPPER_E_CIRCUMFLEX:
            case DIALOG_CHAR_UPPER_E_UMLAUT:
            case DIALOG_CHAR_UPPER_E_ACUTE:
                render_uppercase_diacritic(&xCoord, &yCoord, ASCII_TO_DIALOG('E'), str[strPos] & 0xF);
                break;
            case DIALOG_CHAR_LOWER_U_GRAVE:
            case DIALOG_CHAR_LOWER_U_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_U_UMLAUT:
                render_lowercase_diacritic(&xCoord, &yCoord, ASCII_TO_DIALOG('u'), str[strPos] & 0xF);
                break;
            case DIALOG_CHAR_UPPER_U_UMLAUT: // @bug grave and circumflex (0x84-0x85) are absent here
                render_uppercase_diacritic(&xCoord, &yCoord, ASCII_TO_DIALOG('U'), str[strPos] & 0xF);
                break;
            case DIALOG_CHAR_LOWER_O_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_O_UMLAUT:
                render_lowercase_diacritic(&xCoord, &yCoord, ASCII_TO_DIALOG('o'), str[strPos] & 0xF);
                break;
            case DIALOG_CHAR_UPPER_O_UMLAUT: // @bug circumflex (0x95) is absent here
                render_uppercase_diacritic(&xCoord, &yCoord, ASCII_TO_DIALOG('O'), str[strPos] & 0xF);
                break;
            case DIALOG_CHAR_LOWER_I_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_I_UMLAUT:
                render_lowercase_diacritic(&xCoord, &yCoord, DIALOG_CHAR_I_NO_DIA, str[strPos] & 0xF);
                break;
#else // i.e. not EU
            case DIALOG_CHAR_DAKUTEN:
                mark = DIALOG_MARK_DAKUTEN;
                break;
            case DIALOG_CHAR_PERIOD_OR_HANDAKUTEN:
                mark = DIALOG_MARK_HANDAKUTEN;
                break;
            case DIALOG_CHAR_NEWLINE:
                gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
                create_dl_translation_matrix(MENU_MTX_PUSH, x, y - (lineNum * MAX_STRING_WIDTH), 0.0f);
                lineNum++;
                break;
            case DIALOG_CHAR_PERIOD:
                create_dl_translation_matrix(MENU_MTX_PUSH, -2.0f, -5.0f, 0.0f);
                render_generic_char(DIALOG_CHAR_PERIOD_OR_HANDAKUTEN);
                gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
                break;
#endif
#if !defined(VERSION_JP) && !defined(VERSION_SH)
            case DIALOG_CHAR_SLASH:
#ifdef VERSION_US
                create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[DIALOG_CHAR_SPACE] * 2), 0.0f, 0.0f);
#elif defined(VERSION_EU)
                xCoord += gDialogCharWidths[DIALOG_CHAR_SPACE] * 2;
#endif
                break;
            case DIALOG_CHAR_MULTI_THE:
#ifdef VERSION_EU
                render_multi_text_string(&xCoord, &yCoord, STRING_THE);
#else
                render_multi_text_string(STRING_THE);
#endif
                break;
            case DIALOG_CHAR_MULTI_YOU:
#ifdef VERSION_EU
                render_multi_text_string(&xCoord, &yCoord, STRING_YOU);
#else
                render_multi_text_string(STRING_YOU);
#endif
                break;
#endif
#ifndef VERSION_EU
            case DIALOG_CHAR_SPACE:
#if defined(VERSION_JP) || defined(VERSION_SH)
                create_dl_translation_matrix(MENU_MTX_NOPUSH, 5.0f, 0.0f, 0.0f);
                break;
#else
                create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[DIALOG_CHAR_SPACE]), 0.0f, 0.0f);
#endif
#endif
                break; // ? needed to match
            default:
#ifdef VERSION_EU
                render_generic_char_at_pos(xCoord, yCoord, str[strPos]);
                xCoord += gDialogCharWidths[str[strPos]];
                break;
#else
                render_generic_char(str[strPos]);
                if (mark != DIALOG_MARK_NONE) {
                    create_dl_translation_matrix(MENU_MTX_PUSH, 5.0f, 5.0f, 0.0f);
                    render_generic_char(mark + 0xEF);
                    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
                    mark = DIALOG_MARK_NONE;
                }

#if defined(VERSION_JP) || defined(VERSION_SH)
                create_dl_translation_matrix(MENU_MTX_NOPUSH, 10.0f, 0.0f, 0.0f);
#else
                create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[str[strPos]]), 0.0f, 0.0f);
                break; // what an odd difference. US added a useless break here.
#endif
#endif
        }

        strPos++;
    }

#ifndef VERSION_EU
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
#endif
}

#ifdef VERSION_EU
void print_hud_char_umlaut(s16 x, s16 y, u8 chr) {
    void **fontLUT = segmented_to_virtual(main_hud_lut);

    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, fontLUT[chr]);
    gSPDisplayList(gDisplayListHead++, dl_rgba16_load_tex_block);
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 16) << 2, (y + 16) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, fontLUT[GLYPH_UMLAUT]);
    gSPDisplayList(gDisplayListHead++, dl_rgba16_load_tex_block);
    gSPTextureRectangle(gDisplayListHead++, x << 2, (y - 4) << 2, (x + 16) << 2, (y + 12) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
}
#endif

/**
 * Prints a hud string depending of the hud table list defined.
 */
void print_hud_lut_string(s8 hudLUT, s16 x, s16 y, const u8 *str) {
    s32 strPos = 0;
    void **hudLUT1 = segmented_to_virtual(menu_hud_lut); // Japanese Menu HUD Color font
    void **hudLUT2 = segmented_to_virtual(main_hud_lut); // 0-9 A-Z HUD Color Font
    u32 curX = x;
    u32 curY = y;

    u32 xStride; // X separation

#if defined(SM64COOPDX_CN_TEXT)
    // The HUD LUT is an RGBA16 coloured 8x8 font and has no CJK glyphs, so a CN
    // string cannot go through the vanilla path at all -- every 0xFF lead byte
    // would end the line immediately. Hand those to print_generic_string(),
    // which already renders CJK through the IA8 dialog font.
    //
    // This is what makes the pause screen's "PAUSE" title and the HUD's
    // "HI SCORE" / "CONGRATULATIONS" lines translate: they are all drawn with
    // this function rather than the menu font.
    //
    // The two paths disagree about where y=0 is, and getting this wrong is
    // silent -- the text still draws, just in the wrong place:
    //   - this function emits gSPTextureRectangle, which gfx_draw_rectangle()
    //     maps straight to NDC with `ndc_y = -(y / (4 * HALF_HEIGHT)) + 1`,
    //     i.e. y grows DOWNWARD from the top of the screen. Every caller relies
    //     on that: y=32 is the HUD timer, y=185 the coin counter.
    //   - print_generic_string() pushes an ortho matrix from
    //     create_dl_ortho_matrix(), in which y grows UPWARD from the bottom.
    // Passing y through unchanged therefore lands a CN string mirrored about
    // the middle of the screen. That is exactly how the pause menu's "暂停"
    // ended up drawn at row 81-from-top == row 143-from-bottom, i.e. on top of
    // the course-name row, where it read as a smudge behind "炸弹王国" rather
    // than as a title.
    //
    // Convert explicitly. A CN glyph cell is CN_GLYPH_H tall and
    // print_generic_string() takes the BOTTOM edge, so a glyph whose top edge
    // should sit at screen row `y` is drawn at menu row
    // SCREEN_HEIGHT - y - CN_GLYPH_H.
    if (cn_is_cn_string(str)) {
        print_generic_string(x, SCREEN_HEIGHT - y - CN_GLYPH_H, str);
        return;
    }
#endif

    if (hudLUT == HUD_LUT_JPMENU) {
        xStride = 16;
    } else { // HUD_LUT_GLOBAL
#ifdef VERSION_JP
        xStride = 14;
#else
        xStride = 12; //? Shindou uses this.
#endif
    }

    while (str[strPos] != GLOBAR_CHAR_TERMINATOR) {
#ifndef VERSION_JP
        switch (str[strPos]) {
#ifdef VERSION_EU
            case GLOBAL_CHAR_SPACE:
                curX += xStride / 2;
                break;
            case HUD_CHAR_A_UMLAUT:
                print_hud_char_umlaut(curX, curY, ASCII_TO_DIALOG('A'));
                curX += xStride;
                break;
            case HUD_CHAR_O_UMLAUT:
                print_hud_char_umlaut(curX, curY, ASCII_TO_DIALOG('O'));
                curX += xStride;
                break;
            case HUD_CHAR_U_UMLAUT:
                print_hud_char_umlaut(curX, curY, ASCII_TO_DIALOG('U'));
                curX += xStride;
                break;
#else
            case GLOBAL_CHAR_SPACE:
                curX += 8;
                break;
#endif
            default:
#endif
                gDPPipeSync(gDisplayListHead++);

                if (hudLUT == HUD_LUT_JPMENU) {
                    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, hudLUT1[str[strPos]]);
                }

                if (hudLUT == HUD_LUT_GLOBAL) {
                    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, hudLUT2[str[strPos]]);
                }

                gSPDisplayList(gDisplayListHead++, dl_rgba16_load_tex_block);
                gSPTextureRectangle(gDisplayListHead++, curX << 2, curY << 2, (curX + 16) << 2,
                                    (curY + 16) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

                curX += xStride;
#ifndef VERSION_JP
        }
#endif
        strPos++;
    }
}

#ifdef VERSION_EU
void print_menu_char_umlaut(s16 x, s16 y, u8 chr) {
    void **fontLUT = segmented_to_virtual(menu_font_lut);

    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_8b, 1, fontLUT[chr]);
    gDPLoadSync(gDisplayListHead++);
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 8 * 8 - 1, CALC_DXT(8, G_IM_SIZ_8b_BYTES));
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 8) << 2, (y + 8) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_8b, 1, fontLUT[DIALOG_CHAR_UMLAUT]);
    gDPLoadSync(gDisplayListHead++);
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 8 * 8 - 1, CALC_DXT(8, G_IM_SIZ_8b_BYTES));
    gSPTextureRectangle(gDisplayListHead++, x << 2, (y - 4) << 2, (x + 8) << 2, (y + 4) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
}
#endif

// Re-arm just the tile parameters the vanilla branches of
// print_menu_generic_string() rely on, after a CJK glyph borrowed the render
// tile for its 16x16 cell.
//
// This deliberately does NOT replay dl_menu_ia8_text_begin, even though that is
// the obvious "put it back" call: the display list also resets the environment
// colour to opaque white (see levels/menu/leveldata.c), and every screen that
// prints its menu text black -- the act select torna sets env colour (0,0,0)
// right before this -- would have its following vanilla characters silently
// flipped to white.
static void restore_menu_ia8_text_tile(void) {
    gDPSetTile(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_8b, 0, 0, G_TX_LOADTILE, 0,
               G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD,
               G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD);
    gDPTileSync(gDisplayListHead++);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_8b, 1, 0, G_TX_RENDERTILE, 0,
               G_TX_CLAMP, 3, G_TX_NOLOD, G_TX_CLAMP, 3, G_TX_NOLOD);
    gDPSetTileSize(gDisplayListHead++, G_TX_RENDERTILE, 0, 0,
                   (8 - 1) << G_TEXTURE_IMAGE_FRAC, (8 - 1) << G_TEXTURE_IMAGE_FRAC);
}

void print_menu_generic_string(s16 x, s16 y, const u8 *str) {
    UNUSED s8 mark = DIALOG_MARK_NONE; // unused in EU
    s32 strPos = 0;
    u32 curX = x;
    u32 curY = y;
    void **fontLUT = segmented_to_virtual(menu_font_lut);
    bool cn = cn_is_cn_string(str);
    // A CJK glyph is drawn with cn_draw_glyph_rect(), which reprograms
    // G_TX_RENDERTILE for a 16x16 IA8 cell. Every vanilla branch below draws a
    // bare gSPTextureRectangle and relies on the tile dl_menu_ia8_text_begin
    // installed, so that tile has to be re-armed before the next vanilla
    // character -- otherwise 8x8 menu glyphs are sampled with CJK geometry and
    // smear. See restore_menu_ia8_text_tile() for why the display list itself
    // is not replayed. (The dialog counterpart render_generic_char() re-emits
    // its own tile settings every call, which is why print_generic_string()
    // does not need this.)
    //
    // This flag is file-static, NOT a local: the tile is display-list state and
    // outlives any single call. The star select screen draws a CJK act name and
    // then the vanilla 8x8 star numbers through separate calls, and a local
    // would be re-initialised to false by the second call -- so the numbers
    // would be sampled with the CJK 16x16 geometry and come out as garbage.
    // That breakage is independent of the text-pack switch, which is why it
    // persisted with the pack both on and off.
    static bool tileDirty = false;

    // NOTE: the loop test cannot stay a byte-wise
    // `str[strPos] != DIALOG_CHAR_TERMINATOR` when CN text is in play: every CN
    // control token starts with 0xFF, so a CN space (0xFF 0x9E) would read as
    // end-of-string and truncate the rest of the line. The CN branch decodes
    // whole two-byte tokens and only 0xFF 0xFF terminates.
    //
    // The iteration count is capped so a vanilla buffer can never be walked off
    // the end of its array.
    for (s32 cap = 0; cap < 1024; cap++) {
        if (cn) {
            CnToken cnTok = cn_read_token(str, strPos, true);

            if (cnTok.kind == CN_TOKEN_END) {
                break;
            }
            if (cnTok.kind == CN_TOKEN_NEWLINE) {
                strPos += cnTok.length;
                continue;
            }
            if (cnTok.kind == CN_TOKEN_SPACE) {
                curX += 4;
                strPos += cnTok.length;
                continue;
            }
            if (cnTok.kind == CN_TOKEN_GLYPH) {
                // A CJK cell is 16px tall and is drawn from curY downwards, but a
                // vanilla menu glyph is only 8px and is also anchored at curY.
                // Inside a CN string the two therefore disagree by 4px: the
                // digit in "冰雪中的8枚红色硬币" sat on the top edge of the line
                // instead of its middle. Shift the 8px glyphs down by half the
                // difference so mixed strings share one baseline.
                u32 glyphY = cn ? (curY + 4) : curY;

                if (cn_glyph_is_vanilla(cnTok.code)) {
                    // 0x00xx is bit-identical to a menu-font index (digits,
                    // A-Z, arrows). Draw it from the menu LUT rather than via
                    // render_cn_char(), which would use the dialog font and so
                    // change the glyph's look mid-string.
                    u8 c = cn_glyph_vanilla_byte(cnTok.code);
                    if (tileDirty) {
                        restore_menu_ia8_text_tile();
                        tileDirty = false;
                    }
                    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_8b, 1, fontLUT[c]);
                    gDPLoadSync(gDisplayListHead++);
                    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 8 * 8 - 1, CALC_DXT(8, G_IM_SIZ_8b_BYTES));
                    gSPTextureRectangle(gDisplayListHead++, curX << 2, glyphY << 2,
                                        (curX + 8) << 2, (glyphY + 8) << 2,
                                        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
                    curX += gDialogCharWidths[c];
                } else {
                    // CJK: a 16x16 cell drawn as a plain rectangle, exactly
                    // like the vanilla branches below. Do NOT push a matrix and
                    // reuse render_cn_char() -- see cn_draw_glyph_rect().
                    cn_draw_glyph_rect(curX, curY, cnTok.code);
                    tileDirty = true;
                    curX += (u32) cn_char_width(cnTok.code);
                }
                strPos += cnTok.length;
                continue;
            }
            // CN_TOKEN_NONE: a vanilla LUT byte embedded in a CN string (the
            // 0xFA/0xFD star icons). Fall through to the vanilla path below.
        }

        // Re-check the terminator here, because the loop test no longer does.
        if (str[strPos] == DIALOG_CHAR_TERMINATOR) {
            break;
        }

        if (tileDirty) {
            restore_menu_ia8_text_tile();
            tileDirty = false;
        }

        switch (str[strPos]) {
#ifdef VERSION_EU
            case DIALOG_CHAR_UPPER_A_UMLAUT:
                print_menu_char_umlaut(curX, curY, ASCII_TO_DIALOG('A'));
                curX += gDialogCharWidths[str[strPos]];
                break;
            case DIALOG_CHAR_UPPER_U_UMLAUT:
                print_menu_char_umlaut(curX, curY, ASCII_TO_DIALOG('U'));
                curX += gDialogCharWidths[str[strPos]];
                break;
            case DIALOG_CHAR_UPPER_O_UMLAUT:
                print_menu_char_umlaut(curX, curY, ASCII_TO_DIALOG('O'));
                curX += gDialogCharWidths[str[strPos]];
                break;
#else
            case DIALOG_CHAR_DAKUTEN:
                mark = DIALOG_MARK_DAKUTEN;
                break;
            case DIALOG_CHAR_PERIOD_OR_HANDAKUTEN:
                mark = DIALOG_MARK_HANDAKUTEN;
                break;
#endif
            case DIALOG_CHAR_SPACE:
                curX += 4;
                break;
            default:
                // Same half-line correction as the CN-glyph branch above: an
                // 8px vanilla glyph inside a 16px CN line sits 4px high.
                gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_8b, 1, fontLUT[str[strPos]]);
                gDPLoadSync(gDisplayListHead++);
                gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 8 * 8 - 1, CALC_DXT(8, G_IM_SIZ_8b_BYTES));
                gSPTextureRectangle(gDisplayListHead++, curX << 2, (curY + (cn ? 4 : 0)) << 2, (curX + 8) << 2,
                                    (curY + (cn ? 4 : 0) + 8) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

#ifndef VERSION_EU
                if (mark != DIALOG_MARK_NONE) {
                    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_8b, 1, fontLUT[mark + 0xEF]);
                    gDPLoadSync(gDisplayListHead++);
                    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 8 * 8 - 1, CALC_DXT(8, G_IM_SIZ_8b_BYTES));
                    gSPTextureRectangle(gDisplayListHead++, (curX + 6) << 2, (curY - 7 + (cn ? 4 : 0)) << 2,
                                        (curX + 6 + 8) << 2, (curY - 7 + (cn ? 4 : 0) + 8) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

                    mark = DIALOG_MARK_NONE;
                }
#endif
#if defined(VERSION_JP) || defined(VERSION_SH)
                curX += 9;
#else
                curX += gDialogCharWidths[str[strPos]];
#endif
        }
        strPos++;
    }
}

void print_credits_string(s16 x, s16 y, const u8 *str) {
    s32 strPos = 0;
    void **fontLUT = segmented_to_virtual(main_credits_font_lut);
    u32 curX = x;
    u32 curY = y;

    gDPSetTile(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0,
                G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD);
    gDPTileSync(gDisplayListHead++);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 2, 0, G_TX_RENDERTILE, 0,
                G_TX_CLAMP, 3, G_TX_NOLOD, G_TX_CLAMP, 3, G_TX_NOLOD);
    gDPSetTileSize(gDisplayListHead++, G_TX_RENDERTILE, 0, 0, (8 - 1) << G_TEXTURE_IMAGE_FRAC, (8 - 1) << G_TEXTURE_IMAGE_FRAC);

    while (str[strPos] != GLOBAR_CHAR_TERMINATOR) {
        switch (str[strPos]) {
            case GLOBAL_CHAR_SPACE:
                curX += 4;
                break;
            default:
                gDPPipeSync(gDisplayListHead++);
                gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, fontLUT[str[strPos]]);
                gDPLoadSync(gDisplayListHead++);
                gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 8 * 8 - 1, CALC_DXT(8, G_IM_SIZ_16b_BYTES));
                gSPTextureRectangle(gDisplayListHead++, curX << 2, curY << 2, (curX + 8) << 2,
                                    (curY + 8) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
                curX += 7;
                break;
        }
        strPos++;
    }
}

void handle_menu_scrolling(s8 scrollDirection, s8 *currentIndex, s8 minIndex, s8 maxIndex) {
    u8 index = 0;

    if (scrollDirection == MENU_SCROLL_VERTICAL) {
        if (gPlayer1Controller->rawStickY > 60) {
            index++;
        }

        if (gPlayer1Controller->rawStickY < -60) {
            index += 2;
        }
    } else if (scrollDirection == MENU_SCROLL_HORIZONTAL) {
        if (gPlayer1Controller->rawStickX > 60) {
            index += 2;
        }

        if (gPlayer1Controller->rawStickX < -60) {
            index++;
        }
    }

    if (((index ^ gMenuHoldKeyIndex) & index) == 2) {
        if (currentIndex[0] == maxIndex) {
            //! Probably originally a >=, but later replaced with an == and an else statement.
            currentIndex[0] = maxIndex;
        } else {
            play_sound(SOUND_MENU_CHANGE_SELECT, gGlobalSoundSource);
            currentIndex[0]++;
        }
    }

    if (((index ^ gMenuHoldKeyIndex) & index) == 1) {
        if (currentIndex[0] == minIndex) {
            // Same applies to here as above
        } else {
            play_sound(SOUND_MENU_CHANGE_SELECT, gGlobalSoundSource);
            currentIndex[0]--;
        }
    }

    // Clamp currentIndex to prevent out of bounds access
    if (currentIndex[0] < minIndex) {
        currentIndex[0] = minIndex;
    }
    if (currentIndex[0] > maxIndex) {
        currentIndex[0] = maxIndex;
    }

    if (gMenuHoldKeyTimer == 10) {
        gMenuHoldKeyTimer = 8;
        gMenuHoldKeyIndex = 0;
    } else {
        gMenuHoldKeyTimer++;
        gMenuHoldKeyIndex = index;
    }

    if ((index & 3) == 0) {
        gMenuHoldKeyTimer = 0;
    }
}

// EU has both get_str_x_pos_from_center and get_str_x_pos_from_center_scale
// US and JP only implement one or the other
#if defined(VERSION_US) || defined(VERSION_EU)
s16 get_str_x_pos_from_center(s16 centerPos, u8 *str, UNUSED f32 scale) {
    s16 strPos = 0;
    f32 spacesWidth = 0.0f;
    bool cn = cn_is_cn_string(str);

    // Token-wise: CN glyphs are two-byte and full-width, and CN control tokens
    // begin with 0xFF, so a byte-wise terminator test would stop early.
    // Iteration capped so a vanilla buffer cannot run off its array.
    for (s32 cap = 0; cap < 4096; cap++) {
        CnToken tok = cn_read_token(str, strPos, cn);

        if (tok.kind == CN_TOKEN_END) {
            break;
        }
        if (tok.kind == CN_TOKEN_NEWLINE) {
            strPos += tok.length;
            continue;
        }
        if (tok.kind == CN_TOKEN_SPACE) {
            spacesWidth += (f32)gDialogCharWidths[DIALOG_CHAR_SPACE];
            strPos += tok.length;
            continue;
        }
        if (tok.kind == CN_TOKEN_GLYPH) {
            spacesWidth += cn_char_width(tok.code);
            strPos += tok.length;
            continue;
        }
        // CN_TOKEN_NONE: vanilla byte, handled below.
        if (str[strPos] == DIALOG_CHAR_TERMINATOR) {
            break;
        }
        if (str[strPos] == DIALOG_CHAR_NEWLINE) {
            strPos++;
            continue;
        }
        spacesWidth += (f32)gDialogCharWidths[str[strPos]];
        strPos++;
    }
    // return the x position of where the string starts as half the string's
    // length from the position of the provided center.
    return (s16)(centerPos - (s16)(spacesWidth / 2.0));
}
#endif

#if defined(VERSION_JP) || defined(VERSION_EU) || defined(VERSION_SH)
s16 get_str_x_pos_from_center_scale(s16 centerPos, u8 *str, f32 scale) {
    s16 strPos = 0;
    f32 charsWidth = 0.0f;
    f32 spacesWidth = 0.0f;

    while (str[strPos] != DIALOG_CHAR_TERMINATOR) {
        //! EU checks for dakuten and handakuten despite dialog code unable to handle it
        if (str[strPos] == DIALOG_CHAR_SPACE) {
            spacesWidth += 1.0;
        } else if (str[strPos] != DIALOG_CHAR_DAKUTEN
                   && str[strPos] != DIALOG_CHAR_PERIOD_OR_HANDAKUTEN) {
            charsWidth += 1.0;
        }
        strPos++;
    }
    // return the x position of where the string starts as half the string's
    // length from the position of the provided center.
    return (f32) centerPos - (scale * (charsWidth / 2.0)) - ((scale / 2.0) * (spacesWidth / 2.0));
}
#endif

#if !defined(VERSION_JP) && !defined(VERSION_SH)
s16 get_string_width(u8 *str) {
    s16 strPos = 0;
    s16 width = 0;
    bool cn = cn_is_cn_string(str);

    // Token-wise walk; see get_str_x_pos_from_center for the rationale.
    // Iteration capped for the same reason.
    for (s32 cap = 0; cap < 4096; cap++) {
        CnToken tok = cn_read_token(str, strPos, cn);

        if (tok.kind == CN_TOKEN_END) {
            break;
        }
        if (tok.kind) {
            if (tok.kind == CN_TOKEN_GLYPH) {
                width += (s16)cn_char_width(tok.code);
            } else if (tok.kind == CN_TOKEN_SPACE) {
                width += gDialogCharWidths[DIALOG_CHAR_SPACE];
            }
            strPos += tok.length;
            continue;
        }
        if (str[strPos] == DIALOG_CHAR_TERMINATOR) {
            break;
        }
        width += gDialogCharWidths[str[strPos]];
        strPos++;
    }
    return width;
}
#endif

u8 gHudSymCoin[] = { GLYPH_COIN, GLYPH_SPACE };
u8 gHudSymX[] = { GLYPH_MULTIPLY, GLYPH_SPACE };

void print_hud_my_score_coins(s32 useCourseCoinScore, s8 fileNum, s8 courseNum, s16 x, s16 y) {
    u8 strNumCoins[4];
    s16 numCoins;

    if (!useCourseCoinScore) {
        numCoins = (u16)(save_file_get_max_coin_score(courseNum) & 0xFFFF);
    } else {
        numCoins = save_file_get_course_coin_score(fileNum, courseNum);
    }

    if (numCoins != 0) {
        print_hud_lut_string(HUD_LUT_GLOBAL, x, y, gHudSymCoin);
        print_hud_lut_string(HUD_LUT_GLOBAL, x + 16, y, gHudSymX);
        int_to_str(numCoins, strNumCoins);
        print_hud_lut_string(HUD_LUT_GLOBAL, x + 32, y, strNumCoins);
    }
}

void print_hud_my_score_stars(s8 fileNum, s8 courseNum, s16 x, s16 y) {
    u8 strStarCount[4];
    s16 starCount;
    u8 textSymStar[] = { GLYPH_STAR, GLYPH_SPACE };
    UNUSED u16 unused;
    u8 textSymX[] = { GLYPH_MULTIPLY, GLYPH_SPACE };

    starCount = save_file_get_course_star_count(fileNum, courseNum);

    if (starCount != 0) {
        print_hud_lut_string(HUD_LUT_GLOBAL, x, y, textSymStar);
        print_hud_lut_string(HUD_LUT_GLOBAL, x + 16, y, textSymX);
        int_to_str(starCount, strStarCount);
        print_hud_lut_string(HUD_LUT_GLOBAL, x + 32, y, strStarCount);
    }
}

void int_to_str(s32 num, u8 *dst) {
    s32 digit1;
    s32 digit2;
    s32 digit3;

    s8 pos = 0;

    if (num > 999) {
        dst[0] = 0x00; dst[1] = DIALOG_CHAR_TERMINATOR;
        return;
    }

    digit1 = num / 100;
    digit2 = (num - digit1 * 100) / 10;
    digit3 = (num - digit1 * 100) - (digit2 * 10);

    if (digit1 != 0) {
        dst[pos++] = digit1;
    }

    if (digit2 != 0 || digit1 != 0) {
        dst[pos++] = digit2;
    }

    dst[pos++] = digit3;
    dst[pos] = DIALOG_CHAR_TERMINATOR;
}

s32 get_dialog_id(void) {
    return gDialogID;
}

void handle_special_dialog_text(s32 dialogID) { // dialog ID tables, in order
    // King Bob-omb (Start), Whomp (Start), King Bob-omb (throw him out), Eyerock (Start), Wiggler (Start)
    enum DialogId dialogBossStart[] = { DIALOG_017, DIALOG_114, DIALOG_128, DIALOG_117, DIALOG_150 };
    // Koopa the Quick (BOB), Koopa the Quick (THI), Penguin Race, Fat Penguin Race (120 stars)
    enum DialogId dialogRaceSound[] = { DIALOG_005, DIALOG_009, DIALOG_055, DIALOG_164 };
    // Red Switch, Green Switch, Blue Switch, 100 coins star, Bowser Red Coin Star
    enum DialogId dialogStarSound[] = { DIALOG_010, DIALOG_011, DIALOG_012, DIALOG_013, DIALOG_014 };
    // King Bob-omb (Start), Whomp (Defeated), King Bob-omb (Defeated, missing in JP), Eyerock (Defeated), Wiggler (Defeated)
#if BUGFIX_KING_BOB_OMB_FADE_MUSIC
    enum DialogId dialogBossStop[] = { DIALOG_017, DIALOG_115, DIALOG_116, DIALOG_118, DIALOG_152 };
#else
    //! @bug JP misses King Bob-omb defeated dialog "116", meaning that the boss music will still
    //! play after King Bob-omb is defeated until BOB loads it's music after the star cutscene
    enum DialogId dialogBossStop[] = { DIALOG_017, DIALOG_115, DIALOG_118, DIALOG_152 };
#endif
    s16 i;

    for (i = 0; i < (s16) ARRAY_COUNT(dialogBossStart); i++) {
        if (dialogBossStart[i] == dialogID) {
            seq_player_unlower_volume(SEQ_PLAYER_LEVEL, 60);
            play_music(SEQ_PLAYER_LEVEL, SEQUENCE_ARGS(4, SEQ_EVENT_BOSS), 0);
            return;
        }
    }

    for (i = 0; i < (s16) ARRAY_COUNT(dialogRaceSound); i++) {
        if (dialogRaceSound[i] == dialogID && gDialogLineNum == 1) {
            play_race_fanfare();
            return;
        }
    }

    for (i = 0; i < (s16) ARRAY_COUNT(dialogStarSound); i++) {
        if (dialogStarSound[i] == dialogID && gDialogLineNum == 1) {
            play_sound(SOUND_MENU_STAR_SOUND, gGlobalSoundSource);
            return;
        }
    }

    for (i = 0; i < (s16) ARRAY_COUNT(dialogBossStop); i++) {
        if (dialogBossStop[i] == dialogID) {
            seq_player_fade_out(SEQ_PLAYER_LEVEL, 1);
            return;
        }
    }
}

static u8 *sOverrideDialogHookString = NULL;

bool handle_dialog_hook(s32 dialogId) {
    bool openDialogBox = true;
    const char *dialogTextOverride = NULL;
    smlua_call_event_hooks(HOOK_ON_DIALOG, dialogId, &openDialogBox, &dialogTextOverride);
    if (!openDialogBox) {
        if (gCamera->cutscene == CUTSCENE_READ_MESSAGE) { gCamera->cutscene = 0; }
        return false;
    }

    free(sOverrideDialogHookString);
    if (dialogTextOverride != NULL) {
        sOverrideDialogHookString = convert_string_ascii_to_sm64(NULL, dialogTextOverride, false);
    } else {
        sOverrideDialogHookString = NULL;
    }
    return true;
}

void create_dialog_box(s32 dialog) {
    if (handle_dialog_hook(dialog) && gDialogID == DIALOG_NONE) {
        gDialogID = dialog;
        gDialogBoxType = DIALOG_TYPE_ROTATE;
    }
}

void create_dialog_box_with_var(s32 dialog, s32 dialogVar) {
    if (handle_dialog_hook(dialog) && gDialogID == DIALOG_NONE) {
        gDialogID = dialog;
        gDialogVariable = dialogVar;
        gDialogBoxType = DIALOG_TYPE_ROTATE;
    }
}

void create_dialog_inverted_box(s32 dialog) {
    if (handle_dialog_hook(dialog) && gDialogID == DIALOG_NONE) {
        gDialogID = dialog;
        gDialogBoxType = DIALOG_TYPE_ZOOM;
    }
}

void create_dialog_box_with_response(s32 dialog) {
    if (handle_dialog_hook(dialog) && gDialogID == DIALOG_NONE) {
        gDialogID = dialog;
        gDialogBoxType = DIALOG_TYPE_ROTATE;
        gLastDialogResponse = 1;
    }
}

void reset_dialog_render_state(void) {
    level_set_transition(0, NULL);

    if (gDialogBoxType == DIALOG_TYPE_ZOOM) {
        trigger_cutscene_dialog(2);
    }

    gDialogBoxScale = 19.0f;
    gDialogBoxOpenTimer = 90.0f;
    gDialogBoxState = DIALOG_STATE_OPENING;
    gDialogID = DIALOG_NONE;
    gDialogTextPos = 0;
    gLastDialogResponse = 0;
    gLastDialogPageStrPos = 0;
    gDialogResponse = 0;
}

#if defined(VERSION_JP) || defined(VERSION_SH)
#define X_VAL1 -5.0f
#define Y_VAL1 2.0
#define Y_VAL2 4
#else
#define X_VAL1 -7.0f
#define Y_VAL1 5.0
#define Y_VAL2 5.0f
#endif

void render_dialog_box_type(struct DialogEntry *dialog, s8 linesPerBox) {
    UNUSED s32 unused;

    if (gOverrideDialogPos != 0) {
        create_dl_translation_matrix(MENU_MTX_NOPUSH, gDialogOverrideX - 61, 240 - gDialogOverrideY - 5, 0);
    }
    else {
        create_dl_translation_matrix(MENU_MTX_NOPUSH, dialog->leftOffset, dialog->width, 0);
    }

    switch (gDialogBoxType) {
        case DIALOG_TYPE_ROTATE: // Renders a dialog black box with zoom and rotation
            if (gDialogBoxState == DIALOG_STATE_OPENING || gDialogBoxState == DIALOG_STATE_CLOSING) {
                sDialogRotationPos = gDisplayListHead;
                if (gDialogBoxState == DIALOG_STATE_OPENING) {
                    sDialogScale = gDialogBoxScale - 1.5f;
                    sDialogRotation = gDialogBoxOpenTimer - 7.5f;
                } else {
                    sDialogScale = gDialogBoxScale + 2.0f;
                    sDialogRotation = gDialogBoxOpenTimer + 10.0f;
                }
                sDialogScalePrev = gDialogBoxScale;
                sDialogRotationPrev = gDialogBoxOpenTimer;
                create_dl_scale_matrix(MENU_MTX_NOPUSH, 1.0 / sDialogScalePrev, 1.0 / sDialogScalePrev, 1.0f);
                // convert the speed into angle
                create_dl_rotation_matrix(MENU_MTX_NOPUSH, sDialogRotationPrev * 4.0f, 0, 0, 1.0f);
            }
            gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 150);
            break;
        case DIALOG_TYPE_ZOOM: // Renders a dialog white box with zoom
            if (gDialogBoxState == DIALOG_STATE_OPENING || gDialogBoxState == DIALOG_STATE_CLOSING) {
                sDialogZoomPos = gDisplayListHead;
                if (gDialogBoxState == DIALOG_STATE_OPENING) {
                    sDialogScale = gDialogBoxScale - 2;
                } else {
                    sDialogScale = gDialogBoxScale + 2;
                }
                sDialogScalePrev = gDialogBoxScale;
                create_dl_translation_matrix(MENU_MTX_NOPUSH, 65.0 - (65.0 / sDialogScalePrev),
                                              (40.0 / sDialogScalePrev) - 40, 0);
                create_dl_scale_matrix(MENU_MTX_NOPUSH, 1.0 / sDialogScalePrev, 1.0 / sDialogScalePrev, 1.0f);
            }
            gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 150);
            break;
    }
    if (gOverrideDialogColor) {
        gDPSetEnvColor(gDisplayListHead++, gDialogBgColorR, gDialogBgColorG, gDialogBgColorB, gDialogBgColorA);
    }

    f32 dialogWidth = 130 * 1.1f;
    if (dialogWidth < gDialogMinWidth) dialogWidth = gDialogMinWidth;
    create_dl_translation_matrix(MENU_MTX_PUSH, X_VAL1, Y_VAL1, 0);

    // iQue gives the box one extra line of height on top of drawing one line
    // fewer per page (see totalLines below). That pairing is the whole trick:
    // the taller 16px CJK glyphs need the room, and the shorter page keeps the
    // text from filling it. Both halves must follow the same runtime switch as
    // the strings, otherwise the CN text overflows the US-sized box.
    f32 boxLines = (f32) linesPerBox;
    if (CN_PACK_ACTIVE()) {
        boxLines += 1.0f;
    }
    create_dl_scale_matrix(MENU_MTX_NOPUSH, dialogWidth / 130, (boxLines / Y_VAL2) + 0.1, 1.0f);

    gSPDisplayList(gDisplayListHead++, dl_draw_text_bg_box);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

void change_and_flash_dialog_text_color_lines(s8 colorMode, s8 lineNum) {
    u8 colorFade;

    if (colorMode == 1) {
        if (lineNum == 1) {
            gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
        } else {
            if (lineNum == gDialogLineNum) {
                colorFade = (gSineTable[gDialogColorFadeTimer >> 4] * 50.0f) + 200.0f;
                gDPSetEnvColor(gDisplayListHead++, colorFade, colorFade, colorFade, 255);
            } else {
                gDPSetEnvColor(gDisplayListHead++, 200, 200, 200, 255);
            }
        }
    } else {
        switch (gDialogBoxType) {
            case DIALOG_TYPE_ROTATE:
                break;
            case DIALOG_TYPE_ZOOM:
                gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 255);
                break;
        }
    }
    if (gOverrideDialogColor) {
        gDPSetEnvColor(gDisplayListHead++, gDialogTextColorR, gDialogTextColorG, gDialogTextColorB, gDialogTextColorA)
    }
}

#ifdef VERSION_EU
void render_generic_dialog_char_at_pos(struct DialogEntry *dialog, s16 x, s16 y, u8 c) {
    s16 width;
    s16 height;
    s16 tmpX;
    s16 tmpY;
    s16 xCoord;
    s16 yCoord;
    void **fontLUT;
    void *packedTexture;
    void *unpackedTexture;

    width = (8.0 - (gDialogBoxScale * 0.8));
    height = (16.0 - (gDialogBoxScale * 0.8));
    tmpX = (dialog->leftOffset + (65.0 - (65.0 / gDialogBoxScale)));
    tmpY = ((240 - dialog->width) - ((40.0 / gDialogBoxScale) - 40));
    xCoord = (tmpX + (x / gDialogBoxScale));
    yCoord = (tmpY + (y / gDialogBoxScale));

    fontLUT = segmented_to_virtual(main_font_lut);
    packedTexture = segmented_to_virtual(fontLUT[c]);
    unpackedTexture = convert_ia4_char(c, packedTexture, 8, 8);

    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_16b, 1, VIRTUAL_TO_PHYSICAL(unpackedTexture));
    gSPDisplayList(gDisplayListHead++, dl_ia_text_tex_settings);
    gSPTextureRectangleFlip(gDisplayListHead++, xCoord << 2, (yCoord - height) << 2,
                            (xCoord + width) << 2, yCoord << 2, G_TX_RENDERTILE, 8 << 6, 4 << 6, 1 << 10, 1 << 10);
}
#endif

#if defined(VERSION_JP) || defined(VERSION_SH)
#define X_VAL3 5.0f
#define Y_VAL3 20
#else
#define X_VAL3 0.0f
#define Y_VAL3 16
#endif

// iQue spaces dialog lines 20px apart (vs the US 16) so a 16px CJK glyph and
// its shadow do not touch the line below. Y_VAL3 is a compile-time constant, so
// the CN value has to be selected at each use.
#if defined(SM64COOPDX_CN_TEXT)
#define CN_DIALOG_LINE_H() (CN_PACK_ACTIVE() ? 20 : Y_VAL3)
#else
#define CN_DIALOG_LINE_H() (Y_VAL3)
#endif

#ifdef VERSION_EU
void handle_dialog_scroll_page_state(s8 lineNum, s8 totalLines, s8 *pageState, s8 *xMatrix)
#else
void handle_dialog_scroll_page_state(s8 lineNum, s8 totalLines, s8 *pageState, s8 *xMatrix, s16 *linePos)
#endif
{
#ifndef VERSION_EU
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
#endif

    if (lineNum == totalLines) {
        pageState[0] = DIALOG_PAGE_STATE_SCROLL;
        return;
    }
#ifdef VERSION_EU
    gDialogY += 16;
#else
    create_dl_translation_matrix(MENU_MTX_PUSH, X_VAL3, 2 - (lineNum * CN_DIALOG_LINE_H()), 0);

    linePos[0] = 0;
#endif
    xMatrix[0] = 1;
}

#if defined(VERSION_JP) || defined(VERSION_SH)
void adjust_pos_and_print_period_char(s8 *xMatrix, s16 *linePos) {
    if (linePos[0] != 0) {
        create_dl_translation_matrix(MENU_MTX_NOPUSH, xMatrix[0] * 10, 0, 0);
    }

    create_dl_translation_matrix(MENU_MTX_PUSH, -2.0f, -5.0f, 0);
    render_generic_char(DIALOG_CHAR_PERIOD_OR_HANDAKUTEN);

    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    linePos[0]++;
    xMatrix[0] = 1;
}
#endif

#ifdef VERSION_EU
void render_star_count_dialog_text(struct DialogEntry *dialog, s8 *linePos)
#else
void render_star_count_dialog_text(s8 *xMatrix, s16 *linePos)
#endif
{
    s8 tensDigit = gDialogVariable / 10;
    s8 onesDigit = gDialogVariable - (tensDigit * 10); // remainder

    if (tensDigit != 0) {
#if defined(VERSION_JP) || defined(VERSION_SH)
        create_dl_translation_matrix(MENU_MTX_NOPUSH, xMatrix[0] * 10, 0, 0);
        render_generic_char(tensDigit);
#elif defined(VERSION_EU)
        render_generic_dialog_char_at_pos(dialog, gDialogX, gDialogY, tensDigit);
        gDialogX += gDialogCharWidths[tensDigit];
        linePos[0] = 1;
#else
        if (xMatrix[0] != 1) {
            create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[DIALOG_CHAR_SPACE] * xMatrix[0]), 0, 0);
        }

        render_generic_char(tensDigit);
        create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32) gDialogCharWidths[tensDigit], 0, 0);
        xMatrix[0] = 1;
        linePos[0]++;
#endif
    }
#ifndef VERSION_EU
    else {
#if defined(VERSION_JP) || defined(VERSION_SH)
        xMatrix[0]++;
#endif
    }
#endif

#ifdef VERSION_EU
    render_generic_dialog_char_at_pos(dialog, gDialogX, gDialogY, onesDigit);
    gDialogX += gDialogCharWidths[onesDigit];
    linePos[0] = 1;
#else

#if defined(VERSION_JP) || defined(VERSION_SH)
    create_dl_translation_matrix(MENU_MTX_NOPUSH, xMatrix[0] * 10, 0, 0);
    render_generic_char(onesDigit);
#else
    if (xMatrix[0] != 1) {
        create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[DIALOG_CHAR_SPACE] * (xMatrix[0] - 1)), 0, 0);
    }

    render_generic_char(onesDigit);
    create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32) gDialogCharWidths[onesDigit], 0, 0);
#endif

    linePos[0]++;
    xMatrix[0] = 1;
#endif
}

#if !defined(VERSION_JP) && !defined(VERSION_SH)
#ifdef VERSION_EU
void render_multi_text_string_lines(s8 multiTextId, s8 lineNum, s8 linesPerBox, UNUSED s16 linePos, s8 lowerBound, struct DialogEntry *dialog)
#else
void render_multi_text_string_lines(s8 multiTextId, s8 lineNum, s16 *linePos, s8 linesPerBox, s8 xMatrix, s8 lowerBound)
#endif
{
    s8 i;
    struct MultiTextEntry textLengths[2] = {
        { 3, { TEXT_THE_RAW } },
        { 3, { TEXT_YOU_RAW } },
    };

    if (lineNum >= lowerBound && lineNum <= (lowerBound + linesPerBox)) {
#ifdef VERSION_US
        if (linePos[0] != 0 || (xMatrix != 1)) {
            create_dl_translation_matrix(MENU_MTX_NOPUSH, (gDialogCharWidths[DIALOG_CHAR_SPACE] * (xMatrix - 1)), 0, 0);
        }
#endif
        for (i = 0; i < textLengths[multiTextId].length; i++) {
#ifdef VERSION_EU
            render_generic_dialog_char_at_pos(dialog, gDialogX, gDialogY, textLengths[multiTextId].str[i]);
            gDialogX += gDialogCharWidths[textLengths[multiTextId].str[i]];
#else
            render_generic_char(textLengths[multiTextId].str[i]);
            create_dl_translation_matrix(MENU_MTX_NOPUSH, (gDialogCharWidths[textLengths[multiTextId].str[i]]), 0, 0);
#endif
        }
    }
#ifdef VERSION_US
    linePos += textLengths[multiTextId].length;
#endif
}
#endif

#ifdef VERSION_EU
void render_dialog_lowercase_diacritic(struct DialogEntry *dialog, u8 chr, u8 diacritic) {
    render_generic_dialog_char_at_pos(dialog, gDialogX, gDialogY, chr);
    render_generic_dialog_char_at_pos(dialog, gDialogX, gDialogY, diacritic + 0xE7);
    gDialogX += gDialogCharWidths[chr];
}

void render_dialog_uppercase_diacritic(struct DialogEntry *dialog, u8 chr, u8 diacritic) {
    render_generic_dialog_char_at_pos(dialog, gDialogX, gDialogY, chr);
    render_generic_dialog_char_at_pos(dialog, gDialogX, gDialogY - 4, diacritic + 0xE3);
    gDialogX += gDialogCharWidths[chr];
}
#endif

u32 ensure_nonnegative(s16 value) {
    if (value < 0) {
        value = 0;
    }

    return value;
}

#if defined(VERSION_JP)
void handle_dialog_text_and_pages(s8 colorMode, struct DialogEntry *dialog)
#else
void handle_dialog_text_and_pages(s8 colorMode, struct DialogEntry *dialog, s8 lowerBound)
#endif
{
    UNUSED s32 pad[2];
#ifdef VERSION_EU
    s16 startY = 14;
#endif

    u8 strChar;

    u8 *str = sOverrideDialogHookString != NULL ? sOverrideDialogHookString : segmented_to_virtual(dialog->str);
    s8 lineNum = 1;
    // Hoisted: the encoding cannot change mid-dialog, and cn_is_cn_string() is
    // a table scan that would otherwise run once per glyph below.
    bool cn = cn_is_cn_string(str);

    s8 totalLines;

    s8 pageState = DIALOG_PAGE_STATE_NONE;
    UNUSED s8 mark = DIALOG_MARK_NONE; // unused in US, EU
    s8 xMatrix = 1;

    s8 linesPerBox = dialog->linesPerBox;

    s16 strIdx;
#ifndef VERSION_EU
    s16 linePos = 0;
#endif

    if (gDialogBoxState == DIALOG_STATE_HORIZONTAL) {
        // If scrolling, consider the number of lines for both
        // the current page and the page being scrolled to.
        //
        // iQue shows one line fewer than US on every page (and in the
        // two-page scroll case). Together with the taller box and the 20px
        // line spacing this is what makes the CJK text breathe instead of
        // spilling out of the box.
        totalLines = CN_PACK_ACTIVE() ? (linesPerBox * 2 - 1) : (linesPerBox * 2 + 1);
    } else {
        totalLines = CN_PACK_ACTIVE() ? linesPerBox : (linesPerBox + 1);
    }

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    strIdx = gDialogTextPos;
#ifdef VERSION_EU
    gDialogX = 0;
    gDialogY = startY;
#endif

    if (gDialogBoxState == DIALOG_STATE_HORIZONTAL) {
#ifdef VERSION_EU
        gDialogY -= gDialogScrollOffsetY;
#else
        sDialogOffset = gDialogScrollOffsetY + dialog->linesPerBox * 2;
        sDialogOffsetPrev = gDialogScrollOffsetY;
        sDialogOffsetPos = gDisplayListHead;
        create_dl_translation_matrix(MENU_MTX_NOPUSH, 0, (f32) sDialogOffsetPrev, 0);
#endif
    }

#ifndef VERSION_EU
    create_dl_translation_matrix(MENU_MTX_PUSH, X_VAL3, 2 - lineNum * CN_DIALOG_LINE_H(), 0);
#endif

    while (pageState == DIALOG_PAGE_STATE_NONE) {
        change_and_flash_dialog_text_color_lines(colorMode, lineNum);
        strChar = str ? str[strIdx] : DIALOG_CHAR_TERMINATOR;

        // Chinese (iQue/CN) tokens. Every CN token is two bytes and the
        // control tokens all start with 0xFF, which the vanilla switch below
        // would immediately treat as DIALOG_CHAR_TERMINATOR. Handle the whole
        // CN token stream here, before the vanilla single-byte switch, so
        // 0xFF 0xFE (newline) and 0xFF 0x9E (space) behave correctly and only
        // 0xFF 0xFF ends the string.
        {
            CnToken cnTok = cn_read_token(str, strIdx, cn);

            if (cnTok.kind == CN_TOKEN_END) {
                pageState = DIALOG_PAGE_STATE_END;
#ifndef VERSION_EU
                gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
#endif
                break;
            }

            if (cnTok.kind == CN_TOKEN_NEWLINE) {
                lineNum++;
#ifdef VERSION_EU
                handle_dialog_scroll_page_state(lineNum, totalLines, &pageState, &xMatrix);
                gDialogX = 0;
#else
                handle_dialog_scroll_page_state(lineNum, totalLines, &pageState, &xMatrix, &linePos);
#endif
                strIdx += cnTok.length;
                continue;
            }

            if (cnTok.kind == CN_TOKEN_SPACE) {
                // Mirror the vanilla DIALOG_CHAR_SPACE case: batch the space
                // into xMatrix and flush it before the next visible glyph.
#ifdef VERSION_EU
                gDialogX += gDialogCharWidths[DIALOG_CHAR_SPACE];
#else
                xMatrix++;
                linePos++;
#endif
                strIdx += cnTok.length;
                continue;
            }

            if (cnTok.kind == CN_TOKEN_GLYPH) {
                if (lineNum >= lowerBound && lineNum <= lowerBound + linesPerBox) {
                    if (linePos || xMatrix != 1) {
                        create_dl_translation_matrix(
                            MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[DIALOG_CHAR_SPACE] * (xMatrix - 1)), 0, 0);
                    }

                    render_cn_char(cnTok.code);
                    create_dl_translation_matrix(MENU_MTX_NOPUSH, cn_char_width(cnTok.code), 0, 0);
                    xMatrix = 1;
                    linePos++;
                }
                strIdx += cnTok.length;
                continue;
            }
        }

        switch (strChar) {
            case DIALOG_CHAR_TERMINATOR:
                pageState = DIALOG_PAGE_STATE_END;
#ifndef VERSION_EU
                gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
#endif
                break;
            case DIALOG_CHAR_NEWLINE:
                lineNum++;
#ifdef VERSION_EU
                handle_dialog_scroll_page_state(lineNum, totalLines, &pageState, &xMatrix);
                gDialogX = 0;
#else
                handle_dialog_scroll_page_state(lineNum, totalLines, &pageState, &xMatrix, &linePos);
#ifdef VERSION_SH
                mark = 0;
#endif
#endif
                break;
#ifdef VERSION_EU
            case DIALOG_CHAR_LOWER_A_GRAVE:
            case DIALOG_CHAR_LOWER_A_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_A_UMLAUT:
                render_dialog_lowercase_diacritic(dialog, ASCII_TO_DIALOG('a'), strChar & 0xF);
                break;
            case DIALOG_CHAR_UPPER_A_GRAVE:
            case DIALOG_CHAR_UPPER_A_CIRCUMFLEX:
            case DIALOG_CHAR_UPPER_A_UMLAUT:
                render_dialog_uppercase_diacritic(dialog, ASCII_TO_DIALOG('A'), strChar & 0xF);
                break;
            case DIALOG_CHAR_LOWER_E_GRAVE:
            case DIALOG_CHAR_LOWER_E_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_E_UMLAUT:
            case DIALOG_CHAR_LOWER_E_ACUTE:
                render_dialog_lowercase_diacritic(dialog, ASCII_TO_DIALOG('e'), strChar & 0xF);
                break;
            case DIALOG_CHAR_UPPER_E_GRAVE:
            case DIALOG_CHAR_UPPER_E_CIRCUMFLEX:
            case DIALOG_CHAR_UPPER_E_UMLAUT:
            case DIALOG_CHAR_UPPER_E_ACUTE:
                render_dialog_uppercase_diacritic(dialog, ASCII_TO_DIALOG('E'), strChar & 0xF);
                break;
            case DIALOG_CHAR_LOWER_U_GRAVE:
            case DIALOG_CHAR_LOWER_U_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_U_UMLAUT:
                render_dialog_lowercase_diacritic(dialog, ASCII_TO_DIALOG('u'), strChar & 0xF);
                break;
            case DIALOG_CHAR_UPPER_U_GRAVE:
            case DIALOG_CHAR_UPPER_U_CIRCUMFLEX:
            case DIALOG_CHAR_UPPER_U_UMLAUT:
                render_dialog_uppercase_diacritic(dialog, ASCII_TO_DIALOG('U'), strChar & 0xF);
                break;
            case DIALOG_CHAR_LOWER_O_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_O_UMLAUT:
                render_dialog_lowercase_diacritic(dialog, ASCII_TO_DIALOG('o'), strChar & 0xF);
                break;
            case DIALOG_CHAR_UPPER_O_CIRCUMFLEX:
            case DIALOG_CHAR_UPPER_O_UMLAUT:
                render_dialog_uppercase_diacritic(dialog, ASCII_TO_DIALOG('O'), strChar & 0xF);
                break;
            case DIALOG_CHAR_LOWER_I_CIRCUMFLEX:
            case DIALOG_CHAR_LOWER_I_UMLAUT:
                render_dialog_lowercase_diacritic(dialog, DIALOG_CHAR_I_NO_DIA, strChar & 0xF);
                break;
#else
            case DIALOG_CHAR_DAKUTEN:
                mark = DIALOG_MARK_DAKUTEN;
                break;
            case DIALOG_CHAR_PERIOD_OR_HANDAKUTEN:
                mark = DIALOG_MARK_HANDAKUTEN;
                break;
#endif
            case DIALOG_CHAR_SPACE:
#ifdef VERSION_EU
                gDialogX += gDialogCharWidths[DIALOG_CHAR_SPACE];
#else
#if defined(VERSION_JP) || defined(VERSION_SH)
                if (linePos != 0) {
#endif
                    xMatrix++;
#if defined(VERSION_JP) || defined(VERSION_SH)
                }
#endif
                linePos++;

#endif
                break;
#if defined(VERSION_JP) || defined(VERSION_SH)
            case DIALOG_CHAR_PERIOD:
                adjust_pos_and_print_period_char(&xMatrix, &linePos);
                break;
#else
            case DIALOG_CHAR_SLASH:
#ifdef VERSION_EU
                gDialogX += gDialogCharWidths[DIALOG_CHAR_SPACE] * 2;
#else
                xMatrix += 2;
                linePos += 2;
#endif
                break;
            case DIALOG_CHAR_MULTI_THE:
#ifdef VERSION_EU
                render_multi_text_string_lines(STRING_THE, lineNum, linesPerBox, xMatrix, lowerBound, dialog);
#else
                render_multi_text_string_lines(STRING_THE, lineNum, &linePos, linesPerBox, xMatrix, lowerBound);
#endif
                xMatrix = 1;
                break;
            case DIALOG_CHAR_MULTI_YOU:
#ifdef VERSION_EU
                render_multi_text_string_lines(STRING_YOU, lineNum, linesPerBox, xMatrix, lowerBound, dialog);
#else
                render_multi_text_string_lines(STRING_YOU, lineNum, &linePos, linesPerBox, xMatrix, lowerBound);
#endif
                xMatrix = 1;
                break;
#endif
            case DIALOG_CHAR_STAR_COUNT:
#ifdef VERSION_EU
                render_star_count_dialog_text(dialog, &xMatrix);
#else
                render_star_count_dialog_text(&xMatrix, &linePos);
#endif
                break;
#ifdef VERSION_EU
            case DIALOG_CHAR_DOUBLE_LOW_QUOTE:
                render_generic_dialog_char_at_pos(dialog, gDialogX, gDialogY + 8, 0xF6);
                gDialogX += gDialogCharWidths[0xF6];
                break;
#endif
            default: // any other character
#if defined(VERSION_JP) || defined(VERSION_SH)
#ifdef VERSION_SH
                if (lineNum >= lowerBound && lineNum <= lowerBound + linesPerBox) {
#endif
                    if (linePos != 0) {
                        create_dl_translation_matrix(MENU_MTX_NOPUSH, xMatrix * 10, 0, 0);
                    }

                    render_generic_char(strChar);
                    xMatrix = 1;
                    linePos++;

                    if (mark != 0) {
                        create_dl_translation_matrix(MENU_MTX_PUSH, 5.0f, 7.0f, 0);
                        render_generic_char(mark + 0xEF);
                        gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
                        mark = 0;
                    }
#ifdef VERSION_SH
                }
#endif
#elif defined(VERSION_US)
                if (lineNum >= lowerBound && lineNum <= lowerBound + linesPerBox) {
                    if (linePos || xMatrix != 1) {
                        create_dl_translation_matrix(
                            MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[DIALOG_CHAR_SPACE] * (xMatrix - 1)), 0, 0);
                    }

                    render_generic_char(strChar);
                    create_dl_translation_matrix(MENU_MTX_NOPUSH, (f32)(gDialogCharWidths[strChar]), 0, 0);
                    xMatrix = 1;
                    linePos++;
                }
#else // VERSION_EU
                if (lineNum >= lowerBound && lineNum <= lowerBound + linesPerBox) {
                    render_generic_dialog_char_at_pos(dialog, gDialogX, gDialogY, strChar);
                }
                gDialogX += gDialogCharWidths[strChar];
#endif
        }

#if defined(VERSION_JP)
        if (linePos == 12) {
            if (str[strIdx + 1] == DIALOG_CHAR_PERIOD) {
                adjust_pos_and_print_period_char(&xMatrix, &linePos);
                strIdx++;
            }

            if (str[strIdx + 1] == DIALOG_CHAR_COMMA) {
                create_dl_translation_matrix(MENU_MTX_NOPUSH, xMatrix * 10, 0, 0);
                render_generic_char(DIALOG_CHAR_COMMA);
                strIdx++;
            }

            if (str[strIdx + 1] == DIALOG_CHAR_NEWLINE) {
                strIdx++;
            }

            if (str[strIdx + 1] == DIALOG_CHAR_TERMINATOR) {
                pageState = DIALOG_PAGE_STATE_END;
                gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
                break; // exit loop
            } else {
                lineNum++;
                handle_dialog_scroll_page_state(lineNum, totalLines, &pageState, &xMatrix, &linePos);
            }
        }
#endif

        strIdx++;
    }
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);

    if (gDialogBoxState == DIALOG_STATE_VERTICAL) {
        if (pageState == DIALOG_PAGE_STATE_END) {
            gLastDialogPageStrPos = -1;
        } else {
            gLastDialogPageStrPos = strIdx;
        }
    }

    gLastDialogLineNum = lineNum;
}

#if defined(VERSION_JP) || defined(VERSION_SH)
#define X_VAL4_1 50
#define X_VAL4_2 25
#define Y_VAL4_1 1
#define Y_VAL4_2 20
#else
#define X_VAL4_1 56
#define X_VAL4_2 47
#define Y_VAL4_1 2
#define Y_VAL4_2 16
#endif

void render_dialog_triangle_choice(void) {
    if (gDialogBoxState == DIALOG_STATE_VERTICAL) {
        handle_menu_scrolling(MENU_SCROLL_HORIZONTAL, &gDialogLineNum, 1, 2);
    }

    // Choice marker geometry. iQue's CN build moves the marker a 20px line
    // lower (Y_VAL4_2 20 vs 16) and its X term is ((n - 1) * 58) + 0; this
    // function's folded `n * X - C` form expresses that as 58 / 58.
    s32 xVal4_1 = X_VAL4_1;
    s32 xVal4_2 = X_VAL4_2;
    s32 yVal4_2 = Y_VAL4_2;
#if defined(SM64COOPDX_CN_TEXT)
    if (CN_PACK_ACTIVE()) {
        xVal4_1 = 58;
        xVal4_2 = 58;
        yVal4_2 = 20;
    }
#endif

    create_dl_translation_matrix(MENU_MTX_NOPUSH, (gDialogLineNum * xVal4_1) - xVal4_2, Y_VAL4_1 - (gLastDialogLineNum * yVal4_2), 0);

    if (gDialogBoxType == DIALOG_TYPE_ROTATE) {
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    } else {
        gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 255);
    }
    if (gOverrideDialogColor) {
        gDPSetEnvColor(gDisplayListHead++, gDialogTextColorR, gDialogTextColorG, gDialogTextColorB, gDialogTextColorA);
    }

    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
}

#ifdef VERSION_EU
#define X_VAL5 122.0f
#define Y_VAL5_1 -16
#define Y_VAL5_2 3
#define X_Y_VAL6 0.5f
#elif defined(VERSION_US)
#define X_VAL5 118.0f
#define Y_VAL5_1 -16
#define Y_VAL5_2 5
#define X_Y_VAL6 0.8f
#elif defined(VERSION_JP) || defined(VERSION_SH)
#define X_VAL5 123.0f
#define Y_VAL5_1 -20
#define Y_VAL5_2 2
#define X_Y_VAL6 0.8f
#endif

void render_dialog_string_color(s8 linesPerBox) {
    s32 timer = gGlobalTimer;

    if (timer & 0x08) {
        return;
    }

    f32 triangleOffset = gDialogMinWidth - 143;
    if (triangleOffset < 0) triangleOffset = 0;

    // iQue's "next page" triangle tracks the 20px CN line pitch (-18 vs -16)
    // so it still sits at the bottom edge of the (one line taller) CN box.
    s32 yVal5_1 = Y_VAL5_1;
#if defined(SM64COOPDX_CN_TEXT)
    if (CN_PACK_ACTIVE()) {
        yVal5_1 = -18;
    }
#endif

    create_dl_translation_matrix(MENU_MTX_PUSH, X_VAL5 + triangleOffset, (linesPerBox * yVal5_1) + Y_VAL5_2, 0);
    create_dl_scale_matrix(MENU_MTX_NOPUSH, X_Y_VAL6, X_Y_VAL6, 1.0f);
    create_dl_rotation_matrix(MENU_MTX_NOPUSH, -DEFAULT_DIALOG_BOX_ANGLE, 0, 0, 1.0f);

    if (gDialogBoxType == DIALOG_TYPE_ROTATE) { // White Text
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    } else { // Black Text
        gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 255);
    }
    if (gOverrideDialogColor) {
        gDPSetEnvColor(gDisplayListHead++, gDialogTextColorR, gDialogTextColorG, gDialogTextColorB, gDialogTextColorA);
    }

    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

s16 gMenuMode = -1;
s16 gPrevMenuMode = -1;

u8 *gEndCutsceneStringsEn[] = {
    INGAME_TEXT_PTR(TEXT_FILE_MARIO_EXCLAMATION),
    INGAME_TEXT_PTR(TEXT_POWER_STARS_RESTORED),
    INGAME_TEXT_PTR(TEXT_THANKS_TO_YOU),
    INGAME_TEXT_PTR(TEXT_THANK_YOU_MARIO),
    INGAME_TEXT_PTR(TEXT_SOMETHING_SPECIAL),
    INGAME_TEXT_PTR(TEXT_LISTEN_EVERYBODY),
    INGAME_TEXT_PTR(TEXT_LETS_HAVE_CAKE),
    INGAME_TEXT_PTR(TEXT_FOR_MARIO),
    // This [8] string is actually unused. In the cutscene handler, the developers do not
    // set the 8th one, but use the first string again at the very end, so Peach ends up
    // saying "Mario!" twice. It is likely that she was originally meant to say "Mario?" at
    // the end but the developers changed their mind, possibly because the line recorded
    // sounded more like an exclamation than a question.
    INGAME_TEXT_PTR(TEXT_FILE_MARIO_QUESTION),
    NULL
};

#ifdef VERSION_EU
u8 gEndCutsceneStrFr0[] = { TEXT_FILE_MARIO_EXCLAMATION };
u8 gEndCutsceneStrFr1[] = { TEXT_POWER_STARS_RESTORED_FR };
u8 gEndCutsceneStrFr2[] = { TEXT_THANKS_TO_YOU_FR };
u8 gEndCutsceneStrFr3[] = { TEXT_THANK_YOU_MARIO_FR };
u8 gEndCutsceneStrFr4[] = { TEXT_SOMETHING_SPECIAL_FR };
u8 gEndCutsceneStrFr5[] = { TEXT_COME_ON_EVERYBODY_FR };
u8 gEndCutsceneStrFr6[] = { TEXT_LETS_HAVE_CAKE_FR };
u8 gEndCutsceneStrFr7[] = { TEXT_FOR_MARIO_FR };
u8 gEndCutsceneStrFr8[] = { TEXT_FILE_MARIO_QUESTION };

u8 *gEndCutsceneStringsFr[] = {
    gEndCutsceneStrFr0,
    gEndCutsceneStrFr1,
    gEndCutsceneStrFr2,
    gEndCutsceneStrFr3,
    gEndCutsceneStrFr4,
    gEndCutsceneStrFr5,
    gEndCutsceneStrFr6,
    gEndCutsceneStrFr7,
    gEndCutsceneStrFr8,
    NULL
};

u8 gEndCutsceneStrDe0[] = { TEXT_FILE_MARIO_EXCLAMATION };
u8 gEndCutsceneStrDe1[] = { TEXT_POWER_STARS_RESTORED_DE };
u8 gEndCutsceneStrDe2[] = { TEXT_THANKS_TO_YOU_DE };
u8 gEndCutsceneStrDe3[] = { TEXT_THANK_YOU_MARIO_DE };
u8 gEndCutsceneStrDe4[] = { TEXT_SOMETHING_SPECIAL_DE };
u8 gEndCutsceneStrDe5[] = { TEXT_COME_ON_EVERYBODY_DE };
u8 gEndCutsceneStrDe6[] = { TEXT_LETS_HAVE_CAKE_DE };
u8 gEndCutsceneStrDe7[] = { TEXT_FOR_MARIO_DE };
u8 gEndCutsceneStrDe8[] = { TEXT_FILE_MARIO_QUESTION };

u8 *gEndCutsceneStringsDe[] = {
    gEndCutsceneStrDe0,
    gEndCutsceneStrDe1,
    gEndCutsceneStrDe2,
    gEndCutsceneStrDe3,
    gEndCutsceneStrDe4,
    gEndCutsceneStrDe5,
    gEndCutsceneStrDe6,
    gEndCutsceneStrDe7,
    gEndCutsceneStrDe8,
    NULL
};
#endif

u16 gCutsceneMsgFade = 0;
s16 gCutsceneMsgIndex = -1;
s16 gCutsceneMsgDuration = -1;
s16 gCutsceneMsgTimer = 0;
s8 gDialogCameraAngleIndex = CAM_SELECTION_MARIO;
s8 gDialogCourseActNum = 1;

#if defined(VERSION_JP) || defined(VERSION_SH)
#define DIAG_VAL1 20
#define DIAG_VAL3 130
#define DIAG_VAL4 4
#else
#define DIAG_VAL1 16
#define DIAG_VAL3 132 // US & EU
#define DIAG_VAL4 5
#endif
#ifdef VERSION_EU
#define DIAG_VAL2 238
#else
#define DIAG_VAL2 240 // JP & US
#endif

void render_dialog_entries(void) {
#ifdef VERSION_EU
    s8 lowerBound = 0;
#endif

#if defined(VERSION_US) || defined(VERSION_SH)
    s8 lowerBound = 0;
#endif
    struct DialogEntry *dialog = dialog_table_get(gDialogID);

    if (dialog == NULL) {
        gDialogID = DIALOG_NONE;
        return;
    }

#ifdef VERSION_EU
    gDialogX = 0;
    gDialogY = 0;
#endif

    switch (gDialogBoxState) {
        case DIALOG_STATE_OPENING:
            if (gDialogBoxOpenTimer == DEFAULT_DIALOG_BOX_ANGLE) {
                play_dialog_sound(gDialogID);
                play_sound(SOUND_MENU_MESSAGE_APPEAR, gGlobalSoundSource);
            }

            if (gDialogBoxType == DIALOG_TYPE_ROTATE) {
                gDialogBoxOpenTimer -= 7.5;
                gDialogBoxScale -= 1.5;
            } else {
                gDialogBoxOpenTimer -= 10.0;
                gDialogBoxScale -= 2.0;
            }

            if (gDialogBoxOpenTimer == 0.0f) {
                gDialogBoxState = DIALOG_STATE_VERTICAL;
                gDialogLineNum = 1;
            }
#if !defined(VERSION_JP)
            lowerBound = 1;
#endif
            break;
        case DIALOG_STATE_VERTICAL:
            gDialogBoxOpenTimer = 0.0f;

            if ((gPlayer1Controller->buttonPressed & A_BUTTON)
                || (gPlayer1Controller->buttonPressed & B_BUTTON)) {
                if (gLastDialogPageStrPos == -1) {
                    handle_special_dialog_text(gDialogID);
                    gDialogBoxState = DIALOG_STATE_CLOSING;
                } else {
                    gDialogBoxState = DIALOG_STATE_HORIZONTAL;
                    play_sound(SOUND_MENU_MESSAGE_NEXT_PAGE, gGlobalSoundSource);
                }
            }
#if !defined(VERSION_JP)
            lowerBound = 1;
#endif
            break;
        case DIALOG_STATE_HORIZONTAL:
            gDialogScrollOffsetY += dialog->linesPerBox * 2;

            if (gDialogScrollOffsetY >= dialog->linesPerBox * DIAG_VAL1) {
                gDialogTextPos = gLastDialogPageStrPos;
                gDialogBoxState = DIALOG_STATE_VERTICAL;
                gDialogScrollOffsetY = 0;
            }
#if !defined(VERSION_JP)
            lowerBound = (gDialogScrollOffsetY / DIAG_VAL1) + 1;
#endif
            break;
        case DIALOG_STATE_CLOSING:
            if (gDialogBoxOpenTimer == 20.0f) {
                level_set_transition(0, NULL);
                play_sound(SOUND_MENU_MESSAGE_DISAPPEAR, gGlobalSoundSource);

                if (gDialogBoxType == DIALOG_TYPE_ZOOM) {
                    trigger_cutscene_dialog(2);
                }

                gDialogResponse = gDialogLineNum;
            }

            gDialogBoxOpenTimer = gDialogBoxOpenTimer + 10.0f;
            gDialogBoxScale = gDialogBoxScale + 2.0f;

            if (gDialogBoxOpenTimer == DEFAULT_DIALOG_BOX_ANGLE) {
                gDialogBoxState = DIALOG_STATE_OPENING;
                gDialogID = DIALOG_NONE;
                gDialogTextPos = 0;
                gLastDialogResponse = 0;
                gLastDialogPageStrPos = 0;
                gDialogResponse = 0;
            }
#if !defined(VERSION_JP)
            lowerBound = 1;
#endif
            break;
    }

    render_dialog_box_type(dialog, dialog->linesPerBox);

#ifdef VERSION_EU
    u32 scissorHeight = ensure_nonnegative((240 - dialog->width) + ((dialog->linesPerBox * 80) / DIAG_VAL4) / gDialogBoxScale);
#else
    u32 scissorHeight = ensure_nonnegative(240 + ((dialog->linesPerBox * 80) / DIAG_VAL4) - dialog->width);
#endif
    u32 scissorY = ensure_nonnegative(DIAG_VAL2 - dialog->width);
    if (gOverrideDialogPos) {
        scissorHeight = ensure_nonnegative(((dialog->linesPerBox * 80) / DIAG_VAL4) + gDialogOverrideY + 5);
        scissorY = ensure_nonnegative(gDialogOverrideY + 5);
    }

    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                  0,
                  scissorY,
                  SCREEN_WIDTH,
                  scissorHeight);
#if defined(VERSION_JP)
    handle_dialog_text_and_pages(0, dialog);
#else
    handle_dialog_text_and_pages(0, dialog, lowerBound);
#endif

    if (gLastDialogPageStrPos == -1 && gLastDialogResponse == 1) {
        render_dialog_triangle_choice();
    }
#ifdef VERSION_EU
#undef BORDER_HEIGHT
#define BORDER_HEIGHT 8
#endif
    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 2, 2, SCREEN_WIDTH - BORDER_HEIGHT/2, SCREEN_HEIGHT - BORDER_HEIGHT/2);
#ifdef VERSION_EU
#undef BORDER_HEIGHT
#define BORDER_HEIGHT 1
#endif
    if (gLastDialogPageStrPos != -1 && gDialogBoxState == DIALOG_STATE_VERTICAL) {
        render_dialog_string_color(dialog->linesPerBox);
    }
}

// Calls a gMenuMode value defined by render_menus_and_dialogs cases
void set_menu_mode(s16 mode) {
    if (gMenuMode == -1 || mode == -1) {
        gMenuMode = mode;
    }
}

void reset_cutscene_msg_fade(void) {
    gCutsceneMsgFade = 0;
}

void dl_rgba16_begin_cutscene_msg_fade(void) {
    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gCutsceneMsgFade);
}

void dl_rgba16_stop_cutscene_msg_fade(void) {
    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);

    if (gCutsceneMsgFade < 250) {
        gCutsceneMsgFade += 25;
    } else {
        gCutsceneMsgFade = 255;
    }
}

u8 ascii_to_credits_char(u8 c) {
    if (c >= 'A' && c <= 'Z') {
        return (c - ('A' - 0xA));
    }

    if (c >= 'a' && c <= 'z') { // remap lower to upper case
        return (c - ('a' - 0xA));
    }

    if (c == ' ') {
        return GLOBAL_CHAR_SPACE;
    }
    if (c == '.') {
        return 0x24;
    }
    if (c == '3') {
        return ASCII_TO_DIALOG('3');
    }
    if (c == '4') {
        return ASCII_TO_DIALOG('4');
    }
    if (c == '6') {
        return ASCII_TO_DIALOG('6');
    }

    return GLOBAL_CHAR_SPACE;
}

void print_credits_str_ascii(s16 x, s16 y, const char *str) {
    s32 pos = 0;
    u8 c = str[pos];
    u8 creditStr[100];

    while (c != 0) {
        creditStr[pos++] = ascii_to_credits_char(c);
        c = str[pos];
    }

    creditStr[pos] = GLOBAR_CHAR_TERMINATOR;

    print_credits_string(x, y, creditStr);
}

void set_cutscene_message(s16 xOffset, s16 yOffset, s16 msgIndex, s16 msgDuration) {
    // is message done printing?
    if (gCutsceneMsgIndex == -1) {
        gCutsceneMsgIndex = msgIndex;
        gCutsceneMsgDuration = msgDuration;
        gCutsceneMsgTimer = 0;
        gCutsceneMsgXOffset = xOffset;
        gCutsceneMsgYOffset = yOffset;
        gCutsceneMsgFade = 0;
    }
}

void do_cutscene_handler(void) {
    s16 x;

    // is a cutscene playing? do not perform this handler's actions if so.
    if (gCutsceneMsgIndex == -1) {
        return;
    }

    create_dl_ortho_matrix();

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gCutsceneMsgFade);

#ifdef VERSION_EU
    switch (eu_get_language()) {
        case LANGUAGE_ENGLISH:
            x = get_str_x_pos_from_center(gCutsceneMsgXOffset, gEndCutsceneStringsEn[gCutsceneMsgIndex], 10.0f);
            print_generic_string(x, 240 - gCutsceneMsgYOffset, gEndCutsceneStringsEn[gCutsceneMsgIndex]);
            break;
        case LANGUAGE_FRENCH:
            x = get_str_x_pos_from_center(gCutsceneMsgXOffset, gEndCutsceneStringsFr[gCutsceneMsgIndex], 10.0f);
            print_generic_string(x, 240 - gCutsceneMsgYOffset, gEndCutsceneStringsFr[gCutsceneMsgIndex]);
            break;
        case LANGUAGE_GERMAN:
            x = get_str_x_pos_from_center(gCutsceneMsgXOffset, gEndCutsceneStringsDe[gCutsceneMsgIndex], 10.0f);
            print_generic_string(x, 240 - gCutsceneMsgYOffset, gEndCutsceneStringsDe[gCutsceneMsgIndex]);
            break;
    }
#else
    // get the x coordinate of where the cutscene string starts.
    x = get_str_x_pos_from_center(gCutsceneMsgXOffset, gEndCutsceneStringsEn[gCutsceneMsgIndex], 10.0f);
    print_generic_string(x, 240 - gCutsceneMsgYOffset, gEndCutsceneStringsEn[gCutsceneMsgIndex]);
#endif

    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);

    // if the timing variable is less than 5, increment
    // the fade until we are at full opacity.
    if (gCutsceneMsgTimer < 5) {
        gCutsceneMsgFade += 50;
    }

    // if the cutscene frame length + the fade-in counter is
    // less than the timer, it means we have exceeded the
    // time that the message is supposed to remain on
    // screen. if (message_duration = 50) and (msg_timer = 55)
    // then after the first 5 frames, the message will remain
    // on screen for another 50 frames until it starts fading.
    if (gCutsceneMsgDuration + 5 < gCutsceneMsgTimer) {
        gCutsceneMsgFade -= 50;
    }

    // like the first check, it takes 5 frames to fade out, so
    // perform a + 10 to account for the earlier check (10-5=5).
    if (gCutsceneMsgDuration + 10 < gCutsceneMsgTimer) {
        gCutsceneMsgIndex = -1;
        gCutsceneMsgFade = 0;
        gCutsceneMsgTimer = 0;
        return;
    }

    gCutsceneMsgTimer++;
}

#ifdef VERSION_JP
#define PEACH_MESSAGE_TIMER 170
#else
#define PEACH_MESSAGE_TIMER 250
#endif

#if defined(VERSION_JP) || defined(VERSION_SH)
#define STR_X 53
#define STR_Y 136
#else
#define STR_X 38
#define STR_Y 142
#endif

// "Dear Mario" message handler
void print_peach_letter_message(void) {
    struct DialogEntry *dialog = dialog_table_get(gDialogID);

    const u8* str = sOverrideDialogHookString != NULL ? sOverrideDialogHookString : dialog->str;

    create_dl_translation_matrix(MENU_MTX_PUSH, 97.0f, 118.0f, 0);

    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gCutsceneMsgFade);
    gSPDisplayList(gDisplayListHead++, castle_grounds_seg7_dl_0700EA58);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 20, 20, 20, gCutsceneMsgFade);

    print_generic_string(STR_X, STR_Y, str);
#if defined(VERSION_JP)
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
#endif
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
#ifndef VERSION_JP
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
    gDPSetEnvColor(gDisplayListHead++, 200, 80, 120, gCutsceneMsgFade);
    gSPDisplayList(gDisplayListHead++, castle_grounds_seg7_us_dl_0700F2E8);
#endif

    // at the start/end of message, reset the fade.
    if (gCutsceneMsgTimer == 0) {
        gCutsceneMsgFade = 0;
    }

    // we're less than 20 increments, so increase the fade.
    if (gCutsceneMsgTimer < 20) {
        gCutsceneMsgFade += 10;
    }

    // we're after PEACH_MESSAGE_TIMER increments, so decrease the fade.
    if (gCutsceneMsgTimer > PEACH_MESSAGE_TIMER) {
        gCutsceneMsgFade -= 10;
    }

    // 20 increments after the start of the decrease, we're
    // back where we are, so reset everything at the end.
    if (gCutsceneMsgTimer > (PEACH_MESSAGE_TIMER + 20)) {
        gCutsceneMsgIndex = -1;
        gCutsceneMsgFade = 0; //! uselessly reset since the next execution will just set it to 0 again.
        gDialogID = DIALOG_NONE;
        gCutsceneMsgTimer = 0;
        return; // return to avoid incrementing the timer
    }

    gCutsceneMsgTimer++;
}

/**
 * Renders the cannon reticle when Mario is inside a cannon.
 * Formed by four triangles.
 */
void render_hud_cannon_reticle(void) {
    create_dl_translation_matrix(MENU_MTX_PUSH, 160.0f, 120.0f, 0);

    gDPSetEnvColor(gDisplayListHead++, 50, 50, 50, 180);
    create_dl_translation_matrix(MENU_MTX_PUSH, -20.0f, -8.0f, 0);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    create_dl_translation_matrix(MENU_MTX_PUSH, 20.0f, 8.0f, 0);
    create_dl_rotation_matrix(MENU_MTX_NOPUSH, 180.0f, 0, 0, 1.0f);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    create_dl_translation_matrix(MENU_MTX_PUSH, 8.0f, -20.0f, 0);
    create_dl_rotation_matrix(MENU_MTX_NOPUSH, DEFAULT_DIALOG_BOX_ANGLE, 0, 0, 1.0f);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    create_dl_translation_matrix(MENU_MTX_PUSH, -8.0f, 20.0f, 0);
    create_dl_rotation_matrix(MENU_MTX_NOPUSH, -DEFAULT_DIALOG_BOX_ANGLE, 0, 0, 1.0f);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

void reset_red_coins_collected(void) {
}

void change_dialog_camera_angle(void) {
    if (cam_select_alt_mode(0) == CAM_SELECTION_MARIO) {
        gDialogCameraAngleIndex = CAM_SELECTION_MARIO;
    } else {
        gDialogCameraAngleIndex = CAM_SELECTION_FIXED;
    }
}

void shade_screen(void) {
    create_dl_translation_matrix(MENU_MTX_PUSH, GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(0), 240.0f, 0);

    // This is a bit weird. It reuses the dialog text box (width 130, height -80),
    // so scale to at least fit the screen.

    create_dl_scale_matrix(MENU_MTX_NOPUSH,
                           GFX_DIMENSIONS_ASPECT_RATIO * SCREEN_HEIGHT / 125.0f, 3.0f, 1.0f);

    gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 110);
    gSPDisplayList(gDisplayListHead++, dl_draw_text_bg_box);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

void print_animated_red_coin(s16 x, s16 y) {
    s32 timer = gGlobalTimer;

    create_dl_translation_matrix(MENU_MTX_PUSH, x, y, 0);
    create_dl_scale_matrix(MENU_MTX_NOPUSH, 0.2f, 0.2f, 1.0f);
    gDPSetRenderMode(gDisplayListHead++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);

    switch (timer & 6) {
        case 0:
            gSPDisplayList(gDisplayListHead++, coin_seg3_dl_03007940);
            break;
        case 2:
            gSPDisplayList(gDisplayListHead++, coin_seg3_dl_03007968);
            break;
        case 4:
            gSPDisplayList(gDisplayListHead++, coin_seg3_dl_03007990);
            break;
        case 6:
            gSPDisplayList(gDisplayListHead++, coin_seg3_dl_030079B8);
            break;
    }

    gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

static inline void red_coins_print_glyph(s16 *x, u8 glyph, u8 width) {
    u8 text[] = { glyph, GLYPH_SPACE };
    print_hud_lut_string(HUD_LUT_GLOBAL, *x, SCREEN_HEIGHT - 35, text);
    *x += width;
}

void render_pause_red_coins(void) {
    if (gCurrentArea->numRedCoins == 8) {
        u8 collected = gCurrentArea->numRedCoins - count_objects_with_behavior(bhvRedCoin);
        for (s32 x = 0; x < collected; x++) {
            print_animated_red_coin(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(30) - x * 20, 16);
        }
        return;
    }

    if (gCurrentArea->numRedCoins > 0) {
        u8 collected = gCurrentArea->numRedCoins - count_objects_with_behavior(bhvRedCoin);
        s16 x = GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(38);
        print_animated_red_coin(x - 8, 20);
        gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
        red_coins_print_glyph(&x, GLYPH_MULTIPLY, 16);
        if (collected >= 100) red_coins_print_glyph(&x, (collected / 100) % 10, 12);
        if (collected >= 10) red_coins_print_glyph(&x, (collected / 10) % 10, 12);
        red_coins_print_glyph(&x, collected % 10, 15);
        red_coins_print_glyph(&x, GLYPH_SLASH, 15);
        if (gCurrentArea->numRedCoins >= 100) red_coins_print_glyph(&x, (gCurrentArea->numRedCoins / 100) % 10, 12);
        if (gCurrentArea->numRedCoins >= 10) red_coins_print_glyph(&x, (gCurrentArea->numRedCoins / 10) % 10, 12);
        red_coins_print_glyph(&x, gCurrentArea->numRedCoins % 10, 15);
        gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);
    }
}

#ifdef VERSION_EU
u8 gTextCourseArr[][7] = {
    { TEXT_COURSE },
    { TEXT_COURSE_FR },
    { TEXT_COURSE_DE }
};
#endif

#if defined(VERSION_JP) || defined(VERSION_SH)
#define CRS_NUM_X1 93
#else
#define CRS_NUM_X1 100
#endif
// Column X positions for the pause menu header block.
//
// These are the vanilla US numbers, and that is deliberate: the official iQue
// build (n64decomp-cn, branch "ique hotfix") uses exactly the same values for
// its CN layout -- TXT_COURSE_X 63, CRS_NUM_X1 100, LVL_NAME_X 117,
// TXT_STAR_X 98 -- because "关卡" is only two 16px glyphs (32px) where
// "COURSE" is 6 narrow ones, so the wider label fits in the same slot and every
// column to the right of it lands in the same place. Deriving the number's
// position from the measured label width (an earlier revision did) broke the
// "COURSE x" spacing on the *vanilla* side, because get_level_name_sm64() and
// the ROM table disagree about which prefix to skip when the pack is off.
//
// The one column that genuinely differs is MYSCORE: "MY SCORE" is 8 narrow
// glyphs (~48px) but "我的积分" is 4 full-width ones (64px), which at x=62 ends
// at 126 and collides with the star counter at x=118. The iQue build moves it
// to 42 for exactly that reason, so MYSCORE_X is chosen at run time below.
#ifdef VERSION_EU
#define TXT_STAR_X 89
#define ACT_NAME_X 107
#define LVL_NAME_X 108
#define MYSCORE_X  48
#define TXT_COURSE_X 30
#else
#define TXT_STAR_X 98
#define ACT_NAME_X 116
#define LVL_NAME_X 117
#define MYSCORE_X  62
#define TXT_COURSE_X 63
#endif

// "我的积分" is 64px wide against "MY SCORE"'s ~48px, so the vanilla x=62 runs
// the label into the star counter at x=118. The iQue build uses 42.
#if defined(SM64COOPDX_CN_TEXT) && !defined(VERSION_EU)
#define MYSCORE_X_CN 42
#endif

void render_pause_my_score_coins(void) {
    // sanity check
    if (!COURSE_IS_VALID_COURSE(gCurrCourseNum)) { return; }

#ifdef VERSION_EU
    u8 textMyScore[][10] = {
        { TEXT_MY_SCORE },
        { TEXT_MY_SCORE_FR },
        { TEXT_MY_SCORE_DE }
    };
#define textMyScore textMyScore[gInGameLanguage]
#else
    INGAME_TEXT_COPY(textCourse, TEXT_COURSE, CN_LABEL_COURSE);
    INGAME_TEXT_COPY(textMyScore, TEXT_MY_SCORE, CN_LABEL_MY_SCORE);
#endif
    INGAME_TEXT_COPY(textStar, TEXT_STAR, CN_LABEL_STAR);
    INGAME_TEXT_COPY(textUnfilledStar, TEXT_UNFILLED_STAR, CN_LABEL_UNFILLED_STAR);

    u8 strCourseNum[4];
    u8 courseIndex = gCurrCourseNum - 1;
    u8 starIndex = clamp(gDialogCourseActNum, 1, MAX_ACTS);
    u8 *courseNameRaw = (u8*) get_level_name_sm64(gCurrCourseNum, gCurrLevelNum, gCurrAreaIndex, 1);
    // Every course-name buffer carries a leading decoration that must not be
    // drawn: the vanilla table has a 3-byte [colour][num][0x9E] prefix (hence
    // the historic `+ 3`), and the CN table adds U+3000 + full-width number +
    // U+3000 on top of that. This line already prints the course number, so
    // both would show it twice.
    //
    // The two sources have to be chosen by the pack switch, not merged. The CN
    // table only holds Chinese while the pack is on -- with it off,
    // apply_cn_text_pack() has re-pointed seg2_course_name_table back at the
    // ROM buffers -- so running cn_strip_course_name_decoration() on it
    // unconditionally handed the *vanilla* buffer to the renderer without the
    // `+ 3`, and the name was drawn with its " 1 " prefix still attached, well
    // clear of "COURSE x". Same reason actName below branches.
    bool cnPack = CN_PACK_ACTIVE();
    u8 *courseName = courseNameRaw + 3;
    if (cnPack) {
        // Main-course names come from the CN-aware table so the pause header
        // shows Chinese too. Non-main courses keep the vanilla helper, which
        // still has to supply the synthetic castle/ending names.
        const u8 *cnName = (gCurrCourseNum >= COURSE_MIN && gCurrCourseNum <= COURSE_STAGES_MAX)
            ? ingame_menu_get_course_name(gCurrCourseNum) : NULL;
        if (cnName != NULL) { courseName = (u8 *)cn_strip_course_name_decoration(cnName); }
    }
    u8 *actName = (u8*) get_star_name_sm64(gCurrCourseNum, starIndex, 1);
    if (cnPack) {
        // Same rationale as the course name right above: the vanilla helper
        // round-trips through the single-byte font and would turn a CN act
        // name into spaces. NULL for slots the CN table does not cover.
        const u8 *cnAct = ingame_menu_get_act_name(gCurrCourseNum, starIndex);
        if (cnAct != NULL) { actName = (u8 *)cnAct; }
    }
    u8 starFlags;

    starFlags = save_file_get_star_flags(gCurrSaveFileNum - 1, gCurrCourseNum - 1);

    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);

    if (courseIndex < COURSE_STAGES_COUNT) {
        print_hud_my_score_coins(1, gCurrSaveFileNum - 1, courseIndex, 178, 103);
        print_hud_my_score_stars(gCurrSaveFileNum - 1, courseIndex, 118, 103);
    }

    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);

    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);

    if (courseIndex < COURSE_STAGES_COUNT && save_file_get_course_star_count(gCurrSaveFileNum - 1, courseIndex) != 0) {
#if defined(SM64COOPDX_CN_TEXT) && !defined(VERSION_EU)
        print_generic_string(cnPack ? MYSCORE_X_CN : MYSCORE_X, 121, textMyScore);
#else
        print_generic_string(MYSCORE_X, 121, textMyScore);
#endif
    }

    if (courseIndex < COURSE_STAGES_COUNT) {
#ifdef VERSION_EU
        print_generic_string(48, 157, gTextCourseArr[gInGameLanguage]);
#else
        print_generic_string(TXT_COURSE_X, 157, textCourse);
#endif
        int_to_str(gCurrCourseNum, strCourseNum);
#ifdef VERSION_EU
        print_generic_string(get_string_width(gTextCourseArr[gInGameLanguage]) + 51, 157, strCourseNum);
#else
        // Fixed column, exactly like vanilla and like the iQue build. Measuring
        // the label instead ("关卡" is 32px, "COURSE" ~37px) put the number at
        // a different place depending on the pack state, which is what made the
        // vanilla header look mis-spaced.
        print_generic_string(CRS_NUM_X1, 157, strCourseNum);
#endif

        if (starFlags & (1 << (starIndex - 1))) {
            print_generic_string(TXT_STAR_X, 140, textStar);
        } else {
            print_generic_string(TXT_STAR_X, 140, textUnfilledStar);
        }
        if (actName != NULL) {
            print_generic_string(ACT_NAME_X, 140, actName);
        }
#ifndef VERSION_JP
        print_generic_string(LVL_NAME_X, 157, courseName);
#endif
    }
#ifndef VERSION_JP
    else {
#if defined(VERSION_US) || defined(VERSION_SH)
        print_generic_string(94, 157, courseName);
#elif defined(VERSION_EU)
        print_generic_string(get_str_x_pos_from_center(159, courseName, 10.0f), 157, courseName);
#endif
    }
#else
    print_generic_string(117, 157, courseName);
#endif
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

#if defined(VERSION_JP) || defined(VERSION_SH)
#define TXT1_X 4
#define TXT2_X 116
#define Y_VAL7 0
#else
#define TXT1_X 3
#define TXT2_X 119
#define Y_VAL7 2
#endif

void render_pause_camera_options(s16 x, s16 y, s8 *index, s16 xIndex) {
    INGAME_TEXT_COPY(textLakituMario, TEXT_LAKITU_MARIO, CN_LABEL_LAKITU_MARIO);
    INGAME_TEXT_COPY(textLakituStop, TEXT_LAKITU_STOP, CN_LABEL_LAKITU_STOP);
#ifdef VERSION_EU
    u8 textNormalUpClose[][20] = {
        { TEXT_NORMAL_UPCLOSE },
        { TEXT_NORMAL_UPCLOSE_FR },
        { TEXT_NORMAL_UPCLOSE_DE }
    };
    u8 textNormalFixed[][17] = {
        { TEXT_NORMAL_FIXED },
        { TEXT_NORMAL_FIXED_FR },
        { TEXT_NORMAL_FIXED_DE },
    };
#define textNormalUpClose textNormalUpClose[gInGameLanguage]
#define textNormalFixed   textNormalFixed[gInGameLanguage]
#else
    INGAME_TEXT_COPY(textNormalUpClose, TEXT_NORMAL_UPCLOSE, CN_LABEL_NORMAL_UPCLOSE);
    INGAME_TEXT_COPY(textNormalFixed, TEXT_NORMAL_FIXED, CN_LABEL_NORMAL_FIXED);
#endif

    handle_menu_scrolling(MENU_SCROLL_HORIZONTAL, index, 1, 2);

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);

    // The US layout is two stacked rows per column: "LAKITU MARIO" over
    // "NORMALUPCLOSE", "LAKITU STOP" over "NORMALFIXED". That only works
    // because the rows are 15px apart and the glyphs are 8px tall. The Chinese
    // labels are 16px tall, so the same rows overlap by a pixel and, worse,
    // "跟随马力欧" (5 full-width glyphs = 80px) sits almost exactly on top of
    // "特写镜头" (4 glyphs = 64px) at the same X -- the pair renders as one
    // illegible smear and reads as "the sub-labels are missing".
    //
    // The official iQue build drops the LAKITU row entirely and keeps only the
    // two real choices, on one line (n64decomp-cn, "ique hotfix",
    // src/game/ingame_menu.c has an explicit #ifdef VERSION_CN branch doing
    // exactly this). Follow it: one row, the two options.
    if (CN_PACK_ACTIVE()) {
        print_generic_string(x + 14, y + 2, textNormalUpClose);
        print_generic_string(x + 124, y + 2, textNormalFixed);
    } else {
        print_generic_string(x + 14, y + 2, textLakituMario);
        print_generic_string(x + TXT1_X, y - 13, textNormalUpClose);
        print_generic_string(x + 124, y + 2, textLakituStop);
        print_generic_string(x + TXT2_X, y - 13, textNormalFixed);
    }

    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
    create_dl_translation_matrix(MENU_MTX_PUSH, ((index[0] - 1) * xIndex) + x, y + Y_VAL7, 0);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    switch (index[0]) {
        case 1:
            cam_select_alt_mode(1);
            break;
        case 2:
            cam_select_alt_mode(2);
            break;
    }
}

#if defined(VERSION_JP) || defined(VERSION_SH)
#define X_VAL8 0
#define Y_VAL8 4
#else
#define X_VAL8 4
#define Y_VAL8 2
#endif

void render_pause_course_options(s16 x, s16 y, s8 *index, s16 yIndex) {
    // "EXIT TO CASTLE" has no TEXT_* constant in the vanilla ROM -- it was
    // built on the stack with the ASCII converter, which cannot emit the
    // two-byte CN encoding. Route it through the label table so it gets a
    // translation; the vanilla ASCII buffer stays as the fallback.
    u8 textExitToCastleVanilla[16] = { DIALOG_CHAR_TERMINATOR };
    convert_string_ascii_to_sm64(textExitToCastleVanilla, "EXIT TO CASTLE", false);
    const u8 *textExitToCastle = cn_label_get(CN_LABEL_EXIT_TO_CASTLE)
        ? cn_label_get(CN_LABEL_EXIT_TO_CASTLE)
        : (const u8 *)textExitToCastleVanilla;

#ifdef VERSION_EU
    u8 textContinue[][10] = {
        { TEXT_CONTINUE },
        { TEXT_CONTINUE_FR },
        { TEXT_CONTINUE_DE }
    };
    u8 textExitCourse[][15] = {
        { TEXT_EXIT_COURSE },
        { TEXT_EXIT_COURSE_FR },
        { TEXT_EXIT_COURSE_DE }
    };

    u8 textCameraAngleR[][24] = {
        { TEXT_CAMERA_ANGLE_R },
        { TEXT_CAMERA_ANGLE_R_FR },
        { TEXT_CAMERA_ANGLE_R_DE }
    };
#define textContinue     textContinue[gInGameLanguage]
#define textExitCourse   textExitCourse[gInGameLanguage]
#define textCameraAngleR textCameraAngleR[gInGameLanguage]
#else
    INGAME_TEXT_COPY(textContinue, TEXT_CONTINUE, CN_LABEL_CONTINUE);
    INGAME_TEXT_COPY(textExitCourse, TEXT_EXIT_COURSE, CN_LABEL_EXIT_COURSE);
    INGAME_TEXT_COPY(textCameraAngleR, TEXT_CAMERA_ANGLE_R, CN_LABEL_CAMERA_ANGLE_R);
#endif

    handle_menu_scrolling(MENU_SCROLL_VERTICAL, index, 1, 4);

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);

    print_generic_string(x + 10, y - 2, textContinue);
    print_generic_string(x + 10, y - 17, textExitCourse);
    print_generic_string(x + 10, y - 32, textExitToCastle);

    if (index[0] != 4) {
        print_generic_string(x + 10, y - 47, textCameraAngleR);
        gSPDisplayList(gDisplayListHead++, dl_ia_text_end);

        create_dl_translation_matrix(MENU_MTX_PUSH, x - X_VAL8, (y - ((index[0] - 1) * yIndex)) - Y_VAL8, 0);

        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
        gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
        gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
    } else {
        render_pause_camera_options(x - 42, y - 57, &gDialogCameraAngleIndex, 110);
    }
}

void render_pause_castle_menu_box(s16 x, s16 y) {
    create_dl_translation_matrix(MENU_MTX_PUSH, x - 78, y - 32, 0);
    create_dl_scale_matrix(MENU_MTX_NOPUSH, 1.2f, 0.8f, 1.0f);
    gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 105);
    gSPDisplayList(gDisplayListHead++, dl_draw_text_bg_box);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    create_dl_translation_matrix(MENU_MTX_PUSH, x + 6, y - 28, 0);
    create_dl_rotation_matrix(MENU_MTX_NOPUSH, DEFAULT_DIALOG_BOX_ANGLE, 0, 0, 1.0f);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    create_dl_translation_matrix(MENU_MTX_PUSH, x - 9, y - 101, 0);
    create_dl_rotation_matrix(MENU_MTX_NOPUSH, 270.0f, 0, 0, 1.0f);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

void render_pause_castle_menu_box_extended(s16 x, s16 y) {
    create_dl_translation_matrix(MENU_MTX_PUSH, x - 98, y - 32, 0);
    create_dl_scale_matrix(MENU_MTX_NOPUSH, 1.5f, 0.8f, 1.0f);
    gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 105);
    gSPDisplayList(gDisplayListHead++, dl_draw_text_bg_box);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    create_dl_translation_matrix(MENU_MTX_PUSH, x + 6, y - 28, 0);
    create_dl_rotation_matrix(MENU_MTX_NOPUSH, DEFAULT_DIALOG_BOX_ANGLE, 0, 0, 1.0f);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    create_dl_translation_matrix(MENU_MTX_PUSH, x - 9, y - 101, 0);
    create_dl_rotation_matrix(MENU_MTX_NOPUSH, 270.0f, 0, 0, 1.0f);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

void highlight_last_course_complete_stars(void) {
    u8 courseDone;

    if (gLastCompletedCourseNum == COURSE_NONE) {
        courseDone = 0;
    } else {
        courseDone = gLastCompletedCourseNum - 1;

        if (courseDone >= COURSE_STAGES_COUNT) {
            courseDone = COURSE_STAGES_COUNT;
        }
    }

    gDialogLineNum = courseDone;
}

// iQue's own title strings for the coloured HUD font, byte-for-byte from
// include/text_strings.h.in of the official CN decomp.
//
// These are single-byte codes into cn_hud_glyphs_rgba16.h and have nothing to do
// with the two-byte CN token encoding the rest of this file walks. The 0xFE
// bytes are the iQue line break, 0xFF terminates.
//
// The blank codes matter: iQue pads every title to a fixed 6-glyph line, and
// four of TEXT_PAUSE's twelve codes (0x86, 0x88, 0x8F, 0x91) are fully
// transparent cells that exist only to place the two characters. cn_draw_hud_title()
// draws them as nothing and still advances 16px, which reproduces iQue's spacing
// exactly -- dropping them instead would shift "停" 32px to the left.
// iQue's own title strings for the coloured HUD font.
//
// The byte sequences are verbatim from include/text_strings.h.in in the official
// CN decomp, including the 0xFE line break: iQue really does draw these over
// TWO lines, because the English titles they replaced ("SAVE AND" / "CONTINUE")
// were two lines long and the layout had to keep matching.
//
// The codes are single bytes indexing cn_hud_glyphs_rgba16.h and have nothing to
// do with the two-byte CN token encoding the rest of this file walks.
//
// Why the two lines are NOT identical: line 1 holds full-width 16px cells,
// while line 2 holds half-width ~8px cells (their ink sits in one half of the
// 16px cell). print_hud_lut_string() advances 16px for every code >= 0x3A
// regardless, so line 2 renders as narrow glyphs with wide gaps. That looks
// broken but it is what the hardware does; do not "fix" the advance per glyph,
// or line 1 stops lining up too.
//
// The blank codes are load-bearing: iQue pads every title to a fixed 6-glyph
// line, and four of TEXT_PAUSE's twelve codes (0x86, 0x88, 0x8F, 0x91) are
// fully transparent cells that exist only to position the characters.
// cn_draw_hud_title() draws them as nothing and still advances 16px -- dropping
// them would shift "停" 32px to the left.
#define CN_HUD_STR_PAUSE   0x86, 0x87, 0x8A, 0x8B, 0x8E, 0x8F, 0xFE, 0x88, 0x89, 0x8C, 0x8D, 0x90, 0x91, 0xFF
#define CN_HUD_STR_HI_SCORE 0x6A, 0x6B, 0x6E, 0x6F, 0x72, 0x73, 0xFE, 0x6C, 0x6D, 0x70, 0x71, 0x74, 0x75, 0xFF
#define CN_HUD_STR_CONGRAT  0x3A, 0x3B, 0x3E, 0x3F, 0x42, 0x43, 0xFE, 0x3C, 0x3D, 0x40, 0x41, 0x44, 0x45, 0xFF

// Placement, taken from the official decomp's own VERSION_CN branches
// (src/game/ingame_menu.c):
//     #elif defined(VERSION_CN)  TXT_HISCORE_X 109  TXT_HISCORE_Y 26
//                               TXT_CONGRATS_X 100 TXT_CONGRATS_Y 62
//     #elif defined(VERSION_CN)  PAUSE_X 123
// These are NOT the US numbers this file compiles with (HISCORE 109/36,
// CONGRATS 70/67, PAUSE_X 123): the coloured CN titles are a different size, so
// iQue moved them. Since we build the US baseline those #elifs are unreachable,
// hence the explicit CN_* copies. They are only read inside a CN_PACK_ACTIVE()
// gate, so the vanilla path keeps its own constants.
//
// iQue anchors both lines at this single x (print_hud_lut_string does
// "curX -= xOffset" on 0xFE), so these are left edges, not centres. Do not
// substitute a centring calculation: the left-aligned origin is what puts the
// glyphs where iQue does.
#define CN_HUD_HISCORE_X 109
#define CN_HUD_HISCORE_Y 26
#define CN_HUD_CONGRATS_X 100
#define CN_HUD_CONGRATS_Y 62
#define CN_HUD_PAUSE_X 113
#define CN_HUD_PAUSE_Y 81

void print_hud_pause_colorful_str(void) {
    INGAME_TEXT_COPY(textPause, TEXT_PAUSE, CN_LABEL_PAUSE);

    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);

#if defined(SM64COOPDX_CN_TEXT)
    if (CN_PACK_ACTIVE()) {
        // The coloured HUD font, not the IA8 dialog font. This is the whole point
        // of the iQue release's title and it cannot be reproduced by tinting the
        // monochrome glyphs: the two are different designs (IoU between the
        // coloured cells and cn_glyphs_16x16.h peaks at 0.43).
        static const u8 sPause[] = { CN_HUD_STR_PAUSE };
        cn_draw_hud_title(CN_HUD_PAUSE_X, CN_HUD_PAUSE_Y, sPause);
        gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);
        return;
    }
#endif

#ifdef VERSION_EU
    print_hud_lut_string(HUD_LUT_GLOBAL, get_str_x_pos_from_center_scale(
                         SCREEN_WIDTH / 2, textPause, 12.0f), 81, textPause);
#else
    print_hud_lut_string(HUD_LUT_GLOBAL, 123, 81, textPause);
#endif

    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);
}

void render_pause_castle_course_stars(s16 x, s16 y, s16 fileNum, s16 courseNum) {
    s16 hasStar = 0;

    u8 str[COURSE_STAGES_COUNT * 2];

    INGAME_TEXT_COPY(textStar, TEXT_STAR, CN_LABEL_STAR);

    u8 starFlags = save_file_get_star_flags(fileNum, courseNum);
    u16 starCount = save_file_get_course_star_count(fileNum, courseNum);

    u16 nextStar = 0;

    if (starFlags & 0x40) {
        starCount--;
        print_generic_string(x + 89, y - 5, textStar);
    }

    while (hasStar != starCount) {
        if (starFlags & (1 << nextStar)) {
            str[nextStar * 2] = DIALOG_CHAR_STAR_FILLED;
            hasStar++;
        } else {
            str[nextStar * 2] = DIALOG_CHAR_STAR_OPEN;
        }

        str[nextStar * 2 + 1] = DIALOG_CHAR_SPACE;
        nextStar++;
    }

    if (starCount == nextStar && starCount != 6) {
        str[nextStar * 2] = DIALOG_CHAR_STAR_OPEN;
        str[nextStar * 2 + 1] = DIALOG_CHAR_SPACE;
        nextStar++;
    }

    str[nextStar * 2] = DIALOG_CHAR_TERMINATOR;

    print_generic_string(x + 14, y + 13, str);
}

// What to hand the course-name renderer for a *main* course slot.
//
// The upstream renderer passes the table entry whole, and that is correct: the
// 3-byte prefix on a vanilla main-course entry is not decoration, it is the
// course number. Read straight out of the US ROM:
//
//   BOB    9E 01 9E "BOB-OMB BATTLEFIELD"      ->  " 1 BOB-OMB BATTLEFIELD"
//   WF     9E 02 9E "WHISKY'S FOUNTAIN"        ->  " 2 ..."
//   SL     01 00 9E "SLIPPERY SLIDE"          ->  " 10 ..."  (two-digit)
//   BITDW  9E 9E 9E "BOWSER IN THE DARK WORLD" ->  no number, 3 blanks
//
// Only the star-select screen strips it, via &courseName[CRS_NAME_START],
// because it draws "COURSE x" out of a separate column. The pause menu has no
// such column, so skipping the prefix there silently deleted the number -- the
// "关卡前应该有数字" bug. Hence main courses pass the base pointer, while the
// secret-star / castle-star slots keep `+ 3`: their prefix is three blank
// glyphs, so dropping it would shift the name left for no gain.
//
// The CN side needs no such test. iQue's course-name table carries NO 3-byte
// prefix -- in the CN ROM the entry starts straight at the U+3000 run (checked
// against the decompressed segment; official CRS_NAME_START is 6, i.e. three
// two-byte tokens, not three bytes). Our generator prepends a synthetic
// [9E][n][9E] so the `+ 3` call sites keep working, so CN buffers must still be
// advanced past it.
static const u8 *pause_castle_course_name_arg(const u8 *courseName) {
    return cn_is_cn_string(courseName) ? courseName + 3 : courseName;
}

void render_pause_castle_main_strings(s16 x, s16 y) {
#ifdef VERSION_EU
    u8 textCoin[] = { TEXT_COIN };
    u8 textX[] = { TEXT_VARIABLE_X };
#else
    INGAME_TEXT_COPY(textCoin, TEXT_COIN_X, CN_LABEL_COIN_X);
#endif

    u8 strVal[8];
    s16 starNum = gDialogLineNum;

    handle_menu_scrolling(MENU_SCROLL_VERTICAL, &gDialogLineNum, -1, COURSE_STAGES_COUNT + 1);

    if (gDialogLineNum == COURSE_STAGES_COUNT + 1) {
        gDialogLineNum = 0;
    }

    if (gDialogLineNum == -1) {
        gDialogLineNum = COURSE_STAGES_COUNT;
    }

    if (gDialogLineNum != COURSE_STAGES_COUNT) {
        while (save_file_get_course_star_count(gCurrSaveFileNum - 1, gDialogLineNum) == 0) {
            if (gDialogLineNum >= starNum) {
                gDialogLineNum++;
            } else {
                gDialogLineNum--;
            }

            if (gDialogLineNum == COURSE_STAGES_COUNT || gDialogLineNum == -1) {
                gDialogLineNum = COURSE_STAGES_COUNT;
                break;
            }
        }
    }

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);

    // Same rationale as render_pause_castle_main_strings_extended(): pull the
    // name from the (CN-aware) course table rather than round-tripping it
    // through the vanilla single-byte ASCII helpers.
    const u8 *courseName;
    {
        void **courseNameTbl = get_course_name_table();
        s32 slot = (gDialogLineNum == COURSE_STAGES_COUNT) ? COURSE_MAX : gDialogLineNum;
        courseName = segmented_to_virtual(courseNameTbl[slot]);
    }

    if (gDialogLineNum < COURSE_STAGES_COUNT) {
        render_pause_castle_course_stars(x, y, gCurrSaveFileNum - 1, gDialogLineNum);
        print_generic_string(x + 34, y - 5, textCoin);
#ifdef VERSION_EU
        print_generic_string(x + 44, y - 5, textX);
#endif
        int_to_str(save_file_get_course_coin_score(gCurrSaveFileNum - 1, gDialogLineNum), strVal);
        print_generic_string(x + 54, y - 5, strVal);
#ifdef VERSION_EU
        print_generic_string(x - 17, y + 30, courseName);
#endif
    } else {
        INGAME_TEXT_COPY(textStarX, TEXT_STAR_X, CN_LABEL_STAR_X);
        print_generic_string(x + 40, y + 13, textStarX);
        int_to_str(save_file_get_total_star_count(gCurrSaveFileNum - 1, COURSE_BONUS_STAGES - 1, COURSE_MAX - 1), strVal);
        print_generic_string(x + 60, y + 13, strVal);
#ifdef VERSION_EU
        print_generic_string(get_str_x_pos_from_center(x + 51, courseName + 3, 10.0f), y + 30, courseName + 3);
#endif
    }

#ifndef VERSION_EU
    // Upstream draws the entry whole at x - 9. On a main course the 3-byte
    // prefix IS the course number, so this is what puts "1" in front of
    // "BOB-OMB BATTLEFIELD"; see pause_castle_course_name_arg().
    print_generic_string(x - 9, y + 30, pause_castle_course_name_arg(courseName));
#endif

    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

#define INDEX_CASTLE_STARS (COURSE_COUNT)

// Returns the CN-aware course name for a 1-based course number, taken straight
// from the (already pack-switched) course name table. Every entry carries the
// vanilla 3-byte [colour][num][0x9E] prefix, so callers that want bare text add
// + 3.
//
// Do NOT route this through get_level_name_sm64(): that helper round-trips
// through convert_string_sm64_to_ascii()/convert_string_ascii_to_sm64(), which
// only understand the vanilla single-byte font. CN course names came back as
// spaces or raw bytes, which is why vanilla-looking glyphs kept showing up.
// Strips the decoration the official iQue course names carry at their front:
// U+3000, the full-width course number, U+3000 -- i.e. "　１　炸弹王国".
//
// `name + 3` only removes the vanilla 3-byte [colour][num][0x9E] prefix that the
// generator emits, so the CN decoration survives it. That is harmless where the
// name is drawn centred on its own (the star-select header), but the pause menu
// lays the line out at fixed X positions and *already* prints the course number
// of its own, so leaving it in put the number on screen twice and pushed the
// name off its column.
//
// Only the leading run is removed, and only when it matches the exact pattern,
// so a name that legitimately begins with a space keeps it.
const u8 *cn_strip_course_name_decoration(const u8 *name) {
    if (name == NULL || !cn_is_cn_string(name)) {
        return name;
    }

    // Drop the vanilla 3-byte [colour][num][0x9E] prefix the generator emits.
    const u8 *p = name + 3;
    const u8 *const fallback = p;

    // The decoration is a run of U+3000, then the course number, then another
    // run of U+3000. Neither the number nor the space run has a fixed width:
    // courses 10-15 encode "10".."15" as two ASCII tokens, and the secret-star
    // slots have no number at all, just three U+3000. So consume tokens, not
    // bytes.
    //
    // Note 0x00 0x00 is the digit '0' (iQue encodes full-width and half-width
    // digits identically), NOT a terminator -- the only terminator is 0xFF 0xFF.
    // Course 10 ("　１０　雪人王国") relies on this, so the number loop must not
    // treat a zero byte as end-of-string. Bound it instead, so a name that is
    // somehow all-digits cannot walk off the buffer.
    while (p[0] == DIALOG_CHAR_TERMINATOR && p[1] == DIALOG_CHAR_SPACE) {
        p += 2;
    }
    for (int guard = 0; guard < 4 && p[0] == 0x00; guard++) {
        p += 2;
    }

    // Trailing U+3000 run. Requiring at least one space is what distinguishes a
    // decorated name from a bare one; if there is none, keep the prefix so the
    // caller still draws the whole string.
    if (!(p[0] == DIALOG_CHAR_TERMINATOR && p[1] == DIALOG_CHAR_SPACE)) {
        return fallback;
    }

    // Drop the whole trailing run, not just one space: the secret-star slots
    // indent with three U+3000 and would otherwise keep two of them.
    while (p[0] == DIALOG_CHAR_TERMINATOR && p[1] == DIALOG_CHAR_SPACE) {
        p += 2;
    }

    return p;
}

const u8 *ingame_menu_get_course_name(s32 courseNum) {
    void **courseNameTbl = get_course_name_table();
    s32 slot = courseNum - 1;

    if (slot < 0 || slot >= INDEX_CASTLE_STARS) {
        return NULL;
    }
    return segmented_to_virtual(courseNameTbl[slot]);
}

// Returns the CN-aware act (star) name for a 1-based course number and 1-based
// star number, taken straight from the (already pack-switched) act name table.
// Unlike course names, act names carry no 3-byte prefix. Returns NULL when the
// slot is out of range (e.g. the 100-coin / secret star rows), so callers can
// fall back to the vanilla-generated string.
const u8 *ingame_menu_get_act_name(s32 courseNum, s32 starNum) {
    s32 courseIdx = courseNum - 1; // COURSE_BOB == 1 -> row 0
    s32 actIdx = starNum - 1;

    if (courseIdx < 0 || courseIdx >= COURSE_STAGES_MAX) {
        return NULL;
    }
    if (actIdx < 0 || actIdx >= MAX_ACTS) {
        return NULL;
    }

    void **actNameTbl = get_act_name_table();
    return segmented_to_virtual(actNameTbl[courseIdx * MAX_ACTS + actIdx]);
}
#define INDEX_FLAGS (COURSE_COUNT + 1)
#define INDEX_MIN (-1)
#define INDEX_MAX (INDEX_FLAGS + 1)

static u32 pause_castle_get_stars(s32 index) {

    // Main courses (0-14), Secret courses (15-24)
    if (index >= 0 && index < INDEX_CASTLE_STARS) {
        return save_file_get_star_flags(gCurrSaveFileNum - 1, index);
    }

    // Castle stars (25)
    if (index == INDEX_CASTLE_STARS) {
        return save_file_get_star_flags(gCurrSaveFileNum - 1, -1);
    }

    // Flags (26)
    if (index == INDEX_FLAGS) {
        return save_file_get_flags();
    }

    return 0;
}



static void render_pause_castle_course_name(const u8 *courseName, s16 x, s16 y) {
    s16 width = 0;
    bool cn = cn_is_cn_string(courseName);
#ifndef VERSION_JP
    // Token-wise walk so two-byte CN glyphs are measured once each and the
    // course name is centred correctly. A byte-wise walk would also stop early
    // on a CN newline.
    {
        s32 pos = 0;
        for (s32 cap = 0; cap < 1024; cap++) {
            CnToken tok = cn_read_token(courseName, pos, cn);

            if (tok.kind == CN_TOKEN_END) {
                break;
            }
            if (tok.kind) {
                if (tok.kind == CN_TOKEN_GLYPH) {
                    width += (s16)cn_char_width(tok.code);
                } else if (tok.kind == CN_TOKEN_SPACE) {
                    width += gDialogCharWidths[DIALOG_CHAR_SPACE];
                }
                pos += tok.length;
                continue;
            }
            // Vanilla single-byte path. gDialogCharWidths has 256 entries, so
            // the byte index is always in range, but the string still has to
            // terminate: a CN buffer reached via this path would never hit
            // 0xFF, hence the iteration cap above.
            if (courseName[pos] == DIALOG_CHAR_TERMINATOR) {
                break;
            }
            width += gDialogCharWidths[courseName[pos]];
            pos++;
        }
    }
#endif
    print_generic_string(x - width / 2, y, courseName);
}

static void render_pause_castle_flag_icon(const u8 *texture, s16 texW, s16 texH, s16 x, s16 y, s16 w, s16 h) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, texW, texH, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0, 0, 0);
    gSPTextureRectangle(gDisplayListHead++, (x) << 2, (SCREEN_HEIGHT - h - y) << 2, (x + w) << 2, (SCREEN_HEIGHT - y) << 2, G_TX_RENDERTILE, 0, 0, ((0x400 * texW) / w), ((0x400 * texH) / h));
}

static void render_pause_castle_flag(s16 x, s16 y, u32 flag) {
    if (save_file_get_flags() & flag) {
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
    } else {
        gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, gDialogTextAlpha / 3);
    }
    switch (flag) {
        case SAVE_FLAG_HAVE_WING_CAP:
            render_pause_castle_flag_icon(exclamation_box_seg8_texture_08015E28, 32, 32, x, y, 12, 12);
            break;

        case SAVE_FLAG_HAVE_METAL_CAP:
            render_pause_castle_flag_icon(exclamation_box_seg8_texture_08014628, 32, 32, x, y, 12, 12);
            break;

        case SAVE_FLAG_HAVE_VANISH_CAP:
            render_pause_castle_flag_icon(exclamation_box_seg8_texture_08012E28, 32, 32, x, y, 12, 12);
            break;

        case SAVE_FLAG_HAVE_KEY_1 | SAVE_FLAG_UNLOCKED_BASEMENT_DOOR:
        case SAVE_FLAG_HAVE_KEY_2 | SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR:
            render_pause_castle_flag_icon(bowser_key_left_texture, 32, 64, x, y, 6, 12);
            render_pause_castle_flag_icon(bowser_key_right_texture, 32, 64, x + 6, y, 6, 12);
            break;
    }
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
}

static void render_pause_castle_course_stars_extended(s16 x, s16 y) {
    bool isMainCourse = COURSE_IS_MAIN_COURSE(gDialogLineNum + 1);
    u32 stars = pause_castle_get_stars(gDialogLineNum);
    u8 str[32];

    // Build the stars string
    s32 lastCollectedStar = 0;
    for (s32 i = 0; i != (isMainCourse ? 6 : 7); ++i) {
        if (stars & (1 << i)) {
            str[2 * i] = DIALOG_CHAR_STAR_FILLED;
            lastCollectedStar = i + 1;
        } else {
            str[2 * i] = DIALOG_CHAR_STAR_OPEN;
        }
        str[2 * i + 1] = DIALOG_CHAR_SPACE;
        str[2 * i + 2] = DIALOG_CHAR_TERMINATOR;
    }

    // Hide the not collected ones after the last collected for secret courses
    if (!isMainCourse) {
        str[2 * lastCollectedStar] = DIALOG_CHAR_TERMINATOR;
    }

    // Render the 100 coins star next to the coin counter for main courses
    if (isMainCourse && (stars & 0x40)) {
        INGAME_TEXT_COPY(textStar, TEXT_STAR, CN_LABEL_STAR);
        print_generic_string(x + 89, y - 5, textStar);
    }

    // Render the stars
    print_generic_string(x + 14, y + 13, str);
}

void render_pause_castle_main_strings_extended(s16 x, s16 y) {

    // Main courses (0-14), Secret courses (15-24), Castle stars (25), Flags (26)
    // Indices -1 and 26 are used to loop back respectively to Flags and Course 1
    s8 prevIndex = gDialogLineNum;
    handle_menu_scrolling(MENU_SCROLL_VERTICAL, &gDialogLineNum, INDEX_MIN, INDEX_MAX);
    s8 scrollDir = (gDialogLineNum >= prevIndex ? +1 : -1);
    if (gDialogLineNum >= INDEX_MAX) {
        gDialogLineNum = 0;
        scrollDir = +1;
    } else if (gDialogLineNum <= INDEX_MIN) {
        gDialogLineNum = INDEX_FLAGS;
        scrollDir = -1;
    }

    // Skip courses with 0 star collected
    if (gDialogLineNum < INDEX_CASTLE_STARS) {
        while (!pause_castle_get_stars(gDialogLineNum)) {
            gDialogLineNum += scrollDir;
            if (gDialogLineNum >= INDEX_CASTLE_STARS) {
                gDialogLineNum = INDEX_CASTLE_STARS;
                break;
            }
            if (gDialogLineNum <= INDEX_MIN) {
                gDialogLineNum = INDEX_FLAGS;
                break;
            }
        }
    }

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);

    // Take the name straight from the course name table. Going through
    // get_level_name_sm64() here used to round-trip the text through
    // convert_string_sm64_to_ascii() -> convert_string_ascii_to_sm64(), which
    // only understands the vanilla single-byte font: CN course names came back
    // as runs of spaces / raw bytes and the renderer then produced garbage.
    //
    // The table is already switched by apply_cn_text_pack(), so this returns CN
    // text when the pack is on and the vanilla ROM string when it is off.
    // Main courses keep the base pointer (the 3-byte prefix holds the course
    // number); see pause_castle_course_name_arg() for the full story.
    const u8 *courseName;
    {
        void **courseNameTbl = get_course_name_table();
        s32 slot = (gDialogLineNum >= INDEX_CASTLE_STARS) ? COURSE_MAX : gDialogLineNum;
        courseName = segmented_to_virtual(courseNameTbl[slot]);
    }

    // Main courses (0-14)
    if (gDialogLineNum < COURSE_STAGES_COUNT) {
        INGAME_TEXT_COPY(textCoin, TEXT_COIN_X, CN_LABEL_COIN_X);
        u8 textCoinCount[8];
        render_pause_castle_course_name(pause_castle_course_name_arg(courseName), 160, y + 30);
        render_pause_castle_course_stars_extended(x + 20, y);
        print_generic_string(x + 54, y - 5, textCoin);
        int_to_str(save_file_get_course_coin_score(gCurrSaveFileNum - 1, gDialogLineNum), textCoinCount);
        print_generic_string(x + 74, y - 5, textCoinCount);
    }

    // Secret courses (15-24)
    else if (gDialogLineNum >= COURSE_STAGES_COUNT && gDialogLineNum < INDEX_CASTLE_STARS) {
        render_pause_castle_course_name(courseName + 3, 160, y + 30);
        render_pause_castle_course_stars_extended(x + 20, y);
    }

    // Castle stars (25)
    else if (gDialogLineNum == INDEX_CASTLE_STARS) {
        INGAME_TEXT_COPY(textStar, TEXT_STAR_X, CN_LABEL_STAR_X);
        u8 textStarCount[8];
        render_pause_castle_course_name(courseName + 3, 160, y + 30);
        print_generic_string(x + 60, y + 13, textStar);
        int_to_str(save_file_get_course_star_count(gCurrSaveFileNum - 1, -1), textStarCount);
        print_generic_string(x + 80, y + 13, textStarCount);
    }

    // Flags (26)
    else if (gDialogLineNum == INDEX_FLAGS) {
        const u8 textFlags[] = { ASCII_TO_DIALOG('P'), ASCII_TO_DIALOG('R'), ASCII_TO_DIALOG('O'), ASCII_TO_DIALOG('G'), ASCII_TO_DIALOG('R'), ASCII_TO_DIALOG('E'), ASCII_TO_DIALOG('S'), ASCII_TO_DIALOG('S'), DIALOG_CHAR_TERMINATOR };
        const u8 textCaps[] = { ASCII_TO_DIALOG('C'), ASCII_TO_DIALOG('A'), ASCII_TO_DIALOG('P'), ASCII_TO_DIALOG('S'), 0xE6, DIALOG_CHAR_TERMINATOR };
        const u8 textKeys[] = { ASCII_TO_DIALOG('K'), ASCII_TO_DIALOG('E'), ASCII_TO_DIALOG('Y'), ASCII_TO_DIALOG('S'), 0xE6, DIALOG_CHAR_TERMINATOR };
        render_pause_castle_course_name(textFlags, 160, y + 30);
        print_generic_string(x + 45, y + 13, textCaps);
        render_pause_castle_flag(x + 80, y + 15, SAVE_FLAG_HAVE_WING_CAP);
        render_pause_castle_flag(x + 96, y + 15, SAVE_FLAG_HAVE_METAL_CAP);
        render_pause_castle_flag(x + 112, y + 15, SAVE_FLAG_HAVE_VANISH_CAP);
        print_generic_string(x + 45, y - 5, textKeys);
        render_pause_castle_flag(x + 80, y - 3, SAVE_FLAG_HAVE_KEY_1 | SAVE_FLAG_UNLOCKED_BASEMENT_DOOR);
        render_pause_castle_flag(x + 96, y - 3, SAVE_FLAG_HAVE_KEY_2 | SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR);
    }

    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

s8 gCourseCompleteCoinsEqual = 0;
s32 gCourseDoneMenuTimer = 0;
s32 gCourseCompleteCoins = 0;
s8 gHudFlash = 0;

s16 render_pause_courses_and_castle(void) {
    s16 num;

#ifdef VERSION_EU
    gInGameLanguage = eu_get_language();
#endif
    switch (gDialogBoxState) {
        case DIALOG_STATE_OPENING:
            gDialogLineNum = 1;
            gDialogTextAlpha = 0;
            //level_set_transition(-1, NULL);
#ifdef VERSION_JP
            play_sound(SOUND_MENU_PAUSE, gGlobalSoundSource);
#else
            play_sound(SOUND_MENU_PAUSE_HIGHPRIO, gGlobalSoundSource);
#endif

            if (COURSE_IS_VALID_COURSE(gCurrCourseNum)) {
                change_dialog_camera_angle();
                gDialogBoxState = DIALOG_STATE_VERTICAL;
            } else {
                highlight_last_course_complete_stars();
                gDialogBoxState = DIALOG_STATE_HORIZONTAL;
            }
            break;
        case DIALOG_STATE_VERTICAL:
            if (!gDjuiPanelPauseCreated && !gPauseMenuHidden) {
                shade_screen();
                render_pause_my_score_coins();
                render_pause_red_coins();

                /* Always allow exiting from course */
                if (gLevelValues.pauseExitAnywhere || (gMarioStates[0].action & ACT_FLAG_PAUSE_EXIT)) {
                    render_pause_course_options(99, 93, &gDialogLineNum, 15);
                }

#ifdef VERSION_EU
                if (gPlayer1Controller->buttonPressed & (A_BUTTON | Z_TRIG | START_BUTTON))
#else
                if (gPlayer1Controller->buttonPressed & A_BUTTON
                 || gPlayer1Controller->buttonPressed & START_BUTTON)
#endif
                {
                    bool allowExit = true;
                    if (gDialogLineNum == 2 || gDialogLineNum == 3) {
                        smlua_call_event_hooks(HOOK_ON_PAUSE_EXIT, gDialogLineNum == 3, &allowExit);
                    }
                    if (allowExit) {
                        level_set_transition(0, NULL);
                        play_sound(SOUND_MENU_PAUSE_2, gGlobalSoundSource);
                        gDialogBoxState = DIALOG_STATE_OPENING;
                        gMenuMode = -1;

                        if (gDialogLineNum == 2 || gDialogLineNum == 3) {
                            num = gDialogLineNum;
                        } else {
                            num = 1;
                        }

                        return num;
                    } else {
                        play_sound(SOUND_MENU_CAMERA_BUZZ | (0xFF << 8), gGlobalSoundSource);
                    }
                }
            }
            break;
        case DIALOG_STATE_HORIZONTAL:
            if (!gDjuiPanelPauseCreated && !gPauseMenuHidden) {
                shade_screen();
                print_hud_pause_colorful_str();

                if (gLevelValues.extendedPauseDisplay) {
                    render_pause_castle_menu_box_extended(160, 143);
                    render_pause_castle_main_strings_extended(84, 60);
                } else {
                    render_pause_castle_menu_box(160, 143);
                    render_pause_castle_main_strings(104, 60);
                }

#ifdef VERSION_EU
                if (gPlayer1Controller->buttonPressed & (A_BUTTON | Z_TRIG | START_BUTTON))
#else
                if (gPlayer1Controller->buttonPressed & A_BUTTON
                 || gPlayer1Controller->buttonPressed & START_BUTTON)
#endif
                {
                    level_set_transition(0, NULL);
                    play_sound(SOUND_MENU_PAUSE_2, gGlobalSoundSource);
                    gMenuMode = -1;
                    gDialogBoxState = DIALOG_STATE_OPENING;

                    return 1;
                }
          }
          break;
    }

    if (gDialogTextAlpha < 250) {
        gDialogTextAlpha += 25;
    }

    if (gDjuiPanelPauseCreated && !gDjuiInPlayerMenu) { shade_screen(); }
    if (gPlayer1Controller->buttonPressed & R_TRIG) {
        djui_panel_pause_create(NULL);
    }
    if ((gPlayer1Controller->buttonPressed & L_TRIG) && network_allow_mod_dev_mode()) {
        network_mod_dev_mode_reload();
    }

    return 0;
}

#if defined(VERSION_JP)
#define TXT_HISCORE_X 112
#define TXT_HISCORE_Y 48
#define TXT_CONGRATS_X 60
#elif defined(VERSION_SH)
#define TXT_HISCORE_X 118
#define TXT_HISCORE_Y 48
#define TXT_CONGRATS_X 70
#else
#define TXT_HISCORE_X 109
#define TXT_HISCORE_Y 36
#define TXT_CONGRATS_X 70
#endif

#define HUD_PRINT_HISCORE         0
#define HUD_PRINT_CONGRATULATIONS 1

void print_hud_course_complete_string(s8 str) {
#ifdef VERSION_EU
    u8 textHiScore[][15] = {
        { TEXT_HUD_HI_SCORE },
        { TEXT_HUD_HI_SCORE_FR },
        { TEXT_HUD_HI_SCORE_DE }
    };
    u8 textCongratulations[][16] = {
        { TEXT_HUD_CONGRATULATIONS },
        { TEXT_HUD_CONGRATULATIONS_FR },
        { TEXT_HUD_CONGRATULATIONS_DE }
    };
#else
    INGAME_TEXT_COPY(textHiScore, TEXT_HUD_HI_SCORE, CN_LABEL_HUD_HI_SCORE);
    INGAME_TEXT_COPY(textCongratulations, TEXT_HUD_CONGRATULATIONS, CN_LABEL_HUD_CONGRATULATIONS);
#endif

    u8 colorFade = sins(gDialogColorFadeTimer) * 50.0f + 200.0f;

    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
    gDPSetEnvColor(gDisplayListHead++, colorFade, colorFade, colorFade, 255);

    if (str == HUD_PRINT_HISCORE) {
#if defined(SM64COOPDX_CN_TEXT)
        if (CN_PACK_ACTIVE()) {
            static const u8 sHiScore[] = { CN_HUD_STR_HI_SCORE };
            cn_draw_hud_title(CN_HUD_HISCORE_X, CN_HUD_HISCORE_Y, sHiScore);
            gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);
            return;
        }
#endif
#ifdef VERSION_EU
        print_hud_lut_string(HUD_LUT_GLOBAL, get_str_x_pos_from_center_scale(160, textHiScore[gInGameLanguage], 12.0f),
                  36, textHiScore[gInGameLanguage]);
#else
        print_hud_lut_string(HUD_LUT_GLOBAL, TXT_HISCORE_X, TXT_HISCORE_Y, textHiScore);
#endif
    } else { // HUD_PRINT_CONGRATULATIONS
#if defined(SM64COOPDX_CN_TEXT)
        if (CN_PACK_ACTIVE()) {
            static const u8 sCongrats[] = { CN_HUD_STR_CONGRAT };
            cn_draw_hud_title(CN_HUD_CONGRATS_X, CN_HUD_CONGRATS_Y, sCongrats);
            gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);
            return;
        }
#endif
#ifdef VERSION_EU
        print_hud_lut_string(HUD_LUT_GLOBAL, get_str_x_pos_from_center_scale(160, textCongratulations[gInGameLanguage], 12.0f),
                  67, textCongratulations[gInGameLanguage]);
#else
        print_hud_lut_string(HUD_LUT_GLOBAL, TXT_CONGRATS_X, 67, textCongratulations);
#endif
    }

    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);
}

void print_hud_course_complete_coins(s16 x, s16 y) {
    u8 courseCompleteCoinsStr[4];
    u8 hudTextSymCoin[] = { GLYPH_COIN, GLYPH_SPACE };
    u8 hudTextSymX[] = { GLYPH_MULTIPLY, GLYPH_SPACE };

    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);

    print_hud_lut_string(HUD_LUT_GLOBAL, x, y, hudTextSymCoin);
    print_hud_lut_string(HUD_LUT_GLOBAL, x + 16, y, hudTextSymX);

    int_to_str(gCourseCompleteCoins, courseCompleteCoinsStr);
    print_hud_lut_string(HUD_LUT_GLOBAL, x + 32, y, courseCompleteCoinsStr);

    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);

    if (gCourseCompleteCoins >= gHudDisplay.coins) {
        gCourseCompleteCoinsEqual = 1;
        gCourseCompleteCoins = gHudDisplay.coins;

        if (gGotFileCoinHiScore) {
            print_hud_course_complete_string(HUD_PRINT_HISCORE);
        }
    } else {
        if ((gCourseDoneMenuTimer & 1) || gHudDisplay.coins > 70) {
            gCourseCompleteCoins++;
            play_sound(SOUND_MENU_YOSHI_GAIN_LIVES, gGlobalSoundSource);

            // retain vanilla behavior
            if (gLevelValues.numCoinsToLife == 50) {
                if (gCourseCompleteCoins == 50 || gCourseCompleteCoins == 100 || gCourseCompleteCoins == 150) {
                    play_sound(SOUND_GENERAL_COLLECT_1UP, gGlobalSoundSource);
                    gMarioStates[0].numLives++;
                }
            } else {
                if (gLevelValues.numCoinsToLife != 0 && gCourseCompleteCoins % gLevelValues.numCoinsToLife == 0 && gCourseCompleteCoins > 0) {
                    play_sound(SOUND_GENERAL_COLLECT_1UP, gGlobalSoundSource);
                    gMarioStates[0].numLives++;
                }
            }
        }

        if (gHudDisplay.coins == gCourseCompleteCoins && gGotFileCoinHiScore) {
            play_sound(SOUND_MENU_MARIO_CASTLE_WARP2, gGlobalSoundSource);
        }
    }
}

void play_star_fanfare_and_flash_hud(s32 arg, u8 starNum) {
    if (gHudDisplay.coins == gCourseCompleteCoins && (gCurrCourseStarFlags & starNum) == 0 && gHudFlash == 0) {
        gCurrCourseStarFlags |= starNum; // SM74 was spamming fanfare without this line
        if (gFanFareDebounce <= 0) {
            gFanFareDebounce = 30 * 5;
            play_star_fanfare();
        }
        gHudFlash = arg;
    }
}

#ifdef VERSION_EU
#define TXT_NAME_X1 centerX
#define TXT_NAME_X2 centerX - 1
#else
#define TXT_NAME_X1 71
#define TXT_NAME_X2 69
#endif
#if defined(VERSION_JP) || defined(VERSION_SH)
#define CRS_NUM_X2 95
#define CRS_NUM_X3 93
#define TXT_CLEAR_X1 205
#define TXT_CLEAR_X2 203
#else
#define CRS_NUM_X2 104
#define CRS_NUM_X3 102
#define TXT_CLEAR_X1 get_string_width(name) + 81
#define TXT_CLEAR_X2 get_string_width(name) + 79
#endif

void render_course_complete_lvl_info_and_hud_str(void) {
#if defined(VERSION_JP)
    u8 textSymStar[] = { GLYPH_STAR, GLYPH_SPACE };
    u8 textCourse[] = { TEXT_COURSE };
    u8 textCatch[] = { TEXT_CATCH };
    u8 textClear[] = { TEXT_CLEAR };
#elif defined(VERSION_EU)
    UNUSED u8 textCatch[] = { TEXT_CATCH }; // unused in EU
    u8 textSymStar[] = { GLYPH_STAR, GLYPH_SPACE };
#define textCourse gTextCourseArr[gInGameLanguage]
#else
    INGAME_TEXT_COPY(textCourse, TEXT_COURSE, CN_LABEL_COURSE);
    INGAME_TEXT_COPY_UNUSED(textCatch, TEXT_CATCH, CN_LABEL_CATCH); // unused in US
    INGAME_TEXT_COPY(textClear, TEXT_CLEAR, CN_LABEL_CLEAR);
    u8 textSymStar[] = { GLYPH_STAR, GLYPH_SPACE };
#endif
    u8 *name;

    u8 strCourseNum[4];

    if (gLastCompletedCourseNum <= COURSE_STAGES_MAX) {
        print_hud_course_complete_coins(118, 103);
        play_star_fanfare_and_flash_hud(1, 1 << (gLastCompletedStarNum - 1));

        name = (u8*) get_star_name_sm64(gLastCompletedCourseNum, gLastCompletedStarNum, 1);
        // Prefer the CN-aware act name when available. get_star_name_sm64()
        // round-trips through the vanilla single-byte font, so CN act names
        // would degrade to spaces.
        if (CN_PACK_ACTIVE()) {
            const u8 *cnName = ingame_menu_get_act_name(gLastCompletedCourseNum, gLastCompletedStarNum);
            if (cnName != NULL) { name = (u8 *)cnName; }
        }

        gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
        int_to_str(gLastCompletedCourseNum, strCourseNum);
        gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, gDialogTextAlpha);
        print_generic_string(65, 165, textCourse);
        print_generic_string(CRS_NUM_X2, 165, strCourseNum);
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
        print_generic_string(63, 167, textCourse);
        print_generic_string(CRS_NUM_X3, 167, strCourseNum);
        gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
    } else if ((gLastCompletedCourseNum == COURSE_BITDW || gLastCompletedCourseNum == COURSE_BITFS) && gLastCollectedStarOrKey == 1) {
        name = (u8*) get_level_name_sm64(gLastCompletedCourseNum, gCurrLevelNum, gCurrAreaIndex, 1) + 3;
        // When the CN text pack is active the course name table already holds
        // iQue names, so prefer it over the vanilla name returned above. Hand
        // the *base* pointer to cn_strip_course_name_decoration() rather than
        // `+ 3`: the iQue buffer carries "　N　" (U+3000 + full-width number +
        // U+3000) on top of the vanilla prefix, and this screen prints the
        // course number of its own right next to the name -- exactly like the
        // pause header. Stripping it keeps that number from showing twice.
        if (CN_PACK_ACTIVE()) {
            const u8 *cnName = ingame_menu_get_course_name(gLastCompletedCourseNum);
            if (cnName != NULL) { name = (u8 *)cn_strip_course_name_decoration(cnName); }
        }
        gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
        gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, gDialogTextAlpha);
#ifdef VERSION_EU
        s16 centerX = get_str_x_pos_from_center(153, name, 12.0f);
#endif
        print_generic_string(TXT_NAME_X1, 130, name);
#ifndef VERSION_EU
        print_generic_string(TXT_CLEAR_X1, 130, textClear);
#endif
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
        print_generic_string(TXT_NAME_X2, 132, name);
#ifndef VERSION_EU
        print_generic_string(TXT_CLEAR_X2, 132, textClear);
#endif
        gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
        print_hud_course_complete_string(HUD_PRINT_CONGRATULATIONS);
        print_hud_course_complete_coins(118, 111);
        play_star_fanfare_and_flash_hud(2, 0); //! 2 isn't defined, originally for key hud?
        return;
    } else {
        name = (u8*) get_level_name_sm64(gLastCompletedCourseNum, gLastCompletedStarNum, gCurrAreaIndex, 1) + 3;
        // Same CN-preference as above (secret star / bonus course names), and
        // the same decoration strip: this branch also prints the course number
        // separately, so the name must not carry its own copy.
        if (CN_PACK_ACTIVE()) {
            const u8 *cnName = ingame_menu_get_course_name(gLastCompletedCourseNum);
            if (cnName != NULL) { name = (u8 *)cn_strip_course_name_decoration(cnName); }
        }
        print_hud_course_complete_coins(118, 103);
        play_star_fanfare_and_flash_hud(1, 1 << (gLastCompletedStarNum - 1));
    }

    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
    print_hud_lut_string(HUD_LUT_GLOBAL, 55, 77, textSymStar);
    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, gDialogTextAlpha);
    print_generic_string(76, 145, name);
#if defined(VERSION_JP) || defined(VERSION_SH)
    print_generic_string(220, 145, textCatch);
#endif
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
    print_generic_string(74, 147, name);
#if defined(VERSION_JP) || defined(VERSION_SH)
    print_generic_string(218, 147, textCatch);
#endif
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

#if defined(VERSION_JP) || defined(VERSION_SH)
#define TXT_SAVEOPTIONS_X x + 10
#elif defined(VERSION_US)
#define TXT_SAVEOPTIONS_X x + 12
#elif defined(VERSION_EU)
#define TXT_SAVEOPTIONS_X xOffset
#endif
#if defined(VERSION_JP) || defined(VERSION_SH)
#define TXT_SAVECONT_Y 2
//#define TXT_SAVEQUIT_Y 18
//#define TXT_SAVE_EXIT_GAME_Y 38
#define TXT_CONTNOSAVE_Y 18
#else
#define TXT_SAVECONT_Y 0
//#define TXT_SAVEQUIT_Y 20
//#define TXT_SAVE_EXIT_GAME_Y 40
#define TXT_CONTNOSAVE_Y 20
#endif

#ifdef VERSION_EU
#define X_VAL9 xOffset - 12
void render_save_confirmation(s16 y, s8 *index, s16 sp6e)
#else
#define X_VAL9 x
void render_save_confirmation(s16 x, s16 y, s8 *index, s16 sp6e)
#endif
{
#ifdef VERSION_EU
    u8 textSaveAndContinueArr[][30] = {
        { TEXT_SAVE_AND_CONTINUE },
        { TEXT_SAVE_AND_CONTINUE_FR },
        { TEXT_SAVE_AND_CONTINUE_DE }
    };
    u8 textSaveAndQuitArr[][30] = {
        { TEXT_SAVE_AND_QUIT },
        { TEXT_SAVE_AND_QUIT_FR },
        { TEXT_SAVE_AND_QUIT_DE }
    };

    u8 textSaveExitGame[][30] = { // New function to exit game
        { TEXT_SAVE_EXIT_GAME },
        { TEXT_SAVE_EXIT_GAME_FR },
        { TEXT_SAVE_EXIT_GAME_DE }
    };

    u8 textContinueWithoutSaveArr[][30] = {
        { TEXT_CONTINUE_WITHOUT_SAVING },
        { TEXT_CONTINUE_WITHOUT_SAVING_FR },
        { TEXT_CONTINUE_WITHOUT_SAVING_DE }
    };

#define textSaveAndContinue textSaveAndContinueArr[gInGameLanguage]
#define textSaveAndQuit textSaveAndQuitArr[gInGameLanguage]
#define textSaveExitGame textSaveExitGame[gInGameLanguage]
#define textContinueWithoutSave textContinueWithoutSaveArr[gInGameLanguage]
    s16 xOffset = get_str_x_pos_from_center(160, textContinueWithoutSaveArr[gInGameLanguage], 12.0f);
#else
    INGAME_TEXT_COPY(textSaveAndContinue, TEXT_SAVE_AND_CONTINUE, CN_LABEL_SAVE_AND_CONTINUE);
    //u8 textSaveAndQuit[] = { TEXT_SAVE_AND_QUIT };
    //u8 textSaveExitGame[] = { TEXT_SAVE_EXIT_GAME };
    INGAME_TEXT_COPY(textContinueWithoutSave, TEXT_CONTINUE_WITHOUT_SAVING, CN_LABEL_CONTINUE_WITHOUT_SAVING);
#endif

    handle_menu_scrolling(MENU_SCROLL_VERTICAL, index, 1, 2); // decreased to '2' to prevent Exit Game

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);

    print_generic_string(TXT_SAVEOPTIONS_X, y + TXT_SAVECONT_Y, textSaveAndContinue);
    //print_generic_string(TXT_SAVEOPTIONS_X, y - TXT_SAVEQUIT_Y, textSaveAndQuit);
    //print_generic_string(TXT_SAVEOPTIONS_X, y - TXT_SAVE_EXIT_GAME_Y, textSaveExitGame);
    print_generic_string(TXT_SAVEOPTIONS_X, y - TXT_CONTNOSAVE_Y, textContinueWithoutSave);

    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);

    create_dl_translation_matrix(MENU_MTX_PUSH, X_VAL9, y - ((index[0] - 1) * sp6e), 0);

    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, gDialogTextAlpha);
    gSPDisplayList(gDisplayListHead++, dl_draw_triangle);

    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

s16 render_course_complete_screen(void) {
    s16 num;
#ifdef VERSION_EU
    gInGameLanguage = eu_get_language();
#endif

    switch (gDialogBoxState) {
        case DIALOG_STATE_OPENING:
            render_course_complete_lvl_info_and_hud_str();
            if (gCourseDoneMenuTimer > 100 && gCourseCompleteCoinsEqual == 1) {
                gDialogBoxState = DIALOG_STATE_VERTICAL;
                //level_set_transition(-1, NULL);
                gDialogTextAlpha = 0;
                gDialogLineNum = 1;
            }
            break;
        case DIALOG_STATE_VERTICAL:
            shade_screen();
            render_course_complete_lvl_info_and_hud_str();
#ifdef VERSION_EU
            render_save_confirmation(86, &gDialogLineNum, 20);
#else
            render_save_confirmation(100, 86, &gDialogLineNum, 20);
#endif
            if (gCourseDoneMenuTimer > 110
                && (gPlayer1Controller->buttonPressed & A_BUTTON
                 || gPlayer1Controller->buttonPressed & START_BUTTON
#ifdef VERSION_EU
                 || gPlayer1Controller->buttonPressed & Z_TRIG
#endif
                )) {
                level_set_transition(0, NULL);
                play_sound(SOUND_MENU_STAR_SOUND, gGlobalSoundSource);
                gDialogBoxState = DIALOG_STATE_OPENING;
                gMenuMode = -1;
                num = gDialogLineNum;
                gCourseDoneMenuTimer = 0;
                gCourseCompleteCoins = 0;
                gCourseCompleteCoinsEqual = 0;
                gHudFlash = 0;

                return num;
            }
            break;
    }

    if (gDialogTextAlpha < 250) {
        gDialogTextAlpha += 25;
    }

    gCourseDoneMenuTimer++;

    return 0;
}

s16 render_menus_and_dialogs(void) {
    s16 mode = 0;

    create_dl_ortho_matrix();

    if (gMenuMode != -1) {
        gPrevMenuMode = gMenuMode;
        switch (gMenuMode) {
            case 0:
                mode = render_pause_courses_and_castle();
                break;
            case 1:
                mode = render_pause_courses_and_castle();
                break;
            case 2:
                mode = render_course_complete_screen();
                break;
            case 3:
                mode = render_course_complete_screen();
                break;
        }

        gDialogColorFadeTimer = (s16) gDialogColorFadeTimer + 0x1000;
    } else if (gDialogID != DIALOG_NONE) {
        // The Peach "Dear Mario" message needs to be repositioned separately
        if (gDialogID == DIALOG_020) {
            print_peach_letter_message();
            return mode;
        }

        render_dialog_entries();
        gDialogColorFadeTimer = (s16) gDialogColorFadeTimer + 0x1000;
    }
    return mode;
}

void set_min_dialog_width(s16 width) {
    gDialogMinWidth = width;
}

void set_dialog_override_pos(s16 x, s16 y) {
    gOverrideDialogPos = 1;
    gDialogOverrideX = x;
    gDialogOverrideY = y;
}

void reset_dialog_override_pos(void) {
    gOverrideDialogPos = 0;
}

void set_dialog_override_color(u8 bgR, u8 bgG, u8 bgB, u8 bgA, u8 textR, u8 textG, u8 textB, u8 textA) {
    gOverrideDialogColor = 1;
    gDialogBgColorR = bgR;
    gDialogBgColorG = bgG;
    gDialogBgColorB = bgB;
    gDialogBgColorA = bgA;
    gDialogTextColorR = textR;
    gDialogTextColorG = textG;
    gDialogTextColorB = textB;
    gDialogTextColorA = textA;
}

void reset_dialog_override_color(void) {
    gOverrideDialogColor = 0;
}

void set_dialog_box_state(u8 state) {
    if (state > DIALOG_STATE_CLOSING) { return; }
    gDialogBoxState = state;
}
