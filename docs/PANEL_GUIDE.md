# Panel hardware and usage guide

Build a compact animated display with an **ESP32, a 5 V HUB08 panel, and Adafruit GFX**. This guide covers the working setup and the eleven-scene showcase.

## What the panel can do

This red **P3.75, 64×16** panel contains sixteen 8×8 modules in an **8×2 grid**. Every pixel is individually addressable, making it suitable for text, icons, simple games, cellular automata, and motion effects. Pixels are on/off; brightness control applies to the whole display rather than to individual pixels.

The driver scans one of sixteen rows at a time. Background refresh continues while your application draws or pauses. The configured 500 µs row period gives a nominal **125 Hz complete refresh**; scene animation runs separately, typically at 25 frames per second.

![Rear view of the panel with INPUT200, OUTPUT201, drivers, and the power terminal](../images/03-panel-rear.jpg)

*Rear PCB: INPUT200 at left, OUTPUT201 at right, power near the centre. These reference photos come from the original bring-up board.*

## Power and level shifting

Use a **regulated 5 V supply** connected directly to the panel's **VCC/GND screw terminal**. A 2–3 A supply is a practical starting point, not a measured maximum panel rating. Keep power leads short and adequately sized. Join the panel, ESP32, and level-shifter grounds. Route LED-panel current through its power terminal rather than through the ESP32 or breadboard rails.

Start at the default brightness **90/255**. Increasing brightness or the number of lit pixels increases the load. USB can power the ESP32 while the separate supply powers the matrix; their grounds must be common.

Pass all eight control signals through a level shifter from ESP32 3.3 V logic to the panel's 5 V logic. The recorded setup uses an **EzSBC LS1**. Its VLo connects to 3.3 V, VHi to the panel logic supply, and each Lo channel pairs with the matching Hi channel. LS1 uses resistor pull-ups; a **74AHCT245** provides actively driven outputs and is another suitable interface. [EzSBC LS1 description](https://levelshifter.weebly.com/), [manufacturer design notes](https://news.ezsbc.com/new-level-shifter-buffer/)

### ESP32-to-matrix wiring

These are **ESP32 GPIO numbers**, not development-board header positions. Connect to **INPUT200** beside U201.

| ESP32 GPIO | Matrix signal | Config name | Function |
|:--|:--|:--|:--|
| 25 | LA | `PIN_A` | Row-address A |
| 26 | LB | `PIN_B` | Row-address B |
| 27 | LC | `PIN_C` | Row-address C |
| 13 | LD | `PIN_D` | Row-address D |
| 22 | EN | `PIN_OE` | Output enable |
| 23 | R1 | `PIN_R1` | Red pixel data |
| 4 | LT | `PIN_LAT` | Column latch |
| 18 | SK | `PIN_CLK` | Shift clock |
| GND | GND | — | Common reference |

All eight signals travel **ESP32 → low-side input → matching high-side output → matrix input**. R2/G1/G2 are unused by this firmware. The configured pins are below 32, as required by the driver's direct GPIO writes.

### Connector reference

The pictured board has an unmarked INPUT200 and labelled OUTPUT201. With the **rear PCB text upright**, OUTPUT201 reads as follows. Row numbers are descriptive, not official IDC pin numbers.

```text
                    LEFT       RIGHT
Top, row 1          GND        LA
     row 2          GND        LB
     row 3          GND        LC
     row 4          EN         LD
     row 5          R1         G1
     row 6          R2         G2
     row 7          GND        LT
Bottom, row 8       GND        SK
```

The matching INPUT200 ground positions in left rows 1/2/3/7/8 were verified on this board. Use this as the connector orientation reference for this panel, not a universal pinout for every HUB08 product. OUTPUT201 is for cascading onward; this firmware controls one 64×16 panel.

![OUTPUT201 connector showing the labelled signal positions](../images/04-output201-header.jpg)

### Using a 74AHCT245

For a standard 20-pin 74AHCT245, connect **pin 20/VCC and pin 1/DIR to 5 V**, and **pin 10 (GND) and pin 19 (/OE) to ground**. Add a 100 nF decoupling capacitor close to VCC/GND. This selects A-to-B direction. Verify the exact package pinout used. The chip's buffer-enable pin is separate from matrix EN. [TI datasheet](https://www.ti.com/lit/ds/symlink/sn74ahct245.pdf)

| GPIO | Buffer input | Buffer output | Matrix |
|:--|:--|:--|:--|
| 25 | A1 / pin 2 | B1 / pin 18 | LA |
| 26 | A2 / pin 3 | B2 / pin 17 | LB |
| 27 | A3 / pin 4 | B3 / pin 16 | LC |
| 13 | A4 / pin 5 | B4 / pin 15 | LD |
| 22 | A5 / pin 6 | B5 / pin 14 | EN |
| 23 | A6 / pin 7 | B6 / pin 13 | R1 |
| 4 | A7 / pin 8 | B7 / pin 12 | LT |
| 18 | A8 / pin 9 | B8 / pin 11 | SK |

## Build and run

Open the project in **VS Code with PlatformIO**, select environment **esp32dev**, and use Build, Upload, then Serial Monitor. The project uses `espressif32@6.9.0`, Arduino, and Adafruit GFX. From the PlatformIO terminal:

```sh
pio run
pio device list
pio run --target upload
pio device monitor --baud 115200
```

Choose the USB port for your ESP32 if several devices are connected. Close an existing monitor before uploading. Reset the board with Serial Monitor open to see the startup message.

The display shows three startup cards, then enters the saved startup view: Clock + demos by default, or a held live clock. Serial prints each scene's number, name, and short description. There is no long blank interval between scenes.

## Wi-Fi, location and startup status

The firmware includes **WiFiManager 2.0.17**, persistent panel settings, and NTP time. If it cannot connect using saved credentials within 10 seconds, it opens **RedPanel-XXXXXX** with setup password **PanelSetup32**. Join it and open **http://192.168.4.1/** to select a 2.4 GHz network.

Once connected, open the serial-reported **http://<panel-IP>/panel**. Set your location label, time zone, startup brightness and startup view. Defaults are Sydney / Australia/Sydney, 90/255 and Clock + demos. The page also shows the device's time, network and hardware settings. **`w`** reopens setup and **`i`** prints status.

Boot prints the firmware header, GPIO map, timing, orientation, location/time zone and connection details. Three 2-second panel cards show version, geometry/brightness, then network/time state. Saved settings are loaded before displaying content at the configured brightness.

Use the [complete ESP32 test configuration](ESP32_TEST_CONFIG.md) for exact buffer/header connections, provisioning, network services, persistence and acceptance checks. [Generated examples](SCREEN_EXAMPLES.md) show the expected output for all scenes and status states.

## The showcase playlist

| Key | Scene | Default time | What it demonstrates |
|:--|:--|:--|:--|
| `1` | Letters | 2.5 s | A–H above I–P, one small letter in each module |
| `2` | Checkerboard | 1.2 s | Six alternating pixel patterns |
| `3` | Bouncing balls | 5.6 s | Three independent paths, filled and outlined circles |
| `4` | Starfield | 5.6 s | Twenty-four stars, three speeds, short trails |
| `5` | Pixel rain | 6 s | Staggered falling trails of different lengths |
| `6` | Animated bars | 6.4 s | Eight smoothly interpolated bars and peak marks |
| `7` | Conway's Life | 7.2 s | Gliders and blinkers with wrapping edges |
| `8` | Beating hearts | 4 s | Three enlarged pixel-art hearts with pulsing brightness |
| `9` | Scrolling text | About 12.2 s | Size-2 text crossing the full screen |
| `g` | Pong | 16 s | Automatic paddles, ball physics, goals, and a small scoreboard |
| `c` | Live clock | 10 s between demos; held by `c` | NTP local time, large HH:MM:SS digits, blinking colons, seconds bar |

**Clock (10 s) → letters → clock (10 s) → checkerboard → clock → balls**, and so on through Pong, then repeat. A complete rotation is approximately **2 minutes 47 seconds** with the default text. A directly selected demo returns to the clock, then advances to the following demo. `n` follows the same clock/demo alternation. The bars are a simulated equalizer, not an audio measurement. Life is a real cellular-automaton update: a cell survives with two or three neighbours and is born with three. It starts from the same gliders and blinkers each time that scene is entered.

Hearts vary the whole-panel requested brightness from about one fifth of your selected level up to that level. The display is monochrome, so the star and rain trails use shapes and spacing rather than per-pixel grey levels.

### Pong and live clock

**Pong** is a fully automatic match. Two paddles follow a two-pixel ball, which rebounds off the top, bottom, and paddles. Paddles sometimes miss after a rally so goals and new serves occur. Each score is a single digit that wraps after 9; selecting `g` starts a fresh match. Use `p` to freeze or resume play.

**Clock** displays real local time after synchronising with NTP. It uses the saved time zone for daylight saving. Before the first sync it shows **NO WIFI / JOIN AP** or **SYNC TIME / WAIT NTP**. The lower label is **NTP** when connected, **HLD** while holding time offline. Selecting `c` holds the clock without resetting it; pausing the playlist leaves time running. No RTC module is required, but a fresh boot waits for network time.

## Serial controls

Click inside the PlatformIO **Serial Monitor terminal** and type a key. **No Enter or comma is required.** Letter commands also accept uppercase. Local echo may be off; the firmware's printed response confirms receipt.

| Key | Action |
|:--|:--|
| `1`–`9` | Start the chosen scene, then continue the playlist |
| `g` | Start a fresh Pong simulation |
| `c` | Hold the live clock; does not reset the time |
| `w` | Open the Wi-Fi setup hotspot without erasing settings |
| `i` | Print the startup/settings status report |
| `d` | Cycle runtime debug level 0–4 |
| `n` | Start the next scene; resumes if paused |
| `p` | Pause/resume the playlist; refresh and live clock continue |
| `r` | Restart at the clock, with letters next |
| `t` | Hold the static letters indefinitely; `p` resumes their timer |
| `s` | Jump directly to scrolling text, then continue the playlist |
| `f` | Hold all pixels OFF |
| `o` | Hold all pixels ON at the selected brightness |
| `b` | Blank the output at brightness zero |
| `+` / `-` | Change requested brightness by 15, bounded to 0–255; `=` also increases it |
| `?` / `h` | Print help, scene list, and selected brightness |

From held `f/o/b`, **`p`, `r`, or `n` returns to the clock** (`r` also resets the next demo to letters). Scene keys (`1`–`9`, `g`, `c`) also leave any held pattern. Changing brightness preserves the current frame and pause state; `b` remains blank even if you increase the selected level. The selected brightness persists between scenes and resets to the saved web setting (or compiled default) when the ESP32 resets. Brightness zero remains dark until increased again.

Requested brightness is an on-time setting, not a calibrated light percentage. At higher settings the driver caps it to the time remaining after shifting, so the upper end can saturate.

For a clear placement reference, send `t`:

```text
A B C D E F G H
I J K L M N O P
```

The rightmost three module columns should show F/G/H above N/O/P. Use `p` to continue from this held scene, or select another scene directly.

## Customise the showcase

Edit [include/config.h](../include/config.h), rebuild, and upload:

| Setting | Default | Purpose |
|:--|:--|:--|
| `DEFAULT_BRIGHTNESS` | 90 | Fallback brightness before any web setting is saved |
| `SCROLL_TEXT` | `ESP32 + HUB08 = FUN!` | Message in scene 9 |
| `LETTER_HOLD_MS` | 2500 | Time on the letter pattern |
| `CHECKER_STEP_MS` | 200 | Time per checkerboard phase |
| `ANIMATION_STEP_MS` | 40 | Frame interval for balls, stars, bars, and hearts |
| `RAIN_STEP_MS` | 50 | Rain frame interval |
| `LIFE_STEP_MS` | 100 | Time per Life generation |
| `SCROLL_STEP_MS` | 40 | Time per one-pixel scroll step |
| `PONG_STEP_MS` | 40 | Pong physics and drawing interval |
| `CLOCK_STEP_MS` | 250 | Live-clock redraw interval; time comes from NTP/system clock |
| `CLOCK_HOLD_MS` | 10000 | Clock display time between demos |
| `WIFI_CONNECT_TIMEOUT_MS` | 10000 | Saved connection attempt before setup hotspot |
| `BOOT_CARD_MS` | 2000 | Time per startup card |

Scene frame counts are in the `scenes` table in [src/main.cpp](../src/main.cpp). Each render draws into the working buffer and calls `show()`. A demo's duration is its frame count multiplied by its interval; scrolling derives its frame count from the message width. If the application is delayed, it advances one frame rather than replaying a backlog. Clock visits use elapsed milliseconds and preserve their remaining time across pauses. The clock redraw interval changes animation cadence only; it does not change the speed of time.

### Orientation and scan settings

| Setting | Value |
|:--|:--|
| Geometry | 64×16; 1/16 scan |
| Column stream | `REVERSE_SHIFT_COLUMNS = true` |
| A/B/C/D source bits | `ROW_ADDRESS_Y_BITS = {0, 1, 2, 3}` |
| Row-address inversion | `ROW_ADDRESS_XOR = 0x0F` |
| Data / output-enable polarity | `DATA_ACTIVE_LOW = true`, `OE_ACTIVE_LOW = true` |
| Row period / signal phase / guard | 500 µs / 1 µs / 50 µs |

These values match the working panel orientation. Drawing coordinates remain **(0,0) at top left**. Changing geometry alone does not make the driver a generic multi-panel cascade driver.

## Draw your own content

`Hub08Panel` derives from `Adafruit_GFX`. Colour **0 is OFF** and a nonzero colour is ON. Draw text or graphics, then call **`show()`**. For a permanent label, replace the demo application in `src/main.cpp` with:

```cpp
#include <Arduino.h>
#include "Hub08Panel.h"

Hub08Panel panel;

void setup() {
  Serial.begin(115200);
  panel.begin();
  panel.setBrightness(DEFAULT_BRIGHTNESS);
  panel.setTextWrap(false);
  panel.setTextSize(1);
  panel.setTextColor(1);
  panel.fillScreen(0);
  panel.setCursor(1, 0);
  panel.print("READY");
  panel.setCursor(1, 8);
  panel.print("ESP32");
  panel.show();
}

void loop() {
  delay(10);  // Background scanning keeps the label visible.
}
```

Use this instead of the original `setup()`/`loop()`, rather than adding a second pair. To draw a border, line, and circle after initialisation:

```cpp
panel.fillScreen(0);
panel.drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, 1);
panel.drawLine(0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 1);
panel.drawCircle(32, 8, 5, 1);
panel.show();
```

Size-1 text fits two rows of small characters. Size-2 text gives a single 14-pixel-tall line, as used by the scroller. `setTextWrap(false)` lets text move through the screen edges. GFX clips drawing to the framebuffer and honours `setRotation()`.

### Driver API

| Method | Use |
|:--|:--|
| `begin()` | Initialise pins and start background refresh; call once |
| `drawPixel(x, y, colour)` | Set a pixel, with clipping and GFX rotation |
| `fillScreen(colour)` | Clear or fill the drawing buffer |
| `show()` | Queue the drawing buffer for the next complete scan |
| `setBrightness(level)` | Set requested row on-time; zero blanks output |
| `rowAddressFor(y)` | Return the mapped hardware address for a logical row |
| `refreshStats()` | Snapshot cumulative rows/deadlines and maximum wake/scan times |

The working, pending and active framebuffers each occupy **128 bytes**. `show()` copies to the pending buffer; refresh applies it only before row zero, so one complete scan uses one picture. Brightness also changes at that boundary.

Hardware timer **TG0/T0** sets the 500 µs row cadence and blanks output. A dedicated high-priority task on core 1 shifts the 64 bits, sets the address and latches. **TG0/T1** ends the brightness pulse in a short interrupt; the row interrupt supplies a second blanking deadline. Late rows remain dark and missed ticks are skipped. These two timers are reserved for the panel. Their IRAM handlers use shared interrupts at an available level 1–3, avoiding the two exclusive level-3 allocations that caused a blank display during startup. [ESP-IDF interrupt allocation](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/system/intr_alloc.html) Interrupts do not shift data or print logs. This removes row work from the shared `esp_timer` task that Wi-Fi activity can delay. The pin map, polarity, 1 µs signal phases and nominal 125 Hz refresh remain the same. [ESP-IDF hardware timer reference](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/timer.html), [ESP timer dispatch limitations](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/system/esp_timer.html)

This addresses possible timing jitter and frame tearing. On 25 October 2026 the user confirmed no visible flicker on the reference panel; two held-letter readings showed increasing rows and unchanged missed/late counters. See the [recorded result](ESP32_TEST_CONFIG.md#recorded-reference-panel-result-25-october-2026) for the measurements and scope. Flash writes can still pause the scan task briefly. See the [bench flicker check](ESP32_TEST_CONFIG.md#check-display-flicker); camera banding is a separate exposure effect.

## Photographing the display

Send `t`, or pause an animation with `p`. A camera can record bands because its exposure interacts with row scanning. Try a supported camera at **1/25 second and low ISO**, then adjust exposure as needed. At the nominal refresh rate this spans five complete scans. [Sony guidance on LED banding](https://www.sony.com.au/electronics/support/articles/00122281)

## Debug and formatting

Logs use the shared runtime levels: 0 Off, 1 Error `[ERR ]`, 2 Warn `[WARN]`, 3 Info `[INFO]`, and 4 Verbose `[VERB]`. `DEBUG_LEVEL` in PlatformIO sets the boot level, and serial `d` cycles it. INFO prints startup settings and scene/network transitions; frequent details use VERBOSE. Wi-Fi passwords are not logged.

C++ follows two-space indentation, attached braces and a 100-column limit in [.clang-format](../.clang-format). See the [test setup](ESP32_TEST_CONFIG.md#boot-status-and-debug-conventions) for formatting and host-check commands.

## Reference material

Open [index.html](../index.html) for the HTML edition. The [photo gallery](PHOTO_LOG.md) provides connector and component close-ups; [manifest.json](../images/manifest.json) records the original attachment checksums. Rebuild HTML and the share ZIP using the instructions in [README.md](../README.md).

Earlier diagnostic commands and electrical measurements are retained in the [archived bring-up report](BRING_UP_REPORT.md). That report describes the older test firmware, not the current showcase controls.
