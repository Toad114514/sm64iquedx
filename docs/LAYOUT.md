# 仓库布局

本仓库只存放「源码改动 + 补丁 + 平台差异」，不存放上游 sm64coopdx 全量源码、也不存放任何 ROM。
构建时由 `install.sh` 克隆上游源码，再把这里的改动叠加进去。

```
src/                  被修改过的文件，完整副本，路径与上游**逐级一致**（唯一编辑源）
patches/NNNN-*.patch  **全部平铺在 patches/ 根目录，不建子目录**：
                      0001 共用主补丁 / 0002 共用可选前端 / 0003 共用语言字符串
                      0004 汉化版 Android 语言 / 0005、0006 两个 Android 平台增量
patches/cn_extra/     上游根本不存在的新增文件，完整副本，路径与上游逐级一致
platform/             特定平台/发行包的补丁与构建脚本
scripts/              生成补丁的辅助脚本
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

上游不存在的文件以完整副本存放，内部路径仍然镜像上游根目录，方便 `install.sh` 整目录覆盖：

```
patches/cn_extra/src/game/cn_font.c       → 上游 src/game/cn_font.c
patches/cn_extra/src/game/cn_tex.c        → 上游 src/game/cn_tex.c
patches/cn_extra/text/cn_dialogs_cn.inc.c → 上游 text/cn_dialogs_cn.inc.c
```

- 不打成 `.patch`：新增文件通常是大体积字形表 / 生成产物，做文本 diff 没有意义，`git apply` 也容易因上下文漂移失败。
- 里面既有手写源码，也有**工具生成产物**（`cn_data/*.h`、`cn_intro_data.c`、`text/cn_*.inc.c`）。生成产物改生成器后要重新同步，别手改。
- 应用方式：`cp -R patches/cn_extra/. <上游根目录>/`，路径一一对应，天然幂等。

## patches/NNNN-*.patch

- 命名：`NNNN-短横线描述.patch`，四位数递增。**所有补丁平铺在 `patches/` 根目录**，不要为某个平台建子目录。
- 一律 `git diff` 格式、相对上游根目录的路径（`a/src/...`）。
- 必须能用 `patch -p1` 或 `git apply -p1` 在 `upstream.txt` 指定的基线上干净应用。
- `0001`/`0002`/`0003` **由 `scripts/gen_all_patches.sh` 从 `src/` 一次生成**；`0004`/`0005`/`0006` 由 `scripts/gen_platform_patches.py` 生成。都不手工编写、不手工改。
- 应用前先 `git apply --check`；选了与声明不同的基线时可以跳过补丁、直接用 `src/` 覆盖终态。
- **只有一份主补丁**：PC 与两个 Android 上游共用 `0001`/`0002`/`0003`，不要为 Android 再复制一整套。平台差异走 `0005`/`0006`，见下一节。

## 语言字符串补丁：0003 / 0004

`lang/English.ini` / `lang/Chinese.ini` 的改动只是往 ini 里加字符串，但每套上游的 ini 行号、自带条目都不同（汉化版 Android 已自带 `Chinese = ...`），混进 `0001` 会让主补丁在汉化版 Android 上打不上。所以整个 `lang/` 前缀从 `0001` 里**排除**——由生成器自己筛掉，不靠应用器的 `--exclude`：

| 补丁 | 生成自 | 适用 |
| --- | --- | --- |
| `0003-lang-strings.patch` | `src/lang/English.ini` 对 PC 基线 diff（加 2 行） | `pc` + `android` |
| `0004-lang-android-cn.patch` | 汉化版 Android 自身的 English.ini / Chinese.ini 三方合并终态（各加 1 行） | `android-cn` |

每个目标只应用其中一份。`0001` 与 `0005`/`0006` 都**不含任何 ini**，改语言字符串时只动这两份补丁。

## 平台增量补丁：0005 / 0006

两份各 4 个文件（`Android.mk`、`Makefile`、`src/pc/configfile.c`、`src/pc/pc_main.c`），只负责共用主补丁覆盖不到的部分：

1. **三个上下文漂移文件**：`Makefile`、`src/pc/configfile.c`、`src/pc/pc_main.c`。应用 `0001` 时用 `--exclude` 跳过它们，由平台补丁给出三方合并后的终态。
2. **`Android.mk` 的宏注入**：ndk-build 不读 Makefile，必须往 `LOCAL_CFLAGS` 加 `-DSM64COOPDX_CN_TEXT=1`，否则汉化整块被编译掉。

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
- `lang/*.ini` 的字符串改动已独立成 `0003`/`0004`（见上节），`0001` 里完全没有语言文件。`CHINESE_TEXT_PACK` 即使不打 `0002` 也无害（只是多一个被 DLANG 引用的条目），所以语言补丁归在必选。
- 0002 对干净基线也能独立 `git apply -p1`（0001 不碰这个文件），没有顺序耦合。

对应 `install.sh` 的 `--frontend`：选中才应用 0002。既然 0002 可选，**就不能用 `cp -R src/.` 覆盖终态**（那会无条件带上前端），要么只覆盖 0001 覆盖的文件，要么用「应用 0001 → 按需应用 0002 → 覆盖 cn_extra」的顺序。

## scripts/

| 脚本 | 作用 |
| --- | --- |
| `apply_patches.sh <目标> <上游根目录> [--frontend]` | **应用器**，按目标选补丁、按序叠加、幂等；平时用它，别手写 `git apply --exclude` |
| `gen_all_patches.sh <上游基线根目录>` | **改完 `src/` 后跑这个**，一次生成共用的 0001 / 0002 / 0003 并断言三段分工不重叠 |
| `gen_platform_patches.py --pc-base … --android … --android-cn …` | 重生成 0004 / 0005 / 0006，内含三方合并、冲突消解与终态自检 |
| `gen_patches.sh <上游根目录> [输出] [--only 前缀] [--exclude 前缀]` | 单个补丁生成器，按相对路径前缀筛选；`--only`/`--exclude` 可各重复多次（取并集） |

`gen_patches.sh` 的行为约定（已覆盖测试）：内容相同的文件跳过；`src/` 里有而基线上没有的文件直接报错（应改放 `patches/cn_extra/`）；含 NUL 字节的二进制直接报错（`diff` 对二进制只回一句「文件不同」且退出码仍为 1，不自己判会把这句话写进补丁）；筛不出差异时写一个空补丁文件而不是保留旧文件，应用器遇到空补丁会跳过。

## platform/

按平台 / 发行包分目录，放与具体构建环境相关的补丁或脚本。

> ⚠️ `install.sh` **目前还没有调用这里的任何脚本**（`ique_patch_apply()` 仍是空的，也不构建/打包）。这些目录现在是「可用但未接线」：脚本可以手工跑，自动流程要等 `install.sh` 补完。

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `platform/DEBIAN/` | `control.in` + `mkdeb.sh` | Debian 包元数据与打包脚本 |
| `platform/pacman/` | `PKGBUILD` + `.install` | Arch 包脚本 |
| `platform/freedesktop/` | `*.desktop` + 图标 + `install.sh` | 非打包发行版共用：启动器与图标 |

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

### platform/freedesktop

给「不打包、直接解压到 /opt」的发行版用：

```sh
sh platform/freedesktop/install.sh [安装根目录]   # 默认 /opt/sm64coopdx-iquedx
sh platform/freedesktop/install.sh --uninstall
DESTDIR=/tmp/stage sh platform/freedesktop/install.sh <目录>   # 装进 staging，不碰系统、不要 root
```

`Exec=` 会按真实安装路径重写；`Icon=sm64coopdx-iquedx` 依赖同时安装的同名 PNG。


## 改动的三个去处

| 改动类型 | 放哪 | 应用方式 |
| --- | --- | --- |
| 改上游已有文件（核心） | `src/`（路径一致） | 生成 `0001-src-overlay.patch`，`git apply -p1` |
| 改上游已有文件（可选前端） | `src/`（路径一致） | 生成 `0002-djui-frontend.patch`，按 `--frontend` 决定要不要打 |
| 改上游已有文件（语言字符串） | `src/`（路径一致） | 生成 `0003-lang-strings.patch`；汉化版 Android 的 ini 由 `0004` 单独生成 |
| 上游没有的新文件 | `patches/cn_extra/`（路径一致） | 整目录拷贝到上游根 |
| Android 平台差异 | `scripts/gen_platform_patches.py` 三方合并 | 生成 `0005` / `0006`，`git apply -p1` |
| 特定平台/发行包差异 | `platform/<name>/` | install.sh 按需调用 |

## 分支

- `main`：可发布状态。
- `dev/<name>`：单个改动开发，合并前必须保证基线 `upstream.txt` 未漂移。

## 版本

打 tag `vX.Y.Z`，tag 说明里写明对应的上游 commit 与启用的可选项。

## 现状（快照，以 git log 为准）

- `src/` 27 个文件：Makefile、text/define_{courses,text}.inc.c、lang/English.ini、`src/game|menu|pc` 下 23 个（含 `rom_checker.h`）。
- `patches/cn_extra/` 12 个上游不存在的文件：`cn_font`、`cn_tex`、`cn_data/*.h`、`cn_intro_data.c`、`text/cn_*.inc.c`、`src/pc/rom_checker_ique.cpp`；均已用 `git cat-file -e 8cd6e59:<path>` 确认上游不存在，且与实验工作区逐字节一致。
- 6 份补丁**平铺**在 `patches/`：0001（25 文件，必选）、0002（1 文件，可选前端）、0003（1 文件，共用语言）、0004（2 文件，汉化版 Android 语言）、0005（4 文件，原版 Android 平台）、0006（4 文件，汉化版 Android 平台）。0001 不含任何 `lang/*.ini`。
- 校验：`8cd6e59` + 0001 + 0003 + 0002 + `cn_extra/` 与 `src/` 27/27 逐字节一致；两个 Android 上游应用后与三方合并终态 28/29 文件逐字节一致；三套基线首次应用 + 幂等复跑均通过；`cn_extra` 12/12 一致，无冲突标记残留。
- `platform/`：DEBIAN（`control.in` + `mkdeb.sh`）、pacman（`PKGBUILD` + `.install`）、freedesktop（`.desktop` + PNG 图标 + `install.sh`）。
- 汉化版上游自带的 `lang/Chinese.ini`、`palettes/*.ini`、`src/pc/djui/*_glyphs.h`、`textures/custom_font/*.png` 属于所选上游本身，不在本仓库收录范围。
