# openBPMcount

A tap-tempo **BPM counter and beatmatch assistant** for vinyl DJing, running on an
**ideaspark ESP32-WROOM-32** with its integrated 0.96" OLED. Tap the onboard button
in time with a record and it reads out the BPM in big, booth-legible digits — then
tells you exactly how much pitch to dial in to match your other deck.

## Hardware

| Part | Detail |
|------|--------|
| Board | ideaspark ESP32-WROOM-32 (integrated OLED variant) |
| Display | 0.96" SSD1306 OLED, 128×64, I2C @ `0x3C` (SDA=GPIO21, SCL=GPIO22) |
| Button | onboard **BOOT** button, GPIO0 (active LOW) |

No wiring required — the OLED and button are on the board. Just plug in over USB.

## Controls (single button)

| Gesture | Action |
|---------|--------|
| **Short press** | Tap a beat for the **active** deck |
| **Long press** (≥0.6s) | Lock the active deck's BPM and switch to the other deck (A ⇄ B) |

## How to beatmatch with it

1. Power on — **Deck A** is active. Tap along with the record that's playing.
   The big number settles on the BPM after a few taps.
2. **Long-press** to lock Deck A and switch to **Deck B**.
3. Tap along with the record you're cueing on Deck B.
4. The bottom line shows the **pitch %** to dial into the active deck plus a
   **SPEED UP / SLOW DN / MATCHED** nudge, relative to the other deck.
5. Long-press any time to hop back to a deck and re-tap it.

```
┌──────────────────────────┐
│ DECK B          TAPS 7   │
│                          │
│        126.8             │
│ ──────────────────────── │
│ A:128.3  B:126.8         │
│ SPEED UP +1.18%          │
└──────────────────────────┘
```

The pitch figure is `((other / active) - 1) × 100` — i.e. how much to move the
active deck's pitch fader so it matches the other deck.

## How the BPM is measured

Each tap records the interval since the previous one. It keeps a rolling window of
the last 8 intervals and averages them (`BPM = 60000 / avg_interval_ms`), which is far
more stable than timing a single pair of taps. Wildly off-beat taps (a missed or
double beat) are rejected instead of corrupting the average, and a pause of more than
3 seconds starts a fresh measurement so you can move straight to the next record.

## Build & flash (PlatformIO)

With [PlatformIO](https://platformio.org/) (VS Code extension or CLI):

```bash
pio run                 # compile
pio run --target upload # flash over USB
pio device monitor      # optional serial monitor @ 115200
```

Everything is configured in [`platformio.ini`](platformio.ini); the only dependency
is [olikraus/U8g2](https://github.com/olikraus/u8g2).

## Ideas for future versions

- Multiple stored BPM slots (A/B/C) for crate digging
- Automatic BPM detection via a microphone module (MAX9814 / INMP441)
- Half/double-time detection for tracks tapped an octave off
