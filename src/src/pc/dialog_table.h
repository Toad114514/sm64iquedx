#ifndef DIALOG_TABLE_H
#define DIALOG_TABLE_H

#include "PR/ultratypes.h"
#include "game/ingame_menu.h"

#define MAX_ALLOCATED_DIALOGS 0x8000

typedef struct GrowingArray DialogTable;

void dialog_table_init(void);
struct DialogEntry* dialog_table_alloc(s32 *dialogId);

// The one and only table. Lua reads and writes it (smlua_text_utils_*), and
// the renderer draws it -- there is no second copy of the dialog data, so a
// mod's replacement and the Chinese pack can no longer disagree about which
// text belongs to an id.
//
// What the pack changes is where `str` points: dialog_table_apply_cn(true)
// points the untouched entries at the UTF-8 strings converted out of the 神游
// cartridge, apply_cn(false) points them back at the ROM's own text. Entries a
// mod replaced are left exactly as the mod left them.
struct DialogEntry* dialog_table_get(s32 dialogId);

// Install (or remove) the Chinese text. Idempotent; entries whose dialog id the
// cartridge does not cover keep their vanilla string. Never touches an entry
// with `replaced` set, so a mod's text always wins over the pack.
//
// Called when a level is loaded, not when the settings checkbox flips -- see
// chinese_text_pack_set_active() in pc_main.c.
void dialog_table_apply_cn(bool on);

void dialog_table_reset(void);
void dialog_table_shutdown(void);

extern DialogTable *gDialogTable;

#define IS_CUSTOM_DIALOG(dialogId) ((dialogId) >= DIALOG_COUNT)
#define IS_VALID_DIALOG(dialogId) ((dialogId) >= 0 && (u32)(dialogId) < gDialogTable->count)
#define IS_VALID_VANILLA_DIALOG(dialogId) ((dialogId) >= 0 && (dialogId) < DIALOG_COUNT)

#endif
