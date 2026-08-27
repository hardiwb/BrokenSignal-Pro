#pragma once

#include <M5Cardputer.h>

void openCalculator();
void openCalculatorHistory();
void closeCalculator();
void drawCalculator();
bool calculatorInputActive();
void refreshCalculatorFormatting();
void handleCalculatorInput(Keyboard_Class::KeysState &ks);
