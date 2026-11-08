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

# macOS 自带没有 timeout(1)（要 brew 装 coreutils 才是 gtimeout），
# 找不到就直接跑：这里所有用例都是本地的短命令，超时保护只是保险。
TMO=$(command -v timeout 2>/dev/null || command -v gtimeout 2>/dev/null || true)
tmo() {
    t_secs=$1; shift
    if [ -n "$TMO" ]; then "$TMO" "$t_secs" "$@"; else "$@"; fi
}

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
        ok "${e_name}：$e_key=$e_got"
    else
        no "${e_name}：$e_key 期望 [$e_want] 实际 [$e_got]"
    fi
}

# 断言某个参数组合必须以非 0 退出，且报错里含指定字样
expect_die() {
    # expect_die <用例名> <期望报错片段> <参数串>
    d_name=$1
    d_msg=$2
    d_args=$3
    # shellcheck disable=SC2086
    d_out=$(cd "$repo" && tmo 20 sh install.sh $d_args 2>&1)
    d_rc=$?
    d_line=$(printf '%s\n' "$d_out" | head -n 1)
    if [ "$d_rc" -ne 0 ] && printf '%s\n' "$d_out" | grep -qF -e "$d_msg"; then
        ok "${d_name}（rc=${d_rc}）"
    else
        no "${d_name}：rc=$d_rc 首行「${d_line}」"
    fi
}

say_section() { printf '\n== %s\n' "$*"; }

# 上游地址不写死：这里的目的是验证「哪个开关选哪个上游」，不是校验 URL 内容。
# 写死过一次 cn-repo 的地址，upstream.txt 改了拼写之后自测就假失败。
uv() { sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" "$repo/upstream.txt" | head -n 1 | tr -d '\r'; }

say_section "A. 参数解析（--print-config）"
expect_var "无参数默认"        ""                             TARGET       pc
expect_var "无参数默认"        ""                             ASK_PACKAGE  ""
expect_var "无参数默认"        ""                             VERSION      "$(uv version)"
expect_var "PC 原版"           "-f"                           UP_URL       "$(uv repo)"
expect_var "PC 汉化"           "-c"                           UP_URL       "$(uv cn-repo)"
expect_var "PC 汉化 TARGET"    "-c"                           TARGET       pc
expect_var "Android 原版"      "-a"                           TARGET       android
expect_var "Android 原版"      "-a"                           ASK_PACKAGE  no
expect_var "Android 汉化"      "-a -c"                        TARGET       android-cn
expect_var "Android 汉化 URL"  "-a -c"                        UP_URL       "$(uv android-cn-repo)"
expect_var "前端补丁"          "-f -n"                        ASK_FRONTEND yes
expect_var "djui_modern 长选项" "--djui-modern"               ASK_DJUI     yes
expect_var "djui_modern 短选项" "-d"                          ASK_DJUI     yes
expect_var "打包方式"          "-p rpm"                       ASK_PACKAGE  rpm
expect_var "便携 zip"          "-p zip"                       ASK_PACKAGE  zip
expect_var "版本号覆盖"        "-V 9.9.9"                     VERSION      9.9.9
expect_var "绝对路径"          "--path /tmp/ph-up"            UPSTREAM_DIR /tmp/ph-up
expect_var "成品目录"          "--dist /tmp/ph-dist"          DIST_DIR     /tmp/ph-dist
expect_var "相对路径拼仓库根"  "--path rel/up"                UPSTREAM_DIR "$repo/rel/up"
expect_var "-o 别名"           "-o /tmp/ph-o"                 UPSTREAM_DIR /tmp/ph-o
expect_var "参数顺序无关"      "-p no -f -d"                  ASK_DJUI     yes
expect_var "Android 忽略打包"  "-p deb -a"                    ASK_PACKAGE  no
expect_var "-p all"            "-p all"                       ASK_PACKAGE  all
expect_var "-po 短选项"        "-po"                          PACKAGE_ONLY yes
expect_var "-po 长选项"        "--package-only"               PACKAGE_ONLY yes

say_section "B. 错误路径"
expect_die "-p 缺值"            "-p 需要一个值"                  "-p"
expect_die "--path 缺值"        "--path 需要一个值"              "--path"
expect_die "--dist 缺值"        "--dist 需要一个值"              "--dist"
expect_die "-V 缺值"            "-V 需要一个值"                  "-V"
expect_die "-p 后跟选项"        "-p 需要一个值"                  "-p -n"
expect_die "-V 后跟选项"        "--version 需要一个值"           "--version -n"
expect_die "--path 后跟选项"    "--path 需要一个值"              "--path --dist x"
expect_die "非法打包方式"       "只接受 auto|all|deb"            "-p war -n"
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
# 桩 makepkg / dpkg-deb / rpmbuild：让 -p all 的三条分支在任何机器上都走同一条路径
# （不能指望宿主机有没有装它们：CI 的 ubuntu 一定有 dpkg-deb，本地 Arch 一定没有）
cat > "$S/bin/makepkg" <<'STUB'
#!/bin/sh
echo "[stub makepkg] $*"
exit 0
STUB
cat > "$S/bin/dpkg-deb" <<'STUB'
#!/bin/sh
echo "[stub dpkg-deb] $*"
exit 0
STUB
cat > "$S/bin/rpmbuild" <<'STUB'
#!/bin/sh
echo "[stub rpmbuild] $*"
exit 0
STUB
cat > "$S/bin/zip" <<'STUB'
#!/bin/sh
echo "[stub zip] $*"
exit 0
STUB
chmod +x "$S/bin/git" "$S/bin/make" "$S/bin/gmake" \
         "$S/bin/makepkg" "$S/bin/dpkg-deb" "$S/bin/rpmbuild" "$S/bin/zip"

# 桩仓库：install.sh + upstream.txt + 记录型 apply_patches.sh + 打包脚本桩
# 打包脚本桩是必需的：-p all 会去 sh platform/<x>/mk*.sh，缺了它 sh 直接 exit 2，
# 于是「缺工具只警告」的用例会因为「脚本不存在」而假失败。
mkdir -p "$C/scripts" "$C/patches" "$C/platform/pacman" \
         "$C/platform/DEBIAN" "$C/platform/rpm" "$C/platform/darwin" \
         "$C/platform/portable"
cp "$repo/install.sh" "$repo/upstream.txt" "$C/"
cat > "$C/scripts/apply_patches.sh" <<'STUB'
#!/bin/sh
printf 'ARGS[%s]\n' "$*"
STUB
printf '#!/bin/sh\necho "[stub mkdeb] $*"\n'  > "$C/platform/DEBIAN/mkdeb.sh"
printf '#!/bin/sh\necho "[stub mkrpm] $*"\n'  > "$C/platform/rpm/mkrpm.sh"
printf '#!/bin/sh\necho "[stub mkmac] $*"\n'  > "$C/platform/darwin/mkmac.sh"
# 便携 zip 用真脚本（逻辑就是调用 zip，桩 zip 足够）
cp "$repo/platform/portable/mkzip.sh" "$C/platform/portable/mkzip.sh"
chmod +x "$C/platform/DEBIAN/mkdeb.sh" "$C/platform/rpm/mkrpm.sh" \
         "$C/platform/darwin/mkmac.sh"

# 「工具缺失」场景用的 PATH（只在 Linux 分支用到，见 C9b）：把当前 PATH 里
# 除 dpkg-deb/rpmbuild/makepkg 之外的可执行文件全部软链到一个目录，只用它当 PATH。
# 直接把 $S/bin 摘掉没用——宿主机 /usr/bin 里可能本来就有 dpkg-deb。
build_core_path() {
    mkdir -p "$S/core"
    (
        IFS=:
        for c_dir in $PATH; do
            [ -d "$c_dir" ] || continue
            for c_f in "$c_dir"/*; do
                [ -f "$c_f" ] && [ -x "$c_f" ] || continue
                c_n=${c_f##*/}
                case "$c_n" in dpkg-deb|rpmbuild|makepkg) continue ;; esac
                [ -e "$S/core/$c_n" ] || ln -s "$c_f" "$S/core/$c_n" 2>/dev/null || true
            done
        done
    )
}

run_tree() {
    # shellcheck disable=SC2086
    ( cd "$C" && STUB_ROOT="$C" PATH="$S/bin:$PATH" tmo 60 sh ./install.sh "$@" 2>&1 )
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

# C6: -po 直接拿已有产物打包，不能再打补丁/编译
out=$(run_tree -po -p no --path "$C/up" --dist "$C/dist-po")
po_rc=$?
if [ "$po_rc" -eq 0 ] \
   && ! printf '%s\n' "$out" | grep -q "ARGS\[" \
   && [ -x "$C/dist-po/sm64coopdx" ]; then
    ok "-po：跳过打补丁/编译，直接用已有产物收集到 dist"
else
    no "-po：rc=${po_rc}，ARGS=[$(printf '%s\n' "$out" | grep ARGS)]，dist=$(ls "$C/dist-po" 2>/dev/null | tr '\n' ' ')"
fi

# C7: 有 --path 但那个树根本没编译过 → 255
mkdir -p "$C/empty"
out=$(run_tree -po -p no --path "$C/empty" --dist "$C/dist-x")
po_rc=$?
[ "$po_rc" -eq 255 ] && ok "-po 无产物：exit 255" || no "-po 无产物：rc=${po_rc}（期望 255）"
printf '%s\n' "$out" | grep -q "还没开始 make" \
    && ok "-po 无产物：提示「还没开始 make」" \
    || no "-po 无产物：提示不对 → $(printf '%s\n' "$out" | tail -n 2 | tr '\n' ' ')"

# C7b: macOS 上 make 把可执行文件 mv 进 .app，build/us_pc/sm64coopdx 根本
#      不存在。-po 必须认 .app 里的那份，否则 CI 上「编完了却说还没开始 make」。
mkdir -p "$C/appup/build/us_pc/sm64coopdx.app/Contents/MacOS" \
         "$C/appup/build/us_pc/sm64coopdx.app/Contents/Resources/lang"
printf '#!/bin/sh\necho stub\n' > "$C/appup/build/us_pc/sm64coopdx.app/Contents/MacOS/sm64coopdx"
chmod +x "$C/appup/build/us_pc/sm64coopdx.app/Contents/MacOS/sm64coopdx"
out=$(run_tree -po -p no --path "$C/appup" --dist "$C/dist-app")
po_rc=$?
if [ "$po_rc" -eq 0 ] && [ -x "$C/dist-app/sm64coopdx" ]; then
    ok "-po 认 .app 里的主程序（macOS 产物形态）"
else
    no "-po .app：rc=${po_rc}，dist=$(ls "$C/dist-app" 2>/dev/null | tr '\n' ' ')"
fi

# C8: -po 没给 --path → 255，并提示路径未指定
out=$(run_tree -po -p no)
po_rc=$?
[ "$po_rc" -eq 255 ] && ok "-po 未指定路径：exit 255" || no "-po 未指定路径：rc=${po_rc}（期望 255）"
printf '%s\n' "$out" | grep -q "路径未指定" \
    && ok "-po 未指定路径：提示「路径未指定」" \
    || no "-po 未指定路径：提示不对 → $(printf '%s\n' "$out" | tail -n 1)"

# C9: -p all 按平台走对应分支。三平台期望不同，必须分开断言，
#     否则 Windows 上「暂不支持打包」会被当成失败（它本来就是对的）。
#     注意不能带 --dist：arch 分支要求成品目录 == <repo>/dist。
os_kind=other
case "$(uname -s)" in
    Darwin) os_kind=mac ;;
    MINGW*|MSYS*|CYGWIN*) os_kind=win ;;
esac

out=$(run_tree -po -p all --path "$C/up")
all_rc=$?
case "$os_kind" in
    win)
        # Windows 没有 deb/rpm/arch，只出便携 zip
        if [ "$all_rc" -eq 0 ] \
           && printf '%s\n' "$out" | grep -q "打包便携 zip" \
           && printf '%s\n' "$out" | grep -q "stub zip"; then
            ok "-p all（Windows）：只出便携 zip"
        else
            no "-p all（Windows）：rc=$all_rc → $(printf '%s\n' "$out" | grep -E '打包|stub' | tr '\n' ' ')"
        fi
        ;;
    mac)
        if [ "$all_rc" -eq 0 ] \
           && printf '%s\n' "$out" | grep -q "打包 .app / .dmg" \
           && printf '%s\n' "$out" | grep -q "stub mkmac" \
           && printf '%s\n' "$out" | grep -q "stub zip"; then
            ok "-p all（macOS）：mkmac + 便携 zip"
        else
            no "-p all（macOS）：rc=$all_rc → $(printf '%s\n' "$out" | grep -E '打包|stub' | tr '\n' ' ')"
        fi
        ;;
    *)
        if [ "$all_rc" -eq 0 ] \
           && printf '%s\n' "$out" | grep -q "stub mkdeb" \
           && printf '%s\n' "$out" | grep -q "stub mkrpm" \
           && printf '%s\n' "$out" | grep -q "stub makepkg" \
           && printf '%s\n' "$out" | grep -q "stub zip"; then
            ok "-p all（Linux）：deb / rpm / arch / zip 四条分支都跑了"
        else
            no "-p all（Linux）：rc=$all_rc → $(printf '%s\n' "$out" | grep -E '跳过|stub|打包' | tr '\n' ' ')"
        fi
        ;;
esac

# C9b: 本机缺 dpkg-deb/rpmbuild/makepkg 时只警告、不中断，rc 仍为 0
#      PATH 只给「除这三个之外的全部命令」，所以三条分支必须都走「跳过」。
if [ "$os_kind" = other ]; then
    build_core_path
    out=$( cd "$C" && STUB_ROOT="$C" PATH="$S/core" tmo 60 sh ./install.sh -po -p all --path "$C/up" 2>&1 )
    all_rc=$?
    if [ "$all_rc" -eq 0 ] \
       && printf '%s\n' "$out" | grep -q "跳过 .deb" \
       && printf '%s\n' "$out" | grep -q "跳过 .rpm" \
       && printf '%s\n' "$out" | grep -q "跳过 Arch 包"; then
        ok "-p all：缺打包工具只警告，rc=0"
    else
        no "-p all：缺工具时 rc=$all_rc → $(printf '%s\n' "$out" | grep -E '跳过|\\[x\\]' | tr '\n' ' ')"
    fi
fi

# C10: -po -a 只收集 APK，不重新 gradle
out=$(run_tree -po -a --path "$C/up-and" --dist "$C/dist-po-a")
rc_a=$?
if [ "$rc_a" -eq 0 ] && ls "$C/dist-po-a"/*.apk >/dev/null 2>&1 \
   && ! printf '%s\n' "$out" | grep -q "assembleRelease"; then
    ok "-po -a：只收集已有 APK"
else
    no "-po -a：rc=${rc_a}，dist=$(ls "$C/dist-po-a" 2>/dev/null | tr '\n' ' ')"
fi

say_section "D. 可移植性静态检查"

# D1: `$var` 紧贴非 ASCII 字符时必须写成 `${var}`。
# macOS 的 /bin/sh 是 bash 3.2，它会把全角标点的首字节吞进变量名，
bad=$(cd "$repo" && LC_ALL=C awk '
    {
        rest = $0
        while (match(rest, /\$[A-Za-z_][A-Za-z0-9_]*/)) {
            n = RSTART + RLENGTH
            c = substr(rest, n, 1)
            if (c != "" && c !~ /[ -~]/) { printf "%s:%d: %s\n", FILENAME, FNR, $0; break }
            rest = substr(rest, n)
        }
    }' $(find . -name '*.sh' -not -path './.git/*') )
if [ -z "$bad" ]; then
    ok "变量紧贴非 ASCII 均已加花括号"
else
    no "变量紧贴中文未加花括号（bash 3.2 会当变量名的一部分）："
    printf '%s\n' "$bad" | sed 's/^/        /'
fi

say_section "结果"
printf '  通过 %d，失败 %d\n' "$pass" "$fail"
[ "$fail" -eq 0 ] || exit 1
