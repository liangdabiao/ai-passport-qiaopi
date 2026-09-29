<p align="right">
  <a href="font-metrics-driven-layout.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Letting Font Metrics Drive the Layout

Also written while building the [Dao De Jing daily reading app](daodejing-daily/README.md).
Every layout bug in that application came from one habit: choosing round numbers
for row heights and writing UI copy first, then discovering the font disagreed.
This entry is the arithmetic that replaced the guessing, and the defects it
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
| `ddj_font_16` | 20 px |
| `ddj_font_24` | 29 px |
| `ddj_font_32` | 38 px |

Note that `line_height` is noticeably larger than the nominal size: the 24 px
font needs 29 px, the 32 px font needs 38. This is the trap. A 40 px row with a
3 px border leaves 34 px of content, which looks like plenty for "24 px text" —
and it is. But the same 40 px row used for a 32 px string leaves 34 px against a
requirement of 38, and the glyphs get clipped top and bottom by 2 px each. It is
visible on hardware and invisible in the source.

So: pick the font first, then solve for the row height.

```text
row height >= line_height + 2 * border width
```

Where that landed here:

| Row | Font | Height | Border | Usable | `line_height` |
| --- | --- | --- | --- | --- | --- |
| List row (`DDJ_ROW_H`) | 24 px | 44 | 3 | 38 | 29 |
| Home menu row | 24 px | 44 | 3 | 38 | 29 |
| Settings row | 24 px | 44 | 3 | 38 | 29 |

44 px is not a round number chosen for looks — it is the smallest height that
also happens to clear the 32 px font's 38 px requirement, which is what makes it
safe to reuse the same row height for a 24 px label today and a 32 px one later.
Making every row 6 px taller than a naive 38 px choice would re-open the vertical
budget for each page; here it is spent once, in one shared constant.

## Budget the page vertically and show the arithmetic

Each page's constants now carry their vertical budget as a comment, and the sum
is easy to re-check. The page card is 230 x 310 with a 5 px inset, the content
column starts 10 px in and is 210 px wide:

```text
Page card  230 x 310, inset 5, radius 25
  top bar     0 .. 36      (DDJ_BAR_H)
  body       36 .. 284     (DDJ_BODY_H = 248)
  hint bar  284 .. 310     (DDJ_HINT_H = 26)
  content x  10 .. 220     (DDJ_BODY_W = 210)

Home
  caption    38 ..  58
  menu       64 .. 258     4 rows x 44 + 3 gaps x 6 = 194

Daily, read layer   (body is 248 tall)
  caption          y = 2
  passage    24 .. 248     DAILY_READ_H = 248 - 24 = 224

Daily, reflection layer
  caption          y = 2
  question   22 ..  90     DAILY_QUESTION_H = 68
  options    96 .. 244     3 rows x 44 + 2 gaps x 8 = 148
```

Write this out before writing code. A page that overflows does not clip
gracefully — the last row simply disappears off the bottom, and on a device with
no scroll view there is no way for the user to reach it. Two consequences show up
in the numbers above:

- The read layer has **no caption of its own** beyond the 2 px offset, because the
  20 px a caption would need is the difference between a 5-line passage and a
  4-line one. Progress is reported in the bottom hint bar instead, which is
  already on screen.
- Only the chapter **lists** (catalog and shelf) get a scrollable container,
  because a list inherently exceeds the screen. Every other layer is laid out
  statically and must be proven to fit — which is exactly what makes the budget
  worth writing down. Keeping the scrollable set to one widget also means there
  is only one place where a stray scroll gesture can do something surprising.

## Turn the per-line budget into a division, and enforce it twice

The content column is 210 px. Because every full-width CJK glyph has an advance
exactly equal to the font size (proven in the font-subsetting entry), "how many
characters fit on one line" is a division, and it is written as one:

```c
#define DDJ_CHARS_PASSAGE (DDJ_BODY_W / 32)   // 6
#define DDJ_CHARS_PARA    (DDJ_BODY_W / 24)   // 8
#define DDJ_CHARS_NOTE    (DDJ_BODY_W / 16)   // 13
```

Writing it as a division rather than as `#define DDJ_CHARS_PASSAGE 6` means the
number changes automatically if the body width does — and, more usefully, it can
be asserted at compile time:

```c
_Static_assert(DDJ_CHARS_PASSAGE * 32 <= DDJ_BODY_W, "passage line budget no longer fits");
_Static_assert(DDJ_CHARS_PARA    * 24 <= DDJ_BODY_W, "commentary line budget no longer fits");
_Static_assert(DDJ_CHARS_NOTE    * 16 <= DDJ_BODY_W, "hint line budget no longer fits");
```

Three numbers of that kind should exist. The same budget is also declared on the
generation side, in `tools/daodejing/content.py`, where it rejects a chapter whose
text is too long to fit. The two must agree; if only one exists, chapters pass
generation and then overflow on the panel. Beyond that, `tests/test_ddj_wrap.c`
links the real content and asserts that no wrapped line exceeds the per-line
figure — so the budget is checked at generation time, at compile time, and in a
host test.

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

Run this over **every string in the UI**, comparing each against the space it
has. That check produced a real design decision in the catalog page. A list row
is 210 px wide and its text area is:

```text
210  - 2 * 3 (border)  - 2 * 8 (padding)  - 88 (right-hand note)  =  100 px
```

At 24 px that is 4 characters. The obvious heading is "chapter number + title" —
six ideographs, or 144 px — which is 44 px too wide, so both halves would have
been ellipsized and the reader would have seen a number with no title. The
chapter number therefore moved into the right-hand note, which is 16 px text in
an 88 px slot (5 characters) and renders the number with room to spare. The
comment at the top of `main/ddj_catalog.c` records that arithmetic, so the next
person does not re-derive it or undo it.

Chinese text is unusually predictable: four characters of the 24 px subset are
exactly 96 px, because CJK glyphs are full-width. Latin strings in the same UI do
not follow that rule at all, which is why the measurement has to be per string,
not per character count.

## Size format buffers for the worst case, not the real case

GCC checks an `snprintf` buffer against the **widest possible** `%d` — 11
characters including the sign — not the two or three digits the counter will
actually hold. For a Chinese UI this is easy to get wrong, because each ideograph
already costs three bytes before any number appears. The bottom hint bar's format
string is six ideographs, four spaces, one middle dot, one slash, and three
integer conversions:

```text
literal text      27 bytes   (6 ideographs, the separators, the slash)
3 x widest %d     33 bytes   (11 bytes each, sign included)
                --------
worst case        60 bytes
+ terminator      61 bytes
```

so `char foot[64]` is the smallest round size that holds it. Sizing that buffer by
the real values instead — "session 3, chapter 1 of 81" — would suggest 16 bytes
and would be wrong by a factor of four. Note also that the middle dot is two
bytes in UTF-8, not one and not three; a character-count estimate of the literal
would have been wrong in both directions.

The rule used here:

- **Size for the type's worst case and say why in a comment**, so that nobody
  later "optimises" it back down to the real value range. The arithmetic goes in
  the comment next to the declaration, because the declaration alone looks absurd
  for a counter that never exceeds two digits.
- **Clamping is the alternative** when the buffer has to stay small — the label
  then caps at 999 instead of being cut off, which is friendlier — but it needs
  the same "this is deliberate" comment.
- **`%s` needs a different argument, and it has to be made explicitly.** For
  `"%s · %s"` there is no widest possible conversion, so the bound comes from the
  two arguments being drawn from closed sets — a volume name that is always two
  characters and a chapter label that is at most five. That argument has to be
  written down too; "it's a string, so it obviously fits" is how the buffer that
  does not fit gets written. Every `%s` in this application is fed from a closed
  set, and each one says so.

Sweep every `snprintf` in the application for this pattern at once rather than
fixing them one compile error at a time. Because the firmware build here does not
enable `-Werror=format-truncation`, a too-small buffer is a warning that scrolls
past rather than a failure; the arithmetic is the reliable check, not the
compiler. A useful way to run that sweep is a small script over the sources:
extract every literal format string, substitute 11 bytes per integer conversion,
add the UTF-8 width of the literal, and compare against the declared size. It is
twenty lines, and it found three buffers here that were sized from the real values
rather than the type's range.

## One more: character counts are not byte counts

A separate defect, but the same family — arithmetic about text done in the wrong
unit. In UTF-8 a CJK character is three bytes, so a buffer sized in characters
and filled in bytes is short by a factor of three. Two places in this application
have to hold both units at once, and both are handled by *naming* the units
rather than by being careful:

```c
#define DDJ_PASSAGE_MAX_CHARS 24   /* characters -- a layout limit */
#define DDJ_POINT_MAX_CHARS   48   /* characters -- a layout limit */

/* bytes -- the buffer must hold the largest of the two, in UTF-8 */
#define DDJ_WRAP_CAPACITY 176
```

The wrap buffer is the instructive one. Wrapped text is written into a single
module-static buffer shared by every layer, so it must be sized for the **worst
caller**, which is the commentary (48 characters), not the passage (24):

```text
48 characters at 8 per line   = 6 lines
                              = 5 inserted line breaks
48 characters x 3 bytes       = 144 bytes of text
144 + 5 breaks                = 149 bytes
+ terminator                  = 150 bytes
```

`DDJ_WRAP_CAPACITY` is 176, leaving 26 bytes of margin. Sizing it from the
passage (24 characters, 4 lines, 76 bytes) would have looked sufficient and
overflowed the first time a six-line commentary point was wrapped. The derivation
is written next to the constant, in characters and bytes, so the next person can
re-check it instead of trusting it.

That also explains why the wrap contract has to say what it does at the edges.
The function inserts a break *between* lines and never a trailing one, so the
break count is `lines - 1` — which is what makes the arithmetic above close. If
it appended a trailing newline the count would be 6 and the same capacity would
still hold, but the number in the comment would be wrong, and a comment whose
arithmetic does not close is worse than no comment.

The same distinction is why the save-format constants are stated as byte counts
with the arithmetic spelled out — `4 header bytes + 81 chapter slots + 2 x 11
wrong-answer bytes + 1 checksum byte = 108` — and asserted against
`DDJ_PROGRESS_BLOB_SIZE` in a host test. A slot count and a byte count are
different things; naming them differently in the source is what stops the mix-up.

## Check list

- Row height is derived from `line_height + 2 * border`, not from a round number.
- Every page has a written vertical budget whose parts sum to the page height.
- Per-line character budgets are written as divisions of the content width and
  asserted with `_Static_assert`.
- The same budget is declared on the generation side, so content is rejected
  before it can overflow the panel.
- Every user-visible string was measured against its available width before the
  code was written, and the decisions that came out of it are recorded in a
  comment.
- Every `snprintf` buffer is sized for the widest possible conversion, with a
  comment saying why it is larger than it looks like it needs to be; where an
  argument is `%s`, the comment says which closed set it comes from.
- Buffers that must hold text in UTF-8 are sized in bytes, from a written
  character-to-byte derivation, and named so the unit is visible.

## Related documents

- [Dao De Jing daily reading app](daodejing-daily/README.md) — the pages these
  constants govern.
- [Cutting a CJK font subset for LVGL](cjk-font-subsetting-for-lvgl.md) — where
  the font sizes and their line heights come from.
- `main/ddj_ui.c` — the shared page, row, and list widgets and their padding.
- `main/ddj_ui.h` — the layout constants and the per-line budgets, with the
  compile-time assertions in the `.c`.
- `main/ddj_daily.c`, `main/ddj_home.c`, `main/ddj_catalog.c` — the pages whose
  constants carry the vertical budget.
