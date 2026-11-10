// Course-name table.
//
// The Chinese (iQue) names used to be compiled in here, one array per slot
// (text/cn_courses_cn.inc.c). They are read out of the player's own 神游 dump
// at startup now -- see src/game/cn_rom_assets.c -- so this file defines only
// the vanilla ROM-backed buffers and the three pointer tables built from them:
//
//   seg2_course_name_table   live table; apply_cn_text_pack() overlays the
//                            loader's names on top of these when the pack is on
//   cn_course_name_vanilla   the "off" side of that switch
//   COURSE_TABLE_original    untouched list, for Lua and the restore paths
//
// Initialising the live table with the vanilla buffers (rather than the CN
// pointers, as the compiled-in version did) costs nothing -- apply_cn_text_pack()
// runs before anything draws -- and it makes the "no rom" case fall out for
// free: the loader returns NULL, the slot keeps its English string.

// Every table below re-defines the same three macros; undef them first so the
// preprocessor does not warn about a non-identical redefinition.
#undef COURSE_ACTS
#undef SECRET_STAR
#undef CASTLE_SECRET_STARS

// Vanilla ROM-backed course-name buffers. The course source used here is
// always the vanilla (numeric-size) one, even in CN builds, so [name] is the
// correct bound.
#define COURSE_ACTS(id, name, a,b,c,d,e,f) \
    static u8 GLUE2(COURSE_TABLE, _ ## id)[name] = { 0xFF };

#define SECRET_STAR(id, name) \
    static u8 GLUE2(COURSE_TABLE, _ ## id)[name] = { 0xFF };

#define CASTLE_SECRET_STARS(str) \
    static u8 GLUE2(COURSE_TABLE, _castle_secret_stars)[str] = { 0xFF };

#define EXTRA_TEXT(id, str)

#include "courses.h"

#undef COURSE_ACTS
#undef SECRET_STAR
#undef CASTLE_SECRET_STARS

// Active table: starts on the vanilla ROM buffers in every build.
// Non-const so apply_cn_text_pack() can repoint each course slot.
#define COURSE_ACTS(id, name, a,b,c,d,e,f) (u8 *)(void *)GLUE2(COURSE_TABLE, _ ## id),
#define SECRET_STAR(id, name) (u8 *)(void *)GLUE2(COURSE_TABLE, _ ## id),
#define CASTLE_SECRET_STARS(str) (u8 *)(void *)GLUE2(COURSE_TABLE, _castle_secret_stars),

u8* COURSE_TABLE[] = {
#include "courses.h"
    NULL
};

// Vanilla mirror: the "off" side of the switch (a no-op distinction in
// non-CN builds, where it aliases the same buffers).
#undef COURSE_ACTS
#undef SECRET_STAR
#undef CASTLE_SECRET_STARS
const u8 *const cn_course_name_vanilla[] = {
#define COURSE_ACTS(id, name, a,b,c,d,e,f) \
    (const u8 *)(void *)GLUE2(COURSE_TABLE, _ ## id),
#define SECRET_STAR(id, name) \
    (const u8 *)(void *)GLUE2(COURSE_TABLE, _ ## id),
#define CASTLE_SECRET_STARS(str) \
    (const u8 *)(void *)GLUE2(COURSE_TABLE, _castle_secret_stars),

#include "courses.h"
    NULL
};

#undef COURSE_ACTS
#undef SECRET_STAR
#undef CASTLE_SECRET_STARS

// "original" table keeps unmodified vanilla names for Lua / restore paths.
#define COURSE_ACTS(id, name, a,b,c,d,e,f) GLUE2(COURSE_TABLE, _ ## id),
#define SECRET_STAR(id, name) GLUE2(COURSE_TABLE, _ ## id),
#define CASTLE_SECRET_STARS(str) GLUE2(COURSE_TABLE, _castle_secret_stars),

const u8* GLUE2(COURSE_TABLE, _original)[] = {
#include "courses.h"
    NULL
};

#undef COURSE_ACTS
#undef SECRET_STAR
#undef CASTLE_SECRET_STARS
