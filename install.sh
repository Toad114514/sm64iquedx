#!/bin/sh
# sm64iquedx 构建入口：克隆上游 → 打补丁 → 编译 → 打包。
#
#   sh install.sh                 # 交互式询问（需要在终端里）
#   sh install.sh -h              # 看所有参数
#   sh install.sh -f -p deb       # 带前端补丁，并生成 .deb
#   sh install.sh -a -c           # 只构建 Android 汉化版 APK
#
# 设计原则：
#   * 补丁怎么打全部交给 scripts/apply_patches.sh（幂等、顺序固定、校验基线），
#     这里只负责「准备干净的源码树 → 调脚本 → 编译 → 交给 platform/ 打包」。
#   * 所有上游地址与 commit 都从 upstream.txt 读，不在脚本里重复第二份。
#   * 严格 POSIX sh：CI 上 Ubuntu 的 /bin/sh 是 dash，bashism（[[ ]]、read -p、
#     local、数组）会直接报错。
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$repo"

# ── 默认值 ────────────────────────────────────────────────────────────────────
ASK_ANDROID=no          # yes = 只构建 Android APK，不构建桌面版
ASK_CHINESE=no          # yes = 用汉化版上游（PC: cn-repo / Android: android-cn-repo）
ASK_FRONTEND=no         # yes = 额外应用 0002（设置 → 其他 里的中文字库开关）
ASK_DJUI=no             # yes = 应用 djui_modern 模块
ASK_PACKAGE=            # 空/auto/deb/rpm/arch/mac/zip/other/no
WORK_DIR="$repo/build-iquedx"   # 上游 checkout 与中间文件（.gitignore 的 build-*/）
UPSTREAM_DIR=""                 # 为空则 = $WORK_DIR/upstream
DIST_DIR="$repo/dist"           # 成品目录，platform/* 的默认值也是这里
VERSION=""                      # 为空则读 upstream.txt 的 version
INTERACTIVE=no
GIVEN_ARGS=no
PATH_GIVEN=no           # --path 是否被显式指定（--package-only 要求显式）
PACKAGE_ONLY=no         # yes = 跳过克隆/打补丁/编译，只用已有构建产物打包
DRY_RUN=no              # yes = 只解析参数并打印结果，不克隆/不编译

# ── 输出 ──────────────────────────────────────────────────────────────────────
# 记下当前阶段：出错时 trap 能把「死在哪一步」打出来。CI 里最贵的就是
# 「exit code 1，然后什么线索都没有」——上次 aarch64 的打包步骤就是这么瞎的。
STAGE="启动"
say()  { printf '%s\n' "$*"; }
step() { STAGE="$*"; printf '\n==> %s\n' "$*"; }
info() { printf '    %s\n' "$*"; }
warn() { printf '[!] %s\n' "$*" >&2; }
die()  { printf '[x] %s\n' "$*" >&2; exit 1; }
# 255 专门表示「没有构建产物，没法打包」，CI 里据此区分「打包失败」与「没编就打包」
die255() { printf '[x] %s\n' "$*" >&2; exit 255; }

# 非 0 退出时补一行「失败于阶段」，不改变退出码。
# IQDX_DEBUG=1 时再打一份现场（CI 上「exit 1 加一句日志都没有」最难查：
# 到底是权限、磁盘满、还是文件没生成）。
trap 'iqdx_rc=$?; if [ "$iqdx_rc" -ne 0 ]; then
        printf "[x] 退出码 %s，失败于阶段「%s」\n" "$iqdx_rc" "$STAGE" >&2
        if [ -n "${IQDX_DEBUG:-}" ]; then
            { uname -a; id; } >&2 || true
            df -h . /tmp >&2 || true
            # 每条都 || true：trap 里再有命令失败会把 255 盖成 1
            { [ -n "${DIST_DIR:-}" ] && [ -d "$DIST_DIR" ] &&
                ls -l "$DIST_DIR" 2>&1 | head -n 20 >&2; } || true
            { [ -n "${UPSTREAM_DIR:-}" ] && [ -d "$UPSTREAM_DIR/build/us_pc" ] &&
                ls -l "$UPSTREAM_DIR/build/us_pc" 2>&1 | head -n 20 >&2; } || true
            true
        fi
      fi' EXIT

usage() {
    cat <<'EOF'
用法: sh install.sh [选项]

不传任何参数且处在终端里时进入交互式询问；传了参数就按参数走，不再提问
（方便 CI：`sh install.sh -f -p deb`）。

构建目标
  -a, --android          只构建 Android APK（需要 Android NDK + Gradle），
                         此模式下不做任何桌面打包
  -c, --chinese          使用汉化版上游（PC 用 cn-repo，Android 用 android-cn-repo）
  -f, --frontend         额外应用 0002 前端补丁：设置 → 其他 → 神游汉化
  -d, --djmd             应用 djui_modern 模块（在打 0001 之前）

成品打包（-a 时忽略）
  -p, --package <方式>   auto | all | deb | rpm | arch | mac | zip | other | no
                         auto 按当前系统判断；all 打本机能打的全部格式
                         zip 便携压缩包（解压即玩，任何系统都能打）
                         no 只留在 dist/；不写这个参数就不打包

  -po, --package-only    不 clone、不打补丁、不编译，只用 --path 指向的已编译
                         源码树打包（配合 -p，缺省等于 all）
                         必须显式给 --path；找不到构建产物时以 255 退出

路径与版本
  -o, --path <目录>      上游 checkout 目录，默认 <仓库>/build-iquedx/upstream
      --dist <目录>      成品目录，默认 <仓库>/dist
                         注意：arch 打包（PKGBUILD）写死了 <仓库>/dist
  -V, --version <版本>   包版本号，默认读 upstream.txt 的 version

  -h, --help             显示本帮助
  -n, --print-config     只解析参数并打印最终配置（含每个变量的取值），
                         不克隆、不编译、不打包；用来核对参数有没有传对

示例
  sh install.sh -f -p deb          # Debian 系，带前端开关
  sh install.sh -c -f -p rpm       # 汉化上游 + 前端 + rpm
  sh install.sh -d -f -p mac       # 带 djui_modern，打 .app/.dmg
  sh install.sh -a -c              # Android 汉化版 APK
  sh install.sh -po -p all --path build-iquedx/upstream
                                   # 不编译，直接用已有产物打包（CI 的第二段）
EOF
}

# ── upstream.txt：唯一的基线来源 ─────────────────────────────────────────────
upstream_value() {
    # 只匹配行首键名，避免 `repo` 命中 `cn-repo`
    sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" "$repo/upstream.txt" |
        head -n 1 | tr -d '\r'
}

# ── 小工具 ────────────────────────────────────────────────────────────────────
nproc_safe() {
    nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4
}

# clone_at <url> <commit> <目录>
# 先只拉那一个 commit；服务器不支持按 sha 浅拉时退回默认分支并提示。
clone_at() {
    c_url=$1
    c_commit=$2
    c_dir=$3

    [ -n "$c_url" ] || die "upstream.txt 里没有对应的仓库地址"
    [ -n "$c_commit" ] || die "upstream.txt 里没有对应的 commit"

    if [ -d "$c_dir/.git" ]; then
        c_head=$(git -C "$c_dir" rev-parse HEAD 2>/dev/null || true)
        if [ "$c_head" = "$c_commit" ]; then
            info "复用已有 checkout：$c_dir"
            return 0
        fi
        info "已有 checkout 的 HEAD 与基线不同，重新拉取：$c_dir"
    fi

    rm -rf "$c_dir"
    mkdir -p "$c_dir"
    git -C "$c_dir" init -q
    git -C "$c_dir" remote add origin "$c_url"

    info "拉取 $c_url @ ${c_commit%?????????}（${c_commit}）"
    if git -C "$c_dir" fetch -q --depth 1 origin "$c_commit" 2>/dev/null; then
        git -C "$c_dir" checkout -q FETCH_HEAD
    else
        warn "按 commit 浅拉失败，改为拉默认分支（补丁可能因基线漂移而失败）"
        git -C "$c_dir" fetch -q --depth 1 origin
        git -C "$c_dir" checkout -q FETCH_HEAD
    fi

    head_commit=$(git -C "$c_dir" rev-parse HEAD)
    if [ "$head_commit" != "$c_commit" ]; then
        warn "当前 HEAD ${head_commit%?????????} 与 upstream.txt 的 ${c_commit%?????????} 不同"
    fi
}

# 结构检查：只查补丁真正会碰的目录，缺一个就直接报错（避免在错误的分支上编半小时）
check_structure() {
    s_dir=$1
    missing=""
    for d in src/game src/pc src/pc/djui text lang; do
        [ -d "$s_dir/$d" ] || missing="$missing $d/"
    done
    [ -f "$s_dir/Makefile" ] || missing="$missing Makefile"

    if [ "$ASK_ANDROID" = yes ]; then
        [ -f "$s_dir/Android.mk" ] || missing="$missing Android.mk"
        [ -f "$s_dir/platform/app/build.gradle" ] || missing="$missing platform/app/build.gradle"
    fi

    if [ -n "$missing" ]; then
        die "目录结构不对，缺少：$missing
    上游可能换了布局，或者分支选错了。"
    fi
    info "目录结构检查通过（src/game、src/pc、src/pc/djui、text、lang）"
}

# ── djui_modern ──────────────────────────────────────────────────────────────
# 模块按 upstream.txt 的 djmd_commit 钉版本；用 --no-submodule + 手工放置模块，
# 因为 `git submodule add` 只能钉分支不能钉 commit，上游模块一动我们就会漂移。
apply_djui_modern() {
    d_up=$1
    d_android=$2
    # 模块仓库放在上游旁边，不污染仓库根的 build-iquedx/（--path 指到别处时也对）
    d_cache=$(dirname "$d_up")/djui_modern

    step "应用 djui_modern"
    clone_at "$DJMD_REPO" "$DJMD_COMMIT" "$d_cache"

    d_mod="$d_up/src/pc/djui_modern"
    info "放置模块到 src/pc/djui_modern"
    rm -rf "$d_mod"
    mkdir -p "$d_mod"
    # 只拷模块内容，跳过 .git：这里不需要它的历史，留着还会让上游树里多出
    # 一个嵌套仓库（git status 变成 untracked repo）。
    for entry in "$d_cache"/* "$d_cache"/.[!.]*; do
        [ -e "$entry" ] || continue
        if [ "$(basename "$entry")" = .git ]; then
            continue
        fi
        cp -R "$entry" "$d_mod/"
    done

    set -- --root "$d_up" --no-submodule
    if [ "$d_android" = yes ]; then
        set -- "$@" --android
    fi
    ( cd "$d_cache" && sh ./install.sh "$@" )
}

# ── 编译 ─────────────────────────────────────────────────────────────────────
build_pc() {
    b_up=$1
    jobs=$(nproc_safe)
    b_make=make
    if command -v gmake >/dev/null 2>&1; then
        b_make=gmake
    fi

    step "编译（$b_make -j${jobs}）"
    if [ "$(uname -s)" = Darwin ]; then
        # 走 Makefile 自带的 .app 打包路径（USE_APP 默认 1）：只有它会用
        # install_name_tool 把 SDL2 / GLEW 的 dylib 拷进 Contents/MacOS 并改写
        # 加载路径。USE_APP=0 得到的是链到 /opt/homebrew 的裸二进制，换台机器
        # 就跑不起来。collect_dist 会把 .app 摊平成 dist/。
        info "$b_make OSX_BUILD=1 -j$jobs"
        ( cd "$b_up" && $b_make OSX_BUILD=1 -j"$jobs" )
        fix_mac_bundle "$b_up"
    else
        ( cd "$b_up" && $b_make -j"$jobs" )
    fi
}

# 修 .app 里缺的 dylib。
#
# 上游 Makefile 的 app bundle 段这么找库：
#     GLEW_LIB := $(shell find $(BREW_PREFIX)/lib/ | grep libGLEW.2.2.0 | sort -n | uniq)
#     SDL2_LIB := $(shell find $(BREW_PREFIX)/lib/ | grep libSDL2-     | sort -n | uniq)
# 两个坑：
#   1. brew 的 glew 已经是 2.3.x，`grep libGLEW.2.2.0` 匹配不到 → GLEW_LIB 为空
#      → `cp  <dest>/libGLEW.dylib`（只有一个参数）报 No such file or directory；
#   2. `find | grep libSDL2-` 可能命中多个（sdl2 / sdl2-compat 的别名），
#      多个源 + 文件当目标 → `cp: <dest>/libSDL2.dylib: Not a directory`。
# 两处都不中断 make，于是 .app 少两个库，而可执行文件的 install_name 已经被
# 改成 @executable_path/libSDL2.dylib → 换台机器直接起不来。
# 这里按 brew 里的真实文件补回去（版本号会涨，所以用 glob 取最新的那个）。
fix_mac_bundle() {
    f_dir="$1/build/us_pc/sm64coopdx.app/Contents/MacOS"
    f_exe="$f_dir/sm64coopdx"
    [ -f "$f_exe" ] || return 0

    f_brew=$(brew --prefix 2>/dev/null || printf '%s' /opt/homebrew)
    f_fixed=0

    for f_spec in "libSDL2.dylib|libSDL2-2.0.0.dylib|libSDL2-*.dylib" \
                  "libGLEW.dylib|libGLEW.dylib|libGLEW*.dylib"; do
        f_dst=${f_spec%%|*}
        f_rest=${f_spec#*|}
        f_prefer=${f_rest%%|*}
        f_glob=${f_rest#*|}

        # 包里已经有了（且不是空文件）就不动
        if [ -s "$f_dir/$f_dst" ]; then
            continue
        fi

        f_src=""
        if [ -f "$f_brew/lib/$f_prefer" ]; then
            f_src="$f_brew/lib/$f_prefer"
        else
            for f_cand in "$f_brew"/lib/$f_glob; do
                [ -f "$f_cand" ] && f_src="$f_cand"
            done
        fi
        if [ -z "$f_src" ]; then
            warn "找不到 ${f_dst}（${f_brew}/lib 下没有 ${f_glob}），.app 会缺这个库"
            continue
        fi

        cp -f "$f_src" "$f_dir/$f_dst" || { warn "拷贝 $f_dst 失败"; continue; }
        chmod u+w "$f_dir/$f_dst" 2>/dev/null || true
        install_name_tool -id "@executable_path/$f_dst" "$f_dir/$f_dst" >/dev/null 2>&1 || true

        # 把可执行文件里指向这个库的所有绝对路径改成 @executable_path/
        chmod u+w "$f_exe" 2>/dev/null || true
        otool -L "$f_exe" 2>/dev/null | awk 'NR > 1 { print $1 }' | while read -r f_ref; do
            case "$f_ref" in
                @executable_path/*) continue ;;
                */libSDL2*.dylib) [ "$f_dst" = libSDL2.dylib ] || continue ;;
                */libGLEW*.dylib) [ "$f_dst" = libGLEW.dylib ] || continue ;;
                *) continue ;;
            esac
            install_name_tool -change "$f_ref" "@executable_path/$f_dst" "$f_exe" >/dev/null 2>&1 || true
        done

        codesign --force --sign - "$f_dir/$f_dst" >/dev/null 2>&1 || true
        info "补进 ${f_dst}（${f_src}）"
        f_fixed=1
    done

    if [ "$f_fixed" = 1 ]; then
        # 改过 install_name_tool 的可执行文件签名失效，必须重新 ad-hoc 签
        codesign --force --sign - "$f_exe" >/dev/null 2>&1 || true
    fi
}

build_android() {
    a_up=$1
    step "编译 Android APK"

    if [ -z "${ANDROID_HOME:-}${ANDROID_SDK_ROOT:-}" ]; then
        warn "没有 ANDROID_HOME / ANDROID_SDK_ROOT，Gradle 可能找不到 SDK"
    fi
    if [ -z "${ANDROID_NDK_HOME:-}${ANDROID_NDK_ROOT:-}" ]; then
        warn "没有 ANDROID_NDK_HOME / ANDROID_NDK_ROOT，ndk-build 可能找不到 NDK"
    fi

    # 先跑一次 make：APK 里的资源（assets/ 里的文本、贴图、着色器）由 Makefile
    # 的资源管线生成到 build/us_pc，gradle 只是把它们塞进 apk。少了这步会打出
    # 一个没有资源的空壳，或者直接因为缺 build/us_pc 而失败。
    a_jobs=$(nproc_safe)
    a_make=make
    if command -v gmake >/dev/null 2>&1; then
        a_make=gmake
    fi
    info "$a_make TOUCH_CONTROLS=1 -j${a_jobs}（生成资源）"
    ( cd "$a_up" && $a_make TOUCH_CONTROLS=1 -j"$a_jobs" )

    # Gradle 工程根目录与任务名。
    # 任务**不带** :project: 前缀：不带路径时 Gradle 会在所有含该任务的子项目里
    # 执行。上游把 Android 工程挂在 :platform:app，写死 :app:assembleRelease
    # 会直接「project 'app' not found」（CI 上就是这么挂的，42 秒）。
    a_task=assembleRelease
    a_dir=
    a_gradle=

    if [ -x "$a_up/gradlew" ]; then
        a_dir=$a_up
        a_gradle=./gradlew
    elif [ -x "$a_up/platform/gradlew" ]; then
        a_dir=$a_up/platform
        a_gradle=./gradlew
    elif [ -f "$a_up/settings.gradle" ] || [ -f "$a_up/settings.gradle.kts" ] \
         || [ -f "$a_up/build.gradle" ] || [ -f "$a_up/build.gradle.kts" ] \
         || [ -f "$a_up/platform/build.gradle" ]; then
        # 没有 wrapper：用系统 gradle。注意**根目录**才是工程根（platform/ 下面
        # 没有 settings.gradle 时，Gradle 会向上找到根的那份，rootProject 仍是
        # sm64coopdx），所以有根 settings/build 就优先在根跑。
        command -v gradle >/dev/null 2>&1 || die "需要 gradle（上游没有 wrapper）：请装 Gradle 或 Android Studio"
        a_gradle=gradle
        if [ -f "$a_up/settings.gradle" ] || [ -f "$a_up/settings.gradle.kts" ] \
           || [ -f "$a_up/build.gradle" ] || [ -f "$a_up/build.gradle.kts" ]; then
            a_dir=$a_up
        else
            a_dir=$a_up/platform
        fi
    else
        die "找不到 Gradle 工程（既没有 gradlew，也没有 settings.gradle / build.gradle）"
    fi

    info "工程目录 $a_dir"
    info "$a_gradle $a_task"
    ( cd "$a_dir" && $a_gradle "$a_task" )
}

# ── 收集成品 ─────────────────────────────────────────────────────────────────
# 文件清单照抄上游 CI 的发布包（.github/workflows/build-coop.yaml），
# 别想当然：build/us_pc 里还有一大堆 .o / assets 中间产物，不能整目录打包。
collect_dist() {
    c_up=$1
    c_build="$c_up/build/us_pc"
    [ -d "$c_build" ] || die "找不到构建目录 $c_build"

    if [ -z "$DIST_DIR" ] || [ "$DIST_DIR" = "$repo" ] || [ "$DIST_DIR" = "/" ]; then
        die "成品目录不能是 $DIST_DIR"
    fi
    rm -rf "$DIST_DIR"
    mkdir -p "$DIST_DIR"

    # macOS：Makefile 产出的是 .app（资源在 Contents/Resources，可执行文件和
    # dylib 在 Contents/MacOS）。mkmac.sh 本来就把所有东西平铺进 Contents/MacOS，
    # 所以这里把两个目录摊平合并成 dist/，既自包含又能再打成 .app。
    c_src=""
    if [ -d "$c_build/sm64coopdx.app/Contents/MacOS" ]; then
        step "收集成品到 ${DIST_DIR}（从 sm64coopdx.app 摊平）"
        c_src="$c_build/sm64coopdx.app/Contents/MacOS"
    else
        step "收集成品到 $DIST_DIR"
    fi

    if [ -n "$c_src" ]; then
        cp -R "$c_src/." "$DIST_DIR/"
        [ -d "$c_build/sm64coopdx.app/Contents/Resources" ] &&
            cp -R "$c_build/sm64coopdx.app/Contents/Resources/." "$DIST_DIR/"
        info "$(ls -1 "$DIST_DIR" | wc -l | tr -d ' ') 个顶层条目（含随包 dylib）"
    else
        # 文件清单照抄上游 CI 的发布包（.github/workflows/build-coop.yaml）。
        # 不能整目录打包：build/us_pc 里还有一大堆 .o / assets 中间产物。
        for name in dynos lang mods palettes \
                    sm64coopdx sm64coopdx.exe sm64coopdx.arm \
                    coopdx_updater coopdx_updater.exe coop.map; do
            if [ -e "$c_build/$name" ]; then
                cp -R "$c_build/$name" "$DIST_DIR/$name"
                info "收集 $name"
            fi
        done

        # 运行期依赖：libdiscord_game_sdk.so / discord_game_sdk.dll /
        # libcoopnet.dylib / libjuice.*.dylib …名字随平台变，按后缀取。
        for f in "$c_build"/*.so "$c_build"/*.so.* "$c_build"/*.dll \
                 "$c_build"/*.dylib "$c_build"/*.dylib.*; do
            [ -f "$f" ] || continue
            cp "$f" "$DIST_DIR/"
            info "收集依赖 $(basename "$f")"
        done
    fi

    # 至少得有一个主程序，否则后面打包会生成一个空壳
    if [ ! -f "$DIST_DIR/sm64coopdx" ] && [ ! -f "$DIST_DIR/sm64coopdx.exe" ] \
       && [ ! -f "$DIST_DIR/sm64coopdx.arm" ]; then
        die "成品目录里没有主程序，构建可能失败了（看上面的编译输出）"
    fi

    # aarch64 上上游 Makefile 把主程序叫 sm64coopdx.arm（TARGET_RPI / TARGET_RK3588
    # 分支：`EXE := $(BUILD_DIR)/sm64coopdx.arm`），而启动器（.desktop 的 Exec）、
    # platform/*/mk*.sh 都按 sm64coopdx 找它 ⇒ 不改名的话 aarch64 打包直接
    # 「找不到构建产物」失败。二进制叫什么都能跑，所以改名，别留 97MB 副本。
    if [ ! -f "$DIST_DIR/sm64coopdx" ] && [ -f "$DIST_DIR/sm64coopdx.arm" ]; then
        mv "$DIST_DIR/sm64coopdx.arm" "$DIST_DIR/sm64coopdx"
        info "主程序 sm64coopdx.arm → sm64coopdx（aarch64 上游命名）"
    fi
}

collect_apk() {
    a_up=$1
    step "收集 APK"
    a_list=$(find "$a_up/platform" -path '*/build/outputs/apk/*' -name '*.apk' 2>/dev/null || true)
    [ -n "$a_list" ] || die "没找到 APK，检查 Gradle 输出"
    mkdir -p "$DIST_DIR"
    printf '%s\n' "$a_list" | while IFS= read -r apk; do
        [ -n "$apk" ] || continue
        cp "$apk" "$DIST_DIR/sm64coopdx-iquedx_${VERSION}_$(basename "$apk")"
        info "$DIST_DIR/sm64coopdx-iquedx_${VERSION}_$(basename "$apk")"
    done
    say ""
    warn "release APK 默认未签名；要发布请在 Gradle 里配 signingConfig"
}

# ── 打包 ─────────────────────────────────────────────────────────────────────
detect_package() {
    case "$(uname -s)" in
        Darwin) echo mac; return ;;
        MINGW*|MSYS*|CYGWIN*) echo zip; return ;;
    esac

    p_id=""
    p_like=""
    if [ -r /etc/os-release ]; then
        p_id=$(sed -n 's/^ID=//p' /etc/os-release | tr -d '"' | head -n 1)
        p_like=$(sed -n 's/^ID_LIKE=//p' /etc/os-release | tr -d '"' | head -n 1)
    fi
    case "$p_id $p_like" in
        *debian*|*ubuntu*|*mint*|*pop*) echo deb; return ;;
        *arch*|*manjaro*|*endeavouros*) echo arch; return ;;
        *fedora*|*rhel*|*centos*|*rocky*|*almalinux*|*suse*) echo rpm; return ;;
    esac

    # 认不出来就看工具
    if command -v dpkg-deb >/dev/null 2>&1; then echo deb
    elif command -v rpmbuild >/dev/null 2>&1; then echo rpm
    elif command -v makepkg >/dev/null 2>&1; then echo arch
    else echo other
    fi
}
do_package() {
    p_way=$1
    [ "$p_way" = no ] && return 0
    [ "$p_way" = "" ] && return 0

    if [ "$p_way" = all ]; then
        package_all
        return 0
    fi

    if [ ! -d "$DIST_DIR" ]; then
        warn "成品目录不存在，跳过打包：$DIST_DIR"
        return 0
    fi

    case "$p_way" in
        deb)
            step "打包 .deb"
            info "sh platform/DEBIAN/mkdeb.sh $DIST_DIR $VERSION"
            sh "$repo/platform/DEBIAN/mkdeb.sh" "$DIST_DIR" "$VERSION"
            ;;
        rpm)
            step "打包 .rpm"
            info "sh platform/rpm/mkrpm.sh $DIST_DIR $VERSION"
            sh "$repo/platform/rpm/mkrpm.sh" "$DIST_DIR" "$VERSION"
            ;;
        arch)
            step "打包 Arch 包"
            if [ "$DIST_DIR" != "$repo/dist" ]; then
                die "PKGBUILD 写死读取 $repo/dist，--dist 指到别处时无法用 arch 打包"
            fi
            command -v makepkg >/dev/null 2>&1 || die "缺少 makepkg（pacman 包）"
            # PKGBUILD 的 pkgver 是 ${IQDX_VERSION:-默认值}，让包版本跟 upstream.txt 走
            ( cd "$repo/platform/pacman" && IQDX_VERSION="$VERSION" makepkg -f --nodeps )
            for f in "$repo"/platform/pacman/*.pkg.tar.*; do
                [ -f "$f" ] || continue
                cp "$f" "$repo/"
                info "已生成 $repo/$(basename "$f")"
            done
            ;;
        mac)
            step "打包 .app / .dmg"
            info "sh platform/darwin/mkmac.sh $DIST_DIR $VERSION"
            sh "$repo/platform/darwin/mkmac.sh" "$DIST_DIR" "$VERSION"
            ;;
        zip)
            step "打包便携 zip"
            info "sh platform/portable/mkzip.sh $DIST_DIR $VERSION"
            sh "$repo/platform/portable/mkzip.sh" "$DIST_DIR" "$VERSION"
            ;;
        other)
            step "其他发行版"
            info "成品已就绪：$DIST_DIR"
            info "装到 /opt/sm64coopdx-iquedx 并加启动器："
            info "  sudo mkdir -p /opt/sm64coopdx-iquedx && sudo cp -a $DIST_DIR/. /opt/sm64coopdx-iquedx/"
            info "  sudo sh $repo/platform/freedesktop/install.sh"
            ;;
        *)
            die "未知的打包方式：${p_way}（auto|all|deb|rpm|arch|mac|zip|other|no）"
            ;;
    esac
}

# all：本机能打几种就打几种，缺工具只警告不中断（CI 里没装 rpmbuild 也别炸）
package_all() {
    # zip 是唯一三平台通用的格式；缺工具时和其它格式一样只警告
    p_zip=no
    if command -v zip >/dev/null 2>&1; then
        p_zip=yes
    fi

    case "$(uname -s)" in
        Darwin)
            # .app/.dmg 只有 macOS 上能打（mkmac.sh 缺工具时自己会降级）
            do_package mac
            ;;
        MINGW*|MSYS*|CYGWIN*)
            # Windows 没有 deb/rpm/arch 那一套，只出便携 zip
            ;;
        *)
            if command -v dpkg-deb >/dev/null 2>&1; then
                do_package deb
            else
                warn "没有 dpkg-deb，跳过 .deb"
            fi
            if command -v rpmbuild >/dev/null 2>&1; then
                do_package rpm
            else
                warn "没有 rpmbuild，跳过 .rpm"
            fi
            if command -v makepkg >/dev/null 2>&1; then
                do_package arch
            else
                warn "没有 makepkg，跳过 Arch 包"
            fi
            ;;
    esac

    if [ "$p_zip" = yes ]; then
        do_package zip
    else
        warn "没有 zip，跳过便携压缩包"
    fi
}

# ── 交互式询问 ───────────────────────────────────────────────────────────────
ask_yes_no() {
    # ask_yes_no <提示> <默认值 y/n> → 只在 stdout 输出 y / n
    # 提示必须走 stderr：调用方是 `$(ask_yes_no ...)`，混在 stdout 会被一起捕获。
    a_prompt=$1
    a_default=$2
    while :; do
        printf '%s [%s]: ' "$a_prompt" "$a_default" >&2
        if ! IFS= read -r a_reply; then
            printf '\n' >&2
            a_reply=""
        fi
        [ -z "$a_reply" ] && a_reply=$a_default
        case "$a_reply" in
            y|Y|yes|YES) echo y; return ;;
            n|N|no|NO)   echo n; return ;;
            *) printf '请输入 y 或 n\n' >&2 ;;
        esac
    done
}

welcome_ask() {
    say ""
    say "sm64iquedx 构建向导（回车 = 默认值）"
    say ""

    if [ "$(ask_yes_no '只构建 Android 版本？(需要 NDK + Gradle)' n)" = y ]; then
        ASK_ANDROID=yes
    fi
    if [ "$(ask_yes_no '使用汉化版上游？' n)" = y ]; then
        ASK_CHINESE=yes
    fi
    if [ "$(ask_yes_no '应用前端补丁？(设置 -> 其他 -> 神游汉化)' y)" = y ]; then
        ASK_FRONTEND=yes
    fi
    if [ "$(ask_yes_no '应用 djui_modern 模块？' n)" = y ]; then
        ASK_DJUI=yes
    fi

    if [ "$ASK_ANDROID" = no ]; then
        say ""
        say "打包方式：auto（按系统）/ deb / rpm / arch / mac / other（只留 dist）/ no"
        printf '打包方式 [auto]: '
        IFS= read -r reply_pkg || reply_pkg=""
        [ -z "$reply_pkg" ] && reply_pkg=auto
        ASK_PACKAGE=$reply_pkg
    fi
    say ""
}

# ── 参数解析 ─────────────────────────────────────────────────────────────────
# need_value <选项名> <当前 $#> <下一个参数>
# 两个判据都要有：
#   * 没有下一个参数（`-p` 结尾）
#   * 下一个参数长得像选项（`--version -n`）——否则 "-n" 会被当版本号吞掉，
#     脚本于是带着 DRY_RUN=no 一路跑去 clone 上游，排查时极难看出原因。
have_value() {
    [ "$2" -ge 2 ] || die "$1 需要一个值"
    case "$3" in
        -?*) die "$1 需要一个值（后面跟的是选项 $3）" ;;
    esac
}

while [ $# -gt 0 ]; do
    GIVEN_ARGS=yes
    case "$1" in
        -a|--android)  ASK_ANDROID=yes; shift ;;
        -c|--chinese)  ASK_CHINESE=yes; shift ;;
        -f|--frontend) ASK_FRONTEND=yes; shift ;;
        -d|--djmd|--djui-modern) ASK_DJUI=yes; shift ;;
        -p|--package)
            have_value "$1" $# "${2:-}"
            ASK_PACKAGE=$2
            shift 2
            ;;
        -o|--path|--work)
            have_value "$1" $# "${2:-}"
            UPSTREAM_DIR=$2
            PATH_GIVEN=yes
            shift 2
            ;;
        --dist)
            have_value "$1" $# "${2:-}"
            DIST_DIR=$2
            shift 2
            ;;
        -V|--version)
            have_value "$1" $# "${2:-}"
            VERSION=$2
            shift 2
            ;;
        -po|--package-only) PACKAGE_ONLY=yes; shift ;;
        -n|--print-config|--dry-run) DRY_RUN=yes; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "未知参数：$1（-h 看用法）" ;;
    esac
done

# ── 准备 ─────────────────────────────────────────────────────────────────────
# -po 只打包：不 clone、不打补丁，压根用不到 git。容器镜像里（archlinux 之类）
# 常常只装了 base-devel 而没有 git，不该因此拒绝打包。
if [ "$PACKAGE_ONLY" != yes ]; then
    command -v git >/dev/null 2>&1 || die "需要 git"
fi

DJMD_REPO=$(upstream_value djmd_repo)
DJMD_COMMIT=$(upstream_value djmd_commit)

if [ -z "$VERSION" ]; then
    VERSION=$(upstream_value version)
    [ -n "$VERSION" ] || VERSION=1.5.1
fi

case "$ASK_PACKAGE" in
    ""|auto|all|deb|rpm|arch|mac|zip|other|no) ;;
    *) die "--package 只接受 auto|all|deb|rpm|arch|mac|zip|other|no，收到：$ASK_PACKAGE" ;;
esac

# 没有参数：有终端就进向导，没有终端（CI）就按默认值直走
if [ "$GIVEN_ARGS" = no ] && [ -t 0 ]; then
    INTERACTIVE=yes
    welcome_ask
fi

if [ "$ASK_ANDROID" = yes ]; then
    # 选了 Android 就不该再打包桌面格式
    if [ -n "$ASK_PACKAGE" ] && [ "$ASK_PACKAGE" != no ] && [ "$ASK_PACKAGE" != auto ]; then
        warn "Android 构建不生成桌面包，忽略 --package $ASK_PACKAGE"
    fi
    ASK_PACKAGE=no
fi

if [ -n "$UPSTREAM_DIR" ]; then
    case "$UPSTREAM_DIR" in
        /*) : ;;
        *) UPSTREAM_DIR="$repo/$UPSTREAM_DIR" ;;
    esac
else
    UPSTREAM_DIR="$WORK_DIR/upstream"
fi
case "$DIST_DIR" in
    /*) : ;;
    *) DIST_DIR="$repo/$DIST_DIR" ;;
esac
# --print-config 只是核对参数，--package-only 只看已有产物，都不该建目录
if [ "$DRY_RUN" = no ] && [ "$PACKAGE_ONLY" = no ]; then
    mkdir -p "$(dirname "$UPSTREAM_DIR")"
fi

# ── 选上游 ───────────────────────────────────────────────────────────────────
if [ "$ASK_ANDROID" = yes ]; then
    if [ "$ASK_CHINESE" = yes ]; then
        UP_URL=$(upstream_value android-cn-repo)
        UP_COMMIT=$(upstream_value android-cn-commit)
        TARGET=android-cn
    else
        UP_URL=$(upstream_value android)
        UP_COMMIT=$(upstream_value android-commit)
        TARGET=android
    fi
else
    if [ "$ASK_CHINESE" = yes ]; then
        UP_URL=$(upstream_value cn-repo)
        UP_COMMIT=$(upstream_value cn-commit)
    else
        UP_URL=$(upstream_value repo)
        UP_COMMIT=$(upstream_value commit)
    fi
    TARGET=pc
fi

say ""
say "上游      : $UP_URL"
say "commit    : $UP_COMMIT"
say "补丁目标  : $TARGET"
say "前端补丁  : $ASK_FRONTEND"
say "djui_modern: $ASK_DJUI"
say "源码目录  : $UPSTREAM_DIR"
say "成品目录  : $DIST_DIR"
say "版本号    : $VERSION"
if [ "$ASK_ANDROID" = yes ]; then
    say "打包      : （Android 不打包桌面格式）"
else
    say "打包      : ${ASK_PACKAGE:-不打包}"
fi

# ── 参数自检（--print-config）────────────────────────────────────────────────
# 只打印解析结果就退出：用来确认每个开关/路径/版本号真的落到了对应变量上。
if [ "$DRY_RUN" = yes ]; then
    say ""
    say "--- 解析结果（key=value）---"
    say "TARGET=$TARGET"
    say "PACKAGE_ONLY=$PACKAGE_ONLY"
    say "ASK_ANDROID=$ASK_ANDROID"
    say "ASK_CHINESE=$ASK_CHINESE"
    say "ASK_FRONTEND=$ASK_FRONTEND"
    say "ASK_DJUI=$ASK_DJUI"
    say "ASK_PACKAGE=${ASK_PACKAGE:-}"
    say "VERSION=$VERSION"
    say "UPSTREAM_DIR=$UPSTREAM_DIR"
    say "DIST_DIR=$DIST_DIR"
    say "UP_URL=$UP_URL"
    say "UP_COMMIT=$UP_COMMIT"
    say "DJMD_REPO=$DJMD_REPO"
    say "DJMD_COMMIT=$DJMD_COMMIT"
    say "INTERACTIVE=$INTERACTIVE"
    say "--- 结束 ---"
    exit 0
fi

# ── 0. 只打包（--package-only）───────────────────────────────────────────────
# 跳过克隆 / 打补丁 / 编译，直接用 --path 指向的已编译源码树收集并打包。
# 用途：CI 里「编译」和「打包」是两个 job，第二个 job 只有第一个 job 的产物。
# 失败一律 exit 255，与「打包过程出错」（1）区分开。
if [ "$PACKAGE_ONLY" = yes ]; then
    step "只打包（跳过克隆 / 打补丁 / 编译）"

    if [ "$PATH_GIVEN" = no ]; then
        die255 "构建产物路径未指定：--package-only 必须显式给出 --path <已编译的源码树>"
    fi
    [ -d "$UPSTREAM_DIR" ] || die255 "构建产物路径不存在：$UPSTREAM_DIR"

    if [ "$ASK_ANDROID" = yes ]; then
        p_apk=$(find "$UPSTREAM_DIR" -path '*/build/outputs/apk/*' -name '*.apk' 2>/dev/null | head -n 1)
        [ -n "$p_apk" ] || die255 "还没开始编译：$UPSTREAM_DIR 下找不到 APK（先跑一次 -a 编出来）"
        collect_apk "$UPSTREAM_DIR"
        say ""
        say "完成。成品目录：$DIST_DIR"
        exit 0
    fi

    p_bin=""
    for p_name in sm64coopdx sm64coopdx.exe sm64coopdx.arm; do
        if [ -f "$UPSTREAM_DIR/build/us_pc/$p_name" ]; then
            p_bin="$UPSTREAM_DIR/build/us_pc/$p_name"
            break
        fi
    done
    # macOS 上 make 会把可执行文件 mv 进 .app（然后连 .app 一起搬回
    # build/us_pc/），所以 build/us_pc/sm64coopdx 根本不存在。
    p_app="$UPSTREAM_DIR/build/us_pc/sm64coopdx.app/Contents/MacOS/sm64coopdx"
    if [ -z "$p_bin" ] && [ -f "$p_app" ]; then
        p_bin="$p_app"
    fi
    [ -n "$p_bin" ] || die255 "还没开始 make：找不到 $UPSTREAM_DIR/build/us_pc/sm64coopdx（或 sm64coopdx.app）（先编译，或检查 --path）"

    info "构建产物：$p_bin"
    collect_dist "$UPSTREAM_DIR"

    if [ -z "$ASK_PACKAGE" ]; then
        ASK_PACKAGE=all
        info "未指定 --package，默认打包本机可打的全部格式"
    fi
    if [ "$ASK_PACKAGE" = auto ]; then
        ASK_PACKAGE=$(detect_package)
        info "按当前系统判定为：$ASK_PACKAGE"
    fi
    step "打包"
    do_package "$ASK_PACKAGE"

    say ""
    say "完成。成品目录：$DIST_DIR"
    exit 0
fi

# ── 1. 克隆 ──────────────────────────────────────────────────────────────────
step "1/6 克隆上游"
clone_at "$UP_URL" "$UP_COMMIT" "$UPSTREAM_DIR"

# ── 2. 结构检查 ──────────────────────────────────────────────────────────────
step "2/6 检查目录结构"
check_structure "$UPSTREAM_DIR"

# ── 3. djui_modern（必须在 0001 之前）────────────────────────────────────────
if [ "$ASK_DJUI" = yes ]; then
    step "3/6 djui_modern"
    apply_djui_modern "$UPSTREAM_DIR" "$ASK_ANDROID"
else
    step "3/6 djui_modern"
    info "跳过（--djmd 未启用）"
fi

# ── 4. 本体补丁 ──────────────────────────────────────────────────────────────
# 顺序与幂等性都由 apply_patches.sh 固化：
#   0001 → 平台增量(0005/0006) → [0002] → cn_extra 补缺 → 语言字符串
step "4/6 应用补丁"
if [ "$ASK_FRONTEND" = yes ]; then
    sh "$repo/scripts/apply_patches.sh" "$TARGET" "$UPSTREAM_DIR" --frontend
else
    sh "$repo/scripts/apply_patches.sh" "$TARGET" "$UPSTREAM_DIR"
fi

# ── 5. 编译 ──────────────────────────────────────────────────────────────────
step "5/6 编译"
if [ "$ASK_ANDROID" = yes ]; then
    build_android "$UPSTREAM_DIR"
    collect_apk "$UPSTREAM_DIR"
    step "6/6 打包"
    info "Android 构建跳过桌面打包"
    say ""
    say "完成：$DIST_DIR"
    exit 0
fi

build_pc "$UPSTREAM_DIR"
collect_dist "$UPSTREAM_DIR"

# ── 6. 打包 ──────────────────────────────────────────────────────────────────
step "6/6 打包"
if [ -z "$ASK_PACKAGE" ]; then
    info "未指定 --package，成品留在 $DIST_DIR"
else
    if [ "$ASK_PACKAGE" = auto ]; then
        ASK_PACKAGE=$(detect_package)
        info "按当前系统判定为：$ASK_PACKAGE"
    fi
    do_package "$ASK_PACKAGE"
fi

say ""
say "完成。成品目录：$DIST_DIR"
say "运行前把神游 ROM（md5 29047fba820695bd14c5bd7aa1aa4400）放到主程序同级目录或 User 目录。"
