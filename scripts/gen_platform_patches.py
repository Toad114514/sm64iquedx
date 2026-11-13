#!/usr/bin/env python3
"""Generate the single shared Android platform patch, patches/0008-android.patch.

Four upstreams share one main patch set, flat under patches/ -- the four-digit
number in the file name is the apply order::

    0001-build-and-config.patch   build switch (Makefile, configfile)
    0002-rom-assets.patch         iQue rom detection / asset loading
    0003-text-core.patch          encoding + glyph core, HUD and title text
    0004-dialog.patch             dialog tables and rendering
    0005-menus.patch              file select, star select, title intro
    0006-asset-overlay.patch      iQue texture/geometry overlays
    0007-djui-frontend.patch      optional (DJUI toggle), only with --frontend
    0008-android.patch            platform delta, shared by android + android-cn
    cn_extra/                     new files, copied verbatim

``lang/*.ini`` is not patched at all.  Every upstream has its own ini with
different line numbers, and the Chinese fork already ships a
``Chinese = ...`` entry, so a diff per upstream would only multiply.  The two
needed strings are inserted at apply time by ``scripts/add_lang_strings.sh``
(idempotent, key-based), which works on all four upstreams unchanged.

Only two kinds of differences remain for the platform patch:

1. Two files drifted away from the PC baseline, so 01-build / 02-rom have to be
   applied with ``--exclude=...`` on Android.  The platform patch supplies their
   correct final state instead (computed by three-way merge):

       Makefile               PC build script (ndk-build never reads it)
       src/pc/pc_main.c       Android uses SDL_main / TARGET_ANDROID branches

2. ndk-build reads ``Android.mk``, not the PC Makefile.  The
   ``SM64COOPDX_CN_TEXT`` macro therefore has to be injected into
   ``LOCAL_CFLAGS`` or every ``#if defined(SM64COOPDX_CN_TEXT)`` block is
   compiled out and the Chinese pack silently does nothing on Android.

``src/pc/configfile.c`` is excluded on Android too, but it does NOT get a patch:
the two Android forks insert their TOUCH_CONTROLS block at exactly the spot the
PC patch also inserts code, so the only difference between the two per-fork
patches used to be the surrounding context (``configSnapTouch`` vs
``configElementSnap``).  Its six lines are inserted by
``scripts/add_cn_config.sh`` instead -- one script, all upstreams.

Three-way merge for the final state::

    base   = PC baseline (upstream.txt / 8cd6e59)
    ours   = the Android upstream file
    theirs = this repo's src/ (the PC final state)

Usage::

    scripts/gen_platform_patches.py \
        --pc-base    /path/to/pc-baseline-tree \
        --android    /path/to/ManIsCat2-sm64coopdx \
        --android-cn /path/to/sm64coopdx-Chinese-Android

Run ``scripts/gen_all_patches.sh`` first so 0001~0007 are current.
"""

import argparse
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

FRONTEND = "src/pc/djui/djui_panel_misc.c"
# 0007-djui-frontend 里的全部文件；verify B 用它们证明「不应用也编译得过、文件没被动过」。
FRONTEND_FILES = (
    FRONTEND,
    "src/pc/djui/djui_panel_confirm.c",
    "src/pc/djui/djui_panel_confirm.h",
)
ANDROID_MK = "Android.mk"
CONFIGFILE = "src/pc/configfile.c"
EN_INI = "lang/English.ini"
CN_INI = "lang/Chinese.ini"

# 共用补丁：文件名里的四位编号即应用顺序（0001 … 0006）。0008-android 由本脚本
# 生成，0007-djui-frontend 是可选的。
CORE_PATCH_GLOBS = (
    "0001-*.patch",
    "0002-*.patch",
    "0003-*.patch",
    "0004-*.patch",
    "0005-*.patch",
    "0006-*.patch",
)
FRONTEND_PATCH = "0007-djui-frontend.patch"
# 一份平台补丁，两个 Android 上游共用。两边的 Makefile / pc_main.c / Android.mk
# 内容完全相同，所以 hunk 也完全相同；生成时会逐字节比对，一旦哪天漂开就直接
# 报错，而不是静默产出一份只对其中一个上游有效的补丁。
ANDROID_PATCH = "0008-android.patch"

# Files 0001~0006 must skip on Android.  lang/*.ini is NOT here: no group
# ever contains it, so the applier does not need to exclude it.  Keep this list
# in sync with scripts/apply_patches.sh; the script asserts both ends.
#
# CONFIGFILE is excluded but is not covered by the patch -- its lines come from
# scripts/add_cn_config.sh.  The other two are the patch's job.
EXCLUDED = (
    "Makefile",
    CONFIGFILE,
    "src/pc/pc_main.c",
)
PLATFORM_FILES = tuple(sorted((set(EXCLUDED) - {CONFIGFILE}) | {ANDROID_MK}))
LANG_FILES = (EN_INI, CN_INI)

# 语言字符串与配置项都必须由脚本插进去，不能偷偷回到补丁里。
LANG_SCRIPT = "scripts/add_lang_strings.sh"
CONFIG_SCRIPT = "scripts/add_cn_config.sh"
LANG_EXPECT = {
    "android": ((EN_INI, ("CHINESE_TEXT_PACK", "Chinese =")),),
    "android-cn": (
        (EN_INI, ("CHINESE_TEXT_PACK", "Chinese =")),
        (CN_INI, ("CHINESE_TEXT_PACK",)),
    ),
}

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
        if rel == CONFIGFILE:
            ok = "#ifdef TOUCH_CONTROLS" in ours and '.name = "chinese_text"' in theirs
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


def run_script(repo: Path, script: str, tree: Path, *extra) -> str:
    r = run(["sh", str(repo / script), str(tree), *extra])
    out = r.stdout.decode(errors="replace")
    if r.returncode != 0:
        raise SystemExit(f"{script} 失败: {out}{r.stderr.decode(errors='replace')}")
    return out


def check_lang_strings(name: str, tree: Path) -> None:
    for rel, keys in LANG_EXPECT[name]:
        f = tree / rel
        if not f.exists():
            raise SystemExit(f"{name}: 语言字符串校验缺文件 {rel}")
        text = read(f)
        for key in keys:
            if key not in text:
                raise SystemExit(f"{name}: {rel} 缺少 {key}")


def compute(name, upstream, pc_base, src_root):
    """Terminal state for every touched file + the platform diffs for this upstream."""
    work = _tmp / name
    work.mkdir(parents=True, exist_ok=True)

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

    parts = {}
    for rel in PLATFORM_FILES:
        if rel not in final:
            raise SystemExit(f"{name}: 平台文件 {rel} 没有终态（src/ 里缺？）")
        d = diff_text(upstream / rel, work / rel, rel)
        if d:
            parts[rel] = d
    missing = sorted(set(PLATFORM_FILES) - set(parts))
    if missing:
        raise SystemExit(f"{name}: 平台补丁应覆盖 {missing}，但它们与上游无差异")
    return final, parts


def core_patches(repo: Path):
    """0001 … 0006 的补丁，编号即应用顺序。"""
    out = []
    for pat in CORE_PATCH_GLOBS:
        got = sorted((repo / "patches").glob(pat))
        if not got:
            raise SystemExit(f"缺少 patches/{pat}，先跑 scripts/gen_all_patches.sh")
        out.extend(got)
    return out


def apply_core(tree: Path, repo: Path, excludes=()) -> None:
    for p in core_patches(repo):
        if p.stat().st_size == 0:
            continue
        apply(tree, p, excludes)


def verify(name, upstream, final, patch, repo, lang_present):
    # 语言文件与 configfile.c 也要复制进校验树，否则脚本无处可插。
    files = sorted(set(final) | set(FRONTEND_FILES) | set(lang_present))

    # --- verify A: 完整链路 0001~0006(excluded) + 平台补丁 + 0007 + 两个插入脚本 ---
    check = _tmp / f"{name}-check"
    pristine_copy(upstream, check, files)
    apply_core(check, repo, EXCLUDED)
    apply(check, patch)
    apply(check, repo / "patches" / FRONTEND_PATCH)
    run_script(repo, CONFIG_SCRIPT, check)
    run_script(repo, LANG_SCRIPT, check)
    for rel, want in final.items():
        if read(check / rel) != want:
            raise SystemExit(f"{name}: 应用后 {rel} 与三方合并终态不一致")
    check_lang_strings(name, check)
    print(f"  ✅ 0001~0006(--exclude {len(EXCLUDED)}) + {patch.name} + 0007-djui-frontend + "
          f"add_cn_config.sh + add_lang_strings.sh == 终态（{len(final)} 文件逐字节）")

    # --- verify A2: 两个插入脚本幂等 ---
    if "已齐全" not in run_script(repo, LANG_SCRIPT, check, "--check"):
        raise SystemExit(f"{name}: add_lang_strings.sh 复跑不幂等")
    if "已齐全" not in run_script(repo, CONFIG_SCRIPT, check, "--check"):
        raise SystemExit(f"{name}: add_cn_config.sh 复跑不幂等")
    print("  ✅ 两个插入脚本复跑无改动（幂等）")

    # --- verify B: 0007-djui-frontend 真的是可选的 ---
    nofrontend = _tmp / f"{name}-nofrontend"
    pristine_copy(upstream, nofrontend, files)
    apply_core(nofrontend, repo, EXCLUDED)
    apply(nofrontend, patch)
    run_script(repo, CONFIG_SCRIPT, nofrontend)
    run_script(repo, LANG_SCRIPT, nofrontend)
    for rel in FRONTEND_FILES:
        if read(nofrontend / rel) != read(upstream / rel):
            raise SystemExit(f"{name}: 不应用 0007-djui-frontend 时 {rel} 被意外修改")
    for rel, want in final.items():
        if rel not in FRONTEND_FILES and read(nofrontend / rel) != want:
            raise SystemExit(f"{name}: 不应用 0007-djui-frontend 时 {rel} 未达到终态")
    print(f"  ✅ 不应用 0007-djui-frontend 时 {len(FRONTEND_FILES)} 个前端文件保持上游原版")


    # --- verify C: 排除项是否已经不再需要（仅提示，不自动收缩） ---
    pristine = _tmp / f"{name}-pristine"
    pristine_copy(upstream, pristine, files)
    for rel in EXCLUDED:
        d = diff_text(_pc_base / rel, repo / "src" / rel, rel)
        if not d:
            continue
        one = _tmp / f"{name}-one.patch"
        write(one, d)
        r = run(["git", "apply", "--check", "-p1", str(one)], cwd=pristine)
        if r.returncode == 0:
            print(f"  ⚠ {rel} 在本上游已能直接应用，可考虑从 EXCLUDED 移除")
        one.unlink()


def main():
    global _tmp, _pc_base
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
    for patch in core_patches(repo) + [repo / "patches" / FRONTEND_PATCH]:
        if not patch.exists():
            raise SystemExit(f"缺少共用补丁 {patch.relative_to(repo)}，"
                             "先跑 scripts/gen_all_patches.sh")
        if patch.stat().st_size == 0:
            raise SystemExit(f"共用补丁 {patch.relative_to(repo)} 是空的")
    for script in (LANG_SCRIPT, CONFIG_SCRIPT):
        if not (repo / script).exists():
            raise SystemExit(f"缺少 {script}")

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
    if "--frontend" not in applier or "0007-djui-frontend.patch" not in applier:
        raise SystemExit("scripts/apply_patches.sh 没有接线 0007-djui-frontend.patch")
    if CONFIG_SCRIPT.split("/")[-1] not in applier:
        raise SystemExit(f"scripts/apply_patches.sh 没有接线 {CONFIG_SCRIPT}")

    # 共用分组补丁里不应出现 lang/*.ini，也不应出现任何平台补丁覆盖的文件。
    core_files = set()
    for p in core_patches(repo):
        core_files |= {line[6:] for line in read(p).splitlines() if line.startswith("--- a/")}
    if core_files & set(LANG_FILES):
        raise SystemExit("0001~0006 里仍含 lang/*.ini，先重跑 scripts/gen_all_patches.sh")
    # 平台补丁覆盖的文件本来就在共用组里（正因为打不上才需要平台补丁），
    # 所以这里只检查「没被 --exclude 掉却出现在平台补丁里」。
    overlap = (core_files & set(PLATFORM_FILES)) - set(EXCLUDED)
    if overlap:
        raise SystemExit(f"0001~0006 与平台补丁重复: {sorted(overlap)}")
    if set(FRONTEND_FILES) & set(PLATFORM_FILES) or set(PLATFORM_FILES) & set(LANG_FILES):
        raise SystemExit("平台补丁不应包含前端或 lang/*.ini")

    targets = (
        ("android", Path(a.android).resolve()),
        ("android-cn", Path(a.android_cn).resolve()),
    )
    langs = {name: tuple(rel for rel in LANG_FILES if (up / rel).exists()) for name, up in targets}

    with tempfile.TemporaryDirectory(prefix="ique-platform-") as td:
        _tmp = Path(td)
        _pc_base = pc_base

        results, diffs = {}, None
        for name, upstream in targets:
            print(f"\n===== {name} ({upstream}) =====")
            final, parts = compute(name, upstream, pc_base, src_root)
            results[name] = (upstream, final)
            if diffs is None:
                diffs = parts
            elif parts != diffs:
                # 补丁每个上游一份的唯一理由就是这里：某份文件的终态 diff 不一样。
                # 真出现了就说明两个上游确实分道扬镳，得退回「每目标一份平台补丁」。
                bad = sorted(set(parts) ^ set(diffs)) or \
                      [r for r in parts if parts[r] != diffs.get(r)]
                raise SystemExit(
                    f"两个 Android 上游的平台增量不同（{bad}），不能共用 {ANDROID_PATCH}：\n"
                    f"  请把不同的文件从 PLATFORM_FILES 里拆出来，改成每目标一份补丁。"
                )

        patch_text = "".join(diffs[rel] for rel in PLATFORM_FILES)
        patch = repo / "patches" / ANDROID_PATCH
        write(patch, patch_text)
        print(f"\n{ANDROID_PATCH}: {len(diffs)} 个文件 — {', '.join(sorted(diffs))}"
              f"（两个上游共用一份）")

        stale = (repo / "patches" / "0006-android-cn.patch",
                 repo / "patches" / "0005-android.patch",
                 repo / "patches" / "0001-src-overlay.patch",
                 repo / "patches" / "0002-djui-frontend.patch")
        for old in stale:
            if old.exists():
                raise SystemExit(f"旧的 {old.name} 还在，删掉它（补丁已平铺成 编号-分组-职责.patch）")

        for name, upstream in targets:
            print(f"\n===== 校验 {name} =====")
            verify(name, upstream, results[name][1], patch, repo, langs[name])

    print("\n完成。共用 0001~0007 与 cn_extra 未改动；"
          f"{ANDROID_PATCH} 由两个 Android 目标共用，语言字符串与配置项由 "
          "scripts/add_lang_strings.sh / scripts/add_cn_config.sh 插入。")


if __name__ == "__main__":
    main()
