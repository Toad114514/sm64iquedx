#!/usr/bin/env python3
"""补丁自洽性检查，不需要任何上游 checkout。

为什么需要它：`patches/0008-android.patch` 由 `gen_platform_patches.py` 从一份
Android 基线生成，本机不一定有那份基线，所以改完 PC 侧往往只能手改平台补丁。
手改最容易犯两个错，两个都不会被 `sh -n` 或普通 grep 抓到：

1. **改了 hunk 内容，忘了同步 `@@ -a,b +c,d @@` 的计数。** `git apply` 会报
   "corrupt patch at line N"，而 N 是 hunk 的起始行，离真正出错的地方很远。
2. **删掉 `cn_extra/` 里的一个头文件，某个补丁里还留着 `#include`。** PC 目标
   照样编过（那个补丁的 include 只在 Android 分支里），等到 Android CI 才炸。

用法（在仓库根目录）：

    python3 scripts/check_patch_integrity.py

退出码 0 = 全过。可放进 CI，不需要网络，也不需要 `upstream.txt` 里的任何仓库。
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")
INCLUDE = re.compile(r'^\+(?:\s*)#\s*include\s+"([^"]+)"')

failures: list[str] = []
notes: list[str] = []


def hunk_bodies(text: str):
    """Yield (header_line_no, declared_old, declared_new, old_len, new_len)."""
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        m = HUNK.match(lines[i])
        if not m:
            i += 1
            continue
        declared_old = int(m.group(2)) if m.group(2) is not None else 1
        declared_new = int(m.group(4)) if m.group(4) is not None else 1
        j = i + 1
        old = new = 0
        while j < len(lines):
            line = lines[j]
            # A new file / hunk boundary ends this hunk.
            if (line.startswith("@@") or line.startswith("--- ")
                    or line.startswith("+++ ") or line.startswith("diff ")):
                break
            # `\ No newline at end of file` is metadata: it can sit *between* the
            # last `-` line and the first `+` line of a hunk, so skip it instead
            # of ending the hunk there.
            if line.startswith("\\"):
                j += 1
                continue
            if line.startswith("+"):
                new += 1
            elif line.startswith("-"):
                old += 1
            elif line.startswith(" "):
                old += 1
                new += 1
            else:
                # An empty string here is the blank line diff -u puts between
                # hunks; anything else means the patch is malformed.
                break
            j += 1
        yield i + 1, declared_old, declared_new, old, new
        i = j


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    patches = sorted((repo / "patches").glob("000*.patch"))
    if not patches:
        failures.append("patches/000*.patch 一个都没有")

    for patch in patches:
        text = patch.read_text(encoding="utf-8")
        for line_no, d_old, d_new, old, new in hunk_bodies(text):
            if (old, new) != (d_old, d_new):
                failures.append(
                    f"{patch.name}: hunk@{line_no} 声明 -{d_old}/+{d_new}，"
                    f"实际 -{old}/+{new}；git apply 会报 corrupt patch"
                )
    notes.append(f"{len(patches)} 个补丁的 hunk 计数自洽")

    # 补丁里 import 的 cn_*.h 必须真的存在于 cn_extra/（本包新增的头文件只会
    # 在那里；上游不可能有）。按 basename 匹配，因为补丁里的 include 可能是
    # 相对的（"cn_font.h"）也可能是带目录的（"game/cn_rom_assets.h"）。
    extra = repo / "patches" / "cn_extra"
    provided = {p.name for p in extra.rglob("*") if p.is_file()}
    missing: dict[str, set[str]] = {}
    for patch in patches:
        for line in patch.read_text(encoding="utf-8").split("\n"):
            m = INCLUDE.match(line)
            if not m:
                continue
            inc = m.group(1)
            base = inc.rsplit("/", 1)[-1]
            if not base.startswith("cn_"):
                continue
            if base not in provided:
                missing.setdefault(base, set()).add(patch.name)
    for base, where in sorted(missing.items()):
        failures.append(
            f"{'/'.join(sorted(where))} 里 include 了 {base}，但 "
            f"patches/cn_extra/ 下没有这个文件（PC 目标编得过，Android 目标会在 CI 炸）"
        )
    notes.append(f"补丁引用的 cn_* 头文件都在 cn_extra/（{len(provided)} 个文件）")

    # cn_extra 里的每个文件都必须是「上游不存在的新增文件」，所以它自己不能
    # 出现在任何补丁的 `--- a/` 里：那会让同一份内容被应用两次。
    patched = set()
    for patch in patches:
        for line in patch.read_text(encoding="utf-8").split("\n"):
            if line.startswith("--- a/"):
                patched.add(line[6:])
    overlap = sorted(r.relative_to(extra).as_posix() for r in extra.rglob("*")
                     if r.is_file() and r.relative_to(extra).as_posix() in patched)
    for rel in overlap:
        failures.append(f"{rel} 同时在 cn_extra/ 和某个补丁里，会被应用两次")
    notes.append("cn_extra/ 与分组补丁无重叠")

    for n in notes:
        print(f"  ok   {n}")
    if failures:
        print()
        for f in failures:
            print(f"  FAIL {f}")
        print(f"\n失败 {len(failures)} 项")
        return 1
    print(f"\n通过 {len(notes)} 项")
    return 0


if __name__ == "__main__":
    sys.exit(main())
