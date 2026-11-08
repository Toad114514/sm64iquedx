# 补丁索引

**所有补丁平铺在 `patches/` 根目录，不建子目录。** 四套上游共用前三份补丁与 `cn_extra/`，Android 只额外叠加一份平台增量，汉化版 Android 再换一份自己的语言补丁。

| 补丁 | 文件数 | 适用目标 | 作用 |
| --- | --- | --- | --- |
| `0001-src-overlay.patch` | 25 | `pc` / `android` / `android-cn` 共用 | 必选，核心代码改动。**不含任何 `lang/*.ini`，也不含 DJUI 前端** |
| `0002-djui-frontend.patch` | 1 | 三目标共用 | 可选，设置 → 其他 的「中文字库」开关 |
| `0003-lang-strings.patch` | 1 | `pc` / `android` 共用 | 必选，`lang/English.ini` 加 2 条字符串 |
| `0004-lang-android-cn.patch` | 2 | `android-cn` 专用 | 必选，`lang/English.ini` + `lang/Chinese.ini` 各加 1 条字符串 |
| `0005-android.patch` | 4 | `android` 专用 | 必选，Android 平台增量 |
| `0006-android-cn.patch` | 4 | `android-cn` 专用 | 必选，Android 平台增量 |
| `cn_extra/` | 12 | 三目标共用 | 上游不存在的新增文件，整目录覆盖 |

四套上游与目标名的对应见仓库根 `upstream.txt`；`pc` 同时覆盖原版 CoopDX 与上善若的 PC 汉化版。

## 为什么语言字符串要独立、且主补丁要避开

`lang/English.ini` / `lang/Chinese.ini` 的改动**只是往 ini 里加字符串**：

```
CHINESE_TEXT_PACK = "Chinese Text Pack (iQue)"   # DISABLE_POPUPS 之后
Chinese = "Chinese (中文)"                       # 语言列表末尾
CHINESE_TEXT_PACK = "中文字库（神游）"              # 汉化版 Chinese.ini
```

但每套上游的 ini 都不一样：PC 基线 456 行、原版 Android 468 行、汉化版 Android 457 行且**自带 `Chinese = ...` 条目**（再加一次就重复）。行号和 hunk 上下文都会漂移，混在 `0001` 里会让主补丁在汉化版 Android 上直接打不上。

因此整个 `lang/` 前缀从 `0001` 里排除，由生成器自己保证（不是靠应用器 `--exclude`）：

- `0003` 是对 **PC 基线** 的 diff，加 2 行。`pc` 与 `android` 都能干净应用（原版 Android 虽多 12 行，但两处 hunk 的上下文一致，`git apply` 会按内容定位）。
- `0004` 只服务 `android-cn`：`English.ini` 只加 `CHINESE_TEXT_PACK`（`Chinese` 已有）、`Chinese.ini` 加中文文案。二者都是对该上游自身 ini 的 diff。
- 三个目标各自只应用其中**一份**语言补丁，不会叠加。

> 这两行即使不打可选的 `0002` 也无害（只是多/少一个被 DLANG 引用的条目），所以语言补丁归在必选。

## 平台增量补丁

`0005` / `0006` 各 4 个文件：`Android.mk`、`Makefile`、`src/pc/configfile.c`、`src/pc/pc_main.c`。**不含任何 ini**。

| 文件 | 原因 |
| --- | --- |
| `Android.mk` | 注入 `LOCAL_CFLAGS += -DSM64COOPDX_CN_TEXT=1`（带 `CN_TEXT_PACK=false` 开关）。PC 靠 `Makefile` 注入这个宏，**ndk-build 根本不读 Makefile**，不注入的话所有 `#if defined(SM64COOPDX_CN_TEXT)` 都会被编译掉，汉化完全不生效 |
| `Makefile` | 三方合并后的 PC 改动。对 ndk-build 无作用，保留以便与 PC 一致；`CN_TEXT_DEPS` 对生成 `text/define_*.inc.c` 仍有实际作用 |
| `src/pc/configfile.c` | Android 在中文配置项的同一位置插入 `TOUCH_CONTROLS` 块 |
| `src/pc/pc_main.c` | Android 在中文初始化的同一位置切 `SDL_main` / `TARGET_ANDROID` 签名分支 |

这 3 个「上下文漂移」文件在应用 `0001` 时被 `--exclude` 跳过，由平台补丁直接给出终态。终态由三方合并得到：

```
base   = PC 8cd6e59
ours   = 对应 Android 上游
theirs = 本仓库 src/（PC 终态）
```

两处**双方在同一位置插入**的冲突按固定规则消解，出现任何其它冲突时生成脚本直接报错退出，不静默产出坏补丁：

| 文件 | Android 侧 | PC 侧 | 消解 |
| --- | --- | --- | --- |
| `configfile.c` | `#ifdef TOUCH_CONTROLS` 触屏配置项 | 3 个 `configChineseText*` 配置项 | Android 块在前，PC 条目在后 |
| `pc_main.c` | `#ifdef TARGET_ANDROID / int SDL_main / #else` | 中文文本包初始化 | PC 函数块在前，Android 签名分支在后 |

顺序不能反：PC 的初始化函数若落进 `#else` 分支内部，预处理结构就散了。

`Android.mk` 的 `SRC_DIRS` 用 wildcard 收集源码，`cn_extra/` 里的 `.c/.cpp` 会自动进构建，不需要改源文件列表。

### 上游自带差异、但不进平台补丁

`src/pc/configfile.h` 和 `src/pc/lua/utils/smlua_misc_utils.c` 在两个 Android 上游与 PC 基线内容不同，但 `0001` 的 hunk 上下文没被破坏，能直接应用。应用后它们等于「Android 上游 + PC 改动」，与 PC 终态不完全相同是**预期**的；已验证 `0001` 新增的 10 行在这两个文件里一行不缺（含 `cn_tex_gauge_dy()`）。

汉化版 Android 自带的 `palettes/*.ini`、`src/pc/djui/*_glyphs.h`、`textures/custom_font/*.png` 属于上游本身，补丁不改动、也不收录；`cn_extra/` 里没有 `lang/Chinese.ini`，覆盖新增文件时不会把上游这份冲掉。

## 应用

统一走应用器，重复执行安全（已应用的补丁会被识别并跳过）：

```bash
scripts/apply_patches.sh <pc|android|android-cn> <上游干净 checkout> [--frontend]
```

内部顺序：

| 目标 | 顺序 |
| --- | --- |
| `pc` | `0001` → `0003` → [`0002`] → `cn_extra/` |
| `android` | `0001`（跳过 3 个漂移文件）→ `0003` → `0005` → [`0002`] → `cn_extra/` |
| `android-cn` | `0001`（跳过 3 个漂移文件）→ `0004` → `0006` → [`0002`] → `cn_extra/` |

`[]` 表示仅 `--frontend` 时应用。**不要**用 `cp -R src/.` 覆盖终态——那会无条件带上前端，也会把 Android 的平台差异冲掉。

排除列表同时存在于 `scripts/gen_platform_patches.py` 的 `EXCLUDED` 和 `scripts/apply_patches.sh`，生成脚本会断言两者完全一致，**改一处必须改另一处**。

## 重新生成

```bash
# 1. 改动 src/ 后，重生成共用的 0001 / 0002 / 0003
sh scripts/gen_all_patches.sh <PC 8cd6e59 基线目录>

# 2. 重生成 0004 / 0005 / 0006（Android 上游 checkout 见 upstream.txt）
python3 scripts/gen_platform_patches.py \
    --pc-base    <PC 8cd6e59 基线目录> \
    --android    <ManIsCat2 上游目录> \
    --android-cn <汉化版 Android 上游目录>
```

第 1 步会断言三段补丁互不重叠、且 `0001` 里没有 `lang/` 和前端文件。第 2 步自检：`0001` + 语言补丁 + 平台补丁 + `0002` 叠加后与三方合并终态逐字节一致（`android` 28 文件 / `android-cn` 29 文件），不应用 `0002` 时前端文件保持上游原版，平台补丁与 `0001`、语言补丁互不重叠。
