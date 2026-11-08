<div align="center">

# sm64-iQueDeluxe

**适用于 Super Mario 64 Co-op Deluxe 的《神游马力欧64》ROM 汉化资源兼容补丁**

<img src="./screenshot/title.png" width="800"/> <br>

<img alt="sm64coopdx" src="https://img.shields.io/badge/sm64coopdx-v1.5.1-1f6febe">
<img alt="ique" src="https://img.shields.io/badge/iQue_Rom-Support-yellow">
<img alt="Desktop" src="https://img.shields.io/badge/desktop-Windows%20%7C%20macOS%20%7C%20Linux-0078d4">
<img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/toad114514/sm64iquedx">
<img alt="GitHub forks" src="https://img.shields.io/github/forks/toad114514/sm64iquedx">

</div>

## 使用
你必须拥有一个合法的《神游马力欧64》的 z64 decomp 副本，本仓库不包含 Rom。  
Super Mario 64 (Ch) (iQue) \[\!\]：`29047fba820695bd14c5bd7aa1aa4400`  
确认无误后将 ROM 放置在已经应用了 Ique Deluxe 的 sm64coopdx 程序根目录（或者放置在 User 目录也可）  
如果操作无误，那么神游汉化资源功能将自动打开

## 亮点
 - DJUI 前端（可开关）
 - 接近的一比一复刻
 - 标题画面汉化支持，加载此 Mod 可以让 Coopdx 显示标题画面
 - 可与 djui_modern 模块共存
 - 热切换，切换神游资源无需重启
 - 桌面/安卓同时支持
 - 支持生成特定系统的软件包

## 编译
```bash
git clone https://github.com/toad114514/ique-deluxe --depth 1
cd ique-deluxe

# 根据系统选择执行的文件
sh ./install.sh             # For Linux/MacOS
./install.bat               # For Windows NT (目前没有)
```

这会先打开一个交互式的 TUI 界面询问构建产物需要：
 1. 上游版本：`coop-deluxe/sm64coopdx` 或者 `Shangshanruo66958/sm64coopdx-Chinese`
 2. 是否应用 UIPatch 补丁（`patches/0002-djui-frontend.patch`），一般情况下不应用 UIPatch 补丁时只能通过修改配置文件修改，应用后可以在 设置 -> 其他 -> 神游汉化 中进行便捷修改
 3. 是否应用 djui_modern 补丁，这是由上善若制作的现代化 Djoslin's UI 模块，详细可查看 [This](https://github.com/Shangshanruo666/djui_modern)，应用后本补丁也会根据其补丁做兼容

`install.sh` 还包括一些别的问题：
 1. 询问是否构建安卓版本，勾选后将不再生成任何PC构建，只构建 Android。请注意编译本机必须安装了 Android NDK 与 Gradle 依赖才可编译
 2. 询问是否需要生成安装包，可根据平台生成对应的软件包
    - Debian 系 Linux 将生成 `sm64coopdx-iquedx_<版本>_<架构>.deb` 的软件包并使用 apt 安装
    - Arch 系 Linux 将通过 `makepkg -si` 生成 `sm64coopdx-iquedx-<版本>-<rel>-<架构>.pkg.tar.zst` 的软件包并使用 pacman 安装
      - 本来想提交到 AUR 的然后最近好像不给注册开放没办法了，目前还不能 `yay -S sm64coopdx-iquedx`
    - 红帽系 Linux 将生成 RPM 文件并使用 dnf 安装（做一半）
    - MacOS 系可以打包成 pkg 文件（没做）
    - 其他 Linux 发行版 和 Unix-Like 默认将构建产物放在 `/opt/sm64coopdx-iquedx` 下，上述所有软件包应用本体也是在这个路径，也可自定义位置

`install.bat` 还包括别的问题：
  1. 是否选择使用软件打包程序打包，这将使用一个软件打包程序来打包 exe 文件生成安装向导，默认的安装路径在 `C:\Program Files\sm64coopdx-iquedx` （没做aaa）

如果嫌麻烦，你也可以将仓库 fork 后使用 Github Action 自己编译  
或上善若的汉化群 https://qm.qq.com/q/twjhRVjgWs 来找我获取编译成品 *（为控制国内马里奥圈小鬼数量不能直接发布成品）*

## 提交 Issue
请使用 Github 的模板表单！否则任何 issue 一律忽略！

## 仓库结构

本仓库只存「改动」不存上游全量源码：

- `src/` —— 被改过的文件，**路径与上游逐级一致**（唯一编辑源）
- `patches/` —— 6 份补丁**平铺**在此，不建子目录：`0001` 主补丁、`0002` 可选前端、`0003` 语言字符串为四套上游共用，`0004` 汉化版 Android 语言、`0005`/`0006` 两个 Android 平台增量；另有 `cn_extra/` 存放上游不存在的新增文件。`lang/*.ini` 只加字符串、各上游上下文不同，已从主补丁里独立出来
- `platform/` ——  针对特殊平台的其他内容，例如 Debian 包所需的 control
- `scripts/` —— 补丁生成脚本，以及幂等应用器 `apply_patches.sh`
- `upstream.txt` —— 补丁基线（上游仓库 + commit）

补丁索引与应用顺序见 [patches/README.md](patches/README.md)；改完 `src/` 用 `scripts/gen_all_patches.sh` 重生成共用补丁、`scripts/gen_platform_patches.py` 重生成 Android 补丁。不构建、只想手动叠加补丁时用 `scripts/apply_patches.sh <pc|android|android-cn> <上游目录> [--frontend]`，重复执行安全。约定详见 [docs/LAYOUT.md](docs/LAYOUT.md)。
