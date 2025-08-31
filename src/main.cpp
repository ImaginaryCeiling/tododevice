#include <Arduino.h>
#include <TFT_eSPI.h>

// Choose which demo to run:
#define RUN_ENCODER
// #define RUN_DISPLAY

#ifdef RUN_DISPLAY
void display_setup();
void display_loop();
#endif

#ifdef RUN_ENCODER
void encoder_setup();
void encoder_loop();
#endif

void setup() {
  #ifdef RUN_DISPLAY
    display_setup();
  #endif
  #ifdef RUN_ENCODER
    encoder_setup();
  #endif
}

void loop() {
  #ifdef RUN_DISPLAY
    display_loop();
  #endif
  #ifdef RUN_ENCODER
    encoder_loop();
  #endif
}
