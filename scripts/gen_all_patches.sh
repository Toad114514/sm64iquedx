#!/bin/sh
# 一次生成「四套上游共用」的全部分组补丁。改了 src/ 后跑这个，不要单独手改
# patches/*.patch。
#
#   scripts/gen_all_patches.sh <上游基线根目录>
#
# 上游根目录 = upstream.txt 声明的 commit 的干净 checkout（也可用
# `git worktree add /path/to/base` 得到一个）。
#
# 补丁全部平铺在 patches/ 下，文件名 = 四位编号 + 分组 + 职责，**编号就是应用
# 顺序**（`sort` 即顺序，不另存 manifest）：
#
#   0001-build-and-config.patch  构建开关 configChineseText* / Makefile 的 CN_TEXT_PACK
#   0002-rom-assets.patch        神游 ROM 探测、资源分段读取、启动时装配
#   0003-text-core.patch         编码/字形核心、HUD 与标题文本、课程/动作名查表
#   0004-dialog.patch            对话框表与渲染、Lua 侧对话框替换
#   0005-menus.patch             文件选择、选星盘、标题过场
#   0006-asset-overlay.patch     iQue 贴图/几何覆盖（能量表轮盘偏移）
#   0007-djui-frontend.patch     可选：DJUI 设置面板里的中文字库开关（不应用也能编译）
#   0008-android.patch           Android 平台增量（由 gen_platform_patches.py 生成）
#
# 分组只体现在文件名里：可选组用前缀 glob 就够（--frontend 应用 `0007-*.patch`，
# android 目标应用 `0008-*.patch`），不需要为“哪些补丁属于可选组”再维护一份清单，
# 也不需要多套一层目录。新增一个分组 = 加一个更大编号的文件。
#
# 语言字符串（CHINESE_TEXT_PACK / 语言列表里的 Chinese 条目）不生成补丁：
# 各上游的 lang/*.ini 行号与自带条目都不同，做成补丁就是每个上游一份。改由
# scripts/add_lang_strings.sh 在应用阶段按键插入，这里只负责把 lang/ 从
# 全部分组里排除掉。
set -eu

if [ $# -lt 1 ]; then
    echo "用法: scripts/gen_all_patches.sh <上游基线根目录>" >&2
    exit 1
fi

root=$(cd "$(dirname "$0")/.." && pwd)
upstream=$(cd "$1" && pwd)
gen="$root/scripts/gen_patches.sh"
P="$root/patches"

# lang/ 整个前缀不进任何补丁，由 scripts/add_lang_strings.sh 在应用时插入。
LANG_PREFIX=lang/

# 旧版单文件补丁，拆分后必须消失，否则应用器之外的人会拿它打到一半。
LEGACY="$P/0001-src-overlay.patch $P/0002-djui-frontend.patch $P/0005-android.patch"

# 分组 = 输出文件名 + 文件清单。清单里的路径必须是 src/ 下的完整相对路径：
# gen_patches.sh 用 --only 前缀筛选，写全路径即一次只命中一个文件。
gen_group() {
    out=$1
    shift
    args=""
    for f in "$@"; do
        args="$args --only $f"
    done
    # 有意不加引号：$args 要靠词分割拆成多个参数（仓库内路径不含空格）。
    # shellcheck disable=SC2086
    sh "$gen" "$upstream" "$P/$out" --exclude "$LANG_PREFIX" $args
}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

echo "— 0001-build-and-config —" >&2
gen_group 0001-build-and-config.patch \
    Makefile \
    src/pc/configfile.c \
    src/pc/configfile.h

echo "— 0002-rom-assets —" >&2
# mods/mods.c 目前与上游一致（「改版 ⇒ 汉化整包停用」已拆除），留在这里只是
# 让「模组加载后再判定一次」那类改动重新出现时自动落进本组。
gen_group 0002-rom-assets.patch \
    src/pc/rom_checker.h \
    src/pc/rom_assets.c \
    src/pc/pc_main.c \
    src/pc/mods/mods.c

echo "— 0003-text-core —" >&2
gen_group 0003-text-core.patch \
    src/game/print.c \
    src/game/segment2.h \
    src/game/level_info.c \
    src/game/level_info.h \
    src/game/level_update.c \
    src/game/area.c \
    text/define_text.inc.c \
    text/define_courses.inc.c

echo "— 0004-dialog —" >&2
gen_group 0004-dialog.patch \
    src/game/ingame_menu.c \
    src/game/ingame_menu.h \
    src/pc/dialog_table.c \
    src/pc/dialog_table.h \
    src/pc/lua/utils/smlua_text_utils.c

echo "— 0005-menus —" >&2
gen_group 0005-menus.patch \
    src/menu/file_select.c \
    src/menu/ingame_text.h \
    src/menu/star_select.c \
    src/menu/intro_geo.c \
    src/menu/intro_geo.h

echo "— 0006-asset-overlay —" >&2
gen_group 0006-asset-overlay.patch \
    src/pc/djui/djui_hud_utils.c \
    src/pc/lua/utils/smlua_misc_utils.c

# 可选前端：djui_panel_confirm.[ch] 加的 create_ex() 目前没有调用点（首启确认框
# 已改成按神游 ROM 自动判定），留在这一组只是为了以后恢复确认框时省事；它跟
# djui_panel_misc.c 一样属于「用户可见的 DJUI 改动」，一起做成可选补丁。
echo "— 0007-djui-frontend —" >&2
gen_group 0007-djui-frontend.patch \
    src/pc/djui/djui_panel_misc.c \
    src/pc/djui/djui_panel_confirm.c \
    src/pc/djui/djui_panel_confirm.h

# ---- 完整性断言：分组必须「无遗漏、无重复」地覆盖 src/ 的全部差异 ----
# 参照组 = 不加任何 --only 的全量生成（同样排掉 lang/）。
sh "$gen" "$upstream" "$work/all.patch" --exclude "$LANG_PREFIX"
full=$(grep '^--- a/' "$work/all.patch" | sed 's|^--- a/||' | LC_ALL=C sort || true)

grouped=$(grep -h '^--- a/' "$P"/000[1-7]-*.patch 2>/dev/null | sed 's|^--- a/||' | LC_ALL=C sort || true)

if [ "$full" != "$grouped" ]; then
    echo "错误: 0001~0007 分组没有恰好覆盖 src/ 的全部差异" >&2
    printf '%s\n' "$full" > "$work/full.txt"
    printf '%s\n' "$grouped" > "$work/grouped.txt"
    echo "--- 只在全量里（漏分组）:" >&2
    comm -23 "$work/full.txt" "$work/grouped.txt" >&2 || true
    echo "--- 只在分组里（多发）:" >&2
    comm -13 "$work/full.txt" "$work/grouped.txt" >&2 || true
    exit 1
fi

dups=$(printf '%s\n' "$grouped" | LC_ALL=C sort | uniq -d || true)
if [ -n "$dups" ]; then
    echo "错误: 同一个文件出现在多个分组里:" >&2
    printf '%s\n' "$dups" >&2
    exit 1
fi

# 0001~0007 里不允许出现 lang/*.ini（语言字符串由脚本插入）。
if grep -l '^--- a/lang/' "$P"/000[1-7]-*.patch 2>/dev/null | grep -q .; then
    echo "错误: 分组补丁里含 lang/*.ini，语言字符串应交给 add_lang_strings.sh" >&2
    exit 1
fi

# 旧单文件补丁必须已经删掉：它们和分组补丁内容重叠，留着会被人重复应用。
for f in $LEGACY; do
    if [ -e "$f" ]; then
        echo "错误: 旧的 ${f#"$P"/} 还在，拆分后应删除" >&2
        exit 1
    fi
done

# 补丁已改成平铺在 patches/ 下。残留的旧分组目录会让应用器/生成器各读到一份，
# 症状是同样的改动被应用两次（hunk 冲突）或者换了路径却没更新。
olddirs=$(ls -d "$P"/0[1-8]-*/ 2>/dev/null || true)
if [ -n "$olddirs" ]; then
    echo "错误: patches/ 下还有旧的按目录分组，补丁已改成平铺（编号-分组-职责.patch）：" >&2
    printf '%s\n' "$olddirs" | sed 's/^/  /' >&2
    exit 1
fi

n=$(printf '%s\n' "$grouped" | grep -c . || true)
echo "✅ $n 个文件已分成 7 个补丁（0001-build-and-config … 0007-djui-frontend），无遗漏无重复；" >&2
echo "   语言字符串由 add_lang_strings.sh 插入，0008-android 由 gen_platform_patches.py 生成。" >&2
