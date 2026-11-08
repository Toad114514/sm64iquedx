// == dialog ==
// (defines en_dialog_table etc.)

#if defined(SM64COOPDX_CN_TEXT)
// Simplified-Chinese (iQue) text pack: real two-byte CN glyph data.
// Generated from the official n64decomp/sm64 text/cn sources.
//
// Keeping the name dialog_text_<id> for the ROM-backed vanilla buffer lets
// dialog_assets.inc.c load into it unchanged; the CN arrays get their own
// cn_dialog_<id> names and apply_cn_text_pack() picks which one is live.
#include "text/cn_dialogs_cn.inc.c"

// Vanilla ROM-backed text buffers (filled by dialog_assets.inc.c below).
#define DEFINE_DIALOG(id, _1, _2, _3, _4, _len) \
    static u8 dialog_text_ ## id[_len] = { 0xFF };
#else
#define DEFINE_DIALOG(id, _1, _2, _3, _4, _len) \
    static u8 dialog_text_ ## id[_len] = { 0xFF };
#endif

#include "dialogs.h"

#undef DEFINE_DIALOG

#if defined(SM64COOPDX_CN_TEXT)
// Registry of every CN dialog buffer, so cn_is_cn_string() can tell "this
// pointer is CN text" without having to reach into the dialog entries (which
// change at runtime via apply_cn_text_pack and Lua overrides).
const u8 *const cn_dialog_registry[] = {
#define DEFINE_DIALOG(id, _1, _2, _3, _4, _5) cn_dialog_ ## id,
#include "dialogs.h"
    NULL
};
#endif

#undef DEFINE_DIALOG

#if defined(SM64COOPDX_CN_TEXT)
// Non-const so apply_cn_text_pack() can repoint .str at runtime
// (DialogEntry.str is already a mutable pointer to const data) and switch
// .linesPerBox between the vanilla and iQue page heights.
//
// The initial value stays the vanilla number and apply_cn_text_pack() corrects
// it, because where the two disagree (DIALOG_005 3->4, DIALOG_117 1->2) the
// whole box depends on it: the height scale, the page-break line and the
// scissor window are all derived from dialog->linesPerBox. Keeping the US value
// drew a box one line too short, so the last line of text hung out of it.
#define DEFINE_DIALOG(id, unused, linesPerBox, leftOffset, width, _) \
    static struct DialogEntry dialog_entry_orig_ ## id = { \
        unused, linesPerBox, leftOffset, width, cn_dialog_ ## id, NULL, false \
    };
#else
#define DEFINE_DIALOG(id, unused, linesPerBox, leftOffset, width, _) \
    static const struct DialogEntry dialog_entry_orig_ ## id = { \
        unused, linesPerBox, leftOffset, width, dialog_text_ ## id, NULL, false \
    };
#endif

#include "dialogs.h"

#undef DEFINE_DIALOG

#if defined(SM64COOPDX_CN_TEXT)
// Non-zero while the Chinese (iQue) pack supplies the dialog / course / act
// text. Set from configChineseText before the tables are first used.
u8 gCnTextPackActive = 1;
#endif
#define DEFINE_DIALOG(id, _1, _2, _3, _4, _5) &dialog_entry_orig_ ## id,

const struct DialogEntry *const seg2_dialog_original[] = {
#include "dialogs.h"
    NULL
};

#undef DEFINE_DIALOG

// == courses ==
// (defines en_course_name_table etc.)
// The game duplicates this in levels/menu/leveldata.c in EU, so we split
// it out into a separate include file.

#define COURSE_TABLE seg2_course_name_table
#include "define_courses.inc.c"

// == acts ==
// (defines en_act_name_table etc.)

#if defined(SM64COOPDX_CN_TEXT)
// CN mode emits ROM-backed vanilla buffers alongside the CN data so
// apply_cn_text_pack() can switch the act-name table at runtime. The macro
// args are strings in the CN course source, so the CN path must not use them
// as array bounds.
#define COURSE_ACTS(id, name, a,b,c,d,e,f) \
    static u8 act_name_ ## id ## _1[a] = { 0xFF }; \
    static u8 act_name_ ## id ## _2[b] = { 0xFF }; \
    static u8 act_name_ ## id ## _3[c] = { 0xFF }; \
    static u8 act_name_ ## id ## _4[d] = { 0xFF }; \
    static u8 act_name_ ## id ## _5[e] = { 0xFF }; \
    static u8 act_name_ ## id ## _6[f] = { 0xFF };
#else
#define COURSE_ACTS(id, name, a,b,c,d,e,f) \
    static u8 act_name_ ## id ## _1[a] = { 0xFF }; \
    static u8 act_name_ ## id ## _2[b] = { 0xFF }; \
    static u8 act_name_ ## id ## _3[c] = { 0xFF }; \
    static u8 act_name_ ## id ## _4[d] = { 0xFF }; \
    static u8 act_name_ ## id ## _5[e] = { 0xFF }; \
    static u8 act_name_ ## id ## _6[f] = { 0xFF };
#endif

#define SECRET_STAR(id, name)
#define CASTLE_SECRET_STARS(str)

#undef EXTRA_TEXT
#define EXTRA_TEXT(id, str) \
    static u8 extra_text_ ## id[str] = { 0xFF };

#include "courses.h"

#undef COURSE_ACTS
#undef EXTRA_TEXT

#if defined(SM64COOPDX_CN_TEXT)
// COURSE_BOB etc. are enum constants (not macros), so they cannot be pasted
// with ##. CN_CID_<token> (defined in define_courses.inc.c) expands to the
// lower-case name used by the generated CN arrays. Three macro levels are
// needed because the middle one is only reached as a ## operand.
#define CN_ACT_PTR__(id, i) ((u8 *)(void *)&cn_course_ ## id ## _ ## i)
#define CN_ACT_PTR_(id, i)  CN_ACT_PTR__(id, i)
#define CN_ACT_PTR(id, i)   CN_ACT_PTR_(CN_CID_ ## id, i)
#define COURSE_ACTS(id, name, a,b,c,d,e,f) \
    CN_ACT_PTR(id, 1), CN_ACT_PTR(id, 2), CN_ACT_PTR(id, 3), \
    CN_ACT_PTR(id, 4), CN_ACT_PTR(id, 5), CN_ACT_PTR(id, 6),
#else
#define COURSE_ACTS(id, name, a,b,c,d,e,f) \
    (u8 *)(void *)act_name_ ## id ## _1, (u8 *)(void *)act_name_ ## id ## _2, (u8 *)(void *)act_name_ ## id ## _3, \
    (u8 *)(void *)act_name_ ## id ## _4, (u8 *)(void *)act_name_ ## id ## _5, (u8 *)(void *)act_name_ ## id ## _6,
#endif
#define EXTRA_TEXT(id, str) (u8 *)(void *)extra_text_ ## id,

// Active table. Non-const so apply_cn_text_pack() can swap each slot between
// the CN array and the vanilla ROM-backed buffer.
u8* seg2_act_name_table[] = {
#include "courses.h"
    NULL
};

// Vanilla (ROM-backed) mirror: always the "off" side of the switch. In
// non-CN builds this is identical to seg2_act_name_table.
//
// The macro redefinition below is load-bearing, not redundant with the #else
// branch above. Both this table and seg2_act_name_table[] expand the *same*
// courses.h, and the active table's COURSE_ACTS (line 123) resolves to the CN
// pointers in a CN build. Without an explicit #undef + redefinition here, this
// "vanilla" mirror would silently be built from the CN pointers too, making
// both sides of the runtime switch identical: apply_cn_text_pack() would
// install CN act names whether the pack is on or off, and -- because
// cn_is_cn_string() only trusts these pointers while gCnTextPackActive is set
// -- the same bytes would then be walked as vanilla single-byte text with the
// switch off, stopping at the first 0xFF.
#undef COURSE_ACTS
#undef EXTRA_TEXT
#define COURSE_ACTS(id, name, a,b,c,d,e,f) \
    (u8 *)(void *)act_name_ ## id ## _1, (u8 *)(void *)act_name_ ## id ## _2, (u8 *)(void *)act_name_ ## id ## _3, \
    (u8 *)(void *)act_name_ ## id ## _4, (u8 *)(void *)act_name_ ## id ## _5, (u8 *)(void *)act_name_ ## id ## _6,
#define EXTRA_TEXT(id, str) (const u8 *)(void *)extra_text_ ## id,

const u8 *const cn_act_name_vanilla[] = {
#include "courses.h"
    NULL
};

#if defined(SM64COOPDX_CN_TEXT)
// CN mirror: the "on" side of the switch.
#undef COURSE_ACTS
#undef EXTRA_TEXT
const u8 *const cn_act_name_cn[] = {
#define COURSE_ACTS(id, name, a,b,c,d,e,f) \
    CN_ACT_PTR(id, 1), CN_ACT_PTR(id, 2), CN_ACT_PTR(id, 3), \
    CN_ACT_PTR(id, 4), CN_ACT_PTR(id, 5), CN_ACT_PTR(id, 6),
#define EXTRA_TEXT(id, str) extra_text_ ## id,

#include "courses.h"
    NULL
};

#undef COURSE_ACTS
#undef EXTRA_TEXT
#endif

// The "original" table keeps the vanilla pointers in every build so Lua and
// the dialog restore path always see unmodified text.
#define COURSE_ACTS(id, name, a,b,c,d,e,f) \
    act_name_ ## id ## _1, act_name_ ## id ## _2, act_name_ ## id ## _3, \
    act_name_ ## id ## _4, act_name_ ## id ## _5, act_name_ ## id ## _6,
#define EXTRA_TEXT(id, str) extra_text_ ## id,

const u8* seg2_act_name_table_original[] = {
#include "courses.h"
    NULL
};

#undef COURSE_ACTS
#undef EXTRA_TEXT

// Vanilla / CN text is compiled in for both packs; the ROM loads below only
// fill the vanilla buffers, so they must always be emitted in CN builds too.
// act_name_<id>_N and the sized course-name placeholders exist in both modes.
#include "dialog_assets.inc.c"
#include "courses_assets.inc.c"

#if defined(SM64COOPDX_CN_TEXT)
// Repoint the active dialog entries and course/act name slots at the pack
// selected by gCnTextPackActive. Safe to call repeatedly.
//
// IMPORTANT invariant for seg2_course_name_table[] / seg2_act_name_table[]:
// the pointers installed here are *static* arrays (.rodata / .data), NOT heap
// allocations. The Lua text layer (smlua_text_utils.c) frees a table slot when
// it differs from the original before replacing it, so it must only free
// pointers it allocated itself. It keeps an explicit ownership set for exactly
// this reason: free() on one of these static pointers aborts immediately with
// "free(): invalid size", which is what happened whenever a mod was activated
// (hosting or joining a room calls smlua_init() -> smlua_text_utils_reset_all()).
// If you ever make these pointers heap-allocated, update that ownership set too.
void apply_cn_text_pack(void) {
    // dialogs: text pointer AND page height. Both have to follow the same
    // switch -- a CN string paginated with the US line count overflows the box.
// NOTE: the third macro parameter must NOT be named `linesPerBox`. The body
// below also reads *and* writes the struct field of that name, and the C
// preprocessor replaces every occurrence of a parameter name in the macro body
// -- `.linesPerBox` would be rewritten to `.4` (the argument) and the file
// would fail to compile. Hence `_lines`.
#define DEFINE_DIALOG(id, _1, _lines, _3, _4, _5) \
    dialog_entry_orig_ ## id.str = gCnTextPackActive ? cn_dialog_ ## id : dialog_text_ ## id; \
    dialog_entry_orig_ ## id.linesPerBox = gCnTextPackActive ? cn_dialog_lines_ ## id : (_lines);

#include "dialogs.h"

#undef DEFINE_DIALOG

    // course + act name tables
    {
        extern u8 *seg2_course_name_table[];
        extern u8 *seg2_act_name_table[];
        extern const u8 *const cn_course_name_vanilla[];
        extern const u8 *const cn_act_name_vanilla[];
        extern const u8 *const cn_course_name_cn[];
        extern const u8 *const cn_act_name_cn[];
        u32 i;

        // Hard upper bounds: die loudly instead of walking off the end of the
        // live table if a NULL sentinel is ever missing. A missing sentinel
        // here silently writes pointers over adjacent .data and shows up much
        // later as "free(): invalid size", which is very hard to trace back.
        #define CN_COURSE_SLOTS 26
        #define CN_ACT_SLOTS    200

        for (i = 0; i < CN_COURSE_SLOTS && cn_course_name_vanilla[i] != NULL; i++) {
            seg2_course_name_table[i] = (u8 *)cn_course_name_vanilla[i];
        }
        for (i = 0; i < CN_ACT_SLOTS && cn_act_name_vanilla[i] != NULL; i++) {
            seg2_act_name_table[i] = (u8 *)cn_act_name_vanilla[i];
        }

        if (gCnTextPackActive) {
            // overlay the CN pointers on top of the vanilla defaults
            for (i = 0; i < CN_COURSE_SLOTS && cn_course_name_cn[i] != NULL; i++) {
                seg2_course_name_table[i] = (u8 *)cn_course_name_cn[i];
            }
            for (i = 0; i < CN_ACT_SLOTS && cn_act_name_cn[i] != NULL; i++) {
                seg2_act_name_table[i] = (u8 *)cn_act_name_cn[i];
            }
        }

        #undef CN_COURSE_SLOTS
        #undef CN_ACT_SLOTS
    }

    // The dialog entries the renderer actually uses live in a heap copy made by
    // dialog_table_init(), which ran before this function ever did -- so those
    // copies still carry the static initialiser's US linesPerBox. Push the
    // freshly switched values (str *and* page height) into them, otherwise the
    // CN text is paginated with the US line count and overflows the box.
    // Declared here rather than included: this file is textconv'd and included
    // from segment2.c, which has no reason to pull in the PC dialog table.
    {
        extern void dialog_table_sync_vanilla(void);
        dialog_table_sync_vanilla();
    }
}
#endif
