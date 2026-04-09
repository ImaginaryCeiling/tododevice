#pragma once

void networkSetup();   // blocking — connects to WiFi
bool networkConnected();
void networkCheck();   // call in loop — auto-reconnects
