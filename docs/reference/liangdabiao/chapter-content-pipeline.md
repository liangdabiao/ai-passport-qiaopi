<p align="right">
  <a href="chapter-content-pipeline.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Curating Chapter Content into Generated C

Written for the [Dao De Jing daily reading app](daodejing-daily/README.md), whose
entire content is a curated set of chapters — one done, eighty-one planned. This
entry is the pipeline that turns a chapter into firmware data: one source file per
chapter, a parser that refuses anything malformed, a generator that projects the
sources into C tables, and a `--check` mode that makes a stale table fail the
build.

## The problem: a chapter has no fixed shape

A fixed-form recitation text gives you a free structural guarantee — every lesson
is the same number of lines, so "lesson 60" is arithmetic. The Dao De Jing does
not: chapters run from a couple of sentences to a dozen, commentary is as long as
it needs to be, and the whole book is a plain text that anyone can retype.

Two consequences shape everything below:

- **The data model is a flat set of tables plus a per-chapter index record**,
  rather than a fixed record per chapter. The generated header carries separate
  arrays for reading screens, commentary points, and reflection answers, and each
  chapter record holds offsets and counts into them.
- **Only one of the eighty-one chapters exists today.** So the pipeline's main
  job is to make *adding a chapter* the only edit required — no C file touched,
  no constant bumped, no test updated.

## One file per chapter, and a format that is boring on purpose

Chapter sources live in `tools/daodejing/chapters/`, one file per chapter, named
after the chapter number zero-padded to three digits. The format is deliberately
plain:

```text
# comments start with '#' and may appear anywhere
chapter = 1
volume = <one of the two volume names>
title = <a short heading, ideally three or four characters>

[passages]
one reading screen per line, no punctuation added or removed

[commentary]
one point per line

[reflection]
question = one line
option   = exactly three, in slot order
```

Four reasons this format is worth insisting on:

- **The line is the unit the application counts in.** A reading screen is one
  line, a commentary point is one screen, a reflection answer is one row. Making
  the file's unit match the domain's unit removes a whole layer of parsing.
- **It diffs cleanly.** A reviewer sees one changed line, not a reflowed
  paragraph.
- **It has no syntax to get wrong.** No quoting, no escaping, no separators that
  can be confused with content. Section headers are the only delimiters, and there
  are three of them.
- **The absence of punctuation is content, not laziness.** The source text's
  punctuation is part of the reading, and the line breaker's rule is written
  against exactly those marks. Adding or dropping a comma to make a line "look
  better" changes what is displayed and what the line breaker sees.

## The three options are positional, and the format enforces it

The reflection layer ends with a question and three answers. The three answers
carry no right or wrong — they record a stance: *this landed*, *still chewing on
it*, *did not connect*. The queue of chapters to re-read is then derived from the
recorded stance.

That only works if slot 0 always means the same thing. So the format requires
**exactly three** options, in slot order, and the generator emits them unchanged:

```python
if len(options) != OPTION_SLOTS:
    raise ContentError(
        f"{path.name}: [reflection] needs exactly {OPTION_SLOTS} options, got {len(options)}"
    )
```

The alternative — a `key = value` form for each option — would have made the file
easier to read and the ordering meaning implicit. When the ordering *is* the
meaning, making it explicit in the grammar is the cheaper trade.

## Every failure is loud, and names the line

The parser refuses to produce a chapter unless the source satisfies its shape
rules, and it names what broke. The full set it catches:

| Failure | Why it matters |
| --- | --- |
| Unknown key, unknown section, duplicate key, missing key | A typo would otherwise be silently ignored, and the chapter would ship with a default value. |
| File name does not match `chapter =` | Off-by-one file names are the easiest mistake to make and the hardest to notice; chapter 1 would display chapter 2's text and its own title. |
| Numbering has a gap | "Which chapter is next" is computed from the ordering, so a gap silently shortens the book. |
| Volume does not match the chapter number | The volume is displayed on every reading screen; a wrong one lies to the reader on every page of the chapter. |
| Empty title | The catalog and the top bar would show a blank label. |
| Over-long passage, point, question, or option | The panel would overflow at runtime. |

Reporting the location matters more than it looks. "48 characters expected"
without a file and line means bisecting the chapter by hand — and with 81 chapters
planned, that is the failure mode you will hit most often.

## Enforce the screen-fit limits in the generator

The single most valuable rule in the pipeline. The panel fits a fixed number of
characters per line at each layer's font size, so the generator refuses anything
longer than one screenful:

```python
#   layer      font  px/char  per line  lines  limit
#   passage    32     32       6         4      24
#   commentary 24     24       8         6      48
#   question   16     16       13        2      26
#   option     24     24       7         1      7
PASSAGE_MAX_CHARS = 24
POINT_MAX_CHARS = 48
QUESTION_MAX_CHARS = 26
OPTION_MAX_CHARS = 7

for index, passage in enumerate(passages):
    if len(passage) > PASSAGE_MAX_CHARS:
        raise ContentError(
            f"{path.name}: passage {index + 1} is {len(passage)} characters, "
            f"over the {PASSAGE_MAX_CHARS}-character limit for one screen; "
            "split it into another screen instead"
        )
```

Two things make this better than a style guide. First, the error message tells
the author the *fix* — split the passage — not just the violation. Second, the
same numbers are declared on the rendering side, in `main/ddj_ui.h`, as divisions
of the content width with `_Static_assert` guards, so a change to the layout
breaks the build rather than letting content pass generation and then overflow.

And the limits are for the layer's *purpose*, not just its size. The commentary
limit of 48 characters is not "as much as fits" — it is the length at which a
point stops being a bullet and becomes an essay. Writing the limit down is what
keeps the commentary a bullet list after the tenth contributor.

## Generate the C tables, and derive the constants from the data

`tools/daodejing/gen_content.py` reads those files and writes `main/ddj_text.h`
and `main/ddj_text.c`. The point worth copying is that the header's constants are
**computed from the data**, not written by hand:

```c
#define DDJ_TOTAL_CHAPTERS 81
#define DDJ_DAO_LAST_CHAPTER 37

#define DDJ_CHAPTER_COUNT 1
#define DDJ_PASSAGE_COUNT 6
#define DDJ_POINT_COUNT 5
#define DDJ_OPTION_COUNT 3
#define DDJ_PONDER_OPTION_COUNT 3
```

`DDJ_TOTAL_CHAPTERS` is the one constant that is *not* derived, and that is
deliberate: the book has 81 chapters whether or not they are curated yet, and
progress is stored for all 81 from the first day, so the save format does not
change as chapters are added. Everything else — how many chapters exist, how many
reading screens, how many options — is summed from the sources. A hand-maintained
count goes wrong the first time somebody adds a chapter, and the failure mode is a
chapter that is silently short.

## Generated files must say they are generated

Both outputs open with a banner:

```c
// Generated by tools/daodejing/gen_content.py from tools/daodejing/chapters/*.txt.
// Do not edit by hand; edit the chapter source and run the generator.
```

`main/ddj_text.c` is a small file of string literals that nobody will read.
Without the banner it is a prime candidate for a "quick fix" that the next
regeneration silently deletes.

## A `--check` mode so CI can catch a stale file

The generator supports `--check`, which reports whether the committed tables still
match the sources, without writing anything:

```bash
python3 tools/daodejing/gen_content.py --check
```

This is what turns "remember to regenerate" into a rule.
`tools/validate.sh --static` runs it first, before any test. Without it, editing
the chapter file and forgetting the generator produces a build that compiles fine
and ships the old text — the worst kind of failure, because everything looks
healthy.

The font generator has the same flag for the same reason, and it is wired into the
same gate — but only when the tooling it needs is present:

```bash
if python3 -c "import fontTools" >/dev/null 2>&1; then
    PYTHONDONTWRITEBYTECODE=1 python3 tools/daodejing/gen_font.py --check
else
    echo "skip: gen_font.py --check (no fontTools; inventory sync not verified)" >&2
fi
```

Verifying the glyph inventory needs a character map, which needs `fontTools` and
the 16 MB source font. Neither belongs in a CI image. So the step *skips
loudly* — it prints that the check did not run — rather than being silently absent
from the gate or, worse, being written so that a missing dependency looks like a
pass. A gate step that cannot fail is not a check; a gate step that lies about
having run is worse than one that is missing.

## Chain the generators through one module

The two generators are linked by design, and not by convention: both import
`tools/daodejing/content.py`, which is the only thing that reads the chapter
files.

```text
tools/daodejing/chapters/*.txt
   └─ content.py         (parses; the single source of truth)
        ├─ gen_content.py  ->  main/ddj_text.{c,h}       (the tables)
        └─ gen_font.py     ->  assets/fonts/ddj_font_*.c (the glyphs)
```

Adding one character to a chapter can therefore require regenerating the font
subsets too. That is a feature, not a cost: the glyph inventory is the union of
the chapter text and the UI strings, so a new character is picked up
automatically instead of rendering as a blank box on one screen. Had the font
generator re-implemented the parser and read the chapter files directly, the two
could disagree about what the text says — and the symptom would be a missing glyph
in the one chapter nobody proof-read.

Run both, then run the gate.

## Keep the invariants tested where they can fail fast

The generator validates the text at generation time. The host tests re-check the
*generated data* so a stale commit is caught even if nobody runs the generator:

```c
for (int index = 0; index < DDJ_CHAPTER_COUNT; index++) {
    const ddj_chapter_t *chapter = ddj_chapter_at(index);
    assert(chapter->number == index + 1);
    assert(chapter->option_first == (uint16_t)(index * DDJ_PONDER_OPTION_COUNT));
}
assert(passage_sum == DDJ_PASSAGE_COUNT);
assert(DDJ_OPTION_COUNT == DDJ_CHAPTER_COUNT * DDJ_PONDER_OPTION_COUNT);
```

The `option_first` assertion is the one to keep. Each chapter's three answers live
in one shared table, so a chapter record holds only an offset into it. If a chapter
were generated with an offset that is not a multiple of three, that chapter's three
answers would silently be three *other* chapters' answers. A structural rule about
the generator's output is exactly the kind of thing that is invisible in review and
trivial as a host test.

## Workflow after any content change

```bash
# 1. Edit tools/daodejing/chapters/NNN.txt
python3 tools/daodejing/gen_content.py          # regenerate the C tables
python3 tools/daodejing/gen_font.py             # regenerate the glyph subsets
python3 tools/daodejing/gen_content.py --check  # confirm nothing is stale
python3 tools/daodejing/gen_font.py --check
./tools/validate.sh --static                    # data invariants, host tests
```

## Check list

- One curated file per chapter is the single source of truth for the text, and it
  is never edited in C.
- The file format's unit matches the domain's unit, with no syntax to escape.
- Positional data (the three answers) is enforced by the grammar, not by comment.
- Every malformed input is rejected with the file name and line number.
- Screen-fit limits are enforced at generation time, and the message names the fix.
- The same limits are declared on the rendering side and guarded with
  `_Static_assert`.
- The generator derives every count from the data instead of hard-coding it.
- Generated files carry a "do not edit by hand" banner naming the generator.
- A `--check` mode exists so CI can catch a stale output.
- Both generators read the content through one shared module, so the tables and
  the glyph inventory cannot disagree.
- Data invariants are asserted in host tests, not only in the generator.

## Related documents

- [Dao De Jing daily reading app](daodejing-daily/README.md) — the application this
  content drives.
- [Cutting a CJK font subset for LVGL](cjk-font-subsetting-for-lvgl.md) — the
  second consumer of the same chapter sources.
- [Keeping application logic on the host](host-testable-app-logic.md) — where the
  data invariants are asserted.
- [Letting font metrics drive the layout](font-metrics-driven-layout.md) — where
  the screen-fit limits come from.
- `tools/daodejing/content.py`, `tools/daodejing/gen_content.py`,
  `tools/daodejing/gen_font.py`, and `tools/daodejing/chapters/` — the pipeline
  itself.
