#pragma once

#include <M5Cardputer.h>

bool keyboardBackPressed(Keyboard_Class::KeysState &ks);
bool keyboardTextInputChar(Keyboard_Class::KeysState &ks, char c);
bool keyboardApplicationsShortcutPressed(Keyboard_Class::KeysState &ks);
bool keyboardOptionsShortcutPressed(Keyboard_Class::KeysState &ks);
const char *keyboardApplicationsShortcutLabel();
const char *keyboardOptionsShortcutLabel();
void keyboardLoop();
