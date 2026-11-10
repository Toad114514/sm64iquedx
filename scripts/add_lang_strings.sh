#!/bin/sh
# 给目标树里的 lang/*.ini 补上「中文字库」开关所需的两条字符串。幂等，可重复跑。
#
#   scripts/add_lang_strings.sh <上游根目录> [--check]
#
# 为什么是脚本而不是补丁：
#   PC、原版 Android、汉化版 Android 各有自己的一份 lang/，行号与自带条目都
#   不同（汉化版已经自带 Chinese = "Chinese (中文)"）。为它们各生成一份 ini
#   补丁，补丁数会涨上去，上游动一行又要全部重生成。这里改成「按键插入」：
#   锚点存在就插在锚点后，键已存在就跳过，所以同一份脚本对四套上游都成立。
#
# 插入的三条：
#   1. [MISC] 段内，紧跟 DISABLE_POPUPS：
#        CHINESE_TEXT_PACK          设置界面里的复选框标题
#   2. [MISC] 段内，紧跟 CHINESE_TEXT_PACK：
#        CHINESE_TEXT_PACK_NO_ROM   没有神游 dump 时复选框下面那行灰字
#   3. [LANGUAGE] 段内，紧跟 Spanish：
#        Chinese = "..."            语言列表里的中文条目
#      汉化版上游已有这条，脚本自动跳过。
set -eu

usage() {
    cat >&2 <<'EOF'
用法: scripts/add_lang_strings.sh <上游根目录> [--check]

  上游根目录   已叠加过补丁的 checkout（也必须是 patches/cn_extra 覆盖之后的，
               因为 PC 的 lang/Chinese.ini 本身就来自 cn_extra）
  --check      只报告要做什么，不写文件；已完整则退出码 0

幂等：键已存在时跳过。锚点缺失时报错，不猜位置。
EOF
    exit 1
}

[ $# -ge 1 ] || usage
target=$(cd "$1" && pwd)
shift

check=no
for arg in "$@"; do
    case "$arg" in
        --check) check=yes ;;
        *) echo "未知参数: $arg" >&2; usage ;;
    esac
done

changed=0

# insert_after <文件> <锚点行前缀> <要插入的整行> <键名>
insert_after() {
    file=$1
    anchor=$2
    line=$3
    key=$4

    if grep -q "^$key[[:space:]]*=" "$file"; then
        echo "  已有 $key  $file"
        return 0
    fi

    n=$(grep -n "^$anchor" "$file" | head -1 | cut -d: -f1)
    if [ -z "$n" ]; then
        echo "错误: $file 里找不到锚点 ${anchor}，不敢猜插入位置" >&2
        exit 1
    fi

    if [ "$check" = yes ]; then
        echo "  待插入 ${key}（第 $((n + 1)) 行，$anchor 之后）$file"
        changed=$((changed + 1))
        return 0
    fi

    # 用 head/tail 拼接而不是 sed -i：不依赖 GNU/BSD 差异，也不碰 UTF-8 字节。
    tmp="$file.ique-tmp"
    {
        head -n "$n" "$file"
        printf '%s\n' "$line"
        tail -n +"$((n + 1))" "$file"
    } > "$tmp"
    mv "$tmp" "$file"
    echo "  插入 $key  $file"
    changed=$((changed + 1))
}

# 只有这两份 ini 有对应文案。别的语言文件按上游原样留着。
for ini in "$target/lang/English.ini" "$target/lang/Chinese.ini"; do
    if [ ! -f "$ini" ]; then
        echo "  跳过（不存在）$ini"
        continue
    fi
    case "$(basename "$ini")" in
        English.ini)
            pack='CHINESE_TEXT_PACK = "Chinese Text Pack (iQue)"'
            no_rom='CHINESE_TEXT_PACK_NO_ROM = "Requires the iQue dump: put 0010cd30.z64 in the game folder, then restart"'
            name='Chinese = "Chinese (中文)"'
            ;;
        Chinese.ini)
            pack='CHINESE_TEXT_PACK = "中文字库（神游）"'
            no_rom='CHINESE_TEXT_PACK_NO_ROM = "需要神游卡带 dump：把 0010cd30.z64 放进游戏目录后重启"'
            name='Chinese = "中文"'
            ;;
        *) continue ;;
    esac
    insert_after "$ini" DISABLE_POPUPS "$pack" CHINESE_TEXT_PACK
    # 锚点是上一条：它的插入是幂等的，所以第二遍一定找得到这一行。
    # 锚点末尾带一个空格，免得 `^CHINESE_TEXT_PACK` 同时命中 CHINESE_TEXT_PACK_NO_ROM。
    insert_after "$ini" "CHINESE_TEXT_PACK " "$no_rom" CHINESE_TEXT_PACK_NO_ROM
    insert_after "$ini" "Spanish " "$name" Chinese
done

if [ "$changed" -eq 0 ]; then
    echo "语言字符串已齐全，无需改动。"
elif [ "$check" = yes ]; then
    echo "共需插入 $changed 条（--check，未写盘）。"
else
    echo "已插入 $changed 条语言字符串。"
fi
