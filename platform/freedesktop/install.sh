#!/bin/sh
# 为非打包发行版安装 .desktop 启动器与图标。
#
#   sh platform/freedesktop/install.sh [安装根目录] [--uninstall]
#
# 安装根目录默认 /opt/sm64coopdx-iquedx（与 README 中「其他发行版」的约定一致），
# 目录里应当已经有构建产物 sm64coopdx。
set -eu

here=$(cd "$(dirname "$0")" && pwd)
dest=/opt/sm64coopdx-iquedx
uninstall=0

for arg in "$@"; do
    case "$arg" in
        --uninstall|-u) uninstall=1;;
        -*) echo "未知参数：$arg" >&2; exit 2;;
        *) dest=$arg;;
    esac
done

# DESTDIR 用于装进 staging 目录（打包 / 测试），此时不碰系统目录也不需要 root。
prefix=${DESTDIR:-}
appdir=$prefix/usr/share/applications
icondir=$prefix/usr/share/icons/hicolor/256x256/apps
desktop=$appdir/sm64coopdx-iquedx.desktop
icon=$icondir/sm64coopdx-iquedx.png

# 写 /usr/share 需要 root；没有 root 就用 sudo，两者都没有就报错退出。
# 指定了 DESTDIR 时文件落在普通目录里，不去动 sudo。
as_root() {
    if [ -n "${DESTDIR:-}" ]; then
        "$@"
    elif [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        echo "需要 root 权限写入 $appdir（没有 sudo）" >&2
        exit 1
    fi
}

# 刷新桌面数据库 / 图标缓存。DESTDIR 是打包用的 staging 目录，不该去动宿主机的缓存。
refresh_caches() {
    [ -z "${DESTDIR:-}" ] || return 0
    if command -v update-desktop-database >/dev/null 2>&1; then
        as_root update-desktop-database "$appdir" || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        as_root gtk-update-icon-cache -q -t "$prefix/usr/share/icons/hicolor" || true
    fi
}

if [ "$uninstall" -eq 1 ]; then
    as_root rm -f "$desktop" "$icon"
    refresh_caches
    echo "已移除启动器与图标"
    exit 0
fi

exe=$dest/sm64coopdx
if [ ! -x "$exe" ]; then
    echo "找不到可执行文件：$exe" >&2
    echo "先把构建产物放到该目录，或用第一个参数指定安装根目录。" >&2
    exit 1
fi

# Exec 换成真实安装路径：desktop 文件里写的是默认路径，用户可能装到别处。
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT INT TERM
sed "s|^Exec=.*|Exec=$exe|" "$here/sm64coopdx-iquedx.desktop" > "$tmp"

as_root install -d "$appdir" "$icondir"
as_root install -m 644 "$tmp" "$desktop"
as_root install -m 644 "$here/sm64coopdx-iquedx.png" "$icon"

if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$desktop" || echo "[!] desktop 文件校验有警告" >&2
fi
refresh_caches

echo "已安装："
echo "  $desktop"
echo "  $icon"
