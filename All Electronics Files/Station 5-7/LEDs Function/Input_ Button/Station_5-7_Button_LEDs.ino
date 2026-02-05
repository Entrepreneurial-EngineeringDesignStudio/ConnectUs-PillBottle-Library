#include <Adafruit_NeoPixel.h>

// ====== USER SETTINGS (edit these) ======
#define NUM_LEDS     8
#define LED_PIN      4         // NeoPixel DIN - Dont Change
#define BTN_PIN      3         // Button → GND (uses INPUT_PULLUP)- Dont Change
#define BRIGHTNESS   80        // 0–255 <- adjust brightness value
#define COLOR_R      255    // RGB Values, Change accordingly
#define COLOR_G      60
#define COLOR_B      0         
#define ON_TIME_MS   5000      // how long LEDs stay ON after a press (ms)
#define DEBOUNCE_MS  30        // button debounce (ms)
// =======================================

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// One-shot state
bool active = false;           // true while LEDs are in the ON window
unsigned long startMs = 0;

// Button edge tracking (INPUT_PULLUP: HIGH = not pressed, LOW = pressed)
bool btnPrev = true;           // start as "not pressed"
unsigned long lastEdgeMs = 0;

void setup() 
{
  pinMode(BTN_PIN, INPUT_PULLUP);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();
}

void loop() 
{
  unsigned long now = millis();

  // --- Button with debounce ---
  bool btnNowHigh = (digitalRead(BTN_PIN) == HIGH);  // HIGH = not pressed
  if ((btnNowHigh != btnPrev) && (now - lastEdgeMs > DEBOUNCE_MS)) {
    lastEdgeMs = now;

    // Detect PRESS edge: HIGH -> LOW
    if (btnPrev == true && btnNowHigh == false) {
      if (!active) {
        active = true;
        startMs = now;
        // Turn LEDs ON (solid color)
        for (int i = 0; i < NUM_LEDS; i++) {
          strip.setPixelColor(i, strip.Color(COLOR_R, COLOR_G, COLOR_B));
        }
        strip.show();
      }
      // If active, ignore the press (no extension)
    }

    btnPrev = btnNowHigh;
  }

  // --- One-shot timeout ---
  if (active && (now - startMs >= ON_TIME_MS)) 
  {
    active = false;
    strip.clear();
    strip.show();
  }
}
