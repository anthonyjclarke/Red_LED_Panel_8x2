# Red LED Panel 8×2

**Version 1.3.0** — Wi-Fi setup, live local time, an eleven-scene showcase, a faster refresh driver, and firmware upload over Wi-Fi.

An animated showcase for an **ESP32 and a red 64×16 HUB08 panel**: bouncing balls, a starfield, pixel rain, animated bars, Conway's Life, beating hearts, scrolling text, Pong, and a network-synchronised clock. Two simple patterns make it easy to check placement and pixel detail.

The panel is an **8×2 grid of 8×8 modules**, with one colour driven through R1. Drawing uses Adafruit GFX; the driver continuously refreshes the display in the background.

## Get started

1. Connect the panel's **5 V supply**, common ground, and eight control signals through a level shifter. Use the [wiring table](docs/PANEL_GUIDE.md#esp32-to-matrix-wiring).
2. Open this project in VS Code with PlatformIO. Build and upload:

```sh
pio run
pio run --target upload
pio device monitor --baud 115200
```

   After the first USB upload, set `SECRET_OTA_PASSWORD` in `include/secrets.h` to upload over Wi-Fi with `pio run -e esp32dev_ota --target upload`; see [Upload over Wi-Fi](docs/PANEL_GUIDE.md#upload-over-wi-fi-ota).
3. Three boot cards show the header, display settings, and network state, then the clock starts the cycle. Serial Monitor prints the full startup/settings report and each scene.
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
| `c` | Live clock | Network time, local time zone/DST, blinking colons, and seconds bar |

The default cycle is **clock (10 seconds) → letters → clock (10 seconds) → checkerboard → clock → next demo**, continuing through Pong and repeating. A full rotation takes about **2 minutes 47 seconds** with the default text. Selecting a demo runs it immediately, then returns to the clock before the following demo; use `p` to keep a scene on screen. `t` holds the letter pattern immediately. `f`, `o`, and `b` provide all-off, all-on, and output blanking. `r` restarts at the clock, with letters next; `?` prints help.

Pong plays automatically; no controllers are required. **`c` holds the live clock** on screen; `n` leaves it and `p` resumes the playlist timer. Time continues while the playlist is paused.

## Connect Wi-Fi and set your location

If a saved network is unavailable after 10 seconds, join **`RedPanel-XXXXXX`** using **`PanelSetup32`** (change it in a gitignored `include/secrets.h`, copied from `include/secrets.example.h`), then open **`http://192.168.4.1/`**. Choose your 2.4 GHz Wi-Fi network and enter its password. Credentials are stored on the ESP32, not in the source.

After connection, open **`http://<panel-IP>/panel`** using the address printed by serial. Set the location, time zone, startup brightness and startup view. Defaults are **Sydney**, **Australia/Sydney**, **90/255**, and **Clock + demos**. Daylight saving follows the selected zone automatically. An optional `SECRET_PANEL_PASSWORD` adds a login to the settings page.

The clock waits for an NTP response rather than displaying a sample time. After losing Wi-Fi it continues from its last sync, labelled **HLD**, for up to 24 hours. Send **`w`** to reopen setup, **`i`** for status, and **`d`** to cycle debug levels.

Display refresh uses hardware-timed blanking and a dedicated scan task, with complete-frame updates to reduce tearing. From v1.2.0 the next row is shifted over hardware SPI while the current row is lit, so the full brightness range is usable. The reference panel ran v1.2.0 with zero missed or late rows. Send `i` for refresh timing counters; see the [flicker check](docs/ESP32_TEST_CONFIG.md#check-display-flicker) after uploading. Clock colons blink and hearts pulse deliberately.

The reference ESP32/panel checks on **25 October 2026** (v1.1.0, then v1.2.0) confirmed Wi-Fi connection, NTP time and clock/demo progression. The user observed no visible flicker, with no increase in missed/late refresh counters between two held-letter readings.

The [ESP32 reference test setup](docs/ESP32_TEST_CONFIG.md) contains every connection, buffer pin, network address, default setting and bench check. [Screen examples](docs/SCREEN_EXAMPLES.md) show rendered output for every demo and startup/clock state.

## Make it yours

Change **`SCROLL_TEXT`**, **`DEFAULT_BRIGHTNESS`**, and animation timing in [include/config.h](include/config.h). Scene lengths and drawing routines are in [src/main.cpp](src/main.cpp). The [panel guide](docs/PANEL_GUIDE.md) includes the complete control reference, power and wiring details, and examples for drawing your own text and graphics.

The pin map, orientation and timing settings are in the [panel guide](docs/PANEL_GUIDE.md#esp32-to-matrix-wiring). Monitor speed is **115200 baud**.

## Documentation and sharing

Open **[index.html](index.html)** in a browser for the offline documentation: every Markdown document combined into one page with a section sidebar and print layout. It includes the [panel guide](docs/PANEL_GUIDE.md), [ESP32 test configuration](docs/ESP32_TEST_CONFIG.md), [screen examples](docs/SCREEN_EXAMPLES.md), [hardware photos](docs/PHOTO_LOG.md), and [changelog](CHANGELOG.md). Earlier troubleshooting is retained at the end as the [archived bring-up report](docs/BRING_UP_REPORT.md).

Markdown is the editable source; there are no per-document HTML files. To regenerate `index.html` and **`Red_LED_Panel_8x2_Documentation.zip`**, use Python 3.10 or newer:

```sh
python3 -m venv .venv-docs
.venv-docs/bin/python -m pip install -r tools/docs-requirements.txt
.venv-docs/bin/python tools/build_docs.py
```

To regenerate the screen examples from the actual firmware/GFX renderer, install project libraries with `pio pkg install`, then run `.venv-docs/bin/python tools/render_examples.py` (requires a C++17 compiler). The command also runs the host regressions. Edit `web/index.html` for the embedded settings page and PlatformIO embeds it automatically before building firmware (`python3 tools/embed_web.py` also works directly). For a browser preview with mock status: `python3 tools/preview_web.py`.

Logging follows the shared `[ERR ]` / `[WARN]` / `[INFO]` / `[VERB]` convention. C++ formatting is defined in [.clang-format](.clang-format); the [test setup guide](docs/ESP32_TEST_CONFIG.md#boot-status-and-debug-conventions) has check commands.

Extract the ZIP and open `index.html`; keep the `images/` folder beside it. No server or internet connection is needed except to follow external reference links.

Repository: [anthonyjclarke/Red_LED_Panel_8x2](https://github.com/anthonyjclarke/Red_LED_Panel_8x2).
