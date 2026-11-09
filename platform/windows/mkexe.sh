#!/bin/sh
# 把成品目录打成 Windows 安装向导（Inno Setup 的 setup.exe）。
#
#   sh platform/windows/mkexe.sh [成品目录] [版本]
#   sh platform/windows/mkexe.sh --check     # 只探测 ISCC 在不在（install.sh 用）
#
# 只有 Windows（MSYS2 / Git Bash）能跑：ISCC.exe 是原生 Windows 程序。
# 输出 <仓库>/sm64iquedx-<版本>-windows-<架构>-setup.exe。
#
# 和 deb/rpm/arch 一样，生成的安装包放在仓库根、不放 dist/ —— 否则下次打包
# 会把上一次的 setup.exe 一起装进去。
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
iss=$repo/platform/windows/sm64coopdx-iquedx.iss

# ── 找 ISCC.exe ──────────────────────────────────────────────────────────────
# 优先级：$ISCC → PATH → Inno Setup 的默认安装位置。
# 默认路径里有空格和括号，所以是「逐条判断」，绝不拼成一条再由 shell 切词。
find_iscc() {
    if [ -n "${ISCC:-}" ]; then
        if [ -f "$ISCC" ]; then
            printf '%s\n' "$ISCC"
            return 0
        fi
        printf 'ISCC 指向的文件不存在：%s\n' "$ISCC" >&2
        return 1
    fi
    for c in iscc iscc.exe ISCC.exe; do
        if command -v "$c" >/dev/null 2>&1; then
            command -v "$c"
            return 0
        fi
    done
    for d in "/c/Program Files (x86)/Inno Setup 6" \
             "/c/Program Files/Inno Setup 6" \
             "/c/Program Files (x86)/Inno Setup 5"; do
        if [ -f "$d/ISCC.exe" ]; then
            printf '%s\n' "$d/ISCC.exe"
            return 0
        fi
    done
    return 1
}

if [ "${1:-}" = "--check" ]; then
    find_iscc >/dev/null 2>&1
    exit $?
fi

dist=${1:-$repo/dist}
version=${2:-}

[ -d "$dist" ] || {
    printf '成品目录不存在：%s（先编译，或用 --dist 指到成品目录）\n' "$dist" >&2
    exit 1
}
[ -f "$iss" ] || { printf '缺少安装脚本：%s\n' "$iss" >&2; exit 1; }

if [ -z "$version" ]; then
    version=$(sed -n 's/^[[:space:]]*version[[:space:]]*=[[:space:]]*//p' "$repo/upstream.txt" 2>/dev/null | head -n 1 | tr -d '\r')
fi
[ -n "$version" ] || version=1.5.1

# 原生 Windows 程序不认 /c/... 这种 MSYS 路径，也不认 / 作分隔符
win_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s' "$1" | sed -e 's|^/\([A-Za-z]\)/|\1:/|' -e 's|/|\\|g'
    fi
}

iscc=$(find_iscc) || {
    printf '[x] 找不到 ISCC.exe（Inno Setup 编译器）。\n' >&2
    printf '    Windows 上装一个：choco install innosetup  或到 jrsoftware.org 下载安装包。\n' >&2
    printf '    装在非默认目录时用环境变量指定：ISCC="C:\\路径\\ISCC.exe" sh %s\n' "$0" >&2
    exit 1
}

# .iss 丢 BOM 会让中文按 ANSI 解析成乱码，编译不报错但界面全乱 —— 提前查一下
if [ "$(head -c 3 "$iss" | od -An -tx1 | tr -d ' \n')" != "efbbbf" ]; then
    printf '[!] %s 不是 UTF-8 带 BOM，装出来的向导里中文会是乱码\n' "$iss" >&2
fi

# dist 里必须有主程序，否则会打出一个空壳安装包
[ -f "$dist/sm64coopdx.exe" ] || {
    printf '成品目录里没有 sm64coopdx.exe：%s\n' "$dist" >&2
    exit 1
}

arch=$(uname -m)
printf 'ISCC: %s\n' "$iscc"
printf '源目录: %s\n' "$(win_path "$dist")"
printf '版本: %s  架构: %s\n' "$version" "$arch"

# 中文界面：Inno Setup 6 自带 Languages\ChineseSimplified.isl。按 ISCC 的**同级
# 目录**去找，不依赖 ISPP 的预定义变量（那个名字各版本不一致，赌不起）。
# 找不到就退回英文界面 —— 编译失败比界面是英文糟糕得多。
iscc_lang_dir=$(dirname "$iscc")/Languages
chinese=/DWithChinese=0
if [ -f "$iscc_lang_dir/ChineseSimplified.isl" ]; then
    chinese=/DWithChinese=1
else
    printf '[!] %s 里没有 ChineseSimplified.isl，安装向导用英文界面\n' "$iscc_lang_dir" >&2
fi

# /D 的值里可能有空格：整个参数由 shell 合成一项再交给 ISCC，ISCC 自己按
# `=` 之后取到参数末尾，不会再切词。
# shellcheck disable=SC2086
"$iscc" /Qp \
    "$chinese" \
    "/DMyAppVersion=$version" \
    "/DMySourceDir=$(win_path "$dist")" \
    "/DMyOutputDir=$(win_path "$repo")" \
    "$(win_path "$iss")"

out=$repo/sm64iquedx-${version}-windows-${arch}-setup.exe
if [ -f "$out" ]; then
    printf '已生成 %s\n' "$out"
else
    # ISCC 成功但文件名和我们预期不一致（版本宏没生效之类），别假装成功
    printf '[x] ISCC 结束了但没看到 %s，检查上面的输出\n' "$out" >&2
    ls -l "$repo"/*.exe 2>/dev/null >&2 || true
    exit 1
fi
