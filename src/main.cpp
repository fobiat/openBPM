// openBPM — tap-tempo BPM counter & beatmatch assistant for vinyl DJing
//
// Runs on either ideaspark ESP32-WROOM-32 board; pick the PlatformIO env:
//   env:oled -> 0.96" SSD1306 128x64 mono
//   env:tft  -> 1.14" ST7789  240x135 colour
//
// Three actions: TAP, SWAP, MODE. Each accepts both the board's own buttons and
// external momentary buttons wired GPIO -> button -> GND (internal pull-ups,
// active LOW, no resistors needed). See the pin lists below.
//
// MODE cycles: MATCH -> LIBRARY -> WIFI -> MATCH
//
//   MATCH    TAP: tap a beat      SWAP: lock + switch deck A<->B
//   LIBRARY  TAP: next slot       SWAP: store live BPM (0 = clear)
//   WIFI     TAP: toggle the AP   SWAP: -
//
// See README.md for wiring and usage.

#include <Arduino.h>

#include "app.h"
#include "display.h"
#include "webui.h"

// ---------------------------------------------------------------------------
// Button pins.
//
// Each action accepts SEVERAL pins, so the board's own buttons and any external
// buttons you wire both drive the same thing — press whichever is to hand.
// Onboard buttons come first, external GPIO buttons second.
//
// Unsure which onboard pin is which? Flash the scanner and press each button:
//   pio run -e pinscan -t upload && pio device monitor
// then put the pins it reports in the lists below.
//
static const uint8_t TAP_PINS[]  = {27};
static const uint8_t SWAP_PINS[] = {26};
static const uint8_t MODE_PINS[] = {25};

// The onboard BOOT button drives all three actions by press length, so the board
// is fully usable with nothing wired to it at all:
//   short press -> TAP    long press -> SWAP    hold -> MODE
//
// On this board it's the only onboard button software can read; the others are
// EN/RST, which reset the chip in hardware and never reach a GPIO.
//
// GPIO0 is also a boot strapping pin: holding it while the board powers up
// enters the bootloader instead of running the firmware. That's normal — just
// don't hold it through a reset.
//
// Set to 255 to disable gestures once you have three real buttons wired.
static const uint8_t  GESTURE_PIN  = 0;
static const uint16_t LONGPRESS_MS = 600;     // short -> long threshold
static const uint16_t HOLD_MS      = 1600;    // long  -> hold threshold

static const uint16_t DEBOUNCE_MS  = 25;
static const uint32_t SLEEP_MS     = 120000;  // blank the screen after this idle
static const uint16_t TAP_FLASH_MS = 90;

// ---------------------------------------------------------------------------
// Buttons — a debounced falling-edge detector per pin, grouped by action
// ---------------------------------------------------------------------------
static const uint8_t MAX_PINS_PER_ACTION = 4;

struct ButtonGroup {
  const uint8_t* pins;
  uint8_t        n;
  bool           stable[MAX_PINS_PER_ACTION];
  bool           raw[MAX_PINS_PER_ACTION];
  uint32_t       changedMs[MAX_PINS_PER_ACTION];

  ButtonGroup(const uint8_t* p, uint8_t count) : pins(p), n(count) {
    if (n > MAX_PINS_PER_ACTION) n = MAX_PINS_PER_ACTION;
    for (uint8_t i = 0; i < n; i++) {
      stable[i] = false;
      raw[i] = false;
      changedMs[i] = 0;
    }
  }

  void begin() {
    for (uint8_t i = 0; i < n; i++) {
      // GPIO34-39 are input-only with no internal pull-up; those rely on the
      // board's own external pull-up.
      pinMode(pins[i], pins[i] >= 34 ? INPUT : INPUT_PULLUP);
    }
  }

  // True exactly once, on the debounced press edge of ANY pin in the group.
  bool pressed() {
    uint32_t now = millis();
    bool hit = false;
    for (uint8_t i = 0; i < n; i++) {
      bool reading = (digitalRead(pins[i]) == LOW); // active LOW
      if (reading != raw[i]) {
        raw[i] = reading;
        changedMs[i] = now;
      }
      if ((now - changedMs[i]) >= DEBOUNCE_MS && reading != stable[i]) {
        stable[i] = reading;
        if (stable[i]) hit = true;
      }
    }
    return hit;
  }
};

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static ButtonGroup btnTap (TAP_PINS,  COUNT_OF(TAP_PINS));
static ButtonGroup btnSwap(SWAP_PINS, COUNT_OF(SWAP_PINS));
static ButtonGroup btnMode(MODE_PINS, COUNT_OF(MODE_PINS));

// ---------------------------------------------------------------------------
// Gesture button — one pin, three actions by press length
// ---------------------------------------------------------------------------
enum Gesture { G_NONE, G_SHORT, G_LONG, G_HOLD };

struct GestureButton {
  uint8_t  pin;
  bool     stable;
  bool     raw;
  uint32_t changedMs;
  uint32_t downMs;
  bool     holdFired;   // hold already emitted for this press

  explicit GestureButton(uint8_t p)
    : pin(p), stable(false), raw(false), changedMs(0), downMs(0),
      holdFired(false) {}

  bool enabled() const { return pin != 255; }

  void begin() {
    if (enabled()) pinMode(pin, INPUT_PULLUP);
  }

  Gesture poll() {
    if (!enabled()) return G_NONE;
    uint32_t now = millis();
    Gesture ev = G_NONE;

    bool reading = (digitalRead(pin) == LOW); // active LOW
    if (reading != raw) {
      raw = reading;
      changedMs = now;
    }

    if ((now - changedMs) >= DEBOUNCE_MS && reading != stable) {
      stable = reading;
      if (stable) {                  // press begins
        downMs = now;
        holdFired = false;
      } else if (!holdFired) {       // release -> short or long
        ev = ((now - downMs) >= LONGPRESS_MS) ? G_LONG : G_SHORT;
      }
    }

    // Fire HOLD while still held so it feels responsive; the release is then
    // swallowed rather than also counting as a long press.
    if (stable && !holdFired && (now - downMs) >= HOLD_MS) {
      holdFired = true;
      ev = G_HOLD;
    }
    return ev;
  }
};

static GestureButton btnGesture(GESTURE_PIN);

// ---------------------------------------------------------------------------
static Deck  deckA;
static Deck  deckB;
static Deck* active = &deckA;

static Mode     mode           = MODE_MATCH;
static uint8_t  cursorSlot     = 0;
static uint32_t lastTapFlashMs = 0;
static uint32_t lastActivityMs = 0;
static bool     asleep         = false;

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  btnTap.begin();
  btnSwap.begin();
  btnMode.begin();
  btnGesture.begin();

  Lib::begin();
  Display::begin();
  Display::splash();
  delay(1200);

  lastActivityMs = millis();
}

void loop() {
  uint32_t now = millis();

  bool tapHit  = btnTap.pressed();
  bool swapHit = btnSwap.pressed();
  bool modeHit = btnMode.pressed();

  // The onboard button folds into the same three actions by press length.
  switch (btnGesture.poll()) {
    case G_SHORT: tapHit  = true; break;
    case G_LONG:  swapHit = true; break;
    case G_HOLD:  modeHit = true; break;
    case G_NONE:  break;
  }

  if (tapHit || swapHit || modeHit) {
    lastActivityMs = now;
    // If the screen was asleep, this press only wakes it — don't act on it.
    if (asleep) {
      asleep = false;
      Display::setPowerSave(false);
      tapHit = swapHit = modeHit = false;
    }
  }

  // MODE always advances to the next screen.
  if (modeHit) {
    mode = (mode == MODE_MATCH)   ? MODE_LIBRARY
         : (mode == MODE_LIBRARY) ? MODE_WIFI
                                  : MODE_MATCH;
  }

  switch (mode) {
    case MODE_MATCH:
      if (tapHit) {
        active->tap(now);
        lastTapFlashMs = now;
      }
      if (swapHit) {
        if (active->bpm > 0.0f) active->locked = true;
        active = (active == &deckA) ? &deckB : &deckA;
      }
      break;

    case MODE_LIBRARY:
      if (tapHit)  cursorSlot = (cursorSlot + 1) % NUM_SLOTS;
      if (swapHit) Lib::store(cursorSlot, active->bpm); // 0 clears
      break;

    case MODE_WIFI:
      if (tapHit) {
        if (WebUI::isOn()) WebUI::stop();
        else               WebUI::start();
      }
      break;
  }

  WebUI::loop();
  WebUI::setLiveBpm(active->bpm);

  // Sleep the screen after inactivity (battery + burn-in). Never sleep while
  // the AP is up — you'd be looking at the screen for the address.
  if (!asleep && !WebUI::isOn() && (now - lastActivityMs) > SLEEP_MS) {
    asleep = true;
    Display::setPowerSave(true);
  }

  if (!asleep) {
    Deck& other = (active == &deckA) ? deckB : deckA;

    UiState s;
    s.mode      = mode;
    s.bpmA      = deckA.bpm;
    s.bpmB      = deckB.bpm;
    s.activeIsA = (active == &deckA);
    s.activeBpm = active->bpm;
    s.tapCount  = active->tapCount;
    s.locked    = active->locked;
    s.solid     = active->solid();
    s.jitterMs  = active->jitterMs();
    s.beatPulse = active->beatPulse(now);
    s.tapFlash  = (now - lastTapFlashMs) < TAP_FLASH_MS;
    s.match     = computeMatch(active->bpm, other.bpm);
    s.cursor    = cursorSlot;
    s.wifiOn    = WebUI::isOn();
    s.wifiSsid  = WebUI::ssid();
    s.wifiPass  = WebUI::password();
    s.wifiIp    = WebUI::ip();

    Display::render(s);
  }

  delay(5); // ~200 Hz button/UI loop, plenty responsive for tapping
}
