# Changelog

## [1.0.0] - 2026-10-24

Completed the working-panel bring-up and eleven-scene showcase, with setup instructions, historical measurements, photographs, and offline documentation.

### Pong and clock demos
- Added an automatic Pong match with paddle tracking, wall/paddle rebounds, occasional misses, scores, and a serve delay. `g` starts a new match.
- Added a clock simulation with enlarged HH:MM:SS digits, half-second blinking colons, a seconds bar, and a SIM label. `c` starts at 23:59:55 to demonstrate midnight rollover without Wi-Fi or an RTC.
- Extended the looping playlist to eleven scenes while preserving the existing `1`–`9` shortcuts and pause/brightness controls. Both new scenes run for 16 seconds by default.
- Added Pong and clock timing/start-time settings, refreshed the guide and HTML edition, and rebuilt the share bundle.

### Showcase and everyday use
- Replaced the long wiring-test sequence with a looping nine-scene showcase: letters, checkerboard, bouncing balls, starfield, pixel rain, simulated equalizer bars, Conway's Life, beating hearts, and scrolling text.
- Added direct scene selection with `1`–`9`, adjustable brightness with `+/-`, and consistent next/pause/restart controls. Serial output describes scene transitions without per-frame chatter.
- Kept static letters and simple all-off/all-on/blank controls. Removed row/column/address walks, stripes, corner/module walks, the pre-scroll black comparison, runtime OE-polarity switching, and the long-latch experiment. `s` now selects scrolling text.
- Preserved the working GPIO map, orientation, signal polarity, and normal scan timing. Brightness zero still disables output; application pauses keep refresh running.
- Reworked the README, usage guide, and HTML edition around a working display. Retained earlier investigation and photographs as historical reference, and regenerated the offline share bundle.

### Confirmed hardware outcome — 24 October 2026
- Replacing the original panel with an identical panel made the setup work. The original panel was faulty; its precise defective component or connection was not established.
- Recorded the separate corrected GPIO 26/27 wiring error, the EzSBC LS1 flicker improvement, all reported measurements, and the unsuccessful LT-width comparison.

### Documentation
- Added a detailed panel hardware/usage guide, a complete bring-up report, and a captioned six-photo archive in `images/`.
- Added an offline HTML edition of the documentation, local styles, a reproducible Markdown build script, image checksum manifest, and a shareable documentation ZIP.
- Clarified that all supplied photographs depict the faulty original and no working-replacement photo was supplied. Firmware behaviour is unchanged by this documentation update.

### Fixed
- Configurable column-stream reversal and row-address permutation/inversion for panel orientation. Address-bit order is normal after correcting the crossed GPIO 26/27 wiring; software B/C compensation has been removed.
- Blank before every row shift, cancel stale OE-off events, bound light pulses to the remaining row period, and skip overdue row callbacks.

### Added
- Held LT-pulse comparison (`l`): toggle normal 1 µs versus 20 µs latch high time while retaining the current frame and corrected wiring map.
- Static letter mode (`t`): `ABCDEFGH` above `IJKLMNOP`, one letter per 8×8 module, held without scrolling for placement checks.
- Held pre-scroll black comparison (`s` versus `f`) with a firmware revision marker and expected R1-level diagnostics for the remaining partial-panel fault.
- External 74AHCT245 wiring for all eight signals and documented direct-wiring test observations; corrected the HC245 input-threshold description.
- Three-second pre-scroll black interval and VS Code serial command instructions to help reproduce partial-panel faults.
- Runtime EN/OE polarity comparison (`b`, then `e`) with explicit HIGH/LOW reporting to distinguish full blanking from brightness changes.
- Explicit 1 us shift-clock/data/latch timing and held serial diagnostics (`f` black data, `o` all on, `b` OE blank) to isolate stuck module sections.
- Serial-guided 11-test wiring sequence with per-row/column progress, address-bit patterns, stripes, checkerboard, corner pixels, module walk, and OE brightness checks.
- Serial GPIO/polarity summary and restart, pause/resume, next-test, and help controls at 115200 baud.
- Configurable pattern timings and wiring-test instructions in the README.

## [0.1.0] 23-10-2026

### Added
- HUB08 64×16 single-colour panel driver (`Hub08Panel`, an Adafruit_GFX subclass) with esp_timer row refresh and OE-based brightness.
- Boot test sequence: all on, all off, row walk, column walk.
- Scrolling text demo.
- `config.h` with the pin map, signal polarity flags and timing constants.
- README covering the panel analysis and wiring.
