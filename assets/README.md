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

| File | Size and format | Use |
| --- | --- | --- |
| [`fonts/qpq_font_16.c`](fonts/qpq_font_16.c) | 16 px, 4 bpp, uncompressed LVGL | The bottom hint bar, the right-hand stance note inside a list row, and the small caption above the reflection layer. |
| [`fonts/qpq_font_24.c`](fonts/qpq_font_24.c) | 24 px, 4 bpp, uncompressed LVGL | Top-bar titles, the commentary body, list-row text, and the reflection answers. |
| [`fonts/qpq_font_32.c`](fonts/qpq_font_32.c) | 32 px, 4 bpp, uncompressed LVGL | The passage text — one sentence per screen. |
| [`fonts/charset.txt`](fonts/charset.txt) | UTF-8 text | The shared 1303-code-point inventory, kept so the subset can be reviewed without opening the generated C arrays. |

All three subsets come from one master font:

- **Source:** Noto Sans CJK SC Regular (`NotoSansCJKsc-Regular.otf`, 16,437,364 bytes,
  SHA-256 `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`) from
  [`notofonts/noto-cjk`](https://github.com/notofonts/noto-cjk).
- **License:** SIL Open Font License 1.1. Subsetting and redistribution are permitted as
  long as the license text and copyright notice travel with the font.
- **Character range:** 1303 code points — 1079 CJK ideographs taken from the question bank
  sources and the UI strings, plus 111 other code points: printable ASCII, the space, and
  the CJK punctuation the UI draws. LVGL's built-in Montserrat font is deliberately
  **not** used as a fallback: a code point missing from the subset renders as a blank box,
  so the generator verifies every code point against the source font's `cmap` before
  converting anything.
- **Converter:** `lv_font_conv` 1.5.3 with `--bpp 4 --no-compress --format lvgl`.
- **Regenerate (from the repository root):**

  ```sh
  python3 tools/qiaopi/gen_font.py          # rebuild all three sizes
  python3 tools/qiaopi/gen_font.py --check  # verify the inventory and coverage only
  ```

  Both forms read `tools/qiaopi/chapters/*.txt` and the string literals of `main/*.c|*.h`,
  so adding a new UI string or a new chapter requires re-running the generator. `--check` is
  what makes a missing glyph a build-time error rather than a blank box on the device.
- **Destination:** compiled into the `main` component by the `target_sources` call in
  `main/CMakeLists.txt`; declare the fonts with `LV_FONT_DECLARE` and select them per
  widget. `CONFIG_LV_FONT_FMT_TXT_LARGE=y` in `sdkconfig.defaults` is required because
  LVGL's default text-format font stores a glyph's bitmap offset in 16 bits: the 24 px and
  32 px subsets carry roughly 114 KB and 203 KB of bitmap data respectively (1303 glyphs at
  288 and 512 bytes each), both past the 64 KB field. The 16 px subset is about 51 KB and
  would fit, but the flag is per-font-format, so it is set once for all three.
- **Generated source size (measured):** 293,734 / 565,633 / 920,969 bytes. Hex literals
  cost several source bytes per byte of bitmap, so the directory listing overstates the
  Flash footprint — read the size report, not `ls`.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

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
