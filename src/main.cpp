#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

#define BUTTON_PIN 4
#define NUM_ENCODERS 5

const uint8_t encPinA[NUM_ENCODERS]  = {16, 18, 25, 32, 14};
const uint8_t encPinB[NUM_ENCODERS]  = {17, 19, 26, 33, 15};
const uint8_t encPinSW[NUM_ENCODERS] = { 5, 13, 27, 23,  2};

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

volatile long encPos[NUM_ENCODERS] = {};
volatile uint8_t encLast[NUM_ENCODERS] = {};

static void IRAM_ATTR encISR0() {
  uint8_t enc = (digitalRead(encPinA[0]) << 1) | digitalRead(encPinB[0]);
  uint8_t s = (encLast[0] << 2) | enc;
  if (s == 0b1101 || s == 0b0100 || s == 0b0010 || s == 0b1011) encPos[0]++;
  if (s == 0b1110 || s == 0b0111 || s == 0b0001 || s == 0b1000) encPos[0]--;
  encLast[0] = enc;
}
static void IRAM_ATTR encISR1() {
  uint8_t enc = (digitalRead(encPinA[1]) << 1) | digitalRead(encPinB[1]);
  uint8_t s = (encLast[1] << 2) | enc;
  if (s == 0b1101 || s == 0b0100 || s == 0b0010 || s == 0b1011) encPos[1]++;
  if (s == 0b1110 || s == 0b0111 || s == 0b0001 || s == 0b1000) encPos[1]--;
  encLast[1] = enc;
}
static void IRAM_ATTR encISR2() {
  uint8_t enc = (digitalRead(encPinA[2]) << 1) | digitalRead(encPinB[2]);
  uint8_t s = (encLast[2] << 2) | enc;
  if (s == 0b1101 || s == 0b0100 || s == 0b0010 || s == 0b1011) encPos[2]++;
  if (s == 0b1110 || s == 0b0111 || s == 0b0001 || s == 0b1000) encPos[2]--;
  encLast[2] = enc;
}
static void IRAM_ATTR encISR3() {
  uint8_t enc = (digitalRead(encPinA[3]) << 1) | digitalRead(encPinB[3]);
  uint8_t s = (encLast[3] << 2) | enc;
  if (s == 0b1101 || s == 0b0100 || s == 0b0010 || s == 0b1011) encPos[3]++;
  if (s == 0b1110 || s == 0b0111 || s == 0b0001 || s == 0b1000) encPos[3]--;
  encLast[3] = enc;
}
static void IRAM_ATTR encISR4() {
  uint8_t enc = (digitalRead(encPinA[4]) << 1) | digitalRead(encPinB[4]);
  uint8_t s = (encLast[4] << 2) | enc;
  if (s == 0b1101 || s == 0b0100 || s == 0b0010 || s == 0b1011) encPos[4]++;
  if (s == 0b1110 || s == 0b0111 || s == 0b0001 || s == 0b1000) encPos[4]--;
  encLast[4] = enc;
}

void (*encISRs[NUM_ENCODERS])() = {encISR0, encISR1, encISR2, encISR3, encISR4};

int activeEncoder = 0;
bool lastButtonState = HIGH;

bool isEncoderConnected(int i) {
  int a = digitalRead(encPinA[i]);
  int b = digitalRead(encPinB[i]);
  int sw = digitalRead(encPinSW[i]);
  // Unconnected pins with pullup all read HIGH and position stays 0.
  // If any pin reads LOW, or position has moved, it's connected.
  if (a == LOW || b == LOW || sw == LOW) return true;
  if (encPos[i] != 0) return true;
  return false;
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  for (int i = 0; i < NUM_ENCODERS; i++) {
    pinMode(encPinA[i], INPUT_PULLUP);
    pinMode(encPinB[i], INPUT_PULLUP);
    pinMode(encPinSW[i], INPUT_PULLUP);
    encLast[i] = (digitalRead(encPinA[i]) << 1) | digitalRead(encPinB[i]);
    attachInterrupt(digitalPinToInterrupt(encPinA[i]), encISRs[i], CHANGE);
    attachInterrupt(digitalPinToInterrupt(encPinB[i]), encISRs[i], CHANGE);
  }

  Wire.begin(21, 22);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found");
    while (true) delay(1000);
  }

  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println("Encoder");
  oled.println("Tester");
  oled.display();
  Serial.println("Encoder tester ready");
  delay(1000);
}

void loop() {
  bool buttonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && buttonState == LOW) {
    activeEncoder = (activeEncoder + 1) % NUM_ENCODERS;
    Serial.print("Switched to encoder ");
    Serial.println(activeEncoder + 1);
  }
  lastButtonState = buttonState;

  int i = activeEncoder;
  long pos = encPos[i];
  bool swPressed = digitalRead(encPinSW[i]) == LOW;
  bool connected = isEncoderConnected(i);

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);

  for (int e = 0; e < NUM_ENCODERS; e++) {
    if (e == activeEncoder)
      oled.print(">");
    else
      oled.print(" ");
    oled.print(e + 1);
    if (e < NUM_ENCODERS - 1) oled.print(" ");
  }

  oled.setCursor(0, 14);
  oled.print("Encoder ");
  oled.print(i + 1);

  if (!connected) {
    oled.setTextSize(1);
    oled.setCursor(0, 30);
    oled.print("No encoder connected");
  } else {
    oled.setCursor(0, 26);
    oled.print("Pos: ");
    oled.setTextSize(2);
    oled.print(pos);

    oled.setTextSize(1);
    oled.setCursor(0, 46);
    oled.print("Knob: ");
    oled.print(swPressed ? "PRESSED" : "---");
  }

  oled.setCursor(0, 57);
  oled.setTextSize(1);
  oled.print("Red btn to switch");

  oled.display();
  delay(30);
}
