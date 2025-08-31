#include <Arduino.h>

#define ENC_A 32
#define ENC_B 33
#define ENC_SW 25

volatile long encoderPos = 0;
volatile uint8_t lastEncoded = 0;

static inline void IRAM_ATTR handleABChange() {
  uint8_t a = digitalRead(ENC_A);
  uint8_t b = digitalRead(ENC_B);
  uint8_t encoded = (a << 1) | b;
  uint8_t sum = (lastEncoded << 2) | encoded;

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderPos++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderPos--;

  lastEncoded = encoded;
}

void encoder_setup() {
  Serial.begin(115200);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  lastEncoded = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);

  attachInterrupt(digitalPinToInterrupt(ENC_A), handleABChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), handleABChange, CHANGE);
}

void encoder_loop() {
  static long lastPrinted = LONG_MIN;
  long pos = encoderPos;

  if (pos != lastPrinted) {
    Serial.print("Encoder position: ");
    Serial.println(pos);
    lastPrinted = pos;
  }

  if (digitalRead(ENC_SW) == LOW) {
    Serial.println("Button pressed!");
    delay(200);
  }
}
