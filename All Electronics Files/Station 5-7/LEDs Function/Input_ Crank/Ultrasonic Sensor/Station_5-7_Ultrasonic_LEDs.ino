#include <Adafruit_NeoPixel.h>

// ====== USER SETTINGS (edit these) ======
#define NUM_LEDS        8
#define LED_PIN         4        // LED Pin - Dont Change
#define BRIGHTNESS      80       // 0–255
#define COLOR_R         255
#define COLOR_G         60
#define COLOR_B         0        // warm amber

// Ultrasonic sensor (HC-SR04 style)
#define TRIG_PIN        6 // Dont Change
#define ECHO_PIN        7 // Don't Change

// Trigger zone and timing
#define THRESHOLD_CM    50       // trigger when distance <= this
#define HYSTERESIS_CM   5        // must leave zone by this margin to re-arm
#define ON_TIME_MS      3000     // Amount of time LED's stay on
#define MEAS_PERIOD_MS  60       // sensor period (ms)
// =======================================

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// One-shot state
bool active   = false;           // true while LEDs are in the ON window
bool armed    = true;            // true when a new trigger is allowed
unsigned long startMs = 0;

// Sensor sampling throttle
unsigned long lastSampleMs = 0;

// --- Helpers ---
uint16_t readDistanceCm() {
  // Trigger a 10 µs pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure echo (timeout ≈ 25 ms ~ 4.3 m)
  unsigned long us = pulseIn(ECHO_PIN, HIGH, 25000);
  if (us == 0) return 0;         // 0 = out of range / no echo
  return (uint16_t)(us / 58);    // convert µs to cm (approx.)
}

void ledsOn() 
{
  for (int i = 0; i < NUM_LEDS; i++) 
  {
    strip.setPixelColor(i, strip.Color(COLOR_R, COLOR_G, COLOR_B));
  }
  strip.show();
}

void ledsOff() 
{
  strip.clear();
  strip.show();
}

void setup() 
{
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();
}

void loop() 
{
  unsigned long now = millis();

  // ----- Sample ultrasonic at a steady rate -----
  static uint16_t lastCm = 0;
  if (now - lastSampleMs >= MEAS_PERIOD_MS) 
  {
    lastSampleMs = now;
    lastCm = readDistanceCm();

    // ----- Trigger logic -----
    if (!active && armed) 
    {
      if (lastCm > 0 && lastCm <= THRESHOLD_CM) 
      {
        active = true;
        armed  = false;          // ignore further triggers until re-armed
        startMs = now;
        ledsOn();
      }
    }

    // Re-arm only after the object leaves the zone (with hysteresis)
    if (!active && !armed) 
    {
      if (lastCm == 0 || lastCm > (THRESHOLD_CM + HYSTERESIS_CM)) {
        armed = true;
      }
    }
  }

  // ----- One-shot timeout -----
  if (active && (now - startMs >= ON_TIME_MS)) {
    active = false;
    ledsOff();
    // Re-arming handled by hysteresis check above
  }
}
