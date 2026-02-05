#include <Adafruit_NeoPixel.h>

// ================== USER SETTINGS ==================
#define NUM_LEDS        25
#define LED_PIN         4            // NeoPixel DIN
#define BTN_NEXT_PIN    2            // Pattern-cycle button → GND (INPUT_PULLUP)
#define BRIGHTNESS      120          // 0–255

// Ultrasonic pins
#define TRIG_PIN        6
#define ECHO_PIN        7

// Proximity & motion behavior
#define THRESHOLD_CM      13         // ~5 inches (12.7 cm)
#define MOTION_DELTA_CM    2         // change required to count as motion
//#define MOTION_HOLD_MS  2000         // UPDATE TO CHANGE HOW LONG UNTIL LIGHTS SHUT DOWN AFTER MOTION DETECTED

#define MEAS_PERIOD_MS    60         // ~16 Hz sampling

// Frame limiter (roughly 60 FPS)
#define FRAME_MIN_MS      16

// Button debounce
#define DEBOUNCE_MS       30
// ===================================================

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------- App State ----------------
bool ledsEnabled = false;            // controlled by motion
uint8_t currentPattern = 0;

// ------------- Button State -------------
struct Btn {
  uint8_t pin;
  bool prev;            // INPUT_PULLUP: true = NOT pressed
  uint32_t lastEdge;
  Btn(uint8_t p): pin(p), prev(true), lastEdge(0) {}
  Btn(): pin(0), prev(true), lastEdge(0) {}
};
Btn btnNext(BTN_NEXT_PIN);

// ------------- Ultrasonic State -------------
uint32_t lastSampleMs = 0;
uint16_t prevCm = 0;                 // 0 used as "invalid/no-previous"
uint32_t lastMotionMs = 0;

// ------------- Helpers -------------
static inline uint32_t scaleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t scale) {
  return strip.Color(
    (uint8_t)((uint16_t)r * scale / 255),
    (uint8_t)((uint16_t)g * scale / 255),
    (uint8_t)((uint16_t)b * scale / 255)
  );
}

// Simple distance read (HC-SR04). Returns centimeters; 0 means "no echo".
uint16_t readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long us = pulseIn(ECHO_PIN, HIGH, 25000UL); // ~25 ms timeout
  if (us == 0) return 0;   // no echo
  return (uint16_t)(us / 58); // ~58 µs per cm (round trip)
}

// ============= PATTERNS =============

// ---- 0) Wipe with pause (color tweakable) ----
#define WIPE_INTERVAL_MS  25 // SPEED OF THE LIGHT SEQUENCE 
#define PAUSE_AT_END_MS   200 // DO NOT CHNAGE FOR STATION 2
static uint16_t wipeIndex = 0;
static uint32_t wipeLastStep = 0;
static bool wipePause = false;
static uint32_t wipePauseStart = 0;

void pattern_wipe_pause() {
  uint32_t now = millis();

  if (wipePause) {
    if (now - wipePauseStart >= PAUSE_AT_END_MS) {
      wipePause = false;
      wipeIndex = 0;
      wipeLastStep = now;
    }
  } else if (now - wipeLastStep >= WIPE_INTERVAL_MS) {
    wipeLastStep = now;
    if (wipeIndex < NUM_LEDS) wipeIndex++;
    else { wipePause = true; wipePauseStart = now; }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < wipeIndex) strip.setPixelColor(i, strip.Color(255, 20, 147)); // pink
    else               strip.setPixelColor(i, 0);
  }
}

// ---- 1) Snake Bounce (up → down → up) ----
#define SNAKE_INTERVAL_MS   60
#define SNAKE_LEN           6
#define SNAKE_R             0
#define SNAKE_G             200
#define SNAKE_B             120
void pattern_snake_bounce() {
  static int16_t head = 0;
  static int8_t dir = +1;
  static uint32_t lastStep = 0;

  uint32_t now = millis();
  if (now - lastStep >= SNAKE_INTERVAL_MS) {
    lastStep = now;
    head += dir;
    if (head >= (int16_t)(NUM_LEDS - 1)) { head = NUM_LEDS - 1; dir = -1; }
    if (head <= 0)                       { head = 0;             dir = +1; }
  }

  strip.clear();
  int16_t start = head - (SNAKE_LEN - 1);
  for (int16_t i = 0; i < SNAKE_LEN; i++) {
    int16_t idx = start + i;
    if (idx >= 0 && idx < (int16_t)NUM_LEDS) {
      strip.setPixelColor(idx, strip.Color(SNAKE_R, SNAKE_G, SNAKE_B));
    }
  }
}

// ---- 2) Carnival / Theater Chase (marquee bulbs) ----
#define CHASE_INTERVAL_MS  80
#define CHASE_GAP          3
#define CHASE_R1           255
#define CHASE_G1           100
#define CHASE_B1           0
#define CHASE_R2           255
#define CHASE_G2           0
#define CHASE_B2           60
void pattern_carnival_chase() {
  static uint8_t phase = 0;
  static uint32_t lastStep = 0;
  static bool alt = false;

  uint32_t now = millis();
  if (now - lastStep >= CHASE_INTERVAL_MS) {
    lastStep = now;
    phase = (phase + 1) % CHASE_GAP;
    alt = !alt;
  }

  strip.clear();
  uint32_t c = alt ? strip.Color(CHASE_R1, CHASE_G1, CHASE_B1)
                   : strip.Color(CHASE_R2, CHASE_G2, CHASE_B2);

  for (int i = 0; i < NUM_LEDS; i++) {
    if ((i % CHASE_GAP) == phase) strip.setPixelColor(i, c);
  }
}

// ---- 3) Cylon Scanner with fading trail ----
#define CYLON_INTERVAL_MS   20
#define CYLON_TRAIL_LEN     8
#define CYLON_HEAD_R        255
#define CYLON_HEAD_G        80
#define CYLON_HEAD_B        0
void pattern_cylon_scanner() {
  static int16_t pos = 0;
  static int8_t dir = +1;
  static uint32_t lastStep = 0;

  uint32_t now = millis();
  if (now - lastStep >= CYLON_INTERVAL_MS) {
    lastStep = now;
    pos += dir;
    if (pos <= 0) { pos = 0; dir = +1; }
    if (pos >= (int16_t)(NUM_LEDS - 1)) { pos = NUM_LEDS - 1; dir = -1; }
  }

  strip.clear();
  strip.setPixelColor(pos, strip.Color(CYLON_HEAD_R, CYLON_HEAD_G, CYLON_HEAD_B)); // head

  for (int i = 1; i <= CYLON_TRAIL_LEN; i++) {
    uint8_t fade = (uint8_t)(255 - (i * (255 / (CYLON_TRAIL_LEN + 1))));
    int16_t left  = pos - i;
    int16_t right = pos + i;
    if (left  >= 0)       strip.setPixelColor(left,  scaleColor(CYLON_HEAD_R, CYLON_HEAD_G, CYLON_HEAD_B, fade));
    if (right < NUM_LEDS) strip.setPixelColor(right, scaleColor(CYLON_HEAD_R, CYLON_HEAD_G, CYLON_HEAD_B, fade));
  }
}

// ---- 4) Twinkle Sprinkle (random sparkles that fade) ----
#define TWINKLE_NEW_PROB   30    // 0..255 chance per frame
#define TWINKLE_DECAY      18
#define TWINKLE_FRAME_MS   30
void pattern_twinkle() {
  static uint8_t levels[NUM_LEDS] = {0};
  static uint32_t lastFrame = 0;

  uint32_t now = millis();
  if (now - lastFrame < TWINKLE_FRAME_MS) return;
  lastFrame = now;

  for (int i = 0; i < NUM_LEDS; i++) {
    if (levels[i] > TWINKLE_DECAY) levels[i] -= TWINKLE_DECAY;
    else levels[i] = 0;
  }

  if ((uint8_t)random(0, 256) < TWINKLE_NEW_PROB) {
    int i = random(0, NUM_LEDS);
    levels[i] = 255;
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t v = levels[i];
    strip.setPixelColor(i, strip.Color(v, v, v));  // cool white
  }
}

// ---- 5) Bounce Wipe (fill then unfill) ----
#define BOUNCE_INTERVAL_MS  70
#define BOUNCE_R            255
#define BOUNCE_G            60
#define BOUNCE_B            0
void pattern_bounce_wipe() {
  static int16_t count = 0;
  static int8_t dir = +1;
  static uint32_t lastStep = 0;

  uint32_t now = millis();
  if (now - lastStep >= BOUNCE_INTERVAL_MS) {
    lastStep = now;
    count += dir;
    if (count >= (int16_t)NUM_LEDS) { count = NUM_LEDS; dir = -1; }
    if (count <= 0)                  { count = 0;        dir = +1; }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < count) strip.setPixelColor(i, strip.Color(BOUNCE_R, BOUNCE_G, BOUNCE_B));
    else           strip.setPixelColor(i, 0);
  }
}

// -------- Pattern table --------
typedef void (*PatternFn)();
PatternFn patterns[] = {
  pattern_wipe_pause,
  pattern_snake_bounce,
  pattern_carnival_chase,
  pattern_cylon_scanner,
  pattern_twinkle,
  pattern_bounce_wipe
};
const uint8_t PATTERN_COUNT = sizeof(patterns) / sizeof(patterns[0]);

// ============= RENDER =============
void render() {
  if (ledsEnabled) {
    patterns[currentPattern]();  // draw current pattern (no show inside)
  } else {
    strip.clear();
  }
  strip.show();
}

// ============= BUTTONS =============
void handleButton(Btn &b, void (*onRelease)()) {
  uint32_t now = millis();
  bool nowHigh = (digitalRead(b.pin) == HIGH); // HIGH = not pressed (INPUT_PULLUP)

  if (nowHigh != b.prev && (now - b.lastEdge) > DEBOUNCE_MS) {
    b.lastEdge = now;

    // pressed -> released (LOW -> HIGH) triggers action
    if (nowHigh && !b.prev) {
      onRelease();
      render(); // immediate feedback
    }

    b.prev = nowHigh;
  }
}

void onNextReleased() {
  currentPattern = (currentPattern + 1) % PATTERN_COUNT;
  // reset pattern-local state for a clean start on some patterns
  wipeIndex   = 0;
  wipePause   = false;
  wipeLastStep = millis();
  // others naturally settle on next tick
}

// ============= MOTION-BASED CONTROL =============
void updateMotion() {
  uint32_t now = millis();
  if (now - lastSampleMs < MEAS_PERIOD_MS) return;
  lastSampleMs = now;

  uint16_t cm = readDistanceCm();
  if (cm == 0) {
    // No echo -> treat as far away
    cm = 1000;
  }

  // Detect motion only inside the threshold zone
  if (cm <= THRESHOLD_CM && prevCm != 0) {
    uint16_t diff = (cm > prevCm) ? (cm - prevCm) : (prevCm - cm);
    if (diff >= MOTION_DELTA_CM) {
      ledsEnabled = true;
      lastMotionMs = now;     // refresh the 5s hold
    }
  }

  // Auto-off if no motion for MOTION_HOLD_MS (5 seconds)
  if (ledsEnabled && (now - lastMotionMs > MOTION_HOLD_MS)) {
    ledsEnabled = false;
  }

  // Update previous distance
  prevCm = cm;
}

// ============= SETUP/LOOP =============
void setup() {
  pinMode(BTN_NEXT_PIN,   INPUT_PULLUP);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  // Seed randomness for twinkle
  randomSeed(analogRead(A0));
}

void loop() {
  updateMotion();
  handleButton(btnNext, onNextReleased);

  static uint32_t lastFrame = 0;
  uint32_t now = millis();
  if (now - lastFrame >= FRAME_MIN_MS) {
    lastFrame = now;
    render();
  }
}
