<p align="right">
  <a href="cjk-font-subsetting-for-lvgl.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Cutting a CJK Font Subset for LVGL

Written while building the [Dao De Jing daily reading app](daodejing-daily/README.md),
a fully offline Chinese reading device. These notes apply to any AI Passport
application whose UI shows Chinese text: the numbers below are measured on an
ESP32-C3 with 8 MB Flash and no PSRAM.

## Why a subset is unavoidable

LVGL ships Montserrat, which is Latin-only, and an optional Source Han subset
that is off by default. Setting a Chinese string on a widget whose font has no
CJK glyphs does not raise an error — the label simply renders blank or draws
tofu boxes. LVGL has no fallback chain, so **every widget that shows Chinese must
be given a font that contains those characters**.

This application displays one curated chapter plus its own interface strings, and
that already needs **397 distinct code points**. The three subsets in this
project cover all of them.

## Derive the glyph inventory; never hand-list it

A hand-maintained character list goes stale the first time somebody adds a menu
label, and the failure is silent. Instead, generate the inventory by unioning
three sources:

1. **Every character of the chapter sources**, read through the same module that
   feeds the table generator — `tools/daodejing/content.py` — so the C tables and
   the font can never disagree about what the text says. Read them as characters
   on a copy with all whitespace removed; splitting on whitespace yields
   sentences, not single ideographs, and the subset quietly ends up containing
   nothing.
2. **Every non-ASCII character inside a string literal** in the application
   sources. Strip `//` and `/* */` comments *before* scanning, so Chinese
   comments cost no Flash; keep string literals intact, including escapes. The
   generated tables are skipped explicitly — they are already covered by the
   chapter sources, so scanning them is duplicated work.
3. **Printable ASCII** (`0x20`-`0x7E`) plus the Chinese punctuation the UI draws
   on its own: `·，。！？、：；（）「」《》…—`. The CJK punctuation is already
   picked up from the chapter text, but keeping it in the explicit list means a
   chapter edit that drops a punctuation mark cannot silently shrink the font.

That produced 397 code points: 286 ideographs and 111 characters from the other
two groups. Writing them out to a checked-in `charset.txt` gives reviewers
something to diff and gives the next person a way to see *why* a glyph is in
there:

```text
# total code points: 397  (CJK 286, other 111)

# --- CJK ideographs ---
<286 ideographs, sorted by code point, one run with no separators:
 U+4E00 U+4E03 U+4E07 U+4E09 ... >

# --- everything else ---
 !"#$%&'()*+,-./0123456789:;<=>?@ABC...·—…、。《》「」！（），：；？
```

Writing the ideograph group as one unbroken run is deliberate: it is the thing the
converter consumes, and a separator inserted "for readability" would become a
glyph in the subset. The second group is short enough to read as a line, and it
is where reviewers actually look.

The first character of the second group is the space, which is easy to lose: a
trim step in the generator that strips whitespace would drop `U+0020` and the
inventory would silently become 396 while still rendering correctly until the
day a string needs a space.

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

This matters because the source text can change. A curated chapter that quotes a
variant or archaic form introduces a rare ideograph, and a converter given a
character the font lacks does not necessarily stop; it can emit a font that is
missing that glyph, and you discover it as one blank square in the middle of a
line, on the device. Checking first turns that into a build-time error with up to
forty offending characters printed. On this project the check reported 397/397
present in Noto Sans CJK SC — cheap insurance that costs one pass over a
character map.

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
  --lv-font-name ddj_font_24 \
  -o assets/fonts/ddj_font_24.c
```

Choices worth knowing:

- **4 bits per pixel.** At 1 bpp, 16 px Chinese characters turn to mush — the
  strokes are simply not representable. 4 bpp is the point where small text
  stays legible; 8 bpp would double Flash for no visible gain on this panel.
- **`--no-compress`.** Uncompressed glyph data stays memory-mapped in Flash and
  is read directly, which needs no `CONFIG_LV_USE_FONT_COMPRESSED` and no
  decompression buffer. On a part with no PSRAM that is the better trade.
- **Pin the converter version.** `lv_font_conv` 1.5.3 here; the generated data
  changes shape between releases, so the version is written into the generator
  rather than left to whatever is on `PATH`.
- **Name each output after its size** (`ddj_font_16`, `ddj_font_24`,
  `ddj_font_32`) so a widget's declaration says which size it is using. The
  generator derives the name from the size loop, so the two cannot drift.

## Turn on the large-font format

LVGL's default text-format font stores each glyph's bitmap offset in 16 bits, so
anything beyond 64 KB of bitmap data overflows that field. The bitmaps are not
compressed, so the size is arithmetic: 397 glyphs at 4 bpp, each taking
`size * size / 2` bytes.

| Subset | Bytes per glyph | Bitmap payload | Over the 64 KB field? |
| --- | --- | --- | --- |
| 16 px | 128 | 50,816 | no |
| 24 px | 288 | 114,336 | yes |
| 32 px | 512 | 203,264 | yes |

Two of the three overflow, so this is mandatory:

```text
CONFIG_LV_FONT_FMT_TXT_LARGE=y
```

It swaps the offset field to 32 bits. Without it the font either fails to compile
or renders garbage — and the symptom is confusing enough that it is worth
checking this flag first whenever a large generated font misbehaves. Note that
the flag is per font *format*, not per font, so the 16 px subset gets it too
even though it would have fit.

## Know what it actually costs

Measured on the generated sources:

| Size | Generated C source |
| --- | --- |
| 16 px | 293,734 bytes |
| 24 px | 565,633 bytes |
| 32 px | 920,969 bytes |
| **Total** | **1,780,336 bytes (1.70 MB)** |

Two lessons:

- **The `.c` file size is not the Flash cost.** Hex literals take several bytes
  of source per byte of bitmap — here the three files total 1.70 MB of source
  for roughly 368 KB of bitmap payload, plus glyph descriptors and character
  maps. Judging the budget from a directory listing is off by close to five
  times, in the direction that makes you panic for no reason.
- **Budget the `.rodata` section, not the bitmaps.** The bitmaps are only part of
  what a generated font puts in Flash; the descriptors and maps ride along. The
  number that matters is the section size in the build's size report.

The bitmaps are `const`, so `.rodata` is read straight from Flash and the fonts
cost **no RAM**. That is what makes an uncompressed, memory-mapped font the right
trade on a part with no PSRAM.

## Declaration and use

```c
LV_FONT_DECLARE(ddj_font_16);
LV_FONT_DECLARE(ddj_font_24);
LV_FONT_DECLARE(ddj_font_32);
```

Declare all three once in the UI header, then set a font on **every** text
widget, including ones created empty and filled in later. Give the generated
files a comment saying they are generated and how to regenerate them; they are
large enough that nobody will read them, and small enough that editing them by
hand is tempting.

## Keep the source font out of the repository

The source OTF is 16 MB, which does not belong in a firmware repository. The
generator does not need it committed: it looks for the file at a configured path
(`DDJ_SOURCE_FONT`, defaulting to a directory outside the repository) and, when
it is absent, `--download` fetches it from a pinned URL and **verifies a
SHA-256** before writing it, so a changed upstream file fails loudly instead of
silently changing the rendered glyphs. Record the font's name, licence (SIL Open
Font License 1.1), the pinned hash, and the converter version in `assets/README.md`
next to the generated files.

## Check list

- Inventory is generated from the chapter sources, the UI string literals, and a
  symbol list — not hand-written.
- Comments are stripped before scanning literals, so commented-out Chinese is not
  shipped.
- The generated tables are excluded from the scan, so the inventory has exactly
  one source of truth for text.
- Every code point is verified against the source font's character map, and a
  miss fails the build.
- `CONFIG_LV_FONT_FMT_TXT_LARGE=y` is set whenever any subset exceeds 64 KB of
  bitmap data — and the arithmetic that decides it is written down.
- Budget comes from the map or size report, not from the generated file sizes.
- The source font is fetched by pinned URL plus hash, or read from outside the
  repository; it is never committed.

## Related documents

- [Dao De Jing daily reading app](daodejing-daily/README.md) — the application
  these subsets were cut for.
- [Letting font metrics drive the layout](font-metrics-driven-layout.md) — the
  other half: the sizes of these fonts decide how many characters fit on a line
  and how tall a row has to be.
- `docs/development/engineering/lvgl-chinese-fonts.md` — the repository's own
  font guidance.
- `tools/daodejing/gen_font.py` and `assets/fonts/charset.txt` — the generator
  and its checked-in inventory.
