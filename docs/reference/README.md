<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Reference

This area holds reference material for AI Passport development that is not a
binding requirement: reusable development experience and archived application
playbooks. These are consulted when developing something new, not enforced as
rules. Reference is organized by contributing developer's GitHub username: under
each repository-relative `docs/reference/<username>/` folder, experience entries
are stored as flat files and application playbooks as subdirectories.

The engineering rules themselves live under
[`../development/`](../development/README.md); the collaboration conventions under
[`../contribution/`](../contribution/README.md).

## Contributors

### Shinku-Chen

**Experience entries:**

- [Audio Compression Trade-offs on ESP32-C3](shinku-chen/audio-compression-trade-offs.md) — how a voice-playback codec was chosen on limited flash (IMA-ADPCM vs Opus vs MP3), with measured capacity and decoder cost.
- [Post-Release Follow-up for the AI Passport Publishing Flow](shinku-chen/post-release-follow-up.md) — confirm the publish destination, include the data partition in a release, and the consent gates for the post-release tracks.
- [Display Refresh and Deep-sleep on ESP32-C3 (No PSRAM)](shinku-chen/display-refresh-and-deep-sleep.md) — direct panel refresh of a single image rect, RTC-GPIO deep-sleep wakeup, and the LVGL object-type misuse crash signature.
- [Shutting Down On-Board Peripherals Before Deep-Sleep](shinku-chen/deep-sleep-peripheral-power-off.md) — verified register shutdown, shared-bus ordering, terminal GPIO states, LCD deep-sleep holds, the `esp_codec_dev_close()` opened-state trap, and remaining hardware loads.
- [Landscape Rotation and a Deep-sleep Key Wake](shinku-chen/landscape-rotation-and-deep-sleep-key-wake.md) — rotating a portrait panel to a 320 × 240 landscape screen through LVGL, why the corner mask must follow the logical resolution, and how an ADC-owned pin makes a low-level deep-sleep wake fire at sleep entry.
- [Wall-clock Budgets for On-Device Game AI](shinku-chen/on-device-game-ai-wall-clock-budget.md) — why node-count limits misfire on this board (about 15k nodes per second), iterative deepening against a time budget, yielding to keep the idle task fed, and difficulty as a blunder rate.
- [Size Static Buffers from the Panel, and Verify the Release Artifact](shinku-chen/release-artifact-verification.md) — a 51 KB buffer mistake that left 8 KB of free heap, reading the startup log of the published merged image, and replacing a just-published release instead of shipping a follow-up.
- [Two-Device BLE Link Between AI Passport Boards (No PSRAM)](shinku-chen/two-device-ble-link.md) — symmetric peer discovery with an address tiebreak instead of host/join, measured link heap on a no-PSRAM part and its conflict with a static screenshot buffer, two hardware-only NimBLE GATT traps (missing `access_cb`, `EDONE` after a successful subscribe), NVS for RF calibration, and a stop-and-wait layer for turn-based play.

**Application playbooks:**

- [Voice Keychain](shinku-chen/voice-keychain/README.md) — a sound-effects keychain that turns the AI Passport into a pocket audio player.
- [What to Eat Today](shinku-chen/eat-what/README.md) — a button-driven food roulette that turns the AI Passport into a "what should I eat?" spinner.
- [Connect Four](shinku-chen/connect-four/README.md) — a landscape 10 × 7 four-in-a-row game with three computer difficulty levels, a two-player mode, synthesized sound, and an idle deep sleep.

### PhoenixZHC

**Experience entries:**

- [Network Audio Streaming and Memory Budgeting on AI Passport](phoenixzhc/network-audio-streaming-and-memory.md) — bounded HTTP audio streaming, ES8311/I2S ownership, and joint memory budgeting for decoding, JSON, DMA, and LVGL.
- [SoftAP Provisioning and Resource Budgets on AI Passport](phoenixzhc/softap-provisioning-and-resource-budget.md) — DHCP state, captive-portal compatibility, bounded forms and uploads, and no-PSRAM resource planning.

### Y2Lin

**Experience entries:**

- [Implementing the FAP_SCREENSHOT_V1 Serial Screenshot Protocol](y2lin/serial-screenshot-protocol.md) — install the USB-serial-JTAG driver first, substring-match the command, snapshot into a statically reserved full-screen buffer, chunk payload writes to the tx ring buffer, and mute logs during the binary window.
- [Sound-Meter UI: Smoothing, Anchors, and Stray Blocks](y2lin/meter-ui-smoothing-and-layout.md) — an asymmetric EMA for live readouts, creation-time anchors for mascot animations, the usual suspects behind stray screen blocks, and LVGL pool exhaustion as a white-screen cause.

### sunny0826

**Application playbooks:**

- [Offline Pokédex](sunny0826/offline-pokedex/README.md) — a fully offline Pokédex that embeds all 1,025 Pokémon, their sprites, and cries in the firmware.

### liangdabiao

**Experience entries:**

- [Cutting a CJK Font Subset for LVGL](liangdabiao/cjk-font-subsetting-for-lvgl.md) — deriving the glyph inventory from the content sources and the UI strings instead of hand-listing it, verifying every code point against the parent font before converting, why the large-font format flag is mandatory past 64 KB of bitmaps, and what a 397-code-point subset actually costs in flash.
- [Letting Font Metrics Drive the Layout](liangdabiao/font-metrics-driven-layout.md) — turning "how many characters fit on one line" into a division, proving that the glyph advance equals the font size before trusting that division, enforcing the same budget at generation time and at render time, and a display bug caused by confusing character counts with byte counts.
- [Keeping Application Logic on the Host](liangdabiao/host-testable-app-logic.md) — the pure-versus-hardware module split, "on failure do not modify the output" as a testable contract, testing a save format's rejection paths, and testing a line-breaking rule against the real content rather than a copy of it.
- [Building ESP-IDF Firmware from Git Bash on Windows](liangdabiao/windows-git-bash-esp-idf.md) — why `idf.py` exits silently when `MSYSTEM` is set and cannot be unset from the shell, the virtualenv interpreter mismatch, obtaining a host compiler via `zig cc`, and the linker and file-system limits that make a working host test look like a code failure.
- [Chapter Content into Generated C](liangdabiao/chapter-content-pipeline.md) — one file per chapter with no syntax left to get wrong, screen-fit limits enforced at generation time, a `--check` mode that makes a stale generated table fail the build, and one module kept as the single source for both the tables and the font inventory.

**Application playbooks:**

- [Dao De Jing Daily Reading](liangdabiao/daodejing-daily/README.md) — an offline adult daily-reading app for the Dao De Jing: one chapter a day, one screenful at a time, a reflection question with no right answer, and a re-reading queue derived from the reader's own recorded stance.

## Adding an experience entry

Each release may produce **one or more** reusable, post-release learnings; each is
added as its own entry (with the release tag or commit as context). Follow the
repository language rule: keep the default `.md` path in English and the paired
`.zh_CN.md` in Simplified Chinese, aligned in the same change.

An entry is a single `.md` file (with its `.zh_CN.md` peer) stored flat under
`docs/reference/<username>/` and named after the entry's content summary in
lowercase-kebab-case (e.g. `audio-compression-trade-offs.md`), describing the
topic rather than an opaque timestamp. Each entry is routed before submission:
general, upstream-benefiting experience goes to the upstream
`FoloToy/ai-passport` as a PR; fork-specific customization stays in the fork per
[`docs/fork-guide.md`](../fork-guide.md).

## Archiving an application

When an application is published, archive it under the repository-relative
`docs/reference/<username>/<app-name>/`
with an AI-generated bilingual functional summary (`README.md` / `.zh_CN.md`) and
optionally a how-to guide. The archive is **text-only** — record the cover image
by file name and format only, and do not store the firmware `.bin`. The `plays-archive`
skill drives the archive and its convention. Add the application to this index
and its Simplified Chinese peer in the same change.

## Related

- Repository overview and demo branches: [`../README.md`](../README.md)
