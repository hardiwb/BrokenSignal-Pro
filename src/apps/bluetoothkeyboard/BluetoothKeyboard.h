#pragma once

#include <M5Cardputer.h>

void openBluetoothKeyboardApp();
void drawBluetoothKeyboardApp();
bool handleBluetoothKeyboardAppInput(Keyboard_Class::KeysState &keys);
void tickBluetoothKeyboardApp();

// True while the keyboard owns every physical key. Only BtnG0 can leave this
// mode; shell/global shortcuts must not inspect the matrix keyboard meanwhile.
bool bluetoothKeyboardModalActive();
