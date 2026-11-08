# 仓库布局

本仓库只存放「源码改动 + 补丁 + 平台差异」，不存放上游 sm64coopdx 全量源码、也不存放任何 ROM。
构建时由 `install.sh` 克隆上游源码，再把这里的改动叠加进去。

```
src/                  被修改过的文件，完整副本，路径与上游**逐级一致**（唯一编辑源）
patches/0001-*.patch  主补丁：除 DJUI 前端外的全部改动，必选
patches/0002-*.patch  DJUI 前端补丁（README 里的 UIPatch），可选
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

- 命名：`NNNN-短横线描述.patch`，四位数递增，顺序即依赖顺序。
- 一律 `git diff` 格式、相对上游根目录的路径（`a/src/...`）。
- 必须能用 `patch -p1` 或 `git apply -p1` 在 `upstream.txt` 指定的基线上干净应用。
- **由 `scripts/gen_all_patches.sh` 从 `src/` 一次生成全部补丁**，不手工编写、不手工改。
- 应用前先 `git apply --check`；选了与声明不同的基线时可以跳过补丁、直接用 `src/` 覆盖终态。

## 可选补丁：0002-djui-frontend.patch

只改 `src/pc/djui/djui_panel_misc.c`，在「设置 → 其他」里加一个「中文字库（神游）」复选框，运行时切换并持久化。
**不应用它时功能照常可用，只是要改 `config.ini` 里的 `chinese_text` / `chinese_text_prompted` 来切换**（README 中的 UIPatch 就是它）。

拆分边界是一条硬约束，别顺手扩大：

- `src/pc/djui/djui_panel_confirm.[ch]` **必须留在 0001**。`pc_main.c` 的首启确认框调用 `djui_panel_confirm_create_ex()`，这个函数是这两个文件新增的；一起挪到 0002 的话，不选前端补丁就是未定义符号 / 链接失败。
- `lang/English.ini` 里的 `CHINESE_TEXT_PACK` 字符串留在 0001。它只是多一个未被 DLANG 引用的条目，放着无害，也不值得为它再拆一次上下文。
- 0002 对干净基线也能独立 `git apply -p1`（0001 不碰这个文件），没有顺序耦合。

对应 `install.sh` 的 `--frontend`：选中才应用 0002。既然 0002 可选，**就不能用 `cp -R src/.` 覆盖终态**（那会无条件带上前端），要么只覆盖 0001 覆盖的文件，要么用「应用 0001 → 按需应用 0002 → 覆盖 cn_extra」的顺序。

## scripts/

| 脚本 | 作用 |
| --- | --- |
| `gen_all_patches.sh <上游基线根目录>` | **改完 `src/` 后跑这个**，一次生成 0001 / 0002 并断言两者不重叠 |
| `gen_patches.sh <上游根目录> [输出] [--only 前缀] [--exclude 前缀]` | 单个补丁生成器，按相对路径前缀筛选文件 |

`gen_patches.sh` 的行为约定（已覆盖测试）：内容相同的文件跳过；`src/` 里有而基线上没有的文件直接报错（应改放 `patches/cn_extra/`）；含 NUL 字节的二进制直接报错（`diff` 对二进制只回一句「文件不同」且退出码仍为 1，不自己判会把这句话写进补丁）。

## platform/

按平台 / 发行包分目录，放与具体构建环境相关的补丁或脚本，例如 `platform/deb/`、`platform/arch/`。
`install.sh` 只在用户选中对应打包方式时才调用这里的脚本；目录为空时跳过。

## 改动的三个去处

| 改动类型 | 放哪 | 应用方式 |
| --- | --- | --- |
| 改上游已有文件（核心） | `src/`（路径一致） | 生成 `0001-src-overlay.patch`，`git apply -p1` |
| 改上游已有文件（可选前端） | `src/`（路径一致） | 生成 `0002-djui-frontend.patch`，按 `--frontend` 决定要不要打 |
| 上游没有的新文件 | `patches/cn_extra/`（路径一致） | 整目录拷贝到上游根 |
| 特定平台/发行包差异 | `platform/<name>/` | install.sh 按需调用 |

## 分支

- `main`：可发布状态。
- `dev/<name>`：单个改动开发，合并前必须保证基线 `upstream.txt` 未漂移。

## 版本

打 tag `vX.Y.Z`，tag 说明里写明对应的上游 commit 与启用的可选项。

## 现状（快照，以 git log 为准）

- `src/` 26 个文件：Makefile、text/define_{courses,text}.inc.c、lang/English.ini、`src/game|menu|pc` 下 22 个。
- `patches/cn_extra/` 11 个上游不存在的文件：`cn_font`、`cn_tex`、`cn_data/*.h`、`cn_intro_data.c`、`text/cn_*.inc.c`；均已用 `git cat-file -e 8cd6e59:<path>` 确认上游不存在，且与实验工作区逐字节一致。
- `patches/0001-src-overlay.patch`：25 个文件，必选。
- `patches/0002-djui-frontend.patch`：1 个文件（`djui_panel_misc.c`），可选。
- 校验：`8cd6e59` + 0001 + 0002 与 `src/` 26/26 逐字节一致；只打 0001 时 `djui_panel_misc.c` 保持上游原版且 `create_ex` 定义/声明/调用三者齐全；0002 对干净基线也能独立应用。
- 汉化版上游自带的 `lang/Chinese.ini`、`palettes/*.ini`、`src/pc/djui/*_glyphs.h`、`textures/custom_font/*.png` 属于所选上游本身，不在本仓库收录范围。
