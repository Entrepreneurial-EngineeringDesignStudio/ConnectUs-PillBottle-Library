#include <Servo.h>
#include <Adafruit_NeoPixel.h>

// ========================== PINS ==========================
// ---- Buttons for SERVOS (active LOW, to GND) ----
const uint8_t BTN_A = 2;   // bucket button
const uint8_t BTN_B = 3;   // bottle button (opens 3 gates)

// ---- Servos ----
const uint8_t SERVO_BUCKET = 5;
const uint8_t SERVO_B1     = 6;
//const uint8_t SERVO_B2     = 7;
//const uint8_t SERVO_B3     = 8;

// ---- NeoPixel strip ----
#define LED_PIN         11
#define NUM_LEDS        40
#define BRIGHTNESS      120

// ====================== TIMINGS/SETTINGS ==================
const int ANGLE_CLOSED = 0;     // adjust to your mechanics
const int ANGLE_OPEN   = 110;    // adjust to your mechanics

const unsigned long GATE_OPEN_MS = 2000; // how long gates stay open
const unsigned long DEBOUNCE_MS  = 30;   // button debounce

// --- LED pre-roll segment sizes (adjust these) ---
uint16_t LEDS_RED_COUNT    = 8;  // number of LEDs to show red
uint16_t LEDS_YELLOW_COUNT = 8;  // number of LEDs to show yellow
uint16_t LEDS_GREEN_COUNT  = 16;  // not used in the final GREEN phase now (full strip), but kept if you ever revert

// --- LED pre-roll durations (adjust these) ---
uint32_t RED_TIME_MS    = 2000;
uint32_t YELLOW_TIME_MS = 2000;
uint32_t GREEN_TIME_MS  = 0;

// ========================== SERVOS =========================
Servo bucket;
Servo b1, b2, b3;

// Button state for servos
bool aPrev = HIGH, bPrev = HIGH;      // INPUT_PULLUP: HIGH = not pressed
unsigned long aLastEdge = 0, bLastEdge = 0;

// ======================= NEOPIXEL CORE ====================
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ======================= STATE MACHINE ====================
enum PendingAction : uint8_t { ACT_NONE = 0, ACT_BUCKET, ACT_BOTTLES };
enum RunState : uint8_t {
  ST_IDLE = 0,
  ST_RED,
  ST_YELLOW,
  ST_GREEN,
  ST_GATES_OPEN
};

RunState state = ST_IDLE;
PendingAction action = ACT_NONE;

uint32_t stateStartMs = 0;   // when current state started

// ========================= HELPERS ========================
static inline void ledsOff() {
  strip.clear();
  strip.show();
}

static inline uint16_t clampSeg(uint16_t want, uint16_t remain) {
  return (want > remain) ? remain : want;
}

// Render LEDs for each phase.
// RED = left segment (LEDS_RED_COUNT)
// YELLOW = next segment (LEDS_YELLOW_COUNT)
// GREEN (final countdown) = FULL STRIP green
void drawSegments(uint8_t phase_u8) {
  RunState phase = (RunState)phase_u8;
  strip.clear();

  if (phase == ST_GREEN) {
    // FINAL COUNTDOWN: full-strip green
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(0, 200, 60));
    }
    strip.show();
    return;
  }

  uint16_t offset = 0;

  // RED segment (shown only in ST_RED)
  uint16_t redCnt = clampSeg(LEDS_RED_COUNT, NUM_LEDS - offset);
  if (phase == ST_RED) {
    for (uint16_t i = 0; i < redCnt; i++) {
      strip.setPixelColor(offset + i, strip.Color(255, 0, 0));
    }
  }
  offset += redCnt;

  // YELLOW segment (shown only in ST_YELLOW)
  uint16_t yelCnt = clampSeg(LEDS_YELLOW_COUNT, NUM_LEDS - offset);
  if (phase == ST_YELLOW) {
    for (uint16_t i = 0; i < yelCnt; i++) {
      strip.setPixelColor(offset + i, strip.Color(255, 180, 0));
    }
  }
  // (LEDS_GREEN_COUNT kept for compatibility but not used in this phase layout)

  strip.show();
}

// State entry (uint8_t signature avoids Arduino auto-prototype enum issue)
void enterState(uint8_t s_u8) {
  RunState s = (RunState)s_u8;
  state = s;
  stateStartMs = millis();

  switch (state) {
    case ST_IDLE:
      action = ACT_NONE;
      ledsOff();
      break;

    case ST_RED:
      drawSegments((uint8_t)ST_RED);
      break;

    case ST_YELLOW:
      drawSegments((uint8_t)ST_YELLOW);
      break;

    case ST_GREEN:
      drawSegments((uint8_t)ST_GREEN);  // full-strip green
      break;

    case ST_GATES_OPEN:
      // Leave LEDs as-is (solid green from ST_GREEN)
      if (action == ACT_BUCKET) {
        bucket.write(ANGLE_OPEN);
      } else if (action == ACT_BOTTLES) {
        b1.write(ANGLE_OPEN);
        b2.write(ANGLE_OPEN);
        b3.write(ANGLE_OPEN);
      }
      break;
  }
}

void updateStateMachine() {
  uint32_t now = millis();

  switch (state) {
    case ST_IDLE:
      break;

    case ST_RED:
      if (now - stateStartMs >= RED_TIME_MS) {
        enterState((uint8_t)ST_YELLOW);
      }
      break;

    case ST_YELLOW:
      if (now - stateStartMs >= YELLOW_TIME_MS) {
        enterState((uint8_t)ST_GREEN);
      }
      break;

    case ST_GREEN:
      if (now - stateStartMs >= GREEN_TIME_MS) {
        enterState((uint8_t)ST_GATES_OPEN);
      }
      break;

    case ST_GATES_OPEN:
      if (now - stateStartMs >= GATE_OPEN_MS) {
        // Close gates
        bucket.write(ANGLE_CLOSED);
        b1.write(ANGLE_CLOSED);
        b2.write(ANGLE_CLOSED);
        b3.write(ANGLE_CLOSED);

        // Reset LEDs and go idle
        enterState((uint8_t)ST_IDLE);
      }
      break;
  }
}

// ============================= SETUP =============================
void setup() {
  // --- Servo buttons ---
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);

  // --- Servos ---
  bucket.attach(SERVO_BUCKET);
  b1.attach(SERVO_B1);
  //b2.attach(SERVO_B2);
//  b3.attach(SERVO_B3);

  bucket.write(ANGLE_CLOSED);
  b1.write(ANGLE_CLOSED);
  b2.write(ANGLE_CLOSED);
  b3.write(ANGLE_CLOSED);

  // --- LEDs ---
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  randomSeed(analogRead(A0));

  enterState((uint8_t)ST_IDLE);
}

// ============================== LOOP =============================
// Debounced edge detection for both buttons.
// While a sequence is running (non-IDLE), button presses are ignored.
void loop() {
  unsigned long now = millis();

  // --------- Button A (bucket) ---------
  bool aNow = digitalRead(BTN_A); // HIGH=released, LOW=pressed
  if (aNow != aPrev && (now - aLastEdge) > DEBOUNCE_MS) {
    aLastEdge = now;
    // Trigger on press (HIGH->LOW)
    if (aPrev == HIGH && aNow == LOW) {
      if (state == ST_IDLE) {
        action = ACT_BUCKET;
        enterState((uint8_t)ST_RED);
      }
    }
    aPrev = aNow;
  }

  // --------- Button B (3 bottle gates) ---------
  bool bNow = digitalRead(BTN_B);
  if (bNow != bPrev && (now - bLastEdge) > DEBOUNCE_MS) {
    bLastEdge = now;
    if (bPrev == HIGH && bNow == LOW) {
      if (state == ST_IDLE) {
        action = ACT_BOTTLES;
        enterState((uint8_t)ST_RED);
      }
    }
    bPrev = bNow;
  }

  // --------- State machine ---------
  updateStateMachine();
}
