#include "dialog_table.h"
#include "pc/lua/smlua.h"
#include "dialog_ids.h"
#include "PR/ultratypes.h"
#include "game/segment2.h"
#include "pc/lua/utils/smlua_text_utils.h"
#include "game/memory.h"
#include "game/level_info.h"
#include "pc/platform.h"
#include <stdlib.h>
#include <string.h>

DialogTable *gDialogTable = NULL;

void dialog_table_init(void) {
    gDialogTable = growing_array_init(gDialogTable, 256, malloc, free);

    for (u32 i = 0; i < DIALOG_COUNT; i++) {
        const struct DialogEntry* dialogOrig = smlua_text_utils_dialog_get_unmodified(i);
        struct DialogEntry* dialog = dialog_table_alloc(NULL);

        if (!dialog) {
            sys_fatal("Failed to allocate DialogEntry for dialog ID %d", i);
        }

        memcpy(dialog, dialogOrig, sizeof(struct DialogEntry));
        dialog->text = convert_string_sm64_to_ascii(NULL, dialog->str);
    }
}

struct DialogEntry* dialog_table_alloc(s32 *dialogId) {
    DialogTable *table = gDialogTable;

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

struct DialogEntry* dialog_table_get(s32 dialogId) {
    if (!IS_VALID_DIALOG(dialogId)) {
        return NULL;
    }

    return gDialogTable->buffer[dialogId];
}

// Re-copy every vanilla entry from seg2_dialog_original[].
//
// dialog_table_init() runs before chinese_text_pack_init(), so its memcpy sees
// the *static initialiser* values: in a CN build that is the Chinese string but
// the US linesPerBox (the initialiser comes from dialogs.h, which does not know
// about the pack). apply_cn_text_pack() only rewrites the originals, never
// these heap copies, so the renderer kept paginating with 3 lines for a dialog
// iQue gives 4 -- the race with Koopa the Quick overflowed its box and broke
// two lines early. The same staleness made the runtime toggle look like a
// no-op for dialogs (the copies still pointed at the previous set).
//
// Entries flagged `replaced` belong to the Lua text layer and are left alone.
void dialog_table_sync_vanilla(void) {
    u32 i;

    if (gDialogTable == NULL) { return; }

    for (i = 0; i < DIALOG_COUNT && i < gDialogTable->count; i++) {
        struct DialogEntry *dialog = gDialogTable->buffer[i];
        const struct DialogEntry *orig;
        char *oldText;

        if (dialog == NULL || dialog->replaced) { continue; }

        orig = smlua_text_utils_dialog_get_unmodified(i);
        if (orig == NULL) { continue; }

        // `str` belongs to the original (static data) -- only `text` was
        // allocated by convert_string_sm64_to_ascii().
        oldText = dialog->text;
        memcpy(dialog, orig, sizeof(struct DialogEntry));
        dialog->text = convert_string_sm64_to_ascii(NULL, dialog->str);
        free(oldText);
    }
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

    if (!table) return;

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