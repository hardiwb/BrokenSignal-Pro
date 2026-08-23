#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

//==================================================
// SETTINGS
//==================================================

void enterSettingsMenu();
void handleSettingsInput(Keyboard_Class::KeysState &ks);
void drawSettings();

//==================================================
// SETTINGS STORAGE
//==================================================

void loadSettings();
void saveSettings();