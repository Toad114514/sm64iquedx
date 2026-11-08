#ifndef CN_TEX_H
#define CN_TEX_H

#include "types.h"

// Runtime overlay of the assets iQue localised in place.
//
// Two subjects exist, because iQue did not redraw the whole game -- it
// translated a handful of images that carried English words, and adjusted one
// piece of geometry that had to follow one of them:
//
//   * the power meter, whose US "POWER" graphics become 「能量」
//   * the title screen wordmark, whose US frames become the iQue / 神游 logo
//   * the four vertices of the health gauge, which iQue drew four units lower
//     to stay centred in that redrawn artwork (which it moved four texels down
//     inside the same 32x64 cell)
//
// The textures all sit at the *same* segment address and offset in the CN rom
// as in the US rom, which is what makes an overlay possible at all: no segment
// grows or shrinks, so the pixels can simply be swapped in place. Textures that
// CN only *inserted* (the file-select menu, the copyright notices) change the
// segment layout and are deliberately not handled here -- they still need code
// work.
//
// Every entry is applied behind the same gate as the text pack, so turning the
// pack off restores the original US graphics and geometry rather than leaving a
// half-English screen behind.
void cn_tex_apply(bool active);

// How far iQue moved the health gauge inside its socket, in texels (down is
// positive). The vanilla display list gets this from the vertex overlay below;
// anything that draws the gauge *by hand* has to ask for it explicitly.
//
// This matters because the Lua binding `hud_render_power_meter[_interpolated]`
// does not replay `dl_power_meter_base` -- it re-implements the widget as three
// texture quads at hard-coded US offsets. Mods (flood+ is one) call that
// binding instead of letting the vanilla HUD draw, so without this offset they
// put the blue pie four texels too high: the dark inside of the socket shows
// through underneath, which reads on screen as a shadow the iQue original does
// not have. Returns 0 when the pack is off, i.e. the US layout.
s32 cn_tex_gauge_dy(void);

// Called at the end of rom_assets_load(). The asset arrays have just been
// refilled from the US rom, so any earlier backup is stale and must be retaken
// before the overlay is re-applied. Without this, a second rom load would
// "restore" the CN pixels we had saved as if they were the originals.
// `active` is the pack state to apply once the backup has been refreshed.
void cn_tex_on_rom_loaded(bool active);

#endif // CN_TEX_H
