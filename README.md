# RGB_Panel_8x2

ESP32 driver and bring-up test for a P3.75 64×16 single-colour LED sign panel (16 × 8×8 modules in an 8 × 2 grid) with a HUB08 input header.

---

## What this panel actually is

It looks like a MAX7219 array, but there are no MAX7219 chips on it. It is a "dumb" multiplexed HUB08 panel, so neither `MD_MAX72XX` nor the HUB75 DMA library will drive it. The ESP32 must refresh it continuously, one row at a time.

| Part                 | Chip on board              | Role                              |
|:---------------------|:---------------------------|:----------------------------------|
| U101–U115 (odd)      | 8 × 74HC595                | 64 column bits, one shift chain   |
| U100–U114 (even)     | empty footprints           | second colour – not fitted        |
| U205, U204           | 2 × 74HC138                | decode A–D into 16 row selects    |
| U208–U221            | 8 × MW4953 (dual P-FET)    | 16 high-side row drivers          |
| U200, U201           | 2 × 74HC245                | input / output signal buffers     |
| INPUT200, OUTPUT201  | 2 × 8 HUB08 headers        | data in / cascade out             |

The `G1–G8` pins of each LED module are unsoldered, so only one colour is fitted and only `R1` carries data. With 4 address lines (A–D) and 16 row drivers it is 1/16 scan: each refresh step shifts 64 bits and lights one of 16 rows.

---

## Wiring

Connect to **INPUT200** (left end when viewed from the back, next to U201). The arrow on the PCB points from input to output. Connecting to OUTPUT201 is the most common reason these panels stay dark.

The supplied rear photos show signal labels at **OUTPUT201**, while INPUT200 is unmarked. The table below is the intended input wiring, based on the assumed matching connector layout; the photos alone do not verify that assumption. With all power disconnected (including ESP32 USB and the level shifter), check INPUT200 against the panel's GND screw terminal before treating the output labels as an input pinout.

With rear PCB text upright, OUTPUT201's **visible labels** run top to bottom as follows. This is a transcription of the labelled output, not a verified INPUT200 pinout:

```text
          LEFT   RIGHT
top       GND    LA
          GND    LB
          GND    LC
          EN     LD
          R1     G1
          R2     G2
          GND    LT
bottom    GND    SK
```

For the matching-layout hypothesis, INPUT200's left-hand contacts in rows 1, 2, 3, 7, and 8 should have a steady near-zero resistance to panel GND. A matching ground pattern supports the orientation but does not establish all signal assignments. Signal routes can pass through onboard buffers, so corresponding input/output signal pins need not have direct continuity.

| HUB08 pin  | Signal             | ESP32 GPIO |
|:-----------|:-------------------|:-----------|
| `LA`       | row address A      | 25         |
| `LB`       | row address B      | 26         |
| `LC`       | row address C      | 27         |
| `LD`       | row address D      | 13         |
| `EN`       | output enable      | 22         |
| `R1`       | column data        | 23         |
| `LT`       | latch              | 4          |
| `SK`       | shift clock        | 18         |
| `GND` (any)| ground             | GND        |
| `R2 G1 G2` | unused             | –          |

Power the panel from its own 5 V supply (2–3 A) on the `VCC` / `GND` screw terminal. Do not run it from the ESP32's 5 V pin. Join the supply ground to the ESP32 ground.

### Logic levels

The panel's 74HC245 buffer uses 5 V logic. Direct ESP32 3.3 V signals are not a robust interface: [TI's HC245 specifications](https://www.ti.com/lit/ds/symlink/sn74hc245.pdf) specify a HIGH threshold of 3.15 V at a 4.5 V supply and 4.2 V at 6 V; the earlier 3.15 V figure was not a specification at 5 V. An external **74AHCT245 powered at 5 V** accepts 3.3 V inputs and translates them to the panel's logic supply. Its specified HIGH input threshold is 2 V. See the [AHCT245 datasheet](https://www.ti.com/lit/ds/symlink/sn74ahct245.pdf).

### External 74AHCT245 wiring

For a standard 20-pin PDIP/SOIC package (verify the fitted package), wire the ESP32 to the A side and INPUT200 to the B side:

| ESP32 GPIO | Buffer input | Buffer output | INPUT200 signal |
|:-----------|:-------------|:--------------|:----------------|
| 25 | A1, pin 2 | B1, pin 18 | LA |
| 26 | A2, pin 3 | B2, pin 17 | LB |
| 27 | A3, pin 4 | B3, pin 16 | LC |
| 13 | A4, pin 5 | B4, pin 15 | LD |
| 22 | A5, pin 6 | B5, pin 14 | EN |
| 23 | A6, pin 7 | B6, pin 13 | R1 |
| 4  | A7, pin 8 | B7, pin 12 | LT |
| 18 | A8, pin 9 | B8, pin 11 | SK |

Connect pin 20 (VCC) and pin 1 (DIR) to the panel's regulated 5 V supply; pin 10 (GND) and pin 19 (buffer /OE) to common ground. Fit a 100 nF capacitor close to pins 20 and 10. This fixes the direction A to B; GPIO 22 still controls the **panel's EN**, independently of the buffer's /OE. Keep leads short and join panel, buffer, supply, and ESP32 grounds. Connect 5 V buffer outputs only toward the panel; the ESP32 connects to the A inputs. Retest `f`, `o`, and `r` with the existing firmware.

### Panel coordinate mapping

The static-letter photo showed reversed character order on the responding section, the logical lower bank appearing at the top, and displaced horizontal strokes. GPIO 26/27 were subsequently confirmed to be wired incorrectly. After correcting GPIO 26 -> LB and GPIO 27 -> LC, the software B/C exchange was removed. The defaults in `include/config.h` now retain the viewing-orientation correction with normal address-bit order:

- `REVERSE_SHIFT_COLUMNS = true`: reverse the entire 64-bit stream, including both byte order and bit order.
- `ROW_ADDRESS_Y_BITS = {0, 1, 2, 3}`: address outputs A/B/C/D use logical y bits 0/1/2/3, matching the corrected wiring.
- `ROW_ADDRESS_XOR = 0x0F`: invert all four address outputs after the permutation.

Drawing remains in normal top-left coordinates. The mapping is applied during refresh, so letters, lines, row walks, and scrolling all use the same correction. Row-walk serial output reports the resulting raw address bits; address-bit tests identify the GPIO corresponding to each logical y bit. After uploading, check that serial lists A/B/C/D from y bits `0/1/2/3`, then send `t` to confirm placement with the corrected wiring. To restore the original orientation as well, use `false`, `{0, 1, 2, 3}`, and `0x00` respectively. A mapping change alone does not explain a section that stays lit during an all-zero frame.

---

## Boot test sequence

Open the serial monitor at **115200 baud** (`pio device monitor`), then reset the ESP32 to see the complete sequence. Startup prints the GPIO mapping, polarity settings, and brightness. Test descriptions and individual steps are always printed, independently of `DEBUG_LEVEL`.

The firmware runs these 11 tests on every boot, taking about 54 seconds with the default timings:

| Test | Expected display / wiring check |
|:-----|:--------------------------------|
| **ALL ON**, 2 s | Every LED lit. If this is dark but ALL OFF lights everything, flip `DATA_ACTIVE_LOW` in `include/config.h`. |
| **ALL OFF**, 2 s | Every LED dark. Look for stuck pixels or ghosting. |
| **ROW WALK**, 300 ms/row | One row moving top to bottom. Serial prints y=0–15 and the corresponding D C B A address bits. Repeated or misordered rows suggest an address-line problem. |
| **COLUMN WALK**, 60 ms/column | One column moving left to right. Serial prints x=0–63, the 8-column block, and offset. Check R1, SK, LT, and the output mapping if columns repeat, skip, or reverse. |
| **ROW ADDRESS BITS**, 1.5 s/pattern | For logical y bits A, B, C, then D, show rows with that bit 0, then 1: alternating groups of 1, 2, 4, then 8 rows. Serial identifies the mapped address GPIO and its level for the lit groups. Helps isolate swapped or stuck address lines. |
| **ROW STRIPES**, 1.5 s/phase | Even rows, then odd rows. Check for extra lit rows. |
| **COLUMN STRIPES**, 1.5 s/phase | Even columns, then odd columns. Check adjacent columns and data stability. |
| **CHECKERBOARD**, 1.5 s/phase | Alternating pixels and their inverse. Look for smearing, ghosting, or unstable pixels. |
| **CORNERS**, 1.5 s/corner | One pixel: top left, top right, bottom right, bottom left. Serial prints the coordinates to check orientation. |
| **MODULE WALK**, 500 ms/block | One solid 8×8 block moving left to right across the top, then bottom. Serial prints each block's bounds. |
| **BRIGHTNESS**, 1.5 s/level | Whole panel at brightness 0, 30, 60, 90 with the default settings. First dark, then progressively brighter; checks EN/OE control. Maximum is `DEFAULT_BRIGHTNESS`. |

After the tests, the firmware holds a black frame at normal brightness for `SCROLL_BLANK_MS` (3 seconds), then scrolls `SCROLL_TEXT` continuously. Serial marks this interval as `PRE-SCROLL BLACK`. It sends the same black data as ALL OFF and the `f` command. Compare the display with the serial descriptions: the firmware cannot automatically detect wiring faults or declare a pass.

Send these commands from the serial monitor (with or without a line ending):

| Key | Action |
|:----|:-------|
| `r` | Restart the sequence from ALL ON. |
| `p` | Pause/resume on the current pattern or scroll position. Panel refresh continues. |
| `n` | Advance to the next test and resume. From the scroller, restart the tests. |
| `?` or `h` | Print help. |
| `f` | Hold ALL OFF data at normal brightness; continuously shift black into every column. |
| `o` | Hold ALL ON data at normal brightness. |
| `b` | Hold OE blanking (brightness 0), independently of column data. |
| `e` | Toggle EN/OE polarity and hold brightness 0; serial prints the actual HIGH/LOW GPIO level. |
| `s` | Hold the exact pre-scroll black state reached after test 11. Compare with `f`; both send identical black data at normal brightness. |
| `t` | Hold static letters across both rows: `ABCDEFGH` above `IJKLMNOP`, one letter per 8×8 module. |
| `l` | Hold the current pattern and toggle only the LT high pulse between normal 1 µs and test 20 µs. |

### Latch timing comparison

With revision `latch-compare-1`, send `f` to hold black, then lowercase **`l`** (letter L) to widen the LT pulse from 1 µs to 20 µs. Observe whether the right section clears. Send `l` again to restore 1 µs, or use `t` to compare static letters at either setting. The test keeps the pixel data, column clocks, row mapping, and requested brightness unchanged. The driver still bounds the light pulse to the remaining row period. The latch setting persists across other serial commands until toggled back or reset.

R1 measured at OUTPUT201 was reported near 5.2 V with `f` and near 0.3 V with `o`. These are the expected static levels, but do not prove clock/latch pulse quality. The level shifter was subsequently identified as an **EzSBC LS1**, which the [manufacturer describes as a resistor-pull-up translator for I²C/low-speed signals](https://news.ezsbc.com/new-level-shifter-buffer/); it is a different design from the AHCT buffer documented above. If widening LT changes the symptom, investigate pulse timing and edge quality. If it does not, that does not exclude a local latch/clock distribution fault or slow clock edges. This comparison alone cannot establish that the hardware is repaired.

### Static letter test

Send **`t`** (or `T`) at any time to stop the sequence and show:

```text
A B C D E F G H
I J K L M N O P
```

Each letter uses the small built-in font and sits inside its own 8×8 module. The rightmost three module columns should show **F G H** above **N O P**. The pattern stays still indefinitely while panel refresh continues, so you can inspect missing, repeated, reversed, or misplaced characters. `p` does not start scrolling in this mode. Send `r` or `n` to restart the wiring tests, or another diagnostic command to change the held pattern. The regular scrolling font remains unchanged.

Test timings are configurable in `include/config.h`. Pause a pattern to inspect wiring for longer; restart to repeat individual steps. Brightness returns to `DEFAULT_BRIGHTNESS` when changing tests or entering the scroller.

In VS Code, open PlatformIO's **Serial Monitor** from its bottom toolbar, click inside the terminal, and type one command letter at a time (no commas or Enter required). The monitor runs at 115200 baud. Local echo may be off, so typed letters can be invisible; the firmware prints a response. Close the monitor before uploading if it holds the serial port open.

If nothing lights at any step, check that you are on INPUT200 and that the 5 V supply is connected, then try `OE_ACTIVE_LOW = false`.

### Brightness changes but the panel never goes completely dark

Changing brightness alone does not confirm that EN/OE can disable the panel. Upload the current firmware and send `b`, then `e`. These hold GPIO 22 at opposite constant levels while data shifting and row scanning continue. Serial prints the level and `OE_ACTIVE_LOW` value for each test. Compare the whole panel, including the stuck section.

- If one level makes **every module completely dark**, keep that polarity, then send `r` to rerun the tests. Copy the printed `OE_ACTIVE_LOW` value into `include/config.h` to retain it after a reboot. Serial polarity changes otherwise last only until reset.
- If neither level completely blanks the panel, check GPIO 22 to the actual EN input on INPUT200, common ground, and the panel's EN buffer/distribution. Measure both levels at the panel input; a brightness change is not enough to establish that the pin reaches a valid disable level. If some modules blank and others only fade, inspect the enable distribution to the affected section too.

### Some modules stay lit while others run the tests

Send `f`, then `b`, and compare the affected modules. Use `r` to restart afterwards.

- The rear PCB photo places INPUT200 on the left and OUTPUT201 on the right. The stuck three columns seen at the front-right therefore correspond to the rear-left U101/U103/U105 area, with the working/stuck boundary near U105/U107. This identifies the physical area to trace; it does not establish serial-chain direction or prove a failed chip. Rows of unused colour pads and empty alternate-colour IC footprints are not by themselves evidence of damage.
- After fitting a level shifter (later identified as the EzSBC LS1), flickering was reported resolved and `b` still blanked the whole panel, while the right 3×2 section remained lit except around the end of test 11. To isolate the timing of that observation, firmware revision `black-compare-1` adds `s`: it holds the exact pre-scroll state indefinitely. Compare `f` and `s`, waiting at least one second after each. Both print `BLACK CHECK` with identical settings and send the same zero-filled frame. Their labels describe requested output, not measured electrical levels.
- With either `f` or `s` held and the current `DATA_ACTIVE_LOW=true`, R1 should be constantly HIGH at the panel input; `o` should make it LOW. Check the buffer's R1 output/panel R1 input first. With standard 16-pin 74HC595s, compare the upstream QH' pin 9 with the next SER pin 14 at the boundary of the stuck section: during held black data, an intact clocked chain should settle HIGH there too. If R1 enters correctly but stops propagating, check that section's serial connection and SK/LT distribution. These are measurements needed to isolate the fault, not confirmation that a particular chip has failed.
- Reported results with direct ESP32 wiring: `f` leaves the right 3×2 modules steadily lit, `o` lights the whole panel steadily, and `b` blanks everything. These establish full OE blanking, but not correct column-data transfer. The changing reliability of left/right sections makes signal integrity worth resolving before changing geometry or blaming a particular chip. Add the external buffer above, then repeat the same tests. If `f` still fails on a fixed section, trace the shift chain at that boundary.
- If `f` leaves a section lit but `b` blanks it, EN/OE reaches that section. Suspect the column data chain or local SK/LT distribution rather than a global polarity setting. This is a diagnostic inference, not proof of a particular failed component.
- Flickering confined to a fixed section, with the rest stable, also points toward that section's data/clock/latch distribution or local power/ground. It does not by itself identify a failed chip. If the pre-scroll black interval clears it, send `f` to hold the same data pattern for inspection.
- The driver now uses `SIGNAL_DELAY_US = 1` for data setup, clock high/low, and latch settling. This deliberately exceeds the [74HC595 minimum setup and pulse times](https://www.ti.com/lit/ds/symlink/sn74hc595.pdf) to test whether very short pulses were contributing. It cannot repair a broken trace or inadequate logic voltage.
- If a fixed section is still stuck with this timing, inspect the boundary between responding and stuck modules. Trace the actual serial direction: check the upstream 74HC595 QH' (pin 9) to the next SER (pin 14), then SK/SRCLK (pin 11) and LT/RCLK (pin 12) at the first nonresponding chip. These pin numbers apply to the standard 16-pin 74HC595; verify the fitted part/package. With power removed, inspect solder joints and continuity. With power applied, a scope or logic analyser can show whether data, clock, and latch reach that chip.

ALL OFF is sent every refresh, so a longer-than-expected intact chain would eventually fill with black too. A permanently lit block alone is not evidence that more shift clocks are needed. Record whether the right-hand section is viewed from the LED face or the PCB when locating the boundary.

---

## Using it

`Hub08Panel` is an `Adafruit_GFX` subclass. Draw with any GFX call (colour `0` = off, non-zero = on), then call `show()`:

```cpp
panel.fillScreen(0);
panel.setCursor(0, 4);
panel.print("12:34");
panel.show();
```

`panel.setBrightness(0–255)` changes the on-time of each row.

Refresh blanks EN before shifting and latching, cancels the preceding row's pending off timer, and caps each light pulse to the remaining row period minus `ROW_GUARD_US`. Level 255 therefore means the available light window after shifting. Overdue row callbacks are skipped instead of replayed in a burst. This reduces sources of ghosting and pulse overlap; software timer dispatch can still have jitter, as described in the [ESP-IDF timer documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/system/esp_timer.html).
