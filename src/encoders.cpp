#include "encoders.h"

static const uint8_t encPinA[NUM_ENCODERS]  = {16, 18, 25, 32, 14};
static const uint8_t encPinB[NUM_ENCODERS]  = {17, 19, 26, 33, 15};
static const uint8_t encPinSW[NUM_ENCODERS] = {35, 36, 27, 23, 34};

static volatile long encPos[NUM_ENCODERS]    = {};
static volatile uint8_t encLast[NUM_ENCODERS] = {};

// --- ISR handlers (one per encoder — can't pass args to ISRs) ---

#define MAKE_ISR(N)                                                        \
  static void IRAM_ATTR encISR##N() {                                      \
    uint8_t enc = (digitalRead(encPinA[N]) << 1) | digitalRead(encPinB[N]);\
    uint8_t s   = (encLast[N] << 2) | enc;                                \
    if (s==0b1101||s==0b0100||s==0b0010||s==0b1011) encPos[N]++;           \
    if (s==0b1110||s==0b0111||s==0b0001||s==0b1000) encPos[N]--;           \
    encLast[N] = enc;                                                      \
  }

MAKE_ISR(0)
MAKE_ISR(1)
MAKE_ISR(2)
MAKE_ISR(3)
MAKE_ISR(4)

static void (*encISRs[NUM_ENCODERS])() = {encISR0, encISR1, encISR2, encISR3, encISR4};

void encodersSetup() {
  for (int i = 0; i < NUM_ENCODERS; i++) {
    pinMode(encPinA[i], INPUT_PULLUP);
    pinMode(encPinB[i], INPUT_PULLUP);
    // GPIOs 34-36 are input-only with no internal pullup (need external 10K)
    uint8_t swPin = encPinSW[i];
    pinMode(swPin, (swPin >= 34) ? INPUT : INPUT_PULLUP);

    encLast[i] = (digitalRead(encPinA[i]) << 1) | digitalRead(encPinB[i]);
    attachInterrupt(digitalPinToInterrupt(encPinA[i]), encISRs[i], CHANGE);
    attachInterrupt(digitalPinToInterrupt(encPinB[i]), encISRs[i], CHANGE);
  }
}

long encoderPosition(int i) {
  return encPos[i];
}

bool encoderSwitch(int i) {
  return digitalRead(encPinSW[i]) == LOW;
}

int encoderIndex(int i, int optionCount) {
  if (optionCount <= 0) return 0;
  long pos = encPos[i];
  int idx = (int)(pos % optionCount);
  if (idx < 0) idx += optionCount;
  return idx;
}

void encoderReset(int i) {
  encPos[i] = 0;
}
