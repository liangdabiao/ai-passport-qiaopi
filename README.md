<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Qiaopi Quiz — fill in the blank on a three-key handheld

An offline game for a three-button handheld: **each round asks 20 questions**, and every question is
a line taken from a real *qiaopi* letter with one phrase blanked out — pick the missing phrase from
four candidates. All 91 questions, their narration and the background music live inside the device:
**no network, no card, no phone**.

*Qiaopi* are the letters and remittances that overseas Chinese sent home from Southeast Asia in the
19th and 20th centuries. The questions are drawn from the letters themselves and the answer page
names the sender and the year — so this is not a word bank to guess at, it is 91 pieces of history
you can actually read.

## How a round goes

| Page | What is on screen | The three keys |
| --- | --- | --- |
| **Title** | A large title, three entries — Start a round, Volume, Reset record — and a line of progress such as "seen 12/91 · best 240" | UP/DOWN to choose, OK to enter. On Volume, OK cycles six steps (off, then 20% up to 100%). Reset record takes **two presses of OK** |
| **Question** | The category and question number in the top bar (question 1 of 20), one blanked-out line of a letter, and four candidates below it | UP/DOWN to move between the four candidates, OK to answer |
| **Answer** | Right or wrong, the correct answer, an explanation, the full original text and the source; the spoken reading of the correct line plays only **after** you have answered | UP/DOWN to turn through the page line by line (it is taller than the screen), OK for the next question or the score |
| **Summary** | A rank and a comment, plus correct answers out of 20, best streak, time taken and score | OK to play again, a long press on OK to return to the title |

Every page spells out in its bottom bar what the three keys do right now — three keys have to cover
the whole game, so their meaning should never have to be guessed.

## What is in a question

- **91 questions in six categories.** A round draws 20 without repeats and **prefers questions you
  have not seen yet** (progress is kept on the device; picking uniformly at random would take close
  to 20 rounds before you had seen them all).
- Each question carries seven fields, and all of them are shown: category, the blanked line, four
  candidates, an explanation, the source (sender and year — for example a 1928 letter from a Chinese
  migrant in the Philippines to his mother), and the full original passage.
- The answer page **scrolls** instead of cutting content, precisely so that the source and the full
  passage survive.
- Every question has a spoken reading in dialect, and it reads the **whole correct line**; a music
  track loops underneath. Narration, interface sounds and music share one output: the six volume
  steps scale all three together, and the lowest step is off.

## Build and flash

```sh
./tools/validate.sh --static      # repository checks and host tests
./tools/validate.sh --firmware    # cold build plus merged image
```

The firmware gate produces `build/FoloToy-AI-Passport-full.bin`, a merged image flashed from
offset `0x0`.

## Documentation

- [`docs/README.md`](docs/README.md) — documentation index and the conventions every page follows.
- [`docs/reference/liangdabiao/qiaopi-quiz/README.md`](docs/reference/liangdabiao/qiaopi-quiz/README.md)
  — the full application record: the audio blob and its ADPCM encoding, the layout arithmetic and
  vertical budgets, the deliberate differences from the web version, and the verification log.
- [`AGENTS.md`](AGENTS.md) — repository rules for contributors and agents.
- [`assets/README.md`](assets/README.md) — the audio, fonts and images this game ships, with sources and licences.

## Origins

- The content and audio are ported from `game-adaptations/qiaopi/build/app` in the `novel-to-game`
  repository — a build-free single-page web game.
- This repository is a sibling of the **Three Character Classic kids game** (`ai-passport`) and the
  **Daodejing daily reader** (`daodejing-daily`); the three share one set of repository conventions,
  one toolchain and one verification gate.
