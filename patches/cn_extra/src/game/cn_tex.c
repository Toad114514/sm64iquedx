// cn_tex.c - Overlay the assets iQue localised, in place.
//
// Why an overlay rather than a replacement
// ----------------------------------------
// coopdx loads every texture with ROM_ASSET_LOAD_TEXTURE(name, path,
// physicalAddress, physicalSize, segmentedAddress, size), which declares a
// global Texture[] and fills it from the rom at startup. Because every asset
// carries its own segment address *and* offset, a texture that iQue redrew at
// the same offset can simply be swapped in the array afterwards -- no segment
// grows or shrinks, and nothing else has to move.
//
// That "same offset" property is not an assumption. It was read out of
// assets.json (the list the official decomp generates), which records the
// [segment, offset] of every texture per version. For power_meter and
// title_screen_bg the CN numbers are identical to the US ones, so these are
// drop-in swaps. iQue's *inserted* textures (file-select menu, copyright
// notices) have different offsets and are therefore not handled here -- they
// would need the surrounding code to change, not just the pixels.
//
// There is one non-texture entry. Diffing the decompressed segment against the
// US rom shows iQue changed exactly two things in it: the power-meter artwork,
// and the four vertices of vertex_power_meter_health_segments. They belong to
// the same widget, so they are overlaid by the same module -- swapping the
// pixels alone leaves the gauge four texels out of its socket. See
// cn_data/cn_tex_overrides.h for the full account.
//
// Verified with .workbuddy/diff_cn_us_textures.py, which walks the whole asset
// list and reports which textures actually differ between the two roms, and
// with .workbuddy/verify_cn_tex.py, which re-derives every byte from the roms.

#include "cn_tex.h"

#if defined(SM64COOPDX_CN_TEXT)

#include <string.h>
#include <assert.h>

// For Vtx, which the gauge overlay writes field by field. Not included for
// anything else here -- the texture arrays below stay naked u8* on purpose.
#include <ultra64.h>

#include "segment2.h"
#include "cn_rom_assets.h"
#include "cn_data/cn_tex_overrides.h"

// gfx_pc.h cannot be included from a C file: it declares struct
// GfxWindowManagerAPI and wraps its prototypes in extern "C". Only this one
// function is needed, so it is declared locally.
void gfx_texture_cache_clear(void);

// The arrays created by ROM_ASSET_LOAD_TEXTURE(). They sit at file scope in
// actors/common1.c (pulled in via power_meter/model.inc.c) and in
// bin/title_screen_bg.c, so they are ordinary globals.
//
// Declared as u8* rather than Texture* deliberately: Texture is a typedef for
// u8, so this names the same objects while keeping the declaration independent
// of ultra64.h. The sizes below must match the ROM_ASSET_LOAD_TEXTURE lengths
// exactly -- a mismatch would limp the texture rather than fail loudly.
extern u8 texture_power_meter_left_side[];
extern u8 texture_power_meter_right_side[];
// The act-selector wood sign. levels/menu/leveldata.c loads it with
// ROM_ASSET_LOAD_TEXTURE(texture_menu_course_*, 0x002a65b0, ..., 4096), the
// same segment/offset in both roms -- iQue only repainted the board.
extern u8 texture_menu_course_upper[];
extern u8 texture_menu_course_lower[];
extern u8 title_texture_0A0001C0[];
extern u8 title_texture_0A000E40[];
extern u8 title_texture_0A001AC0[];
extern u8 title_texture_0A002740[];
extern u8 title_texture_0A0033C0[];
extern u8 title_texture_0A004040[];
extern u8 title_texture_0A004CC0[];
extern u8 title_texture_0A005940[];

// ROM_ASSET_LOAD_VTX(vertex_power_meter_health_segments, ..., 64) in
// actors/common1.c (via power_meter/model.inc.c) declares Vtx[4]. Its ob[1] is
// the only geometry iQue moved in the whole segment.
extern Vtx vertex_power_meter_health_segments[];

struct CnTexSlot {
    u8 *target;          // the vanilla array rom_assets_load() filled
    u32 tex;             // index into the runtime iQue texture set
    u32 size;
};

// Offsets into sUsBackup are assigned at first use rather than written by hand,
// so a transcription slip cannot silently make one slot's backup overwrite
// another's (which would corrupt two textures in a way that only shows up as
// "restoring the pack off looks wrong").
// `tex` indexes the runtime texture set (cn_rom_assets.h): the pixels are read
// out of the player's 神游 rom at startup instead of being compiled in, so the
// pointer cannot live in this static table -- it does not exist until the rom
// has been opened. cn_tex_apply() resolves it on every call.
static struct CnTexSlot sSlots[] = {
    { texture_power_meter_left_side,  0, 4096 },
    { texture_power_meter_right_side, 1, 4096 },
    { texture_menu_course_upper,      2, 4096 },
    { texture_menu_course_lower,      3, 4096 },
    { title_texture_0A0001C0,         4, 3200 },
    { title_texture_0A000E40,         5, 3200 },
    { title_texture_0A001AC0,         6, 3200 },
    { title_texture_0A002740,         7, 3200 },
    { title_texture_0A0033C0,         8, 3200 },
    { title_texture_0A004040,         9, 3200 },
    { title_texture_0A004CC0,        10, 3200 },
    { title_texture_0A005940,        11, 3200 },
};

#define CN_TEX_SLOT_COUNT ((u32)(sizeof(sSlots) / sizeof(sSlots[0])))

// 4 * 4096 (power meter + the two act-selector boards) + 8 * 3200 (title
// frames) = 41984.
#define CN_TEX_BACKUP_SIZE 41984

// The original US pixels, so the pack can be switched back off at runtime.
//
// This must be captured *after* rom_assets_load() has run and before the first
// overlay: before the load the arrays are all zero, and saving zeros as "the
// originals" would blank the textures the moment the pack was turned off.
// cn_tex_on_rom_loaded() is what keeps that ordering honest.
static u8 sUsBackup[CN_TEX_BACKUP_SIZE];
static u32 sBackupOffset[CN_TEX_SLOT_COUNT];
static bool sBackedUp = false;

static void cn_tex_capture_backup(void) {
    u32 i, offset = 0;

    for (i = 0; i < CN_TEX_SLOT_COUNT; i++) {
        sBackupOffset[i] = offset;
        offset += sSlots[i].size;
    }
    assert(offset <= CN_TEX_BACKUP_SIZE);

    for (i = 0; i < CN_TEX_SLOT_COUNT; i++) {
        memcpy(&sUsBackup[sBackupOffset[i]], sSlots[i].target, sSlots[i].size);
    }
    sBackedUp = true;
}

// Move the health gauge to where iQue drew it.
//
// No backup is needed for this one: unlike the textures, whose US pixels have
// to be read out of the loaded asset, both scripts are compile-time constants
// taken from the two roms (see cn_tex_overrides.h), so writing the US values
// back *is* restoring the rom's own data.
static void cn_tex_apply_gauge(bool active) {
    u32 i;

    for (i = 0; i < CN_POW_METER_GAUGE_VTX_COUNT; i++) {
        vertex_power_meter_health_segments[i].v.ob[1] =
            active ? sCnPowMeterGaugeY_cn[i] : sCnPowMeterGaugeY_us[i];
    }
}

s32 cn_tex_gauge_dy(void) {
    if (!gCnTextPackActive) { return 0; }
    // Vertex y grows upwards: the HUD ortho matrix is guOrtho(0, SCREEN_WIDTH,
    // 0, SCREEN_HEIGHT), so a smaller ob[1] is *lower* on screen. iQue's quad
    // therefore sits (us - cn) texels lower than the US one.
    return sCnPowMeterGaugeY_us[0] - sCnPowMeterGaugeY_cn[0];
}

void cn_tex_apply(bool active) {
    u32 i;

    if (!sBackedUp) {
        cn_tex_capture_backup();
    }

    for (i = 0; i < CN_TEX_SLOT_COUNT; i++) {
        const u8 *localised = active ? cn_rom_texture(sSlots[i].tex) : NULL;
        memcpy(sSlots[i].target,
               (localised != NULL) ? localised : &sUsBackup[sBackupOffset[i]],
               sSlots[i].size);
    }

    cn_tex_apply_gauge(active);

    // gfx_pc's texture cache (gfx_texture_cache_lookup) keys on the source
    // address, format and size -- never on the contents. These addresses are
    // unchanged, so a texture already uploaded this session would keep being
    // drawn and the swap would look like a no-op. Dropping the cache forces the
    // next draw to re-import from the array we just wrote.
    //
    // Same root cause as the glyph work, where each glyph needed its own fixed
    // buffer for exactly this reason.
    gfx_texture_cache_clear();
}

void cn_tex_on_rom_loaded(bool active) {
    // The arrays have just been refilled with US pixels, so whatever we saved
    // earlier is no longer the original.
    sBackedUp = false;
    cn_tex_apply(active);
}

#else // !SM64COOPDX_CN_TEXT

// Built without the pack: the iQue data is not compiled in, so these have to
// stay callable no-ops for the call sites to remain unconditional.
void cn_tex_apply(UNUSED bool active) {
}

void cn_tex_on_rom_loaded(UNUSED bool active) {
}

s32 cn_tex_gauge_dy(void) {
    return 0;
}

#endif
