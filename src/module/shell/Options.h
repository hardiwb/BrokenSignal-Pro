#pragma once

#include <M5Cardputer.h>

extern bool optionsMenuVisible;

void enterOptionsMenu();
void exitOptionsMenu();
void drawOptionsMenu();
void handleOptionsInput(Keyboard_Class::KeysState &ks);
