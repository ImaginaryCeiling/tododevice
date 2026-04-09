#include "companion_ws.h"
#include "config.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

static WebSocketsClient ws;
static bool wsConnected = false;
static String transcript = "";
static bool   hasFinal   = false;

static void onEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {

    case WStype_CONNECTED:
      wsConnected = true;
      Serial.println("Companion WS connected");
      break;

    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("Companion WS disconnected");
      break;

    case WStype_TEXT: {
      DynamicJsonDocument doc(1024);
      DeserializationError err = deserializeJson(doc, payload, length);
      if (err) {
        Serial.printf("WS JSON parse error: %s\n", err.c_str());
        break;
      }

      const char* msgType = doc["type"] | "";

      if (strcmp(msgType, "partial") == 0) {
        transcript = doc["text"].as<String>();
        hasFinal = false;
      } else if (strcmp(msgType, "final") == 0) {
        transcript = doc["text"].as<String>();
        hasFinal = true;
        Serial.printf("Final transcript: %s\n", transcript.c_str());
      } else if (strcmp(msgType, "error") == 0) {
        Serial.printf("Companion error: %s\n", doc["message"].as<const char*>());
      }
      break;
    }

    default:
      break;
  }
}

void companionSetup() {
  ws.begin(COMPANION_HOST, COMPANION_PORT, "/");
  ws.onEvent(onEvent);
  ws.setReconnectInterval(3000);
  Serial.printf("Companion WS -> %s:%d\n", COMPANION_HOST, COMPANION_PORT);
}

void companionLoop() {
  ws.loop();
}

bool companionConnected() {
  return wsConnected;
}

void companionStartRecording() {
  transcript = "";
  hasFinal = false;
  ws.sendTXT("{\"action\":\"start\"}");
  Serial.println("Sent start to companion");
}

void companionStopRecording() {
  ws.sendTXT("{\"action\":\"stop\"}");
  Serial.println("Sent stop to companion");
}

String companionGetTranscript() {
  return transcript;
}

bool companionHasFinal() {
  return hasFinal;
}

void companionClearTranscript() {
  transcript = "";
  hasFinal = false;
}
