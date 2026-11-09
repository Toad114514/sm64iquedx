#!/usr/bin/env python3
"""校验 .github/workflows/build.yml 的编译选项拼装结果。

workflow_dispatch 的三个输入（上游 / UIPatch 前端 / djui_modern）要拼成
`sh install.sh <args>` 的一串开关，而 GitHub 表达式有两个特别容易踩的坑：

  1. `cond && 'a' || 'b'` 里，真分支如果写成空字符串，`true && ''` 得到**假值**，
     会被 `||` 吃掉 ⇒ 结果永远是 'b'。本文件按这条规则检查真分支非空。
  2. boolean 输入的 false 在表达式里是假值，`inputs.frontend || 'true'` 会得到
     'true'；必须用 `github.event.inputs.*`（永远是字符串）。

这里不靠脑内推演：直接读 build.yml 里的原文，把 `${{ }}` 按 GH 语义求值
（`&&`→Python `and`、`||`→`or`，两者同样返回操作数、优先级也一致），对比期望。

用不上就跳过：没有 PyYAML 时打印提示并返回 0，不让 CI 因为缺依赖变红。
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WF = os.path.join(REPO, ".github", "workflows", "build.yml")

try:
    import yaml
except ImportError:
    print("  跳过（没有 PyYAML；apt install python3-yaml 后可运行）")
    sys.exit(0)

EXPR = re.compile(r"\$\{\{(.*?)\}\}", re.S)
# 每个 job 的编译步骤都必须走同一份拼装结果
WANT_USES = 5  # linux / linux-arm / windows / macos / android

fail = 0


def bad(msg):
    global fail
    fail += 1
    print("  FAIL " + msg)


raw = open(WF, encoding="utf-8").read()
doc = yaml.safe_load(raw)
# YAML 1.1 把裸 `on:` 解析成布尔 True
on = doc.get("on") or doc.get(True) or {}
inputs = (on.get("workflow_dispatch") or {}).get("inputs") or {}

print("  输入：", ", ".join(f"{k}({v.get('type')},{v.get('default')!r})" for k, v in inputs.items()))
for want in ("upstream", "frontend", "djui_modern"):
    if want not in inputs:
        bad(f"缺 workflow_dispatch 输入 {want}")
if inputs.get("upstream", {}).get("type") != "choice":
    bad("upstream 应该是 choice（原版/汉化版），否则用户没法从下拉里选")

tpl = (doc.get("env") or {}).get("IQDX_BUILD_ARGS")
if not tpl:
    bad("env.IQDX_BUILD_ARGS 不存在：各 job 就没法共用同一份拼装逻辑")
    sys.exit(1)


def eval_args(event_inputs):
    """event_inputs 缺的键当空字符串 —— push / pull_request 下就是这个样子。"""
    out, pos = [], 0
    for m in EXPR.finditer(tpl):
        out.append(tpl[pos:m.start()])
        body = re.sub(
            r"github\.event\.inputs\.([A-Za-z0-9_]+)",
            lambda mm: repr(event_inputs.get(mm.group(1), "")),
            m.group(1),
        )
        body = body.replace("&&", " and ").replace("||", " or ")
        val = eval(body, {"__builtins__": {}}, {})  # noqa: S307 - 只喂本文件的表达式
        out.append("" if val is False or val is None else str(val))
        pos = m.end()
    out.append(tpl[pos:])
    return " ".join("".join(out).split())


def opts(upstream="汉化版", frontend="true", djui="true"):
    return {"upstream": upstream, "frontend": frontend, "djui_modern": djui}


CASES = [
    ("push/PR（无 inputs）→ 默认组合", {}, "-c -f -d"),
    ("汉化版 + 前端 + djmd", opts(), "-c -f -d"),
    ("原版 + 都不带", opts("原版", "false", "false"), ""),
    ("原版 + 只前端", opts("原版", "true", "false"), "-f"),
    ("原版 + 只 djmd", opts("原版", "false", "true"), "-d"),
    ("汉化版 + 只 djmd", opts("汉化版", "false", "true"), "-c -d"),
    ("汉化版 + 只前端", opts("汉化版", "true", "false"), "-c -f"),
]
for name, inp, want in CASES:
    got = eval_args(inp)
    if got == want:
        print(f"  ok   {name}: {got!r}")
    else:
        bad(f"{name}: {got!r}，期望 {want!r}")

# 这三个开关只能由 IQDX_BUILD_ARGS 提供，job 里再写死 -c/-f/-d 就会和用户的选择打架。
# 只看 install.sh **之后**的参数：`su builder -c "sh install.sh ..."` 里的 -c 是 su 的。
for ln in raw.splitlines():
    s = ln.strip()
    if s.startswith("#") or "install.sh" not in s:
        continue
    tail = s.split("install.sh", 1)[1]
    if "IQDX_BUILD_ARGS" in tail:
        continue
    for flag in ("-c", "-f", "-d"):
        if re.search(r"(^|\s)" + re.escape(flag) + r"(\s|\"|$)", tail):
            bad(f"job 里还写死了 {flag}，用户的选择会被覆盖：{s}")

# 注释里也提到这条命令，所以注释行不算
uses = sum(
    1
    for ln in raw.splitlines()
    if not ln.strip().startswith("#")
    and re.search(r"install\.sh(\s+-a)?\s+\$\{IQDX_BUILD_ARGS\}", ln)
)
if uses != WANT_USES:
    bad(f"引用 IQDX_BUILD_ARGS 的编译步骤有 {uses} 处，应为 {WANT_USES}")
else:
    print(f"  ok   5 个 job 共用同一份编译选项")

sys.exit(1 if fail else 0)
