#!/bin/sh
# 把补丁叠加到一份干净的上游 checkout 上。所有补丁都平铺在 patches/ 根目录。
#
#   scripts/apply_patches.sh <pc|android|android-cn> <上游根目录> [--frontend]
#
# 四套上游共用 0001 / 0002 / cn_extra/；Android 额外需要 0005 或 0006 平台补丁。
#
# lang/*.ini 的改动既不在 0001 里、也不做成补丁：各上游的 ini 行号与自带条目
# 都不同，做成补丁每个上游一份，量太大也不好维护。改由
# scripts/add_lang_strings.sh 按键插入（已存在则跳过），四套上游共用一份。
#
# 0001 在 Android 上只需跳过 3 个上下文漂移文件（该列表必须与
# scripts/gen_platform_patches.py 的 EXCLUDED 一致，生成脚本会做一致性断言）。
#
# 重复执行是安全的：已应用的补丁与已存在的字符串都会被识别并跳过。
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
PLAT_ANDROID="$P/0005-android.patch"
PLAT_ANDROID_CN="$P/0006-android-cn.patch"

platform=""
case "$target" in
    pc)
        set --
        ;;
    android)
        platform="$PLAT_ANDROID"
        set -- \
            --exclude=Makefile \
            --exclude=src/pc/configfile.c \
            --exclude=src/pc/pc_main.c
        ;;
    android-cn)
        platform="$PLAT_ANDROID_CN"
        set -- \
            --exclude=Makefile \
            --exclude=src/pc/configfile.c \
            --exclude=src/pc/pc_main.c
        ;;
    *) echo "未知目标: $target" >&2; usage ;;
esac

for f in "$MAIN" "$FRONT" "$P/cn_extra"; do
    [ -e "$f" ] || { echo "缺少 $f" >&2; exit 1; }
done
if [ -n "$platform" ] && [ ! -f "$platform" ]; then
    echo "缺少 ${platform}，先跑 gen_platform_patches.py" >&2
    exit 1
fi

apply_patch() {
    patch=$1
    shift
    name=$(basename "$patch")
    if [ ! -s "$patch" ]; then
        echo "  跳过 ${name}（空补丁）"
        return
    fi
    if (cd "$upstream" && git apply --check -p1 "$@" "$patch") 2>/dev/null; then
        (cd "$upstream" && git apply -p1 "$@" "$patch")
        echo "  应用 $name"
    elif (cd "$upstream" && git apply -R --check -p1 "$@" "$patch") 2>/dev/null; then
        echo "  跳过 ${name}（已应用）"
    else
        echo "错误: $name 无法干净应用，上游不是 upstream.txt 声明的基线？" >&2
        exit 1
    fi
}

echo "目标: $target  上游: $upstream"
apply_patch "$MAIN" "$@"
if [ -n "$platform" ]; then
    apply_patch "$platform"
fi
if [ "$frontend" = yes ]; then
    apply_patch "$FRONT"
fi

# cn_extra/ 的语义是「上游不存在的新增文件」，所以只补缺、不覆盖：
# 汉化版 Android 自带的 lang/Chinese.ini 必须原样留着，不能被 PC 那份顶掉。
copy_extras() {
    ( cd "$P/cn_extra" && find . -type f -print ) | sed 's|^\./||' | while IFS= read -r rel; do
        if [ -e "$upstream/$rel" ]; then
            echo "  保留上游已有的 $rel"
            continue
        fi
        mkdir -p "$upstream/$(dirname "$rel")"
        cp "$P/cn_extra/$rel" "$upstream/$rel"
        echo "  新增 $rel"
    done
}

copy_extras

# 必须在 cn_extra 之后：PC 的 lang/Chinese.ini 就是 cn_extra 带过来的。
echo "语言字符串:"
sh "$root/scripts/add_lang_strings.sh" "$upstream"

echo "完成。"
