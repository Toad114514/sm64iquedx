#ifndef CN_TEXT_GATE_H
#define CN_TEXT_GATE_H

#include <stdbool.h>

// Can the Chinese (iQue) text pack run? Pure, side-effect free, so the rule
// below can be exercised without a window, a rom or a menu.
//
//   detected -- rom_checker_ique_rom_present() AND cn_rom_assets_load() succeeded
//   wanted   -- configChineseText (the position of the settings checkbox)
//   romhack  -- an enabled mod ships its own level / actor / behavior data
//               (cn_romhack_mods_present() in pc_main.c)
//
// Two hard requirements, and that is all. There is deliberately no third
// "has the player touched the switch yet" input:
//
//   * no 神游 cartridge dump, no Chinese text -- the glyphs, dialog strings and
//     textures are read out of that dump at startup, so without it there is
//     literally nothing to draw;
//   * a romhack outranks the player. Every romhack of this engine is built on
//     the US data: it re-authors levels, behaviors and -- crucially -- the text
//     that lives in them (course/act names, dialog entries, pause-menu labels).
//     Overlaying iQue's tables on top of re-authored data does not translate
//     anything: the ids no longer line up, so the pack either keeps serving
//     vanilla strings over hacked text or feeds hacked bytes into the CN decoder
//     (exactly the garbage the pause menu and the dialog box show when a romhack
//     runs with the pack on). The localised textures are no better -- they were
//     captured from the iQue rom's vanilla layout, not from the hack's.
//
// `wanted` therefore only decides the case that is otherwise fine: dump present,
// no romhack. The default is true, so a first launch with the dump in place is
// already Chinese, and the checkbox is the only way it ever changes -- the live
// state is never written back to the config (see chinese_text_pack_init), so a
// launch without the dump cannot silently flip the player's choice.
//
// When the answer is false because of either hard requirement, the settings
// checkbox is greyed out (djui_panel_misc.c) instead of accepting a click that
// would do nothing.
static inline bool cn_text_pack_target(bool detected, bool wanted, bool romhack) {
    return detected && !romhack && wanted;
}

// 1 when the enabled mod set contains a romhack (see cn_romhack_mods_present()
// in pc_main.c). Defined in pc_main.c, read by the settings panel to decide
// whether the checkbox is greyed out, and re-derived every time mods are
// activated.
extern bool gCnTextPackRomhack;

// Re-derive gCnTextPackRomhack from the enabled mods and, if a romhack turned
// up while the pack was on, switch the pack off (tables + textures). Cheap
// enough to call whenever the active mod set changes.
void cn_text_pack_romhack_refresh(void);

#endif
