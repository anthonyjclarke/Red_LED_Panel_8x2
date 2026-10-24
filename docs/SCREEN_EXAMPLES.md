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

## 04 — Letters

Key 1, or t to hold: A-H above I-P, one letter per 8x8 module.

![Rendered LETTERS output](../images/demos/04-letters.gif)

## 05 — Checkerboard

Key 2: alternating single-pixel phases.

![Rendered CHECKERBOARD output](../images/demos/05-checkerboard.gif)

## 06 — Bouncing balls

Key 3: independent moving circles, filled and outlined.

![Rendered BOUNCING BALLS output](../images/demos/06-bouncing-balls.gif)

## 07 — Starfield

Key 4: several speeds and short horizontal star trails.

![Rendered STARFIELD output](../images/demos/07-starfield.gif)

## 08 — Pixel rain

Key 5: staggered falling trails.

![Rendered PIXEL RAIN output](../images/demos/08-pixel-rain.gif)

## 09 — Animated bars

Key 6: simulated equalizer bars; no audio input.

![Rendered ANIMATED BARS output](../images/demos/09-animated-bars.gif)

## 10 — Conway's life

Key 7: actual glider/blinker updates on wrapping edges.

![Rendered CONWAY'S LIFE output](../images/demos/10-conway-s-life.gif)

## 11 — Beating hearts

Key 8: enlarged pixel art with whole-display brightness modulation.

![Rendered BEATING HEARTS output](../images/demos/11-beating-hearts.gif)

## 12 — Scrolling text

Key 9 or s: full-height text crossing the screen.

![Rendered SCROLLING TEXT output](../images/demos/12-scrolling-text.gif)

## 13 — Pong

Key g: automatic paddles, collisions, goals and fresh serves.

![Rendered PONG output](../images/demos/13-pong.gif)

## 14 — Clock

Starts the cycle and returns for 10 seconds between demos. Key c holds it indefinitely: local HH:MM:SS, blinking colons, seconds bar and NTP source label. Captured with a fixed network-time fixture.

![Rendered CLOCK output](../images/demos/14-clock.gif)

## 15 — All pixels off

Key f: an empty pixel buffer at the selected brightness.

![Rendered All pixels off output](../images/demos/15-all-pixels-off.gif)

## 16 — All pixels on

Key o: every pixel on at the selected brightness.

![Rendered All pixels on output](../images/demos/16-all-pixels-on.gif)

## 17 — Output blanked

Key b: output disabled. Visually dark like f; it uses zero output on-time.

![Rendered Output blanked output](../images/demos/17-output-blanked.gif)

## 18 — Clock offline holdover

Time continues from its last sync; HLD replaces the NTP label.

![Rendered Clock offline holdover output](../images/demos/18-clock-offline-holdover.gif)

## 19 — Clock waiting for Wi-Fi

No valid time yet: NO WIFI / JOIN AP. No sample epoch is displayed.

![Rendered Clock waiting for Wi-Fi output](../images/demos/19-clock-waiting-for-wi-fi.gif)

## 20 — Clock waiting for NTP

Wi-Fi is connected, but time has not arrived: SYNC TIME / WAIT NTP.

![Rendered Clock waiting for NTP output](../images/demos/20-clock-waiting-for-ntp.gif)

## Regenerate and verify

Install the project libraries with `pio pkg install`. With a C++17 compiler and the dependencies in `tools/docs-requirements.txt`, run:

```sh
python3 tools/render_examples.py
python3 tools/build_docs.py
```

The renderer runs host regressions with address/undefined-behaviour sanitizers before writing these GIFs and their [manifest](../images/demos/manifest.json). Tests cover clock/demo alternation and deadlines, controls, Conway rules, Pong, live-clock/DST/holdover, settings validation, portal/LAN routes, NVS failure/reload and reconnects. The actual refresh driver is exercised with timer/GPIO fixtures for scan-boundary frames, OE deadlines, late rows and brightness. Real interrupt latency, Wi-Fi association, captive portals and electrical output require the bench checks.

Original panel photographs remain separate in the [hardware photo log](PHOTO_LOG.md).
