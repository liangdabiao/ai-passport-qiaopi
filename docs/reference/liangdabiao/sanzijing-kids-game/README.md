<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# San Zi Jing Kids Game

A three-button learning game that turns the AI Passport into a pocket recitation
tutor for the *Three Character Classic* (San Zi Jing), the classic Chinese
priming text. The whole text lives inside the firmware — 404 three-character
lines grouped into 101 lessons — so it works with no network, no card, and no
phone.

The application is a worked example of three things this repository asks for and
that are easy to get wrong: redesigning the UI instead of reusing the demo
shell, subsetting a CJK font down to exactly the glyphs you need, and keeping
the game logic on the host so it can be tested without the board.

## What it does

- **The full text is built in.** 404 lines, 1,212 characters, 565 distinct
  characters, in 101 lessons of four lines each. Nothing is fetched at runtime;
  the text is a generated C array.
- **Pick-the-next-line quiz.** Each lesson asks three questions. Question *i*
  shows one line and asks which of three lines comes next. Wrong options are
  pulled from *other* lessons, so you cannot score by feel alone.
- **Wrong-answer notebook.** Every line you miss is recorded in a bitmap. The
  notebook replays up to six of them as a short review; a line you then get
  right is removed from the notebook.
- **Flashcards.** A three-cell practice grid draws the current line character by
  character at 32 px, above the full lesson as two six-character half-lines.
- **Stars.** Up to three per lesson, one per question answered correctly on the
  first try. Stars only ever go up: replaying a lesson cannot lower them.
- **Progress survives power loss.** Stars, the wrong-answer bitmap, and the
  flashcard position are stored in NVS as one 156-byte blob guarded by a magic
  byte, a version, and a checksum.
- **Battery gauge** in the top bar, falling back to `--` when the fuel gauge
  cannot be read.
- **Six synthesized sound effects** (move, enter, back, correct, wrong, lesson
  complete) generated on-board from RTTTL strings — no audio assets in Flash.

## Interaction

Three keys drive everything. Two rules are global: UP and DOWN move the cursor
inside the current page, and a long press on OK always means "go back one
level".

**Home — four entries: start learning, flashcards, wrong-answer notebook,
settings.**

- **UP / DOWN (click)**: move the menu cursor.
- **OK (click)**: activate the highlighted entry.
- Picking the wrong-answer notebook while it is empty plays a rejection sound
  and stays put, rather than opening an empty page.

Each row also carries a right-aligned status: the lesson you are up to (or the
equivalent of "review from the start" once all 101 are done), the current
flashcard position, and how many lines are in the wrong-answer notebook. The
bottom bar sums up your stars and how many lessons you have passed.

**Quiz page.**

- **UP / DOWN (click)**: move between the three options.
- **OK (click)**: submit. The page locks briefly while the answer is marked, so
  a bouncing finger cannot answer the next question by accident.
- **OK (long press)**: back to home (also used to leave the result screen).

**Flashcards.**

- **UP / DOWN (click)**: previous / next line.
- **OK (click)**: jump to the first line of the next lesson. At one press per
  line, reaching the end of a 404-line text would take 404 presses. Pressing it
  in the last lesson plays a rejection sound rather than wrapping around.
- **OK (long press)**: back to home.

**Settings.**

- **UP / DOWN (click)**: move between rows.
- **OK (click)**: toggle the sound, or start the two-step progress reset.
- **OK (long press)**: back to home.

The two rows sit above an information panel that states the size of the text
(404 lines, 1,212 characters, 101 lessons) and what the firmware is built on.

Resetting progress is irreversible, so it takes two presses: the first turns the
row into a red warning and rewrites the hint line, and only the second press
erases anything. Any other key in between counts as backing out.

## Lesson and quiz model

A lesson is four consecutive lines. For lesson `L`, line `4L+i` is the prompt
for question `i`, and the answer is the line right after it, `4L+i+1`. Question
2 therefore needs line `4L+3`, which exists — so every one of the 101 lessons
yields exactly three questions and the last lesson does not run off the end.

A distractor is accepted only if it is a valid line index, is not the prompt or
the answer, does not come from the prompt's own lesson, and does not repeat the
prompt text, the answer text, or an already-chosen option. Candidates are drawn
with a fixed-seed linear congruential generator, with a linear fallback scan if
64 random attempts all fail. Because the seed depends only on
`(lesson, question)`, the same lesson always produces the same three questions —
which is what makes the host tests meaningful and lets the review mode regenerate
a question for a line you missed.

The review mode reuses the same builder by treating the missed line as an answer
and its predecessor as the prompt.

Two invariants make this safe, and both are checked rather than assumed: the
text file must be a multiple of four lines (404 = 4 × 101), and no two adjacent
lines may have identical text (otherwise a question has two correct answers).
A three-character line is nine UTF-8 bytes plus the terminator, which the
generator pins down as a constant.

## Progress and persistence

The stored record is a fixed-size structure rather than a text format, so a
corrupt or truncated blob is rejected instead of half-applied:

| Field | Size | Meaning |
| --- | --- | --- |
| `magic` | 1 byte | `0x53`; a quick "is this ours?" check |
| `version` | 1 byte | Format version, currently 1 |
| `card_line` | 2 bytes, little-endian | Flashcard position |
| `stars` | 101 bytes | Stars per lesson, 0–3 |
| `wrong` | 51 bytes | Bitmap of missed lines (`(404+7)/8`) |
| `checksum` | 1 byte | Trailing checksum over the payload |

156 bytes total. Deserialization refuses the data if the magic, version, length,
or checksum disagree, and leaves the caller's structure untouched. Recording a
lesson takes the maximum of the old and new star count, so a replay can only
help.

## UI and fonts

The UI is written from scratch; the baseline test menu and its `ui_pixel` shell
are not reused. The look is a warm paper-and-ink classroom: a rice-paper body, a
dark ink top bar carrying the title and battery, a bamboo-slip title plate on
the home page, and cinnabar red for the active element.

Rendering Chinese needs a font that actually contains the glyphs — Montserrat,
which ships with LVGL, has none. The three subsets in `assets/fonts/` are cut
from Noto Sans CJK SC at 16, 24, and 32 px, 4 bits per pixel, uncompressed, and
cover 785 code points that were derived from the source text and the UI strings
rather than guessed. Coverage was verified against the parent font's character
map, one code point at a time, with zero missing. See the companion experience
entry for the pipeline and its cost.

Two details are worth calling out because they bite anyone doing this:

- **The glyph inventory is generated, not hand-listed.** The generator unions the
  characters that appear in the text and in the UI's own string literals with the
  printable ASCII range, so adding a menu label cannot silently produce blank
  text.
- **C source size is not Flash cost.** The three generated `.c` files total about
  3.7 MB of hex literals, but the compiled bitmaps are 571 KB of Flash. Judging
  the budget from the file listing would be off by a factor of six.

## Implementation map

All application code is new and lives in `main/`. The BSP is reused unchanged.

| File | Responsibility |
| --- | --- |
| `main/szj_text.c`, `.h` | Generated: the 404 lines and the text-shape constants |
| `main/szj_text_util.c`, `.h` | Line and lesson lookup helpers on top of the generated array |
| `main/szj_quiz.c`, `.h` | Question generation and answer checking (pure) |
| `main/szj_session.c`, `.h` | One practice sitting as a state machine (pure) |
| `main/szj_progress.c`, `.h` | Stars, wrong-answer bitmap, serialization (pure) |
| `main/szj_store.c`, `.h` | NVS namespace and blob load/save |
| `main/szj_sound.c`, `.h` | The six effects on top of the RTTTL player |
| `main/rtttl_player.c`, `.h` | Synthesized audio, reused from `demo/tetris-game` |
| `main/szj_ui.c`, `.h` | Theme, page frame, and the row/option/grid widgets |
| `main/szj_home.c`, `szj_lesson.c`, `szj_card.c`, `szj_settings.c` | The four pages |
| `main/szj_app.c`, `.h` | Key semantics and page navigation |
| `main/main.c` | Application entry: bring up BSP, store, sound, then load the UI |
| `tools/sanzijing/gen_content.py` | Text file to `szj_text.c` |
| `tools/sanzijing/gen_font.py` | Charset inventory and font subsetting |
| `tests/test_szj_*.c` | Host tests for the four pure modules |

The four pure modules — text helpers, quiz, session, progress — do not include
ESP-IDF, LVGL, or NVS headers, so they compile and run on a PC. Only the UI, the
store, and the entry point touch hardware.

## Build and verification

Build with the repository's shared gate:

```bash
./tools/validate.sh --static      # repository checks and host tests
./tools/validate.sh --firmware    # cold build plus merged image
```

Results on the recorded build:

- **Firmware gate: pass.** Application image 1,348,800 bytes in an 8,323,072-byte
  factory partition — 84% free. Merged image 1,414,336 bytes, written from
  offset `0x0`.
- **Host tests: pass.** Four new tests (`test_szj_text_util`, `test_szj_quiz`,
  `test_szj_progress`, `test_szj_session`) are wired into `--static`, covering
  text-shape invariants, option legality and determinism, serialization
  round-trips including corrupt input, and the full session state machine.
- **Repository checks: pass** (`check_repo.py`, `actionlint`, and the BSP button,
  LVGL-init, and audio tests).
- **Memory:** DRAM 147,984 of 321,296 bytes (46%), 173 KB free. Text is 527,628
  bytes of code; fonts and the text array sit in `.rodata` and are read straight
  from Flash, so they cost no RAM.
- Known local limitation: the four pre-existing `test_demo_*_runtime` tests fail
  on this host because they rely on the GNU linker dropping undefined LVGL stub
  symbols, which the host linker here does not do. The same failures reproduce on
  a pristine checkout, so they are unrelated to this application.

## Not verified on hardware

Nothing was flashed during development, so the following still need a real
board: the panel and its rounded-corner mask, the three-key ADC ladder's feel
and debounce, the ES8311 effects, the CW2017 battery reading, NVS persistence
across an actual power cut, and how the LVGL pool behaves across long sessions of
page switching.

## Source

- Repository: `liangdabiao/ai-passport`, branch `feature/sanzijing-kids-game`
  (<https://gitee.com/FoloToy/ai-passport>).
- Text: the widely circulated 1,212-character edition of the *Three Character
  Classic*, cross-checked against two independent online editions. Seven lines
  were corrected and two omitted blocks restored; the curated result is
  `tools/sanzijing/sanzijing.txt`.
- Font: Noto Sans CJK SC Regular (SIL Open Font License 1.1), subset with
  `lv_font_conv` 1.5.3.

## Related

- [Cutting a CJK font subset for LVGL](../cjk-font-subsetting-for-lvgl.md)
- [Letting font metrics drive the layout](../font-metrics-driven-layout.md)
- [Keeping application logic on the host](../host-testable-app-logic.md)
- [Curating a curriculum text into generated C](../curriculum-text-curation-pipeline.md)
