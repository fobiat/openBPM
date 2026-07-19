# Hardware guide

Everything you need to build an openBPM. Total cost is roughly **£20–30 / $25–35**
depending on what's already in your parts bin.

## Bill of materials

| # | Part | Qty | Notes |
|---|------|-----|-------|
| 1 | **ideaspark ESP32-WROOM-32 with integrated display** | 1 | Two variants work — see below |
| 2 | **12 mm momentary push button**, panel mount | 3 | Must be *momentary*, not latching |
| 3 | **Hammond 1591XXLBK** enclosure | 1 | 87 × 57 × 39 mm ABS |
| 4 | Hook-up wire, ~24 AWG | ~30 cm | Stranded is easier in a small box |
| 5 | 3.7 V LiPo with 2-pin 1.25 mm JST | 1 | *Optional* — for cordless use |
| 6 | 100 nF ceramic capacitor | 3 | *Optional* — extra button debounce |

### Choosing the board

| Variant | Display | Build env | Verdict |
|---------|---------|-----------|---------|
| 1.14" TFT | ST7789, 240×135, colour | `tft` | **Recommended.** Colour carries the match state — green/amber/red reads across a dark booth far faster than digits |
| 0.96" OLED | SSD1306, 128×64, mono | `oled` | Perfectly usable, but no colour and a smaller readout |

Both are ESP32-WROOM-32 boards with the display already attached, a CH340 USB-serial
chip, and a LiPo connector on the back. No display wiring is needed.

### Buttons

Get **momentary** push buttons — the springy kind that return when released. A
latching toggle switch will stay on and is useless for tapping a beat.

Standard 4-pin tactile buttons have their pins bridged in diagonal pairs. If a button
seems permanently pressed, rotate it 90°; using one pin from each diagonal is always
correct.

**Make the TAP button physically distinct** — larger, or a different coloured cap. You
need to find it by feel in a dark booth without looking down. This matters more in
practice than it sounds.

## Wiring

The display is already connected on the board. All you add is three buttons, each
between a GPIO and **GND**:

```
        ESP32
      ┌────────┐
      │ GPIO27 ├──────[ TAP  ]───┐
      │ GPIO26 ├──────[ SWAP ]───┤
      │ GPIO25 ├──────[ MODE ]───┤
      │    GND ├──────────────────┘
      └────────┘
```

**No resistors required.** The firmware enables the ESP32's internal pull-ups, so each
pin idles HIGH and reads LOW when its button connects it to GND.

On a breadboard, run one wire from the board's GND to the ground rail, then each
button's return leg to that rail.

The optional 100 nF capacitors go across each button's two legs for hardware
debounce. The firmware already debounces 25 ms in software, so they're a nicety.

### Works with no wiring at all

The onboard **BOOT** button (GPIO0) drives all three actions by press length — short
= TAP, long = SWAP, hold = MODE. Flash the board and it works before you solder
anything. Wired buttons are an ergonomic upgrade, not a requirement.

## GPIO reference

| GPIO | Use | Free? |
|------|-----|-------|
| 0 | Onboard BOOT button (gestures) | reserved |
| 2, 4, 15, 18, 23, 32 | ST7789 display (TFT board) | ✗ on `tft` |
| 21, 22 | SSD1306 I2C (OLED board) | ✗ on `oled` |
| 25, 26, 27 | Buttons (default) | configurable |
| 6–11 | SPI flash — **never use** | ✗ |
| 12 | Boot strap, must be low at boot | avoid |
| 34, 35, 36, 39 | Input-only, **no internal pull-up** | avoid for buttons |
| 5, 13, 14, 16, 17, 19, 33 | Free | ✓ |

Button pins are set at the top of [`src/main.cpp`](../src/main.cpp).

### Identifying onboard buttons on other boards

Board revisions differ. Rather than trusting a datasheet, measure:

```bash
pio run -e pinscan -t upload
pio device monitor
```

Press each button; the scanner reports the GPIO behind it. A real button gives clean,
paired press/release lines. Pins that fire in bursts, move in step with each other, or
never settle are **floating**, not buttons.

On the ideaspark boards the result is: **GPIO0 is the only readable onboard button.**
The others are EN/RST, which reset the chip in hardware and never reach a GPIO.

## Enclosure

The **Hammond 1591XXLBK** (87 × 57 × 39 mm external, roughly 82 × 52 × 35 mm inside)
is about the size of a guitar pedal and suits a booth well.

- The board (~55 × 28 mm) leaves plenty of room
- Three 12 mm buttons in a row need ~48 mm of the 87 mm length
- 35 mm of depth easily takes a LiPo underneath the board

Suggested build:

1. Mount the board to the **lid** so the screen sits flush behind a cut window.
2. Drill the three button holes in a row below the screen.
3. Cut a slot in the **side** for the USB port — you want to charge and reflash
   without opening the box.
4. If fitting a LiPo, tape it to the base so it can't shift against the pins.

## Power

- **USB** — plug in and go
- **LiPo** — a 3.7 V cell on the 2-pin 1.25 mm JST connector on the board's back. The
  board charges it over USB. The firmware blanks the screen after 2 minutes idle to
  save power; any button press wakes it.

Turn the WiFi AP off when you're not using it — it's by far the largest power draw.
