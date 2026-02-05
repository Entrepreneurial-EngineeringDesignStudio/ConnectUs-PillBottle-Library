#include <Servo.h>
#include <Adafruit_NeoPixel.h>

// ====== USER SETTINGS (edit these) ======
#define BTN_PIN        2       // Button → GND (uses INPUT_PULLUP)
#define SERVO_PIN      3        // Servo signal
#define LED_PIN        4        // LED signal Pin
#define NUM_LEDS       8       // number of pixels on your strip

#define BRIGHTNESS     120      // 0–255
#define COLOR_R        0        // snake color RGB
#define COLOR_G        200
#define COLOR_B        120

#define ANGLE_CLOSED   0        // Closed Gate angle (0-180)
#define ANGLE_OPEN     90       // Open Gate angle (0-180)
#define GATE_OPEN_MS   3000     // Time Gate is open (ms)

#define DEBOUNCE_MS    30       // button debounce (ms)

// Snake animation
#define SNAKE_LEN          6    // number of lit pixels in the snake
#define SNAKE_INTERVAL_MS  60   // movement speed (ms per step)
// =======================================

Servo gate;
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---- One-shot state ----
bool active = false;              // true while gate is open & animation running
unsigned long startMs = 0;

// ---- Button edge tracking (INPUT_PULLUP: HIGH = not pressed) ----
bool btnPrev = true;
unsigned long lastEdgeMs = 0;

// ---- Snake state ----
int16_t snakeHead = -SNAKE_LEN;   // start off-strip so it "enters" from pixel 0
unsigned long lastStepMs = 0;

void ledsOff() {
  strip.clear();
  strip.show();
}

void drawSnake(int16_t head) {
  strip.clear();
  for (int i = 0; i < SNAKE_LEN; i++) {
    int16_t idx = head - i;                // tail behind head
    if (idx >= 0 && idx < NUM_LEDS) {
      strip.setPixelColor(idx, strip.Color(COLOR_R, COLOR_G, COLOR_B));
    }
  }
  strip.show();
}

void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);

  gate.attach(SERVO_PIN);
  gate.write(ANGLE_CLOSED);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  ledsOff();
}

void loop() {
  unsigned long now = millis();

  // -------- Button with debounce (press edge only) --------
  bool btnNowHigh = (digitalRead(BTN_PIN) == HIGH); // HIGH = not pressed
  if ((btnNowHigh != btnPrev) && (now - lastEdgeMs > DEBOUNCE_MS)) {
    lastEdgeMs = now;

    // PRESS: HIGH -> LOW
    if (btnPrev == true && btnNowHigh == false) {
      if (!active) {
        // Begin one-shot
        active = true;
        startMs = now;

        // Open gate immediately
        gate.write(ANGLE_OPEN);

        // Reset snake so it starts at the beginning
        snakeHead = -SNAKE_LEN;
        lastStepMs = now;

        // Draw initial frame
        drawSnake(snakeHead);
      }
      // If already active, ignore (prevents spamming)
    }
    btnPrev = btnNowHigh;
  }

  // -------- While active: animate snake & handle timeout --------
  if (active) {
    // Move snake forward every SNAKE_INTERVAL_MS
    if (now - lastStepMs >= SNAKE_INTERVAL_MS) {
      lastStepMs = now;
      snakeHead++;
      if (snakeHead >= NUM_LEDS) {
        // Loop the snake if animation time is longer than one pass
        snakeHead = -SNAKE_LEN;
      }
      drawSnake(snakeHead);
    }

    // Close after the open window
    if (now - startMs >= GATE_OPEN_MS) {
      active = false;
      gate.write(ANGLE_CLOSED);
      ledsOff();
    }
  }
}
