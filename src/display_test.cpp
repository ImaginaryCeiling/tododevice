#include <Arduino.h>
#include <TFT_eSPI.h>

#define TFT_BL 4
TFT_eSPI tft;

int boxW = 40, boxH = 30;
int x, y, vx = 3, vy = 2;
int W, H;

void display_setup() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  W = tft.width();
  H = tft.height();

  x = 20;
  y = 40;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Hello TFT_eSPI!", 10, 10, 2);
}

void display_loop() {
  tft.fillRect(x, y, boxW, boxH, TFT_BLACK);
  x += vx; y += vy;
  if (x < 0) { x = 0; vx = -vx; }
  if (y < 20){ y = 20; vy = -vy; }
  if (x + boxW > W){ x = W - boxW; vx = -vx; }
  if (y + boxH > H){ y = H - boxH; vy = -vy; }
  tft.fillRect(x, y, boxW, boxH, TFT_CYAN);
  delay(16);
}
