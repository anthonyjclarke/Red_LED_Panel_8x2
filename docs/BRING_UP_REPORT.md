# Archived bring-up report

> **Historical reference:** this report describes the diagnostic firmware used before the showcase. Its eleven tests and `e`/`l`/pre-scroll commands are retired. For current setup and controls, use the [panel guide](PANEL_GUIDE.md).

**Project:** Red LED Panel 8×2 · **Recorded outcome:** 24 October 2026 · **Controller:** ESP32 DevKit · **Display:** single-colour red P3.75 HUB08, 64×16 pixels.

## Resolution

**The original panel was faulty.** After the owner replaced it with another panel described as exactly the same, the display worked with the existing setup. This was the decisive functional comparison.

The investigation also found **crossed GPIO 26/27 row-address wiring**, which was corrected separately. A software B/C exchange introduced during diagnosis was removed after that correction. Adding an **EzSBC LS1** removed the observed flickering, but it did not remove the stuck section on the original panel.

The exact defective trace, latch connection, shift register, buffer, or output stage inside the original panel was **not established**. A measured `OL` between U105 and U107 latch pins remains an observation, not a proven root cause. The proposed comparison measurements on other adjacent chips were not reported before the successful panel replacement.

## The starting point

The initial application already provided all-on, all-off, row-walk, and column-walk tests followed by scrolling text. Individual row/column logs used a verbose debug level, making it harder to compare the panel with serial output at the default settings.

The application was expanded into an eleven-test sequence with explicit descriptions and per-step serial output at **115200 baud**. Controls were added to restart, pause, advance, and hold diagnostic states without stopping row refresh. Subsequent additions made a static letter pattern, the pre-scroll black state, runtime OE polarity, and LT-pulse width directly comparable.

The rightmost **three module columns by two module rows** stayed lit during tests. This is a 24×16-pixel section, or **384 LEDs**; the responding left section covered five module columns. The physical left/right distinction later mattered when inspecting the board from behind.

## Investigation chronology

### 1. Localised constant illumination and flicker

The owner reported that tests partially worked while the right 3×2 modules remained on. Later clarification established that the visible flickering was confined to that section rather than the whole panel.

The driver's column transmission covers all 64 bits on every row scan, including during a black frame. A different column orientation can rearrange content, but cannot turn a uniform black buffer into a solid lit block. A simple increase in shift-chain length was therefore not adopted as a fix: an intact continuously clocked chain would eventually fill with uniform OFF data.

### 2. Separate black data from output blanking

The held serial commands made the distinction observable:

| Command | Requested state | Original-panel observation |
|:--|:--|:--|
| `f` | Black data, ordinary output timing | Right 3×2 stayed on, steadily |
| `o` | All pixels ON | Entire panel on, no flicker in the held state |
| `b` | All-one data with brightness 0 / OE disabled | Entire panel dark |

An early description that the section cleared “after Test 11” was ambiguous. Later the owner identified the exact line: **`brightness 0/255 (dark)`** in Test 11. That phase suppresses output through EN/OE. It does not show that the right section accepted black column data.

### 3. Explicit signal and refresh timing

The driver gained 1 µs data setup and clock high/low timing, plus latch settling intervals. Refresh was adjusted to blank before shifting, cancel old OE-off events, cap each light pulse to the remaining row time with a 50 µs margin, and skip overdue row callbacks rather than process a backlog in a burst.

These changes addressed identifiable timing weaknesses in the software. They were not evidence of a repaired panel, and they did not eliminate the original section fault.

### 4. Compare both black-frame entry paths

The pre-scroll interval and `f` both request a zero-filled **128-byte** framebuffer at brightness **90/255**. The `s` command was added to hold the exact pre-scroll state, while `BLACK CHECK` logged its settings.

The owner confirmed that **`f` and `s` behaved the same**, with the right section remaining on. This resolved the apparent difference between black-data states. The pre-scroll black interval was also lengthened to three seconds for easy observation.

A representative command report was:

```text
TEST 2/11: ALL OFF - every LED should be dark
BLACK CHECK: frame=128 zero bytes, brightness=90/255,
             OE_ACTIVE_LOW=true, DATA_ACTIVE_LOW=true
R1 GPIO 23 should stay HIGH; each row still gets 64 SK clocks and one LT pulse.
HELD: pattern continuously shifted and latched at normal brightness
```

Serial labels describe intended electrical/display states; there is no automatic readback or wiring-fault detector in the firmware.

### 5. Add a level shifter

The initial ESP32-to-panel connection was direct. After a level shifter was fitted, the owner reported **no flickering**, while the right section still failed to clear with black data.

The specific device was subsequently identified as **EzSBC.COM LS1**. Earlier references to a possible HCT/AHCT buffer were therefore corrected in the documentation. LS1 uses a different design from the 74AHCT245 alternative described in the usage guide.

The flicker improvement was a real observation. It did not establish the cause of the remaining fixed section fault. The later successful identical-panel replacement is stronger evidence for that remaining fault being in the original panel.

### 6. Static letters reveal a second, independent mapping issue

The `t` mode displays one character per module:

```text
A B C D E F G H
I J K L M N O P
```

The first photo showed letter-like content on the left, but in reversed order with displaced row strokes. The right section was fully lit. Comparing the glyph patterns with the installed GFX font suggested reversed columns, inverted row orientation, and an exchange of row bits B/C.

![Original static-letter result before mapping correction](../images/01-static-before-mapping.jpg)

*First static-letter photograph. The responding section has incorrect mapping; the right section remains solidly on.*

Configurable output mapping was introduced. A follow-up photo showed **ABCDE above IJKLM** on the responding left five columns, while the right three columns remained on.

![Static letters with corrected responding-section placement but the right section still lit](../images/02-static-after-mapping.jpg)

*Second static-letter photograph, taken before the wiring-error correction was completed. It shows successful placement on the left, not a working replacement panel.*

### 7. Find and correct crossed GPIO 26/27

The owner asked whether software was compensating for incorrect wiring. It could: a B/C permutation in code can hide crossed B/C wires.

The connections were traced and **GPIO 26 and 27 were found to be wrong**. The intended wiring is **26 → LB** and **27 → LC** through the level shifter. The wiring was corrected, and `ROW_ADDRESS_Y_BITS` was restored to **`{0, 1, 2, 3}`**. Column reversal and full row-address inversion were retained as viewing-orientation settings.

This was a separate fault from the right section remaining lit. Reordering rows or columns leaves a uniform zero buffer unchanged, so a mapping correction could not resolve that symptom.

### 8. Verify accessible voltages and header orientation

R1 voltage was measured while `f` and `o` held their states indefinitely. The input levels matched the requested active-low column-data convention. Supply values and later output-header readings were also recorded in the table below.

INPUT200 has no signal labels in the supplied image. OUTPUT201 does. The initial assumption of matching connector layout was treated as a hypothesis and the ground contacts were verified with all power disconnected. The owner confirmed continuity from the panel GND terminal to the proposed INPUT200 left contacts in rows 1, 2, 3, 7, and 8.

That confirms the ground pattern/orientation check, not a complete independent schematic trace of every connector signal. The subsequent replacement-panel result confirms the corrected setup was practically usable.

### 9. Test a longer latch pulse

The `l` comparison changes **only LT high time**, between the normal **1 µs** and a **20 µs** diagnostic setting, while holding the current image. It leaves the requested brightness, column data, and row mapping unchanged; row-time limits still apply.

The owner reported **no change**. Therefore a wider LT pulse by itself did not solve the original fault. This does not establish that every local latch/clock connection was intact, and no oscilloscope captures were supplied.

### 10. Inspect the physical boundary

The full rear view established that front-right corresponds to rear-left. The suspect area is around **U101/U103/U105**, with the working/stuck transition near **U105/U107**.

A close-up identified the SM74HC595D packages and the pin-1 orientation. Pin 12 is the storage/latch clock. With all power disconnected, resistance between **U105 pin 12 and U107 pin 12** was reported as **`OL`**.

![Close-up of U105 and U107 at the working/stuck boundary](../images/06-u105-u107-closeup.jpg)

*U105 at left and U107 at right. The image made package orientation readable for the resistance check. It does not establish an electrical break by itself.*

Comparison checks were proposed between U103/U105 and U107/U109 latch pins. No results for those comparisons were recorded. No jumper repair, trace repair, chip replacement, or reflow outcome was reported.

### 11. Replace the panel

The owner obtained another panel described as exactly the same and replaced the original. The replacement **worked**. The reported change was the panel replacement; the fault investigation did not proceed to a component-level repair of the original.

The correct final diagnosis is therefore **a faulty original panel**, alongside a corrected external B/C wiring error and observed flicker improvement after adding level shifting. It is not a confirmed diagnosis of “U105 failed” or “the U105–U107 latch trace is broken.”

## Measurement record

All values below are owner-reported readings from the original setup. They were taken at different points in the investigation; they are not a simultaneous power-distribution survey. The `f - 5.2v` / `o -0.3v` notation was interpreted in context as labels followed by approximately 5.2 V and 0.3 V.

| Measurement | Held state / condition | Reported result | What it established |
|:--|:--|:--|:--|
| R1 at matrix input, relative to panel GND | `f` | 4.7 V | HIGH DC data level reached the input measurement point |
| R1 at matrix input, relative to panel GND | `o` | 0 V | LOW DC data level reached the input measurement point |
| LS1 VLo | Powered | 3.31 V | Low-side supply reading |
| LS1 VHi | Powered | 4.5 V | High-side supply reading; further supply-terminal comparisons were suggested but not reported |
| R1 at OUTPUT201, relative to panel GND | `f` | Approximately 5.2 V | HIGH appeared at the output contact |
| R1 at OUTPUT201, relative to panel GND | `o` | Approximately 0.3 V | LOW appeared at the output contact |
| INPUT200 ground pattern | All power disconnected | Confirmed | Proposed left-hand ground contacts in rows 1/2/3/7/8 connected to panel GND |
| U105 pin 12 ↔ U107 pin 12 | All power disconnected, resistance test | `OL` | No measurable DC connection within the meter's selected range in that test |
| LT pulse comparison | `f`, then `l`: 1 µs versus 20 µs | No change | Widening LT alone did not resolve the symptom |
| Identical-panel replacement | Existing setup | Worked | Original panel was responsible for the remaining functional fault |

DC R1 readings cannot prove pulse widths, edge quality, every intermediate serial stage, or correct latched parallel outputs. Likewise, one `OL` result can reflect an open route, separate buffered paths, intervening components, contact/range problems, or an incorrect assumption about the net. Those alternatives were not fully distinguished before replacement.

## Diagnostic firmware at the time of resolution

The following records the diagnostic build at resolution. The current showcase replaces these tests and runtime comparison modes:

| Item | Final recorded state |
|:--|:--|
| GPIO B/C wiring | 26 → LB; 27 → LC, corrected physically |
| Software B/C mapping | Normal `{0, 1, 2, 3}` |
| Column / row orientation | Reversed column stream; row XOR `0x0F` |
| Data / OE polarity | Active LOW / active LOW |
| Row timing | 500 µs slots; nominal 125 Hz frames |
| Brightness on reset | 90/255 |
| LT on reset | Normal 1 µs; diagnostic 20 µs selectable with `l` |
| Tests and serial controls | Retained for future panels |
| Firmware revision banner | `latch-compare-1` |

Runtime latch and OE toggles are not persisted through reset. The exact runtime latch setting at the moment of replacement was not separately recorded; normal 1 µs remains the reset default.

## Lessons for the next panel

1. **Compare a known-good panel early** when a sharply bounded section remains faulty after basic input checks. The replacement was the decisive experiment here.
2. **Verify wires before changing bit mappings.** The temporary B/C compensation produced readable letters while hiding an external wiring error.
3. **Distinguish pixel OFF from OE blanking.** A section going dark only at brightness 0 does not show that it accepted black data.
4. **Use a held, recognisable pattern.** The sixteen static letters exposed mapping errors that moving text made difficult to interpret.
5. **Treat electrical measurements as bounded evidence.** Correct DC levels do not replace pulse measurements; one open-circuit reading does not identify a failed chip.
6. **Keep observations separate from hypotheses.** The record supports board replacement as the resolution without inventing a component-level cause.

## Evidence and further reading

The [photo log](PHOTO_LOG.md) contains all six supplied images in order. No working-replacement photograph was supplied, so the visual archive is explicitly a record of the original faulty panel. Attachment files are copied without re-encoding at the resolution retained in the conversation.

For current wiring, command instructions, and application examples, use the [panel guide](PANEL_GUIDE.md). Chip pin functions can be checked in the [manufacturer-authored Sunmoon SM74HC595 datasheet](https://taoic.oss-cn-hangzhou.aliyuncs.com/6466/product/HZ6749_1623831323000.pdf); the investigation's measurements and outcome come from the owner, not from that datasheet.
