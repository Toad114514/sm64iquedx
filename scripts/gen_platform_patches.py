#!/usr/bin/env python3
"""Generate the small per-Android patches.  All of them live flat in patches/.

Four upstreams share one main patch set, also flat in patches/::

    0001-src-overlay.patch        required, shared (no lang/*.ini in it)
    0002-djui-frontend.patch      optional, shared (DJUI toggle)
    0003-lang-strings.patch       language strings, shared by pc + android
    0004-lang-android-cn.patch    language strings, android-cn only
    0005-android.patch            platform delta, android only
    0006-android-cn.patch         platform delta, android-cn only
    cn_extra/                     new files, copied verbatim

``lang/*.ini`` only gains a few string entries, but every upstream has a
different ini (different line numbers, and the Chinese fork already ships a
``Chinese = ...`` entry).  They are therefore kept out of 0001 entirely and
split into 0003 (shared, generated from src/ against the PC baseline) and
0004 (Chinese Android fork, generated here against its own ini files).

Only two kinds of differences remain for the platform patches:

1. Three files drifted away from the PC baseline, so 0001 has to be applied
   with ``--exclude=...`` on Android.  The platform patch supplies their
   correct final state instead (computed by three-way merge):

       Makefile               PC build script (ndk-build never reads it)
       src/pc/configfile.c    Android inserts a TOUCH_CONTROLS config block
       src/pc/pc_main.c       Android uses SDL_main / TARGET_ANDROID branches

2. ndk-build reads ``Android.mk``, not the PC Makefile.  The
   ``SM64COOPDX_CN_TEXT`` macro therefore has to be injected into
   ``LOCAL_CFLAGS`` or every ``#if defined(SM64COOPDX_CN_TEXT)`` block is
   compiled out and the Chinese pack silently does nothing.

Three-way merge for the final state::

    base   = PC baseline (upstream.txt / 8cd6e59)
    ours   = the Android upstream file
    theirs = this repo's src/ (the PC final state)

Usage::

    scripts/gen_platform_patches.py \
        --pc-base    /path/to/pc-baseline-tree \
        --android    /path/to/ManIsCat2-sm64coopdx \
        --android-cn /path/to/sm64coopdx-Chinese-Android

Run ``scripts/gen_all_patches.sh`` first so the shared 0001/0002/0003 are current.
"""

import argparse
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

FRONTEND = "src/pc/djui/djui_panel_misc.c"
ANDROID_MK = "Android.mk"
EN_INI = "lang/English.ini"
CN_INI = "lang/Chinese.ini"
CN_INI_ANCHOR = "DISABLE_POPUPS"
CN_INI_LINE = 'CHINESE_TEXT_PACK = "中文字库（神游）"\n'

MAIN_PATCH = "0001-src-overlay.patch"
FRONTEND_PATCH = "0002-djui-frontend.patch"
LANG_PATCH = "0003-lang-strings.patch"          # pc + android
LANG_CN_PATCH = "0004-lang-android-cn.patch"    # android-cn only
PLATFORM_PATCH = {
    "android": "0005-android.patch",
    "android-cn": "0006-android-cn.patch",
}

# Files 0001 must skip on Android.  lang/*.ini is NOT here any more: 0001
# never contains it, so the applier does not need to exclude it.  Keep this
# list in sync with scripts/apply_patches.sh; the script asserts both ends.
EXCLUDED = (
    "Makefile",
    "src/pc/configfile.c",
    "src/pc/pc_main.c",
)
LANG_FILES = (EN_INI, CN_INI)

MK_INSERT = """
# iQue Deluxe: gates the bundled Simplified-Chinese text pack.
# Pass CN_TEXT_PACK=false to ndk-build to build without it (same knob as the
# PC Makefile). Without this macro every #if defined(SM64COOPDX_CN_TEXT)
# block is compiled out and the text pack silently does nothing on Android.
ifneq ($(CN_TEXT_PACK),false)
  LOCAL_CFLAGS += -DSM64COOPDX_CN_TEXT=1
endif
"""

CONFLICT = re.compile(
    r"^<<<<<<< [^\n]*\n(.*?)^=======\n(.*?)^>>>>>>> [^\n]*\n",
    re.MULTILINE | re.DOTALL,
)


def read(p: Path) -> str:
    return p.read_text(encoding="utf-8", errors="surrogateescape")


def write(p: Path, text: str) -> None:
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8", errors="surrogateescape")


def run(args, **kw):
    return subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kw)


def three_way(ours: Path, base: Path, theirs: Path, rel: str) -> str:
    r = run(["git", "merge-file", "-p", str(ours), str(base), str(theirs)])
    text = r.stdout.decode("utf-8", errors="surrogateescape")
    if r.returncode < 0:
        raise SystemExit(f"git merge-file 失败: {rel}: {r.stderr.decode(errors='replace')}")
    if r.returncode > 0:
        text = resolve_conflicts(text, rel, r.returncode)
    return text


def resolve_conflicts(text: str, rel: str, count: int) -> str:
    """Resolve the known *pure insertion* conflicts.

    Both Android forks insert code at exactly the spot where the PC patch also
    inserts code.  Keeping both sides is correct, but the order matters: the
    Chinese helpers must sit *above* the SDL_main/#else signature branch.
    Anything unexpected fails loudly instead of producing a broken merge.
    """
    blocks = list(CONFLICT.finditer(text))
    if len(blocks) != count:
        raise SystemExit(f"{rel}: 期望 {count} 个冲突块，实际 {len(blocks)} 个")

    def decide(m):
        ours, theirs = m.group(1), m.group(2)
        if rel == "src/pc/configfile.c":
            ok = "#ifdef TOUCH_CONTROLS" in ours and "chinese_text_user_set" in theirs
            return ours + theirs if ok else None
        if rel == "src/pc/pc_main.c":
            ok = "int SDL_main" in ours and "chinese_text_pack_init" in theirs
            return theirs + ours if ok else None
        return None

    out = []
    for m in blocks:
        merged = decide(m)
        if merged is None:
            raise SystemExit(
                f"{rel}: 出现未预期的三方冲突，需人工处理后再生成补丁\n"
                f"  ours  : {m.group(1)[:120]!r}\n"
                f"  theirs: {m.group(2)[:120]!r}"
            )
        out.append(merged)

    it = iter(out)
    result = CONFLICT.sub(lambda _m: next(it), text)
    if "<<<<<<<" in result or ">>>>>>>" in result:
        raise SystemExit(f"{rel}: 冲突标记未清理干净")
    print(f"  ~ 已消解 {count} 处插入冲突: {rel}")
    return result


def diff_text(a: Path, b: Path, rel: str) -> str:
    r = run(["diff", "-u", "--label", f"a/{rel}", "--label", f"b/{rel}", str(a), str(b)])
    if r.returncode >= 2:
        raise SystemExit(f"diff 失败: {rel}: {r.stderr.decode(errors='replace')}")
    return r.stdout.decode("utf-8", errors="surrogateescape")


def inject_android_mk(orig: Path) -> str:
    text = read(orig)
    if "SM64COOPDX_CN_TEXT" in text:
        raise SystemExit(f"{ANDROID_MK}: 已含 SM64COOPDX_CN_TEXT，检查上游是否已自行合入")
    lines = text.splitlines(keepends=True)
    idx = next((i for i, l in enumerate(lines) if l.startswith("LOCAL_CFLAGS :=")), None)
    if idx is None:
        raise SystemExit(f"{ANDROID_MK}: 找不到 LOCAL_CFLAGS := 行")
    lines.insert(idx + 1, MK_INSERT)
    return "".join(lines)


def inject_cn_ini(orig: Path) -> str:
    text = read(orig)
    if "CHINESE_TEXT_PACK" in text:
        return text
    lines = text.splitlines(keepends=True)
    idx = next((i for i, l in enumerate(lines) if l.startswith(CN_INI_ANCHOR)), None)
    if idx is None:
        raise SystemExit(f"{CN_INI}: 找不到 {CN_INI_ANCHOR} 锚点")
    lines.insert(idx + 1, CN_INI_LINE)
    return "".join(lines)


def pristine_copy(upstream: Path, dest: Path, files) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    for rel in files:
        (dest / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(upstream / rel, dest / rel)


def apply(cwd: Path, patch: Path, excludes=()) -> None:
    args = ["git", "apply", "-p1"] + [f"--exclude={p}" for p in excludes] + [str(patch)]
    r = run(args, cwd=cwd)
    if r.returncode != 0:
        raise SystemExit(f"应用失败 {patch.name}: {r.stderr.decode(errors='replace')}")


def build_one(name, upstream, pc_base, src_root, repo, tmp):
    print(f"\n===== {name} ({upstream}) =====")
    out_dir = repo / "patches"          # 平铺，不再建 patches/<target>/ 子目录
    out_dir.mkdir(parents=True, exist_ok=True)
    work = tmp / name
    work.mkdir(parents=True, exist_ok=True)
    platform_patch = out_dir / PLATFORM_PATCH[name]
    lang_patch = out_dir / (LANG_CN_PATCH if name == "android-cn" else LANG_PATCH)

    # --- final state for every file in src/, via three-way merge ---
    final = {}
    for src_file in sorted(src_root.rglob("*")):
        if not src_file.is_file() or src_file.name == ".gitkeep":
            continue
        rel = src_file.relative_to(src_root).as_posix()
        base, ours = pc_base / rel, upstream / rel
        if not base.exists():
            raise SystemExit(f"PC 基线缺文件: {rel}")
        if not ours.exists():
            raise SystemExit(f"{name} 上游缺文件: {rel}（新增文件应走 patches/cn_extra/）")
        text = read(src_file) if ours.read_bytes() == base.read_bytes() else three_way(ours, base, src_file, rel)
        final[rel] = text
        write(work / rel, text)

    # --- Android-only build macro ---
    final[ANDROID_MK] = inject_android_mk(upstream / ANDROID_MK)
    write(work / ANDROID_MK, final[ANDROID_MK])

    # --- the Chinese fork's own language file ---
    if name == "android-cn":
        ini = upstream / CN_INI
        if not ini.exists():
            raise SystemExit(f"{name}: 期望存在 {CN_INI}")
        final[CN_INI] = inject_cn_ini(ini)
        write(work / CN_INI, final[CN_INI])

    # --- 0004: android-cn language strings (English.ini + Chinese.ini) ---
    if name == "android-cn":
        parts, covered = [], []
        for rel in LANG_FILES:
            d = diff_text(upstream / rel, work / rel, rel)
            if d:
                parts.append(d)
                covered.append(rel)
        write(out_dir / LANG_CN_PATCH, "".join(parts))
        print(f"  {LANG_CN_PATCH}: {len(covered)} 个文件 — {', '.join(covered)}")
        for rel in covered:
            if not rel.startswith("lang/"):
                raise SystemExit(f"{LANG_CN_PATCH} 混入非语言文件: {rel}")

    # --- platform patch = excluded drift files + Android-only build file ---
    platform_files = sorted(set(EXCLUDED) | {ANDROID_MK})
    for rel in platform_files:
        if rel not in final:
            raise SystemExit(f"{name}: 平台文件 {rel} 没有终态（src/ 里缺？）")

    parts, covered = [], []
    for rel in platform_files:
        d = diff_text(upstream / rel, work / rel, rel)
        if d:
            parts.append(d)
            covered.append(rel)
    write(platform_patch, "".join(parts))
    print(f"  {PLATFORM_PATCH[name]}: {len(covered)} 个文件 — {', '.join(covered)}")

    main = out_dir / MAIN_PATCH
    frontend = out_dir / FRONTEND_PATCH
    main_files = {
        line[6:] for line in read(main).splitlines() if line.startswith("--- a/")
    }
    if set(main_files) & set(LANG_FILES):
        raise SystemExit(f"0001 仍含 lang/*.ini，先重跑 scripts/gen_all_patches.sh")
    if not set(EXCLUDED) <= set(covered):
        raise SystemExit(f"{name}: 被排除的文件未全部由平台补丁覆盖: "
                         f"{sorted(set(EXCLUDED) - set(covered))}")
    overlap = (set(covered) & main_files) - set(EXCLUDED)
    if overlap:
        raise SystemExit(f"{name}: 平台补丁与 0001 重复: {sorted(overlap)}")
    if set(covered) & set(LANG_FILES):
        raise SystemExit(f"{name}: 平台补丁不应含 lang/*.ini（语言改动走 0003/0004）")
    if FRONTEND in covered:
        raise SystemExit(f"{name}: 平台补丁不应包含可选前端 {FRONTEND}")

    files = sorted(set(final) | {FRONTEND})

    # --- verify A: full path, shared 0001 (excluded) + lang + platform + 0002 ---
    check = tmp / f"{name}-check"
    pristine_copy(upstream, check, files)
    apply(check, main, EXCLUDED)
    apply(check, lang_patch)
    apply(check, platform_patch)
    apply(check, frontend)
    for rel, want in final.items():
        if read(check / rel) != want:
            raise SystemExit(f"{name}: 应用后 {rel} 与三方合并终态不一致")
    print(f"  ✅ 0001(--exclude {len(EXCLUDED)}) + {lang_patch.name} + "
          f"{platform_patch.name} + 0002 == 终态（{len(final)} 文件逐字节）")

    # --- verify B: 0002 really is optional ---
    nofrontend = tmp / f"{name}-nofrontend"
    pristine_copy(upstream, nofrontend, files)
    apply(nofrontend, main, EXCLUDED)
    apply(nofrontend, lang_patch)
    apply(nofrontend, platform_patch)
    if read(nofrontend / FRONTEND) != read(upstream / FRONTEND):
        raise SystemExit(f"{name}: 不应用 0002 时前端文件被意外修改")
    for rel, want in final.items():
        if rel != FRONTEND and read(nofrontend / rel) != want:
            raise SystemExit(f"{name}: 不应用 0002 时 {rel} 未达到终态")
    print(f"  ✅ 不应用 0002 时 {FRONTEND} 保持上游原版")

    # --- verify C: is any exclusion now stale? (informational only) ---
    pristine = tmp / f"{name}-pristine"
    pristine_copy(upstream, pristine, files)
    for rel in EXCLUDED:
        d = diff_text(pc_base / rel, src_root / rel, rel)
        if not d:
            continue
        one = tmp / f"{name}-one.patch"
        write(one, d)
        r = run(["git", "apply", "--check", "-p1", str(one)], cwd=pristine)
        if r.returncode == 0:
            print(f"  ⚠ {rel} 在本上游已能直接应用，可考虑从 EXCLUDED 移除")
        one.unlink()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--pc-base", required=True)
    ap.add_argument("--android", required=True)
    ap.add_argument("--android-cn", required=True)
    a = ap.parse_args()

    repo = Path(a.repo).resolve()
    src_root = repo / "src"
    pc_base = Path(a.pc_base).resolve()
    if not src_root.is_dir() or not pc_base.is_dir():
        raise SystemExit("src/ 或 --pc-base 不存在")
    for patch in (MAIN_PATCH, FRONTEND_PATCH, LANG_PATCH):
        if not (repo / "patches" / patch).exists():
            raise SystemExit(f"缺少共用补丁 patches/{patch}，先跑 scripts/gen_all_patches.sh")

    # The exclusion list exists in two places; never let them drift apart.
    apply_script = repo / "scripts" / "apply_patches.sh"
    if not apply_script.exists():
        raise SystemExit("缺少 scripts/apply_patches.sh")
    applier = read(apply_script)
    listed = set(re.findall(r"--exclude=([^\s\\]+)", applier))
    if listed != set(EXCLUDED):
        raise SystemExit(
            f"排除列表与 scripts/apply_patches.sh 不一致:\n"
            f"  生成脚本: {sorted(EXCLUDED)}\n"
            f"  应用脚本: {sorted(listed)}"
        )

    with tempfile.TemporaryDirectory(prefix="ique-platform-") as td:
        tmp = Path(td)
        build_one("android", Path(a.android).resolve(), pc_base, src_root, repo, tmp)
        build_one("android-cn", Path(a.android_cn).resolve(), pc_base, src_root, repo, tmp)

    print("\n完成。共用 0001/0002/0003 与 cn_extra 未改动；"
          "0004 是汉化版 Android 的语言补丁，0005/0006 是平台增量。")


if __name__ == "__main__":
    main()
