#!/bin/sh
# 把中文字库的配置项插进 src/pc/configfile.c。
#
# 为什么这个文件不做成补丁：两套 Android 上游的 configfile.c 都在 PC 补丁要插入
# 的同一位置插了自己的 TOUCH_CONTROLS 块，所以 0001 必须 --exclude 掉它；而剩下的
# 「上游 + 我们这 6 行」只有在**各自的上游**上才是正确终态，做成补丁就得每个上游
# 各一份（原来的 0005 / 0006），而两份补丁的差别只有 hunk 上下文里的
# `configSnapTouch` / `configElementSnap` —— 纯属上下文漂移。
#
# 我们加的东西本身就是「在某个锚点前/后插几行」，正好能用幂等插入表达，于是和
# add_lang_strings.sh 一样做成脚本：一份脚本服务所有上游。
#
# 用法：
#   scripts/add_cn_config.sh <上游根目录> [--check]
#
# 行为：
#   两处锚点各插一组，已存在就跳过（按键名判断，不看行号）；
#   锚点缺失直接报错退出，不猜位置；
#   --check 只报告不写盘，退出码 0。

set -eu

target=${1:-}
mode=${2:-}
[ -n "$target" ] || { echo "用法: $0 <上游根目录> [--check]" >&2; exit 2; }
[ -d "$target" ] || { echo "目录不存在：$target" >&2; exit 2; }

cfg=$target/src/pc/configfile.c
[ -f "$cfg" ] || { echo "找不到 $cfg" >&2; exit 2; }

GLOBALS='bool         configChineseText                    = true;
bool         configChineseTextPrompted            = false;
bool         configChineseTextUserSet             = false;'

ENTRIES='    {.name = "chinese_text",                   .type = CONFIG_TYPE_BOOL,   .boolValue   = &configChineseText},
    {.name = "chinese_text_prompted",          .type = CONFIG_TYPE_BOOL,   .boolValue   = &configChineseTextPrompted},
    {.name = "chinese_text_user_set",          .type = CONFIG_TYPE_BOOL,   .boolValue   = &configChineseTextUserSet},'

# 锚点用**固定子串**而不是正则：`\{` 在 BRE 里是区间的开头，GNU grep 会直接报
# 「不匹配的 \{」。这两个子串在三份 configfile.c（PC 基线、两个 Android 上游）里
# 都各自只出现一次（已实测）。
#
# 第一处：配置变量声明，紧跟 configLanguage（与 0001 的 PC 改动同一位置）
G_ANCHOR='configLanguage[MAX_CONFIG_STRING]'
# 第二处：配置表条目，插在 force_4by3 之前。
# 选它当锚点是因为三份文件里它都紧跟「语言相关的最后一项」；0001 的 PC 版本把
# 条目插在 language 之后，而 language 下面紧跟的就是它，所以落点完全一致。
E_ANCHOR='{.name = "force_4by3"'

have_globals=no
have_entries=no
if grep -qF 'bool         configChineseText' "$cfg"; then have_globals=yes; fi
if grep -qF '{.name = "chinese_text"' "$cfg"; then have_entries=yes; fi

if [ "$have_globals" = yes ] && [ "$have_entries" = yes ]; then
    echo "已齐全：$(basename "$target")"
    exit 0
fi

if [ "$have_globals" = no ] && ! grep -qF "$G_ANCHOR" "$cfg"; then
    echo "锚点缺失：$cfg 里找不到 configLanguage 的声明行" >&2
    exit 1
fi
if [ "$have_entries" = no ] && ! grep -qF "$E_ANCHOR" "$cfg"; then
    echo "锚点缺失：$cfg 里找不到 force_4by3 的配置表条目" >&2
    exit 1
fi

if [ "$mode" = "--check" ]; then
    echo "待插入：$(basename "$target")/src/pc/configfile.c"
    if [ "$have_globals" = no ]; then echo "  - 3 个 configChineseText* 声明"; fi
    if [ "$have_entries" = no ]; then echo "  - 3 条 chinese_text* 配置表条目"; fi
    exit 0
fi

# 用 awk 重写整份文件再 mv 回去：不用 sed -i（GNU/BSD 参数不同，且容易动到
# UTF-8 字节），也不留一个猜不到名字的临时文件。
tmp=$cfg.cnconfig.$$
f_decl=$cfg.cndecl.$$
f_ent=$cfg.cnent.$$
trap 'rm -f "$tmp" "$f_decl" "$f_ent"' EXIT

# ⚠️ 待插入的文本**必须走文件**，不能用 `awk -v g="$GLOBALS"` 传。
# mawk（Ubuntu 的 /bin/awk 就是它）的 -v 赋值是拿源码解析的，值里有裸换行就
# 直接 `awk: newline in string ...` + 退出码 2；gawk 容忍，所以本机测不出来。
# 用 getline 读文件则所有实现都一致。
printf '%s\n' "$GLOBALS" > "$f_decl"
printf '%s\n' "$ENTRIES" > "$f_ent"

if ! awk -v g_file="$f_decl" -v e_file="$f_ent" \
    -v g_anchor="$G_ANCHOR" -v e_anchor="$E_ANCHOR" \
    -v want_g="$have_globals" -v want_e="$have_entries" '
    function slurp(f,   s, l) {
        s = ""
        while ((getline l < f) > 0) { s = s l "\n" }
        close(f)
        return s
    }
    { lines[NR] = $0 }
    index($0, g_anchor) && $0 !~ /^[[:space:]]*(extern|[/])/ && g_at == 0 { g_at = NR }
    index($0, e_anchor) && e_at == 0 { e_at = NR }
    END {
        # 锚点只以 extern/注释形式出现时前面 grep 是过得去的，这里不能静默跳过
        if (want_g == "no" && g_at == 0) { print "add_cn_config: 锚点无效（只有 extern/注释行）：" g_anchor > "/dev/stderr"; exit 3 }
        if (want_e == "no" && e_at == 0) { print "add_cn_config: 锚点无效：" e_anchor > "/dev/stderr"; exit 3 }
        g = slurp(g_file)
        e = slurp(e_file)
        for (i = 1; i <= NR; i++) {
            # 配置表条目要在锚点行**之前**插入
            if (want_e == "no" && i == e_at) { printf "%s", e }
            print lines[i]
            # 声明要在锚点行**之后**插入
            if (want_g == "no" && i == g_at) { printf "%s", g }
        }
    }
' "$cfg" > "$tmp"; then
    rm -f "$tmp"
    echo "插入失败：$cfg" >&2
    exit 1
fi

mv "$tmp" "$cfg"
trap - EXIT
echo "已插入：$(basename "$target")/src/pc/configfile.c"
