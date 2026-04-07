#include "display.h"
#include "config.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// Hardware SPI (HSPI) remapped to our custom pins
static SPIClass hspi(HSPI);
static Adafruit_ST7789 tft(&hspi, TFT_CS, TFT_DC, TFT_RST);

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------
#define COL_BG       ST77XX_BLACK
#define COL_TEXT     ST77XX_WHITE
#define COL_LABEL    ST77XX_YELLOW
#define COL_VALUE    ST77XX_CYAN
#define COL_OK       ST77XX_GREEN
#define COL_ERR      ST77XX_RED
#define COL_DIM      0x7BEF   // 50% gray in RGB565

// ---------------------------------------------------------------------------
// Layout constants (landscape 320x240, textSize 2 = 12x16 px)
// ---------------------------------------------------------------------------
#define FIELD_LABEL_X   10
#define FIELD_VALUE_X   142
#define FIELD_VALUE_W   170    // pixels to clear when updating a value
#define FIELD_START_Y   48
#define FIELD_SPACING   32
#define NUM_FIELDS      5

static const char* FIELD_LABELS[NUM_FIELDS] = {
  "Status", "Priority", "Assignee", "Project", "Label"
};

static int fieldY(int f) { return FIELD_START_Y + f * FIELD_SPACING; }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void printTrunc(const char* text, int maxChars) {
  int len = strlen(text);
  if (len <= maxChars) {
    tft.print(text);
  } else {
    for (int i = 0; i < maxChars - 1; i++) tft.write(text[i]);
    tft.write('~');
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void displaySetup() {
  hspi.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);        // physical: 240 wide, 320 tall
  tft.setRotation(1);        // landscape: 320 wide, 240 tall
  tft.fillScreen(COL_BG);
  Serial.println("Display init OK");
}

void displayBoot(const char* line1, const char* line2) {
  tft.fillScreen(COL_BG);
  tft.setTextSize(2);
  tft.setTextColor(COL_TEXT);
  tft.setCursor(40, 90);
  tft.print(line1);
  if (line2) {
    tft.setCursor(40, 120);
    tft.print(line2);
  }
}

void displayIdle(uint32_t issueNum, const char* teamName) {
  tft.fillScreen(COL_BG);

  // "Ready"
  tft.setTextSize(4);
  tft.setTextColor(COL_OK);
  tft.setCursor(88, 40);
  tft.print("Ready");

  // Issue number
  tft.setTextSize(2);
  tft.setTextColor(COL_VALUE);
  tft.setCursor(100, 100);
  tft.print("Issue #");
  tft.print(issueNum);

  // Team name
  tft.setTextSize(1);
  tft.setTextColor(COL_DIM);
  tft.setCursor(120, 130);
  tft.print("Team: ");
  printTrunc(teamName, 20);

  // Hint
  tft.setTextSize(2);
  tft.setTextColor(COL_DIM);
  tft.setCursor(44, 200);
  tft.print("[Press to begin]");
}

void displayConfigBegin(uint32_t issueNum) {
  tft.fillScreen(COL_BG);

  // Title bar
  tft.setTextSize(2);
  tft.setTextColor(COL_TEXT);
  tft.setCursor(10, 10);
  tft.print("#");
  tft.print(issueNum);
  tft.print(" Device Issue");

  // Divider
  tft.drawFastHLine(10, 36, 300, COL_DIM);

  // Field labels
  tft.setTextSize(2);
  tft.setTextColor(COL_LABEL);
  for (int i = 0; i < NUM_FIELDS; i++) {
    tft.setCursor(FIELD_LABEL_X, fieldY(i));
    tft.print(FIELD_LABELS[i]);
  }

  // Bottom divider + submit hint
  tft.drawFastHLine(10, 212, 300, COL_DIM);
  tft.setTextSize(2);
  tft.setTextColor(COL_OK);
  tft.setCursor(76, 222);
  tft.print("> Submit");
}

void displayConfigValue(int field, const char* value) {
  if (field < 0 || field >= NUM_FIELDS) return;
  int y = fieldY(field);

  // Clear previous value
  tft.fillRect(FIELD_VALUE_X, y, FIELD_VALUE_W, 16, COL_BG);

  // Draw new value — 14 chars max at textSize 2 in the available width
  tft.setTextSize(2);
  tft.setTextColor(COL_VALUE);
  tft.setCursor(FIELD_VALUE_X, y);
  printTrunc(value, 14);
}

void displaySubmitting() {
  tft.fillScreen(COL_BG);
  tft.setTextSize(2);
  tft.setTextColor(COL_TEXT);
  tft.setCursor(56, 108);
  tft.print("Creating issue...");
}

void displayResult(bool success, const char* message) {
  tft.fillScreen(COL_BG);

  if (success) {
    tft.setTextSize(3);
    tft.setTextColor(COL_OK);
    tft.setCursor(72, 50);
    tft.print("Created!");

    tft.setTextSize(2);
    tft.setTextColor(COL_VALUE);
    tft.setCursor(100, 120);
    tft.print(message);
  } else {
    tft.setTextSize(3);
    tft.setTextColor(COL_ERR);
    tft.setCursor(100, 50);
    tft.print("Error");

    tft.setTextSize(2);
    tft.setTextColor(COL_TEXT);
    tft.setCursor(20, 120);
    printTrunc(message, 25);
  }
}
