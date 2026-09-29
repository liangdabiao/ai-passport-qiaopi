<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Qiaopi Quiz — fill-in-the-blank on a three-key handheld

An offline quiz game for the FoloToy AI Passport (ESP32-C3), ported from an
existing single-page web game about *qiaopi* — the remittance letters that
Chinese emigrants sent home from Southeast Asia in the 19th and 20th centuries.
Each round draws 20 questions from a bank of 91; every question is a line from a
real letter with one phrase missing, and the reader picks the missing phrase from
four candidates.

- **Hardware:** FoloToy AI Passport — ESP32-C3, 8 MB flash, no PSRAM,
  ST7789P3 240x320, three keys sharing one ADC ladder, ES8311 codec.
- **Portrait, offline, battery-powered.** No Wi-Fi is enabled in this build.
- **Source of the content:** `game-adaptations/qiaopi/build/app` in the
  `novel-to-game` repository — a zero-build HTML/CSS/JS page plus 92 audio files.
- **Repository:** `aiot/qiaopi-quiz`, a standalone fork of the same base as the
  San Zi Jing game and the Dao De Jing daily reader.

## Why this content survives the port

The first question about any web game moving to a three-key device is whether its
interaction can exist at all. Here it can, because the web game is not free text
entry: every question is four candidates plus one answer index, and the
candidates are tagged with the four stem characters. Up and down move through
four options; OK confirms. That is the same interaction the daily reader already
uses for its reflection step.

The three things that did have to be measured rather than assumed:

| Question | Measurement | Result |
| --- | --- | --- |
| Does the audio fit in flash? | 92 source files decoded with ffprobe | 5.30 MB after re-encoding, in a 7.94 MB app partition |
| Does the text fit 240x320? | Longest of each field against its layer budget | Every field is inside its cap; see the table below |
| Is the source audio what it claims to be? | Codec name of every file | 31 of 92 files are uncompressed PCM with an `.mp3` extension |

That last row is the interesting one: **63 percent of the original 15 MB of audio
was not compressed at all.** The files were named `.mp3` but held raw 16-bit PCM,
so simply shipping them would have cost six times what the content needed.

## Audio: one blob, one writer, no decoder library

The device has no filesystem, so the audio is a single self-describing binary
blob embedded in the firmware image and read through the flash mapping:

```
offset  0   magic "QPQA", version, clip count, sample rate, reserved  (16 bytes)
offset 16   index: { uint32 offset, uint32 samples } per clip
then        per clip: int16 first sample, uint8 initial step index,
            uint8 reserved, then the nibbles, two samples per byte
```

Clips 0..90 are the dialect narrations of the correct sentence for questions 1..91
— the clip index equals the question index — and clip 91 is the looping
background music.

**Codec choice: IMA-ADPCM, 16 kHz mono, not MP3.** Decoding is a table lookup and
an add; there is no converter library, no 28 KB of decoder state, and no CPU
contention with the display on a single-core part. The cost is size — about twice
MP3 at equal speech quality — and the budget above shows there is room.

Two details that only show up when measured:

- **Cold start.** With the step index starting at zero the first eight samples
  cannot track the signal: a 1 kHz test tone showed a peak error of 11760 out of
  12000, against 464 in steady state. The encoder therefore estimates an initial
  step index from the first 32 samples; real clips went from a four-digit startup
  error to 1, at 27.8 to 31.8 dB SNR.
- **One writer.** The baseline firmware already had an RTTTL tone player writing
  to I2S. Two tasks writing the same DMA buffer interleave their samples, which
  sounds like noise and is hard to attribute. The player here owns the output and
  plays narration, music and interface tones from one place; interface tones are
  **mixed into** the music rather than interrupting it, because a keypress that
  restarts the background music every time is worse than the tone is worth.

## Deliberate differences from the web version

| Web behaviour | On the device | Why |
| --- | --- | --- |
| Explanation appears inline under the options | Separate scrollable reveal page | Four option rows plus a paragraph do not fit in 248 px |
| Narration is layered over the music | Music yields to narration, then resumes | One PCM sink, and on a 40 mm speaker speech under music is mud |
| Question order is uniformly random | Questions not yet seen are preferred | The web has no memory between sessions; the device has NVS, and 91 questions random-sampled take about 20 rounds to see once |
| Mute buttons for narration and music separately | One mute switch | Three keys do not justify two settings rows |

The narration timing is kept exactly: the recording reads the **correct full
sentence**, so it plays after the answer is given. Playing it earlier would give
the answer away.

## Layout arithmetic

Chinese glyphs at a given size have the same advance as the size itself, so
"how many characters fit on one line" is a division, not a judgement. The content
area is 210 px wide:

| Layer | Font | Per line | Lines | Cap | Longest shipped |
| --- | --- | --- | --- | --- | --- |
| Sentence, slot form | 24 px | 8 | 3 | 22 | 22 |
| Sentence, filled form | 24 px | 8 | 3 | 21 | 19 |
| Option | 24 px | 8 | 1 | 6 | 4 |
| Explanation | 24 px | 8 | scrolls | 60 | 56 |
| Full original | 24 px | 8 | scrolls | 48 | 46 |
| Provenance | 16 px | 13 | 2 | 26 | 21 |
| Category | 16 px | 13 | 1 | 6 | 4 |

The sentence has two forms and both are capped: the ask page can only show the
blank, so it renders an empty slot of the same character count as the source
sentence, while the reveal page shows the answer filled in. Capping only one of
them would fail somewhere in the middle of a round.

It is worth being clear about what the cap in that table is and is not. It is set
from the longest sentence actually shipped (22 characters), not derived from the
line budget, and the two are not independent: because the break rule can lose one
character per line, 22 characters can in principle need `ceil(22 / 7) = 4` lines —
152 px against the 87 px the ask page reserves for the sentence. What rules that
case out is the host test, which wraps the real bank and asserts the result rather
than trusting the cap. A cap that *derived* three lines would have to be 21.

Line breaking is hand-written, because LVGL breaks on spaces and Chinese has
none. The rule is: keep at most N characters per line, and among the break
positions that respect N, take the last one whose following character is not a
closing punctuation mark. The more common "break one character early when the
next character is punctuation" rule fails on consecutive punctuation — when the
first mark lands exactly on the last column, the second still starts the next
line. Searching backwards has no such hole.

## Verification

- **Build:** cold build from an empty directory — **1,865 compilation units, 55
  minutes, zero warnings and zero errors**. The merged image is produced from
  offset 0 and verified section by section.
- **Image:** app 7,139,632 bytes inside the 8,323,072-byte `factory` partition (14%
  free); merged firmware 7,205,168 bytes, SHA-256 `3b114b46...`. Both
  `_binary_qpq_audio_bin_start` and `qpq_font_{16,24,32}` appear in the map file,
  which is what proves the audio blob and all three font subsets actually made it
  into the image instead of merely sitting in the repository.
- **Host tests:** six suites of this app's own over the platform-independent layer
  — the ADPCM decoder against fixed vectors produced by the Python reference
  encoder (bit exact), the blob index parser including nine rejection paths and a
  check against the real `assets/audio/qpq_audio.bin` for clip count and total
  samples, the question tables, the line breaker against the real bank in both
  sentence forms, the round state machine, and the save format including its six
  rejection paths. Together with the two inherited suites and the five BSP suites,
  **13 C suites pass**.
- **Generator freshness:** the question tables, the audio blob and the font
  inventory are all `--check`ed, so editing the source and forgetting to
  regenerate fails the gate instead of shipping stale data.
- **Not verified:** nothing has been run on hardware. Key behaviour, the corner
  mask, audio output level, battery gauge and NVS behaviour across power loss are
  all unverified until the image is flashed.
- **Two things this machine cannot attest** (both reproduced on the fork baseline
  `776b7c5`, so neither is caused by this work): `tests/test_check_repo.py` fails
  ten cases here, all of them about **symlinks**, with the identical case names
  failing on the baseline; and `tests/test_archive_firmware.py` plus
  `tests/test_install_passport_skills.py` hang, because deleting a temporary
  directory hangs on this machine even when the temporary directory is moved to
  another drive. On the firmware side, `test_verify_firmware` and
  `test_deep_sleep_contract` pass.

## Content model

Each of the 91 questions carries seven fields, all visible to the reader:

- category — one of six thematic groups
- sentence — one line from a letter, with a blank marker
- four candidates and the index of the correct one
- explanation — the usage or allusion behind the answer
- provenance — the sender and the year, for example a 1928 letter from a
  Philippine emigrant to his mother
- full original — the complete passage the question was cut from

The provenance and the full text are why the reveal page scrolls rather than
being trimmed: they are the part that turns a quiz answer into a piece of
history, and they are the reason the source material was chosen in the first
place.
