#!/bin/sh
# 把成品目录打成便携 zip（解压即玩，不装包管理器）。
#
#   sh platform/portable/mkzip.sh [成品目录] [版本]
#
# 成品目录默认 <仓库>/dist；输出 <仓库>/sm64iquedx-<版本>-<系统>-portable.zip。
# 和 deb/rpm/arch 一样，包本身放在仓库根，不放进 dist/ —— 否则下次打包会把
# 上一次的 zip 一起装进去（deb 里出现 deb）。
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
dist=${1:-$repo/dist}
version=${2:-}

if [ ! -d "$dist" ]; then
    echo "成品目录不存在：${dist}（先编译，或用 --dist 指到成品目录）" >&2
    exit 1
fi

if ! command -v zip >/dev/null 2>&1; then
    echo "缺少 zip 命令（Debian: apt install zip / Arch: pacman -S zip / macOS 自带 / MSYS2: pacman -S zip）" >&2
    exit 1
fi

if [ -z "$version" ]; then
    version=$(sed -n 's/^[[:space:]]*version[[:space:]]*=[[:space:]]*//p' "$repo/upstream.txt" 2>/dev/null | head -n 1 | tr -d '\r')
fi
[ -n "$version" ] || version=1.5.1

case "$(uname -s)" in
    Darwin)                os=macos ;;
    MINGW*|MSYS*|CYGWIN*)  os=windows ;;
    *)                     os=linux ;;
esac

out=$repo/sm64iquedx-${version}-${os}-portable.zip
rm -f "$out"

# -q 安静、-r 递归；在成品目录里 . 打进去，解压出来就是一层平铺的文件
( cd "$dist" && zip -qr "$out" . )

echo "已生成 $out"
