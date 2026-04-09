#pragma once
#include <Arduino.h>

void displaySetup();
void displayBoot(const char* line1, const char* line2 = nullptr);
void displayIdle(uint32_t issueNum, const char* teamName);
void displayRecordingBegin();
void displayRecordingText(const char* text);
void displayConfigBegin(uint32_t issueNum);
void displayConfigValue(int field, const char* value);
void displaySubmitting();
void displayResult(bool success, const char* message);
