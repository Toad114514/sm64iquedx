#!/bin/sh
# 把构建好的成品目录打成 macOS 的 .app，并按需再打 .dmg / .pkg / .zip。
#
#   sh platform/darwin/mkmac.sh [成品目录] [版本号] [--app-only|--zip|--pkg]
#
# 成品目录默认 <仓库根>/dist，里面应当有构建产物 sm64coopdx。
# 产物默认放在 <仓库根>：
#   SM64CoopDeluxe.app                        （--app-only / 没有 hdiutil 时）
#   sm64coopdx-iquedx_<版本>_macos-<架构>.dmg （默认，需要 macOS 的 hdiutil）
#   sm64coopdx-iquedx_<版本>_macos-<架构>.zip （--zip，任何系统都能打）
#   sm64coopdx-iquedx_<版本>_<架构>.pkg       （--pkg，需要 pkgbuild）
#
# 名字里带架构（arm64 / x86_64）：Apple Silicon 和 Intel 两个 job 都产 dmg，
# 同名的话下载下来分不清哪个能在自己机器上跑。
#
# 为什么把整个成品目录塞进 Contents/MacOS：游戏是按「可执行文件同目录下有 res/、
# lang/、*.ini」找资源的，把这些挪进 Contents/Resources 就得改代码，所以保持平铺。
# 这也意味着 .app 里没有 Contents/Resources 之外的资源目录，签名后仍照常读。
set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)

mode=dmg
dist=
version=

for arg in "$@"; do
    case "$arg" in
        --app-only) mode=none;;
        --zip)      mode=zip;;
        --pkg)      mode=pkg;;
        --dmg)      mode=dmg;;
        -h|--help)  sed -n '2,18p' "$0"; exit 0;;
        -*)         echo "未知参数：$arg" >&2; exit 2;;
        *)
            if [ -z "$dist" ]; then dist=$arg
            elif [ -z "$version" ]; then version=$arg
            else echo "多余的参数：$arg" >&2; exit 2
            fi
            ;;
    esac
done

dist=${dist:-$repo/dist}
version=${version:-1.5.1}

# uname -m：Apple Silicon 上是 arm64，Intel 上是 x86_64
arch=$(uname -m)

if [ ! -x "$dist/sm64coopdx" ]; then
    echo "找不到构建产物：$dist/sm64coopdx" >&2
    echo "先构建，或用第一个参数指定成品目录。" >&2
    exit 1
fi

app=$repo/SM64CoopDeluxe.app
# 只清我们自己生成的 .app；多写一层判断，避免路径被改坏时 rm 到别处。
if [ -e "$app" ]; then
    case "$app" in
        *.app) rm -rf "$app";;
        *) echo "拒绝删除非 .app 路径：$app" >&2; exit 1;;
    esac
fi

mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp -a "$dist/." "$app/Contents/MacOS/"
chmod +x "$app/Contents/MacOS/sm64coopdx"

sed -e "s|@BUNDLE_NAME@|SM64CoopDeluxe|g" \
    -e "s|@BUNDLE_DISPLAY_NAME@|Super Mario 64 Co-op Deluxe (iQue)|g" \
    -e "s|@BUNDLE_ID@|io.github.toad114514.sm64coopdx-iquedx|g" \
    -e "s|@EXECUTABLE@|sm64coopdx|g" \
    -e "s|@VERSION@|$version|g" \
    -e "s|@MIN_OS@|10.13|g" \
    "$here/Info.plist.in" > "$app/Contents/Info.plist"

# PkgInfo 内容固定 8 字节：APPL + 四字符签名，与 Info.plist 的 CFBundleSignature 对应。
printf 'APPL????' > "$app/Contents/PkgInfo"

# —— 图标 ——
# 优先 macOS 自带的 sips + iconutil（能出整套 sizes，Retina 也齐）；
# 没有就退化成「把 256×256 PNG 直接塞进 icns 的 ic08 块」——macOS 10.7 起支持
# PNG 承载的 icns，缺点是只有一档尺寸，Dock 里放大略糊。
make_icns() {
    src=$1
    dst=$2

    if command -v sips >/dev/null 2>&1 && command -v iconutil >/dev/null 2>&1; then
        tmp=$(mktemp -d)
        iconset=$tmp/AppIcon.iconset
        mkdir -p "$iconset"
        for s in 16 32 128 256 512; do
            d=$((s * 2))
            sips -z "$s" "$s" "$src" --out "$iconset/icon_${s}x${s}.png" >/dev/null 2>&1 || true
            sips -z "$d" "$d" "$src" --out "$iconset/icon_${s}x${s}@2x.png" >/dev/null 2>&1 || true
        done
        if iconutil -c icns "$iconset" -o "$dst" >/dev/null 2>&1; then
            rm -rf "$tmp"
            return 0
        fi
        rm -rf "$tmp"
    fi

    if command -v python3 >/dev/null 2>&1; then
        python3 - "$src" "$dst" <<'PY' && return 0
import struct, sys, zlib

src, dst = sys.argv[1], sys.argv[2]
png = open(src, 'rb').read()
if png[:8] != b'\x89PNG\r\n\x1a\n':
    sys.exit(1)

# icns 容器：'icns' + 总长度 + 若干块；ic08 = 256×256 PNG 块
block = b'ic08' + struct.pack('>I', len(png) + 8) + png
open(dst, 'wb').write(b'icns' + struct.pack('>I', len(block) + 8) + block)
PY
    fi

    return 1
}

if make_icns "$repo/platform/freedesktop/sm64coopdx-iquedx.png" "$app/Contents/Resources/AppIcon.icns"; then
    :
else
    echo "[!] 没有可用的图标转换工具（sips/iconutil 或 python3），跳过图标" >&2
fi

# 校验 plist：macOS 上用 plutil；其它系统上有些发行版也装了同名命令，但那是
# Swift 版实现，读文件参数会误报，所以按 uname 分支——非 Darwin 用 python3 的
# plistlib 验（CI 里在 Linux 上打包也能抓到语法错）。
lint_plist() {
    if [ "$(uname -s)" = Darwin ] && command -v plutil >/dev/null 2>&1; then
        plutil -lint "$1" >/dev/null 2>&1
    elif command -v python3 >/dev/null 2>&1; then
        python3 -c 'import plistlib, sys; plistlib.load(open(sys.argv[1], "rb"))' "$1" >/dev/null 2>&1
    else
        return 0
    fi
}

if ! lint_plist "$app/Contents/Info.plist"; then
    echo "Info.plist 校验失败" >&2
    exit 1
fi

# Apple Silicon 上未签名的 arm64 可执行文件会被内核直接拒绝，ad-hoc 签名即可；
# 这不是公证（notarization），从网上下载的包仍会被 Gatekeeper 拦一次。
if command -v codesign >/dev/null 2>&1; then
    if codesign --force --deep --sign - "$app" >/dev/null 2>&1; then
        echo "[i] 已做 ad-hoc 签名"
    else
        echo "[!] ad-hoc 签名失败（不影响打包，但 arm64 机器上可能起不来）" >&2
    fi
fi

echo "已生成 $app"

case "$mode" in
    none)
        exit 0
        ;;
    zip)
        zipout=$repo/sm64coopdx-iquedx_${version}_macos-${arch}.zip
        rm -f "$zipout"
        # ditto 能保留 macOS 的资源分支与权限；没有就退到普通 zip
        if command -v ditto >/dev/null 2>&1; then
            ditto -c -k --sequesterRsrc --keepParent "$app" "$zipout"
        elif command -v zip >/dev/null 2>&1; then
            ( cd "$repo" && zip -qry "$zipout" "$(basename "$app")" )
        else
            echo "没有 ditto / zip，无法生成 .zip" >&2
            exit 1
        fi
        echo "已生成 $zipout"
        ;;
    pkg)
        if ! command -v pkgbuild >/dev/null 2>&1; then
            echo "缺少 pkgbuild（Xcode Command Line Tools），无法生成 .pkg" >&2
            exit 1
        fi
        pkgout=$repo/sm64coopdx-iquedx_${version}_${arch}.pkg
        rm -f "$pkgout"
        # --component 会把 .app 原样装进 /Applications 并保留包结构
        pkgbuild --component "$app" \
                 --install-location /Applications \
                 --identifier io.github.toad114514.sm64coopdx-iquedx \
                 --version "$version" \
                 "$pkgout"
        echo "已生成 $pkgout"
        ;;
    dmg)
        if ! command -v hdiutil >/dev/null 2>&1; then
            echo "这台机器没有 hdiutil（不是 macOS），无法生成 .dmg" >&2
            echo "可以改用：sh platform/darwin/mkmac.sh $dist $version --zip" >&2
            echo ".app 已经生成好了：$app" >&2
            exit 1
        fi
        # 拖拽安装要有个 /Applications 快捷方式；hdiutil 只吃一个目录，所以先铺 staging
        stage=$(mktemp -d)
        trap 'rm -rf "$stage"' EXIT INT TERM
        cp -a "$app" "$stage/"
        ln -s /Applications "$stage/Applications"
        dmg=$repo/sm64coopdx-iquedx_${version}_macos-${arch}.dmg
        rm -f "$dmg"
        hdiutil create -quiet -volname "SM64 Coop Deluxe (iQue)" \
            -srcfolder "$stage" -ov -format UDZO "$dmg"
        echo "已生成 $dmg"
        ;;
esac

cat <<EOF

提示：
  * 本机构建、本机运行：直接 open "$app" 即可。
  * 从网上下载的 dmg/zip 会被 Gatekeeper 拦（未公证），首次请右键 → 打开，
    或执行：xattr -dr com.apple.quarantine "$app"
  * 游戏需要自备 ROM，放在 .app 内 Contents/MacOS/ 下（与 sm64coopdx 同级）。
EOF
