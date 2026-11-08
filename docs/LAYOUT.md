# 仓库布局

本仓库只存放「源码改动 + 补丁 + 模块」，不存放上游 sm64coopdx 全量源码、也不存放任何 ROM。
构建时由 `install.sh` 克隆上游源码，再把这里的改动叠加进去。

```
src/         被修改过的文件，完整副本，路径与上游**逐级一致**（唯一编辑源）
patches/     由 src/ 对基线 diff 出来的补丁，供 install.sh 按序应用
modules/     可独立开关的功能模块，一个目录一个模块
scripts/     构建 / 生成补丁 / 应用补丁的辅助脚本
docs/        设计说明
upstream.txt 补丁基于的上游仓库与 commit（唯一权威基线）
```

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
5. **新增文件不进 `src/`**。上游根本没有的文件放 `modules/<name>/files/`，否则无法区分「改」和「加」，上游升级时也看不出哪些是新增。

## patches/

- 命名：`NNNN-短横线描述.patch`，四位数递增，顺序即依赖顺序。
- 一律 `git diff` 格式、相对上游根目录的路径（`a/src/...`）。
- 必须能用 `patch -p1` 或 `git apply -p1` 在 `upstream.txt` 指定的基线上干净应用。
- 含二进制文件时改用 `git diff --binary`。
- **由 `scripts/gen_patches.sh` 从 `src/` 生成**，不手工编写、不手工改。

## scripts/

| 脚本 | 作用 |
| --- | --- |
| `gen_patches.sh <上游根目录> [输出]` | 遍历 `src/`，对基线逐文件 diff，生成 `patches/0001-src-overlay.patch` |

`gen_patches.sh` 的行为约定（已覆盖测试）：内容相同的文件跳过；`src/` 里有而基线上没有的文件直接报错（应改放 `modules/<name>/files/`）；含 NUL 字节的二进制直接报错（`diff` 对二进制只回一句「文件不同」且退出码仍为 1，不自己判会把这句话写进补丁）。

## modules/

每个模块一个目录，约定三个可选入口：

```
modules/<name>/
  module.conf      元信息：name / 依赖 / 默认是否启用 / 互斥项
  files/           要新增的文件，内部路径镜像目标仓库根目录
  patch.patch      对既有文件的修改（同 patches/ 的格式）
  apply.sh         需要额外动作时（生成代码、改 Makefile 等）的钩子
```

关键约束：

1. **模块之间不互相直接改文件**。A 需要 B 的符号时，用 `module.conf` 声明依赖，由应用器保证顺序，而不是让 A 去 patch B 的产物。
2. **可关闭**。任何模块被关掉后，其余部分必须仍能编译通过；关闭逻辑只能落在 `apply.sh` 里做条件跳过，不能靠注释掉代码。
3. **幂等**。`apply.sh` 重复执行结果一致（先检查标记，再写文件）。
4. **新增 vs 修改**。`files/` 只负责「原仓库里没有的文件」，改已有文件走 `patch.patch`，这样上游升级时冲突能立刻暴露。

## 改动的三个去处

| 改动类型 | 放哪 | 应用方式 |
| --- | --- | --- |
| 改上游已有文件 | `src/`（路径一致） | 生成 `patches/NNNN-*.patch`，按序 `git apply` |
| 上游没有的新文件 | `modules/<name>/files/` | 整目录拷贝 |
| 需要动构建/生成代码 | `modules/<name>/apply.sh` | 脚本钩子 |

## 分支

- `main`：可发布状态。
- `dev/<name>`：单个模块开发，合并前必须保证基线 `upstream.txt` 未漂移。

## 版本

打 tag `vX.Y.Z`，tag 说明里写明对应的上游 commit 与本版启用的模块清单。

## 现状（快照，以 git log 为准）

- `src/` 26 个文件：Makefile、text/define_{courses,text}.inc.c、lang/English.ini、`src/game|menu|pc` 下 22 个。
- `modules/cn-text/files/` 12 个上游不存在的文件：`cn_font`、`cn_tex`、`cn_data/*.h`、`cn_intro_data.c`、`text/cn_*.inc.c`，以及 `lang/Chinese.ini`（上游 `lang/` 只有 11 种语言，没有中文）。
- `patches/0001-src-overlay.patch`：对 `8cd6e59` 生成，应用后与 `src/` 逐字节一致（26/26 校验通过）。
