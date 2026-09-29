<p align="right">
  <a href="font-metrics-driven-layout.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Letting font metrics decide the layout

Recorded while building the [Qiaopi Quiz app](qiaopi-quiz/README.md). Every layout
bug in it came from the same habit: pick a round line height, write the copy, then
discover that the font disagrees. This entry is the arithmetic that replaced the
guessing, and the decisions it forced.

## Measure the faces before placing anything

Three subsets are generated at 16, 24 and 32 px, all from the same master font.
Their metrics, read out of the font rather than assumed:

| Size | Line height | Advance for a CJK glyph |
| --- | --- | --- |
| 16 px | 20 | 16.000 |
| 24 px | 29 | 24.000 |
| 32 px | 38 | 32.000 |

Two consequences, both load-bearing:

- **A line of Chinese text is `characters × size` pixels wide, exactly.** The
  advance for every ideograph equals the size, so "how many characters fit in
  210 px" is a division, not a judgement call: 6 at 32 px, 8 at 24 px, 13 at
  16 px. This is what makes the content caps checkable at generation time instead
  of visible only on the device.
- **Line height is not the size.** Choosing a row of 24 px for a 24 px font leaves
  the glyphs no room: the face needs 29. Every row in this app is 36 px tall —
  29 plus the 3 px border on each side plus a pixel of slack, and no more, because
  four rows at 36 px plus three 4 px gaps is 156 px, and the ask page has to fit
  the sentence's three lines (87 px) above them inside 248 px.

## Two forms of the same sentence, and both need a cap

The ask page can only show the gap, so it renders the sentence with the gap
replaced by a slot. The reveal page shows the sentence with the answer filled in.
The two differ in length, and the slot form is the longer one — which is the
opposite of what you assume, since the filled form is what you picture while
reading the content.

Both caps are enforced, in the generator and again in a host test against the
whole bank:

| Form | Cap | Longest shipped |
| --- | --- | --- |
| slot (ask page) | 22 characters | 22 |
| filled (reveal page) | 21 characters | 19 |

## The wrap buffer is sized from the worst caller, not the average one

Wrapped text goes into a single module-static buffer shared by every layer, so it
has to fit the most expensive caller — the explanation, which is capped at 60
characters and can reach eight lines:

```
60 characters x 3 bytes        = 180 bytes
break-early newlines, worst 10 =  10 bytes
terminator                     =   1 byte
                                 ---------
                                 191 bytes   ->  QPQ_WRAP_CAPACITY = 208
```

Sizing it from the sentence (22 characters, 3 lines, 66 bytes) would have looked
plausible and overflowed the first time a long explanation was wrapped. The
derivation sits next to the constant, in characters and bytes, so the next person
can re-check it instead of trusting it.

## Size every `snprintf` buffer for the widest conversion, and say so

GCC checks an `snprintf` buffer against the widest possible `%d` — 11 characters
including the sign — not the two or three digits the counter will actually hold.
For a Chinese interface this is easy to get wrong, because each ideograph already
costs three bytes before any number appears. The rule used here:

- **Size for the type's worst case and put the arithmetic in a comment**, so
  nobody later "optimises" it back down to the observed value range.
- **A zeroed buffer on failure is the other half of the contract**: the wrap
  helper returns 0 and writes an empty string rather than leaving a half-written
  line, so a too-small buffer shows as a missing line instead of corrupted text.

The firmware build here does not enable `-Werror=format-truncation`, so a
too-small buffer is a warning that scrolls past rather than a failure. The
arithmetic is the reliable check, not the compiler.

## Bind the generated budgets to the layout at compile time

The per-line character counts appear in three places that must agree: the content
caps in `tools/qiaopi/content.py`, the generated macros in `main/qpq_text.h`, and
the layout constants in `main/qpq_ui.h`. `qpq_ui.c` asserts the relationships
rather than trusting them:

```c
_Static_assert(QPQ_CHARS_BODY * 24 <= QPQ_BODY_W, "24px per line no longer fits");
_Static_assert(QPQ_OPTION_MAX_CHARS <= QPQ_CHARS_BODY,
               "an option must fit on one line");
_Static_assert(QPQ_EXPLAIN_MAX_CHARS * 3 + 10 + 1 <= QPQ_WRAP_CAPACITY,
               "the wrap buffer cannot hold the longest explanation");
```

Narrowing the content area, or raising a content cap, now fails the build instead
of pushing text off the bottom of the panel where nobody looks until the device is
in someone's hand.

## Measure the page budgets in one place

Page geometry is fixed and written down, so the next person can check a new page
against it instead of discovering the constraint by trial:

```
Page card 230 x 310, inset 5, radius 25
  top bar    0 .. 36     (QPQ_BAR_H)
  body      36 .. 284    (QPQ_BODY_H = 248)
  hint bar 284 .. 310    (QPQ_HINT_H = 26)
  content x 10 .. 220    (QPQ_BODY_W = 210)

Ask page (body 248 px tall)
  sentence   0 .. 87     24 px, 3 lines x 29
  options   92 .. 248    4 rows x 36 + 3 gaps x 4 = 156

Reveal page
  everything in a vertical flex column inside the one scrollable container,
  because explanation + full passage + provenance cannot fit 248 px and trimming
  them would remove the reason the content was chosen
```

The ask page comes out at exactly 248 px, which is the point: the numbers were
solved for, not fitted afterwards.

## The vertical budget needs asserting too — and a scroll container needs a key

The assertions above are all about **width**. The vertical budget was written down in
a comment and nowhere else, and the first hardware test found two defects that both
came out of that gap:

1. **A scrollable container that no key could scroll.** The reveal page is taller
   than one screen and is the only scrollable object in the app. But its key handler
   forwarded every key to the state machine, and the state machine's reveal stage
   recognises only OK — up and down returned "no action" and were dropped. On the
   device the reader could see the result and the answer, and never the explanation,
   the full passage or the provenance. Making something scrollable is not the same as
   making it reachable; **every scrollable region needs a key that moves it**, and
   the check is a one-liner: what does each physical key do while this region is on
   screen?
2. **A line placed below the body region.** The title page's statistics label sat at
   y = 250 in a 248 px tall body. LVGL clips children to their parent by default, so
   the line simply did not exist on the device. Nothing looked broken, because a
   missing "seen 12/91 · best 240" line does not look like a bug — this kind of defect
   is invisible in review and invisible on screen.

Each page now asserts its own vertical budget:

```c
#define TITLE_MENU_END \
    (TITLE_MENU_Y + (TITLE_ITEMS - 1) * (QPQ_ROW_H + QPQ_ROW_GAP) + QPQ_ROW_H)
_Static_assert(TITLE_MENU_END <= QPQ_BODY_H, "the last menu row will be clipped");
_Static_assert(TITLE_STATS_Y + TITLE_STATS_H <= QPQ_BODY_H, "the statistics line will be clipped");
_Static_assert(TITLE_STATS_Y + TITLE_STATS_H <= TITLE_MENU_Y, "the statistics line overlaps the menu");
```

A host test cannot cover this: the constants live in files that include `lvgl.h`. The
compile-time assertion is the only mechanism that stays in the loop for free — and
the fix for the statistics line was to move it **into** the space freed by a
decorative subtitle that duplicated the top bar, so the assertion passed without
shrinking anything.

## Related

- [Qiaopi Quiz app](qiaopi-quiz/README.md) — the pages these constants govern.
- [Subsetting a CJK font for LVGL](cjk-font-subsetting-for-lvgl.md) — where the
  sizes and line heights come from.
- [Keeping application logic on the host](host-testable-app-logic.md) — the test
  that asserts the real content still fits these budgets.
