#!/bin/sh
# 把补丁叠加到一份干净的上游 checkout 上。
#
# 补丁全部平铺在 patches/ 下，文件名 = 四位编号 + 分组 + 职责，**编号就是应用
# 顺序**（`sort` 即顺序）：
#
#   patches/0001-build-and-config.patch   构建开关（Makefile / configfile）
#   patches/0002-rom-assets.patch         神游 ROM 探测与资源读取
#   patches/0003-text-core.patch          编码/字形核心、HUD 与标题文本
#   patches/0004-dialog.patch             对话框表与渲染
#   patches/0005-menus.patch              文件选择、选星盘、标题过场
#   patches/0006-asset-overlay.patch      iQue 贴图/几何覆盖
#   patches/0007-djui-frontend.patch      可选前端（--frontend 才应用）
#   patches/0008-android.patch            Android 平台增量（仅 android / android-cn）
#   patches/cn_extra/                     上游不存在的新增文件，直接拷贝
#
# 分组只体现在文件名里：可选组靠前缀 glob 区分（0007-* / 0008-*），不给可选的
# 补丁再套一层目录。
#
# 四套上游共用 0001~0006；0007 可选；0008 由两个 Android 目标共用。
#
# lang/*.ini 的改动既不在任何补丁里、也不做成补丁：各上游的 ini 行号与自带条目
# 都不同，做成补丁每个上游一份，量太大也不好维护。改由
# scripts/add_lang_strings.sh 按键插入（已存在则跳过），四套上游共用一份。
# src/pc/configfile.c 的中文配置项同理，由 scripts/add_cn_config.sh 插入。
#
# Android 上 0001-build / 0002-rom 里各有文件要跳过（该列表必须与
# scripts/gen_platform_patches.py 的 EXCLUDED 一致，生成脚本会做一致性断言）。
# 其中 Makefile / pc_main.c 由 0008-android 直接给出终态，configfile.c 由插入脚本补。
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
  --frontend  额外应用 0007-djui-frontend.patch，在「设置 → 其他」里显示中文字库开关

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
FRONT_PATCH="$P/0007-djui-frontend.patch"
PLAT_PATCH="$P/0008-android.patch"
# 两个 Android 上游共用同一份平台补丁：它们的 Makefile / pc_main.c / Android.mk
# 内容完全相同，以前每目标一份只是上下文漂移（configSnapTouch vs configElementSnap）。

platform=""
case "$target" in
    pc)
        set --
        ;;
    android)
        platform="$PLAT_PATCH"
        set -- \
            --exclude=Makefile \
            --exclude=src/pc/configfile.c \
            --exclude=src/pc/pc_main.c
        ;;
    android-cn)
        platform="$PLAT_PATCH"
        set -- \
            --exclude=Makefile \
            --exclude=src/pc/configfile.c \
            --exclude=src/pc/pc_main.c
        ;;
    *) echo "未知目标: $target" >&2; usage ;;
esac

core_count=0
for f in "$P"/000[1-6]-*.patch; do
    [ -e "$f" ] && core_count=$((core_count + 1))
done
[ "$core_count" -gt 0 ] || {
    echo "缺少 patches/000[1-6]-*.patch，先跑 scripts/gen_all_patches.sh" >&2
    exit 1
}
[ -d "$P/cn_extra" ] || { echo "缺少 patches/cn_extra/" >&2; exit 1; }

# 补丁已改成平铺在 patches/ 下。两种情况都要拦，因为它们会让同一处改动被应用
# 两次（症状是 hunk 冲突）或者读到一份没人更新的旧副本：
#   * 残留的旧分组目录 01-build/ … 08-platform/；
#   * 文件名不是「编号-分组」的平铺补丁（拆分前的 0001-src-overlay.patch 等）。
olddirs=$(ls -d "$P"/0[1-8]-*/ 2>/dev/null || true)
if [ -n "$olddirs" ]; then
    echo "错误: patches/ 下还有旧的按目录分组，补丁已改成平铺（编号-分组-职责.patch）：" >&2
    printf '%s\n' "$olddirs" | sed 's/^/  /' >&2
    exit 1
fi
stray=""
for f in "$P"/*.patch; do
    [ -e "$f" ] || continue
    case "$(basename "$f")" in
        000[1-8]-*) ;;
        *) stray="$stray
  $f" ;;
    esac
done
if [ -n "$stray" ]; then
    echo "错误: 这些补丁的文件名不是「四位编号-分组-职责.patch」：" >&2
    printf '%s\n' "$stray" >&2
    exit 1
fi
if [ -n "$platform" ] && [ ! -f "$PLAT_PATCH" ]; then
    echo "缺少 ${PLAT_PATCH#"$P"/}，先跑 gen_platform_patches.py" >&2
    exit 1
fi
if [ "$frontend" = yes ] && [ ! -f "$FRONT_PATCH" ]; then
    echo "缺少 ${FRONT_PATCH#"$P"/}，先跑 gen_all_patches.sh" >&2
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
# 文件名编号是全局的，glob 排序即应用顺序（0001 → 0008）。
for patch in "$P"/000[1-6]-*.patch; do
    [ -e "$patch" ] || continue
    apply_patch "$patch" "$@"
done
if [ -n "$platform" ]; then
    apply_patch "$platform"
fi
if [ "$frontend" = yes ]; then
    apply_patch "$FRONT_PATCH"
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

# configfile.c 的中文配置项：Android 上 01-build 因为上下文漂移被 --exclude
# 掉了，这里补上。（PC 目标不用管，01-build 已经直接把它改好了）
if [ "$target" != pc ]; then
    echo "配置项:"
    sh "$root/scripts/add_cn_config.sh" "$upstream"
fi

# 必须在 cn_extra 之后：PC 的 lang/Chinese.ini 就是 cn_extra 带过来的。
echo "语言字符串:"
sh "$root/scripts/add_lang_strings.sh" "$upstream"

echo "完成。"
