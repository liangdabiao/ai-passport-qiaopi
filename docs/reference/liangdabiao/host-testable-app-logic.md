<p align="right">
  <a href="host-testable-app-logic.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Keeping Application Logic on the Host

Written for the [San Zi Jing kids game](sanzijing-kids-game/README.md), whose
application code is about 3,800 lines — a fully playable quiz, a review mode, an
unlock rule, and a versioned save format. Roughly 600 lines of that logic, plus
a 410-line generated text table, runs and is tested on a PC with no board
attached.

## The split that makes it possible

The application has two kinds of module, and the boundary is a single rule: a
**pure** module must not include `esp_*.h`, `lvgl.h`, `nvs.h`, `freertos/*`, or
anything else from the platform. It takes plain C types, returns plain C values,
and holds no hardware state.

| Pure (host-tested) | Hardware-bound (device only) |
| --- | --- |
| Text lookup helpers | The four UI pages |
| Question generation and answer checking | Key dispatch and page navigation |
| The practice session state machine | NVS load and save |
| Stars, wrong-answer bitmap, serialization | Sound playback |

The payoff is not just "tests are nice". It is that the whole rules layer can be
exercised exhaustively, in milliseconds, on the machine you are already typing
on:

```bash
cc -std=c11 -Wall -Wextra -Werror -I main \
   tests/test_szj_quiz.c main/szj_quiz.c main/szj_text_util.c main/szj_text.c \
   -o test_szj_quiz && ./test_szj_quiz
```

## Make every entry point keep a promise

A module that returns `bool` is only testable if the failure path is specified.
The rule used throughout: **on failure, do not touch the output.** Callers then
never have to inspect a half-written structure, and tests can assert the promise
directly rather than only checking the return value.

```c
assert(!szj_stanza_text(0, text, 8));    // buffer too small
assert(text[0] == '\0');                 // ...and the output was cleared
```

The same applies to invalid indices and null pointers, which are asserted
explicitly instead of being left as "probably won't happen":

```c
assert(szj_progress_total_stars(NULL) == 0);
assert(!szj_progress_is_wrong(NULL, 0));
assert(!szj_quiz_build(0, -1, NULL));
```

It is tempting to skip these. A `NULL` check that is asserted in a host test is
one you know exists; one that is only reasoned about is one that will be missing
from the path that actually gets hit.

## Determinism is a design requirement, not a nicety

The quiz picks its wrong options pseudo-randomly, but the seed depends only on
`(lesson, question)`:

```c
static uint32_t question_seed(int lesson, int question) {
    return (uint32_t)(lesson * SZJ_QUESTIONS_PER_LESSON + question) * 2654435761u + 12345u;
}
```

That single decision buys three things:

- Tests can assert exact content — question 7 of lesson 2 has the same three
  options every run, so a failure is reproducible instead of "it failed once".
- The review mode can regenerate the question for a line the player missed, from
  the line number alone, with no stored question data.
- "Randomness" stays honest: options are still spread unpredictably across
  sessions to a human, while the code stays deterministic to a test.

## Sweep it all, not a sample

Host tests are cheap enough to check every case rather than a representative one.
Each of these loops runs in well under a millisecond:

- All **404 lines**: non-null, exactly 9 UTF-8 bytes.
- All **101 lessons**: the combined text is exactly 36 bytes, and fits the
  64-byte buffer.
- All **101 × 3 = 303 questions**: the answer is the line after the prompt, the
  three options are in range and have three distinct texts, none of them is the
  prompt, and `correct_slot` really points at the answer.
- All **403 adjacent line pairs** for the "no two adjacent lines are identical"
  invariant.

That last one is worth dwelling on. A repeated line is not a code bug — it is a
data bug that produces **a question with two correct answers**, and on a device
it appears as one confusing question somewhere in lesson 60, months later. As a
host test on the generated array it is a five-line loop.

## Test the data and the constants, not just the functions

Two classes of assertion caught problems that no amount of reading would have:

**Structural invariants of the generated data.**

```c
assert(SZJ_LINE_COUNT % SZJ_LINES_PER_STANZA == 0);
assert(SZJ_STANZA_COUNT * SZJ_LINES_PER_STANZA == SZJ_LINE_COUNT);
```

If the text file ever grows to a count that is not a multiple of four, the last
lesson is truncated and the UI silently mis-groups it. This turns that into a
failed build.

**Self-consistency of derived constants.**

```c
assert(SZJ_PROGRESS_BLOB_SIZE ==
       SZJ_PROGRESS_HEADER_BYTES + SZJ_LESSON_COUNT + SZJ_WRONG_BYTES + 1);
assert(SZJ_WRONG_BYTES == (SZJ_LINE_COUNT + 7) / 8);
```

These are the constants that decide how many bytes go into the save blob. Get one
wrong and the save is short by a byte; the checksum catches it on load, but only
on a device, only after a power cut.

## Round-trip the save format, then abuse it

Serialization deserves more than a happy-path test, because the data crosses a
power failure and a flash controller:

```c
assert(szj_progress_deserialize(&back, blob, size));       // round trip
assert(back.card_line == 42);
assert(back.stars[7] == 2);
assert(szj_progress_is_wrong(&back, 11));

assert(!szj_progress_deserialize(&back, bad, sizeof(bad) - 1));   // truncated
```

Corrupt the magic, the version, the length, and the checksum, one at a time, and
assert rejection every time. A format that cannot be rejected is a format that
will be half-applied at the worst moment.

## Semantic rules belong in the pure layer too

Anything the player would notice as "the game behaving strangely" is a rule, and
rules go where they can be tested:

```c
szj_progress_record_lesson(&p, 0, 2);
szj_progress_record_lesson(&p, 0, 1);
assert(p.stars[0] == 2);        // stars never go down, even after a replay
```

That is the "replaying a lesson cannot cost you a star" rule, asserted in two
lines. Finding it by playing the game on hardware would take a very patient
tester.

## What not to try to host-test

Do not stretch this pattern to cover the UI. Layout, widget lifetimes, key
timing, and NVS need the device or the repository's stub-based demo runtime
tests. The goal is to shrink the set of things that *can only* be checked on
hardware, not to eliminate it — the pure layer above covers the rules, the
unlock logic, and the save format, and leaves the LVGL layer to be checked on a
real board.

## A bug this found immediately

`szj_quiz.c` used `uint32_t` without including `<stdint.h>`. It happened to
compile inside the ESP-IDF build, where some other header pulled the type in
transitively. Compiled standalone for a host test with `-Werror`, it failed on
the first try. That is the general argument for this whole approach: the host
test is a much harsher, much faster compiler for the pure layer, and it runs
before the firmware build's several-minute cycle.

## Check list

- Pure modules include no platform headers; the boundary is stated in a comment
  at the top of each one.
- Every entry point documents and enforces "on failure, do not modify the
  output".
- Invalid index and null pointer cases are asserted, not assumed.
- Randomness is seeded deterministically from stable inputs.
- Loops sweep every lesson, question, and line, not a sample.
- Data-shape invariants (multiples, byte widths, no adjacent duplicates) and
  derived constants are asserted.
- The save format is tested for rejection, not only for round trips.
- User-visible game rules are asserted in the pure layer.
- Every test is wired into `tools/validate.sh --static`, so it runs in CI and
  locally without anyone remembering to run it.

## Related documents

- [San Zi Jing kids game](sanzijing-kids-game/README.md) — the modules and tests
  referenced here.
- [Building this project on Windows with Git Bash](windows-git-bash-esp-idf.md) —
  how to get a host compiler and why the repository's stub-based demo tests
  behave differently from these.
- `docs/development/engineering/build-and-test.md` — the shared validation gate.
- `tools/validate.sh` — where each test's source list is spelled out.
