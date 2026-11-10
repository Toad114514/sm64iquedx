#ifndef CN_TEXT_GATE_H
#define CN_TEXT_GATE_H

#include <stdbool.h>

// Should the Chinese (iQue) text pack be active? Pure, side-effect free, so the
// rule below can be exercised without a window, a rom or a menu.
//
//   detected -- rom_checker_ique_rom_present() AND cn_rom_assets_load() succeeded
//   userSet  -- configChineseTextUserSet (the player flipped the switch by hand)
//   wanted   -- configChineseText (what the player asked for)
//
// One rule is absolute: no 神游 cartridge dump, no Chinese text. The glyphs,
// dialog strings and textures are read out of that dump at startup, so without
// it there is literally nothing to draw -- but the rule is still written down
// here, because "the loader happens to return false" is a property of another
// file and this branch is what the settings UI and the texture overlay both
// consult. Writing it as `userSet ? wanted : detected` would let a player with
// no cartridge light the pack up by hand -- keep `detected` outermost.
//
// With the dump present the player wins over detection in both directions
// (otherwise unchecking the box would silently undo itself on the next start),
// and a player who never touched the switch gets the pack by default.
static inline bool cn_text_pack_target(bool detected, bool userSet, bool wanted) {
    return detected && (!userSet || wanted);
}

#endif
