#!/bin/sh
# 生成 .deb 安装包。
#
#   sh platform/DEBIAN/mkdeb.sh [成品目录] [版本号]
#   sh platform/DEBIAN/mkdeb.sh --print [成品目录] [版本号]   # 只打印生成的 control
#
# 成品目录默认是 <仓库根>/dist，里面应当已经有构建产物 sm64coopdx。
#
# 为什么 control 是模板（control.in）而不是成品：Debian 的 Architecture 字段
# 只能写**一个**架构名，“amd64, aarch64” 这种写法 dpkg 不接受，而且 Debian 侧
# arm64 不叫 aarch64。所以架构与版本号都在这里按当前构建机填充。
set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)

print_only=0
if [ "${1:-}" = "--print" ]; then
    print_only=1
    shift
fi

dist=${1:-$repo/dist}
version=${2:-1.5.1}

# Debian 的架构命名：有 dpkg 就问它，没有就按 uname 映射。
deb_arch() {
    if command -v dpkg >/dev/null 2>&1; then
        dpkg --print-architecture
        return
    fi
    case "$(uname -m)" in
        x86_64|amd64) echo amd64;;
        aarch64|arm64) echo arm64;;
        armv7l|armhf) echo armhf;;
        *) echo "无法确定 Debian 架构名，请先在 Debian 系系统上运行" >&2; exit 1;;
    esac
}

arch=$(deb_arch)

gen_control() {
    sed -e "s|@ARCH@|$arch|g" -e "s|@VERSION@|$version|g" "$here/control.in"
}

if [ "$print_only" -eq 1 ]; then
    gen_control
    exit 0
fi

if [ ! -x "$dist/sm64coopdx" ]; then
    echo "找不到构建产物：$dist/sm64coopdx" >&2
    echo "先构建，或用第一个参数指定成品目录。" >&2
    exit 1
fi

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "缺少 dpkg-deb（dpkg-dev / dpkg 包），无法生成 .deb" >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

pkg=$work/sm64coopdx-iquedx
mkdir -p "$pkg/DEBIAN" "$pkg/opt/sm64coopdx-iquedx" \
         "$pkg/usr/share/applications" \
         "$pkg/usr/share/icons/hicolor/256x256/apps"

cp -a "$dist/." "$pkg/opt/sm64coopdx-iquedx/"
gen_control > "$pkg/DEBIAN/control"
chmod 644 "$pkg/DEBIAN/control"

sed "s|^Exec=.*|Exec=/opt/sm64coopdx-iquedx/sm64coopdx|" \
    "$repo/platform/freedesktop/sm64coopdx-iquedx.desktop" \
    > "$pkg/usr/share/applications/sm64coopdx-iquedx.desktop"
chmod 644 "$pkg/usr/share/applications/sm64coopdx-iquedx.desktop"

install -m 644 "$repo/platform/freedesktop/sm64coopdx-iquedx.png" \
    "$pkg/usr/share/icons/hicolor/256x256/apps/sm64coopdx-iquedx.png"

out=$repo/sm64coopdx-iquedx_${version}_${arch}.deb
echo "架构：${arch}　成品目录：${dist}　输出：${out}"
# --root-owner-group 是 dpkg 1.19 才有的；老一点的 dpkg-deb 不认这个参数，
# 会直接退 2（看着像「打包脚本坏了」）。有就用，没有就退到普通 --build。
if dpkg-deb --help 2>&1 | grep -q -- '--root-owner-group'; then
    dpkg-deb --build --root-owner-group "$pkg" "$out"
else
    dpkg-deb --build "$pkg" "$out"
fi
echo "已生成 $out"
