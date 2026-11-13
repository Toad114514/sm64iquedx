// cn_romhack.h - "is one of the enabled mods a romhack?", reduced to the two
// questions that can be answered about a single mod without touching the mod
// loader.
//
// Header-only and free of engine types on purpose: this is the part of the
// Chinese pack's gate that decides whether a player gets any text at all, and
// it used to guess from directory names (levels/ + actors/ + data/). That
// guess turned the pack off for a player whose only "romhack" was a character
// skin, so the rule is now:
//
//   * the mod's own metadata says `-- incompatible: romhack`, or
//   * the mod ships a levels/ directory (level data re-orders the text ids the
//     iQue tables are addressed by, whether or not the author filled in the
//     metadata).
//
// An actor model or a behavior script is *not* a romhack here: neither touches
// the level list, the course/act tables or the text id space, so the iQue data
// still lines up. See pc_main.c for the full rationale, and
// .workbuddy/test_cn_romhack.sh for the table this file is pinned by.

#ifndef CN_ROMHACK_H
#define CN_ROMHACK_H

// ASCII-only, because this also has to build in the host-side tests next to a
// two-line types.h stub.
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// Is `token` (not NUL-terminated, `len` bytes) the word "romhack", ASCII
// case-insensitively?
//
// The field is space separated ("gamemode nametags"), so this compares whole
// tokens rather than substrings: "noromhack" must not match, and neither must
// "romhack2".
static inline bool cn_token_is_romhack(const char *token, size_t len) {
    static const char sWord[] = "romhack";
    size_t i;

    if (token == NULL || len != sizeof(sWord) - 1) { return false; }

    for (i = 0; i < len; i++) {
        char c = token[i];
        if (c >= 'A' && c <= 'Z') { c = (char)(c - 'A' + 'a'); }
        if (c != sWord[i]) { return false; }
    }
    return true;
}

// Does a mod's `-- incompatible:` metadata name romhack? NULL / empty / a
// different name (gamemode, FloodNightmare, Flood+MusicPack, ...) all mean no.
static inline bool cn_incompatible_is_romhack(const char *incompatible) {
    const char *p = incompatible;

    if (p == NULL) { return false; }

    while (*p != '\0') {
        const char *start;

        while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';') { p++; }
        start = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != ',' && *p != ';') { p++; }

        if (cn_token_is_romhack(start, (size_t)(p - start))) { return true; }
    }
    return false;
}

// Does this path (relative to the mod root, as the mod loader stores it) live
// under levels/?
//
// The prefix is compared with its trailing slash so a mod that merely has a
// file *named* `levels` cannot match.
static inline bool cn_path_ships_levels(const char *relativePath) {
    return relativePath != NULL && strncmp(relativePath, "levels/", 7) == 0;
}

#endif // CN_ROMHACK_H
