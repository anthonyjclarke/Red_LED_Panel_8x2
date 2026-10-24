# Changelog

## [Unreleased]

### Fixed
- Configurable column-stream reversal and row-address permutation/inversion for panel orientation. Address-bit order is normal after correcting the crossed GPIO 26/27 wiring; software B/C compensation has been removed.
- Blank before every row shift, cancel stale OE-off events, bound light pulses to the remaining row period, and skip overdue row callbacks.

### Added
- Held LT-pulse comparison (`l`): toggle normal 1 µs versus 20 µs latch high time while retaining the current frame and corrected wiring map.
- Static letter mode (`t`): `ABCDEFGH` above `IJKLMNOP`, one letter per 8×8 module, held without scrolling for placement checks.
- Held pre-scroll black comparison (`s` versus `f`) with a firmware revision marker and expected R1-level diagnostics for the remaining partial-panel fault.
- External 74AHCT245 wiring for all eight signals and documented direct-wiring test observations; corrected the HC245 input-threshold description.
- Three-second pre-scroll black interval and VS Code serial command instructions to help reproduce partial-panel faults.
- Runtime EN/OE polarity comparison (`b`, then `e`) with explicit HIGH/LOW reporting to distinguish full blanking from brightness changes.
- Explicit 1 us shift-clock/data/latch timing and held serial diagnostics (`f` black data, `o` all on, `b` OE blank) to isolate stuck module sections.
- Serial-guided 11-test wiring sequence with per-row/column progress, address-bit patterns, stripes, checkerboard, corner pixels, module walk, and OE brightness checks.
- Serial GPIO/polarity summary and restart, pause/resume, next-test, and help controls at 115200 baud.
- Configurable pattern timings and wiring-test instructions in the README.

## [0.1.0] 23-10-2026

### Added
- HUB08 64×16 single-colour panel driver (`Hub08Panel`, an Adafruit_GFX subclass) with esp_timer row refresh and OE-based brightness.
- Boot test sequence: all on, all off, row walk, column walk.
- Scrolling text demo.
- `config.h` with the pin map, signal polarity flags and timing constants.
- README covering the panel analysis and wiring.
