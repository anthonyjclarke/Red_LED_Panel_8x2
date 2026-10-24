# CLAUDE.md — RGB_Panel_8x2

## Hardware
- Generic ESP32 DevKit (`esp32dev`), not a CYD. The CYD doesn't have 8 free output pins.
- Panel: P3.75 64×16 single-colour, 8×2 grid of 8×8 modules, **HUB08** interface, 1/16 scan.
  - 8 × 74HC595 in one chain for the columns. Bottom-row modules share column lines with the top row.
  - 2 × 74HC138 + 8 × MW4953 P-FETs drive the rows. A–D select 1 of 16 rows.
  - Only `R1` carries data. `R2`, `G1` and `G2` are unused because the second-colour 595s are not fitted.
- The ESP32 must drive the **INPUT200** header, not OUTPUT201.

## Never do
- Never use MD_MAX72XX/MD_Parola: there is no MAX7219 on this panel.
- Never use the HUB75 DMA library: the interface is HUB08, single colour, and its polarity differs.
- Never use `digitalWrite()` in the refresh path. Use `REG_WRITE(GPIO_OUT_W1TS/W1TC_REG)`, so all panel pins must be GPIO < 32 (enforced by `static_assert`).
- Never stop refreshing. Without continuous multiplexing, one row sits lit at full current.

## Architecture
- `Hub08Panel` subclasses `Adafruit_GFX`. Drawing goes to `drawBuf_`, and `show()` copies it to `showBuf_` under a `portMUX`.
- Refresh runs from a periodic `esp_timer` (esp_timer task on core 0), one row per `ROW_PERIOD_US`. Brightness comes from a second one-shot `esp_timer` that blanks OE early. Both APIs are the same in Arduino core 2.x and 3.x.
- Pixel bit = 1 means on in the buffers. Polarity is applied only at shift-out time, via `DATA_ACTIVE_LOW` and `OE_ACTIVE_LOW` in `config.h`.

## Unverified on hardware (confirm with the boot test)
- Data polarity (assumed active-low), OE polarity (assumed active-low), and the column bit order within each 595.
- Whether 3.3 V logic into the 5 V 74HC245 is reliable. If it isn't, add a 74HCT245 level shifter.
