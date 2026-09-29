<p align="right">
  <a href="question-bank-pipeline.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Turning a prose bank into generated C tables

Recorded while porting the [Qiaopi Quiz app](qiaopi-quiz/README.md), a
fill-in-the-blank game whose 91 questions come from real overseas-remittance
letters. The pipeline is small enough to describe in one page, and every rule in
it exists because breaking it produced a specific failure.

## The problem: the content is prose, the device wants a table

A question is not a record with a fixed shape. It carries a sentence with a blank
in it, four candidates, an index saying which one is right, an explanation that
cites a classical source, the sender and year of the letter it came from, and the
complete original passage. The sentence can be 11 characters or 22; the
explanation runs from 30 to 56.

So the data model is a **flat struct per question, and nothing clever**. There is
no per-category table, no offsets, no joined strings. 91 questions of seven
pointers each is under 3 KB of pointers and it is obvious to read.

## One file, one block per question, and a format that is boring on purpose

The source of truth is `tools/qiaopi/bank.txt`. One block per question, fixed key
order, one value per line:

```
== q01
category: <one of six themes>
sentence: <one line from a letter, with ____ marking the gap>
options: <four candidates, separated by " / ">
answer: <0-based index of the correct candidate>
explain: <the usage or allusion behind the answer>
source: <sender and year>
full: <the complete original passage>
```

The format is deliberately dull so that every failure is a loud one. A missing
key, an unknown key, a repeated key, five candidates instead of four, an answer
index out of range — all abort the generator rather than quietly shipping a
question with a default value.

**Why one file and not one file per question.** The previous app in this
repository used one file per chapter, because chapters were hundreds of lines
each. Here a question is eight short lines, so 91 files would be 91 diff noise
sources and a directory listing nobody reads. One block per question in one file
still gives a clean diff — a change shows as one line in one block — while
keeping the whole bank reviewable in a single scroll.

## The extractor runs once; after that the file is the source

`extract_bank.py` pulls the questions out of the web version's `index.html`. It is
a **one-shot bootstrap tool**: it refuses to overwrite an existing `bank.txt`
unless given `--force`, and its header says so. Otherwise there would be two
sources of truth, and editing the bank would be silently undone the next time
someone re-ran the extractor.

The extractor audits before writing: candidate count, answer range, the correct
candidate appearing in the full passage (which catches rows shifted by one), and
exactly one blank marker per sentence. On the first run it reported 91 questions
and zero violations; that is the only reason the file was written at all.

## Two sentence forms, and both are capped

This is the rule that is easy to get wrong, because the two forms are rendered on
different pages and only one of them is the one you are looking at while coding:

- the **ask page** can only show the blank, so it renders the sentence with the
  blank replaced by a slot of the same character count as the source text;
- the **reveal page** shows the sentence with the correct candidate filled in.

They differ in length by up to two characters. Capping only the filled form —
which is the natural thing to do, since the filled form is what you picture —
means the ask page can overflow on a question the cap says is fine. Both are
checked, both in the generator and again in the host test against the real bank.

## Generator constraints are arithmetic, not taste

The device is 240x320 with a 210 px content area, and a Chinese glyph's advance
equals the font size, so "characters per line" is a division:

| Field | Font | Per line | Lines | Cap | Longest shipped |
| --- | --- | --- | --- | --- | --- |
| sentence, slot form | 24 px | 8 | 3 | 22 | 22 |
| sentence, filled form | 24 px | 8 | 3 | 21 | 19 |
| option | 24 px | 8 | 1 | 6 | 4 |
| explanation | 24 px | 8 | scrolls | 60 | 56 |
| full original | 24 px | 8 | scrolls | 48 | 46 |
| provenance | 16 px | 13 | 2 | 26 | 21 |
| category | 16 px | 13 | 1 | 6 | 4 |

Every one of those caps was raised at least once during the port, because the
first attempt was a guess. The measured column is why they are now believable.

## `--check` is what turns "remember to regenerate" into a rule

```
python3 tools/qiaopi/gen_content.py            # rewrite the tables
python3 tools/qiaopi/gen_content.py --check    # fail if they are stale
```

`--check` is what `tools/validate.sh` and CI run. Without it, editing the bank and
forgetting the generator produces a build that compiles fine and ships the old
questions — the worst kind of failure, because everything looks healthy. The
audio generator and the font generator have the same flag, for the same reason.

## One module owns the content facts

`content.py` is imported by both `gen_content.py` (the C tables) and `gen_font.py`
(the glyph inventory). Neither re-implements "what characters are in the bank".
If the two disagreed, the tables could reference a glyph the font subset does not
have, and the failure would be a blank box on a device, in the middle of a
sentence, days later.

## What the parser refuses

| Condition | Why it must fail loudly |
| --- | --- |
| Unknown key, duplicate key, missing key | A typo would otherwise be ignored and the question would ship with a default. |
| Candidate count not four | The ask page renders exactly four rows; a fifth has nowhere to go. |
| Answer index out of range | The reveal page indexes the options with it directly. |
| Correct candidate absent from the full passage | The near-certain signature of a parse that shifted by one record. |
| Blank marker missing, or present twice | The layout assumes exactly one slot to draw. |
| Over-long field | It would be clipped on screen rather than failing the build. |
| Duplicate sentence | Two identical questions in a 20-question round look like a bug to the reader. |

## Related

- [Qiaopi Quiz app](qiaopi-quiz/README.md) — the application this pipeline feeds.
- [Subsetting a CJK font for LVGL](cjk-font-subsetting-for-lvgl.md) — the other
  consumer of the same content module.
- [Keeping application logic on the host](host-testable-app-logic.md) — how the
  generated tables are asserted against the real budgets.
