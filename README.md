# Red LED Panel 8×2

**Version 1.0.0** — working-panel showcase and complete setup guide.

An animated showcase for an **ESP32 and a red 64×16 HUB08 panel**: bouncing balls, a starfield, pixel rain, animated bars, Conway's Life, beating hearts, scrolling text, Pong, and a simulated clock. Two simple patterns make it easy to check placement and pixel detail.

The panel is an **8×2 grid of 8×8 modules**, with one colour driven through R1. Drawing uses Adafruit GFX; the driver continuously refreshes the display in the background.

## Get started

1. Connect the panel's **5 V supply**, common ground, and eight control signals through a level shifter. Use the [wiring table](docs/PANEL_GUIDE.md#esp32-to-matrix-wiring).
2. Open this project in VS Code with PlatformIO. Build and upload:

```sh
pio run
pio run --target upload
pio device monitor --baud 115200
```

3. The eleven-scene playlist starts automatically and repeats. Serial Monitor prints each scene as it begins.
4. Type **`1`–`9`**, **`g`** (Pong), or **`c`** (clock) to select a scene, **`n`** for next, **`p`** to pause/resume, or **`+` / `-`** to change brightness. No Enter is needed.

## What's on screen

| Key | Scene | Capability |
|:--|:--|:--|
| `1` | Letters | A–H above I–P, one character per module |
| `2` | Checkerboard | Individual pixels and alternating patterns |
| `3` | Bouncing balls | Filled/outlined shapes and independent motion |
| `4` | Starfield | Several speeds and short star trails |
| `5` | Pixel rain | Falling, staggered trails |
| `6` | Animated bars | Smooth simulated equalizer animation; no microphone needed |
| `7` | Conway's Life | Gliders and oscillators on a wrapping 64×16 grid |
| `8` | Beating hearts | Enlarged pixel art and a brightness pulse |
| `9` | Scrolling text | Full-height text moving across the display |
| `g` | Pong | Automatic paddles, wall rebounds, rallies, and scoring |
| `c` | Clock simulation | HH:MM:SS, blinking colons, seconds bar, and midnight rollover |

The default playlist takes about **83 seconds**. Scene selection continues from that point in the playlist; use `p` to keep a scene on screen. `t` holds the letter pattern immediately. `f`, `o`, and `b` provide all-off, all-on, and output blanking. `r` restarts the playlist; `?` prints help.

The clock is a self-contained simulation starting at **23:59:55** each time it is selected. It needs no Wi-Fi or RTC. Pong plays automatically; no controllers are required.

## Make it yours

Change **`SCROLL_TEXT`**, **`DEFAULT_BRIGHTNESS`**, and animation timing in [include/config.h](include/config.h). Scene lengths and drawing routines are in [src/main.cpp](src/main.cpp). The [panel guide](docs/PANEL_GUIDE.md) includes the complete control reference, power and wiring details, and examples for drawing your own text and graphics.

The default ESP32 pin map is **25 → LA, 26 → LB, 27 → LC, 13 → LD, 22 → EN, 23 → R1, 4 → LT, 18 → SK**. Monitor speed is **115200 baud**. The configured panel orientation uses reversed columns and row-address XOR `0x0F`; row bits are in normal A/B/C/D order.

## Documentation and sharing

Open **[index.html](index.html)** in a browser for the offline HTML edition. It includes the [panel guide](docs/PANEL_GUIDE.md), [hardware photos](docs/PHOTO_LOG.md), and [changelog](CHANGELOG.md). Earlier troubleshooting is retained separately in the [archived bring-up report](docs/BRING_UP_REPORT.md).

Markdown is the editable source. To regenerate the HTML and **`Red_LED_Panel_8x2_Documentation.zip`**, use Python 3.10 or newer:

```sh
python3 -m venv .venv-docs
.venv-docs/bin/python -m pip install -r tools/docs-requirements.txt
.venv-docs/bin/python tools/build_docs.py
```

Extract the ZIP and open `index.html`; keep its folders together. No server or internet connection is needed except to follow external reference links.

Repository: [anthonyjclarke/Red_LED_Panel_8x2](https://github.com/anthonyjclarke/Red_LED_Panel_8x2).
