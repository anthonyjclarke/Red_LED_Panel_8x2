# Changelog

## [1.6.0] 25-10-2026

Uploaded over Wi-Fi from the MacBook on 25 October 2026. After the upload the panel reported v1.6.0 with its saved brightness (105) and other settings kept, confirming the settings upgrade on real hardware; clock time 10 s and an empty message were added. Not yet checked by eye or committed.

### Changed

- Removed the letter test (A–H above I–P) and its `t` key. Scene keys are now `1` checkerboard, `2` balls, `3` stars, `4` rain, `5` bars, `6` Life, `7` hearts, `8` scrolling text, `9` Pong (was `g`) and `c` clock. The playlist starts with the checkerboard.
- The clock time between demos is a web setting (**Clock time between demos**, 5–3600 s, default 10), replacing the fixed `CLOCK_HOLD_MS`. It applies from the next clock visit.
- The scrolling message is a web setting (**Scrolling message**, up to 64 plain characters). Empty, the default, scrolls device details composed when the scene starts: firmware version, uptime, time since the last sync, address and location. The scene length comes from the message width, so the whole message always crosses the panel before the clock returns. Replaces the fixed `SCROLL_TEXT`.
- Settings record schema 2 adds the two fields. A schema-1 record from v1.5.0 or earlier is upgraded on load, keeping location, zone, brightness and view.
- `/api/status` reports `clockHold` and `scrollText`; `/api/settings` validates both.
- OTA uploads read the password from `include/secrets.h` (`tools/ota_auth.py`), with `PANEL_OTA_PASSWORD` as an override for password changes. This avoids the empty password sent when the variable is set in another terminal. espota's `--debug` flag, which printed the password, is no longer passed.

### Added

- Host regressions: clock time from the setting, device-details and custom messages, scroll never cut short, input validation for both fields, and the schema-1 upgrade in both the app and network tests.

## [1.5.0] 25-10-2026

Uploaded over Wi-Fi from the MacBook on 25 October 2026; the panel reported v1.5.0 and the date `Sun 25 Oct 2026`.

### Changed

- The clock's lower row shows the day of the week and the date, for example **SUN 25 OCT 2026** (15 characters, x 2–60), using a new set of 3×5 capitals for the day and month names.
- Removed the NTP/HLD label. Offline holdover is now shown by **solid colons**; they blink with the real second while Wi-Fi is connected.

### Added

- Host regressions for the day/date row layout, the missing label, and blinking versus solid colons; regenerated the clock example GIFs.

## [1.4.0] 25-10-2026

**First Wi-Fi (OTA) upload**: sent from the MacBook to the reference panel on 25 October 2026 and confirmed working. Not committed or tagged on its own; its changes are included with 1.5.0.

### Changed

- The clock's lower row now shows the local date as **DD-MM-YYYY** in the 3×5 digit font (x 22–60), beside the NTP/HLD label. It replaces the seconds bar, since the seconds are already shown as digits.

### Added

- Host regression for the date row's position and dash rows; regenerated the clock example GIFs.

## [1.3.0] 25-10-2026

Flashed over USB on the reference panel on 25 October 2026 and tagged `v1.3.0-flashed`. The panel reports v1.3.0 with OTA **ready**; the new settings UI and the optional page login (user `admin`) were confirmed working. A first Wi-Fi (OTA) upload had not yet been run when this was recorded.

### Added

- Firmware upload over Wi-Fi with ArduinoOTA. It is enabled only when `SECRET_OTA_PASSWORD` is set in `include/secrets.h`, starts on the first Wi-Fi connection on port 3232 (`OTA_PORT`), and advertises itself over mDNS.
- `esp32dev_ota` PlatformIO environment: `PANEL_OTA_PASSWORD=… pio run -e esp32dev_ota -t upload`. The password comes from an environment variable, so it never enters `platformio.ini`.
- Panel shows **OTA** with a percentage and progress bar during an upload. A failed upload returns to the current scene; success restarts.
- `[ota]` status in boot and `i` reports, `ota` in `/api/status`, and a help line for network uploads.
- The `esp32dev_ota` build fails with a clear message when `SECRET_OTA_PASSWORD` is empty, because that firmware would disable OTA and need USB again.

### Changed

- Redesigned the settings page in the documentation's style: a sidebar (a sticky top bar on phones) with **Clock**, **Settings**, **Connection** and **Device** sections that highlight as you scroll, plus links to the Wi-Fi network and system-info pages. The Device section shows firmware, OTA state, timing, polarity and a GPIO table instead of raw JSON.
- The settings page now opens at `/` on the LAN and in the setup hotspot (`/panel` still works). WiFiManager's own pages are restyled to match and gain a navigation bar back to the settings.
- Updated the settings-page screenshot and the preview server's mock status.
- Host regressions: `/` serves the panel UI, `ota` status, OTA disabled without a password, configured once with one, progress and error callbacks, a second network test build with page and OTA passwords set, and the progress display in the app test.

### Fixed

- Connecting to Wi-Fi sent two NTP requests. The retry check compared a stale `now` with the time of the request just made, so the unsigned difference wrapped. The host fixture now advances time during a request to cover this.
- `default_envs = esp32dev` keeps plain `pio run -t upload` on USB now that a second environment exists.

## [1.2.0] 25-10-2026

Flashed and checked on the reference panel on 25 October 2026: normal display through 255/255, zero missed ticks and late rows over more than 500,000 rows, peak wake 237 µs (v1.1.0: 491 µs), and serial brightness saved and restored after reset. Tagged `v1.2.0-flashed`.

### Changed

- Refresh driver shifts the next row while the current row is lit, so only the address/latch step and the 50 µs guard are dark. Full brightness now lights each row for about 445 µs of 500 µs (about 250 µs before), so the whole 0–255 range is usable. Peak panel current at 255/255 rises by roughly 1.8 times.
- Column data now goes out over the VSPI hardware SPI peripheral at `SHIFT_CLOCK_HZ` (500 kHz, matching the former 1 µs clock and setup timing) instead of bit-banged GPIO with busy-wait delays. Shifting takes about 128 µs per row instead of at least 195 µs, which frees time on core 1. Blank data is preloaded into the 74HC595s at start-up.
- Saving web settings applies brightness at once and restarts the display only when the startup view changed.
- Serial `+`/`-` brightness is saved as the startup brightness after a 5-second quiet period (`BRIGHTNESS_SAVE_DELAY_MS`), so key bursts write flash once.
- Clock colons now light for the first half of each real second instead of following `millis()`.
- Clock holdover ends after 24 hours without an NTP sync (`CLOCK_HOLDOVER_MAX_MS`); the clock then shows its waiting state instead of drifted time.
- The setup-hotspot password moved from `config.h` to a gitignored `include/secrets.h` (`SECRET_AP_PASSWORD`), with `include/secrets.example.h` as the template and fallback. The default is still `PanelSetup32`.
- WiFiManager timeouts now come from `WIFI_CONNECT_TIMEOUT_MS` and the new `WIFI_SAVE_CONNECT_TIMEOUT_MS`.
- Documentation is now one self-contained `index.html` built from all Markdown sources, with per-document sections, a collapsible sidebar, a print layout and a generated release and version log. The nine per-document HTML files were removed.
- Rewrote `CLAUDE.md` as a short maintainer primer. README links to the guide for the pin map, and the guide links to the test configuration for buffer pins, so they are no longer repeated.

### Added

- Optional login for `/panel` and `/api/*`: set `SECRET_PANEL_PASSWORD` in `include/secrets.h` to require HTTP Basic login (user `admin`). Empty, the default, keeps the previous open behaviour.
- `DBG_ALWAYS` debug macro for command confirmations that must print at every level (used by `d`).
- Host regressions for overlapped shifting, SPI column order and polarity, the full-brightness light window, SPI start-up failure cleanup, holdover expiry, brightness persistence and brightness-only settings saves.

### Fixed

- Removed `delay(1)` from `loop()`; the application is fully `millis()`-driven.
- Removed duplicated assignments in `PanelNetwork::startPortal()` and dead upper-case `switch` cases (input is already lower-cased).
- Clock visits now end only on elapsed time, not on a second, redundant frame counter.
- Screen-example headings use sentence case (for example "Conway's life", "Clock waiting for NTP") instead of `str.title()` output.
- The `platformio.ini` header comment no longer uses the old RGB project name.
- The test configuration now names the EzSBC LS1 as the reference panel's level shifter, matching the guide.

## [1.1.0] 25-10-2026

### Reference-panel check — 25 October 2026

- User confirmed the corrected pre-release build displayed normally with no visible flicker.
- Serial confirmed Wi-Fi association, NTP synchronisation in Sydney/AEDT, and clock → letters → clock → checkerboard progression.
- While letters were held, rows advanced from 67,642 to 77,988; missed ticks stayed at 39 and late rows at 45. No new missed/late events appeared between those snapshots. Recorded maximum wake/scan times were cumulative peaks, not a continuous latency measurement.
- Finalised the firmware version as 1.1.0 and refreshed the example gallery, HTML documentation and share bundle.

### Blank-display startup fix

- Fixed `timer_isr_callback_add` failing with `ESP_ERR_NOT_FOUND`, which left OE blanked and refresh rows at zero. The two timers now request compatible shared IRAM interrupts at an available low/medium level instead of two exclusive level-3 vectors.
- Added stage-specific startup errors and cleanup of acquired timers, interrupts and the scan task if setup fails.
- Added a constrained-interrupt regression that reproduces the old failure, verifies shared allocation starts scanning, and checks failure cleanup. The corrected driver subsequently passed the reference-panel visual check.

### Clock-led cycle and refresh timing

- Start with the clock for 10 seconds, run one demo, then return to the clock before the next demo. `r` resets the demo cursor, while direct `c` and saved Clock mode remain clock-only.
- Preserve the clock visit deadline across pause/resume and handle delayed loops and millis rollover.
- Move refresh from shared `esp_timer` callbacks to hardware row/OE timers and a dedicated scan task on core 1. Late rows stay blank, and pending pictures/brightness apply at row zero to reduce tearing.
- Add serial `i` refresh counters and host driver regressions for pulse deadlines, late slots and complete-scan updates. Keep electrical mapping and scan timings unchanged. Reference-panel bench results are recorded below.

### Wi-Fi, real time and configuration
- Added WiFiManager 2.0.17 with a setup hotspot, saved-network retries and a LAN-accessible `/panel` configuration/status page.
- Replaced the clock simulation with NTP-synchronised local time, automatic daylight saving, honest waiting states and offline holdover. Selecting or pausing the clock no longer resets/freezes actual time.
- Added validated persistent location, time zone, startup brightness and startup-view settings. Sydney is the default. Credentials remain in device storage and are never logged.
- Added startup/version/settings cards and a full serial/web status report. Serial `w` opens setup, `i` prints status and `d` controls runtime verbosity.
- Applied the shared debug prefixes/runtime levels, two-space C++ formatting, `.clang-format` and `.editorconfig`.
- Added the complete ESP32 reference bench configuration and rendered screen examples for every demo, boot and clock state. Host regressions use the actual application/GFX and network application with hardware doubles.
- Regenerated the offline HTML and share bundle. The reference-board log confirms Wi-Fi connection, NTP sync and clock/demo alternation; the user reported no visible flicker. Captive-portal phone behaviour, persistence/reconnect scenarios and extended stress testing remain separate acceptance checks.

## [1.0.0] 24-10-2026

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
