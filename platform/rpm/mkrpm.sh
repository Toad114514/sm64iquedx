#!/bin/sh
# 生成 .rpm 安装包（Fedora / RHEL / openSUSE 等）。
#
#   sh platform/rpm/mkrpm.sh [成品目录] [版本号]
#   sh platform/rpm/mkrpm.sh --print [成品目录] [版本号]   # 不打包，只打印装进去的东西和 rpmbuild 命令
#
# 成品目录默认 <仓库根>/dist，里面应当是完整可运行的构建产物（sm64coopdx + res/ + lang/）。
#
# 为什么不把成品当 Source0：RPM 的正常流程是「源码 → %build 编译 → 装」，而我们
# 的源码（上游 + 补丁 + cn_extra）在 apply_patches.sh 阶段就已经变成 dist/ 了，
# 再打一个只含二进制的 tar 包毫无意义。所以这里先按目标布局铺一个 staging 目录，
# 再用 --define 'stage …' 让 spec 直接 cp 进 %{buildroot}，spec 里没有 %prep/%build。
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

if [ ! -x "$dist/sm64coopdx" ]; then
    echo "找不到构建产物：$dist/sm64coopdx" >&2
    echo "先构建，或用第一个参数指定成品目录。" >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

# —— staging：目录结构就是要装进系统的结构 ——
stage=$work/stage
mkdir -p "$stage/opt/sm64coopdx-iquedx" \
         "$stage/usr/share/applications" \
         "$stage/usr/share/icons/hicolor/256x256/apps"

cp -a "$dist/." "$stage/opt/sm64coopdx-iquedx/"

# desktop 文件里的 Exec 默认就指向 /opt/sm64coopdx-iquedx/sm64coopdx；
# 这里再 sed 一遍，避免以后改默认路径时静默漂移（与 PKGBUILD / mkdeb.sh 一致）。
sed "s|^Exec=.*|Exec=/opt/sm64coopdx-iquedx/sm64coopdx|" \
    "$repo/platform/freedesktop/sm64coopdx-iquedx.desktop" \
    > "$stage/usr/share/applications/sm64coopdx-iquedx.desktop"
chmod 644 "$stage/usr/share/applications/sm64coopdx-iquedx.desktop"

install -m 644 "$repo/platform/freedesktop/sm64coopdx-iquedx.png" \
    "$stage/usr/share/icons/hicolor/256x256/apps/sm64coopdx-iquedx.png"

# 成品目录里的可执行位不能被 cp -a 之外的步骤弄丢；顺手确认一次。
if [ ! -x "$stage/opt/sm64coopdx-iquedx/sm64coopdx" ]; then
    echo "staging 里 sm64coopdx 丢了可执行位，检查 $dist 的挂载参数（noexec?）" >&2
    exit 1
fi

if [ "$print_only" -eq 1 ]; then
    echo "== staging 目录（前 40 项）=="
    find "$stage" -maxdepth 3 | sort | head -n 40
    echo
    echo "== 成品目录顶层 =="
    ls -1 "$dist" | head -n 20
    echo
    echo "== 将要执行的命令 =="
    echo "rpmbuild -bb platform/rpm/sm64coopdx-iquedx.spec \\"
    echo "    --define \"_topdir <tmp>\" --define \"stage $stage\" --define \"ver $version\""
    exit 0
fi

if ! command -v rpmbuild >/dev/null 2>&1; then
    echo "缺少 rpmbuild（rpm-build / rpmdevtools 包），无法生成 .rpm" >&2
    echo "Fedora: dnf install rpm-build   openSUSE: zypper in rpm-build" >&2
    exit 1
fi

# —— 给 rpmbuild 一个干净的 topdir，别污染 ~/rpmbuild ——
top=$work/rpmbuild
mkdir -p "$top/BUILD" "$top/RPMS" "$top/SOURCES" "$top/SPECS" "$top/BUILDROOT" "$top/SRPMS"
cp "$here/sm64coopdx-iquedx.spec" "$top/SPECS/"

# --define "ver"：spec 里 Version: %{ver}，这样版本号不用改文件（同 control.in 的思路）
# --define "_build_id_links none"：双保险，防止 __os_install_post 被别的宏重新打开时又生成 build-id
rpmbuild -bb "$top/SPECS/sm64coopdx-iquedx.spec" \
    --define "_topdir $top" \
    --define "stage $stage" \
    --define "ver $version" \
    --define "_build_id_links none"

rpmfile=$(find "$top/RPMS" -type f -name '*.rpm' | head -n 1)
if [ -z "$rpmfile" ]; then
    echo "rpmbuild 没有产出 .rpm，检查上面的输出" >&2
    exit 1
fi

out=$repo/$(basename "$rpmfile")
cp "$rpmfile" "$out"
echo "已生成 $out"
echo "安装：sudo dnf install $out   （openSUSE: sudo zypper in $out）"
