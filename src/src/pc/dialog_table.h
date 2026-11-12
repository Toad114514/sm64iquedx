#ifndef DIALOG_TABLE_H
#define DIALOG_TABLE_H

#include "PR/ultratypes.h"
#include "game/ingame_menu.h"

#define MAX_ALLOCATED_DIALOGS 0x8000

typedef struct GrowingArray DialogTable;

void dialog_table_init(void);
struct DialogEntry* dialog_table_alloc(s32 *dialogId);

// The US table: what the Lua text layer reads and writes (smlua_text_utils_*).
// Never contains Chinese; a mod's replacement lands here and stays here.
struct DialogEntry* dialog_table_get(s32 dialogId);

// The table the renderer should draw: the iQue pack's own entries while the
// pack is on and the rom is loaded, the US table otherwise. Falls back to the
// US entry for custom dialogs and when the rom is missing.
struct DialogEntry* dialog_table_get_render(s32 dialogId);

// True when dialog_table_get_render() would return the Chinese entry, i.e. the
// pack supplies this dialog and a mod's override cannot be merged into it.
bool dialog_table_render_is_cn(s32 dialogId);

// Rebuild the Chinese table from the loaded rom. Cheap; safe to call when the
// table or the rom is absent (it then leaves the entries in "no rom" state).
void dialog_table_refresh_cn(void);

void dialog_table_reset(void);
void dialog_table_shutdown(void);

extern DialogTable *gDialogTable;

#define IS_CUSTOM_DIALOG(dialogId) ((dialogId) >= DIALOG_COUNT)
#define IS_VALID_DIALOG(dialogId) ((dialogId) >= 0 && (u32)(dialogId) < gDialogTable->count)
#define IS_VALID_VANILLA_DIALOG(dialogId) ((dialogId) >= 0 && (dialogId) < DIALOG_COUNT)

#endif
