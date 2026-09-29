<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### Qiaopi Quiz app

The application ships three uncompressed LVGL bitmap subsets so that every label the
reader can see is drawn by a font we control:

| File | Format | Use |
| --- | --- | --- |
| [`fonts/qpq_font_16.c`](fonts/qpq_font_16.c) | 16 px, 4 bpp, uncompressed LVGL | Top-bar titles and counters, the bottom hint bar, the right-hand note inside a list row, the small captions and the source line on the answer page, and the statistics lines on the title and summary pages. |
| [`fonts/qpq_font_24.c`](fonts/qpq_font_24.c) | 24 px, 4 bpp, uncompressed LVGL | The four candidates on the question page, and the result, explanation, filled sentence and full text on the answer page. |
| [`fonts/qpq_font_32.c`](fonts/qpq_font_32.c) | 32 px, 4 bpp, uncompressed LVGL | The one element the reader must not misread: the sentence being completed, plus the rank headline on the summary page and the headline on the title page. |
| [`fonts/charset.txt`](fonts/charset.txt) | UTF-8 text | The shared 1303-code-point inventory, kept so the subset can be reviewed without opening the generated C arrays. |

All three subsets come from one master font:

- **Source:** Noto Sans CJK SC Regular (`NotoSansCJKsc-Regular.otf`, 16,437,364 bytes,
  SHA-256 `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`) from
  [`notofonts/noto-cjk`](https://github.com/notofonts/noto-cjk).
- **License:** SIL Open Font License 1.1. Subsetting and redistribution are permitted as
  long as the license text and copyright notice travel with the font.
- **Character range:** 1303 code points — 1079 CJK ideographs, plus 224 other code points:
  printable ASCII, the space, the CJK punctuation the UI draws, and the fullwidth low line
  that draws the blank the reader fills in. LVGL's built-in Montserrat font is deliberately
  **not** used as a fallback: a code point missing from the subset renders as a blank box,
  so the generator verifies every code point against the source font's `cmap` before
  converting anything.
- **Converter:** `lv_font_conv` 1.5.3 with `--bpp 4 --no-compress --format lvgl`.
- **Regenerate (from the repository root):**

  ```sh
  python3 tools/qiaopi/gen_font.py          # rebuild all three sizes
  python3 tools/qiaopi/gen_font.py --check  # verify the inventory and coverage only
  ```

  Both forms read `tools/qiaopi/bank.txt` and the string literals of `main/*.c|*.h`, so
  adding a UI string or a new question requires re-running the generator. `--check` is what
  makes a missing glyph a build-time error rather than a blank box on the device.
- **Destination:** compiled into the `main` component by the `target_sources` call in
  `main/CMakeLists.txt`; declare the fonts with `LV_FONT_DECLARE` and select them per
  widget. `CONFIG_LV_FONT_FMT_TXT_LARGE=y` in `sdkconfig.defaults` is required because
  LVGL's default text-format font stores a glyph's bitmap offset in 16 bits, and **all
  three** subsets are past that 65,536-byte field:

  | Size | Glyph bitmap data | Past the 16-bit offset field |
  | --- | --- | --- |
  | 16 px | 145,642 bytes (142.2 KB) | yes |
  | 24 px | 322,451 bytes (314.9 KB) | yes |
  | 32 px | 551,508 bytes (538.6 KB) | yes |

  The flag is per font format, not per font, so setting it once covers the three of them.
- **Generated source size (measured):** 1,068,125 / 2,116,944 / 3,482,000 bytes. Hex
  literals cost several source bytes per byte of bitmap, so the directory listing
  overstates the Flash footprint by roughly a factor of six — read the byte counts in the
  table above, not `ls`.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

### The four screens of the Qiaopi game

`images/qiaopi/` holds the screen images the project README embeds:

| File | Dimensions and format | Content |
| --- | --- | --- |
| [`images/qiaopi/qiaopi-title.png`](images/qiaopi/qiaopi-title.png) | 1152 × 1536, PNG RGB | Title page: the large title, three entries and the progress line, with the first entry selected. |
| [`images/qiaopi/qiaopi-ask.png`](images/qiaopi/qiaopi-ask.png) | 1152 × 1536, PNG RGB | Question page: category and question number, one line of a letter with a blank, and four candidates. |
| [`images/qiaopi/qiaopi-reveal.png`](images/qiaopi/qiaopi-reveal.png) | 1152 × 1536, PNG RGB | Answer page: right or wrong, the correct answer, the filled line and the explanation; the content is cut at 248px, which is exactly why the page has to be turned by key. |
| [`images/qiaopi/qiaopi-summary.png`](images/qiaopi/qiaopi-summary.png) | 1152 × 1536, PNG RGB | Summary page: rank and comment, correct answers, best streak, time and score. |
| [`images/qiaopi/screens-source.py`](images/qiaopi/screens-source.py) | Python | The generator that turns the layout constants and a real question into HTML. |
| [`images/qiaopi/screens-source.html`](images/qiaopi/screens-source.html) | HTML, 13 KB | Its output: four 240 × 320 device frames side by side, for review and capture. |

- **These four images are drawn from the layout constants in the source; they are not device
  photographs.** The geometry comes from `main/qpq_ui.{h,c}` (page inset 5, top bar 36 / hint bar 26,
  body 210 × 248, row height 36 with a 4px gap), `main/qpq_page_*.c` (each page's element coordinates)
  and `main/qpq_wrap.c` (the line-breaking algorithm, replicated character by character so the breaks
  match the device). The question content is question 1 from `main/qpq_text.c`, and the summary
  numbers follow `QPQ_SCORE_CORRECT` / `QPQ_SCORE_STREAK_BONUS` consistently: 13 of 20, best streak 5
  means 115 points, 65%, one rank above the middle.
- **They are not firmware assets**: documentation only, never flashed, costing no Flash.
- **Regenerate:** run `python3 screens-source.py` to rebuild the HTML, then render each 240 × 320
  frame on its own at 4.8× device scale (`--window-size=240,320 --force-device-scale-factor=4.8`),
  which lands on exactly 1152 × 1536. The system's Noto Sans CJK / Microsoft YaHei stands in for the
  device's Noto Sans CJK SC, so glyph shapes differ slightly.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

### Qiaopi Quiz app

This application is the first one in the family that **plays recordings**, not just
synthesised tones. It ships a single self-describing binary instead of a directory of
media files:

| File | Size and format | Use |
| --- | --- | --- |
| [`audio/qpq_audio.bin`](audio/qpq_audio.bin) | 5,298,976 bytes, IMA-ADPCM 4-bit, 16 kHz mono | One blob holding all 92 clips as one memory-mapped `rodata` region. |
| [`audio/clips.txt`](audio/clips.txt) | UTF-8 text | The clip inventory with duration and byte length, so the blob can be reviewed without a hex editor. |

- **Content:** 92 clips, 662 seconds in total (11 min 2 s). Clips 0–90 are the narration
  that plays after an answer — the correct sentence read aloud in dialect, one per
  question. Clip 91 is the background music.
- **Format:** IMA-ADPCM, 4-bit, 16 kHz mono. It is exactly 4 bytes of encoded data for
  every 8 source samples, so 4.00:1 against 16-bit PCM at the same rate — and 6.00:1
  against the 24 kHz mono PCM that some of the sources turned out to be, since the
  downsample to 16 kHz is part of the saving. Decoding is one table lookup and a shift per
  sample, which is what makes it viable on a single-core C3 with no PSRAM; a real MP3
  decoder would have cost roughly 28 KB of heap and a much larger dependency for no
  audible gain at this speaker size.
- **Blob layout:** a 16-byte header (magic, version, clip count, sample rate, reserved
  word), then a fixed-size 8-byte index entry of `(offset, sample count)` per clip, then
  the clips themselves. Each clip is an int16 first sample, a uint8 initial step index, a
  reserved byte, and then the nibbles, two samples per byte. Every field is fixed-width and
  read byte by byte — the blob is placed in Flash by the linker, and a RISC-V unaligned
  load traps, so nothing here is read through a cast. Opening the blob validates the whole
  index and rejects it as a whole if anything disagrees; the rejection paths are covered by
  a host test. `audio/clips.txt` also records the SHA-256 of each source file, which
  `--check` re-verifies when the sources are reachable.
- **Regenerate (from the repository root):**

  ```sh
  python3 tools/qiaopi/gen_audio.py          # re-encode all 92 clips
  python3 tools/qiaopi/gen_audio.py --check  # verify the blob against the sources only
  ```

  Both forms need `ffmpeg` on `PATH` to decode the sources. The encoder is
  `tools/qiaopi/adpcm.py`, and the same run emits `tests/qpq_adpcm_fixture.h` — a fixed
  source/encoded/expected triple that the C decoder is asserted against, so the two
  implementations cannot drift apart unnoticed.
- **Destination:** attached to the `main` component by `target_add_binary_data` in
  `main/CMakeLists.txt`. The linker generates the symbol from the file name
  (`_binary_qpq_audio_bin_start`), and `main/qpq_audio_blob.c` is the one file that
  declares it. Do not also add the blob to `SRCS`, and do not generate it as a C array: a
  5 MB array slows the compile and link noticeably and the result cannot be reviewed.
- **Source:** `game-adaptations/qiaopi/build/app/audio/` in the `novel-to-game`
  repository, which carries an MIT license (`Copyright (c) 2026 NovelToGame
  contributors`). Note the discrepancy before publishing: that repository's build brief
  states that sound effects are synthesised with the Web Audio API and that there are
  **zero** audio files, yet the built app contains 92. The provenance of the individual
  recordings is not documented there, so redistribution permission for the narration and
  the music should be confirmed with the upstream author before this app is distributed
  beyond the device it was built for.
- **Audit note:** the source audio adds up to 14.94 MB, which looked almost too large for
  a Flash-based device. It was not — 31 of the 92 files (9.37 MB, 63% of the total) were
  named `.mp3` but actually contained uncompressed 24 kHz mono PCM. The pipeline decodes
  every source with `ffmpeg` and reads the codec that comes back, not the extension, which
  is why the result fits in 5 MB instead of 15.
