#!/bin/sh
# 由 src/ 对上游基线生成补丁。
#
#   scripts/gen_patches.sh <上游根目录> [输出文件] [--only 前缀] [--exclude 前缀]
#
# 上游根目录 = 与 upstream.txt 声明的 commit 一致的干净 checkout。
# 默认输出 patches/0001-src-overlay.patch，可被 `git apply -p1` / `patch -p1` 应用。
# --only / --exclude 按相对路径前缀筛选（各一个），用于把可选改动拆成独立补丁。
set -eu

if [ $# -lt 1 ]; then
    sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
fi

root=$(cd "$(dirname "$0")/.." && pwd)
upstream=$(cd "$1" && pwd)
shift
if [ $# -gt 0 ] && [ "${1#--}" = "$1" ]; then
    out=$1
    shift
else
    out=patches/0001-src-overlay.patch
fi

only=""
exclude=""
while [ $# -gt 0 ]; do
    case "$1" in
        --only)    [ $# -ge 2 ] || { echo "--only 需要参数" >&2; exit 2; }; only=$2;    shift 2;;
        --exclude) [ $# -ge 2 ] || { echo "--exclude 需要参数" >&2; exit 2; }; exclude=$2; shift 2;;
        *) echo "未知参数：$1" >&2; exit 2;;
    esac
done

[ -d "$root/src" ] || { echo "找不到 $root/src" >&2; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM
: > "$work/out.patch"
find "$root/src" -type f ! -name '.gitkeep' | sort > "$work/list"

count=0
while IFS= read -r f; do
    rel=${f#"$root/src/"}

    if [ -n "$only" ]; then
        case "$rel" in "$only"*) ;; *) continue ;; esac
    fi
    if [ -n "$exclude" ]; then
        case "$rel" in "$exclude"*) continue ;; esac
    fi

    target="$upstream/$rel"

    if [ ! -e "$target" ]; then
        echo "错误: $rel 在上游不存在。新增文件请放 patches/cn_extra/" >&2
        exit 1
    fi
    cmp -s "$f" "$target" && continue

    # diff 遇到二进制只打印一行「二进制文件不同」并当作普通差异，
    # 必须先自己判定，否则会把那句提示当成补丁内容写进 .patch
    if ! LC_ALL=C tr -d '\000' < "$f" | cmp -s - "$f" ||
       ! LC_ALL=C tr -d '\000' < "$target" | cmp -s - "$target"; then
        echo "错误: $rel 含 NUL 字节（二进制）。请放 patches/cn_extra/" >&2
        exit 1
    fi

    rc=0
    diff -u --label "a/$rel" --label "b/$rel" "$target" "$f" >> "$work/out.patch" || rc=$?
    if [ "$rc" -ge 2 ]; then
        echo "错误: $rel 生成 diff 失败（rc=$rc）" >&2
        exit 1
    fi
    echo "  M $rel" >&2
    count=$((count + 1))
done < "$work/list"

if [ "$count" -gt 0 ]; then
    mkdir -p "$(dirname "$out")"
    cat "$work/out.patch" > "$out"
    echo "已生成 $out（$count 个文件）" >&2
else
    echo "筛出的文件与基线一致，无需生成补丁" >&2
fi
