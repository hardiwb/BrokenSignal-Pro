#pragma once

#include <Arduino.h>

void loadSettings();
void saveSettings();
void drawAll();
void drawCurrentScreen();
void drawMusicApp();
void showHdrMsg(const char *msg);
void showVolumeMessage();
void adjustSystemVolume(int direction);
void adjustSystemBrightness(int direction);
void setTheme(uint8_t idx);
void toggleScreen();
void wakeScreen();
void enterDeepSleep();
