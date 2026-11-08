<center>

# sm64-iQueDeluxe

**适用于 Super Mario 64 Co-op Deluxe 的《神游马力欧64》ROM 汉化资源兼容补丁**

<img src="./screenshot/title.png" width="800"/>



</center>

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
 1. 询问是否构建安卓版本，勾选后将不再生成任何PC构建，只构建 Android。请注意编译本机必须安装了 Android NDK 依赖才可编译
 2. 询问是否需要生成安装包，勾选后会根据平台生成对应的软件包
   - Debian 系 Linux 将生成 sm64coopdx-iquedx_x.x.x_x86_64.deb 的软件包并使用 apt 安装
   - Arch 系 Linux 将通过 `makepkg -si` 生成 sm64coopdx-iquedx_x.x.x_x86_64.tar.xv 的软件包并使用 pacman 安装
     - 本来想提交到 AUR 的然后最近好像不给注册开放没办法了，目前还不能 `yay -S sm64coopdx-iquedx`
   - 其他 Linux 发行版将自动把构建产物放在 `/opt/sm64coopdx-iquedx` 下，上述所有软件包应用本体也是在这个路径

你也可以将仓库 fork 后使用 Github Action 自己编译
或上善若的汉化群 https://qm.qq.com/q/twjhRVjgWs 直接获取编译成品

## 仓库结构

本仓库只存「改动」不存上游全量源码：

- `src/` —— 被改过的文件，**路径与上游逐级一致**（唯一编辑源）
- `patches/` —— `0001-src-overlay.patch` 必选主补丁、`0002-djui-frontend.patch` 可选前端补丁；`patches/cn_extra/` 存放上游不存在的新增文件（保持目录结构）
- `platform/` ——  针对特殊平台的其他内容，例如 Debian 包所需的 debinfo
- `scripts/gen_all_patches.sh` —— 改完 `src/` 后重新生成全部补丁
- `upstream.txt` —— 补丁基线（上游仓库 + commit）

约定详见 [docs/LAYOUT.md](docs/LAYOUT.md)。
