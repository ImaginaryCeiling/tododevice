#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "encoders.h"
#include "display.h"
#include "network.h"
#include "linear.h"

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

enum AppState {
  STATE_BOOT,
  STATE_IDLE,
  STATE_CONFIGURING,
  STATE_SUBMITTING,
  STATE_RESULT
};

static AppState      state     = STATE_BOOT;
static AppState      prevState = STATE_BOOT;
static LinearData    linearData;
static Preferences   prefs;
static uint32_t      issueNum;
static bool          lastResultOk;
static String        resultMsg;
static unsigned long resultTime;

// ---------------------------------------------------------------------------
// Priority lookup (hardcoded — Linear uses ints 0-4)
// ---------------------------------------------------------------------------

static const char* PRIO_NAMES[] = {"None", "Urgent", "High", "Medium", "Low"};
static const int   PRIO_VALS[]  = { 0,      1,        2,      3,        4    };
static const int   NUM_PRIOS    = 5;

// ---------------------------------------------------------------------------
// Button debounce
// ---------------------------------------------------------------------------

static bool          rawBtn       = HIGH;
static bool          stableBtn    = HIGH;
static unsigned long btnChangeAt  = 0;
static bool          btnPressed   = false;
static bool          btnLongPress = false;
static unsigned long btnDownAt    = 0;
static const unsigned long LONG_PRESS_MS = 2000;

static void readButton() {
  btnPressed = false;
  btnLongPress = false;
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != rawBtn) {
    btnChangeAt = millis();
    rawBtn = reading;
  }

  if ((millis() - btnChangeAt) > DEBOUNCE_MS && rawBtn != stableBtn) {
    if (rawBtn == LOW) {
      btnDownAt = millis();
    } else {
      if ((millis() - btnDownAt) < LONG_PRESS_MS) {
        btnPressed = true;
      }
    }
    stableBtn = rawBtn;
  }

  if (stableBtn == LOW && btnDownAt > 0 && (millis() - btnDownAt) >= LONG_PRESS_MS) {
    btnLongPress = true;
    btnDownAt = 0;
  }
}

// Encoder switch debounce (push-to-reset)
static bool encSwLast[NUM_ENCODERS] = {false};

static void checkEncoderSwitches() {
  for (int i = 0; i < NUM_ENCODERS; i++) {
    bool pressed = encoderSwitch(i);
    if (pressed && !encSwLast[i]) {
      encoderReset(i);
      Serial.printf("Encoder %d reset\n", i + 1);
    }
    encSwLast[i] = pressed;
  }
}

// ---------------------------------------------------------------------------
// Config screen — track previous indices to avoid flicker
// ---------------------------------------------------------------------------

static int prevIdx[5] = {-1, -1, -1, -1, -1};

static void resetPrevIdx() {
  for (int i = 0; i < 5; i++) prevIdx[i] = -1;
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  displaySetup();
  encodersSetup();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // --- WiFi ---
  displayBoot("Connecting", "WiFi...");
  networkSetup();

  if (!networkConnected()) {
    displayResult(false, "WiFi failed");
    delay(3000);
    ESP.restart();
  }

  // --- Fetch Linear data (up to 3 attempts) ---
  displayBoot("Fetching", "Linear data...");

  bool fetched = false;
  for (int i = 0; i < 3 && !fetched; i++) {
    fetched = linearFetchData(linearData);
    if (!fetched) delay(1000);
  }

  if (!fetched) {
    displayResult(false, "Linear fetch fail");
    delay(3000);
    ESP.restart();
  }

  Serial.printf("Team: %s | states:%d members:%d projects:%d labels:%d\n",
    linearData.teamName.c_str(),
    linearData.states.size(), linearData.members.size(),
    linearData.projects.size(), linearData.labels.size());

  // --- Issue counter (persisted in NVS) ---
  prefs.begin("tododevice", false);
  issueNum = prefs.getUInt("issueNum", 1);

  state = STATE_IDLE;
  Serial.println("-> IDLE");
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------

void loop() {
  readButton();

  bool stateChanged = (state != prevState);
  prevState = state;

  switch (state) {

    // ---- IDLE ----
    case STATE_IDLE:
      if (stateChanged) {
        displayIdle(issueNum, linearData.teamName.c_str());
      }
      if (btnPressed) {
        state = STATE_CONFIGURING;
        Serial.println("-> CONFIGURING");
      }
      if (btnLongPress) {
        displayBoot("Refreshing", "Linear data...");
        if (linearFetchData(linearData)) {
          Serial.println("Linear data refreshed");
        } else {
          displayResult(false, "Refresh failed");
          delay(2000);
        }
        displayIdle(issueNum, linearData.teamName.c_str());
      }
      break;

    // ---- CONFIGURING ----
    case STATE_CONFIGURING: {
      checkEncoderSwitches();

      int idx[5] = {
        encoderIndex(0, linearData.states.size()),
        encoderIndex(1, NUM_PRIOS),
        encoderIndex(2, linearData.members.size()),
        encoderIndex(3, linearData.projects.size()),
        encoderIndex(4, linearData.labels.size())
      };

      if (stateChanged) {
        displayConfigBegin(issueNum);
        resetPrevIdx();
      }

      // Only redraw fields that changed
      const char* vals[5] = {
        linearData.states[idx[0]].name.c_str(),
        PRIO_NAMES[idx[1]],
        linearData.members[idx[2]].name.c_str(),
        linearData.projects[idx[3]].name.c_str(),
        linearData.labels[idx[4]].name.c_str()
      };

      for (int f = 0; f < 5; f++) {
        if (idx[f] != prevIdx[f]) {
          displayConfigValue(f, vals[f]);
          prevIdx[f] = idx[f];
        }
      }

      if (btnPressed) {
        displaySubmitting();

        IssueParams p;
        p.teamId     = linearData.teamId;
        p.title      = "Device Issue #" + String(issueNum);
        p.stateId    = linearData.states[idx[0]].id;
        p.priority   = PRIO_VALS[idx[1]];
        p.assigneeId = linearData.members[idx[2]].id;
        p.projectId  = linearData.projects[idx[3]].id;
        p.labelId    = linearData.labels[idx[4]].id;

        IssueResult res = linearCreateIssue(p);

        lastResultOk = res.success;
        if (res.success) {
          resultMsg = res.identifier;
          issueNum++;
          prefs.putUInt("issueNum", issueNum);
          Serial.printf("Created: %s\n", res.identifier.c_str());
        } else {
          resultMsg = res.error;
          Serial.printf("Error: %s\n", res.error.c_str());
        }

        resultTime = millis();
        state = STATE_RESULT;
        Serial.println("-> RESULT");
      }
      break;
    }

    // ---- RESULT ----
    case STATE_RESULT:
      if (stateChanged) {
        displayResult(lastResultOk, resultMsg.c_str());
      }
      if (millis() - resultTime > RESULT_DISPLAY_MS) {
        state = STATE_IDLE;
        Serial.println("-> IDLE");
      }
      break;

    default:
      break;
  }

  delay(20);
}
