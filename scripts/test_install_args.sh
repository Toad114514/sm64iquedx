#!/bin/sh
# install.sh 参数传递自测。
#
#   sh scripts/test_install_args.sh
#
# 三组用例，全部离线（用桩替换 git / make / gradle，不克隆、不编译）：
#   A. 解析：用 --print-config 断言每个开关/路径/版本落到哪个变量
#   B. 错误路径：缺值、值像选项、非法值、未知参数都必须非 0 退出
#   C. 传播：参数是否真的传到 apply_patches.sh 与成品目录
#
# 退出码 0 = 全过。CI 里可以直接跑。
set -u

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d 2>/dev/null || echo "${TMPDIR:-/tmp}/iqargs.$$")
trap 'rm -rf "$tmp"' EXIT

pass=0
fail=0
ok()  { pass=$((pass + 1)); printf '  \033[32mok\033[0m   %s\n' "$*"; }
no()  { fail=$((fail + 1)); printf '  \033[31mFAIL\033[0m %s\n' "$*"; }

# 断言 --print-config 里 KEY=期望值
expect_var() {
    # expect_var <用例名> <参数串> <KEY> <期望>
    e_name=$1
    e_args=$2
    e_key=$3
    e_want=$4
    # shellcheck disable=SC2086
    e_got=$(cd "$repo" && sh install.sh $e_args -n 2>/dev/null | sed -n "s/^$e_key=//p" | head -n 1)
    if [ "$e_got" = "$e_want" ]; then
        ok "$e_name：$e_key=$e_got"
    else
        no "$e_name：$e_key 期望 [$e_want] 实际 [$e_got]"
    fi
}

# 断言某个参数组合必须以非 0 退出，且报错里含指定字样
expect_die() {
    # expect_die <用例名> <期望报错片段> <参数串>
    d_name=$1
    d_msg=$2
    d_args=$3
    # shellcheck disable=SC2086
    d_out=$(cd "$repo" && timeout 20 sh install.sh $d_args 2>&1)
    d_rc=$?
    d_line=$(printf '%s\n' "$d_out" | head -n 1)
    if [ "$d_rc" -ne 0 ] && printf '%s\n' "$d_out" | grep -qF -e "$d_msg"; then
        ok "$d_name（rc=$d_rc）"
    else
        no "$d_name：rc=$d_rc 首行「$d_line」"
    fi
}

say_section() { printf '\n== %s\n' "$*"; }

say_section "A. 参数解析（--print-config）"
expect_var "无参数默认"        ""                             TARGET       pc
expect_var "无参数默认"        ""                             ASK_PACKAGE  ""
expect_var "无参数默认"        ""                             VERSION      "$(sed -n 's/^version[[:space:]]*=[[:space:]]*//p' "$repo/upstream.txt")"
expect_var "PC 原版"           "-f"                           UP_URL       https://github.com/coop-deluxe/sm64coopdx
expect_var "PC 汉化"           "-c"                           UP_URL       https://github.com/Shangshanruo66/sm64coopdx-Chinese
expect_var "PC 汉化 TARGET"    "-c"                           TARGET       pc
expect_var "Android 原版"      "-a"                           TARGET       android
expect_var "Android 原版"      "-a"                           ASK_PACKAGE  no
expect_var "Android 汉化"      "-a -c"                        TARGET       android-cn
expect_var "Android 汉化 URL"  "-a -c"                        UP_URL       https://github.com/Shangshanruo666/sm64coopdx-Chinese-Android
expect_var "前端补丁"          "-f -n"                        ASK_FRONTEND yes
expect_var "djui_modern 长选项" "--djui-modern"               ASK_DJUI     yes
expect_var "djui_modern 短选项" "-d"                          ASK_DJUI     yes
expect_var "打包方式"          "-p rpm"                       ASK_PACKAGE  rpm
expect_var "版本号覆盖"        "-V 9.9.9"                     VERSION      9.9.9
expect_var "绝对路径"          "--path /tmp/ph-up"            UPSTREAM_DIR /tmp/ph-up
expect_var "成品目录"          "--dist /tmp/ph-dist"          DIST_DIR     /tmp/ph-dist
expect_var "相对路径拼仓库根"  "--path rel/up"                UPSTREAM_DIR "$repo/rel/up"
expect_var "-o 别名"           "-o /tmp/ph-o"                 UPSTREAM_DIR /tmp/ph-o
expect_var "参数顺序无关"      "-p no -f -d"                  ASK_DJUI     yes
expect_var "Android 忽略打包"  "-p deb -a"                    ASK_PACKAGE  no

say_section "B. 错误路径"
expect_die "-p 缺值"            "-p 需要一个值"                  "-p"
expect_die "--path 缺值"        "--path 需要一个值"              "--path"
expect_die "--dist 缺值"        "--dist 需要一个值"              "--dist"
expect_die "-V 缺值"            "-V 需要一个值"                  "-V"
expect_die "-p 后跟选项"        "-p 需要一个值"                  "-p -n"
expect_die "-V 后跟选项"        "--version 需要一个值"           "--version -n"
expect_die "--path 后跟选项"    "--path 需要一个值"              "--path --dist x"
expect_die "非法打包方式"       "只接受 auto|deb|rpm"            "-p zip -n"
expect_die "未知参数"           "未知参数"                       "--nope"
expect_die "未知短选项"         "未知参数"                       "-Z"

say_section "C. 参数是否传到下游（桩环境）"
S="$tmp/stub"
C="$tmp/tree"
mkdir -p "$S/bin"
HEAD_SHA=$(sed -n 's/^commit[[:space:]]*=[[:space:]]*//p' "$repo/upstream.txt")

# 桩 git：checkout 时造出下游需要的目录结构，其余命令空转
cat > "$S/bin/git" <<'STUB'
#!/bin/sh
d=""
while [ $# -gt 0 ]; do
    case "$1" in
        -C) d=$2; shift 2 ;;
        -*) shift ;;
        *)  cmd=$1; shift; break ;;
    esac
done
case "$cmd" in
    checkout)
        mkdir -p "$d/src/game" "$d/src/pc/djui" "$d/text" "$d/lang"
        : > "$d/Makefile"
        : > "$d/Android.mk"
        mkdir -p "$d/platform/app"
        : > "$d/platform/build.gradle"
        : > "$d/platform/app/build.gradle"
        cat > "$d/platform/gradlew" <<'G'
#!/bin/sh
mkdir -p build/outputs/apk/release
: > build/outputs/apk/release/app-release-unsigned.apk
G
        chmod +x "$d/platform/gradlew"
        ;;
    rev-parse)
        sed -n 's/^commit[[:space:]]*=[[:space:]]*//p' "$STUB_ROOT/upstream.txt"
        ;;
esac
exit 0
STUB

# 桩 make：只造出运行期文件
cat > "$S/bin/make" <<'STUB'
#!/bin/sh
b=build/us_pc
mkdir -p "$b/lang" "$b/mods" "$b/dynos" "$b/palettes"
printf '#!/bin/sh\necho stub\n' > "$b/sm64coopdx"; chmod +x "$b/sm64coopdx"
printf 'stub\n' > "$b/coopdx_updater"
: > "$b/libdiscord_game_sdk.so"
STUB
cp "$S/bin/make" "$S/bin/gmake"
chmod +x "$S/bin/git" "$S/bin/make" "$S/bin/gmake"

# 桩仓库：install.sh + upstream.txt + 记录型 apply_patches.sh
mkdir -p "$C/scripts" "$C/patches"
cp "$repo/install.sh" "$repo/upstream.txt" "$C/"
cat > "$C/scripts/apply_patches.sh" <<'STUB'
#!/bin/sh
printf 'ARGS[%s]\n' "$*"
STUB

run_tree() {
    # shellcheck disable=SC2086
    ( cd "$C" && STUB_ROOT="$C" PATH="$S/bin:$PATH" timeout 60 sh ./install.sh "$@" 2>&1 )
}

# C1: PC + 前端补丁 → apply_patches.sh 应收到 pc <目录> --frontend
out=$(run_tree -c -f -p no --path "$C/up" --dist "$C/dist")
echo "$out" | grep -q "ARGS\[pc $C/up --frontend\]" \
    && ok "PC 前端：apply_patches.sh 收到「pc <目录> --frontend」" \
    || no "PC 前端：apply_patches 参数不对 → $(echo "$out" | grep ARGS)"

# C2: 不带前端 → 不应有 --frontend
out=$(run_tree -p no --path "$C/up" --dist "$C/dist")
echo "$out" | grep -q "ARGS\[pc $C/up\]" \
    && ok "PC 无前端：apply_patches.sh 收到「pc <目录>」" \
    || no "PC 无前端：→ $(echo "$out" | grep ARGS)"

# C3: 成品落在 --dist 指定的目录
[ -x "$C/dist/sm64coopdx" ] && ok "成品目录：主程序落在 --dist" \
    || no "成品目录：$C/dist 里没有主程序"

# C4: Android 汉化 → TARGET=android-cn 传给 apply_patches.sh，且不出桌面包
out=$(run_tree -a -c --path "$C/up-and" --dist "$C/dist-and")
echo "$out" | grep -q "ARGS\[android-cn $C/up-and\]" \
    && ok "Android 汉化：apply_patches.sh 收到「android-cn <目录>」" \
    || no "Android 汉化：→ $(echo "$out" | grep ARGS)"
ls "$C/dist-and"/*.apk >/dev/null 2>&1 && ok "Android：APK 已收集" \
    || no "Android：dist-and 里没有 APK"

# C5: Android 版不能悄悄去读 PC 的 dist
[ ! -e "$C/dist-and/sm64coopdx" ] && ok "Android：没有误产出桌面主程序" \
    || no "Android：dist-and 里混进了桌面主程序"

say_section "结果"
printf '  通过 %d，失败 %d\n' "$pass" "$fail"
[ "$fail" -eq 0 ] || exit 1
