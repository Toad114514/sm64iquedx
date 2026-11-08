#if defined(SM64COOPDX_CN_TEXT)
// Simplified-Chinese (iQue) course name data.
#include "text/cn_courses_cn.inc.c"

// COURSE_BOB etc. are *enum* constants, not preprocessor macros, so they can
// never be pasted into an identifier with ##. These aliases map each course
// enum token to the lower-case name used by the generated CN arrays.
#define CN_CID_COURSE_BOB      course_bob
#define CN_CID_COURSE_WF       course_wf
#define CN_CID_COURSE_JRB      course_jrb
#define CN_CID_COURSE_CCM      course_ccm
#define CN_CID_COURSE_BBH      course_bbh
#define CN_CID_COURSE_HMC      course_hmc
#define CN_CID_COURSE_LLL      course_lll
#define CN_CID_COURSE_SSL      course_ssl
#define CN_CID_COURSE_DDD      course_ddd
#define CN_CID_COURSE_SL       course_sl
#define CN_CID_COURSE_WDW      course_wdw
#define CN_CID_COURSE_TTM      course_ttm
#define CN_CID_COURSE_THI      course_thi
#define CN_CID_COURSE_TTC      course_ttc
#define CN_CID_COURSE_RR       course_rr

// Secret-star and castle-star slots occupy indices 15..25 of the same table.
// Missing these was the pause-menu segfault: the slots fell through to the
// vanilla {0xFF, 0x00...} ROM stubs, which the CN tokenizer mis-parses.
#define CN_CID_COURSE_BITDW    course_bitdw
#define CN_CID_COURSE_BITFS    course_bitfs
#define CN_CID_COURSE_BITS     course_bits
#define CN_CID_COURSE_PSS      course_pss
#define CN_CID_COURSE_COTMC    course_cotmc
#define CN_CID_COURSE_TOTWC    course_totwc
#define CN_CID_COURSE_VCUTM    course_vcutm
#define CN_CID_COURSE_WMOTR    course_wmotr
#define CN_CID_COURSE_SA       course_sa
#define CN_CID_COURSE_CAKE_END course_cake_end

#define CN_COURSE_NAME__(id) (&cn_course_ ## id ## _0)
#define CN_COURSE_NAME_(id)  CN_COURSE_NAME__(id)
#define CN_COURSE_NAME(id)   CN_COURSE_NAME_(CN_CID_ ## id)
#endif

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

// Active table: CN data when the pack is on, vanilla ROM buffer when off.
// Non-const so apply_cn_text_pack() can repoint each course slot.
#if defined(SM64COOPDX_CN_TEXT)
#define COURSE_ACTS(id, name, a,b,c,d,e,f) (u8 *)(void *)CN_COURSE_NAME(id),
#else
#define COURSE_ACTS(id, name, a,b,c,d,e,f) (u8 *)(void *)GLUE2(COURSE_TABLE, _ ## id),
#endif
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

#if defined(SM64COOPDX_CN_TEXT)
// CN mirror: the "on" side of the switch.
#undef COURSE_ACTS
#undef SECRET_STAR
#undef CASTLE_SECRET_STARS
const u8 *const cn_course_name_cn[] = {
#define COURSE_ACTS(id, name, a,b,c,d,e,f) (const u8 *)CN_COURSE_NAME(id),
#define SECRET_STAR(id, name) (const u8 *)CN_COURSE_NAME(id),
#define CASTLE_SECRET_STARS(str) (const u8 *)(&cn_course_course_castle_secret_stars),

#include "courses.h"
    NULL
};

#undef COURSE_ACTS
#undef SECRET_STAR
#undef CASTLE_SECRET_STARS
#endif

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

