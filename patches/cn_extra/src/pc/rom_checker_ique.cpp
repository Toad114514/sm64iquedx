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
// The engine still runs on the US image; this rom is a second file that the
// Chinese text pack is read out of (cn_rom_assets.c). See pc_main.c's
// chinese_text_pack_init() for how it is used.
//
// Detection is the original strict one -- 8060928 bytes and this exact MD5 --
// over two folders only: the one holding the program and the user folder. A
// dump found next to the program is copied into the user folder and read from
// there, so a dump on a removable drive or in a throwaway extraction folder
// keeps working on later launches.

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

// Where a dump found next to the program gets copied to, inside the user
// folder. A fixed name on purpose: the point of the copy is to have *one*
// known path that survives the original moving (a removable drive, a temp
// extraction folder, an upgraded install directory), so the file we pick on
// the next launch has to be the file we wrote this time.
#define IQUE_ROM_COPY_NAME "baserom.ique.z64"

static bool is_direct_child_of(const std::string &path, const char *dir) {
    std::error_code ec;
    fs::path const child = fs::weakly_canonical(path, ec);
    if (ec) { return false; }
    fs::path const parent = fs::weakly_canonical(dir, ec);
    if (ec) { return false; }
    return child.parent_path() == parent;
}

// Copy the dump into the user folder and hand back the copy's path. Returns
// false when the copy could not be written (read-only home, no space, a
// sandbox), in which case the caller keeps using the original location: the
// pack is still readable there, so a failed copy must not disable it.
static bool copy_into_user_folder(const std::string &src, const char *dst, std::string *out) {
    if (dst == NULL) { return false; }

    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) { return false; }

    // Hash the copy rather than trusting the write: a half-flushed file would
    // be picked up as "the dump" next launch and then fail every anchor check.
    if (!is_ique_rom(dst)) { return false; }

    *out = dst;
    return true;
}

// `depth` is how many levels of subdirectories to descend into. The game folder
// is scanned with depth 1 because a dumped release usually arrives as its own
// folder (iQue_Mario_CHN_iQP-SUXXORS/0010cd30.z64) rather than loose at the
// root, while mod folders one level deeper are not worth walking.
static bool scan_dir_for_ique_rom(const std::string &dir, int depth, std::string *out) {
    std::error_code ec;
    fs::directory_iterator it(dir, ec), itEnd;
    if (ec) { return false; }

    for (; it != itEnd; it.increment(ec)) {
        if (ec) { break; }

        const fs::path &entryPath = it->path();
        std::error_code typeEc;
        if (it->is_directory(typeEc)) {
            if (depth > 0 && !typeEc && scan_dir_for_ique_rom(entryPath.generic_string(), depth - 1, out)) {
                return true;
            }
            continue;
        }

        std::string path = entryPath.generic_string();
        if (path_ends_with(path.c_str(), ".z64") && is_ique_rom(path)) {
            if (out != NULL) { *out = path; }
            return true;
        }
    }
    return false;
}

// Is a genuine 神游 rom available next to the game or in the user folder, and
// if so, where is it?
//
// Cached: the answer cannot change during a session, and both the startup path
// and the settings panel ask for it. The path is cached separately from the
// boolean because the game now streams the text pack out of this file.
static std::string sIqueRomPath;
static bool        sIqueRomScanned = false;
static bool        sIqueRomPresent = false;

// Only two folders are ever scanned: the one holding the program, and this
// port's user folder (~/.local/share/sm64coopdx on Linux, %APPDATA% on
// Windows). Deliberately nowhere else -- not the working directory, not the
// mod folders, not the save folder. A full 8 MB hash per candidate is cheap
// only because the candidate list is this short.
static void find_ique_rom(void) {
    if (sIqueRomScanned) { return; }
    sIqueRomScanned = true;

    // fs_get_write_path() returns a pointer into one static buffer, so the
    // folder has to be copied out before it is called a second time for the
    // destination path below.
    const char *writeRaw = fs_get_write_path("");
    std::string const writePath = (writeRaw != NULL) ? writeRaw : "";
    const char *exePath = sys_exe_path_dir();

    std::string found;

    // The user folder wins when both have one: if a previous launch already
    // copied the dump here, that is the file to keep using.
    if (!writePath.empty() && scan_dir_for_ique_rom(writePath, 1, &found)) {
        sIqueRomPath = found;
        sIqueRomPresent = true;
        return;
    }

    if (exePath == NULL || !scan_dir_for_ique_rom(exePath, 1, &found)) { return; }

    // Found next to the program. Load it from there even if the copy fails;
    // the copy is an attempt to make it survive, not a precondition.
    sIqueRomPath = found;
    sIqueRomPresent = true;

    if (writePath.empty() || is_direct_child_of(found, writePath.c_str())) { return; }

    // fs_get_write_path() joins the user folder and this name the same way the
    // US rom's copy path is built in rom_checker.cpp.
    std::string copy;
    if (copy_into_user_folder(found, fs_get_write_path(IQUE_ROM_COPY_NAME), &copy)) {
        sIqueRomPath = copy;
    }
}

bool rom_checker_ique_rom_present(void) {
    find_ique_rom();
    return sIqueRomPresent;
}

const char *rom_checker_ique_rom_path(void) {
    find_ique_rom();
    return sIqueRomPresent ? sIqueRomPath.c_str() : NULL;
}
