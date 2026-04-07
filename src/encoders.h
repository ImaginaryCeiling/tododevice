#pragma once
#include <Arduino.h>
#include "config.h"

void encodersSetup();
long encoderPosition(int i);
bool encoderSwitch(int i);
int  encoderIndex(int i, int optionCount);
void encoderReset(int i);
