#pragma once

#include <Arduino.h>

void loadSettings();
void saveSettings();
void drawAll();
void showHdrMsg(const char *msg);
void setTheme(uint8_t idx);
void toggleScreen();
void wakeScreen();
