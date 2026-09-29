<p align="right">
  <a href="cjk-font-subsetting-for-lvgl.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Cutting a CJK Font Subset for LVGL

Written while building the [San Zi Jing kids game](sanzijing-kids-game/README.md),
a fully offline Chinese learning game. These notes apply to any AI Passport
application whose UI shows Chinese text: the numbers below are measured on an
ESP32-C3 with 8 MB Flash and no PSRAM.

## Why a subset is unavoidable

LVGL ships Montserrat, which is Latin-only, and an optional Source Han subset
that is off by default. Setting a Chinese string on a widget whose font has no
CJK glyphs does not raise an error — the label simply renders blank or draws
tofu boxes. LVGL has no fallback chain, so **every widget that shows Chinese must
be given a font that contains those characters**.

For a game built on the *Three Character Classic* that means roughly 700
distinct ideographs, so the font has to be cut down. The three subsets in this
project cover **785 code points** in total.

## Derive the glyph inventory; never hand-list it

A hand-maintained character list goes stale the first time somebody adds a menu
label, and the failure is silent. Instead, generate the inventory by unioning
three sources:

1. **Every character of the curriculum text**, taken from the text file the rest
   of the build already treats as the source of truth. Read it as characters on
   a copy with all whitespace removed — splitting on whitespace yields
   three-character strings, not single ideographs, and the subset quietly ends up
   containing nothing.
2. **Every non-ASCII character inside a string literal** in the application
   sources. Strip `//` and `/* */` comments *before* scanning, so Chinese
   comments cost no Flash; keep string literals intact, including escapes.
3. **Printable ASCII**, plus the Chinese punctuation and symbols the UI draws on
   its own: `，。！？、：；（）「」《》…—·""''★☆`. Star symbols matter here — a
   five-star rating built from a text string needs glyphs for the stars.

That produced 785 code points: 668 ideographs and 117 characters from the other
two groups. Writing it out to a checked-in `charset.txt` gives reviewers
something to diff and gives the next person a way to see *why* a glyph is in
there.

## Verify coverage before converting

The single highest-value step: load the source font's character map and check
every code point in the inventory against it, then fail the build if any are
missing.

```python
from fontTools.ttLib import TTFont

font = TTFont(source_font, fontNumber=0, lazy=True)
cmap: set[int] = set()
for table in font["cmap"].tables:
    cmap |= set(table.cmap.keys())
missing = [c for c in chars if ord(c) not in cmap]
```

This matters because expanded editions of the text use rare characters. In this
project the hard cases were eleven rare ideographs — U+7C5D, U+81BA, U+608C,
U+83FD, U+530F, U+66F7, U+6A50, U+704F, U+97EB, U+7F0C, and U+7881 (the Chinese
page of this entry names them). A
converter given a character the font lacks does not necessarily stop; it can emit
a font that is missing that glyph, and you discover it as one blank square in the
middle of a lesson, on the device. Checking first turns that into a build-time
error with the offending characters printed. Noto Sans CJK SC covered all 785.

## Convert with the character set as a literal string

Pass the inventory through `--symbols` as one literal string rather than
describing ranges. Ranges drag in thousands of unused ideographs; the whole point
is to pay only for what is displayed.

```bash
lv_font_conv \
  --font NotoSansCJKsc-Regular.otf \
  --symbols "$(cat charset.txt without comments)" \
  --size 24 --bpp 4 \
  --format lvgl --no-compress \
  --lv-include lvgl.h \
  --lv-font-name szj_font_24 \
  -o assets/fonts/szj_font_24.c
```

Choices worth knowing:

- **4 bits per pixel.** At 1 bpp, 16 px Chinese characters turn to mush — the
  strokes are simply not representable. 4 bpp is the point where small text
  stays legible; 8 bpp would double Flash for no visible gain on this panel.
- **`--no-compress`.** Uncompressed glyph data stays memory-mapped in Flash and
  is read directly, which needs no `CONFIG_LV_USE_FONT_COMPRESSED` and no
  decompression buffer. On a part with no PSRAM that is the better trade.
- **Pin the converter version.** `lv_font_conv` 1.5.3 here; the generated data
  changes shape between releases.
- **Name each output after its size** (`szj_font_16`, `szj_font_24`,
  `szj_font_32`) so a widget's declaration says which size it is using.

## Turn on the large-font format

The 32 px subset's bitmap data is about 316 KB, and LVGL's default text-format
font stores each glyph's bitmap offset in 16 bits. Anything beyond 64 KB of
bitmap data overflows that field. The generator emits `.bitmap_index` values well
past 65535, so this is mandatory:

```text
CONFIG_LV_FONT_FMT_TXT_LARGE=y
```

It swaps the offset field to 32 bits. Without it the font either fails to compile
or renders garbage — and the symptom is confusing enough that it is worth
checking this flag first whenever a large generated font misbehaves.

## Know what it actually costs

Measured on the built image (785 glyphs per size):

| Size | Bitmap data | Generated C source |
| --- | --- | --- |
| 16 px | 83,825 bytes | 622,464 bytes |
| 24 px | 184,397 bytes | 1,218,920 bytes |
| 32 px | 316,378 bytes | 2,004,704 bytes |
| **Total** | **584,600 bytes (571 KB)** | **3,846,088 bytes (3.67 MB)** |

Two lessons:

- **The `.c` file size is not the Flash cost.** Hex literals take about six
  bytes of source per byte of bitmap; judging budget from a directory listing is
  off by more than six times.
- **The whole `.rodata` section is 744,004 bytes**, of which the bitmaps are
  584,600. The rest is glyph descriptors, character maps, and other read-only
  data. Budget the section, not just the bitmaps.

On this device that is affordable: the application image is 1,348,800 bytes in
an 8,323,072-byte partition, and `.rodata` is read straight from Flash, so the
fonts cost **no RAM**. DRAM sits at 46% used.

## Declaration and use

```c
LV_FONT_DECLARE(szj_font_16);
LV_FONT_DECLARE(szj_font_24);
LV_FONT_DECLARE(szj_font_32);
```

Declare all three once in the UI header, then set a font on **every** text
widget, including ones created empty and filled in later. Give the generated
files a comment saying they are generated and how to regenerate them; they are
large enough that nobody will read them, and small enough that editing them by
hand is tempting.

## Keep the source font out of the repository

The source OTF is 16 MB, which does not belong in a firmware repository. The
generator downloads it on demand from a pinned URL and **verifies a SHA-256**
before use, so a changed upstream file fails loudly instead of silently changing
the rendered glyphs. Record the font's name, licence (SIL Open Font License
1.1), and the converter version in `assets/README.md` next to the generated
files.

## Check list

- Inventory is generated from the text, the UI string literals, and a symbol list
  — not hand-written.
- Comments are stripped before scanning literals, so commented-out Chinese is not
  shipped.
- Every code point is verified against the source font's character map, and a
  miss fails the build.
- `CONFIG_LV_FONT_FMT_TXT_LARGE=y` is set whenever any subset exceeds 64 KB of
  bitmap data.
- Budget comes from the map or size report, not from the generated file sizes.
- The source font is fetched by pinned URL plus hash, not committed.

## Related documents

- [San Zi Jing kids game](sanzijing-kids-game/README.md) — the application these
  subsets were cut for.
- [Letting font metrics drive the layout](font-metrics-driven-layout.md) — the
  other half: the sizes of these fonts decide how tall a list row has to be.
- `docs/development/engineering/lvgl-chinese-fonts.md` — the repository's own
  font guidance.
- `tools/sanzijing/gen_font.py` and `assets/fonts/charset.txt` — the generator
  and its checked-in inventory.
