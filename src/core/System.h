#pragma once

#include <Arduino.h>

void loadSettings();
void saveSettings();
void drawAll();
void drawCurrentScreen();
void showHdrMsg(const char *msg);
void showVolumeMessage();
void setTheme(uint8_t idx);
void toggleScreen();
void wakeScreen();
void enterDeepSleep();
