#include "dialog_table.h"
#include "pc/lua/smlua.h"
#include "dialog_ids.h"
#include "PR/ultratypes.h"
#include "game/segment2.h"
#include "game/cn_rom_assets.h"
#include "game/cn_font.h"
#include "game/cn_unicode.h"
#include "pc/lua/utils/smlua_text_utils.h"
#include "game/memory.h"
#include "game/level_info.h"
#include "pc/platform.h"
#include <stdlib.h>
#include <string.h>

// One dialog table.
//
// The table holds the game's dialog entries: text, page height, box geometry,
// and a `text` field that is the human-readable copy Lua sees. A mod's
// replacement lives here (smlua_replace_dialog / dialog_set), and the renderer
// draws from here -- there is no second table for the Chinese pack any more.
//
// Switching the pack on does not copy anything: it just repoints `str` at the
// UTF-8 strings converted out of the 神游 cartridge (cn_rom_dialog_str), and
// switching it off repoints them back at the ROM. That is the whole mechanism,
// which is why a mod's English replacement and the pack's Chinese strings can
// coexist in one table without either one inheriting the other's geometry.
//
// Two rules keep the state honest:
//   * an entry with `replaced` set is never touched by dialog_table_apply_cn(),
//     so whatever a mod wrote stays exactly as written (including mixed
//     Chinese/English, which the UTF-8 renderer handles natively);
//   * `linesPerBox` comes from the cartridge while the Chinese string is
//     installed, because the line breaks in that string were authored for it.
//     Every other geometry field stays vanilla: one box, one layout, no
//     "English text with Chinese spacing".

DialogTable *gDialogTable = NULL;

// Allocate one entry from `table`. Only the single table ever grows at runtime
// (Lua custom dialogs), so `dialogId` is the game's id space.
static struct DialogEntry *dialog_table_alloc_from(DialogTable *table, s32 *dialogId) {
    if (table == NULL) { return NULL; }

    if (table->count >= MAX_ALLOCATED_DIALOGS) {
        LOG_LUA_WARNING("Dialog limit reached! (%d max)", MAX_ALLOCATED_DIALOGS);
        if (dialogId) *dialogId = DIALOG_NONE;
        return NULL;
    }

    struct DialogEntry* dialog = growing_array_alloc(table, sizeof(struct DialogEntry));

    if (!dialog) {
        LOG_LUA_WARNING("Failed to allocate DialogEntry");
        if (dialogId) *dialogId = DIALOG_NONE;
        return NULL;
    }

    if (dialogId) *dialogId = table->count - 1;

    return dialog;
}

// The Lua-facing copy of a string.
//
// UTF-8 strings (the ones converted from the cartridge, and anything a mod
// wrote in Chinese) can be handed to Lua verbatim -- that is the point of the
// encoding change: a mod reading dialog.text now sees 炸弹王国 instead of an
// empty string. Vanilla strings still go through the ASCII conversion.
static char *dialog_text_from_str(const u8 *str) {
    if (str == NULL) { return NULL; }

    if (cn_is_cn_string(str)) {
        size_t len = strlen((const char *) str);
        char *copy = (char *) malloc(len + 1);
        if (copy != NULL) { memcpy(copy, str, len + 1); }
        return copy;
    }

    return convert_string_sm64_to_ascii(NULL, str);
}

void dialog_table_init(void) {
    gDialogTable = growing_array_init(gDialogTable, 256, malloc, free);

    for (u32 i = 0; i < DIALOG_COUNT; i++) {
        const struct DialogEntry* dialogOrig = smlua_text_utils_dialog_get_unmodified(i);
        struct DialogEntry* dialog = dialog_table_alloc(NULL);

        if (!dialog) {
            sys_fatal("Failed to allocate DialogEntry for dialog ID %d", i);
        }

        memcpy(dialog, dialogOrig, sizeof(struct DialogEntry));
        dialog->text = dialog_text_from_str(dialog->str);
    }
}

struct DialogEntry* dialog_table_alloc(s32 *dialogId) {
    return dialog_table_alloc_from(gDialogTable, dialogId);
}

struct DialogEntry* dialog_table_get(s32 dialogId) {
    if (!IS_VALID_DIALOG(dialogId)) {
        return NULL;
    }

    return gDialogTable->buffer[dialogId];
}

void dialog_table_apply_cn(bool on) {
#if defined(SM64COOPDX_CN_TEXT)
    u32 i;

    if (gDialogTable == NULL) { return; }

    for (i = 0; i < DIALOG_COUNT && i < gDialogTable->count; i++) {
        struct DialogEntry *dialog = gDialogTable->buffer[i];
        const struct DialogEntry *orig = smlua_text_utils_dialog_get_unmodified(i);
        const u8 *cnStr;
        const u8 *want;

        if (dialog == NULL || orig == NULL) { continue; }

        // A mod's text is not ours to overwrite. This is what keeps a Lua
        // replacement alive across a level change with the pack on.
        if (dialog->replaced) { continue; }

        cnStr = on ? cn_rom_dialog_str(i) : NULL;
        want = (cnStr != NULL) ? cnStr : orig->str;

        dialog->unused = orig->unused;
        dialog->leftOffset = orig->leftOffset;
        dialog->width = orig->width;
        // The cartridge's line breaks were authored for its own page height;
        // take the count with the string. Everything else stays vanilla.
        dialog->linesPerBox = (cnStr != NULL) ? (s8) cn_rom_dialog_lines(i) : orig->linesPerBox;

        if (dialog->str == want) { continue; }

        dialog->str = (u8 *) want;
        free(dialog->text);
        dialog->text = dialog_text_from_str(want);
    }
#else
    (void) on;
#endif
}

void dialog_table_reset(void) {
    DialogTable *table = gDialogTable;

    if (!table) return;

    for (u32 i = 0; i < table->count; i++) {
        struct DialogEntry *dialog = table->buffer[i];

        if (!dialog->replaced) continue;

        if (!IS_CUSTOM_DIALOG(i)) {
            smlua_text_utils_dialog_restore(i);
        } else {
            free((u8*)dialog->str);
            free(dialog->text);
            free(dialog);
            table->buffer[i] = NULL;
        }
    }

    table->count = DIALOG_COUNT;
}

void dialog_table_shutdown(void) {
    DialogTable *table = gDialogTable;

    if (table) {
        dialog_table_reset();

        for (u32 i = 0; i < DIALOG_COUNT; i++) {
            struct DialogEntry *dialog = table->buffer[i];

            free(dialog->text);
            free(dialog);
        }

        free(table->buffer);
        free(table);
        gDialogTable = NULL;
    }
}
