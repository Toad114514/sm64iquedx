#!/bin/sh
# 一次生成全部补丁。改了 src/ 后跑这个，不要单独手改 patches/*.patch。
#
#   scripts/gen_all_patches.sh <上游基线根目录>
#
# 上游根目录 = upstream.txt 声明的 commit 的干净 checkout（也可用
# `git worktree add /path/to/base <commit>` 得到一个）。
set -eu

if [ $# -lt 1 ]; then
    echo "用法: scripts/gen_all_patches.sh <上游基线根目录>" >&2
    exit 1
fi

root=$(cd "$(dirname "$0")/.." && pwd)
upstream=$(cd "$1" && pwd)
gen="$root/scripts/gen_patches.sh"

# 可选补丁：DJUI 设置面板里的「中文字库（神游）」开关。
# 不应用它时只能改 config 文件切换，且其它改动仍能独立编译。
# 注意 djui_panel_confirm.[ch] 不能一起移走：pc_main.c 的首启确认框
# 硬依赖 djui_panel_confirm_create_ex()，拆走会导致不应用本补丁时链接失败。
FRONTEND=src/pc/djui/djui_panel_misc.c

sh "$gen" "$upstream" "$root/patches/0001-src-overlay.patch" --exclude "$FRONTEND"
sh "$gen" "$upstream" "$root/patches/0002-djui-frontend.patch" --only "$FRONTEND"

# 断言：两个补丁互不重叠，且 0002 只动前端那一个文件
if grep -q "^--- a/$FRONTEND$" "$root/patches/0001-src-overlay.patch"; then
    echo "错误: 0001 里仍含前端文件 $FRONTEND" >&2
    exit 1
fi
n=$(grep -c '^--- a/' "$root/patches/0002-djui-frontend.patch" || true)
if [ "$n" -ne 1 ] || ! grep -q "^--- a/$FRONTEND$" "$root/patches/0002-djui-frontend.patch"; then
    echo "错误: 0002 应只含 $FRONTEND，实际含 $n 个文件" >&2
    exit 1
fi
echo "✅ 0001 主补丁 / 0002 前端补丁分工正确" >&2
