#!/usr/bin/env python3
"""生成本应用的中文 LVGL 字库子集。

字符清单是**推导出来的，从不手工维护**，来源有三处：

  * 题库源文件里的每一个字（通过 ``content.content_characters()`` —— 与
    ``gen_content.py`` 用的是同一个模块，所以 C 表与字库不可能各说各话）
  * ``main/*.c|*.h`` 的字符串字面量里的每一个非 ASCII 字符
    （先剥掉注释，所以中文注释不占 Flash）
  * 可打印 ASCII 与界面自己绘制的标点

每个码点在转换前都会对着母字体核对一次，所以「漏字」是构建期错误，而不是设备上
的一个空白框。

依赖：``lv_font_conv``（npm，下面锁了版本）与 ``fonttools``。
母字体：Noto Sans CJK SC Regular（SIL Open Font License 1.1）。

用法（在仓库根目录）：
    python3 tools/qiaopi/gen_font.py            # 生成三档字库
    python3 tools/qiaopi/gen_font.py --check    # 只核对清单与母字体覆盖，不转换
    python3 tools/qiaopi/gen_font.py --download # 先下载母字体

环境变量覆盖：
    QPQ_SOURCE_FONT   母字体 OTF 路径
    LV_FONT_CONV      lv_font_conv 入口脚本路径
    NODE_EXE          用来运行转换器的 node 可执行文件
"""
from __future__ import annotations

import argparse
import hashlib
import os
import re
import subprocess
import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import content  # noqa: E402  （上面的 sys.path 已经指到同目录）

ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main"
FONT_DIR = ROOT / "assets" / "fonts"
CHARSET = FONT_DIR / "charset.txt"

# 生成物：题库 C 表已经由题库源覆盖，扫它只会重复劳动；音频头文件同理
# （它里面只有宏，没有中文）。
SKIP_SOURCES = {"qpq_text.c", "qpq_text.h", "qpq_audio.h"}

SOURCE_FONT_NAME = "NotoSansCJKsc-Regular.otf"
SOURCE_FONT_URL = (
    "https://raw.githubusercontent.com/notofonts/noto-cjk/main/"
    "Sans/OTF/SimplifiedChinese/" + SOURCE_FONT_NAME
)
SOURCE_FONT_SHA256 = "2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b"
SOURCE_FONT_LICENSE = "SIL Open Font License 1.1"

DEFAULT_SOURCE_FONT = Path("D:/esp/fontsrc") / SOURCE_FONT_NAME
LV_FONT_CONV_VERSION = "1.5.3"

# 三档字号。16 = 顶栏、出处、结算数据与评语；24 = 句子、候选、解析、完整原文；
# 32 = 标题页主标题与结算评级。这三个数字同时出现在 main/qpq_ui.h 的每行字数
# 预算里 —— 只改这里不改那里，版式算术就断了。
SIZES = (16, 24, 32)
BPP = 4

# 可打印 ASCII，加上界面自己绘制的符号。中文标点本来会从题库文本里收到，
# 但这里再列一遍：题库改动若恰好删掉某个标点，字库不会因此悄悄缩水。
BASE_RANGES = ((0x20, 0x7E),)
EXTRA_SYMBOLS = "·，。！？、：；（）「」《》…—＿"


def strip_comments(text: str) -> str:
    """去掉 // 与 /* */ 注释，同时保持字符串字面量原样。"""
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == '"':
            out.append(ch)
            i += 1
            while i < n:
                out.append(text[i])
                if text[i] == "\\" and i + 1 < n:
                    out.append(text[i + 1])
                    i += 2
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            continue
        if ch == "'":
            out.append(ch)
            i += 1
            while i < n:
                out.append(text[i])
                if text[i] == "\\" and i + 1 < n:
                    out.append(text[i + 1])
                    i += 2
                    continue
                if text[i] == "'":
                    i += 1
                    break
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out)


STRING_LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def source_characters() -> set[str]:
    """应用字符串字面量里用到的非 ASCII 字符。"""
    chars: set[str] = set()
    for path in sorted(list(MAIN.glob("*.c")) + list(MAIN.glob("*.h"))):
        if path.name in SKIP_SOURCES:
            continue
        text = strip_comments(path.read_text(encoding="utf-8"))
        for literal in STRING_LITERAL_RE.findall(text):
            chars.update(c for c in literal if ord(c) > 0x7F)
    return chars


def build_inventory() -> list[str]:
    chars: set[str] = set()
    try:
        chars.update(content.content_characters(content.load_bank()))
    except content.ContentError as error:
        raise SystemExit(f"题库源文件错误：{error}")
    chars.update(source_characters())
    for start, end in BASE_RANGES:
        chars.update(chr(c) for c in range(start, end + 1))
    chars.update(EXTRA_SYMBOLS)
    chars.discard("\n")
    chars.discard("\r")
    chars.discard("\t")
    return sorted(chars)


def load_cmap(path: Path) -> set[int]:
    from fontTools.ttLib import TTFont

    font = TTFont(str(path), fontNumber=0, lazy=True)
    cmap: set[int] = set()
    for table in font["cmap"].tables:
        cmap |= set(table.cmap.keys())
    font.close()
    return cmap


def describe(chars: list[str]) -> str:
    cjk = [c for c in chars if 0x4E00 <= ord(c) <= 0x9FFF]
    others = [c for c in chars if not (0x4E00 <= ord(c) <= 0x9FFF)]
    lines = [
        "# 侨批 · 填字问答 的字符清单。",
        "# 由 tools/qiaopi/gen_font.py 生成，请勿手改。",
        f"# 母字体：{SOURCE_FONT_NAME}（{SOURCE_FONT_LICENSE}）",
        f"# 码点总数：{len(chars)}（汉字 {len(cjk)}，其它 {len(others)}）",
        "",
        "# --- 汉字 ---",
        "".join(cjk),
        "",
        "# --- 其它（ASCII、中文标点、空格位用的全角下划线）---",
        "".join(others),
        "",
    ]
    return "\n".join(lines)


def download_source_font(target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    print(f"下载 {SOURCE_FONT_URL}")
    with urllib.request.urlopen(SOURCE_FONT_URL, timeout=180) as response:
        data = response.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != SOURCE_FONT_SHA256:
        raise SystemExit(f"母字体哈希不符：得到 {digest}，期望 {SOURCE_FONT_SHA256}")
    target.write_bytes(data)
    print(f"wrote {target.relative_to(ROOT)}（{len(data)} 字节）")


def resolve_source_font(explicit: str | None, download: bool) -> Path:
    path = Path(explicit or os.environ.get("QPQ_SOURCE_FONT") or DEFAULT_SOURCE_FONT)
    if download or not path.is_file():
        if not download:
            raise SystemExit(
                f"找不到母字体：{path}\n"
                f"用 --download 下载，或用 --font <path>，或设 QPQ_SOURCE_FONT。\n"
                f"期望 SHA-256：{SOURCE_FONT_SHA256}\n"
                f"期望许可：{SOURCE_FONT_LICENSE}"
            )
        download_source_font(path)
    return path


def resolve_converter() -> list[str]:
    explicit = os.environ.get("LV_FONT_CONV")
    if explicit:
        return [explicit]
    candidates = [
        Path("D:/esp/fonttools/node_modules/lv_font_conv/lv_font_conv.js"),
        ROOT / "node_modules" / "lv_font_conv" / "lv_font_conv.js",
    ]
    node = os.environ.get("NODE_EXE", "node")
    for candidate in candidates:
        if candidate.is_file():
            return [node, str(candidate)]
    raise SystemExit(
        "找不到 lv_font_conv。安装方式：\n"
        f"  npm install lv_font_conv@{LV_FONT_CONV_VERSION}\n"
        "然后用 LV_FONT_CONV 环境变量把它的路径传进来。"
    )


def run_converter(command: list[str], source_font: Path, symbols: str, size: int,
                  output: Path) -> None:
    args = command + [
        "--font", str(source_font),
        "--symbols", symbols,
        "--size", str(size),
        "--bpp", str(BPP),
        "--format", "lvgl",
        "--no-compress",
        "--lv-include", "lvgl.h",
        "--lv-font-name", f"qpq_font_{size}",
        "-o", str(output),
    ]
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout[-4000:], file=sys.stderr)
        print(result.stderr[-4000:], file=sys.stderr)
        raise SystemExit(f"lv_font_conv 在字号 {size} 上失败")
    if result.stdout.strip():
        print(result.stdout.strip())


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--font", help="母字体 OTF 路径")
    parser.add_argument("--download", action="store_true", help="先下载母字体")
    parser.add_argument("--check", action="store_true",
                        help="只核对清单与母字体覆盖，不转换")
    args = parser.parse_args()

    chars = build_inventory()
    symbols = "".join(chars)
    print(f"字符清单：{len(chars)} 个码点")

    FONT_DIR.mkdir(parents=True, exist_ok=True)
    content_text = describe(chars)
    if CHARSET.is_file() and CHARSET.read_text(encoding="utf-8") == content_text:
        print(f"unchanged: {CHARSET.relative_to(ROOT)}")
    elif args.check:
        print(f"STALE: {CHARSET.relative_to(ROOT)}", file=sys.stderr)
    else:
        CHARSET.write_text(content_text, encoding="utf-8", newline="\n")
        print(f"wrote: {CHARSET.relative_to(ROOT)}")

    source_font = resolve_source_font(args.font, args.download)
    cmap = load_cmap(source_font)
    missing = [c for c in chars if ord(c) not in cmap]
    if missing:
        preview = "".join(missing[:40])
        raise SystemExit(
            f"{len(missing)} 个码点在 {source_font.name} 里没有：{preview}\n"
            "要么是题库改动引入了母字体不提供的字，要么是某条界面文案用了生僻字。"
        )
    print(f"覆盖：{len(chars)}/{len(chars)} 个码点都在 {source_font.name} 里")

    if args.check:
        return 0

    command = resolve_converter()
    for size in SIZES:
        output = FONT_DIR / f"qpq_font_{size}.c"
        run_converter(command, source_font, symbols, size, output)
        print(f"wrote: {output.relative_to(ROOT)}（{output.stat().st_size} 字节）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
