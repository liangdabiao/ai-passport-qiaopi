<p align="right">
  <a href="host-testable-app-logic.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Keeping Application Logic on the Host

Written for the [Dao De Jing daily reading app](daodejing-daily/README.md), whose
application code is about 2,300 lines — five pages, a four-layer reading flow, a
versioned save format, and a hand-written CJK line breaker. Roughly 610 lines of
that logic, plus a 44-line generated content table, runs and is tested on a PC
with no board attached, backed by 727 lines of host tests.

## The split that makes it possible

The application has two kinds of module, and the boundary is a single rule: a
**pure** module must not include `esp_*.h`, `lvgl.h`, `nvs.h`, `freertos/*`, or
anything else from the platform. It takes plain C types, returns plain C values,
and holds no hardware state.

| Pure (host-tested) | Hardware-bound (device only) |
| --- | --- |
| Chapter table access, offsets, and labels | The five UI pages |
| The four-layer session state machine | Key dispatch and page navigation |
| CJK line breaking | NVS load and save |
| The progress model, the re-reading queue, serialization | Sound playback |

The payoff is not just "tests are nice". It is that the whole rules layer can be
exercised exhaustively, in milliseconds, on the machine you are already typing
on:

```bash
cc -std=c11 -Wall -Wextra -Werror -I main \
   tests/test_ddj_wrap.c main/ddj_wrap.c main/ddj_chapter.c main/ddj_text.c \
   -o test_ddj_wrap && ./test_ddj_wrap
```

Two details in that command are deliberate. `-Werror` makes the host build a
harsher compiler than the firmware build. And the test links `ddj_chapter` and
`ddj_text` as well, so it asserts on **the content that will actually be on the
screen** rather than on a sample copied into the test file — a chapter edit is
noticed immediately instead of at the next firmware build.

## Make every entry point keep a promise

A module that returns `bool` is only testable if the failure path is specified.
The rule used throughout: **on failure, do not touch the output.** Callers then
never have to inspect a half-written structure, and tests can assert the promise
directly rather than only checking the return value.

```c
/* screen_1 is the chapter's first reading screen: 8 characters, 24 bytes of
   UTF-8. Wrapped at 6 characters per line it becomes two lines, so the output is
   25 bytes -- the text plus one inserted break. */
assert(ddj_wrap_utf8(screen_1, 6, out, 25) == 0);
assert(out[0] == '\0');    /* 25 bytes of output needs 26 with the terminator */
assert(ddj_wrap_utf8(screen_1, 6, out, 26) == 25);
```

Note what the boundary is measured on: the wrapped **output**, not the input. The
input is 24 bytes; the break the function inserts makes it 25. Sizing the check
from the source text would have been one byte short, and the test would have
"passed" with a buffer that truncates on the device.

That pair is worth more than either half: the first line proves the boundary is
detected, the second proves the boundary is where the comment says it is. Off by
one byte in the other direction and the first assertion would still pass.

The same applies to invalid indices and null pointers, which are asserted
explicitly instead of being left as "probably won't happen":

```c
assert(ddj_chapter_at(-1) == NULL);
assert(ddj_chapter_at(DDJ_CHAPTER_COUNT) == NULL);
assert(ddj_chapter_passage(NULL, 0) == NULL);
assert(ddj_progress_read_count(NULL) == 0);
assert(ddj_progress_pending_at(NULL, 5, 0) == -1);
```

It is tempting to skip these. A `NULL` check that is asserted in a host test is
one you know exists; one that is only reasoned about is one that will be missing
from the path that actually gets hit.

One boundary needs a weaker promise, and it is worth stating rather than hiding:
a zero-capacity buffer cannot be written to at all, so `ddj_chapter_label` only
guarantees its return value there, not a cleared string. The header says so
explicitly, and the test asserts only the return value. A contract that promises
more than it can deliver is worse than one that admits the gap — the test written
against the too-strong version fails, and the failure looks like a code bug.

## Sweep it all, not a sample

Host tests are cheap enough to check every case rather than a representative one.
Each of these loops runs in well under a millisecond:

- All **81 chapter slots** for the progress model: the stance accessor returns
  "no stance" for both ends of the range and for out-of-range indices.
- All **six passages, five commentary points, and three answers** of the curated
  chapter, wrapped and checked against the per-line budget, the line-count ceiling,
  and the punctuation rule.
- All **81 chapters** in the re-reading queue queries, at every prefix length from
  `0` to `chapter_count`.
- The **round trip** of the save blob, plus nine separate corruptions of it.
- The **key sequence space** of the session state machine: move at the top and the
  bottom of each layer, step down, step back, and cancel from every stage.

That last one is worth dwelling on. The flow's rules — "down at the last sentence
advances to the commentary layer", "back at the first sentence leaves without
recording anything", "the stance is readable from the session, not stored
separately" — are exactly the things a tester would otherwise have to hunt for by
pressing buttons on a device, and they are 133 lines of assertions here.

## Test the data and the constants, not just the functions

Two classes of assertion caught problems that no amount of reading would have:

**Structural invariants of the generated data.**

```c
for (int index = 0; index < DDJ_CHAPTER_COUNT; index++) {
    const ddj_chapter_t *chapter = ddj_chapter_at(index);
    assert(chapter->number == index + 1);
    assert(chapter->point_count >= 3 && chapter->point_count <= 5);
    assert(chapter->option_first == (uint16_t)(index * DDJ_PONDER_OPTION_COUNT));
}
assert(passage_sum == DDJ_PASSAGE_COUNT);
assert(point_sum == DDJ_POINT_COUNT);
assert(DDJ_OPTION_COUNT == DDJ_CHAPTER_COUNT * DDJ_PONDER_OPTION_COUNT);
```

The `option_first` assertion is the one that matters. Each chapter's three
answers live in one shared table, and the chapter record stores only an offset
into it. If a chapter were generated with an offset that is not a multiple of
three, that chapter's three answers would silently be three *other* chapters'
answers — a data bug that reading the code cannot reveal.

**Self-consistency of derived constants.**

```c
assert(DDJ_NOTES_BYTES == 81);
assert(DDJ_BITMAP_BYTES == 11);          /* (81 + 7) / 8 */
assert(DDJ_PROGRESS_BLOB_SIZE == 4 + 81 + 11 + 11 + 1);
assert(DDJ_PROGRESS_BLOB_SIZE == 108);
```

These are the constants that decide how many bytes go into the save blob. Get one
wrong and the save is short by a byte; the checksum catches it on load, but only
on a device, only after a power cut.

## Round-trip the save format, then abuse it

Serialization deserves more than a happy-path test, because the data crosses a
power failure and a flash controller:

```c
assert(ddj_progress_deserialize(&back, blob, size));       // round trip
assert(ddj_progress_slot(&back, 0) == DDJ_SLOT_CHEWING);
assert(ddj_progress_slot(&back, 80) == DDJ_SLOT_MISSED);
assert(ddj_progress_read_count(&back) == 2);
```

Corrupt the magic, the version, the length, and the checksum, one at a time, and
assert rejection every time. Two of those need naming because they are easy to
miss:

- **Truncation and extension both.** `sizeof(bad) - 1` and `sizeof(bad) + 1` are
  both rejected. A length check written as `< expected` catches the first and lets
  the second through.
- **A out-of-range value behind a valid checksum.** The stance byte is set to a
  number past the last enum value and the checksum is recomputed so it is
  correct. A format that trusted the checksum alone would accept it and then index
  past the end of the stance table. Rejecting it is a separate assertion on the
  decoded value, not on the container.

And the promise from the previous section applies here too, which is why the test
sets a sentinel first:

```c
untouched.sessions = 777;
assert(!ddj_progress_deserialize(&untouched, bad, sizeof(bad)));
assert(untouched.sessions == 777);   // a rejected load must not half-apply
```

A format that cannot be rejected is a format that will be half-applied at the
worst moment.

## Semantic rules belong in the pure layer too

Anything the reader would notice as "the app behaving strangely" is a rule, and
rules go where they can be tested. The re-reading queue is the clearest example,
because its whole value is that it agrees with what the reader recorded:

```c
ddj_progress_set_slot(&pending, 0, DDJ_SLOT_LANDED);
ddj_progress_set_slot(&pending, 1, DDJ_SLOT_CHEWING);
ddj_progress_set_slot(&pending, 2, DDJ_SLOT_MISSED);
assert(ddj_progress_pending_count(&pending, 3) == 2);   // "landed" is not pending
assert(ddj_progress_pending_count(&pending, 1) == 0);   // chapter 1 is neither
assert(ddj_progress_pending_count(&pending, 2) == 1);
```

Two rules fall out of those three lines, and both are ones a reader would notice:
a chapter they said had *landed* must not come back asking to be read again, and a
chapter that has not been curated yet must not appear at all. The queue is a pure
function of stored state — it is **derived, never stored** — which is what makes
it impossible for the queue and the recorded stances to disagree. Had the queue
been a second piece of saved data, updating both would have been a rule, and rules
in the save path are exactly the ones that break on the device.

The saturation case is asserted in the same spirit: the session counter stops at
`UINT16_MAX` instead of wrapping to zero and telling the reader their practice
count went backwards.

## What not to try to host-test

Do not stretch this pattern to cover the UI. Layout, widget lifetimes, key
timing, and NVS need the device or the repository's stub-based demo runtime
tests. The goal is to shrink the set of things that *can only* be checked on
hardware, not to eliminate it — the pure layer above covers the rules, the queue,
the line breaker, and the save format, and leaves the LVGL layer to be checked on
a real board.

## Bugs this found

The per-line budget assertion found the line breaker's rule was wrong before it
ever reached a panel. The first version kept closing punctuation off the start of
a line by pulling it *up* onto the previous line — which makes that line one
character too wide, so LVGL wraps it a second time and the line count stops being
predictable, invalidating the whole vertical budget. Asserting
`max_line_chars(out) <= BUDGET` against the real chapter text makes that
impossible to reintroduce. The rule is now "break one character early".

The same test then caught a wrong expectation of mine. I had asserted that a
six-character string would break into five characters and one, and the assertion
failed: the string is exactly six characters, so it fits one line and there is no
break at all. The test was wrong, not the code — but a test that is *checked*
against reality is how you find out which one is wrong. A test that had been
written against a copy of the content in the test file would have "passed" on the
copy's behaviour and told me nothing about the app.

## Check list

- Pure modules include no platform headers; the boundary is stated in a comment
  at the top of each one.
- Every entry point documents and enforces "on failure, do not modify the
  output" — and where the promise has to be weaker, the header says so.
- Invalid index, null pointer, and zero-capacity cases are asserted, not assumed.
- Loops sweep every chapter, layer, and list length, not a sample.
- Tests link the real content module, so assertions are about what ships.
- Data-shape invariants (offsets, count sums, byte widths) and derived constants
  are asserted.
- The save format is tested for rejection — truncation, extension, bad magic, bad
  version, bad checksum, and out-of-range values — not only for round trips.
- User-visible rules (the re-reading queue, counter saturation) are asserted in
  the pure layer.
- Every test is wired into `tools/validate.sh --static`, so it runs in CI and
  locally without anyone remembering to run it.

## Related documents

- [Dao De Jing daily reading app](daodejing-daily/README.md) — the modules and
  tests referenced here.
- [Building ESP-IDF firmware from Git Bash on Windows](windows-git-bash-esp-idf.md) —
  how to get a host compiler and why the repository's stub-based demo tests
  behave differently from these.
- [Curating chapter content into generated C](chapter-content-pipeline.md) — the
  generator whose output these tests re-check.
- `docs/development/engineering/build-and-test.md` — the shared validation gate.
- `tools/validate.sh` — where each test's source list is spelled out.
