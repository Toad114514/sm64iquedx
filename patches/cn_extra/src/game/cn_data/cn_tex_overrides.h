// cn_tex_overrides.h - what is left of the iQue texture overlay after the
// pixels moved out of the binary.
//
// The twelve textures (power meter left/right, the two act-selector "COURSE"
// boards, the eight title frames) are no longer compiled in. They are read out
// of the player's 神游 rom at startup: three MIO0 blocks are expanded and the
// twelve cells copied into one contiguous buffer. See cn_rom_assets.c, which
// also documents which block and offset each one lives at.
//
// What stays here is the *geometry*, because unlike the pixels it was never
// extracted from the picture -- it is eight numbers whose only job is to line
// the gauge up with the redrawn artwork.

#ifndef CN_TEX_OVERRIDES_H
#define CN_TEX_OVERRIDES_H

#include "types.h"

// ---------------------------------------------------------------------------
// The one piece of geometry iQue moved.
//
// `vertex_power_meter_health_segments` (actors/power_meter/model.inc.c) is the
// four-vertex quad the coloured health gauge is drawn into. In the US rom it
// straddles the widget origin (y = -16 .. +16). iQue drew it lower, because the
// redrawn power-meter artwork is itself four texels lower inside the same 32x64
// cell -- 「能量」 is a 16px glyph where "POWER" was 8px, so the whole picture was
// nudged down to keep the label clear of the cloud.
//
// Overlaying the pixels without overlaying these vertices leaves the gauge
// sitting four texels high in its socket: the cloud's dark rim shows all the way
// round the sphere and the widget reads as two rings stacked on top of each
// other. The artwork and the geometry that consumes it are one unit -- moving
// one without the other is the bug.
//
// Only ob[1] differs between the two roms; the other 15 bytes of every vertex
// are identical, which the generator asserts.
static const s16 sCnPowMeterGaugeY_us[4] = { -16, -16, 16, 16 };
static const s16 sCnPowMeterGaugeY_cn[4] = { -20, -20, 12, 12 };

#define CN_POW_METER_GAUGE_VTX_COUNT 4

#endif // CN_TEX_OVERRIDES_H
