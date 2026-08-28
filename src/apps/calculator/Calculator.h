#pragma once

#include <M5Cardputer.h>

void openCalculator();
void openCalculatorHistory();
void closeCalculator();
void drawCalculator();
bool calculatorInputActive();
bool calculatorOverlayActive();
bool calculatorEditActive();
void cancelCalculatorEdit();
void refreshCalculatorFormatting();
void handleCalculatorInput(Keyboard_Class::KeysState &ks);
