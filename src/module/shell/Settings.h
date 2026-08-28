#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

void enterSettingsMenu();
void exitSettingsMenu();
void cancelSettingsInputOverlay();
void drawSettingsMenu();
void handleSettingsInput(Keyboard_Class::KeysState &ks);
bool settingsInputOverlayActive();
