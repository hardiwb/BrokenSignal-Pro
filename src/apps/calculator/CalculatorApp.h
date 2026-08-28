#pragma once

#include "core/App.h"

void openCalculatorApp();
bool handleCalculatorAppInput(Keyboard_Class::KeysState &keys);
void tickCalculatorApp();
bool calculatorQuickAccessAvailable(HostApp foreground);
