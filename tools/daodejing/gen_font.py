#!/usr/bin/env python3
"""Generate the application's Chinese LVGL font subsets.

The glyph inventory is derived from the sources, never hand-maintained:

  * every character of the chapter sources under ``tools/daodejing/chapters/``
    (through ``content.content_characters()`` -- the same module that feeds
    ``gen_content.py``, so the tables and the font can never disagree)
  * every non-ASCII character inside a string literal of ``main/*.c|*.h``
    (comments are stripped first, so Chinese comments cost no flash)
  * printable ASCII and the punctuation the UI itself draws

Every code point is verified against the source font before conversion, so a
missing glyph becomes a build-time error instead of a blank box on the device.

Requirements: ``lv_font_conv`` (npm, pinned version below) and ``fonttools``.
Source font: Noto Sans CJK SC Regular (SIL Open Font License 1.1).

Usage (from the repository root):
    python3 tools/daodejing/gen_font.py            # generate all sizes
    python3 tools/daodejing/gen_font.py --check    # verify inventory + source font
    python3 tools/daodejing/gen_font.py --download # fetch the source font first

Environment overrides:
    DDJ_SOURCE_FONT   path to the source OTF
    LV_FONT_CONV      path to the lv_font_conv entry script
    NODE_EXE          node executable to run the converter with
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

import content  # noqa: E402  (path set up above)

ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main"
FONT_DIR = ROOT / "assets" / "fonts"
CHARSET = FONT_DIR / "charset.txt"

# 生成的 C 表与它的头文件已经由章节源覆盖，扫它们只会重复劳动。
SKIP_SOURCES = {"ddj_text.c", "ddj_text.h"}

SOURCE_FONT_NAME = "NotoSansCJKsc-Regular.otf"
SOURCE_FONT_URL = (
    "https://raw.githubusercontent.com/notofonts/noto-cjk/main/"
    "Sans/OTF/SimplifiedChinese/" + SOURCE_FONT_NAME
)
SOURCE_FONT_SHA256 = "2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b"
SOURCE_FONT_LICENSE = "SIL Open Font License 1.1"

DEFAULT_SOURCE_FONT = Path("D:/esp/fontsrc") / SOURCE_FONT_NAME
LV_FONT_CONV_VERSION = "1.5.3"

# Generated font sizes. 16 = captions and the 参究 question, 24 = body and
# options, 32 = the 经文 itself. These three numbers also appear in
# main/ddj_ui.h as the per-line character budgets -- changing a size here
# without changing the budgets there breaks the layout arithmetic.
SIZES = (16, 24, 32)
BPP = 4

# Printable ASCII plus the symbols the UI draws itself. The CJK punctuation is
# already picked up from the chapter text, but keeping it here means a chapter
# edit that drops a punctuation mark cannot silently shrink the font.
BASE_RANGES = ((0x20, 0x7E),)
EXTRA_SYMBOLS = "·，。！？、：；（）「」《》…—"


def strip_comments(text: str) -> str:
    """Remove // and /* */ comments while keeping string literals intact."""
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
    """Non-ASCII characters used inside string literals of the application."""
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
        chars.update(content.content_characters())
    except content.ContentError as error:
        raise SystemExit(f"content error: {error}")
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
        "# Glyph inventory for the 道德经日课 font subsets.",
        "# Generated by tools/daodejing/gen_font.py -- do not edit by hand.",
        f"# source font: {SOURCE_FONT_NAME} ({SOURCE_FONT_LICENSE})",
        f"# total code points: {len(chars)}  (CJK {len(cjk)}, other {len(others)})",
        "",
        "# --- CJK ideographs ---",
        "".join(cjk),
        "",
        "# --- everything else ---",
        "".join(others),
        "",
    ]
    return "\n".join(lines)


def download_source_font(target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    print(f"downloading {SOURCE_FONT_URL}")
    with urllib.request.urlopen(SOURCE_FONT_URL, timeout=180) as response:
        data = response.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != SOURCE_FONT_SHA256:
        raise SystemExit(
            f"source font hash mismatch: got {digest}, expected {SOURCE_FONT_SHA256}"
        )
    target.write_bytes(data)
    print(f"wrote {target.relative_to(ROOT)} ({len(data)} bytes)")


def resolve_source_font(explicit: str | None, download: bool) -> Path:
    path = Path(explicit or os.environ.get("DDJ_SOURCE_FONT") or DEFAULT_SOURCE_FONT)
    if download or not path.is_file():
        if not download:
            raise SystemExit(
                f"source font not found: {path}\n"
                f"Run with --download, or pass --font <path>, or set DDJ_SOURCE_FONT.\n"
                f"Expected SHA-256: {SOURCE_FONT_SHA256}\n"
                f"Expected licence: {SOURCE_FONT_LICENSE}"
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
        "lv_font_conv not found. Install it with:\n"
        f"  npm install lv_font_conv@{LV_FONT_CONV_VERSION}\n"
        "then pass its path through the LV_FONT_CONV environment variable."
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
        "--lv-font-name", f"ddj_font_{size}",
        "-o", str(output),
    ]
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout[-4000:], file=sys.stderr)
        print(result.stderr[-4000:], file=sys.stderr)
        raise SystemExit(f"lv_font_conv failed for size {size}")
    if result.stdout.strip():
        print(result.stdout.strip())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", help="path to the source OTF")
    parser.add_argument("--download", action="store_true",
                        help="download the source font first")
    parser.add_argument("--check", action="store_true",
                        help="verify the inventory and the source font, do not convert")
    args = parser.parse_args()

    chars = build_inventory()
    symbols = "".join(chars)
    print(f"glyph inventory: {len(chars)} code points")

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
            f"{len(missing)} code point(s) absent from {source_font.name}: {preview}\n"
            "A chapter edit or a UI string uses a glyph the source font does not provide."
        )
    print(f"coverage: {len(chars)}/{len(chars)} code points present in {source_font.name}")

    if args.check:
        return 0

    command = resolve_converter()
    for size in SIZES:
        output = FONT_DIR / f"ddj_font_{size}.c"
        run_converter(command, source_font, symbols, size, output)
        print(f"wrote: {output.relative_to(ROOT)} ({output.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
