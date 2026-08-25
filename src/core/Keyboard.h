#pragma once

#include <M5Cardputer.h>

bool keyboardBackPressed(Keyboard_Class::KeysState &ks);
bool keyboardTextInputChar(Keyboard_Class::KeysState &ks, char c);
void keyboardLoop();
