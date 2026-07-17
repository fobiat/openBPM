// openBPMcount — tap-tempo BPM counter & beatmatch assistant for vinyl DJing
// Target: ideaspark ESP32-WROOM-32 + integrated 0.96" SSD1306 OLED (128x64, I2C)
//
// Controls (single onboard BOOT button, GPIO0):
//   SHORT press  -> tap a beat for the ACTIVE deck
//   LONG  press  -> lock the active deck's BPM and switch to the other deck (A <-> B)
//
// Workflow while beatmatching:
//   1. Boot -> deck A is active. Tap along with the playing record.
//   2. Long-press to lock A and move to deck B.
//   3. Tap along with the record you're cueing on deck B.
//   4. The footer shows the pitch % to dial into the ACTIVE deck to match the
//      other, plus a SPEED UP / SLOW DOWN / MATCHED nudge.
//   Long-press again to hop back to a deck and re-tap it any time.

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
static const uint8_t  PIN_BUTTON      = 0;     // onboard BOOT button (active LOW)
static const uint8_t  OLED_SDA        = 21;    // ideaspark integrated OLED
static const uint8_t  OLED_SCL        = 22;

static const uint16_t DEBOUNCE_MS     = 25;    // contact debounce
static const uint16_t LONGPRESS_MS    = 600;   // hold time for "switch deck"
static const uint32_t IDLE_RESET_MS   = 3000;  // gap that starts a fresh tap run
static const uint8_t  MAX_INTERVALS   = 8;     // rolling window for averaging
static const uint8_t  LOCK_TAPS       = 6;     // taps before we call the BPM solid
static const float    MATCH_TOL_BPM   = 0.10f; // within this = "MATCHED"

// Full-frame-buffer SSD1306 on hardware I2C. Reset pin not wired on this board.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ---------------------------------------------------------------------------
// BPM engine — one instance per deck
// ---------------------------------------------------------------------------
struct Deck {
  uint32_t intervals[MAX_INTERVALS] = {0};
  uint8_t  count      = 0;      // number of valid intervals stored
  uint8_t  head       = 0;      // ring-buffer write position
  uint32_t lastTapMs  = 0;      // millis() of the most recent tap
  uint32_t tapCount   = 0;      // total taps in the current run
  float    bpm        = 0.0f;   // current averaged BPM (0 = not measured)
  bool     locked     = false;  // frozen by a long-press

  void clear() {
    count = 0;
    head = 0;
    tapCount = 0;
    lastTapMs = 0;
    // bpm/locked are intentionally preserved so a locked value survives a reset
  }

  // Register a beat. Returns true if this tap contributed to a live measurement.
  void tap(uint32_t now) {
    // A long silence means we've moved to a new record: start a fresh run.
    if (lastTapMs != 0 && (now - lastTapMs) > IDLE_RESET_MS) {
      count = 0;
      head = 0;
      tapCount = 0;
    }

    if (lastTapMs != 0) {
      uint32_t interval = now - lastTapMs;

      // Outlier rejection: if the tap is wildly off the running average
      // (missed/double beat), restart the window instead of poisoning it.
      if (count > 0) {
        uint32_t avg = averageInterval();
        if (interval > avg * 2 || interval * 2 < avg) {
          count = 0;
          head = 0;
        }
      }

      if (interval > 100 && interval < 3000) { // 20..600 BPM sanity band
        intervals[head] = interval;
        head = (head + 1) % MAX_INTERVALS;
        if (count < MAX_INTERVALS) count++;
        recompute();
      }
    }

    lastTapMs = now;
    tapCount++;
    locked = false; // a fresh tap unlocks and refines
  }

  uint32_t averageInterval() const {
    if (count == 0) return 0;
    uint64_t sum = 0;
    for (uint8_t i = 0; i < count; i++) sum += intervals[i];
    return (uint32_t)(sum / count);
  }

  void recompute() {
    uint32_t avg = averageInterval();
    bpm = (avg > 0) ? (60000.0f / (float)avg) : 0.0f;
  }

  bool solid() const { return count >= (LOCK_TAPS - 1); }
};

Deck deckA;
Deck deckB;
Deck* active = &deckA;   // deck the taps currently drive

// ---------------------------------------------------------------------------
// Button handling — debounce + short/long press detection
// ---------------------------------------------------------------------------
enum ButtonEvent { BTN_NONE, BTN_SHORT, BTN_LONG };

bool     btnStable    = false;   // debounced logical state (true = pressed)
bool     btnRaw       = false;
uint32_t btnChangedMs = 0;
uint32_t btnDownMs    = 0;
bool     longFired    = false;   // long-press already emitted for this hold

ButtonEvent pollButton() {
  ButtonEvent ev = BTN_NONE;
  uint32_t now = millis();

  bool reading = (digitalRead(PIN_BUTTON) == LOW); // active LOW
  if (reading != btnRaw) {
    btnRaw = reading;
    btnChangedMs = now;
  }

  if ((now - btnChangedMs) >= DEBOUNCE_MS && reading != btnStable) {
    btnStable = reading;
    if (btnStable) {           // press begins
      btnDownMs = now;
      longFired = false;
    } else {                   // release
      if (!longFired) ev = BTN_SHORT;
    }
  }

  // Fire the long-press while still held, so the hold feels responsive.
  if (btnStable && !longFired && (now - btnDownMs) >= LONGPRESS_MS) {
    longFired = true;
    ev = BTN_LONG;
  }
  return ev;
}

// ---------------------------------------------------------------------------
// Beatmatch math
// ---------------------------------------------------------------------------
// Pitch % to apply to `from` so it matches `to`:  ((to/from) - 1) * 100
float pitchPercent(float from, float to) {
  if (from <= 0.0f || to <= 0.0f) return 0.0f;
  return ((to / from) - 1.0f) * 100.0f;
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
uint32_t lastTapFlashMs = 0; // brief visual pulse on each tap

void formatBpm(char* buf, size_t n, float bpm) {
  if (bpm <= 0.0f) snprintf(buf, n, "---.-");
  else             snprintf(buf, n, "%.1f", bpm);
}

void draw() {
  u8g2.clearBuffer();

  const bool activeIsA = (active == &deckA);
  Deck& other = activeIsA ? deckB : deckA;

  char big[8];
  formatBpm(big, sizeof(big), active->bpm);

  // --- Header: which deck is live + tap count -----------------------------
  u8g2.setFont(u8g2_font_6x12_tf);
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "DECK %c", activeIsA ? 'A' : 'B');
  u8g2.drawStr(0, 10, hdr);

  if (active->locked) {
    u8g2.drawStr(74, 10, "LOCKED");
  } else {
    char taps[12];
    snprintf(taps, sizeof(taps), "TAPS %lu", (unsigned long)active->tapCount);
    u8g2.drawStr(74, 10, taps);
  }

  // Tap-flash marker in the top-right corner for beat feedback.
  if (millis() - lastTapFlashMs < 90) {
    u8g2.drawBox(124, 0, 4, 4);
  }

  // --- Big BPM number ------------------------------------------------------
  u8g2.setFont(u8g2_font_fub25_tn);
  uint8_t w = u8g2.getStrWidth(big);
  u8g2.drawStr((128 - w) / 2, 42, big);

  // --- Footer: both decks + pitch/nudge -----------------------------------
  u8g2.drawHLine(0, 47, 128);
  u8g2.setFont(u8g2_font_6x12_tf);

  char foot[26];
  char a[8], b[8];
  formatBpm(a, sizeof(a), deckA.bpm);
  formatBpm(b, sizeof(b), deckB.bpm);
  snprintf(foot, sizeof(foot), "A:%s  B:%s", a, b);
  u8g2.drawStr(0, 58, foot);

  // Bottom line: guidance for the ACTIVE deck relative to the OTHER deck.
  u8g2.setFont(u8g2_font_6x10_tf);
  if (active->bpm > 0.0f && other.bpm > 0.0f) {
    float diff  = other.bpm - active->bpm;     // >0 => active is too slow
    float pct   = pitchPercent(active->bpm, other.bpm);
    char line[26];
    if (fabsf(diff) <= MATCH_TOL_BPM) {
      snprintf(line, sizeof(line), "MATCHED  %+.2f%%", pct);
    } else if (diff > 0) {
      snprintf(line, sizeof(line), "SPEED UP %+.2f%%", pct);
    } else {
      snprintf(line, sizeof(line), "SLOW DN  %+.2f%%", pct);
    }
    u8g2.drawStr(0, 64, line);
  } else {
    u8g2.drawStr(0, 64, "long-press: lock+swap");
  }

  u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.setI2CAddress(0x3C << 1);
  u8g2.begin();

  // Splash
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(6, 26, "openBPMcount");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 44, "tap the beat...");
  u8g2.sendBuffer();
  delay(1200);
}

void loop() {
  ButtonEvent ev = pollButton();
  uint32_t now = millis();

  if (ev == BTN_SHORT) {
    active->tap(now);
    lastTapFlashMs = now;
  } else if (ev == BTN_LONG) {
    // Lock the active deck's current reading and swap to the other deck.
    if (active->bpm > 0.0f) active->locked = true;
    active = (active == &deckA) ? &deckB : &deckA;
  }

  draw();
  delay(5); // ~200 Hz UI/button loop, plenty responsive for tapping
}
