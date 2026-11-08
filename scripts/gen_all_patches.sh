#!/bin/sh
# 一次生成两份「四套上游共用」的补丁。改了 src/ 后跑这个，不要单独手改 patches/*.patch。
#
#   scripts/gen_all_patches.sh <上游基线根目录>
#
# 上游根目录 = upstream.txt 声明的 commit 的干净 checkout（也可用
# `git worktree add /path/to/base <commit>` 得到一个）。
#
# 语言字符串（CHINESE_TEXT_PACK / 语言列表里的 Chinese 条目）不生成补丁：
# 各上游的 lang/*.ini 行号与自带条目都不同，做成补丁就是每个上游一份。改由
# scripts/add_lang_strings.sh 在应用阶段按键插入，这里只负责把 lang/ 从
# 0001 里排除掉。
set -eu

if [ $# -lt 1 ]; then
    echo "用法: scripts/gen_all_patches.sh <上游基线根目录>" >&2
    exit 1
fi

root=$(cd "$(dirname "$0")/.." && pwd)
upstream=$(cd "$1" && pwd)
gen="$root/scripts/gen_patches.sh"
P="$root/patches"

# 可选补丁：DJUI 设置面板里的「中文字库（神游）」开关。
# 不应用它时只能改 config 文件切换，且其它改动仍能独立编译。
# djui_panel_confirm.[ch] 留在 0001：它加的 create_ex() 目前没有调用点
# （首启确认框已改成按神游 ROM 自动判定），保留只是为了以后恢复确认框时省事。
FRONTEND=src/pc/djui/djui_panel_misc.c

# lang/ 整个前缀不进任何补丁，由 scripts/add_lang_strings.sh 在应用时插入。
LANG_PREFIX=lang/

MAIN="$P/0001-src-overlay.patch"
FRONT_PATCH="$P/0002-djui-frontend.patch"

sh "$gen" "$upstream" "$MAIN" --exclude "$FRONTEND" --exclude "$LANG_PREFIX"
sh "$gen" "$upstream" "$FRONT_PATCH" --only "$FRONTEND"

# ---- 分工断言：两段互不重叠，且各自只含该含的文件 ----
if grep -q "^--- a/$FRONTEND$" "$MAIN"; then
    echo "错误: 0001 里仍含前端文件 $FRONTEND" >&2
    exit 1
fi
if grep -q '^--- a/lang/' "$MAIN"; then
    echo "错误: 0001 里仍含 lang/*.ini，语言字符串应交给 add_lang_strings.sh" >&2
    exit 1
fi
if grep -q '^--- a/lang/' "$FRONT_PATCH"; then
    echo "错误: 0002 里混入了 lang/*.ini" >&2
    exit 1
fi
n=$(grep -c '^--- a/' "$FRONT_PATCH" || true)
if [ "$n" -ne 1 ] || ! grep -q "^--- a/$FRONTEND$" "$FRONT_PATCH"; then
    echo "错误: 0002 应只含 ${FRONTEND}，实际含 $n 个文件" >&2
    exit 1
fi
echo "✅ 0001 核心 / 0002 前端分工正确；语言字符串由脚本插入" >&2
