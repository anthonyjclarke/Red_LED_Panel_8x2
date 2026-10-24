# Maintainer notes — Red LED Panel 8×2

## Target and configuration

- ESP32 DevKit, PlatformIO `esp32dev`, `espressif32@6.9.0`, Arduino framework.
- Red HUB08 64×16, eight by two 8×8 modules, 1/16 scan; Adafruit GFX drawing.
- GPIO 25/26/27/13 → LA/LB/LC/LD, 22 → EN, 23 → R1, 4 → LT, 18 → SK.
- Active-low data/OE. Reversed column stream, normal row-bit order `{0,1,2,3}`, row XOR `0x0F`.
- All configured GPIOs must be below 32 for direct register writes.
- The project folder's RGB name is historical; the firmware drives red only.

## Showcase application

`src/main.cpp` has an eleven-scene looping playlist: letters, checkerboard, balls, stars, rain, animated bars, Life, hearts, scrolling, Pong, and a simulated clock. Each scene has a frame count and interval; scrolling derives its count from text bounds. Animation timing uses unsigned millisecond subtraction, renders one frame at a time, and does not replay missed frames. Serial prints scene transitions and user commands, not individual animation frames.

`1`–`9`, `g` (Pong), and `c` (clock) select a scene; `n` advances; `p` pauses/resumes; `r` restarts. `t` holds letters; `s` selects scrolling. `f/o/b` hold off/on/blank; `p/r/n` return to the playlist. Brightness changes with `+/-` in steps of 15, bounded to 0–255; hearts modulate up to that requested level. Conway's Life uses packed buffers and wrapping neighbours. Bars are simulated, with no audio input.

Pong uses quarter-pixel ball positions, bounded paddle tracking, collision/reflection rules, single-digit scores, and a serve delay. Update its state only in `stepPong()`, not rendering, so pause and brightness controls preserve gameplay. Clock derives simulated seconds from `CLOCK_START_SECONDS + frame / 2`, wraps at midnight, and renders 3×5 digits at scale 2 with a SIM label. It needs no network or RTC and restarts on scene entry.

Retired diagnostics (`e` polarity, `l` latch width, pre-scroll black comparison, row/address walks) are absent from the current application and driver API. Keep the working wiring, polarity, and scan timing stable when adding scenes.

## Refresh driver

`Hub08Panel` subclasses `Adafruit_GFX`. Draw into `drawBuf_`; `show()` copies 128 bytes to `showBuf_` under the same lock used for row reads. This is not a frame-boundary swap.

A periodic `esp_timer` callback blanks output, cancels an old off event, shifts 64 bits, sets the row address, pulses LT, and enables a bounded light window. A one-shot timer blanks it. Keep the 500 µs row period, 1 µs signal phases, 50 µs guard, and overdue-event skipping. Nominal complete-frame rate is 125 Hz; animation frame rate is separate. Pausing application animation must leave scanning running.

Framebuffer 1 means ON. Output mapping/polarity apply during scanning; GFX coordinates remain top-left. Brightness is a requested on-time, capped by remaining row time.

## Documentation

[The guide](docs/PANEL_GUIDE.md) describes the current application. The archived [bring-up report](docs/BRING_UP_REPORT.md) records the former diagnostic firmware and original faulty panel; its commands and settings are historical. All six photos are from that original board. An identical replacement worked; no specific failed chip or trace was confirmed.

Rebuild HTML and the share ZIP with Python 3.10+ using [tools/build_docs.py](tools/build_docs.py) and [tools/docs-requirements.txt](tools/docs-requirements.txt). Edit Markdown and the template/CSS, not generated HTML. The ZIP includes the current firmware source snapshot and original photo bytes.
