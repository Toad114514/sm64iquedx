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

## 特性
 - 简易的 djui 前端开关
 - 接近的一比一复刻
 - 标题画面汉化支持，加载此 Mod 可以让 Coopdx 显示标题画面
 - 可与 djui_modern 模块共存
 - 热切换，切换神游资源无需重启
 - 桌面/安卓 多平台支持
 - 支持生成特定系统的软件包

## 编译

```bash
git clone https://github.com/toad114514/sm64iquedx --depth 1
cd sm64iquedx

# 不带参数 = 交互式向导（需要终端）
sh ./install.sh

# 也可以直接用参数，不再提问
sh ./install.sh -h                        # 看全部选项
sh ./install.sh -f -p deb                 # 带前端开关，生成 .deb
sh ./install.sh -c -f -p rpm              # 汉化上游 + 前端 + .rpm
sh ./install.sh -d -f -p mac              # 带 djui_modern，出 .app / .dmg
sh ./install.sh -a -c                     # 只构建 Android 汉化版 APK
```

可用的所有参数：

| 参数 | 作用 |
| --- | --- |
| `-a, --android` | 只构建 Android APK（需要 NDK + Gradle），优先级高于 `--package` |
| `-c, --chinese` | 使用 `上善若和梅塔的长名字` 制作的汉化版上游仓库 `sm64coopdx-Chinese` 和 `sm64coopdx-Chinese-Android` |
| `-f, --frontend` | 应用 `patches/0002-djui-frontend.patch`，在 设置 → 其他 里加开关；不应用时只能改配置文件 |
| `-d, --djmd` | 应用 [djui_modern](https://github.com/Shangshanruo666/djui_modern) 模块 |
| `-p, --package <方式>` | `auto`（按系统）/ `all`（本机能打的全部）/ `deb` / `rpm` / `arch` / `mac` / `other` / `no`；不写就不打包 |
| `-po, --package-only` | 不克隆/不打补丁/不编译，只用 `--path` 指向的已编译树打包；缺产物时 exit 255 |
| `-o, --path <目录>` | 上游 checkout 位置，默认 `build-iquedx/upstream` |
| `--dist <目录>` | 成品目录，默认 `dist/`（arch 打包写死读 `dist/`） |
| `-V, --version <版本>` | 包版本号，默认读 `upstream.txt` 的 `version` |
| `-n, --print-config` | 只解析参数并打印最终配置，不克隆/不编译；核对参数是否传对 |

打包：

- Debian 系 → `sm64coopdx-iquedx_<版本>_<架构>.deb`（`dpkg -i`）
- Arch 系 → `platform/pacman/` 下 `makepkg -f --nodeps` 出 `sm64coopdx-iquedx-<版本>-<rel>-<架构>.pkg.tar.zst`
  - 本来想提交到 AUR 的，最近不给注册开放，所以目前还不能 `yay -S sm64coopdx-iquedx`
- 红帽系 / openSUSE → `sm64coopdx-iquedx-<版本>-<rel>.<架构>.rpm`（`dnf` / `zypper`）
- macOS → `SM64CoopDeluxe.app`，默认再包一层 `.dmg`（`--pkg` 出 `.pkg`、`--zip` 出 zip、`--app-only` 只出 app）
- 其他 Linux / Unix → `--package other` 只提示安装命令，本体一律装在 `/opt/sm64coopdx-iquedx`

Windows 目前只保证 `make` 能出 `sm64coopdx.exe`（MSYS2/MinGW），打包向导（`install.bat`）还没做。

### 只打包（`-po`）

编译和打包可以拆成两次执行——CI 里就是两个 job，第二个 job 只有第一个的产物：

```bash
sh install.sh -c -f -d -p no                                # 只编译
sh install.sh -po -p all --path build-iquedx/upstream        # 只打包
```

`-po/--package-only` 跳过克隆、打补丁、编译，直接用 `--path` 指向的已编译源码树收集成品并打包：

- **必须显式给 `--path`**（不接受默认值猜测）；
- 不给 `-p` 时默认等于 `all`（本机能打的全部格式，缺工具只警告不跳过流程）；
- 找不到构建产物时**以 `255` 退出**，并区分两种情况：「构建产物路径未指定」/「还没开始 make」。

### GitHub Actions

- `.github/workflows/build-desktop.yml` —— Linux / Windows / macOS 三平台，默认参数 `-c -f -d`。Linux 编译完后再用 `-po -p all` 出 `.deb` / `.rpm`（没有 `makepkg` 就跳过 Arch 包），产物按平台上传为 artifact。
- `.github/workflows/build-android.yml` —— 默认参数 `-a -c -f -d`，装 SDK 33 + build-tools 33.0.2，跑完 `make TOUCH_CONTROLS=1` 再 `gradlew assembleRelease`，用上游自带证书签名后上传 APK。

两个 workflow 都会先跑 `scripts/test_install_args.sh`(45 项，离线) 当门禁。

也可以 fork 后用 GitHub Actions 自己编译，或到上善若的汉化群 https://qm.qq.com/q/twjhRVjgWs 找我要成品 *（为控制国内马里奥圈小鬼数量不直接发布）*。

## 提交 Issue
请使用 Github 的模板表单！否则任何 issue 一律忽略！

## 仓库结构

本仓库只存「改动」不存上游全量源码：

- `src/` —— 被改过的文件，**路径与上游逐级一致**（唯一编辑源）
- `patches/` —— 4 份补丁**平铺**在此，不建子目录：`0001` 主补丁、`0002` 可选前端为四套上游共用，`0005`/`0006` 是两个 Android 平台增量；另有 `cn_extra/` 存放上游不存在的新增文件（只补缺、不覆盖）。语言字符串不是补丁，由 `scripts/add_lang_strings.sh` 在应用时按键插入
- `platform/` ——  针对特殊平台的其他内容，例如 Debian 包所需的 control
- `scripts/` —— 补丁生成脚本、语言字符串插入脚本 `add_lang_strings.sh`，以及幂等应用器 `apply_patches.sh`
- `upstream.txt` —— 补丁基线（上游仓库 + commit）

补丁索引与应用顺序见 [patches/README.md](patches/README.md)；改完 `src/` 用 `scripts/gen_all_patches.sh` 重生成共用补丁、`scripts/gen_platform_patches.py` 重生成 Android 补丁。不构建、只想手动叠加补丁时用 `scripts/apply_patches.sh <pc|android|android-cn> <上游目录> [--frontend]`，重复执行安全。约定详见 [docs/LAYOUT.md](docs/LAYOUT.md)。
