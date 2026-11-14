#!/bin/sh
# 在本机复现 Android CI 的「应用补丁」阶段，提前发现补丁与真实基线漂移。
#
#   scripts/preflight_android.sh [android-cn|android] [--frontend]
#
# 为什么需要它：`patches/0008-android.patch` 是 Android 平台增量，本机通常没有
# 那两个上游的 checkout，所以改完 PC 侧只能手改平台补丁。`check_patch_integrity.py`
# 只能查 hunk 计数和 include 是否齐全，查不出「上下文在上游里根本不存在」——
# 这一类的症状是 CI 跑到第 4 步直接 `0008-android.patch 无法干净应用`，
# PC 侧却一切正常（因为 PC 目标根本不应用 0008）。
#
# 这里按 upstream.txt 声明的 repo/commit 现拉一份浅克隆，跑真实的 apply_patches.sh，
# 跑完就删。需要网络；不需要手动准备任何目录。
#
# 注意：只覆盖「应用补丁」这一步。djui_modern 由 install.sh 在它之前挂载，但它
# 改的是 Makefile / djui.c / Android.mk 的 SRC_DIRS 一行，不碰本仓库补丁的任何上下文。
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
target=android-cn
frontend=no

for arg in "$@"; do
    case "$arg" in
        android|android-cn) target=$arg ;;
        --frontend) frontend=yes ;;
        -h|--help)
            sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "未知参数: $arg" >&2; exit 1 ;;
    esac
done

# upstream.txt 的键名按行首锚定读，避免 cn-repo 命中 repo、android-commit 命中 commit。
read_key() {
    sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" "$root/upstream.txt" | head -n 1
}

case "$target" in
    android)
        repo=$(read_key android)
        sha=$(read_key android-commit)
        ;;
    android-cn)
        repo=$(read_key android-cn-repo)
        sha=$(read_key android-cn-commit)
        ;;
esac
[ -n "$repo" ] && [ -n "$sha" ] || { echo "upstream.txt 里读不到 $target 的 repo/commit" >&2; exit 1; }

# 浅克隆约 100 MB，别放 /tmp（很多机器上它是小容量 tmpfs，会 write error）。
work=$(mktemp -d "${PREFLIGHT_TMP:-${TMPDIR:-/var/tmp}}/preflight-android.XXXXXX")
trap 'rm -rf "$work" 2>/dev/null || true' EXIT INT HUP TERM
tree="$work/upstream"

echo "目标    : $target"
echo "上游    : $repo"
echo "commit  : $sha"
echo "临时目录: $tree"
echo

# 只拉这一个 commit（GitHub 支持按 sha fetch），比整仓克隆快很多。
mkdir -p "$tree"
(
    cd "$tree"
    git init -q .
    git remote add origin "$repo"
    git fetch -q --depth 1 origin "$sha"
    git -c advice.detachedHead=false checkout -q FETCH_HEAD
)

set --
if [ "$frontend" = yes ]; then
    set -- --frontend
fi

sh "$root/scripts/apply_patches.sh" "$target" "$tree" "$@" || {
    echo >&2
    echo "预检失败：补丁无法应用到 ${repo} @ ${sha}。" >&2
    echo "  若是 0008-android.patch，说明 Android.mk / Makefile / pc_main.c 的上下文" >&2
    echo "  已经变了，需要按真实基线重新生成（scripts/gen_platform_patches.py）。" >&2
    exit 1
}

echo
echo "预检通过：全部补丁都能干净应用到 $target 基线。"
