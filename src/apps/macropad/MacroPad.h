#pragma once

#include <M5Cardputer.h>

void openMacroPadApp();
void drawMacroPadApp();
bool handleMacroPadAppInput(Keyboard_Class::KeysState &keys);
void tickMacroPadApp();

// True while Macro Pad owns the keyboard matrix for its BLE HID session.
bool macroPadModalActive();
