#pragma once

#include "secrets.h"

// -- Display (ST7789 240x320 SPI) --
#define TFT_WIDTH      320   // landscape
#define TFT_HEIGHT     240
#define TFT_MOSI       21
#define TFT_SCLK       22
#define TFT_CS          5
#define TFT_DC          2
#define TFT_RST        13
// BL tied to 3.3V (always on)

// -- Button --
#define BUTTON_PIN     4
#define DEBOUNCE_MS    50

// -- Encoders --
#define NUM_ENCODERS   5

// -- Linear API --
#define LINEAR_API_URL "https://api.linear.app/graphql"

// -- Timing --
#define RESULT_DISPLAY_MS  3000
#define WIFI_TIMEOUT_MS    15000
