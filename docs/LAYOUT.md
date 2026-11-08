# 仓库布局

本仓库只存放「补丁 + 模块」，不存放上游 sm64coopdx 源码、也不存放任何 ROM。
构建时由 `install.sh` 克隆上游源码，再把这里的模块叠加进去。

```
patches/    按序应用的全量补丁（0001-*.patch …），顺序即依赖顺序
modules/    可独立开关的功能模块，一个目录一个模块
scripts/    构建 / 应用补丁的辅助脚本
docs/       设计说明
upstream.txt 补丁基于的上游仓库与 commit（唯一权威基线）
```

## patches/

- 命名：`NNNN-短横线描述.patch`，四位数递增。
- 一律 `git diff` 格式、相对上游根目录的路径（`a/src/...`）。
- 必须能用 `patch -p1` 或 `git apply -p1` 在 `upstream.txt` 指定的基线上干净应用。
- 含二进制文件时改用 `git diff --binary`。
- 改上游代码就放这里；新增文件不要放这里，放模块。

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
4. **新增 vs 修改**。`files/` 只负责「原仓库里没有的文件」，改已有文件一律用 `patch.patch`，这样上游升级时冲突能立刻暴露。

## 分支

- `main`：可发布状态。
- `dev/<name>`：单个模块开发，合并前必须保证基线 `upstream.txt` 未漂移。

## 版本

打 tag `vX.Y.Z`，tag 说明里写明对应的上游 commit 与本版启用的模块清单。
