#!/bin/sh

# tools/传参 不传参则使用 read 询问

welcome_ask() {
    read -p "是否只编译 Android 版本？(需要 Android NDK/Gradle 依赖) [y/n]：" ASK_ANDROID # y/n
    read -p "使用汉化版的上游？(Shangshanruo666/sm64coopdx-Chinese) [y/n]：" ASK_CHINESE # y/n
    read -p "是否需要 Djui 配置前端？(应用后可以在 设置 -> 更多 -> 神游汉化 找到)：" ASK_FRONTEND 
    read -p "是否应用 djui_modern 补丁？[y/n]：" ASK_DJUI # y/n

    if [[ $ASK_ANDROID = "n" ]]; then
        read -p "要构建软件包吗？[auto/deb/arch]：" ASK_PACKAGE
    fi
}

# djui modern Patch
djui_modern_apply() {
    git clone https://github.com/Shangshanruo666/djui_modern --depth 1 djui_modern
    FLAG="--root upstream"
    if [[ $ASK_ANDROID = "y" ]]; then
        FLAG="--android --root upstream"
    fi

    echo "[!] 应用 djui_modern 补丁..."
    sh ./djui_modern/install $FLAG
}

# 本体 Patch
ique_patch_apply() {
    
}

## 两个不同平台的初处理
android_comp() {
    UPSTREAM="https://github.com/ManisCat2/sm64coopdx"
    if [[ $ASK_CHINESE = "y" ]]; then
        UPSTREAM="https://github.com/Shangshanruo666/sm64coopdx-Chinese"
    fi

    echo "[!] Clone Repo"
    git clone "$UPSTREAM" --depth 1 upstream

    if [[ $ASK_DJUI = "y" ]]; then
        djui_modern_apply
    fi

    ique_patch_apply
}

pc_comp() {
    UPSTREAM="https://github.com/coop-deluxe/sm64coopdx"
    if [[ $ASK_CHINESE = "y" ]]; then
        UPSTREAM="https://github.com/Shangshanruo666/sm64coopdx-Chinese-Android"
    fi

    echo "[!] Clone Repo"
    git clone "$UPSTREAM" --depth 1 upstream

    if [[ $ASK_DJUI = "y" ]]; then
        djui_modern_apply
    fi

    ique_patch_apply
}

## 主要部分
start_comp() {
    if [[ $ASK_ANDROID = "y" ]]; then
        android_comp
        exit 0
    fi
    pc_comp
}

usage() {

}

main() {
    ASK_ANDROID=""
    ASK_CHINESE=""
    ASK_DJUI=""
    ASK_FRONTEND=""
    ASK_PACKAGE=""

    while [ $# -gt 0 ]; do
        case "$1" in
            --android|-a) ASK_ANDROID="y"; shift;;
            --djmd|-d) ASK_DJUI="y"; shift;;
            --frontend|-f) ASK_FRONTEND="y"; shift;;
            --package|-p) ASK_PACKAGE="$2"; shift;;
            --chinese|-c) ASK_CHINESE="y"; shift;;
            --help|-h) usage; exit 0;;
            -*)
                echo "未知的参数：$1 (-h)"
                exit 2
                ;;
            *) welcome_ask;;
        esac
    done

    start_comp
}

main