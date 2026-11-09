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

# MSYS2 默认以 noacl 挂载：chmod 是空操作，一个没有扩展名的文件永远
# 不被当成可执行（`[ -x foo ]` 恒假）。Windows 上「有没有 x 位」本来就没意义，
# 所以断言文件存在就够了；Linux/macOS 上仍然要真的可执行。
is_exe() {
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) [ -f "$1" ] ;;
        *)                    [ -x "$1" ] ;;
    esac
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

# 桩 make：只造出运行期文件。
# Windows 上真实产物是 sm64coopdx.exe，`-p all` 会接着打安装向导，而 mkexe.sh
# 要求 dist 里有 sm64coopdx.exe —— 桩里也必须造出同名文件，否则 Windows 上
# 自测会在「打包 Windows 安装向导」阶段失败（那是桩不对，不是包不对）。
cat > "$S/bin/make" <<'STUB'
#!/bin/sh
b=build/us_pc
mkdir -p "$b/lang" "$b/mods" "$b/dynos" "$b/palettes"
printf '#!/bin/sh\necho stub\n' > "$b/sm64coopdx"; chmod +x "$b/sm64coopdx"
printf 'stub\n' > "$b/coopdx_updater"
: > "$b/libdiscord_game_sdk.so"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        printf 'stub\n' > "$b/sm64coopdx.exe"
        printf 'stub\n' > "$b/coopdx_updater.exe"
        ;;
esac
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
         "$C/platform/portable" "$C/platform/windows"
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
# 安装向导也用真脚本 + 真 .iss：Windows CI 上 `-p all` 要真跑它，
# 其它平台上它只在 `-p setup` 用例里被「平台不对」提前挡住。
cp "$repo/platform/windows/mkexe.sh" "$repo/platform/windows/sm64coopdx-iquedx.iss" \
   "$C/platform/windows/"
chmod +x "$C/platform/DEBIAN/mkdeb.sh" "$C/platform/rpm/mkrpm.sh" \
         "$C/platform/darwin/mkmac.sh"

# 桩 ISCC（Inno Setup 编译器）。ISCC.exe 只有 Windows 上有，而 Windows 上的
# `-p all` 会真的去调它 —— 不注入桩的话自测会编译一遍真安装包（慢、还依赖
# 那台机器的 Inno 版本）。桩只记参数，并按 $FAKE_SETUP_OUT 假装产出 setup.exe。
fiscc=$C/fake-iscc
cat > "$fiscc" <<'FAKE'
#!/bin/sh
printf '%s\n' "$@" > "${ISCC_ARGS_OUT:-/dev/null}"
[ -n "${FAKE_SETUP_OUT:-}" ] && : > "$FAKE_SETUP_OUT"
exit 0
FAKE
chmod +x "$fiscc"

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
is_exe "$C/dist/sm64coopdx" && ok "成品目录：主程序落在 --dist" \
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
   && is_exe "$C/dist-po/sm64coopdx"; then
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
if [ "$po_rc" -eq 0 ] && is_exe "$C/dist-app/sm64coopdx"; then
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

if [ "$os_kind" = win ]; then
    # Windows 上 `-p all` 会真的调 ISCC ⇒ 必须注入桩，否则自测会编译一遍真安装包
    win_ver=$(sed -n 's/^[[:space:]]*version[[:space:]]*=[[:space:]]*//p' "$C/upstream.txt" \
              | head -n 1 | tr -d '\r')
    [ -n "$win_ver" ] || win_ver=1.5.1
    win_setup=$C/sm64iquedx-${win_ver}-windows-$(uname -m)-setup.exe
    out=$( ISCC="$fiscc" ISCC_ARGS_OUT="$C/iscc-all.args" FAKE_SETUP_OUT="$win_setup" \
           run_tree -po -p all --path "$C/up" )
    all_rc=$?
else
    out=$(run_tree -po -p all --path "$C/up")
    all_rc=$?
fi
case "$os_kind" in
    win)
        if [ "$all_rc" -eq 0 ] \
           && printf '%s\n' "$out" | grep -q "打包 Windows 安装向导" \
           && [ -f "$win_setup" ] \
           && grep -qF '/DMyAppVersion=' "$C/iscc-all.args" \
           && printf '%s\n' "$out" | grep -q "打包便携 zip" \
           && printf '%s\n' "$out" | grep -q "stub zip"; then
            ok "-p all（Windows）：安装向导 + 便携 zip"
        else
            no "-p all（Windows）：rc=$all_rc → $(printf '%s\n' "$out" | grep -E '打包|stub|\[x\]|ISCC|成品' | tr '\n' ' ')"
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

# C11: 便携 zip 的文件名必须带架构 —— x86_64 与 aarch64 的包同名的话，
#      下载下来根本分不清该给自己哪台机器用。
zout=$(PATH="$S/bin:$PATH" sh "$C/platform/portable/mkzip.sh" "$C/dist" 9.9.9 2>&1)
printf '%s\n' "$zout" | grep -q "9.9.9-[a-z]*-$(uname -m)-portable.zip" \
    && ok "便携 zip：文件名带架构（$(uname -m)）" \
    || no "便携 zip：文件名不对 → $(printf '%s\n' "$zout" | tail -n 1)"

# C12: aarch64 上游把主程序叫 sm64coopdx.arm，收集成品时必须改名成 sm64coopdx
#      —— 否则 platform/*/mk*.sh 一律报「找不到构建产物」，CI 上 aarch64 就是
#      这么挂的（-po 的文案显示它找到了 .arm，紧接着打包说没有 sm64coopdx）。
mkdir -p "$C/up-arm/build/us_pc/lang"
printf 'stub\n' > "$C/up-arm/build/us_pc/sm64coopdx.arm"
chmod +x "$C/up-arm/build/us_pc/sm64coopdx.arm"
out=$(run_tree -po -p no --path "$C/up-arm" --dist "$C/dist-arm")
if is_exe "$C/dist-arm/sm64coopdx" && [ ! -e "$C/dist-arm/sm64coopdx.arm" ]; then
    ok "aarch64：sm64coopdx.arm 收集时改名成 sm64coopdx"
else
    no "aarch64：改名失败 → $(ls "$C/dist-arm" 2>/dev/null | tr '\n' ' ')"
fi

# C13: add_cn_config.sh —— configfile.c 的中文配置项从「每上游一份补丁」改成了
#      插入脚本（旧的 0006 因此删掉），这里守住「插的位置对、且幂等、锚点缺失会报错」。
#
# ⚠️ 三处调用统一走一个 awk 包装壳，按 **mawk** 的规则拒绝「-v 的值含裸换行」。
# 起因：脚本原来用 `awk -v g="$GLOBALS"` 传要插入的多行文本，gawk 照收，
# Ubuntu 的 /bin/awk（mawk）直接 `awk: newline in string ...` + 退出码 2，
# 于是 CI 上「插入位置不对（rc=2）」。本机是 gawk，不带这层壳就永远测不出来。
REAL_AWK=$(command -v awk || echo /usr/bin/awk)
mkdir -p "$C/mawkish"
cat > "$C/mawkish/awk" <<'SHIM'
#!/bin/sh
prev=
for a in "$@"; do
    if [ "$prev" = v ]; then
        case $a in
            *'
'*) printf 'awk: newline in string %s at source line 1\n' "$a" >&2; exit 2 ;;
        esac
    fi
    case $a in
        -v) prev=v ;;
        -v?*)
            case $a in
                *'
'*) printf 'awk: newline in string %s at source line 1\n' "$a" >&2; exit 2 ;;
            esac
            prev= ;;
        *) prev= ;;
    esac
done
exec "$REAL_AWK" "$@"
SHIM
chmod +x "$C/mawkish/awk"
cfg_run() { REAL_AWK="$REAL_AWK" PATH="$C/mawkish:$PATH" sh "$repo/scripts/add_cn_config.sh" "$@"; }

cfg=$C/cfgprobe
mkdir -p "$cfg/src/pc"
cat > "$cfg/src/pc/configfile.c" <<'CFG'
char         configLanguage[MAX_CONFIG_STRING]    = "";
bool         configForce4By3                      = false;
static const ConfigEntry table[] = {
    {.name = "force_4by3",                     .type = CONFIG_TYPE_BOOL,   .boolValue   = &configForce4By3},
};
CFG
cp "$cfg/src/pc/configfile.c" "$C/cfgprobe.orig"

cfg_rc_a=0; cfg_run "$cfg" >/dev/null 2>&1 || cfg_rc_a=$?
if [ "$cfg_rc_a" -eq 0 ] \
   && [ "$(grep -c 'configChineseText' "$cfg/src/pc/configfile.c")" -eq 6 ] \
   && [ "$(grep -n 'configLanguage\[MAX_CONFIG_STRING\]' "$cfg/src/pc/configfile.c" | cut -d: -f1)" = 1 ] \
   && [ "$(grep -n 'configChineseText                    = true' "$cfg/src/pc/configfile.c" | cut -d: -f1)" = 2 ] \
   && [ "$(grep -n '{.name = "chinese_text"' "$cfg/src/pc/configfile.c" | cut -d: -f1)" = 7 ] \
   && [ "$(grep -n '{.name = "force_4by3"' "$cfg/src/pc/configfile.c" | cut -d: -f1)" = 10 ]; then
    ok "add_cn_config：声明插在 configLanguage 之后、表项插在 force_4by3 之前"
else
    no "add_cn_config：插入位置不对（rc=${cfg_rc_a}）→ $(sed -n '1,12p' "$cfg/src/pc/configfile.c" | tr '\n' '|')"
fi

cfg_out=$(cfg_run "$cfg" 2>&1)
printf '%s\n' "$cfg_out" | grep -q "已齐全" \
    && ok "add_cn_config：重复执行幂等" \
    || no "add_cn_config：复跑不幂等 → $cfg_out"

# 只有声明、没有配置表锚点 → 必须报错，不能瞎猜位置
mkdir -p "$C/cfgnoforce/src/pc"
printf 'char         configLanguage[MAX_CONFIG_STRING]    = "";\n' > "$C/cfgnoforce/src/pc/configfile.c"
cfg_rc_b=0
cfg_run "$C/cfgnoforce" >/dev/null 2>&1 || cfg_rc_b=$?
[ "$cfg_rc_b" -ne 0 ] \
    && ok "add_cn_config：锚点缺失时非 0 退出（不猜位置）" \
    || no "add_cn_config：锚点缺失却返回 0"

# C14: mkexe.sh（Windows 安装向导）。ISCC.exe 只有 Windows 上有，CI 的 Linux/macOS
#      job 也得能验，所以用桩脚本（${fiscc}，见上）；临时树完整复制了 repo 的形状
#      （mkexe.sh 按 `$(dirname $0)/../..` 找仓库根），setup.exe 落在临时树里，
#      不往真仓库扔垃圾。
mkroot=$C/fakerepo
mkdir -p "$mkroot/platform/windows" "$mkroot/dist"
cp "$repo/platform/windows/mkexe.sh" "$mkroot/platform/windows/"
cp "$repo/platform/windows/sm64coopdx-iquedx.iss" "$mkroot/platform/windows/"
printf 'version = 9.9.9\n' > "$mkroot/upstream.txt"
printf 'stub\n' > "$mkroot/dist/sm64coopdx.exe"

# 安装脚本的 .iss 必须带 UTF-8 BOM，否则中文界面会按 ANSI 解成乱码
if [ "$(head -c 3 "$repo/platform/windows/sm64coopdx-iquedx.iss" | od -An -tx1 | tr -d ' \n')" = "efbbbf" ]; then
    ok "安装脚本是 UTF-8 带 BOM（Inno Setup 才不会把中文当 ANSI）"
else
    no "安装脚本丢了 UTF-8 BOM，装出来的向导中文会乱码"
fi

mk_out=$mkroot/sm64iquedx-9.9.9-windows-$(uname -m)-setup.exe
mk_rc=0
ISCC="$fiscc" ISCC_ARGS_OUT="$C/iscc.args" FAKE_SETUP_OUT="$mk_out" \
    sh "$mkroot/platform/windows/mkexe.sh" "$mkroot/dist" >/dev/null 2>&1 || mk_rc=$?
if [ "$mk_rc" -eq 0 ] \
   && [ -f "$mk_out" ] \
   && grep -qxF '/DWithChinese=0' "$C/iscc.args" \
   && grep -qxF '/DMyAppVersion=9.9.9' "$C/iscc.args" \
   && grep -qF 'MySourceDir=' "$C/iscc.args" \
   && grep -qF 'sm64coopdx-iquedx.iss' "$C/iscc.args"; then
    ok "mkexe：版本从 upstream.txt 读、源目录与 iss 都传给了 ISCC"
else
    no "mkexe 参数不对（rc=${mk_rc}）→ $(tr '\n' '|' < "$C/iscc.args" 2>/dev/null)"
fi

# 没有 ISCC 必须明确报错，不能打出一个空壳安装包
mk_rc2=0
ISCC=/nonexistent/ISCC.exe sh "$mkroot/platform/windows/mkexe.sh" "$mkroot/dist" \
    >"$C/mkerr" 2>&1 || mk_rc2=$?
if [ "$mk_rc2" -ne 0 ] && grep -qF 'Inno Setup' "$C/mkerr"; then
    ok "mkexe：找不到 ISCC 时非 0 退出并提示装 Inno Setup"
else
    no "mkexe：没有 ISCC 却返回 ${mk_rc2} → $(tr '\n' '|' < "$C/mkerr")"
fi

# 成品目录里没有主程序 → 同样是「前置条件不满足」，不能当成功
mkdir -p "$C/mkempty"
mk_rc3=0
ISCC="$fiscc" ISCC_ARGS_OUT="$C/iscc3.args" FAKE_SETUP_OUT=/dev/null \
    sh "$mkroot/platform/windows/mkexe.sh" "$C/mkempty" >/dev/null 2>&1 || mk_rc3=$?
[ "$mk_rc3" -ne 0 ] \
    && ok "mkexe：成品目录没有 sm64coopdx.exe 时非 0 退出" \
    || no "mkexe：没有主程序却返回 0"

# 编译器目录里有简体中文语言文件时，应该把中文界面开关传过去
mkdir -p "$C/Languages" && : > "$C/Languages/ChineseSimplified.isl"
mk_rc4=0
ISCC="$fiscc" ISCC_ARGS_OUT="$C/iscc4.args" FAKE_SETUP_OUT="$mk_out" \
    sh "$mkroot/platform/windows/mkexe.sh" "$mkroot/dist" >/dev/null 2>&1 || mk_rc4=$?
if [ "$mk_rc4" -eq 0 ] && grep -qxF '/DWithChinese=1' "$C/iscc4.args"; then
    ok "mkexe：编译器带 ChineseSimplified.isl 时传中文界面开关"
else
    no "mkexe：没传中文界面开关（rc=${mk_rc4}）→ $(tr '\n' '|' < "$C/iscc4.args" 2>/dev/null)"
fi

# C15: `-p setup` 在非 Windows 上必须明确拒绝，别静默打出一个空壳安装包
su_rc=0
su_out=$( ISCC="$fiscc" ISCC_ARGS_OUT="$C/iscc5.args" \
          run_tree -po -p setup --path "$C/up-arm" --dist "$C/dist-setup" ) || su_rc=$?
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        # 注入桩 ISCC 后，失败点必须落在「成品里没有 sm64coopdx.exe」上，
        # 否则这条用例只是在验「这台机器没装 Inno」，不是验我们的检查。
        if [ "$su_rc" -ne 0 ] && printf '%s' "$su_out" | grep -qF 'sm64coopdx.exe'; then
            ok "setup：Windows 上成品缺 sm64coopdx.exe 时非 0 退出"
        else
            no "setup：rc=${su_rc} → $(printf '%s' "$su_out" | grep -E '\[x\]|sm64coopdx' | tr '\n' '|')"
        fi ;;
    *)
        if [ "$su_rc" -ne 0 ] && printf '%s' "$su_out" | grep -qF '只能在 Windows 上打'; then
            ok "setup：非 Windows 上明确拒绝（提示改用 -p zip）"
        else
            no "setup：非 Windows 上 rc=${su_rc} → $(printf '%s' "$su_out" | tr '\n' '|')"
        fi ;;
esac

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

# D2: awk 的 /regex/ 字面量里不能出现「方括号包斜杠」——用来表达「斜杠」的字符类。
# BSD awk（macOS 的 /usr/bin/awk 就是 one-true-awk）的正则词法器 regexpr() 是
# 逐字节扫到第一个 `/` 就收尾、**完全不识别方括号**的，于是 `...(/[/])/` 这种写法
# 会把正则从中间截断，报 `awk: nonterminated character class ...`（rc=2）。
# gawk / mawk 都会正确跳过方括号里的斜杠 ⇒ 本地永远测不出来，只能静态守。
# 要判「行首是不是斜杠」就写 substr($0, 1, 1) != "/"。
awkhit=$(cd "$repo" && grep -lF '[/]' scripts/*.sh install.sh 2>/dev/null \
            | grep -v 'test_install_args.sh' || true)
if [ -z "$awkhit" ]; then
    ok "awk 正则里没有「方括号包斜杠」（macOS 的 awk 会截断正则）"
else
    no "awk 正则里出现「方括号包斜杠」（macOS 的 awk 会当正则提前结束）：$awkhit"
fi

say_section "结果"
printf '  通过 %d，失败 %d\n' "$pass" "$fail"
[ "$fail" -eq 0 ] || exit 1
