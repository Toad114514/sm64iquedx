# 补丁索引

**所有补丁平铺在 `patches/` 根目录，不建子目录。** 四套上游共用 `0001` / `0002` / `cn_extra/`；Android 只额外叠加一份平台增量。语言字符串**不是补丁**，由 `scripts/add_lang_strings.sh` 在应用阶段插入。

| 补丁 | 文件数 | 适用目标 | 作用 |
| --- | --- | --- | --- |
| `0001-src-overlay.patch` | 25 | `pc` / `android` / `android-cn` 共用 | 必选，核心代码改动。**不含任何 `lang/*.ini`，也不含 DJUI 前端** |
| `0002-djui-frontend.patch` | 1 | 三目标共用 | 可选，设置 → 其他 的「中文字库」开关 |
| `0005-android.patch` | 4 | `android` 专用 | 必选，Android 平台增量 |
| `0006-android-cn.patch` | 4 | `android-cn` 专用 | 必选，Android 平台增量 |
| `cn_extra/` | 13 | 三目标共用 | 上游不存在的新增文件，仅补缺、不覆盖 |

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

`0001` 因此把整个 `lang/` 前缀排除掉（由生成器保证，不靠应用器的 `--exclude`）：这样主补丁在四套上游上都不碰 ini，语言相关的东西只有这一处。

> 这两条字符串即使不打可选的 `0002` 也无害（只是多/少一个被 DLANG 引用的条目）。

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

汉化版 Android 自带的 `palettes/*.ini`、`src/pc/djui/*_glyphs.h`、`textures/custom_font/*.png` 属于上游本身，补丁不改动、也不收录。

## cn_extra/ 是「新增文件」，只补缺不覆盖

`cn_extra/` 收录 13 个上游不存在的文件，其中包括 **`lang/Chinese.ini`**（PC 上游 `lang/` 只有 11 种语言，没有中文；汉化版 Android 则有自己的一份）。因此应用时逐个文件判断：目标已存在就跳过，只补缺。

```bash
# scripts/apply_patches.sh 内部语义
[ -e "$target/$rel" ] && 保留上游已有的 $rel || 复制
```

这条不是多余的保险：汉化版 Android 自带的 `lang/Chinese.ini` 与 PC 那份内容并不完全相同（它有自己的触屏相关条目），整目录 `cp -R` 会把它顶掉。已用哨兵内容实测：应用后上游内容保留，只在 `DISABLE_POPUPS` 后多出一行。原版 Android 上游没有 `lang/Chinese.ini`，于是直接拿到 PC 那份——它的触屏相关字符串会回落到英文，这是可接受的取舍。

## 应用

统一走应用器，重复执行安全（已应用的补丁会被识别并跳过，已存在的字符串也跳过）：

```bash
scripts/apply_patches.sh <pc|android|android-cn> <上游干净 checkout> [--frontend]
```

内部顺序：

| 目标 | 顺序 |
| --- | --- |
| `pc` | `0001` → [`0002`] → `cn_extra/`（补缺）→ `add_lang_strings.sh` |
| `android` | `0001`（跳过 3 个漂移文件）→ `0005` → [`0002`] → `cn_extra/`（补缺）→ `add_lang_strings.sh` |
| `android-cn` | `0001`（跳过 3 个漂移文件）→ `0006` → [`0002`] → `cn_extra/`（补缺）→ `add_lang_strings.sh` |

`[]` 表示仅 `--frontend` 时应用。语言脚本**必须放在 `cn_extra/` 之后**：PC 的 `lang/Chinese.ini` 就是 `cn_extra/` 带过来的。**不要**用 `cp -R src/.` 覆盖终态——那会无条件带上前端，也会把 Android 的平台差异冲掉。

排除列表同时存在于 `scripts/gen_platform_patches.py` 的 `EXCLUDED` 和 `scripts/apply_patches.sh`，生成脚本会断言两者完全一致，**改一处必须改另一处**。

## 重新生成

```bash
# 1. 改动 src/ 后，重生成共用的 0001 / 0002
sh scripts/gen_all_patches.sh <PC 8cd6e59 基线目录>

# 2. 重生成 0005 / 0006（Android 上游 checkout 见 upstream.txt）
python3 scripts/gen_platform_patches.py \
    --pc-base    <PC 8cd6e59 基线目录> \
    --android    <ManIsCat2 上游目录> \
    --android-cn <汉化版 Android 上游目录>
```

第 1 步会断言 `0001` 里既没有前端文件也没有 `lang/`，`0002` 只含那一个前端文件。第 2 步自检：`0001`（跳过 3 个漂移文件）+ 平台补丁 + `0002` + `add_lang_strings.sh` 叠加后与三方合并终态逐字节一致（`android` 28 文件 / `android-cn` 28 文件），语言字符串齐全，语言脚本复跑无改动（幂等），不应用 `0002` 时前端文件保持上游原版，平台补丁与 `0001` 不重叠。

语言字符串改了文案时不用重生成任何补丁，只改 `scripts/add_lang_strings.sh` 里的两处常量。
