#include "dialog_table.h"
#include "pc/lua/smlua.h"
#include "dialog_ids.h"
#include "PR/ultratypes.h"
#include "game/segment2.h"
#include "game/cn_rom_assets.h"
#include "pc/lua/utils/smlua_text_utils.h"
#include "game/memory.h"
#include "game/level_info.h"
#include "pc/platform.h"
#include <stdlib.h>
#include <string.h>

// Two dialog tables, on purpose.
//
// `gDialogTable` is the game's own table: the US text, copied out of
// seg2_dialog_original[] (which holds the vanilla ROM data in every build). The
// Lua text layer reads and writes *this* one -- smlua_text_utils_dialog_get(),
// smlua_replace_dialog(), dialog_get()/dialog_set() in Lua, Character Select's
// HOOK_ON_DIALOG round trip. A mod's replacement lives here and nowhere else.
//
// `sDialogTableCn` is the iQue pack's table: strings, page heights and box
// geometry straight out of the 神游 rom. Nothing writes to it except
// dialog_table_refresh_cn(), and no mod can reach it, because every entry point
// the Lua API uses goes through dialog_table_get().
//
// Why the split (upstream request): arbitrary mod text cannot be translated, so
// while the pack shared the single table both sources had to share one entry per
// dialog. That coupling is what produced "replaced text stays English but is
// drawn with Chinese line spacing" -- the text came from the mod, the geometry
// from the pack. Two tables make the bad state unrepresentable: the renderer
// draws exactly one of them, so the string, the line pitch, the page length and
// the box height always come from the same source. The runtime switch now means
// "which table is on screen" and nothing else.
//
// Fallback rules, both deliberate:
//   * no rom (or a rom that does not cover this entry) -> the CN entry has
//     str == NULL and the renderer uses the US entry: no cartridge, no Chinese.
//   * custom dialogs (id >= DIALOG_COUNT) exist only in the US table; they are
//     mod content and the pack has nothing to say about them.

DialogTable *gDialogTable = NULL;

#if defined(SM64COOPDX_CN_TEXT)
static DialogTable *sDialogTableCn = NULL;
#endif

// Allocate one entry from `table`. Shared by both tables; only the US table ever
// grows at runtime (Lua custom dialogs), so `dialogId` is the US id space.
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

void dialog_table_init(void) {
    gDialogTable = growing_array_init(gDialogTable, 256, malloc, free);

    for (u32 i = 0; i < DIALOG_COUNT; i++) {
        const struct DialogEntry* dialogOrig = smlua_text_utils_dialog_get_unmodified(i);
        struct DialogEntry* dialog = dialog_table_alloc(NULL);

        if (!dialog) {
            sys_fatal("Failed to allocate DialogEntry for dialog ID %d", i);
        }

        memcpy(dialog, dialogOrig, sizeof(struct DialogEntry));
        // The US strings are ASCII in every build (the pack has its own table),
        // so this is a real conversion and `text` is never NULL.
        dialog->text = convert_string_sm64_to_ascii(NULL, dialog->str);
    }

#if defined(SM64COOPDX_CN_TEXT)
    sDialogTableCn = growing_array_init(sDialogTableCn, DIALOG_COUNT + 1, malloc, free);
    for (u32 i = 0; i < DIALOG_COUNT; i++) {
        struct DialogEntry *cn = dialog_table_alloc_from(sDialogTableCn, NULL);
        if (!cn) {
            sys_fatal("Failed to allocate Chinese DialogEntry for dialog ID %d", i);
        }
        memset(cn, 0, sizeof(struct DialogEntry));
    }
    dialog_table_refresh_cn();
#endif
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

#if defined(SM64COOPDX_CN_TEXT)
// Pull the pack's dialog entries out of the rom.
//
// Called from apply_cn_text_pack() (startup, the settings toggle, and every Lua
// reset) rather than once from dialog_table_init(), because the rom can finish
// loading after this table exists.
void dialog_table_refresh_cn(void) {
    u32 i;

    if (sDialogTableCn == NULL || gDialogTable == NULL) { return; }

    for (i = 0; i < DIALOG_COUNT && i < sDialogTableCn->count; i++) {
        struct DialogEntry *cn = sDialogTableCn->buffer[i];
        const struct DialogEntry *orig = smlua_text_utils_dialog_get_unmodified(i);
        const u8 *str = cn_rom_dialog_str(i);

        if (cn == NULL || orig == NULL) { continue; }

        cn->unused = orig->unused;
        // .text stays NULL: it is the Lua-facing ASCII view of a string, and Lua
        // only ever sees gDialogTable (see the note at the top of the file).
        cn->text = NULL;
        cn->replaced = false;
        cn->str = str;

        if (str != NULL) {
            // The iQue table carries its own geometry per entry (DIALOG_005 is
            // 4 lines, DIALOG_117 is 2). The box height, the page break and the
            // scissor window are all derived from linesPerBox.
            cn->linesPerBox = (s8) cn_rom_dialog_lines(i);
            cn->leftOffset = (s16) cn_rom_dialog_left(i);
            cn->width = (s16) cn_rom_dialog_width(i);
        } else {
            // No rom: keep the vanilla metrics so nothing reads garbage. This
            // entry is not drawn anyway -- str == NULL makes it fall back.
            cn->linesPerBox = orig->linesPerBox;
            cn->leftOffset = orig->leftOffset;
            cn->width = orig->width;
        }
    }
}
#else
void dialog_table_refresh_cn(void) {
}
#endif

struct DialogEntry* dialog_table_get_render(s32 dialogId) {
    if (!IS_VALID_DIALOG(dialogId)) {
        return NULL;
    }

#if defined(SM64COOPDX_CN_TEXT)
    if (gCnTextPackActive && IS_VALID_VANILLA_DIALOG(dialogId) && sDialogTableCn != NULL) {
        struct DialogEntry *cn = sDialogTableCn->buffer[dialogId];
        if (cn != NULL && cn->str != NULL) {
            return cn;
        }
    }
#endif

    return dialog_table_get(dialogId);
}

bool dialog_table_render_is_cn(s32 dialogId) {
    if (!IS_VALID_DIALOG(dialogId)) { return false; }
    return dialog_table_get_render(dialogId) != dialog_table_get(dialogId);
}

void dialog_table_reset(void) {
    DialogTable *table = gDialogTable;

    if (!table) return;

    // Only the US table carries `replaced` entries; the CN table is rebuilt from
    // the rom by dialog_table_refresh_cn() and has nothing to undo.
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

#if defined(SM64COOPDX_CN_TEXT)
    // The CN entries own nothing but pointers into the rom's segment, so the
    // entries and the array are all there is to release.
    if (sDialogTableCn != NULL) {
        for (u32 i = 0; i < sDialogTableCn->count; i++) {
            free(sDialogTableCn->buffer[i]);
        }

        free(sDialogTableCn->buffer);
        free(sDialogTableCn);
        sDialogTableCn = NULL;
    }
#endif
}
