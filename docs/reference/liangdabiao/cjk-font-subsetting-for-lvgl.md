<p align="right">
  <a href="cjk-font-subsetting-for-lvgl.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Subsetting a CJK font for LVGL

Recorded while building the [Qiaopi Quiz app](qiaopi-quiz/README.md), an offline
fill-in-the-blank game for the FoloToy AI Passport. Everything below applies to any
application on this board that has to render Chinese; every number was measured on
an ESP32-C3 with 8 MB of flash and no PSRAM.

## Derive the inventory from the sources, never maintain it by hand

The app shows 91 questions with explanations and provenance lines, plus its own
interface strings, and that already needs **1303 distinct code points**. Nobody is
going to keep a list like that correct by hand, so `gen_font.py` derives it from
three places:

1. every character of the question bank, through the same `content.py` module the
   C table generator uses;
2. every non-ASCII character inside a string literal of `main/*.c|*.h`, after
   comments are stripped — Chinese comments therefore cost no flash;
3. printable ASCII plus the punctuation the interface draws itself.

The result is written to `assets/fonts/charset.txt` so a reviewer has something to
diff and so that "why is this glyph in here" has an answer:

```
# total code points: 1303  (CJK 1079, other 224)

# --- CJK ideographs ---
<1079 ideographs, in one unbroken run, no separators>

# --- everything else (ASCII, CJK punctuation, the fullwidth low line) ---
 !"#$%&'()*+,-./0123456789:;<=>?@ABC...·—…、。《》「」！（），：；？＿
```

The first character of the second group is the space, which is easy to lose: a
trim step in the generator that strips whitespace would drop `U+0020` and the
inventory would silently become 1302 while still rendering correctly until the day
a string needs a space.

The ideographs are kept as a single unbroken run on purpose: a separator between
them would itself be picked up by the converter as a glyph to include.

The last character of the second group is a fullwidth low line, used to draw the
blank the reader has to fill. It is in the inventory because it appears in a
string literal of `qpq_content.c`, not because it was listed anywhere by hand.

## Verify every code point before converting

The master font here is Noto Sans CJK SC Regular, under the SIL Open Font License
1.1. Before conversion, every code point is looked up in the font's `cmap` with
`fontTools`, and a miss aborts the run with up to forty offending characters
printed. On this project the check reported 1303/1303 present.

This matters because the source text changes. A new question's explanation may
quote a rare variant character, and handing a font a code point it does not have
does **not** reliably stop the converter — it may happily emit a subset that is
missing that glyph, and you find out on the device, in the middle of a line, as a
blank box. Checking in advance converts that into a build error. It costs one pass
over the character map, which is cheap insurance.

## Three sizes, uncompressed, and why the large format is required

Sizes are 16 px (top bar, provenance, summary labels), 24 px (sentence, options,
explanation, full passage) and 32 px (title page headline and result rank), all at
4 bits per pixel and uncompressed, so they can be read straight out of the flash
mapping without decompression.

In LVGL's default text format the per-glyph bitmap offset is a 16-bit field, and
the 32 px subset's bitmap data exceeds 64 KB. `CONFIG_LV_FONT_FMT_TXT_LARGE=y` in
`sdkconfig.defaults` is therefore mandatory, not a tuning knob.

## The size of the generated `.c` file is not the flash cost

This one cost an hour of unnecessary alarm. The three generated files are 1.07 MB,
2.12 MB and 3.48 MB of source, which looks impossible next to a 7.94 MB partition.
It is not: each byte is written as `0xXX,`, six characters per byte of data.

Counting the actual data bytes in the three files gives roughly 147 KB, 324 KB and
553 KB, or **about 0.98 MB for all three faces**. The source files can be
arbitrarily large; what matters is what the compiler keeps.

The practical rule: estimate flash from the data, not from the generated text. If
you must guess before building, `glyphs × size × size ÷ 2` bytes per face (for
4 bpp) is the right shape of estimate, and the real answer comes from the firmware
size report.

## Related

- [Qiaopi Quiz app](qiaopi-quiz/README.md) — the subsets these serve.
- [Turning a prose bank into generated C tables](question-bank-pipeline.md) — the
  module that supplies the content half of the inventory.
- [Letting font metrics decide the layout](font-metrics-driven-layout.md) — the
  character-per-line budgets these sizes imply.
