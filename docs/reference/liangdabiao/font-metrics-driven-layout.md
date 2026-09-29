<p align="right">
  <a href="font-metrics-driven-layout.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Letting Font Metrics Drive the Layout

Also written while building the [San Zi Jing kids game](sanzijing-kids-game/README.md).
Every layout bug in that application came from one habit: choosing round numbers
for row heights and writing UI copy first, then discovering the font disagreed.
This entry is the arithmetic that replaced the guessing, and the three defects it
caught.

## A row's usable height is not its height

A list row with a 3 px border does not get its full height for text — the border
eats into it from both sides:

```text
usable text height = row height - 2 * border width
```

That value has to be at least the font's `line_height`. For the three subsets
used here, LVGL reports:

| Font | `line_height` |
| --- | --- |
| `szj_font_16` | 20 px |
| `szj_font_24` | 29 px |
| `szj_font_32` | 38 px |

Note that `line_height` is noticeably larger than the nominal size: the 24 px
font needs 29 px, the 32 px font needs 38. This is the trap. A 40 px row with a
3 px border leaves 34 px of content, which looks like plenty for "24 px text" —
and it is. But the same 40 px row used for a 32 px option leaves 34 px against a
requirement of 38, and the glyphs get clipped top and bottom by 2 px each. It is
visible on hardware and invisible in the source.

So: pick the font first, then solve for the row height.

```text
row height >= line_height + 2 * border width
```

Where that landed in practice:

| Row | Before | After | Why |
| --- | --- | --- | --- |
| Home menu row (24 px text) | 34 | **36** | 34 left 28 px, needed 29 |
| Quiz option row (32 px text) | 40 | **46** | 40 left 34 px, needed 38 |
| Settings row (24 px text) | 40 | 40 | already sufficient |

Making the option rows 6 px taller meant the prompt panel above them had to give
up 16 px (76 to 60) so the page still fit. That is the normal consequence: on a
240 × 320 screen, fixing one row's height re-opens the vertical budget for the
whole page.

## Budget the page vertically and show the arithmetic

Each page's constants now carry their vertical budget as a comment, and the sum
is easy to re-check:

```text
Home  (page card is 310 px tall, hint bar starts at 284)
  top bar     0 .. 36
  title plate 44 .. 110
  menu      118 .. 280    4 rows x 36 + 3 gaps x 6 = 162
  hint bar  284 .. 310

Quiz  (content container is 248 px tall)
  question no.   6 .. 26
  prompt        32 .. 92
  options       96 .. 242   3 rows x 46 + 2 gaps x 4
  bottom slack            6
```

Write this out before writing code. A page that overflows does not clip
gracefully — the last row simply disappears off the bottom, and on a device with
no scroll view there is no way for the user to reach it.

## Measure text width against the real font

LVGL lays a label out by summing glyph advances with no kerning, so the width can
be predicted exactly from the font file. Summing advances and scaling by the
pixel size gives the same number the device will use:

```python
from fontTools.ttLib import TTFont

font = TTFont("NotoSansCJKsc-Regular.otf", lazy=True)
upem = font["head"].unitsPerEm
cmap = font.getBestCmap()
hmtx = font["hmtx"]

def width(text, px):
    return sum(hmtx[cmap[ord(ch)]][0] for ch in text) * px / upem
```

Run this over **every string in the UI** before the UI is written, comparing each
against the space it has. That check found a real collision: the settings row
label — six Chinese characters meaning "reset learning progress" — is 144 px wide
at 24 px, but the primary text area in that row is only 124 px. The row is
210 px, minus a 3 px border on each side, minus 8 px padding on each side, minus
64 px reserved for the right-aligned status label. The text would have run under
the status label. Shortening the label to four characters ("reset progress")
fixed it.

Chinese text is unusually predictable: four characters of the 24 px subset are
exactly 96 px, because CJK glyphs are full-width. Latin strings in the same UI do
not follow that rule at all, which is why the measurement has to be per string,
not per character count.

## Size format buffers for the worst case, not the real case

Two `snprintf` calls failed the build with `-Werror=format-truncation`. The
buffer looked right for the values involved, and that is exactly the problem:
GCC checks the buffer against the **widest possible** `%d` — 11 characters
including the sign — not the two or three digits the counter will actually hold.

A question-number label is built from:

```c
char caption[16];   // too small
snprintf(caption, sizeof(caption), QUESTION_FORMAT, index + 1, total);
```

The format has 9 bytes of literal text (two ideographs at 3 bytes each, plus
spaces and a slash) and two integer conversions. Worst case that is
`9 + 11 + 11 = 31` bytes of text plus a terminator: 32. A 16-byte buffer is not
close.

The fix used here is to size the buffer for the type's worst case and say why in
a comment, so that nobody later "optimises" it back down to the real value range:

```c
// Size for the worst decimal width of int, not for the actual question count:
// the real maximum is 6 questions, but -Wformat-truncation only looks at the
// type's range, so sizing to the real values reports a truncation error.
char caption[48];
snprintf(caption, sizeof(caption), QUESTION_FORMAT, index + 1, total);
```

Clamping the value is the alternative when the buffer has to stay small — the
label then caps at 999 instead of being cut off, which is friendlier — but it
needs the same "this is deliberate" comment.

Chinese literals make this much easier to hit than Latin ones, because each CJK
character already costs three bytes before any numbers appear. Sweep every
`snprintf` in the application for this pattern at once rather than fixing them
one compile error at a time.

## Stop text from overlapping when copy changes

The measurements fix the copy you have today. Copy changes. Two cheap
defences make the next change degrade instead of break:

- **Give every row an explicit width** and let the label ellipsize
  (`LV_LABEL_LONG_MODE_DOTS`) rather than letting a label size itself to content
  and grow into its neighbour.
- **Give dynamic numbers a fixed-width container.** A counter that can grow a
  digit, or a `--` fallback that replaces a two-digit percentage, must not move
  anything else on screen.

## One more: character counts are not byte counts

A separate defect, but the same family — arithmetic about text done in the wrong
unit. The flashcard page splits each 12-character lesson into two half-lines for
display. The code computed a half-line as "3 bytes per character × 4 lines = 12
bytes" and copied 12 bytes per half-line, which only covers 8 of the lesson's 12
characters (12 bytes = 4 characters, so 2 × 4 = 8). **The fourth line of every
lesson was never displayed.**

The fix is 18 bytes per half-line (6 characters × 3), plus a compile-time
constraint so the relationship cannot drift again:

```c
_Static_assert(CARD_HALF_BYTES * CARD_HALF_COUNT ==
                   (SZJ_LINE_BYTES - 1) * SZJ_LINES_PER_STANZA,
               "two half-lines must cover the whole lesson");
```

`SZJ_LINE_BYTES - 1` is one line's worth of text without its terminator, so the
right-hand side is the lesson's 36 bytes of characters, and the left-hand side is
"bytes per half-line times how many half-lines there are".

A slot count and a byte count are different things. Naming them differently in
the source — `CARD_GLYPH_BYTES` for the byte width of one character,
`CARD_CELL_COUNT` for how many characters fit — is what stops the mix-up.

## Check list

- Row height is derived from `line_height + 2 * border`, not from a round number.
- Every page has a written vertical budget whose parts sum to the page height.
- Every user-visible string was measured against its available width before the
  code was written.
- Every `snprintf` buffer is sized for the widest possible conversion, with a
  comment saying why it is larger than it looks like it needs to be.
- Labels that can grow have fixed widths and ellipsize.
- Compile-time assertions cover any arithmetic that mixes characters, bytes, and
  slots.

## Related documents

- [San Zi Jing kids game](sanzijing-kids-game/README.md) — the pages these
  constants govern.
- [Cutting a CJK font subset for LVGL](cjk-font-subsetting-for-lvgl.md) — where
  the font sizes and their line heights come from.
- `main/szj_ui.c` — the shared row and option widgets and their padding.
- `main/szj_home.c`, `main/szj_lesson.c`, `main/szj_card.c` — the pages whose
  constants carry the vertical budget.
