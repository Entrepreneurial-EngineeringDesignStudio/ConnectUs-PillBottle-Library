#include <Adafruit_NeoPixel.h>

// ====== USER SETTINGS ======
#define NUM_LEDS        8
#define LED_PIN         4        // LED Pin
#define BRIGHTNESS      80       // Brightness Adjustment 0-255
#define COLOR_R         255    // RGB Values to Change
#define COLOR_G         60
#define COLOR_B         0        

// Potentiometer input (use any analog pin)
#define POT_PIN         A1

// Trigger & timing (tune these)
#define DELTA_THRESH    120      // Increase = require more twist
#define RESET_BAND      40       // must return within this band of baseline to re-arm
#define ON_TIME_MS      3000     // Time LED's are on
#define SAMPLE_MS       10       // pot sample period (ms)
#define SMOOTH_ALPHA_Q  4        // 
// ===========================

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// One-shot state
bool active   = false;           // LEDs currently on-timed
bool armed    = true;            // can accept a new trigger
unsigned long startMs = 0;
unsigned long lastSampleMs = 0;

// Pot filtering & baseline
int baseline     = 0;            // reference position when (re)armed
int emaValue     = 0;            // smoothed analog value (EMA)

// Helpers
void ledsOn() 
{
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(COLOR_R, COLOR_G, COLOR_B));
  strip.show();
}
void ledsOff() 
{
  strip.clear();
  strip.show();
}

void setup() 
{
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  // Initialize baseline & EMA from current pot position
  int raw = analogRead(POT_PIN);
  emaValue = raw;
  baseline = raw;
}

void loop() 
{
  unsigned long now = millis();

  // ----- Sample & smooth the potentiometer -----
  if (now - lastSampleMs >= SAMPLE_MS) 
  {
    lastSampleMs = now;
    int raw = analogRead(POT_PIN);

    // Exponential moving average: ema += (raw - ema)/alpha
    emaValue += (raw - emaValue) / SMOOTH_ALPHA_Q;

    int delta = emaValue - baseline;
    int absDelta = (delta >= 0) ? delta : -delta;

    // ----- Trigger logic (distance from baseline) -----
    if (!active && armed) {
      if (absDelta >= DELTA_THRESH) 
      {
        active = true;
        armed  = false;        // lock out
        startMs = now;
        ledsOn();
      }
    }

    // ----- Re-arm logic: return close to baseline -----
    if (!active && !armed) 
    {
      if (absDelta <= RESET_BAND) 
      {
        armed = true;
        // Optional: recenter baseline to current spot so next twist can be from here
        baseline = emaValue;
      }
    }
  }

  // ----- One-shot timeout -----
  if (active && (now - startMs >= ON_TIME_MS))
  {
    active = false;
    ledsOff();
    // Re-arming happens only after returning near baseline (above)
  }
}
