**English** | [简体中文](README.md)

# Patch index

Every patch sits **flat in `patches/`**. The filename is a four-digit number plus a group name plus a
responsibility; **the number is the application order** (`sort` gives the order, the scripts keep no separate
list). Groups exist only in the filename: optional groups are selected by a filename glob (`--frontend` →
`0007-*.patch`, android → `0008-*.patch`), so optional patches do not need an extra directory level.

| Patch file | Files | Targets | Purpose |
| --- | --- | --- | --- |
| `0001-build-and-config.patch` | 3 | all four upstreams | `CN_TEXT_PACK` switch in `Makefile` + the three `configChineseText*` config entries |
| `0002-rom-assets.patch` | 3 | shared | iQue ROM detection (`rom_checker.h`), startup wiring (`pc_main.c`), segmented asset reading (`rom_assets.c`). `mods/mods.c` was restored to stock upstream when "romhack ⇒ disable the whole pack" was removed, so it is no longer part of any patch |
| `0003-text-core.patch` | 8 | shared | Two-byte encoding / glyph core, HUD and title text (`print.c` / `area.c`), course and act name tables (`level_info.[ch]`), swapping the text table on level entry (`level_update.c`), `text/define_*.inc.c` |
| `0004-dialog.patch` | 5 | shared | Single-table dialog architecture (`dialog_table.[ch]`), dialog rendering plus paging / box height (`ingame_menu.[ch]`), Lua-side replacement |
| `0005-menus.patch` | 5 | shared | File select, star select, title cutscene (including the iQue title grid) |
| `0006-asset-overlay.patch` | 2 | shared | Consumer side of the iQue texture / geometry overlay: power-meter wheel offset (HUD layer + Lua binding) |
| `0007-djui-frontend.patch` | 3 | all three targets, **optional** | "Chinese text pack" toggle under Settings → Other; `create_ex()` in `djui_panel_confirm.[ch]` (currently has no call sites) |
| `0008-android.patch` | 3 | `android` / `android-cn` **shared** | Android platform delta (`Android.mk` / `Makefile` / `pc_main.c`) |
| `cn_extra/` | 19 | all three targets | Files that do not exist upstream; fill gaps only, never overwrite |

`0001`~`0006` are mandatory and shared by all four upstreams; `0007` is applied only with `--frontend`; `0008` is
**one single file** shared by both Android targets. Adding a group means adding a file with a larger number — no
group table in the applier to update. Language strings and the Chinese config entry are **not patches**: they are
inserted at apply time by `scripts/add_lang_strings.sh` / `scripts/add_cn_config.sh`.

The split totals 29 files (`0001`~`0007` combined). Every `scripts/gen_all_patches.sh` run diffs against the full
unfiltered change set and asserts the groups are **complete and non-overlapping**; it also rejects two leftovers
that would apply the same change twice — the pre-split single-file patches (`0001-src-overlay.patch` and friends)
and the old per-directory groups (`01-build/` … `08-platform/`).

`upstream.txt` in the repository root maps the four upstreams to their target names; `pc` covers both stock CoopDX
and 上善若's localised PC build.

## Why language strings are not a patch

Only two lines need to be added:

```
CHINESE_TEXT_PACK = "..."    # [MISC] section, right after DISABLE_POPUPS
Chinese = "..."              # [LANGUAGE] section, right after Spanish
```

But every upstream ships a different ini: the PC baseline has 456 lines, stock Android 468, and the localised
Android 457 lines **already containing `Chinese = ...`** (adding it again would duplicate the entry). Generating
one ini patch per target means maintaining three nearly identical diffs with different line numbers and context —
and regenerating all of them whenever upstream moves a line.

So it is done by **key insertion** instead, one script for all four upstreams:

```bash
scripts/add_lang_strings.sh <upstream root> [--check]
```

| Behaviour | Detail |
| --- | --- |
| Key already present | Skipped (matched with `^KEY[[:space:]]*=`, line numbers are irrelevant) |
| Anchor present | Inserted right after the anchor line, so section membership is correct by construction |
| Anchor missing | **Errors out** instead of guessing a position |
| Scope | Only `lang/English.ini` and `lang/Chinese.ini`; other language files are left untouched |
| Text | `English.ini` → `CHINESE_TEXT_PACK = "Chinese Text Pack (iQue)"`, `Chinese = "Chinese (中文)"`; `Chinese.ini` → `CHINESE_TEXT_PACK = "中文字库（神游）"`, `Chinese = "中文"` |

`--check` reports the entries to insert without writing anything. Re-running is always safe (existing keys are
skipped). Running it once against the PC baseline produces a file **byte-identical** to the experimental
workspace's `lang/English.ini`, which is why that file is not kept in `src/` — the script is the single source of
truth for these strings.

Each group excludes the whole `lang/` prefix at generation time (enforced by the generator, not by the applier's
`--exclude`), so no patch ever touches an ini on any upstream, and language handling stays in exactly one place.

> Both strings are harmless even without the optional `0007-djui-frontend.patch` (you just gain or lose an entry
> that DLANG references).

## Platform delta patch

There is **only one** `0008-android.patch`, shared by both Android targets, touching 3 files: `Android.mk`,
`Makefile`, `src/pc/pc_main.c`. It contains **no ini and no `configfile.c`**.

| File | Why |
| --- | --- |
| `Android.mk` | Injects `LOCAL_CFLAGS += -DSM64COOPDX_CN_TEXT=1` (with a `CN_TEXT_PACK=false` escape hatch). On PC the macro comes from `Makefile`, but **ndk-build never reads Makefile**; without the injection every `#if defined(SM64COOPDX_CN_TEXT)` block is compiled out and the localisation silently does nothing |
| `Makefile` | The PC change after the three-way merge. It has no effect on ndk-build and is kept only for parity with PC; `CN_TEXT_DEPS` still matters for generating `text/define_*.inc.c` |
| `src/pc/pc_main.c` | Android switches the `SDL_main` / `TARGET_ANDROID` signature branch in the same place where Chinese init happens |

The two "context drift" files — `Makefile` (in `0001-build-and-config.patch`) and `pc_main.c` (in
`0002-rom-assets.patch`) — are skipped by `--exclude` while applying `0001`~`0006`, and the platform patch supplies
their final state directly. That final state comes from a three-way merge:

```
base   = PC 8cd6e59
ours   = the matching Android upstream
theirs = this repo's src/ (the PC final state)
```

`pc_main.c` is an **insert-at-the-same-spot-by-both-sides** case, resolved by a fixed rule; any other conflict makes
the generator abort instead of quietly emitting a broken patch:

| File | Android side | PC side | Resolution |
| --- | --- | --- | --- |
| `pc_main.c` | `#ifdef TARGET_ANDROID / int SDL_main / #else` | Chinese text pack init | PC function block first, Android signature branch after |

The order cannot be flipped: if the PC init function ended up inside the `#else` branch, the preprocessor structure
would fall apart.

`Android.mk` collects sources with a wildcard over `SRC_DIRS`, so the `.c`/`.cpp` files in `cn_extra/` are picked up
automatically; no source list needs editing.

### Why `configfile.c` is not part of the platform patch (the old `0006` is gone)

It used to be one patch per target: `0005` for `android`, `0006` for `android-cn`. Diffing the two line by line
showed only **24 differing lines**, all inside the two `configfile.c` hunks — the 6 inserted lines were **identical**;
only the surrounding context differed:

```diff
-    {.name = "touch_snap",     .type = ..., .boolValue = &configSnapTouch},
+    {.name = "touch_snap",     .type = ..., .boolValue = &configElementSnap},
```

Stock Android upstream still has the older `configSnapTouch`, while the localised Android build has already moved to
`configElementSnap`. The patch was not wrong; the mistake was **maintaining two nearly identical patches for one bit
of context drift** — every upstream line change meant regenerating both, and it was easy to update only one.

It is now an idempotent insertion script, `scripts/add_cn_config.sh` (anchor-based, skips existing entries,
`--check` reports only). Those 6 lines were identical anyway, so what gets inserted is identical too. Verified
byte-for-byte on three trees:

| Scenario | Result |
| --- | --- |
| `android` upstream + script | == final `configfile.c` produced by the old `0005` |
| `android-cn` upstream + script | == final state produced by the old `0006` |
| PC baseline + script | == this repo's `src/src/pc/configfile.c` (PC final state) |

Anchors: the declarations go **after** the `configLanguage[MAX_CONFIG_STRING]` line, the config-table entries
**before** the `{.name = "force_4by3"` line. Each of those substrings occurs exactly once in all three
`configfile.c` files (the script uses `grep -F` with fixed strings rather than a regex — `\{` starts an interval in
BRE and GNU grep would fail with "Unmatched \{ ").

`configfile.c` stays in the `--exclude` list (the `0001-build-and-config.patch` hunk really does not apply there);
it is just no longer the patch that supplies its final state.

### Upstream differences that stay out of the platform patch

`src/pc/configfile.h` (in `0001-build-and-config.patch`) and `src/pc/lua/utils/smlua_misc_utils.c` (in
`0006-asset-overlay.patch`) differ between the two Android upstreams and the PC baseline, but their hunk context is
intact, so they apply directly. After applying they equal "Android upstream + the PC change", which is
**expected** to differ slightly from the PC final state; it has been verified that all 10 added lines are present in
both files (including `cn_tex_gauge_dy()`).

The localised Android upstream's own `palettes/*.ini`, `src/pc/djui/*_glyphs.h` and `textures/custom_font/*.png` are
part of that upstream. The patches neither modify nor vendor them.

## cn_extra/ holds new files — fill gaps only, never overwrite

`cn_extra/` contains 19 files that do not exist upstream, including **`lang/Chinese.ini`**, which the PC upstream's
`lang/` directory lacks (it ships only 11 languages; the localised Android build has its own). So application is
decided per file: skip if the target already exists, copy only what is missing.

```bash
# semantics inside scripts/apply_patches.sh
[ -e "$target/$rel" ] && keep upstream's $rel || copy
```

This is not redundant caution: the localised Android build ships a `lang/Chinese.ini` that is *not* identical to the
PC one (it has its own touch-control entries), and a blanket `cp -R` would clobber it. Verified with sentinel
content: after applying, the upstream file survives and only gains one line after `DISABLE_POPUPS`. Stock Android
upstream has no `lang/Chinese.ini` at all, so it gets the PC one — its touch-control strings fall back to English,
which is an acceptable trade-off.

## djui_modern compatibility

[`Shangshanruo666/djui_modern`](https://github.com/Shangshanruo666/djui_modern) (`djmd_commit` in `upstream.txt`) is
a module that lives under `src/pc/djui_modern` and applies 9 desktop / 10 Android patches to upstream djui files. It
has **no file-level overlap** with this repo's patches: it touches `djui.c`, `djui_rect.c`, `djui_gfx.c`,
`djui_hud_utils.c`, `djui_unicode.c`, `djui_text.c`, `djui_panel_host_mods.c`, `gfx_dxgi.cpp`; we touch
`djui_panel_confirm.[ch]` and `djui_panel_misc.c`.

The only shared files are build files, and the edit sites are far apart so they apply with offsets:

| File | Ours | djui_modern | Result |
| --- | --- | --- | --- |
| `Makefile` | `~L194` adds `CN_TEXT_PACK`, `~L454` adds a flip guard | `~L470` `SRC_DIRS` gains `src/pc/djui_modern` | Applies (disjoint context) |
| `Android.mk` | `~L106` adds `SM64COOPDX_CN_TEXT` after `LOCAL_CFLAGS` | `~L70` `SRC_DIRS` gains `src/pc/djui_modern` | Applies |

Measured with the baselines from `upstream.txt` (desktop both directions, each Android upstream once): all of
`0001`~`0008` together with djui_modern's 9 desktop patches and 2 Android build patches apply cleanly **in both
directions with zero conflicts**.

**The patches do not conflict, but the module has a hard dependency.** `djui_modern_font_atlas.c` contains:

```c
static const DjuiModernCnGlyph kCnGlyphs[] = {
#include "pc/djui/cn_glyphs.h"
};
```

It includes `src/pc/djui/cn_glyphs.h` as an **array initialiser list**, and that file is **shipped by the localised
upstream** (a 6790-line GB2312 table). So installing djui_modern on **stock PC / stock Android** fails at compile
time — the `#include` cannot be resolved. Only the two localised upstreams build.

**This repo does not vendor that table.** It used to ship a 1061-line stand-in in `cn_extra/`, generated from the
iQue glyph table (`cn_map.json`, the same table as `cn_data/cn_glyphs_16x16.h`). Removed, because:

- djui_modern's author (上善若) has said upstream compatibility is coming from their side; at that point the file
  will either live in the module repo or be handled by its build script. A *patch* repo providing it would only
  collide with their eventual solution.
- That table is a subset of the iQue table and only makes sense in this project's context; on someone else's stock
  upstream it means nothing.

So `install.sh` **hard-rejects** the combination instead of patching around it:

```bash
$ sh install.sh -d -p zip
[!] djui_modern 只能配汉化上游：它的字体图集里 #include "pc/djui/cn_glyphs.h"，
[!] 这个文件由汉化上游（PC cn-repo / Android android-cn-repo）自带。
[!] 本项目不再收录该字表（等 djui_modern 上游自己兼容原版上游）。
[x] 请加 -c 使用汉化上游，或去掉 -d/--djmd。
```

The rule is simply "`-d` requires `-c`" (the interactive wizard only asks about djui_modern after you pick the
localised upstream). Once djui_modern supports stock upstream, the restriction can be deleted outright; until then,
stock upstream users must supply their own `src/pc/djui/cn_glyphs.h`.

The 6790-line table that ships with the localised upstream is left untouched: `cn_extra/` only fills gaps.

## Applying

Always go through the applier. It is idempotent — applied patches are recognised and skipped, existing strings are
skipped:

```bash
scripts/apply_patches.sh <pc|android|android-cn> <clean upstream checkout> [--frontend]
```

Internal order:

| Target | Order |
| --- | --- |
| `pc` | `0001`~`0006` (numeric order) → [`0007`] → `cn_extra/` (fill gaps) → `add_lang_strings.sh` |
| `android` | `0001`~`0006` (3 drift files skipped) → `0008` → [`0007`] → `cn_extra/` (fill gaps) → `add_cn_config.sh` → `add_lang_strings.sh` |
| `android-cn` | Same as `android` (**the same** `0008-android.patch`; only the upstream's own files differ) |

`[]` means applied only with `--frontend`. The numbering in the filenames makes the order unique: within a group,
patches sort by filename; across groups, by group name. The language script **must run after `cn_extra/`**: on PC,
`lang/Chinese.ini` comes from `cn_extra/`. Do **not** overwrite the final state with `cp -R src/.` — that would pull
in the frontend unconditionally and destroy the Android platform delta.

The exclusion list lives in both `scripts/gen_platform_patches.py` (`EXCLUDED`) and `scripts/apply_patches.sh`, and
the generator asserts they match — **changing one means changing the other**.

## Regenerating

```bash
# 1. After editing src/, regenerate all shared groups 0001~0007
sh scripts/gen_all_patches.sh <PC 8cd6e59 baseline directory>

# 2. Regenerate 0008-android.patch (Android upstream checkouts per upstream.txt)
python3 scripts/gen_platform_patches.py \
    --pc-base    <PC 8cd6e59 baseline directory> \
    --android    <ManIsCat2 upstream directory> \
    --android-cn <localised Android upstream directory>
```

Step 1 asserts that `0001`~`0007` cover the whole `src/` delta (nothing missing, nothing duplicated), that none of
them contain `lang/`, and that no old group directories or pre-split single-file patches are left in `patches/`.
Step 2 self-checks: `0001`~`0006` (with the 3 drift files skipped) + the platform patch + `0007` + the two insertion
scripts equal the three-way-merged final state byte for byte (31 files each for `android` and `android-cn`), all
language strings are present, both scripts are idempotent on a second run, the 3 frontend files stay at upstream
state when `0007` is not applied, and the platform patch does not overlap `0001`~`0006`. **The generator compares
the per-file platform delta computed from both upstreams and aborts if they differ** — that would mean the two
upstreams have genuinely diverged and the file must be split into per-target patches instead of letting a shared
patch silently work on only one of them.

Changing the wording of a language string, or renaming a Chinese config entry, needs no patch regeneration: edit the
constants in `scripts/add_lang_strings.sh` / `scripts/add_cn_config.sh`.

**Always run `python3 scripts/check_patch_integrity.py` after hand-editing `0008-android.patch`.** When the two
Android baselines are not available locally, step 2 cannot run, and changes to the PC side of `pc_main.c` often can
only be mirrored into the platform patch by hand — and there are two traps that a PC build will never reveal: a
forgotten recount of `+c,d` in a hunk header (`git apply` reports "corrupt patch" with a line number far from the
actual mistake), and a header removed from `cn_extra/` while the platform patch still `#include`s it (the PC target
never compiles that branch; Android CI blows up). The script catches both without needing any baseline.

**But it cannot catch the third kind: a hunk whose context does not exist upstream at all.** That one shows up as CI
failing at "4/6 应用补丁" (step 4 of 6, applying patches) with
`0008-android.patch 无法干净应用，上游不是 upstream.txt 声明的基线？` while local PC
builds are perfectly fine (the PC target never applies `0008`). Whenever the network is available, run

```bash
sh scripts/preflight_android.sh android-cn --frontend
```

It shallow-clones the repo/commit declared in `upstream.txt` (temp directory defaults to `/var/tmp`, override with
`PREFLIGHT_TMP` — do not use `/tmp`, it is often a tiny tmpfs), runs the real `apply_patches.sh`, and deletes the
clone afterwards. Every hunk in `0008` is written against the **real baseline** context: the `Android.mk` injection
point is right after the `LOCAL_CFLAGS :=` line (matching the generator's `inject_android_mk()`, and that 113-line
file is byte-identical on both Android upstreams), while `Makefile` / `pc_main.c` are three-way-merged final states.
If upstream changes any of those three files, rerun step 2 — do not hand-edit hunks.
