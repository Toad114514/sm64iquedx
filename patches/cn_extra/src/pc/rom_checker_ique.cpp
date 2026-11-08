// Presence test for the 神游 (iQue) Chinese rom.
//
// Kept in its own translation unit, separate from rom_checker.cpp, for one
// practical reason: this is pure filesystem + hashing work with exactly three
// engine dependencies, so it can be compiled against stubs and exercised by
// .workbuddy/test_rom_detect/ on a synthetic folder layout (root, one-level
// dump folder, wrong size, right size with wrong bytes). A bug in the scan
// would otherwise only show up as "the pack did not turn on" on a player's
// machine.
//
// The engine still runs on the US image; this rom is only a signal of
// ownership. See pc_main.c's chinese_text_pack_init() for how it is used.

#include <filesystem>
#include <sstream>

extern "C" {
#include "platform.h"
#include "mods/mods_utils.h" // for path_ends_with
#include "mods/mod_cache.h"  // for md5 hashing
#include "fs/fs.h"
#include "rom_checker.h"
}

namespace fs = std::filesystem;

// MD5 of the Chinese localisation of SM64 ("神游马力欧64", iQue). This is the
// exact dump the sm64decomp-cn project is based on, i.e.
// iQue_Mario_CHN_iQP-SUXXORS/0010cd30.z64 (8060928 bytes, z64 byte order).
//
// Only this one is accepted. Near-miss dumps (raw 8 MB cart dumps, trimmed
// sets, byteswapped copies) hash differently, and the text pack in this build
// was extracted from *this* image -- treating "some rom that looks Chinese" as
// a match would mean enabling data derived from a different cartridge than the
// one the player supplied.
#define IQUE_ROM_MD5  "29047fba820695bd14c5bd7aa1aa4400"
#define IQUE_ROM_SIZE 8060928

static bool is_ique_rom(const std::string &romPath) {
    std::error_code ec;
    auto size = fs::file_size(romPath, ec);
    // Reject on size before hashing: an 8 MB MD5 per candidate .z64 in the
    // game folder would be a visible startup cost, and this check rules out
    // the US rom and every partial dump for free.
    if (ec || size != IQUE_ROM_SIZE) { return false; }

    u8 dataHash[16] = { 0 };
    mod_cache_md5(romPath.c_str(), dataHash);

    std::stringstream ss;
    for (int i = 0; i < 16; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(dataHash[i]);
    }
    return ss.str() == IQUE_ROM_MD5;
}

// `depth` is how many levels of subdirectories to descend into. The game folder
// is scanned with depth 1 because a dumped release usually arrives as its own
// folder (iQue_Mario_CHN_iQP-SUXXORS/0010cd30.z64) rather than loose at the
// root, while mod folders one level deeper are not worth walking.
static bool scan_dir_for_ique_rom(const std::string &dir, int depth) {
    std::error_code ec;
    fs::directory_iterator it(dir, ec), itEnd;
    if (ec) { return false; }

    for (; it != itEnd; it.increment(ec)) {
        if (ec) { break; }

        const fs::path &entryPath = it->path();
        std::error_code typeEc;
        if (it->is_directory(typeEc)) {
            if (depth > 0 && !typeEc && scan_dir_for_ique_rom(entryPath.generic_string(), depth - 1)) {
                return true;
            }
            continue;
        }

        std::string path = entryPath.generic_string();
        if (path_ends_with(path.c_str(), ".z64") && is_ique_rom(path)) {
            return true;
        }
    }
    return false;
}

// Is a genuine 神游 rom available next to the game or in the user folder?
//
// Cached: the answer cannot change during a session, and both the startup path
// and the settings panel ask for it.
bool rom_checker_ique_rom_present(void) {
    static int cached = -1;
    if (cached >= 0) { return cached != 0; }

    cached = 0;
    const char *writePath = fs_get_write_path("");
    const char *exePath   = sys_exe_path_dir();

    if (writePath != NULL && scan_dir_for_ique_rom(writePath, 1)) {
        cached = 1;
    } else if (exePath != NULL && scan_dir_for_ique_rom(exePath, 1)) {
        cached = 1;
    }
    return cached != 0;
}
