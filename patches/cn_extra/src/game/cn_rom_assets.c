// cn_rom_assets.c - stream the Chinese text pack out of the player's own
// 神游 (iQue) dump. See cn_rom_assets.h for why this is not compiled in.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "cn_rom_assets.h"

// The engine's MIO0 decoder. It lives in src/pc/rom_assets.c because that is
// where the vanilla segments are expanded; there is no game-side copy, and
// declaring it here is cheaper than duplicating a decoder that already handles
// the byte order and the back-reference window correctly.
extern u8 *rom_assets_decompress(u32 *data, u32 *decompressedSize);

// MIO0 header is 16 bytes; the block itself is 55 KB for this segment. Read a
// fixed generous chunk rather than trying to work out the compressed length in
// advance: header field 1 is the *decompressed* size and field 2 is where the
// back-reference stream starts, neither of which tells us where it ends.
#define CN_ROM_MIO0_MAX 65536

static FILE *sRomFile = NULL;
static u8   *sSeg2 = NULL;
static u8   *sGlyphs = NULL;
static u8   *sTextures = NULL;

// Course (26) and act (90) names, rebuilt from the rom's own pointer tables.
//
// These are copies rather than pointers into sSeg2 for one reason: the iQue
// strings do not begin with the 3-byte `[9E][num][9E]` prefix that every caller
// in this port still expects (`courseName + 3`, and cn_is_cn_string() which
// walks on from base + 3). Re-creating the prefix here means not one consumer
// had to change, and the bytes are identical to what the old generated
// text/cn_courses_cn.inc.c held -- verified entry by entry when it was removed.
static u8 sCourseNames[CN_ROM_COURSE_COUNT][CN_ROM_NAME_BYTES];
static u8 sActNames[CN_ROM_ACT_MAIN_COUNT][CN_ROM_NAME_BYTES];

// Owned by this module, declared in cn_data/cn_glyphs_16x16.h, consumed by
// cn_font.c. NULL until the rom has been read.
const u8 *sCnGlyphs16x16 = NULL;

// The twelve localised textures. `block` is the file offset of the MIO0 block
// that carries them, `off` is the offset inside that block once expanded.
//
// These addresses are properties of the cartridge, not copies of its pixels --
// and they were measured by searching the expanded blocks for the very arrays
// this file used to contain, so they are exact rather than inferred. Three
// distinct blocks, so a single scratch buffer and one pass per block is enough.
struct CnRomTexDesc {
    u32 block;
    u32 off;
    u32 size;
};

static const struct CnRomTexDesc sTexDesc[CN_ROM_TEXTURE_COUNT] = {
    { 0x1EC940, 0x233E0, 4096 },  // power_meter_left
    { 0x1EC940, 0x243E0, 4096 },  // power_meter_right
    { 0x293360, 0x0BCE0, 4096 },  // menu_course_upper
    { 0x293360, 0x0CCE0, 4096 },  // menu_course_lower
    { 0x25C280, 0x001C0, 3200 },  // title frame 0
    { 0x25C280, 0x00E40, 3200 },  // title frame 1
    { 0x25C280, 0x01AC0, 3200 },  // title frame 2
    { 0x25C280, 0x02740, 3200 },  // title frame 3
    { 0x25C280, 0x033C0, 3200 },  // title frame 4
    { 0x25C280, 0x04040, 3200 },  // title frame 5
    { 0x25C280, 0x04CC0, 3200 },  // title frame 6
    { 0x25C280, 0x05940, 3200 },  // title frame 7
};

static u32 read_be32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static u32 read_be16(const u8 *p) {
    return ((u32)p[0] << 8) | (u32)p[1];
}

// One glyph = two 16-byte ia1 tiles, each drawn transposed and horizontally
// mirrored, side by side (left half then right half), then the whole 16x16 cell
// is flipped vertically so row 0 is the top.
//
// This is the same transform .workbuddy/gen_cn_glyphs.py applies, kept here so
// the renderer can keep consuming a plain 16x16 1bpp table and none of the
// carefully-debugged drawing code had to change.
static void build_glyph(u8 *dst, const u8 *tiles) {
    int x, y;

    memset(dst, 0, 32);
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int half = x >> 3;             // 0 = left tile, 1 = right tile
            int tx = x & 7;                // column inside that half
            const u8 *t = tiles + half * 0x10;
            // The tile is 8 rows x 16 columns of bits; the CN vertex maps the
            // tile's columns to the quad's y and its rows (mirrored) to x.
            int row = 7 - tx;
            int col = y;
            int bit = (t[row * 2 + (col >> 3)] >> (7 - (col & 7))) & 1;
            if (bit) {
                int outRow = 15 - y;       // final vertical flip
                int idx = outRow * 16 + x;
                dst[idx >> 3] |= (u8)(1 << (7 - (idx & 7)));
            }
        }
    }
}

// Structural sanity check on the decompressed segment: every pointer the ROM's
// own dialog table holds must land inside the buffer, and the glyph area must
// not be empty. Content is never compared -- that would be the very data we are
// trying not to ship -- but a wrong/truncated dump has to be rejected here,
// because afterwards the engine will happily read from these pointers.
static bool validate_segment(const u8 *seg) {
    u32 i;
    u32 glyphBits = 0;

    for (i = 0; i < CN_ROM_DIALOG_COUNT; i++) {
        const u8 *e = seg + CN_ROM_DIALOG_OFF + i * CN_ROM_DIALOG_STRIDE;
        u32 addr = read_be32(e + CN_ROM_DIALOG_STR);
        u32 lines = e[CN_ROM_DIALOG_LINES];

        if (addr < CN_ROM_SEG_BASE) { return false; }
        if (addr - CN_ROM_SEG_BASE >= CN_ROM_SEG2_SIZE) { return false; }
        if (lines == 0 || lines > 16) { return false; }
    }

    for (i = 0; i < CN_ROM_GLYPH_BYTES; i++) {
        glyphBits |= seg[CN_ROM_GLYPH_OFF + i];
    }
    return glyphBits != 0;
}

bool cn_rom_assets_ready(void) {
    return sSeg2 != NULL;
}

// Translate one of the segment's absolute addresses (the 0x02000000 window) to
// an offset we can index sSeg2 with.
static bool seg_offset(u32 addr, u32 *off) {
    if (addr < CN_ROM_SEG_BASE) { return false; }
    if (addr - CN_ROM_SEG_BASE + 2 > CN_ROM_SEG2_SIZE) { return false; }
    *off = addr - CN_ROM_SEG_BASE;
    return true;
}

// Copy the token stream at `off` into `dst`, optionally behind the 3-byte
// vanilla course prefix, and append the second 0xFF 0xFF the generated arrays
// carried (the string already ends with one; the old generator emitted another
// pair and the renderers were tuned against those exact lengths).
//
// Walks two bytes at a time because 0xFF 0xFF -- not any single 0xFF -- is the
// terminator here: 0xFF is also the lead byte of the space and newline tokens,
// so a byte-wise search would cut every name at its first space.
static bool copy_name(u8 *dst, u32 off, int prefixByte) {
    u32 n = 0;

    if (prefixByte >= 0) {
        dst[n++] = 0x9E;
        dst[n++] = (u8)prefixByte;
        dst[n++] = 0x9E;
    }

    for (;;) {
        // room for this token plus the trailing 0xFF 0xFF
        if (n + 4 > CN_ROM_NAME_BYTES) { return false; }
        if (off + 2 > CN_ROM_SEG2_SIZE) { return false; }
        dst[n++] = sSeg2[off + 0];
        dst[n++] = sSeg2[off + 1];
        if (sSeg2[off] == 0xFF && sSeg2[off + 1] == 0xFF) { break; }
        off += 2;
    }

    dst[n++] = 0xFF;
    dst[n++] = 0xFF;
    memset(dst + n, 0, CN_ROM_NAME_BYTES - n);
    return true;
}

// Fill sCourseNames / sActNames from the rom's course and act tables.
//
// A failure here rejects the whole load: the tables are the last thing checked,
// so at this point the segment has already been proven to be the expected iQue
// one, and a name pointer that does not land in it means the dump is not what
// the offsets describe. Leaving the pack off is the safe answer -- half-swapped
// text is not.
static bool build_names(void) {
    u32 i;

    memset(sCourseNames, 0, sizeof(sCourseNames));
    memset(sActNames, 0, sizeof(sActNames));

    for (i = 0; i < CN_ROM_COURSE_COUNT; i++) {
        u32 off;
        // The prefix byte is the slot's course number for the fifteen stages
        // (1..15) and the slot index for the secret/castle slots (15..25).
        // Nothing reads it back -- every consumer skips these three bytes -- but
        // reproducing it exactly keeps the bytes identical to the dump the port
        // used to compile in, so no behaviour can shift by accident.
        int const prefix = (i < CN_ROM_STAGE_COUNT) ? (int)i + 1 : (int)i;
        if (!seg_offset(read_be32(sSeg2 + CN_ROM_COURSE_OFF + i * 4), &off)) { return false; }
        if (!copy_name(sCourseNames[i], off, prefix)) { return false; }
    }

    for (i = 0; i < CN_ROM_ACT_MAIN_COUNT; i++) {
        u32 off;
        if (!seg_offset(read_be32(sSeg2 + CN_ROM_ACT_OFF + i * 4), &off)) { return false; }
        if (!copy_name(sActNames[i], off, -1)) { return false; }
    }

    return true;
}

// Cheap integrity fingerprint of what actually got loaded.
//
// Printed only when CN_ROM_ASSETS_DEBUG is set in the environment, because the
// interesting question ("did the pack come out of the cartridge intact?") comes
// up exactly once per bug report and not on every launch. The numbers are
// reproducible from the rom with .workbuddy/probe_ique_seg2.py.
static u32 fnv1a(const u8 *p, u32 n) {
    u32 h = 0x811C9DC5u;
    u32 i;
    for (i = 0; i < n; i++) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}

// Expand one MIO0 block into `dst`, which must hold `expect` bytes.
//
// The compressed length is not recorded anywhere we can read without decoding,
// so we read a generous chunk and let decompress() stop when it reaches the
// declared output size. Reading the rest of the file instead would mean a 6 MB
// read for a 200 KB segment on every launch.
static bool load_block(u32 blockOff, u8 *dst, u32 expect, u8 **scratch) {
    u8  hdr[16];
    u32 uncompressed;
    u32 want;
    u32 got;

    if (fseek(sRomFile, (long)blockOff, SEEK_SET) != 0) { return false; }
    if (fread(hdr, 1, sizeof(hdr), sRomFile) != sizeof(hdr)) { return false; }
    if (memcmp(hdr, "MIO0", 4) != 0) { return false; }

    uncompressed = read_be32(hdr + 4);
    if (uncompressed != expect) { return false; }

    // MIO0 only ever wins over raw storage, so the compressed size stays below
    // the expanded size; double it for margin and clamp at EOF.
    want = expect * 2 + 64;
    if (want < sizeof(hdr)) { want = sizeof(hdr); }

    if (*scratch == NULL) {
        *scratch = (u8 *)malloc(want);
        if (*scratch == NULL) { return false; }
    } else {
        u8 *grown = (u8 *)realloc(*scratch, want);
        if (grown == NULL) { return false; }
        *scratch = grown;
    }

    if (fseek(sRomFile, (long)blockOff, SEEK_SET) != 0) { return false; }
    got = (u32)fread(*scratch, 1, want, sRomFile);
    if (got < sizeof(hdr)) { return false; }

    {
        u32 outSize = 0;
        u8 *out = rom_assets_decompress((u32 *)*scratch, &outSize);
        if (out == NULL) { return false; }
        if (outSize != expect) { free(out); return false; }
        memcpy(dst, out, expect);
        free(out);
    }
    return true;
}

// Pull the twelve textures out of their three blocks. The result is one
// contiguous buffer in cn_tex.c's slot order, so the overlay/restore code keeps
// working with a single base pointer plus offsets.
static bool load_textures(void) {
    u8 *scratch = NULL;
    u32 i;
    u32 dstOff = 0;
    u32 cachedBlock = 0;
    u8 *blockBuf = NULL;
    bool ok = false;

    sTextures = (u8 *)malloc(CN_ROM_TEXTURE_BYTES);
    if (sTextures == NULL) { goto done; }

    for (i = 0; i < CN_ROM_TEXTURE_COUNT; i++) {
        const struct CnRomTexDesc *d = &sTexDesc[i];

        if (blockBuf == NULL || d->block != cachedBlock) {
            u32 blockSize;

            // Only three distinct blocks; pick the size from whichever entry
            // belongs to this block (all entries of one block share it).
            switch (d->block) {
                case 0x1EC940: blockSize = 209680; break;
                case 0x293360: blockSize = 56928;  break;
                case 0x25C280: blockSize = 30208;  break;
                default: goto done;
            }
            if (blockBuf == NULL) {
                blockBuf = (u8 *)malloc(209680);
                if (blockBuf == NULL) { goto done; }
            }
            if (!load_block(d->block, blockBuf, blockSize, &scratch)) { goto done; }
            cachedBlock = d->block;
        }

        memcpy(sTextures + dstOff, blockBuf + d->off, d->size);
        dstOff += d->size;
    }

    ok = (dstOff == CN_ROM_TEXTURE_BYTES);

done:
    free(scratch);
    free(blockBuf);
    if (!ok) {
        free(sTextures);
        sTextures = NULL;
    }
    return ok;
}

bool cn_rom_assets_load(const char *romPath) {
    u8  *scratch = NULL;
    bool ok = false;

    if (sSeg2 != NULL) { return true; }
    if (romPath == NULL) { return false; }

    sRomFile = fopen(romPath, "rb");
    if (sRomFile == NULL) { return false; }

    // --- segment 2: glyphs, HUD cells, dialogs, names ----------------------
    sSeg2 = (u8 *)malloc(CN_ROM_SEG2_SIZE);
    if (sSeg2 == NULL) { goto done; }
    if (!load_block(CN_ROM_SEG2_MIO0_OFF, sSeg2, CN_ROM_SEG2_SIZE, &scratch)) { goto done; }
    free(scratch);
    scratch = NULL;

    if (!validate_segment(sSeg2)) { goto done; }

    // Expand the glyphs once so the renderer keeps its flat 32-bytes-per-glyph
    // table. 34 KB of heap, and it keeps the hot path a single index.
    sGlyphs = (u8 *)malloc(CN_ROM_GLYPH_BYTES);
    if (sGlyphs == NULL) { goto done; }
    {
        u32 i;
        for (i = 0; i < CN_ROM_GLYPH_COUNT; i++) {
            build_glyph(sGlyphs + i * 32, sSeg2 + CN_ROM_GLYPH_OFF + i * 0x20);
        }
    }
    sCnGlyphs16x16 = sGlyphs;

    // --- course / act names ------------------------------------------------
    if (!build_names()) { goto done; }

    // --- the twelve localised textures ------------------------------------
    if (!load_textures()) { goto done; }

    if (getenv("CN_ROM_ASSETS_DEBUG") != NULL) {
        const u8 *d0 = cn_rom_dialog_str(0);
        fprintf(stderr, "cn_rom_assets: glyph FNV %08X, hud0 FNV %08X, "
                        "dialog0 FNV %08X, tex FNV %08X, "
                        "course FNV %08X, act FNV %08X\n",
                fnv1a(sGlyphs, CN_ROM_GLYPH_BYTES),
                fnv1a(cn_rom_hud_cell(0), CN_ROM_HUD_CELL_SIZE),
                (d0 != NULL) ? fnv1a(d0, 256) : 0,
                fnv1a(sTextures, CN_ROM_TEXTURE_BYTES),
                fnv1a(&sCourseNames[0][0], (u32)sizeof(sCourseNames)),
                fnv1a(&sActNames[0][0], (u32)sizeof(sActNames)));
    }

    ok = true;

done:
    free(scratch);
    if (!ok) {
        free(sSeg2);
        free(sGlyphs);
        free(sTextures);
        sSeg2 = NULL;
        sGlyphs = NULL;
        sTextures = NULL;
        sCnGlyphs16x16 = NULL;
        if (sRomFile != NULL) {
            fclose(sRomFile);
            sRomFile = NULL;
        }
    }
    return ok;
}

const u8 *cn_rom_glyphs(void) {
    return sGlyphs;
}

const u8 *cn_rom_hud_cell(u32 index) {
    if (sSeg2 == NULL || index >= CN_ROM_HUD_COUNT) { return NULL; }
    return sSeg2 + CN_ROM_HUD_OFF + index * CN_ROM_HUD_CELL_SIZE;
}

u32 cn_rom_dialog_count(void) {
    return CN_ROM_DIALOG_COUNT;
}

static const u8 *dialog_entry(u32 index) {
    if (sSeg2 == NULL || index >= CN_ROM_DIALOG_COUNT) { return NULL; }
    return sSeg2 + CN_ROM_DIALOG_OFF + index * CN_ROM_DIALOG_STRIDE;
}

u32 cn_rom_dialog_lines(u32 index) {
    const u8 *e = dialog_entry(index);
    return (e != NULL) ? e[CN_ROM_DIALOG_LINES] : 0;
}

u32 cn_rom_dialog_left(u32 index) {
    const u8 *e = dialog_entry(index);
    return (e != NULL) ? read_be16(e + CN_ROM_DIALOG_LEFT) : 0;
}

u32 cn_rom_dialog_width(u32 index) {
    const u8 *e = dialog_entry(index);
    return (e != NULL) ? read_be16(e + CN_ROM_DIALOG_WIDTH) : 0;
}

const u8 *cn_rom_dialog_str(u32 index) {
    const u8 *e = dialog_entry(index);
    u32 addr;

    if (e == NULL) { return NULL; }
    addr = read_be32(e + CN_ROM_DIALOG_STR);
    if (addr < CN_ROM_SEG_BASE || addr - CN_ROM_SEG_BASE >= CN_ROM_SEG2_SIZE) {
        return NULL;
    }
    return sSeg2 + (addr - CN_ROM_SEG_BASE);
}

const u8 *cn_rom_course_name(u32 slot) {
    if (sSeg2 == NULL || slot >= CN_ROM_COURSE_COUNT) { return NULL; }
    return sCourseNames[slot];
}

const u8 *cn_rom_act_name(u32 index) {
    if (sSeg2 == NULL || index >= CN_ROM_ACT_MAIN_COUNT) { return NULL; }
    return sActNames[index];
}

const u8 *cn_rom_seg(u32 offset) {
    if (sSeg2 == NULL || offset >= CN_ROM_SEG2_SIZE) { return NULL; }
    return sSeg2 + offset;
}

const u8 *cn_rom_texture(u32 index) {
    u32 offset = 0;
    u32 i;

    if (sTextures == NULL || index >= CN_ROM_TEXTURE_COUNT) { return NULL; }
    for (i = 0; i < index; i++) { offset += sTexDesc[i].size; }
    return sTextures + offset;
}

bool cn_rom_read(u32 offset, void *dst, u32 size) {
    if (sRomFile == NULL || dst == NULL) { return false; }
    if (fseek(sRomFile, (long)offset, SEEK_SET) != 0) { return false; }
    return fread(dst, 1, size, sRomFile) == size;
}
