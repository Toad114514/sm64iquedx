# 仓库布局

本仓库只存放「源码改动 + 补丁 + 平台差异」，不存放上游 sm64coopdx 全量源码、也不存放任何 ROM。
构建时由 `install.sh` 克隆上游源码，再把这里的改动叠加进去。

```
src/                  被修改过的文件，完整副本，路径与上游**逐级一致**（唯一编辑源）
patches/NNNN-*.patch  **全部平铺在 patches/ 根目录，不建子目录**：
                      0001 共用主补丁 / 0002 共用可选前端
                      0005 一份 Android 平台增量（两个 Android 目标共用）
                      （语言字符串/配置项不是补丁，见 scripts/add_{lang_strings,cn_config}.sh）
patches/cn_extra/     上游根本不存在的新增文件，完整副本，路径与上游逐级一致
platform/             特定平台/发行包的补丁与构建脚本
scripts/              生成补丁的辅助脚本 + 参数自测
.github/workflows/    CI：桌面三平台 / Android 两条流水线
docs/                 设计说明
upstream.txt          补丁基于的上游仓库与 commit（唯一权威基线）
```

## 判据：改动 vs 新增

一个文件属于「改动」还是「新增」，只能拿基线判定，不能看本地 `git status`：

```bash
git cat-file -e <基线commit>:<相对路径>     # 存在 ⇒ 改动(src/)；不存在 ⇒ 新增(cn_extra/)
```

本地 fork 里某个文件显示为 `M`，不代表上游有它；反过来，上游有、汉化版上游没有的情况也要按所选基线分别确认。
`upstream.txt` 里声明了 `repo` / `cn-repo` / `android` / `android-cn-repo` 四套基线。

## src/

镜像上游目录结构，只放**被改过的文件**，没动过的文件不要放。

```
sm64iquedx/src/src/game/print.c          ← 对应上游 src/game/print.c
sm64iquedx/src/src/menu/intro_geo.c      ← 对应上游 src/menu/intro_geo.c
sm64iquedx/src/Makefile                  ← 对应上游根目录 Makefile
```

规则：

1. **路径即目标路径**。`src/` 下的相对路径直接拼到上游根目录，不加任何中间层。
2. **整文件保存**。存的是改完之后的完整文件，不是片段；因此在编辑器里打开就能读、能编译。
3. **只放改过的**。判断标准是「与基线上游逐字节相同否」，相同的一律不入库。
4. **这里是唯一编辑源**。改代码先改 `src/`，`patches/` 由脚本生成，不要手改补丁。
5. **新增文件不进 `src/`**，放 `patches/cn_extra/`。

## patches/cn_extra/

上游不存在的文件以完整副本存放，内部路径仍然镜像上游根目录：

```
patches/cn_extra/src/game/cn_font.c       → 上游 src/game/cn_font.c
patches/cn_extra/src/game/cn_tex.c        → 上游 src/game/cn_tex.c
patches/cn_extra/text/cn_dialogs_cn.inc.c → 上游 text/cn_dialogs_cn.inc.c
patches/cn_extra/lang/Chinese.ini         → 上游 lang/Chinese.ini（PC 上游没有中文）
```

- 不打成 `.patch`：新增文件通常是大体积字形表 / 生成产物，做文本 diff 没有意义，`git apply` 也容易因上下文漂移失败。
- 里面既有手写源码，也有**工具生成产物**（`cn_data/*.h`、`cn_intro_data.c`、`text/cn_*.inc.c`）。生成产物改生成器后要重新同步，别手改。
- 应用方式是**逐文件补缺，不覆盖**：目标已存在就跳过（`apply_patches.sh` 里的 `copy_extras()`）。因为汉化版 Android 自带 `lang/Chinese.ini`，整目录 `cp -R` 会用 PC 那份把它顶掉。

## patches/NNNN-*.patch

- 命名：`NNNN-短横线描述.patch`，四位数递增。**所有补丁平铺在 `patches/` 根目录**，不要为某个平台建子目录。
- 一律 `git diff` 格式、相对上游根目录的路径（`a/src/...`）。
- 必须能用 `patch -p1` 或 `git apply -p1` 在 `upstream.txt` 指定的基线上干净应用。
- `0001`/`0002` **由 `scripts/gen_all_patches.sh` 从 `src/` 一次生成**；`0005` 由 `scripts/gen_platform_patches.py` 生成。都不手工编写、不手工改。
- 应用前先 `git apply --check`；选了与声明不同的基线时可以跳过补丁、直接用 `src/` 覆盖终态。
- **只有一份主补丁、一份平台补丁**：PC 与两个 Android 上游共用 `0001`/`0002`，两个 Android 目标共用 `0005`。不要为某个上游复制一整套，也不要再做一个「每上游一份」的平台补丁——上下文漂移改用插入脚本消解，见下一节。

## 语言字符串：脚本插入，不做补丁

`lang/*.ini` 只需要加两条字符串（`CHINESE_TEXT_PACK` 与语言列表里的 `Chinese`），但每套上游的 ini 行号与自带条目都不同（汉化版 Android 已自带 `Chinese = ...`）。给每个目标各生成一份 ini 补丁，补丁数量会随上游变多而膨胀，上游动一行还得全部重生成。

所以：**整个 `lang/` 前缀从 `0001` 里排除**（由生成器筛掉，不靠应用器 `--exclude`），字符串由 `scripts/add_lang_strings.sh` 在应用阶段按键插入。

```bash
scripts/add_lang_strings.sh <上游根目录> [--check]
```

| 行为 | 说明 |
| --- | --- |
| 键已存在 | 跳过（按 `^KEY[[:space:]]*=` 判断） |
| 锚点存在 | 插在锚点行之后，段归属天然正确 |
| 锚点缺失 | 报错退出，不猜位置 |
| 处理范围 | 只处理 `lang/English.ini` 与 `lang/Chinese.ini`，其它语言文件原样保留 |

改文案只改脚本里的两处常量，任何补丁都不用重生成。`--check` 只报告不写盘；对 PC 基线跑一次的结果与 `src/lang/English.ini` 逐字节一致。

## 平台增量补丁：0005（两个 Android 目标共用）

一份补丁，3 个文件（`Android.mk`、`Makefile`、`src/pc/pc_main.c`），只负责共用主补丁覆盖不到的部分：

1. **两个上下文漂移文件**：`Makefile`、`src/pc/pc_main.c`。应用 `0001` 时用 `--exclude` 跳过它们，由平台补丁给出三方合并后的终态。
2. **`Android.mk` 的宏注入**：ndk-build 不读 Makefile，必须往 `LOCAL_CFLAGS` 加 `-DSM64COOPDX_CN_TEXT=1`，否则汉化整块被编译掉。

### 第三个漂移文件 `configfile.c` 为什么不进补丁

它同样被 `--exclude`，但终态由 `scripts/add_cn_config.sh` 插入那 6 行（3 个 `configChineseText*` 声明 + 3 条 `chinese_text*` 配置表条目）。原因：原版 Android 上游与汉化版 Android 上游在这里差了半代（`configSnapTouch` vs `configElementSnap`），做补丁就得每个上游一份，而两份的差别只有上下文、加进去的行完全一样。以前确实这么干过（`0005` / `0006`），逐行比只有 24 行差异，全是这种上下文漂移——维护成本全在「上游一动就重生成两份」。

插入脚本是幂等的（按锚点插、已存在则跳过、`--check` 只报告），三棵树实测逐字节一致：`android` 上游 + 脚本 == 旧 `0005` 终态、`android-cn` 上游 + 脚本 == 旧 `0006` 终态、PC 基线 + 脚本 == `src/src/pc/configfile.c`。

`gen_platform_patches.py` 会**逐文件比对两个上游算出的平台增量**，一旦不同就直接报错——那说明两个上游真的分道扬镳，该把那个文件拆出去做每目标一份，而不是让共用补丁静默只对其中一个有效。

排除列表只剩这 3 个文件，同时存在于 `scripts/gen_platform_patches.py` 的 `EXCLUDED` 和 `scripts/apply_patches.sh`，生成脚本会断言两者一致——**改一处必须改另一处**。

应用统一走 `scripts/apply_patches.sh`（幂等），别手写 `--exclude`：

```bash
scripts/apply_patches.sh <pc|android|android-cn> <上游干净 checkout> [--frontend]
```

它不是 `install.sh`，不会构建或打包，只负责把补丁叠加到上游树上；`install.sh` 之后可以直接调用它。

## 可选补丁：0002-djui-frontend.patch

只改 `src/pc/djui/djui_panel_misc.c`，在「设置 → 其他」里加一个「中文字库（神游）」复选框，运行时切换并持久化。
**不应用它时功能照常可用，只是要改 `config.ini` 里的 `chinese_text` / `chinese_text_prompted` 来切换**（README 中的 UIPatch 就是它）。

拆分边界是一条硬约束，别顺手扩大：

- `src/pc/djui/djui_panel_confirm.[ch]` 留在 0001。它给原版面板加了一个 `djui_panel_confirm_create_ex()`（"否"按钮也能挂回调）。**注意**：首启确认框已经改成「检测到神游 ROM 就自动开启」（见 `src/pc/pc_main.c`），所以 `_ex` 目前**没有任何调用点**；保留它只为以后要恢复确认框时不用再改一遍。将来确定不要了，把这两个文件的改动一起删掉即可，不影响其它部分。
- `lang/*.ini` 的字符串改动既不在 `0001` 里、也不做成补丁，由 `scripts/add_lang_strings.sh` 插入（见上节）。`CHINESE_TEXT_PACK` 即使不打 `0002` 也无害（只是多一个被 DLANG 引用的条目）。
- 0002 对干净基线也能独立 `git apply -p1`（0001 不碰这个文件），没有顺序耦合。

对应 `install.sh` 的 `--frontend`：选中才应用 0002。既然 0002 可选，**就不能用 `cp -R src/.` 覆盖终态**（那会无条件带上前端），要么只覆盖 0001 覆盖的文件，要么用「应用 0001 → 按需应用 0002 → 覆盖 cn_extra」的顺序。

## scripts/

| 脚本 | 作用 |
| --- | --- |
| `apply_patches.sh <目标> <上游根目录> [--frontend]` | **应用器**，按目标选补丁、按序叠加、补缺拷贝 `cn_extra/`、插语言字符串，幂等；平时用它，别手写 `git apply --exclude` |
| `add_lang_strings.sh <上游根目录> [--check]` | 往 `lang/*.ini` 按键插入「中文字库」开关的两条字符串，四套上游共用，幂等 |
| `add_cn_config.sh <上游根目录> [--check]` | 往 `src/pc/configfile.c` 插入 3 个 `configChineseText*` 声明和 3 条配置表条目（`--exclude` 掉的第三个漂移文件），两个 Android 上游共用，幂等 |
| `gen_all_patches.sh <上游基线根目录>` | **改完 `src/` 后跑这个**，一次生成共用的 0001 / 0002 并断言分工 |
| `gen_platform_patches.py --pc-base … --android … --android-cn …` | 重生成 0005（两个上游共用一份），内含三方合并、冲突消解、两上游增量一致性断言与终态自检 |
| `gen_patches.sh <上游根目录> [输出] [--only 前缀] [--exclude 前缀]` | 单个补丁生成器，按相对路径前缀筛选；`--only`/`--exclude` 可各重复多次（取并集） |
| `test_install_args.sh` | `install.sh` 的参数自测：解析、错误路径、以及参数是否真的传到下游（桩掉 git/make/gradle，离线、不编译） |

`gen_patches.sh` 的行为约定（已覆盖测试）：内容相同的文件跳过；`src/` 里有而基线上没有的文件直接报错（应改放 `patches/cn_extra/`）；含 NUL 字节的二进制直接报错（`diff` 对二进制只回一句「文件不同」且退出码仍为 1，不自己判会把这句话写进补丁）；筛不出差异时写一个空补丁文件而不是保留旧文件，应用器遇到空补丁会跳过。

## platform/

按平台 / 发行包分目录，放与具体构建环境相关的补丁或脚本。

> `install.sh` 在第 6 步按 `--package`（或向导里的选择）调用这里：`deb` → `DEBIAN/mkdeb.sh`、`rpm` → `rpm/mkrpm.sh`、`arch` → 在 `pacman/` 里跑 `makepkg`、`mac` → `darwin/mkmac.sh`、`setup` → `windows/mkexe.sh`、`zip` → `portable/mkzip.sh`、`other` → 只打印安装命令（`freedesktop/install.sh`）。

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `platform/DEBIAN/` | `control.in` + `mkdeb.sh` | Debian 包元数据与打包脚本 |
| `platform/pacman/` | `PKGBUILD` + `.install` | Arch 包脚本 |
| `platform/rpm/` | `sm64coopdx-iquedx.spec` + `mkrpm.sh` | Fedora / RHEL / openSUSE 的 RPM |
| `platform/darwin/` | `Info.plist.in` + `mkmac.sh` | macOS 的 `.app` / `.dmg` / `.pkg` / `.zip` |
| `platform/freedesktop/` | `*.desktop` + 图标 + `install.sh` | 非打包发行版共用：启动器与图标 |
| `platform/portable/` | `mkzip.sh` | 便携 zip（三平台通用，解压即玩） |
| `platform/windows/` | `sm64coopdx-iquedx.iss` + `mkexe.sh` + `ChineseSimplified.isl` | Inno Setup 安装向导（`setup.exe`），只能在 Windows 上打；语言文件是仓库自带的（官方安装包不带简体中文） |

### platform/DEBIAN

`control` 是**模板**（`control.in`），不是成品：Debian 的 `Architecture` 只能写一个值，
`amd64, aarch64` 会被 dpkg 拒绝，而且 Debian 侧 aarch64 叫 `arm64`。架构与版本号由
`mkdeb.sh` 在打包时填：

```sh
sh platform/DEBIAN/mkdeb.sh                 # 成品取 <仓库根>/dist，版本 1.5.1
sh platform/DEBIAN/mkdeb.sh <成品目录> <版本>
sh platform/DEBIAN/mkdeb.sh --print         # 只打印填好的 control，便于核对
```

包内路径：`/opt/sm64coopdx-iquedx/`（本体）+ `/usr/share/applications/`（启动器）
+ `/usr/share/icons/hicolor/256x256/apps/`（图标）。需要 `dpkg-deb`（`dpkg` 包自带）。

### platform/pacman

`PKGBUILD` **不自带源码**（`source=()`）：`install.sh` 已经把上游 + 补丁 + cn_extra
组装并构建成 `<仓库根>/dist/`，这里只负责按 Arch 规范打包到 `/opt` 并附带启动器与图标。
`pkgver` 必须与 `upstream.txt` 声明的上游版本对齐，改上游时记得同步。

### platform/rpm

同样**不自带源码**：成品在 `<仓库根>/dist/`。spec 里没有 `%prep` / `%build`，入口
`mkrpm.sh` 先按目标布局铺一个 staging 目录（`opt/sm64coopdx-iquedx` +
`usr/share/applications` + `usr/share/icons/…`），再用 `--define 'stage …'` 让 spec
直接 `cp` 进 `%{buildroot}`：

```sh
sh platform/rpm/mkrpm.sh                 # 成品取 <仓库根>/dist，版本 1.5.1
sh platform/rpm/mkrpm.sh <成品目录> <版本>
sh platform/rpm/mkrpm.sh --print         # 不打包，只打印 staging 内容与将执行的 rpmbuild 命令
```

spec 里三个开关必须保持关闭：`debug_package`（没源码可分）、`__os_install_post`
（含 strip 与 build-id 重链接，会改二进制）、`AutoReqProv`（自动依赖会写进构建机的
库名，交叉打包就错）。依赖按 `%if 0%{?suse_version}` 分两支：openSUSE 用
`libSDL2-2_0-0 / libglvnd / libz1 / libcurl4`，其余用 `SDL2 / mesa-libGL / zlib / libcurl`。
需要 `rpmbuild`（Fedora `dnf install rpm-build`、openSUSE `zypper in rpm-build`）。

### platform/darwin

`mkmac.sh` 把成品目录**整体**塞进 `Contents/MacOS/`——游戏按「可执行文件同目录下有
`res/`、`lang/`」找资源，挪进 `Resources/` 就得改代码；`Contents/Info.plist` 由
`Info.plist.in` 填占位符生成：

```sh
sh platform/darwin/mkmac.sh <成品目录> <版本>          # 默认：.app + .dmg（需要 hdiutil）
sh platform/darwin/mkmac.sh <成品目录> <版本> --zip    # 任意系统都能打 zip
sh platform/darwin/mkmac.sh <成品目录> <版本> --pkg    # .pkg（需要 pkgbuild）
sh platform/darwin/mkmac.sh <成品目录> <版本> --app-only
```

图标优先用 `sips` + `iconutil` 出整套尺寸；没有就退化成「把 256×256 PNG 塞进 icns
的 `ic08` 块」（macOS 10.7+ 认 PNG 承载的 icns，只有一档尺寸）。有 `codesign` 时做
ad-hoc 签名——**Apple Silicon 上未签名的 arm64 二进制会被内核直接拒绝**；这不是公证，
网上下载的包仍会被 Gatekeeper 拦一次（右键 → 打开，或 `xattr -dr com.apple.quarantine`）。
plist 校验在 Darwin 上用 `plutil -lint`，其它系统用 python3 的 `plistlib`——有些发行版
也装了同名 `plutil`，那是 Swift 实现，读文件参数会误报。

### platform/freedesktop

给「不打包、直接解压到 /opt」的发行版用：

```sh
sh platform/freedesktop/install.sh [安装根目录]   # 默认 /opt/sm64coopdx-iquedx
sh platform/freedesktop/install.sh --uninstall
DESTDIR=/tmp/stage sh platform/freedesktop/install.sh <目录>   # 装进 staging，不碰系统、不要 root
```

`Exec=` 会按真实安装路径重写；`Icon=sm64coopdx-iquedx` 依赖同时安装的同名 PNG。

### platform/windows

Inno Setup 的安装向导：成品目录整个装进 `{app}`，带开始菜单/桌面快捷方式、卸载程序和
可选的中文界面。

```sh
sh platform/windows/mkexe.sh                          # 成品取 <仓库根>/dist，版本读 upstream.txt
sh platform/windows/mkexe.sh <成品目录> <版本>
sh platform/windows/mkexe.sh --check                  # 只探测 ISCC 在不在（install.sh 用）
```

- 只能 Windows 上跑：`ISCC.exe` 是原生 Windows 程序，Linux/macOS 没有。其它系统用 `-p zip`。
- 找 ISCC 的顺序是 `$ISCC` → `PATH` → Inno Setup 6 的默认安装位置，默认路径里有空格和括号，
  所以是逐条判断、不拼接。
- `.iss` 必须是 **UTF-8 带 BOM**：Inno Setup 6 只在看到 BOM 时才按 UTF-8 解析，
  否则中文界面全变乱码，而且**编译不报错**。`mkexe.sh` 会查首三个字节，自测里也有一条守着。
- 传参用 `/DMyAppVersion` / `/DMySourceDir` / `/DMyOutputDir` / `/DMyCnLang`。路径要先用
  `cygpath -w` 转成 Windows 形式，否则 ISCC 会把 `/c/...` 当成命令行开关。
- **调 ISCC 前必须 `export MSYS2_ARG_CONV_EXCL='*'`**：MSYS2 会把 `/D` 开头的参数当 POSIX
  路径改写（`/DMyAppVersion=1.5.1` → `D:/MyAppVersion=1.5.1`），ISCC 就把它当成第二个脚本
  文件名，只回一句 `You may not specify more than one script filename.`。路径本来就由
  `cygpath` 转好了，自动转换必须整个关掉。
- **官方 Inno Setup 不带简体中文**（`Languages\` 里只有 26 种官方语言），所以仓库自带
  `platform/windows/ChineseSimplified.isl`（取自 jrsoftware/issrc 的 `Files/Languages/`，
  UTF-8 + CRLF、无 BOM）。查找顺序：用户自己装在编译器目录的那份 → 仓库自带 → 都没有才用
  英文界面；命中后以 `/DMyCnLang=<绝对路径>` 传入。
- 语言文件比编译器新时会报 `Unknown message name`，`mkexe.sh` 因此会**丢掉语言文件重试一次**
  （英文界面），第一次的报错原文照旧打出来 —— 宁可界面是英文，也别整个包产不出来。
- CI 的 Windows job 打完包还会**静默安装 → 检查主程序/dynos/卸载器 → 静默卸载**，
  编译通过不等于装得上，这一步才验得到 `[Files]` 递归和卸载程序。


## GitHub Actions

只有一条流水线 `build.yml`，五个 job 覆盖桌面两平台 × 两架构 + Android：

| job | 默认编译选项 | 说明 |
| --- | --- | --- |
| `linux`（`ubuntu-22.04`） | `-c -f -d` | x86_64。先 `-p no` 只编译，再用 `install.sh -po -p all --path build-iquedx/upstream` 出 `.deb` / `.rpm` / 便携 zip；Arch 包进 `archlinux:latest` 容器跑 `makepkg` |
| `linux-arm`（`ubuntu-22.04-arm`） | `-c -f -d` | aarch64，只出 `.deb` / pacman / 便携 zip（不装 rpm，`-p all` 会跳过并警告）；Arch 包进 `menci/archlinuxarm:base-devel`（官方 archlinux 镜像只有 x86_64） |
| `windows`（`windows-latest`） | `-c -f -d` | MSYS2 mingw64，出 Inno Setup 安装向导（`*-setup.exe`）+ 便携 zip。Inno 用 `choco install innosetup` 装，`ISCC` 的路径写进 `GITHUB_ENV`；找不到就只警告跳过向导，zip 照出 |
| `macos`（矩阵：`macos-latest` / `macos-15-intel`） | `-c -f -d` | arm64 与 x86_64 各出一份 dmg + 便携 zip；编译后多一步「`.app` 不能链回 brew」的自包含检查 |
| `android`（`ubuntu-22.04`） | `-a -c -f -d` | SDK 33 / build-tools 33.0.2；`install.sh` 内部先 `make TOUCH_CONTROLS=1` 生成资源再 `gradle assembleRelease`，之后用上游自带 `platform/android/{key.pk8,certificate.pem}` 签名并上传 |

「默认编译选项」这一列只在 push / PR 触发时成立。手动触发（`workflow_dispatch`）时三个输入会改写它：

| 输入 | 取值 | 作用 |
| --- | --- | --- |
| `upstream` | 汉化版 / 原版 | 汉化版 → `-c`（PC 用 `cn-repo`，Android 用 `android-cn-repo`）；原版不带 |
| `frontend` | 勾选（默认开） | `-f` → 额外应用 `0002`，「设置 → 其他」里出现中文字库开关 |
| `djui_modern` | 勾选（默认开） | `-d` → 应用 djui_modern 模块（在 `0001` 之前）。**只能配汉化版上游**：模块硬依赖汉化上游自带的 `src/pc/djui/cn_glyphs.h`，选「原版 + djui_modern」会被 `install.sh` 当场拒绝并提示 |

拼装逻辑在 `build.yml` 的顶层 `env.IQDX_BUILD_ARGS`，五个 job 共用同一份结果；`scripts/check_workflow_inputs.py` 按 GitHub 表达式的语义把 7 种组合重新求值（boolean 的 `false` 是假值、真分支不能写空串，这两个坑改一行就可能踩回去）。产物名（artifact）不带变体标记，下载前看 run 里「编译选项」那行日志。

每个 job 第一步都是 `sh scripts/test_install_args.sh`，参数表一改就红，避免 CI 里才发现选项没接上。五个 job 共用一个 concurrency 组（`build-<ref>`），push 一次只跑一套，新 push 会把旧的一起取消。

**为什么拆成两次 `install.sh`**：GitHub 的 job 之间不共享工作区，只有 artifact。`-po/--package-only` 就是给「第二个 job 只有产物」准备的：跳过克隆/补丁/编译，直接吃 `--path` 指到的已编译树；没产物一律 `exit 255`（与打包出错的 1 区分）。

## 改动的三个去处

| 改动类型 | 放哪 | 应用方式 |
| --- | --- | --- |
| 改上游已有文件（核心） | `src/`（路径一致） | 生成 `0001-src-overlay.patch`，`git apply -p1` |
| 改上游已有文件（可选前端） | `src/`（路径一致） | 生成 `0002-djui-frontend.patch`，按 `--frontend` 决定要不要打 |
| 改语言字符串 | `scripts/add_lang_strings.sh` 的常量 | 应用阶段按键插入，不生成补丁 |
| 改中文配置项（Android） | `scripts/add_cn_config.sh` 的常量 | 同上，按锚点插入 |
| 上游没有的新文件 | `patches/cn_extra/`（路径一致） | 逐文件补缺拷贝到上游根（已存在的跳过） |
| Android 平台差异 | `scripts/gen_platform_patches.py` 三方合并 | 生成共享的 `0005`，`git apply -p1` |
| 特定平台/发行包差异 | `platform/<name>/` | install.sh 按需调用 |

## 分支

- `main`：可发布状态。
- `dev/<name>`：单个改动开发，合并前必须保证基线 `upstream.txt` 未漂移。

## 版本

打 tag `vX.Y.Z`，tag 说明里写明对应的上游 commit 与启用的可选项。

## 现状（快照，以 git log 为准）

- `src/` 26 个文件：Makefile、text/define_{courses,text}.inc.c、`src/game|menu|pc` 下 23 个（含 `rom_checker.h`）。**`lang/English.ini` 不在其中**：它的改动只有两条字符串，由 `scripts/add_lang_strings.sh` 在应用时插入，脚本对基线跑一次即得到与实验工作区逐字节相同的文件。
- `patches/cn_extra/` 15 个上游不存在的文件：`cn_font`、`cn_tex`、`cn_rom_assets`、`cn_text_gate.h`、`cn_data/*.h`、`cn_intro_data.c`、`text/cn_*.inc.c`、`src/pc/rom_checker_ique.cpp`、`lang/Chinese.ini`；均已用 `git cat-file -e 8cd6e59:<path>` 确认上游不存在，且与实验工作区逐字节一致。**不再收录 `src/pc/djui/cn_glyphs.h`**（djui_modern 的硬依赖）：那份表只有汉化上游自带，等 djui_modern 上游自己兼容，见 `patches/README.md`。
- 3 份补丁**平铺**在 `patches/`：0001（25 文件，必选）、0002（1 文件，可选前端）、0005（3 文件，Android 平台，两个 Android 目标共用）。0001 不含任何 `lang/*.ini`，`configfile.c` 的中文配置项由 `scripts/add_cn_config.sh` 插。
- 校验：`8cd6e59` + 0001 + 0002 + `cn_extra/` + `add_lang_strings.sh` 与 `src/` 26/26 逐字节一致，`lang/English.ini` 与实验工作区一致；两个 Android 上游（0001 + 共享 0005 + 0002 + 两个插入脚本）应用后与三方合并终态各 27 文件逐字节一致；三套基线首次应用 + 幂等复跑均通过；`cn_extra` 15/15 一致，无冲突标记残留。
- `platform/`：DEBIAN（`control.in` + `mkdeb.sh`）、pacman（`PKGBUILD` + `.install`）、rpm（`sm64coopdx-iquedx.spec` + `mkrpm.sh`）、darwin（`Info.plist.in` + `mkmac.sh`）、freedesktop（`.desktop` + PNG 图标 + `install.sh`）。
- 汉化版上游自带的 `palettes/*.ini`、`src/pc/djui/*_glyphs.h`、`textures/custom_font/*.png` 属于所选上游本身，不在本仓库收录范围；它自带的 `lang/Chinese.ini` 也不会被 `cn_extra/` 覆盖（只补缺）。
