<p align="right">
  <a href="host-testable-app-logic.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Keeping application logic on the host

Written for the [Qiaopi Quiz app](qiaopi-quiz/README.md), a port of a web quiz to a
three-key handheld. The firmware is roughly 1900 lines of interface and service
code, but the part that decides anything — how a round is drawn, how it is scored,
how text is broken into lines, how audio is decoded, how progress is stored — is
about 890 hand-written lines plus 1333 generated ones that never touch ESP-IDF or
LVGL. Those are compiled and run on the development machine, against 1658 lines of
tests, with no board attached.

The payoff is not philosophical. It is that a scoring bug is a two-second test run
instead of a flash cycle, and that several real defects were found before anything
was ever flashed.

## Where the line is drawn

Pure, and therefore testable on the host:

| Module | Lines | Responsibility |
| --- | --- | --- |
| `qpq_session` | 233 | Round state machine, scoring, streak bonus, question draw |
| `qpq_progress` | 155 | Save format, seen-question bitmap, rejection paths |
| `qpq_wrap` | 157 | UTF-8 line breaking at a character budget |
| `qpq_content` | 118 | Question access, sentence forms, rank thresholds |
| `qpq_adpcm` | 120 | Streaming IMA-ADPCM decode |
| `qpq_audio_index` | 106 | Audio blob header and index validation |

Not testable on the host, and kept as thin as possible: `qpq_player` (ISR-adjacent
audio output), `qpq_store` (NVS), `qpq_ui` and the four pages. The rule for
deciding which side a piece of code belongs on: **if it can be expressed as a
function of its arguments, it goes on the host.**

## Make the two implementations agree by construction, then prove it

The audio blob is produced by a Python encoder and played by a C decoder. If those
two ever disagree, the symptom is a subtle distortion that is nearly impossible to
attribute. So the generator emits a fixture — a 64-sample signal, the bytes the
Python encoder produced for it, and the sequence the Python decoder read back —
and the C test decodes the same bytes and asserts equality sample by sample:

```c
for (uint32_t i = 0; i < QPQ_ADPCM_FIXTURE_SAMPLES; i++) {
    assert(s_bulk[i] == qpq_adpcm_fixture_expected[i]);
}
```

This is the strongest test in the repository, because it fails on either side of
the boundary. It has already caught one real defect: the C stream reader opened
with its sample cursor at 1 instead of 0 and silently dropped the first sample of
every clip.

The fixture also documents what is *not* guaranteed. Its second half is a
full-scale alternating square wave, which ADPCM cannot track at this sample rate;
the quality assertion covers only the first 48 samples, which are a decaying sine.
Claiming a bound over the whole fixture would have been a lie that happened to pass.

## Test the rejection paths, not just the happy one

Both serialised formats are attacked deliberately, because the failures that
matter happen on a device that lost power mid-write, not on the first run:

- **The audio blob index** has nine rejection tests: null pointer, truncated
  header, truncated index, wrong magic, wrong version, wrong sample rate, a zero
  sample count, an offset that skips a clip, a clip that overruns the blob, and a
  length that disagrees with what the index implies.
- **The save blob** has six: wrong length, wrong magic, wrong version, a flipped
  bit in the body, a flipped bit in the seen bitmap, and a corrupted checksum.

Both suites assert not only that the failure is reported but that **the caller's
structure is left untouched**. That is what the second test found: a failed
`qpq_audio_index_open` returned early without clearing the index, so a caller that
ignored the return value would keep addressing clips through the *previous*
successful open — a bug that runs fine and reads the wrong memory.

## Tests should link the real data, not a copy of it

The wrap test links `qpq_content` and `qpq_text` and walks the whole bank, in both
sentence forms, asserting that no wrapped line exceeds its layer's budget:

```
ok  real sentences wrap: at most 3 lines (budget 3), at most 8 chars (budget 8)
ok  ask-page slot form:  at most 3 lines, at most 8 chars
```

A test written against a copy of the content in the test file would have passed on
the copy's behaviour and said nothing about the app. Linking the real tables is
what makes "it fits on one screen" a checkable claim rather than a hope — and it
found the budget mismatch between the two sentence forms.

One wrong expectation surfaced this way too: a six-character sentence was asserted
to wrap onto two lines, but six characters fit one line, so there is no break at
all. The test was wrong, the code was right, and the only reason that was
discoverable is that the test was checked against reality.

## Make the randomness reproducible

The round state machine contains the only randomness in the application, and it
uses a xorshift32 seeded by the caller rather than `rand()`. A fixed seed produces
a fixed draw, so "a round of 20 contains no duplicates" is a fact the test asserts,
not a probability it hopes for. It also makes the seen-question preference rule
testable: mark all but three questions as seen and assert those three appear.

## Related

- [Qiaopi Quiz app](qiaopi-quiz/README.md) — the modules and the pages on top.
- [Letting font metrics decide the layout](font-metrics-driven-layout.md) — the
  budgets the content test asserts against.
- [Building ESP-IDF firmware from Git Bash on Windows](windows-git-bash-esp-idf.md)
  — how to get a host compiler, and why some tests behave differently from these.
