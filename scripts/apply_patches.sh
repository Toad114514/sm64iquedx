#!/bin/sh
# 把补丁叠加到一份干净的上游 checkout 上。所有补丁都平铺在 patches/ 根目录。
#
#   scripts/apply_patches.sh <pc|android|android-cn> <上游根目录> [--frontend]
#
# 四套上游共用 0001 / 0002 / 0003 / cn_extra/；Android 额外需要
# 0005 或 0006 平台补丁，汉化版 Android 的 ini 与其它上游不同，改用 0004。
#
# lang/*.ini 的改动已从 0001 里彻底拿掉，所以 0001 在 Android 上只需要跳过
# 3 个上下文漂移文件（该列表必须与 scripts/gen_platform_patches.py 的
# EXCLUDED 一致，生成脚本会做一致性断言）。
#
# 重复执行是安全的：已应用的补丁会被识别并跳过。
set -eu

usage() {
    cat >&2 <<'EOF'
用法: scripts/apply_patches.sh <pc|android|android-cn> <上游根目录> [--frontend]

  pc          coop-deluxe/sm64coopdx @ upstream.txt 的 pc-commit
              （Shangshanruo 的 PC 汉化版也用这个目标）
  android     ManIsCat2/sm64coopdx
  android-cn  Shangshanruo666/sm64coopdx-Chinese-Android
  --frontend  额外应用 0002，在「设置 → 其他」里显示中文字库开关

上游根目录必须是 upstream.txt 声明的 commit 的干净 checkout。
EOF
    exit 1
}

[ $# -ge 2 ] || usage
target=$1
upstream=$(cd "$2" && pwd)
shift 2

frontend=no
for arg in "$@"; do
    case "$arg" in
        --frontend) frontend=yes ;;
        *) echo "未知参数: $arg" >&2; usage ;;
    esac
done

root=$(cd "$(dirname "$0")/.." && pwd)
P="$root/patches"
MAIN="$P/0001-src-overlay.patch"
FRONT="$P/0002-djui-frontend.patch"
LANG_PC="$P/0003-lang-strings.patch"
LANG_CN="$P/0004-lang-android-cn.patch"
PLAT_ANDROID="$P/0005-android.patch"
PLAT_ANDROID_CN="$P/0006-android-cn.patch"

platform=""
case "$target" in
    pc)
        set --
        lang="$LANG_PC"
        ;;
    android)
        platform="$PLAT_ANDROID"
        lang="$LANG_PC"
        set -- \
            --exclude=Makefile \
            --exclude=src/pc/configfile.c \
            --exclude=src/pc/pc_main.c
        ;;
    android-cn)
        platform="$PLAT_ANDROID_CN"
        lang="$LANG_CN"
        set -- \
            --exclude=Makefile \
            --exclude=src/pc/configfile.c \
            --exclude=src/pc/pc_main.c
        ;;
    *) echo "未知目标: $target" >&2; usage ;;
esac

for f in "$MAIN" "$FRONT" "$lang" "$P/cn_extra"; do
    [ -e "$f" ] || { echo "缺少 $f" >&2; exit 1; }
done
if [ -n "$platform" ] && [ ! -f "$platform" ]; then
    echo "缺少 $platform，先跑 gen_platform_patches.py" >&2
    exit 1
fi

apply_patch() {
    patch=$1
    shift
    name=$(basename "$patch")
    if [ ! -s "$patch" ]; then
        echo "  跳过 $name（空补丁）"
        return
    fi
    if (cd "$upstream" && git apply --check -p1 "$@" "$patch") 2>/dev/null; then
        (cd "$upstream" && git apply -p1 "$@" "$patch")
        echo "  应用 $name"
    elif (cd "$upstream" && git apply -R --check -p1 "$@" "$patch") 2>/dev/null; then
        echo "  跳过 $name（已应用）"
    else
        echo "错误: $name 无法干净应用，上游不是 upstream.txt 声明的基线？" >&2
        exit 1
    fi
}

echo "目标: $target  上游: $upstream"
apply_patch "$MAIN" "$@"
apply_patch "$lang"
if [ -n "$platform" ]; then
    apply_patch "$platform"
fi
if [ "$frontend" = yes ]; then
    apply_patch "$FRONT"
fi

cp -R "$P/cn_extra/." "$upstream/"
echo "  覆盖 cn_extra/ 新增文件"
echo "完成。"
