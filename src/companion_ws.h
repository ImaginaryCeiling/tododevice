#pragma once
#include <Arduino.h>

void companionSetup();
void companionLoop();
bool companionConnected();

void companionStartRecording();
void companionStopRecording();

String companionGetTranscript();
bool   companionHasFinal();
void   companionClearTranscript();
