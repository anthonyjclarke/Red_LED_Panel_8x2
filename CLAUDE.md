# Maintainer notes — Red LED Panel 8×2

## Target and configuration

- ESP32 DevKit, PlatformIO `esp32dev`, `espressif32@6.9.0`, Arduino framework.
- Red HUB08 64×16, eight by two 8×8 modules, 1/16 scan; Adafruit GFX drawing.
- GPIO 25/26/27/13 → LA/LB/LC/LD, 22 → EN, 23 → R1, 4 → LT, 18 → SK.
- Active-low data/OE. Reversed column stream, normal row-bit order `{0,1,2,3}`, row XOR `0x0F`.
- All configured GPIOs must be below 32 for direct register writes.
- The project folder's RGB name is historical; the firmware drives red only.

## Showcase application

`src/main.cpp` starts with a real NTP clock for 10 seconds, runs one demo, returns to the clock for 10 seconds, then runs the next demo. The ten demos cycle through letters, checkerboard, balls, stars, rain, animated bars, Life, hearts, scrolling and Pong. A separate cursor tracks the next demo; direct demo selection advances that cursor, and `r` resets it to letters while showing the clock first. Each scene has a frame count and interval; scrolling derives its count from text bounds. Animation timing uses unsigned millisecond subtraction, renders one frame at a time, and does not replay missed frames. Serial prints scene transitions and user commands, not individual animation frames.

`1`–`9`, `g` (Pong), and `c` (held live clock) select a scene; `n` advances; `p` pauses/resumes; `r` restarts. `t` holds letters; `s` selects scrolling. `f/o/b` hold off/on/blank; `p/r/n` return to the playlist. Brightness changes with `+/-` in steps of 15, bounded to 0–255; hearts modulate up to that requested level. Conway's Life uses packed buffers and wrapping neighbours. Bars are simulated, with no audio input.

Pong uses quarter-pixel ball positions, bounded paddle tracking, collision/reflection rules, single-digit scores, and a serve delay. Update its state only in `stepPong()`, not rendering, so pause and brightness controls preserve gameplay. Clock reads `PanelNetwork::localTime()` and never invents a sample epoch. It renders waiting/holdover states, enlarged digits, and NTP/HLD labels. Pausing freezes playlist progression, not the live clock. Direct `c` and saved Clock startup mode hold it indefinitely; normal clock visits last `CLOCK_HOLD_MS` (10 seconds). Elapsed-time deadlines survive pauses, delayed app loops and millis rollover.

Retired diagnostics (`e` polarity, `l` latch width, pre-scroll black comparison, row/address walks) are absent from the current application and driver API. Keep the working wiring, polarity, and scan timing stable when adding scenes.

## Network, settings and conventions

`src/PanelNetwork.*` owns WiFiManager 2.0.17, one HTTP server (portal or LAN), Preferences, mDNS and SNTP. `loop()` processes it even while animation is paused. Initial/retry association is asynchronous. WiFiManager's save/connect path can briefly pause animation, but background row scanning continues. Keep custom routes registered through its web-server callback so they survive portal/LAN transitions.

`/panel` serves `include/ConfigPage.h`, generated from `web/index.html` by `tools/embed_web.py` (also invoked by the PlatformIO pre-build hook). `/api/status` reports device time/settings without passwords; POST settings/sync/setup requests require a boot-local token. Settings are validated, saved as one NVS record, then applied in the application loop. A failed NVS write must not change active settings. IANA IDs map to a fixed POSIX-zone table in `include/ClockSettings.h`; Sydney is the default.

Serial `w` opens setup, `i` prints status, `d` cycles one shared runtime debug level. Use the standard `[ERR ]`, `[WARN]`, `[INFO]`, `[VERB]` macros; log transitions rather than frames, and never credentials. `.clang-format` defines two-space indentation, attached braces and 100 columns. Generated `ConfigPage.h` is excluded from formatting.

## Refresh driver

`Hub08Panel` subclasses `Adafruit_GFX`. Draw into `drawBuf_`; `show()` copies 128 bytes to `pendingBuf_` under a lock. The scan task commits it to `showBuf_` only at row zero, together with brightness. Intermediate pending pictures may be replaced; the active picture remains stable for a full scan.

Hardware timers TG0/T0 (row tick) and TG0/T1 (OE off) are reserved for refresh. The row ISR blanks and notifies a dedicated high-priority task pinned to the same core as setup/interrupt allocation (core 1 on this target); the off ISR only blanks. Allocate both timer callbacks with `ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM`; two exclusive level-3 requests failed on the reference board with `ESP_ERR_NOT_FOUND`, leaving the panel blank. The IDF timer wrapper filters each source by its status bit. Unwind acquired interrupts, timers and the scan task on initialization failure, and log the failing stage. Keep ISRs short, IRAM-safe, and free of logging. Shifting occurs in the task, never an ISR or the shared `esp_timer` task. Use the hardware row counter and tick generation to cap light windows; if a tick arrives during shifting, leave the expired row dark. Skip accumulated notifications instead of replaying a backlog. Keep the 500 µs row period, 1 µs signal phases and 50 µs guard. Nominal complete-frame rate is 125 Hz; animation timing is separate. Serial `i` reports lifetime rows/missed ticks/late rows/max wake and scan durations. The user confirmed normal display with no visible flicker on 25 October 2026, and two held-letter readings showed advancing rows with unchanged missed/late counters. See the recorded reference-panel result in `docs/ESP32_TEST_CONFIG.md`. This is limited visual/serial evidence; host fixtures do not measure real interrupt latency, and flash writes can still stall the shifting task.

Framebuffer 1 means ON. Output mapping/polarity apply during scanning; GFX coordinates remain top-left. Brightness is a requested on-time, capped by remaining row time.

## Documentation

[The guide](docs/PANEL_GUIDE.md) describes the current application. The archived [bring-up report](docs/BRING_UP_REPORT.md) records the former diagnostic firmware and original faulty panel; its commands and settings are historical. All six photos are from that original board. An identical replacement worked; no specific failed chip or trace was confirmed.

Rebuild HTML and the share ZIP with Python 3.10+ using [tools/build_docs.py](tools/build_docs.py) and [tools/docs-requirements.txt](tools/docs-requirements.txt). Edit Markdown and the template/CSS, not generated HTML. The ZIP includes the current firmware source snapshot and original photo bytes.

## Checks and examples

`tools/render_examples.py` compiles the actual main/GFX renderer and actual network application against `test_host` fixtures, with address/undefined-behaviour sanitizers. It checks controls, Life, Pong, live-clock/DST/holdover, settings validation, HTTP routes/tokens, NVS failure/reload and reconnects. It also compiles the actual refresh driver against timer/GPIO fixtures for frame boundaries, OE deadlines, late slots, brightness and initialization failures. It renders documented example GIFs from the captured framebuffer. This is not a hardware or real-Wi-Fi test.

`tools/preview_web.py` serves mock status locally for browser UI checks. Rebuild the embedded HTML after edits, run formatting/build/host checks, then regenerate HTML/ZIP. Do not commit or push unless explicitly requested for the current work.
