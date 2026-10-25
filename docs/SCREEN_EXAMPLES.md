# Screen output examples

Generated from the actual `src/main.cpp` drawing routines and installed Adafruit GFX library, using host fixtures for panel GPIO, Wi-Fi and time. These are **rendered examples, not photographs or proof of hardware operation**.

Each dot represents one pixel. GIFs sample animation frames to keep the archive compact; dot colour and brightness are illustrative, not calibrated light measurements. Clock values come from a fixed host time and do not represent the time when you open this document.

Use the [panel guide](PANEL_GUIDE.md) for controls and the [ESP32 test configuration](ESP32_TEST_CONFIG.md) for wiring, Wi-Fi and bench checks.

## 01 — Boot 1

Startup header and firmware version.

![Rendered Boot 1 output](../images/demos/01-boot-1.gif)

## 02 — Boot 2

Panel geometry, scan ratio and selected brightness.

![Rendered Boot 2 output](../images/demos/02-boot-2.gif)

## 03 — Boot 3

Network/time status. This fixture is connected and synchronised; a fresh offline boot shows waiting states instead.

![Rendered Boot 3 output](../images/demos/03-boot-3.gif)

## 04 — Checkerboard

Key 1: alternating single-pixel phases.

![Rendered CHECKERBOARD output](../images/demos/04-checkerboard.gif)

## 05 — Bouncing balls

Key 2: independent moving circles, filled and outlined.

![Rendered BOUNCING BALLS output](../images/demos/05-bouncing-balls.gif)

## 06 — Starfield

Key 3: several speeds and short horizontal star trails.

![Rendered STARFIELD output](../images/demos/06-starfield.gif)

## 07 — Pixel rain

Key 4: staggered falling trails.

![Rendered PIXEL RAIN output](../images/demos/07-pixel-rain.gif)

## 08 — Animated bars

Key 5: simulated equalizer bars; no audio input.

![Rendered ANIMATED BARS output](../images/demos/08-animated-bars.gif)

## 09 — Conway's life

Key 6: actual glider/blinker updates on wrapping edges.

![Rendered CONWAY'S LIFE output](../images/demos/09-conway-s-life.gif)

## 10 — Beating hearts

Key 7: enlarged pixel art with whole-display brightness modulation.

![Rendered BEATING HEARTS output](../images/demos/10-beating-hearts.gif)

## 11 — Scrolling text

Key 8 or s: the message from the settings page, or device details when it is empty (shown here with fixture values). It always scrolls fully before the clock returns.

![Rendered SCROLLING TEXT output](../images/demos/11-scrolling-text.gif)

## 12 — Pong

Key 9: automatic paddles, collisions, goals and fresh serves.

![Rendered PONG output](../images/demos/12-pong.gif)

## 13 — Clock

Starts the cycle and returns between demos for the clock time set on the web page (default 10 seconds). Key c holds it indefinitely: local HH:MM:SS with colons blinking each second, and the day and date below, for example SUN 25 OCT 2026. Captured with a fixed network-time fixture.

![Rendered CLOCK output](../images/demos/13-clock.gif)

## 14 — All pixels off

Key f: an empty pixel buffer at the selected brightness.

![Rendered All pixels off output](../images/demos/14-all-pixels-off.gif)

## 15 — All pixels on

Key o: every pixel on at the selected brightness.

![Rendered All pixels on output](../images/demos/15-all-pixels-on.gif)

## 16 — Output blanked

Key b: output disabled. Visually dark like f; it uses zero output on-time.

![Rendered Output blanked output](../images/demos/16-output-blanked.gif)

## 17 — Clock offline holdover

Time continues from its last sync; the colons stay solid instead of blinking.

![Rendered Clock offline holdover output](../images/demos/17-clock-offline-holdover.gif)

## 18 — Clock waiting for Wi-Fi

No valid time yet: NO WIFI / JOIN AP. No sample epoch is displayed.

![Rendered Clock waiting for Wi-Fi output](../images/demos/18-clock-waiting-for-wi-fi.gif)

## 19 — Clock waiting for NTP

Wi-Fi is connected, but time has not arrived: SYNC TIME / WAIT NTP.

![Rendered Clock waiting for NTP output](../images/demos/19-clock-waiting-for-ntp.gif)

## Regenerate and verify

Install the project libraries with `pio pkg install`. With a C++17 compiler and the dependencies in `tools/docs-requirements.txt`, run:

```sh
python3 tools/render_examples.py
python3 tools/build_docs.py
```

The renderer runs host regressions with address/undefined-behaviour sanitizers before writing these GIFs and their [manifest](../images/demos/manifest.json). Tests cover clock/demo alternation and deadlines, controls, Conway rules, Pong, live-clock/DST/holdover, settings validation, portal/LAN routes, NVS failure/reload and reconnects. The actual refresh driver is exercised with timer/GPIO fixtures for scan-boundary frames, OE deadlines, late rows and brightness. Real interrupt latency, Wi-Fi association, captive portals and electrical output require the bench checks.

Original panel photographs remain separate in the [hardware photo log](PHOTO_LOG.md).
