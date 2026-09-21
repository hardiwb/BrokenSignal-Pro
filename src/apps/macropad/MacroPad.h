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

// True only while Macro Pad is showing a connection or binding-editor modal.
// The ready macro list is a normal host surface so shell and hardware hotkeys
// remain available.
bool macroPadModalActive();
