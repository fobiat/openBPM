// Persisted BPM library (NVS/flash).
//
// Split out of app.cpp because this is the only part of the core that needs the
// platform. Keeping it separate lets the BPM engine build and be tested on a
// desktop via `pio test -e native`.
#include "app.h"

#include <Arduino.h>
#include <Preferences.h>


// ---------------------------------------------------------------------------
// Library
// ---------------------------------------------------------------------------
namespace Lib {

Slot slots[NUM_SLOTS];
static Preferences prefs;

static void persist(uint8_t i) {
  char key[6];
  snprintf(key, sizeof(key), "s%u", i);
  prefs.putFloat(key, slots[i].bpm);
  snprintf(key, sizeof(key), "n%u", i);
  prefs.putString(key, slots[i].name);
}

void begin() {
  prefs.begin("openbpm", false);
  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    char key[6];
    snprintf(key, sizeof(key), "s%u", i);
    slots[i].bpm = prefs.getFloat(key, 0.0f);

    snprintf(key, sizeof(key), "n%u", i);
    String n = prefs.getString(key, "");
    strncpy(slots[i].name, n.c_str(), NAME_LEN - 1);
    slots[i].name[NAME_LEN - 1] = '\0';
  }
}

void store(uint8_t i, float bpm) {
  if (i >= NUM_SLOTS) return;
  // Storing with no live reading is how you clear a slot from the device, so
  // it has to drop the name too — otherwise the slot reads as empty but keeps
  // the old record's name attached to whatever you store there next.
  if (bpm <= 0.0f) {
    clear(i);
    return;
  }
  slots[i].bpm = bpm;
  persist(i);
}

void setName(uint8_t i, const char* name) {
  if (i >= NUM_SLOTS || name == NULL) return;
  strncpy(slots[i].name, name, NAME_LEN - 1);
  slots[i].name[NAME_LEN - 1] = '\0';
  persist(i);
}

void clear(uint8_t i) {
  if (i >= NUM_SLOTS) return;
  slots[i].bpm = 0.0f;
  slots[i].name[0] = '\0';
  persist(i);
}

} // namespace Lib
