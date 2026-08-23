#pragma once
#include "core/State.h"
void drawSplash(const char *statusLine);
void drawAll();
void drawHeader();
void drawTrackList();
void drawStatus();
void toggleHelp();
void drawHelp();
void drawOverlayFrame(const char *title);
void showHdrMsg(const char *msg);
void showToast();
void cycleRepeat();
void toggleShuffle();
void setTheme(uint8_t idx);
void toggleScreen();
void wakeScreen();
void enterSettingsMenu();
void exitSettingsMenu();
void drawSettingsMenu();
void handleSettingsInput(Keyboard_Class::KeysState &ks);
void toggleDebug();
void drawDebug();
void setTheme(uint8_t idx);
void drawMarquee(
    M5Canvas& c,
    const String& text,
    int x,
    int y,
    int width,
    uint32_t color,
    uint32_t startMs
);