# Red LED Panel 8×2

**Version 1.7.0** — Wi-Fi setup, live local time, night mode, a ten-scene showcase with your own scrolling message, a faster refresh driver, and firmware upload over Wi-Fi.

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
4. Type **`1`–`9`** or **`c`** (clock) to select a scene, **`n`** for next, **`p`** to pause/resume, or **`+` / `-`** to change brightness. No Enter is needed.

## What's on screen

| Key | Scene | Capability |
|:--|:--|:--|
| `1` | Checkerboard | Individual pixels and alternating patterns |
| `2` | Bouncing balls | Filled/outlined shapes and independent motion |
| `3` | Starfield | Several speeds and short star trails |
| `4` | Pixel rain | Falling, staggered trails |
| `5` | Animated bars | Smooth simulated equalizer animation; no microphone needed |
| `6` | Conway's Life | Gliders and oscillators on a wrapping 64×16 grid |
| `7` | Beating hearts | Enlarged pixel art and a brightness pulse |
| `8` | Scrolling text | Your message from the settings page, or device details |
| `9` | Pong | Automatic paddles, wall rebounds, rallies, and scoring |
| `c` | Live clock | Network time, local time zone/DST, blinking colons, day and date (SUN 25 OCT 2026) |

The default cycle is **clock → checkerboard → clock → bouncing balls → clock → next demo**, continuing through Pong and repeating. The clock stays for **10 seconds** by default; change it on the settings page (5–3600 s). A full rotation takes about **three minutes** with the defaults. Selecting a demo runs it immediately, then returns to the clock before the following demo; use `p` to keep a scene on screen. `f`, `o`, and `b` provide all-off, all-on, and output blanking. `r` restarts at the clock, with the checkerboard next; `?` prints help.

The **scrolling message** is set on the settings page (up to 64 plain characters). Left empty, it shows device details: firmware version, uptime, time since the last network sync, the panel's address and location. The message always scrolls fully off the panel before the clock returns.

Pong plays automatically; no controllers are required. **`c` holds the live clock** on screen; `n` leaves it and `p` resumes the playlist timer. Time continues while the playlist is paused.

## Connect Wi-Fi and set your location

If a saved network is unavailable after 10 seconds, join **`RedPanel-XXXXXX`** using **`PanelSetup32`** (change it in a gitignored `include/secrets.h`, copied from `include/secrets.example.h`), then open **`http://192.168.4.1/`**. Choose your 2.4 GHz Wi-Fi network and enter its password. Credentials are stored on the ESP32, not in the source.

After connection, open **`http://<panel-IP>/`** using the address printed by serial; a sidebar moves between the clock, settings, connection and device sections and the Wi-Fi pages. Set the location, time zone, startup brightness, startup view, clock time between demos, scrolling message and night mode. Defaults are **Sydney**, **Australia/Sydney**, **90/255**, **Clock + demos**, **10 s**, device details, and the display off from **22:00 to 06:00**. Daylight saving follows the selected zone automatically. An optional `SECRET_PANEL_PASSWORD` adds a login to the settings page. **Turn display on / off** buttons on the clock card override night mode until its next change.

The clock waits for an NTP response rather than displaying a sample time. After losing Wi-Fi it continues from its last sync for up to 24 hours, with the colons held solid instead of blinking. Send **`w`** to reopen setup, **`i`** for status, and **`d`** to cycle debug levels.

Display refresh uses hardware-timed blanking and a dedicated scan task, with complete-frame updates to reduce tearing. From v1.2.0 the next row is shifted over hardware SPI while the current row is lit, so the full brightness range is usable. The reference panel ran v1.2.0 with zero missed or late rows. Send `i` for refresh timing counters; see the [flicker check](docs/ESP32_TEST_CONFIG.md#check-display-flicker) after uploading. Clock colons blink and hearts pulse deliberately.

The reference ESP32/panel checks on **25 October 2026** (v1.1.0, then v1.2.0) confirmed Wi-Fi connection, NTP time and clock/demo progression. The user observed no visible flicker, with no increase in missed/late refresh counters between two held-letter readings.

The [ESP32 reference test setup](docs/ESP32_TEST_CONFIG.md) contains every connection, buffer pin, network address, default setting and bench check. [Screen examples](docs/SCREEN_EXAMPLES.md) show rendered output for every demo and startup/clock state.

## Make it yours

Set the scrolling message and clock time on the settings page. Change **`DEFAULT_BRIGHTNESS`** and animation timing in [include/config.h](include/config.h). Scene lengths and drawing routines are in [src/main.cpp](src/main.cpp). The [panel guide](docs/PANEL_GUIDE.md) includes the complete control reference, power and wiring details, and examples for drawing your own text and graphics.

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
