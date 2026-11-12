#include "sm64.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include "course_table.h"
#include "game/memory.h"
#include "game/ingame_menu.h"
#include "game/save_file.h"
#include "game/segment2.h"
#include "game/level_info.h"
#include "pc/pc_main.h"
#include "../smlua.h"
#include "smlua_level_utils.h"
#include "smlua_text_utils.h"
#include "pc/dialog_table.h"

#ifdef VERSION_EU
extern s32 gInGameLanguage;
#include "eu_translation.h"
#endif

#define INVALID_COURSE_NUM(courseNum) (smlua_level_util_get_info_from_course_num(courseNum) == NULL && !COURSE_IS_VALID_COURSE(courseNum))

/*
---------------------------------------------------
Mapping gReplacedCourseActNameTable <-> seg2 tables
---------------------------------------------------

For courseNum from COURSE_BOB to COURSE_RR:
- gReplacedCourseActNameTable[courseNum].courseName     <->  seg2_course_name_table[courseNum - 1]
- gReplacedCourseActNameTable[courseNum].actName[0..5]  <->  seg2_act_name_table[(courseNum - 1) * 6 + (0..5)]
  (7th act name doesn't map to anything, but can be retrieved with `smlua_text_utils_act_name_get`)

For courseNum from COURSE_BITDW to COURSE_CAKE_END:
- gReplacedCourseActNameTable[courseNum].courseName  <->  seg2_course_name_table[courseNum - 1]
- gReplacedCourseActNameTable[courseNum].actName[0..6] don't map to anything, but they can be retrieved with `smlua_text_utils_act_name_get`

For castle secret stars and extra text:
- gReplacedCourseActNameTable[COURSE_END].courseName     <->  seg2_course_name_table[COURSE_MAX] (which is seg2_course_name_table_castle_secret_stars)
- gReplacedCourseActNameTable[COURSE_END].actName[0..6]  <->  seg2_act_name_table[COURSE_STAGES_MAX * 6 + (0..6)] (which are extra_text_0~6)
*/
struct CourseActNames gReplacedCourseActNameTable[COURSE_END + 1] = { 0 };

static bool sSmluaTextUtilsInited = false;

#define smlua_text_utils_init_from_vanilla(obj, tableFunc, tableOrigFunc, tableOffset) { \
    char buffer[50]; \
    convert_string_sm64_to_ascii(buffer, tableOrigFunc()[tableOffset]); \
    snprintf(obj.name.value, sizeof(obj.name.value), "%s", buffer); \
    obj.name.get_table = tableFunc; \
    obj.name.offset = tableOffset; \
    snprintf(obj.orig.value, sizeof(obj.orig.value), "%s", buffer); \
    obj.orig.get_table = tableOrigFunc; \
    obj.orig.offset = tableOffset; \
    tableFunc()[tableOffset] = tableOrigFunc()[tableOffset]; \
}

// Save all vanilla act names and course names
void smlua_text_utils_init(void) {
    dialog_table_init();
    memset(gReplacedCourseActNameTable, 0, sizeof(gReplacedCourseActNameTable));

    // Vanilla courses
    for (s16 courseNum = COURSE_MIN; courseNum <= COURSE_MAX; ++courseNum) {
        smlua_text_utils_init_from_vanilla(
            gReplacedCourseActNameTable[courseNum].courseName,
            get_course_name_table,
            get_course_name_table_original,
            courseNum - 1
        );

        // Vanilla acts
        if (COURSE_IS_MAIN_COURSE(courseNum)) {
            for (s16 actIndex = 0; actIndex < MAX_ACTS; ++actIndex) {
                smlua_text_utils_init_from_vanilla(
                    gReplacedCourseActNameTable[courseNum].actName[actIndex],
                    get_act_name_table,
                    get_act_name_table_original,
                    (courseNum - 1) * MAX_ACTS + actIndex
                );
            }
        }
    }

    // Vanilla castle secrets and extras
    smlua_text_utils_init_from_vanilla(
        gReplacedCourseActNameTable[COURSE_END].courseName,
        get_course_name_table,
        get_course_name_table_original,
        COURSE_MAX
    );
    for (s16 actIndex = 0; actIndex < MAX_ACTS_AND_100_COINS; ++actIndex) {
        smlua_text_utils_init_from_vanilla(
            gReplacedCourseActNameTable[COURSE_END].actName[actIndex],
            get_act_name_table,
            get_act_name_table_original,
            COURSE_STAGES_MAX * MAX_ACTS + actIndex
        );
    }

    sSmluaTextUtilsInited = true;
}

void smlua_text_utils_shutdown(void) {
    if (sSmluaTextUtilsInited) {
        sSmluaTextUtilsInited = false;
    }
}

static u8* smlua_text_utils_convert(const char* str) {
    return convert_string_ascii_to_sm64(NULL, str, false);
}

// Checks the first 3 characters
static bool str_starts_with_spaces(const char* str) {
    for (u8 i = 0; i < 3; i++) {
        if (str[i] != ' ') { return false; }
    }
    return true;
}

// Statically-typed text tables (the vanilla ROM-backed buffers and the CN
// iQue arrays) live in .rodata / .bss. The course/act name slots in
// seg2_course_name_table[] can legitimately point at them (for example after
// the Chinese text pack installs its arrays), so "pointer differs from the
// original" no longer implies "this pointer came from malloc".
//
// Track the handful of strings this file actually allocates so we never hand a
// static pointer to free(). Freeing a non-heap pointer aborts with
// "free(): invalid size", which is otherwise very hard to trace back here.
#define SMLUA_OWNED_TEXT_MAX 512
static void *sSmluaOwnedText[SMLUA_OWNED_TEXT_MAX];
static u32 sSmluaOwnedTextCount = 0;

static void smlua_text_utils_register_owned(void *ptr) {
    if (ptr == NULL) { return; }
    if (sSmluaOwnedTextCount >= SMLUA_OWNED_TEXT_MAX) {
        // Should never happen; leaking one string beats corrupting the heap.
        LOG_LUA_WARNING("smlua text ownership table full, leaking pointer");
        return;
    }
    sSmluaOwnedText[sSmluaOwnedTextCount++] = ptr;
}

static void smlua_text_utils_free_if_owned(void *ptr) {
    u32 i;

    if (ptr == NULL) { return; }

    for (i = 0; i < sSmluaOwnedTextCount; i++) {
        if (sSmluaOwnedText[i] == ptr) {
            free(ptr);
            sSmluaOwnedText[i] = sSmluaOwnedText[--sSmluaOwnedTextCount];
            return;
        }
    }

    // Not ours: a static vanilla / CN table pointer. Leave it alone; the
    // caller restores the original pointer immediately after this returns.
}

static void smlua_text_utils_reset_course_or_act_name(struct ReplacedName *name) {
    if (name->name.get_table && name->orig.get_table) {
        void **tblName = name->name.get_table() + name->name.offset;
        void **tblOrig = name->orig.get_table() + name->orig.offset;
        if (*tblName != *tblOrig) {
            smlua_text_utils_free_if_owned(*tblName);
            *tblName = *tblOrig;
        }
    }
    memcpy(name->name.value, name->orig.value, sizeof(name->name.value));
    name->modNum = 0;
}

static void smlua_text_utils_replace_course_or_act_name(struct ReplacedName *name, const char *replacement, s32 modIndex, bool removeLeadingSpaces) {
    if (removeLeadingSpaces) { replacement += 3 * str_starts_with_spaces(replacement); }
    if (name->name.get_table && name->orig.get_table) {
        void **tblName = name->name.get_table() + name->name.offset;
        void **tblOrig = name->orig.get_table() + name->orig.offset;
        if (*tblName != *tblOrig) {
            smlua_text_utils_free_if_owned(*tblName);
        }
        *tblName = smlua_text_utils_convert(replacement);
        smlua_text_utils_register_owned(*tblName);
    }
    snprintf(name->name.value, sizeof(name->name.value), "%s", replacement);
    name->modNum = modIndex + 1;
}

void smlua_text_utils_reset_all(void) {
    dialog_table_reset();

    if (sSmluaTextUtilsInited) {
        for (s16 courseNum = 0; courseNum <= COURSE_END; courseNum++) {

            // Restore vanilla course names
            struct ReplacedName *courseName = &gReplacedCourseActNameTable[courseNum].courseName;
            smlua_text_utils_reset_course_or_act_name(courseName);

            // Restore vanilla act names
            for (s16 actIndex = 0; actIndex < MAX_ACTS_AND_100_COINS; actIndex++) {
                struct ReplacedName *actName = &gReplacedCourseActNameTable[courseNum].actName[actIndex];
                smlua_text_utils_reset_course_or_act_name(actName);
            }
        }
    } else {
        dialog_table_shutdown();
    }

#if defined(SM64COOPDX_CN_TEXT)
    // Resetting the course/act name slots writes the *vanilla* rom-backed
    // pointers back into seg2_course_name_table[] / seg2_act_name_table[] --
    // see smlua_text_utils_init_from_vanilla(), which snapshots
    // tableOrigFunc() and assigns it into tableFunc(). That silently undoes
    // apply_cn_text_pack().
    //
    // This is not only a shutdown-time problem: smlua_init() starts with
    // smlua_shutdown(), and it is called again on every network join
    // (network.c, packet_join.c) -- i.e. long after chinese_text_pack_init()
    // ran during startup. Re-apply the pack here so the Chinese text survives
    // any reset regardless of when it happens. apply_cn_text_pack() only
    // rewrites the tables, so calling it when the pack is off is a no-op.
    apply_cn_text_pack();
#endif
}

struct DialogEntry* smlua_text_utils_dialog_get(enum DialogId dialogId) {
    struct DialogEntry* dialog = dialog_table_get(dialogId);
    return dialog;
}

const struct DialogEntry* smlua_text_utils_dialog_get_unmodified(enum DialogId dialogId) {
    if (!IS_VALID_VANILLA_DIALOG(dialogId)) { return NULL; }

    void **dialogTableOrg;

#ifdef VERSION_EU
    switch (gInGameLanguage) {
        case LANGUAGE_ENGLISH:
            dialogTableOrg = segmented_to_virtual(dialog_table_eu_en_original);
            break;
        case LANGUAGE_FRENCH:
            dialogTableOrg = segmented_to_virtual(dialog_table_eu_fr_original);
            break;
        case LANGUAGE_GERMAN:
            dialogTableOrg = segmented_to_virtual(dialog_table_eu_de_original);
            break;
    }
#else
    dialogTableOrg = segmented_to_virtual(seg2_dialog_original);
#endif

    return segmented_to_virtual(dialogTableOrg[dialogId]);
}

// True when this call cannot change anything: same text, same layout.
//
// Why it has to exist: several mods (Character Select, notably) read
// `DialogEntry.text` in HOOK_ON_DIALOG and feed it straight back into
// dialog_replace(), optionally through a gsub that matches nothing. That is
// semantically a no-op, but the pack cannot survive the round trip on the byte
// level -- a CN string is a two-byte token stream, and
// convert_string_sm64_to_ascii() deliberately reports it as an empty string
// (there is no ASCII image of it). So the mod would hand us "" and
// smlua_text_utils_convert() would install a genuinely empty string, replacing
// the Chinese text with nothing: the dialog box then draws with the right
// height and no glyphs at all, which is what a signpost looked like with
// Character Select enabled (upstream issue #3, follow-up).
//
// Comparing against the live entry instead of against "did the pack produce
// this" keeps it honest: if a mod really does want different text, the strings
// differ and the override goes through as before.
static bool dialog_replace_is_noop(const struct DialogEntry *dialog, u32 unused,
                                   s8 linesPerBox, s16 leftOffset, s16 width,
                                   const char *str) {
    if (dialog->text == NULL || str == NULL) { return false; }
    return dialog->unused == unused
        && dialog->linesPerBox == linesPerBox
        && dialog->leftOffset == leftOffset
        && dialog->width == width
        && strcmp(dialog->text, str) == 0;
}

void smlua_text_utils_dialog_replace(enum DialogId dialogId, UNUSED u32 unused, s8 linesPerBox, s16 leftOffset, s16 width, const char* str) {
    if (!IS_VALID_DIALOG(dialogId)) { return; }

    struct DialogEntry *dialog = smlua_text_utils_dialog_get(dialogId);

    if (!dialog) { return; }

    if (dialog_replace_is_noop(dialog, unused, linesPerBox, leftOffset, width, str)) { return; }

    if (dialog->replaced) {
        free((u8*)dialog->str);
    }

    free(dialog->text);

    dialog->unused = unused;
    dialog->linesPerBox = linesPerBox;
    dialog->leftOffset = leftOffset;
    dialog->width = width;
    dialog->str = smlua_text_utils_convert(str);
    dialog->text = strdup(str);
    dialog->replaced = true;
}

void smlua_text_utils_dialog_restore(enum DialogId dialogId) {
    if (!IS_VALID_VANILLA_DIALOG(dialogId)) { return; }

    struct DialogEntry *dialog = smlua_text_utils_dialog_get(dialogId);

    if (!dialog->replaced) return;

    const struct DialogEntry *dialogOrig = smlua_text_utils_dialog_get_unmodified(dialogId);

    free((u8*)dialog->str);
    free(dialog->text);

    memcpy(dialog, dialogOrig, sizeof(struct DialogEntry));
    dialog->text = convert_string_sm64_to_ascii(NULL, dialog->str);
}

bool smlua_text_utils_dialog_is_replaced(enum DialogId dialogId) {
    if (!IS_VALID_DIALOG(dialogId)) { return false; }

    struct DialogEntry *dialog = dialog_table_get(dialogId);
    return dialog->replaced;
}

s32 smlua_text_utils_allocate_dialog(void) {
    s32 dialogId;
    struct DialogEntry* dialog = dialog_table_alloc(&dialogId);

    if (dialog) {
        dialog->replaced = true;
    }

    return dialogId;
}

void smlua_text_utils_course_acts_replace(s16 courseNum, const char* courseName, const char* act1, const char* act2, const char* act3, const char* act4, const char* act5, const char* act6) {
    if (!COURSE_IS_VALID_COURSE(courseNum)) { return; }

    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[courseNum];
    smlua_text_utils_replace_course_or_act_name(&courseActNames->courseName, courseName + (3 * (strlen(courseName) > 3)), gLuaActiveMod->index, false);

#define REPLACE_ACT_NAME(i) { \
        smlua_text_utils_replace_course_or_act_name(&courseActNames->actName[i - 1], act##i, gLuaActiveMod->index, false); \
    }

    REPLACE_ACT_NAME(1);
    REPLACE_ACT_NAME(2);
    REPLACE_ACT_NAME(3);
    REPLACE_ACT_NAME(4);
    REPLACE_ACT_NAME(5);
    REPLACE_ACT_NAME(6);
}

void smlua_text_utils_course_name_replace(s16 courseNum, const char* name) {
    if (!COURSE_IS_VALID_COURSE(courseNum)) { return; }

    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[courseNum];
    smlua_text_utils_replace_course_or_act_name(&courseActNames->courseName, name, gLuaActiveMod->index, false);
}

const char* smlua_text_utils_course_name_get(s16 courseNum) {
    if (!COURSE_IS_VALID_COURSE(courseNum)) { return NULL; }

    return gReplacedCourseActNameTable[courseNum].courseName.name.value;
}

s32 smlua_text_utils_course_name_mod_index(s16 courseNum) {
    if (!COURSE_IS_VALID_COURSE(courseNum)) { return -1; }

    return (s32) gReplacedCourseActNameTable[courseNum].courseName.modNum - 1;
}

void smlua_text_utils_course_name_reset(s16 courseNum) {
    if (!COURSE_IS_VALID_COURSE(courseNum)) { return; }

    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[courseNum];
    smlua_text_utils_reset_course_or_act_name(&courseActNames->courseName);
}

void smlua_text_utils_act_name_replace(s16 courseNum, u8 actNum, const char* name) {
    if (!COURSE_IS_VALID_COURSE(courseNum)) { return; }
    if (actNum < 1 || actNum > MAX_ACTS_AND_100_COINS) { return; }

    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[courseNum];
    smlua_text_utils_replace_course_or_act_name(&courseActNames->actName[actNum - 1], name, gLuaActiveMod->index, false);
}

const char* smlua_text_utils_act_name_get(s16 courseNum, u8 actNum) {
    if (!COURSE_IS_VALID_COURSE(courseNum)) { return NULL; }
    if (actNum < 1 || actNum > MAX_ACTS_AND_100_COINS) { return NULL; }

    return gReplacedCourseActNameTable[courseNum].actName[actNum - 1].name.value;
}

s32 smlua_text_utils_act_name_mod_index(s16 courseNum, u8 actNum) {
    if (!COURSE_IS_VALID_COURSE(courseNum)) { return -1; }
    if (actNum < 1 || actNum > MAX_ACTS_AND_100_COINS) { return -1; }

    return (s32) gReplacedCourseActNameTable[courseNum].actName[actNum - 1].modNum - 1;
}

void smlua_text_utils_act_name_reset(s16 courseNum, u8 actNum) {
    if (!COURSE_IS_VALID_COURSE(courseNum)) { return; }
    if (actNum < 1 || actNum > MAX_ACTS_AND_100_COINS) { return; }

    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[courseNum];
    smlua_text_utils_reset_course_or_act_name(&courseActNames->actName[actNum - 1]);
}

void smlua_text_utils_secret_star_replace(s16 courseNum, const char* courseName) {
    if (courseNum <= COURSE_STAGES_MAX || courseNum > COURSE_MAX) { return; }

    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[courseNum];
    smlua_text_utils_replace_course_or_act_name(&courseActNames->courseName, courseName, gLuaActiveMod->index, true);
}

void smlua_text_utils_castle_secret_stars_replace(const char* name) {
    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[COURSE_END];
    smlua_text_utils_replace_course_or_act_name(&courseActNames->courseName, name, gLuaActiveMod->index, false);
}

const char* smlua_text_utils_castle_secret_stars_get() {
    return gReplacedCourseActNameTable[COURSE_END].courseName.name.value;
}

s32 smlua_text_utils_castle_secret_stars_mod_index() {
    return (s32) gReplacedCourseActNameTable[COURSE_END].courseName.modNum - 1;
}

void smlua_text_utils_castle_secret_stars_reset() {
    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[COURSE_END];
    smlua_text_utils_reset_course_or_act_name(&courseActNames->courseName);
}

void smlua_text_utils_extra_text_replace(s16 index, const char* text) {
    if (index < 0 || index > MAX_ACTS_AND_100_COINS) { return; }

    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[COURSE_END];
    smlua_text_utils_replace_course_or_act_name(&courseActNames->actName[index], text, gLuaActiveMod->index, false);
}

const char* smlua_text_utils_extra_text_get(s16 index) {
    if (index < 0 || index > MAX_ACTS_AND_100_COINS) { return NULL; }

    return gReplacedCourseActNameTable[COURSE_END].actName[index].name.value;
}

s32 smlua_text_utils_extra_text_mod_index(s16 index) {
    if (index < 0 || index > MAX_ACTS_AND_100_COINS) { return -1; }

    return (s32) gReplacedCourseActNameTable[COURSE_END].actName[index].modNum - 1;
}

void smlua_text_utils_extra_text_reset(s16 index) {
    if (index < 0 || index > MAX_ACTS_AND_100_COINS) { return; }

    struct CourseActNames *courseActNames = &gReplacedCourseActNameTable[COURSE_END];
    smlua_text_utils_reset_course_or_act_name(&courseActNames->actName[index]);
}

const char* smlua_text_utils_get_language(void) {
    return configLanguage;
}
