#pragma once

#include <M5Cardputer.h>

void openMacroPadApp();
void drawMacroPadApp();
bool handleMacroPadAppInput(Keyboard_Class::KeysState &keys);
void tickMacroPadApp();

bool macroPadBindingsEditable();
bool macroPadCanAddBinding();
bool macroPadHasSelectedBinding();
bool macroPadCanMoveSelectedBinding(int direction);
void macroPadRequestAddBinding();
void macroPadRequestChangeKey();
void macroPadRequestDeleteBinding();
void macroPadMoveSelectedBinding(int direction);
String macroPadStarterInputLabel();
void macroPadAdjustStarterInput(int direction);

// True while Macro Pad owns the keyboard matrix for its BLE HID session.
bool macroPadModalActive();
