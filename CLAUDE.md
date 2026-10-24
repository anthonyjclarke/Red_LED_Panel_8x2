# Maintainer notes — Red LED Panel 8×2

## Target

- ESP32 DevKit (`esp32dev`), `espressif32@6.9.0` = Arduino core 2.0.17 / IDF 4.4. Keep pinned: the driver uses legacy `driver/timer.h` (no `gptimer` before IDF 5).
- Red HUB08 64×16 (8×2 modules), 1/16 scan, Adafruit GFX, WiFiManager 2.0.17. Firmware v1.3.0.
- GPIO 25/26/27/13 → LA/LB/LC/LD, 22 → EN, 4 → LT direct register writes (all GPIO must be < 32). 23 → R1 and 18 → SK are VSPI (SPI HAL `spiWriteNL`, MSB first).
- Active-low data and OE; reversed column stream (bit-reversed bytes in software), row bits `{0,1,2,3}`, row XOR `0x0F`.

## Refresh driver rules

- TG0/T0 (row tick) and TG0/T1 (OE off) are reserved. Allocate both with `ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM`; two exclusive level-3 requests failed with `ESP_ERR_NOT_FOUND` and left the panel blank.
- ISRs only blank OE / notify; no logging, shifting or allocation. Row work runs in a high-priority task on the setup core, never in `esp_timer`.
- Per tick: blank → address → latch the columns shifted in the previous slot → arm OE-off → shift the next row while lit. Frames commit only when row 0 is prepared; brightness when row 0 latches.
- Late ticks leave the row dark; skip backlogs. Keep 500 µs row, 1 µs latch phases, 50 µs guard. Unwind timers, interrupts, SPI and task on any init failure.
- v1.2.0 (overlapped 500 kHz SPI shift) confirmed on the reference panel 25 Oct 2026: 0 missed/late rows, peak wake 237 µs. v1.1.0 shifted while dark.

## Application rules

- Pong state changes only in `stepPong()`. Clock never invents an epoch; holdover ends 24 h after last sync. Clock visits end by elapsed time, not frames.
- Unsigned `millis()` subtraction everywhere; render one frame, never replay missed frames. Log transitions, not frames.
- Web settings save applies brightness live; the view restarts only if `startClock` changed. Serial brightness saves after a 5 s quiet period.

## Network and storage

- NVS namespace `redpanel`, key `clock`: one `ClockSettings` record (schema 1). Validate before use; a failed write must not change active settings. NVS writes can stall the scan task briefly.
- Register custom routes in WiFiManager's web-server callback so they survive portal/LAN switches; it runs before WiFiManager adds its own, so our `/` wins. WiFiManager pages get CSS and a nav bar through `setCustomHeadElement`. POSTs need the boot-local token.
- `secrets.h` (gitignored) supplies `SECRET_AP_PASSWORD`, optional `SECRET_PANEL_PASSWORD` and `SECRET_OTA_PASSWORD` (empty = OTA off); `secrets.example.h` is the fallback. Never log credentials.
- ArduinoOTA starts on first Wi-Fi connect with its own mDNS disabled; `loop()` owns mDNS and re-adds the `arduino` service on reconnect. `ArduinoOTA.handle()` blocks during an upload, so progress drawing runs from its callbacks. `default_envs = esp32dev` keeps plain uploads on USB.
- `include/ConfigPage.h` is generated from `web/index.html` by `tools/embed_web.py` (pre-build hook). Edit the HTML, not the header.

## Checks and docs

- `tools/render_examples.py` runs host regressions (app, network, driver fixtures) and regenerates GIFs and `docs/SCREEN_EXAMPLES.md`. Not a hardware test.
- `tools/build_docs.py` builds the single `index.html` (all Markdown, CSS inlined, release log from CHANGELOG and git) and the share ZIP. Never add per-document HTML.
- `BRING_UP_REPORT.md` and all six photos describe the original faulty panel; historical only.
