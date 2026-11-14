#ifndef CN_TEXT_GATE_H
#define CN_TEXT_GATE_H

#include <stdbool.h>

// Can the Chinese (iQue) text pack run? Pure, side-effect free, so the rule
// below can be exercised without a window, a rom or a menu.
//
//   detected -- rom_checker_ique_rom_present() AND cn_rom_assets_load() succeeded
//   wanted   -- configChineseText (the position of the settings checkbox)
//
// One hard requirement, and that is all. There is deliberately no "has the
// player touched the switch yet" input:
//
//   * no 神游 cartridge dump, no Chinese text -- the glyphs, dialog strings and
//     textures are read out of that dump at startup, so without it there is
//     literally nothing to draw.
//
// The default of `wanted` is true, so a first launch with the dump in place is
// already Chinese, and the checkbox is the only way it ever changes -- the live
// state is never written back to the config (see chinese_text_pack_init), so a
// launch without the dump cannot silently flip the player's choice.
//
// ---------------------------------------------------------------------------
// Romhacks are NOT excluded any more
// ---------------------------------------------------------------------------
//
// The pack used to switch itself off whenever an enabled mod was a romhack:
// every romhack of this engine is built on the US data and re-authors the very
// tables the pack used to replace wholesale, so iQue's strings sat over ids that
// no longer existed (vanilla Chinese over hacked English, mangled pause menu).
//
// That was a property of the old pack, which *owned* the tables: it swapped
// whole segments, spoke a private two-byte encoding and could not draw a string
// it had not produced itself. It is now a per-entry overlay over data read from
// the player's own dump, and all text is plain UTF-8, so each of the three
// subsystems defers to the hack on exactly the entries the hack touched:
//
//   * course / act names -- apply_cn_text_pack() rewrites a slot only while it
//     still holds the vanilla buffer, the pack's own name or NULL. A name a mod
//     installed (every romhack does, through smlua_text_utils) is a heap
//     pointer this pack does not own, so that slot keeps the hack's name.
//   * dialog entries -- dialog_table_apply_cn() skips everything with
//     `replaced` set, i.e. everything a mod wrote via smlua_replace_dialog().
//     Custom dialogs (id >= DIALOG_COUNT) are outside its loop entirely.
//   * custom level / star names -- these never went through the course tables;
//     they are UTF-8 strings a mod registered. get_level_name_sm64() and
//     get_star_name_sm64() hand them to the renderers unchanged instead of
//     round-tripping them through the single-byte charmap (which turned a
//     Chinese name into spaces), so the font system draws the hack's own name in
//     either script.
//
// Art is the only thing left: the pack still overlays the twelve textures iQue
// redrew. A mod's own texture (DynOS `textures/`) is resolved before the loaded
// texture is read, so a hack that replaces those pixels wins on screen and the
// iQue overlay simply becomes dead data.
//
// When the answer is false (no dump) the settings checkbox is greyed out
// (djui_panel_misc.c) instead of accepting a click that would do nothing.
static inline bool cn_text_pack_target(bool detected, bool wanted) {
    return detected && wanted;
}

#endif
