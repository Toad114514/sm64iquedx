[English](README.en.md) | **简体中文**

# 补丁索引

补丁**全部平铺在 `patches/` 下**，文件名 = 四位编号 + 分组 + 职责，**编号就是应用顺序**（`sort` 即顺序，脚本不另存一份列表）。分组只活在文件名里：可选组用前缀 glob 区分（`--frontend` → `0007-*.patch`，android → `0008-*.patch`），不再为可选补丁多套一层目录。

| 补丁文件 | 文件数 | 适用目标 | 作用 |
| --- | --- | --- | --- |
| `0001-build-and-config.patch` | 3 | 四套上游共用 | `Makefile` 的 `CN_TEXT_PACK` 开关 + `configChineseText*` 三个配置项 |
| `0002-rom-assets.patch` | 3 | 共用 | 神游 ROM 探测（`rom_checker.h`）、启动装配（`pc_main.c`）、分段资源读取（`rom_assets.c`）。`mods/mods.c` 已在「改版 ⇒ 汉化整包停用」拆除时恢复成上游原样，不再出现在补丁里 |
| `0003-text-core.patch` | 8 | 共用 | 两字节编码/字形核心、HUD 与标题文本（`print.c` / `area.c`）、课程与动作名查表（`level_info.[ch]`）、进关时切换文本表（`level_update.c`）、`text/define_*.inc.c` |
| `0004-dialog.patch` | 5 | 共用 | 双表架构（`dialog_table.[ch]`）、对话框渲染与分页/盒高（`ingame_menu.[ch]`）、Lua 侧替换 |
| `0005-menus.patch` | 5 | 共用 | 文件选择、选星盘、标题过场（含 iQue 标题网格） |
| `0006-asset-overlay.patch` | 2 | 共用 | iQue 贴图/几何覆盖的消费侧：能量表轮盘偏移（HUD 底层 + Lua 绑定） |
| `0007-djui-frontend.patch` | 3 | 三目标共用，**可选** | 设置 → 其他 的「中文字库」开关；`djui_panel_confirm.[ch]` 的 `create_ex()`（目前无调用点） |
| `0008-android.patch` | 3 | `android` / `android-cn` **共用** | Android 平台增量（`Android.mk` / `Makefile` / `pc_main.c`） |
| `cn_extra/` | 19 | 三目标共用 | 上游不存在的新增文件，仅补缺、不覆盖 |

`0001`~`0006` 必选、四套上游共用；`0007` 只在 `--frontend` 时应用；`0008` 由两个 Android 目标共用**同一份**。新增一个分组 = 加一个编号更大的文件，不需要改应用器的分组表。语言字符串与中文配置项**不是补丁**，由 `scripts/add_lang_strings.sh` / `scripts/add_cn_config.sh` 在应用阶段插入。

拆分后总数 29 个文件（`0001`~`0007` 合计），`scripts/gen_all_patches.sh` 每次生成都会拿「不加分组过滤的全量差异」当参照，断言分组**无遗漏、无重复**；同时拦住两种会「同一处改动被打两遍」的残留：拆分前的单文件补丁（`0001-src-overlay.patch` 等），以及按目录分组的旧目录（`01-build/` … `08-platform/`）。

四套上游与目标名的对应见仓库根 `upstream.txt`；`pc` 同时覆盖原版 CoopDX 与上善若的 PC 汉化版。

## 语言字符串为什么不用补丁

要加的其实只有两条：

```
CHINESE_TEXT_PACK = "..."    # [MISC] 段，紧跟 DISABLE_POPUPS
Chinese = "..."              # [LANGUAGE] 段，紧跟 Spanish
```

但每套上游的 ini 都不同：PC 基线 456 行、原版 Android 468 行、汉化版 Android 457 行且**自带 `Chinese = ...`**（再加一次就重复）。给每个目标生成一份 ini 补丁，就要维护 3 份内容几乎相同、行号和上下文各不相同的 diff，上游动一行还得全部重生成。

所以改成**按键插入**，一份脚本服务四套上游：

```bash
scripts/add_lang_strings.sh <上游根目录> [--check]
```

| 行为 | 说明 |
| --- | --- |
| 已存在该键 | 跳过（按 `^KEY[[:space:]]*=` 判断，不看行号） |
| 锚点存在 | 插在锚点行之后，因此段归属天然正确 |
| 锚点缺失 | **报错退出**，不猜位置 |
| 只处理 | `lang/English.ini` 与 `lang/Chinese.ini`；其它语言文件按上游原样保留 |
| 文案 | `English.ini` → `CHINESE_TEXT_PACK = "Chinese Text Pack (iQue)"`、`Chinese = "Chinese (中文)"`；`Chinese.ini` → `CHINESE_TEXT_PACK = "中文字库（神游）"`、`Chinese = "中文"` |

`--check` 只报告待插入条目、不写盘，用于校验；重复执行始终安全（已插入的键全部跳过）。对 PC 基线跑一次的结果与实验工作区 `lang/English.ini` **逐字节一致**，所以 `src/` 里不再放这个文件——字符串只有一个来源，就是脚本。

每个分组在生成时都把整个 `lang/` 前缀排除掉（由生成器保证，不靠应用器的 `--exclude`）：这样所有补丁在四套上游上都不碰 ini，语言相关的东西只有这一处。

> 这两条字符串即使不打可选的 `0007-djui-frontend.patch` 也无害（只是多/少一个被 DLANG 引用的条目）。

## 平台增量补丁

`0008-android.patch` **只有一份**，两个 Android 目标共用，含 3 个文件：`Android.mk`、`Makefile`、`src/pc/pc_main.c`。**不含任何 ini，也不含 `configfile.c`**。

| 文件 | 原因 |
| --- | --- |
| `Android.mk` | 注入 `LOCAL_CFLAGS += -DSM64COOPDX_CN_TEXT=1`（带 `CN_TEXT_PACK=false` 开关）。PC 靠 `Makefile` 注入这个宏，**ndk-build 根本不读 Makefile**，不注入的话所有 `#if defined(SM64COOPDX_CN_TEXT)` 都会被编译掉，汉化完全不生效 |
| `Makefile` | 三方合并后的 PC 改动。对 ndk-build 无作用，保留以便与 PC 一致；`CN_TEXT_DEPS` 对生成 `text/define_*.inc.c` 仍有实际作用 |
| `src/pc/pc_main.c` | Android 在中文初始化的同一位置切 `SDL_main` / `TARGET_ANDROID` 签名分支 |

`Makefile`（在 `0001-build-and-config.patch`）与 `pc_main.c`（在 `0002-rom-assets.patch`）这两个「上下文漂移」文件在应用 `0001`~`0006` 时被 `--exclude` 跳过，由平台补丁直接给出终态。终态由三方合并得到：

```
base   = PC 8cd6e59
ours   = 对应 Android 上游
theirs = 本仓库 src/（PC 终态）
```

`pc_main.c` 是**双方在同一位置插入**，按固定规则消解；出现任何其它冲突时生成脚本直接报错退出，不静默产出坏补丁：

| 文件 | Android 侧 | PC 侧 | 消解 |
| --- | --- | --- | --- |
| `pc_main.c` | `#ifdef TARGET_ANDROID / int SDL_main / #else` | 中文文本包初始化 | PC 函数块在前，Android 签名分支在后 |

顺序不能反：PC 的初始化函数若落进 `#else` 分支内部，预处理结构就散了。

`Android.mk` 的 `SRC_DIRS` 用 wildcard 收集源码，`cn_extra/` 里的 `.c/.cpp` 会自动进构建，不需要改源文件列表。

### 为什么 `configfile.c` 不进平台补丁（旧的 `0006` 已删）

以前每个目标一份：`0005` 给 `android`、`0006` 给 `android-cn`。但把两份补丁逐行比一遍只有 **24 行差异**，且全部落在 `configfile.c` 的两个 hunk 上——加进去的 6 行**完全一样**，不同的只是上下文：

```diff
-    {.name = "touch_snap",     .type = ..., .boolValue = &configSnapTouch},
+    {.name = "touch_snap",     .type = ..., .boolValue = &configElementSnap},
```

也就是原版 Android 上游的 `configfile.c` 还停在旧版（`configSnapTouch`），汉化版 Android 已经跟到 `configElementSnap`。补丁没有错，错的是**为一处上下文漂移维护两份几乎相同的补丁**：上游每动一行就要重生成两份，还容易只改一份。

改成幂等插入脚本 `scripts/add_cn_config.sh`（按锚点插、已存在则跳过、`--check` 只报告）。那 6 行本来就一模一样，插进去的自然也一模一样。已实测三棵树逐字节一致：

| 场景 | 结果 |
| --- | --- |
| `android` 上游 + 脚本 | == 旧 `0005` 给出的 `configfile.c` 终态 |
| `android-cn` 上游 + 脚本 | == 旧 `0006` 给出的终态 |
| PC 基线 + 脚本 | == 本仓库 `src/src/pc/configfile.c`（PC 终态） |

锚点：声明插在 `configLanguage[MAX_CONFIG_STRING]` 行**之后**，配置表条目插在 `{.name = "force_4by3"` 行**之前**。这两个子串在三份 `configfile.c` 里各只出现一次（脚本里用 `grep -F` 固定子串而不是正则——`\{` 在 BRE 里是区间开头，GNU grep 会直接报「不匹配的 \{」）。

`configfile.c` 仍留在 `--exclude` 列表里（`0001-build-and-config.patch` 在它上面的 hunk 上下文确实对不上），只是不再由补丁提供终态。

### 上游自带差异、但不进平台补丁

`src/pc/configfile.h`（`0001-build-and-config.patch`）和 `src/pc/lua/utils/smlua_misc_utils.c`（`0006-asset-overlay.patch`）在两个 Android 上游与 PC 基线内容不同，但这两个 hunk 的上下文没被破坏，能直接应用。应用后它们等于「Android 上游 + PC 改动」，与 PC 终态不完全相同是**预期**的；已验证新增的 10 行在这两个文件里一行不缺（含 `cn_tex_gauge_dy()`）。

汉化版 Android 自带的 `palettes/*.ini`、`src/pc/djui/*_glyphs.h`、`textures/custom_font/*.png` 属于上游本身，补丁不改动、也不收录。

## cn_extra/ 是「新增文件」，只补缺不覆盖

`cn_extra/` 收录 19 个上游不存在的文件，其中 **`lang/Chinese.ini`** 在 PC 上游 `lang/` 里没有（只有 11 种语言；汉化版 Android 则有自己的一份）。因此应用时逐个文件判断：目标已存在就跳过，只补缺。

```bash
# scripts/apply_patches.sh 内部语义
[ -e "$target/$rel" ] && 保留上游已有的 $rel || 复制
```

这条不是多余的保险：汉化版 Android 自带的 `lang/Chinese.ini` 与 PC 那份内容并不完全相同（它有自己的触屏相关条目），整目录 `cp -R` 会把它顶掉。已用哨兵内容实测：应用后上游内容保留，只在 `DISABLE_POPUPS` 后多出一行。原版 Android 上游没有 `lang/Chinese.ini`，于是直接拿到 PC 那份——它的触屏相关字符串会回落到英文，这是可接受的取舍。

## djui_modern 兼容

[`Shangshanruo666/djui_modern`](https://github.com/Shangshanruo666/djui_modern)（`upstream.txt` 的 `djmd_commit`）是一套挂在 `src/pc/djui_modern` 下、并向上游 djui 文件打 9 个桌面补丁 / 10 个 Android 补丁的模块。它和本仓库的补丁**文件级没有重叠**：它改 `djui.c`、`djui_rect.c`、`djui_gfx.c`、`djui_hud_utils.c`、`djui_unicode.c`、`djui_text.c`、`djui_panel_host_mods.c`、`gfx_dxgi.cpp`，我们改 `djui_panel_confirm.[ch]` 和 `djui_panel_misc.c`。

两边都会碰的只有构建文件，且位置互不相邻、能带偏移应用：

| 文件 | 我们 | djui_modern | 结果 |
| --- | --- | --- | --- |
| `Makefile` | `~L194` 加 `CN_TEXT_PACK`、`~L454` 加 flip guard | `~L470` 的 `SRC_DIRS` 行插 `src/pc/djui_modern` | 可应用（上下文不重叠） |
| `Android.mk` | `~L106` 的 `LOCAL_CFLAGS` 后加 `SM64COOPDX_CN_TEXT` | `~L70` 的 `SRC_DIRS` 行插 `src/pc/djui_modern` | 可应用 |

实测（基线取自 `upstream.txt`，桌面双向、Android 两个上游各一遍）：`0001`~`0008` 全部补丁与 djui_modern 的 9 个桌面补丁、2 个 Android 构建补丁，**正向反向都干净应用，零冲突**。

**补丁不冲突，但模块本身有硬依赖**：`djui_modern_font_atlas.c` 里有一行

```c
static const DjuiModernCnGlyph kCnGlyphs[] = {
#include "pc/djui/cn_glyphs.h"
};
```

它把 `src/pc/djui/cn_glyphs.h` 当**数组初始化列表**包进来，而这个文件是**汉化上游自带的**（6790 行 GB2312 表）。所以在 **PC 原版 / Android 原版** 上装 djui_modern，`#include` 找不到文件 → 编译直接失败；只有两个汉化上游能编过。

**本仓库不收录这份字表。** 曾经在 `cn_extra/` 里放过一份由 iQue 字表（`cn_map.json`，与 `cn_data/cn_glyphs_16x16.h` 同一张表）生成的 1061 行替代品，现在删掉了：

- djui_modern 作者（上善若）已说明后续会自己向原版上游兼容，届时这个文件要么进模块仓库、要么由构建脚本判断，总之不该由**补丁仓库**替它提供；我们收录只会和它将来自己的方案打架；
- 那份表是 iQue 字表的子集，只在本项目的语境下成立，放到别人的原版上游上没有意义。

因此现在 `install.sh` 是**硬拒绝**而不是想办法补齐：

```bash
$ sh install.sh -d -p zip
[!] djui_modern 只能配汉化上游：它的字体图集里 #include "pc/djui/cn_glyphs.h"，
[!] 这个文件由汉化上游（PC cn-repo / Android android-cn-repo）自带。
[!] 本项目不再收录该字表（等 djui_modern 上游自己兼容原版上游）。
[x] 请加 -c 使用汉化上游，或去掉 -d/--djmd。
```

判据是 `-d` 必须配 `-c`（交互向导里也只在选了汉化上游后才会问 djui_modern）。等 djui_modern 上游合并了原版兼容，这条限制可以直接删掉；在那之前，原版上游想用这个模块只能自己准备 `src/pc/djui/cn_glyphs.h`。

汉化上游自带的那份 6790 行表原样保留：`cn_extra/` 只补缺不覆盖，也不会去碰它。

## 应用

统一走应用器，重复执行安全（已应用的补丁会被识别并跳过，已存在的字符串也跳过）：

```bash
scripts/apply_patches.sh <pc|android|android-cn> <上游干净 checkout> [--frontend]
```

内部顺序：

| 目标 | 顺序 |
| --- | --- |
| `pc` | `0001`~`0006`（编号序）→ [`0007`] → `cn_extra/`（补缺）→ `add_lang_strings.sh` |
| `android` | `0001`~`0006`（跳过 3 个漂移文件）→ `0008` → [`0007`] → `cn_extra/`（补缺）→ `add_cn_config.sh` → `add_lang_strings.sh` |
| `android-cn` | 同 `android`（**同一份** `0008-android.patch`，只差上游自带文件） |

`[]` 表示仅 `--frontend` 时应用。目录名带序号 ⇒ 同一目录内的补丁按文件名排序，跨目录按目录名排序，顺序唯一。语言脚本**必须放在 `cn_extra/` 之后**：PC 的 `lang/Chinese.ini` 就是 `cn_extra/` 带过来的。**不要**用 `cp -R src/.` 覆盖终态——那会无条件带上前端，也会把 Android 的平台差异冲掉。

排除列表同时存在于 `scripts/gen_platform_patches.py` 的 `EXCLUDED` 和 `scripts/apply_patches.sh`，生成脚本会断言两者完全一致，**改一处必须改另一处**。

## 重新生成

```bash
# 1. 改动 src/ 后，重生成共用的 0001~0007 全部分组
sh scripts/gen_all_patches.sh <PC 8cd6e59 基线目录>

# 2. 重生成 0008-android.patch（Android 上游 checkout 见 upstream.txt）
python3 scripts/gen_platform_patches.py \
    --pc-base    <PC 8cd6e59 基线目录> \
    --android    <ManIsCat2 上游目录> \
    --android-cn <汉化版 Android 上游目录>
```

第 1 步会断言 `0001`~`0007` 恰好覆盖 `src/` 的全部差异（无遗漏、无重复）、都不含 `lang/`、且 `patches/` 下没有旧的分组目录与旧单文件补丁残留。第 2 步自检：`0001`~`0006`（跳过 3 个漂移文件）+ 平台补丁 + `0007` + 两个插入脚本叠加后与三方合并终态逐字节一致（`android` / `android-cn` 各 31 文件），语言字符串齐全，两个脚本复跑无改动（幂等），不应用 `0007` 时 3 个前端文件保持上游原版，平台补丁与 `0001`~`0006` 不重叠。**生成器会逐文件比对两个上游算出来的平台增量，一旦不同就直接报错**——那说明两个上游真的分道扬镳，得退回「每目标一份补丁」，而不是让共享补丁静默只对其中一个有效。

语言字符串改了文案、或中文配置项改了名字时不用重生成任何补丁，只改 `scripts/add_lang_strings.sh` / `scripts/add_cn_config.sh` 里对应的常量。

**手改 `0008-android.patch` 之后一定要跑 `python3 scripts/check_patch_integrity.py`。** 本机没有那两个 Android 基线时，第 2 步跑不了，改 PC 侧的 `pc_main.c` 往往只能手工同步到平台补丁里——而这里有两个不会在 PC 构建里暴露的坑：hunk 头里 `+c,d` 的计数忘了跟着改（`git apply` 报 corrupt patch，报错行号离真正出错的位置很远），以及删掉 `cn_extra/` 里某个头文件后平台补丁还留着 `#include`（PC 目标根本不走那个分支，Android CI 才炸）。这个脚本不需要任何基线就能把两种都拦住。

**但它拦不住第三种：hunk 的上下文在上游里根本不存在。** 那一类的症状是 CI 跑到「4/6 应用补丁」直接 `0008-android.patch 无法干净应用，上游不是 upstream.txt 声明的基线？`，而本机 PC 构建一切正常（PC 目标压根不应用 `0008`）。只要网络可用，先跑一遍

```bash
sh scripts/preflight_android.sh android-cn --frontend
```

它按 `upstream.txt` 声明的 repo/commit 现拉一份浅克隆（临时目录默认 `/var/tmp`，可用 `PREFLIGHT_TMP` 覆盖——别用 `/tmp`，那常常是小容量 tmpfs），跑真实的 `apply_patches.sh`，跑完即删。`0008` 里每个文件的 hunk 都是按**真实基线**的上下文写的：`Android.mk` 的注入点紧接 `LOCAL_CFLAGS :=` 那一行之后（与生成器 `inject_android_mk()` 一致，两个 Android 上游这一带的 113 行文件逐字节相同），`Makefile` / `pc_main.c` 是三方合并出来的终态。上游改了这三个文件就得重跑第 2 步，不要手改 hunk。
