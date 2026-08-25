#pragma once

#include <M5Cardputer.h>

void openCalculator();
void closeCalculator();
void drawCalculator();
bool calculatorInputActive();
void handleCalculatorInput(Keyboard_Class::KeysState &ks);
