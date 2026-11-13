#ifndef CN_TEXT_GATE_H
#define CN_TEXT_GATE_H

#include <stdbool.h>

// Should the Chinese (iQue) text pack be active? Pure, side-effect free, so the
// rule below can be exercised without a window, a rom or a menu.
//
//   detected -- rom_checker_ique_rom_present() AND cn_rom_assets_load() succeeded
//   userSet  -- configChineseTextUserSet (the player flipped the switch by hand)
//   wanted   -- configChineseText (what the player asked for)
//   romhack  -- an enabled mod ships its own level / actor / behavior data
//               (cn_romhack_mods_present() in pc_main.c)
//
// Two rules are absolute.
//
// 1. No 神游 cartridge dump, no Chinese text. The glyphs, dialog strings and
//    textures are read out of that dump at startup, so without it there is
//    literally nothing to draw -- but the rule is still written down here,
//    because "the loader happens to return false" is a property of another file
//    and this branch is what the settings UI and the texture overlay both
//    consult. Writing it as `userSet ? wanted : detected` would let a player
//    with no cartridge light the pack up by hand -- keep `detected` outermost.
//
// 2. A romhack outranks the player. Every romhack of this engine is built on
//    the US data: it re-authors levels, behaviors and -- crucially -- the text
//    that lives in them (course/act names, dialog entries, pause-menu labels).
//    Overlaying iQue's tables on top of re-authored data does not translate
//    anything: the ids no longer line up, so the pack either keeps serving
//    vanilla strings over hacked text or feeds hacked bytes into the CN decoder
//    (exactly the garbage the pause menu and the dialog box show when a romhack
//    runs with the pack on). The localised textures are no better -- they were
//    captured from the iQue rom's vanilla layout, not from the hack's. So when a
//    romhack is loaded the pack is off for the whole session, whatever the
//    config or the checkbox says, and the checkbox is greyed out with a reason
//    (djui_panel_misc.c) instead of silently doing nothing.
//
// Otherwise: with the dump present the player wins over detection in both
// directions (otherwise unchecking the box would silently undo itself on the
// next start), and a player who never touched the switch gets the pack by
// default.
static inline bool cn_text_pack_target(bool detected, bool userSet, bool wanted, bool romhack) {
    return detected && !romhack && (!userSet || wanted);
}

// 1 when the enabled mod set contains a romhack (see cn_romhack_mods_present()
// in pc_main.c). Defined in pc_main.c, read by the settings panel to explain
// why the checkbox is greyed out, and re-derived every time mods are activated.
extern bool gCnTextPackRomhack;

// Re-derive gCnTextPackRomhack from the enabled mods and, if a romhack turned
// up while the pack was on, switch the pack off (tables + textures). Cheap
// enough to call whenever the active mod set changes.
void cn_text_pack_romhack_refresh(void);

#endif
