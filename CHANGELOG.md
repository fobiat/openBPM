# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.0] — 2026-07-19

### Added

- **Gesture control on the onboard BOOT button**, so the board is fully usable with
  nothing wired to it: short press = TAP, long press = SWAP, hold = MODE. Set
  `GESTURE_PIN = 255` to disable once real buttons are wired.

### Changed

- `pinscan` no longer reports per-edge activity on input-only pins (GPIO34–39), which
  float and cross-talk. It prints a twice-a-second level snapshot instead, so a *held*
  button is distinguishable from a floating pin.

### Notes

A pin scan of the ideaspark ESP32-WROOM-32 established that **GPIO0 is the only
onboard button software can read**. The board's other buttons are EN/RST, which reset
the chip in hardware and never reach a GPIO.

## [1.0.0] — 2026-07-19

First tagged release.

### Added

- **Tap tempo** with a rolling 8-interval average, outlier rejection for missed or
  double taps, and a 3-second idle auto-reset
- **Two decks (A/B)** with a pitch-percentage readout — how far to move the fader
- **Octave-aware matching** so half/double-time records still line up, tagged
  `x2` / `1/2`
- **Drift timer** — seconds until the decks slip a full beat apart (`60 / ΔBPM`)
- **Pitch bar** with a centre detent, colour-coded on the TFT
- **Pitch-range warning** when a match needs more than ±8 %
- **Beat pulse** indicator acting as a visual metronome
- **Tap-jitter readout** so you know whether to trust the reading
- **8 named BPM slots** persisted to flash, surviving power-off
- **WiFi library** — the ESP32 hosts its own access point so records can be named and
  exported as CSV from a phone
- **Screen sleep** after 2 minutes idle
- Support for **two displays from one codebase** via a display abstraction and
  separate PlatformIO environments (`oled`, `tft`)
- `pinscan` diagnostic environment for identifying button GPIOs by measurement
- MIT licence, and a GitHub Actions matrix build across all three environments

[Unreleased]: https://github.com/fobiat/openBPM/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/fobiat/openBPM/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/fobiat/openBPM/releases/tag/v1.0.0
