<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Dao De Jing Daily Reading

An offline daily-reading application for the FoloToy AI Passport. It presents
the Dao De Jing one chapter at a time, one screenful at a time, for an adult
reader — it is deliberately not a reference reader, a search tool, or a
translation comparison.

- **Hardware:** FoloToy AI Passport (ESP32-C3, 8 MB flash, no PSRAM), 240 x 320
  portrait panel with a 30 px corner mask, three keys (up / down / OK).
- **Release:** the initial prototype. The firmware does not display a version
  string; the settings page reports the size of the book and how many chapters are
  curated, which is the information a bug report actually needs.
- **Content shipped:** chapter 1 of a planned 81.
- **Cover image:** none.

## What it is for

The content area is 210 px wide and 248 px tall. At the 24 px reading size that
is eight characters per line and eight lines — about 64 characters on screen, and
even the shortest chapter here is longer than that. That rules out reading a
chapter as continuous prose, so the product is built around the opposite habit:
**one small mouthful a day, chewed repeatedly**.

That single constraint explains every other decision:

- A chapter opens as a sequence of **read screens**, one sentence per screen.
  Advancing is the reading rhythm, not scrolling.
- Commentary is **bullet-sized**: three to five points per chapter, each one
  screenful at most.
- Each chapter ends with a **reflection question** and three answers. The three
  answers carry no right or wrong — they record a stance: *landed*, *still
  chewing*, *did not connect*.
- The home screen's third entry lists the chapters whose recorded stance is
  *still chewing* or *did not connect*. This is the app's re-reading queue, and
  it is the reason the app exists at all: the same sentence should be able to
  answer differently on a different day.

Because the stance is not a score, there is no correct/incorrect colour and no
correct/incorrect sound anywhere in the interface. The accent colour (cinnabar)
marks the current selection and nothing else.

## Why there is no "favourite" action

The board has three keys, and inside the four-layer daily flow every one of them
already has a job. Adding a favourite key would necessarily collide with one of
them. The re-reading queue is therefore derived from data the reader already
produces — the stance recorded during reflection — instead of from a new
gesture. `ddj_progress` still carries a per-chapter bit for a future explicit
favourite feature; nothing writes it today.

## Content model

A chapter is variable-length, unlike a fixed-form recitation text, so the data
model is a flat set of tables plus a per-chapter index record:

| Field | Meaning |
| --- | --- |
| `number`, `volume`, `title` | Chapter number, volume (Dao, chapters 1-37 / De, 38-81), and a short heading. |
| `passage_first`, `passage_count` | Slice of the shared passage table: one row per reading screen. |
| `point_first`, `point_count` | Slice of the shared commentary table: one row per screen. |
| `option_first` | Offset into the shared options table, always a multiple of three. |
| `question` | The reflection question, one per chapter. |

Progress is stored for all 81 chapters from the first day, independent of how
many chapters have content, so filling in the remaining chapters needs no change
to the save format.

Chapter sources are plain text files, one per chapter, under
`tools/daodejing/chapters/`. `tools/daodejing/gen_content.py` projects them into
`main/ddj_text.c` and `main/ddj_text.h`; nothing is ever edited in C directly.
The same module that feeds the generator also feeds the font-subset generator, so
the tables and the glyph inventory cannot disagree about what the text says.

## Copyright

The Dao De Jing itself is public domain. The commentary is **written for this
project** and is not a reproduction of any modern author's interpretation. The
application is positioned as a reading device with editorial notes, not as an
edition of a specific commentator.

## Layout is arithmetic, not taste

The content area is 210 px wide. Arabic and CJK glyphs at a given size are not
the same width, but every full-width CJK glyph in the source font has an advance
exactly equal to the font size, which was verified per code point before
generating the subsets. So "how many characters fit on one line" is a division:

| Layer | Font | Characters per line | Lines budgeted | Character cap |
| --- | --- | --- | --- | --- |
| Passage | 32 px | 6 | 5 | 24 |
| Commentary | 24 px | 8 | 7 | 48 |
| Reflection question | 16 px | 13 | 3 | 26 |
| Reflection answers | 24 px | 7 | 1 | 7 |

Both sides of that table are enforced: the generator refuses a chapter whose
text exceeds the cap, and `tests/test_ddj_wrap.c` links the real content and
asserts that no wrapped line exceeds the per-line character budget. A chapter
edit that breaks the fit fails the build instead of overflowing the panel.

## Line breaking

LVGL's automatic wrapping looks for spaces, and this text has none, so the
application wraps it itself. The rule for closing punctuation is **break one
character early** rather than pulling the punctuation up onto the previous line:
pulling it up would make that line one character too wide, LVGL would wrap it a
second time, and the line count would stop being predictable — which would
invalidate the table above. Breaking early keeps both lines inside the budget and
still keeps closing punctuation off the start of a line.

## Key semantics

| Key | Action |
| --- | --- |
| Up / Down | Move within the current layer: previous/next sentence, previous/next commentary point, previous/next answer, previous/next list entry. |
| OK (short) | Step down one layer. On the last sentence or point this advances the layer rather than moving. |
| OK (long) | Step back one layer. On the first read screen this leaves the chapter without recording anything. |

## Verification

- `tools/validate.sh --static` — repository contract, generated-table freshness,
  and four host tests over the platform-independent logic (content access, line
  breaking, progress model, four-layer session state machine).
- `tools/validate.sh --firmware` — cold build plus merged image, verified
  section by section against the expected flash offsets.

**Not verified on hardware.** The panel and its corner mask, the three-key ADC
ladder thresholds, the synthesised sound effects, the battery gauge, NVS
persistence across power loss, and the LVGL memory pool under sustained page
turning have all been reasoned about but never observed on a real device.

## Related

- [Reference index](../../README.md)
