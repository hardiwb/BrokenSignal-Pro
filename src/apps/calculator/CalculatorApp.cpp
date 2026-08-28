#include "apps/calculator/CalculatorApp.h"

#include "apps/calculator/Calculator.h"

void openCalculatorApp()
{
    openCalculatorHistory();
}

bool handleCalculatorAppInput(Keyboard_Class::KeysState &keys)
{
    handleCalculatorInput(keys);
    return true;
}

void tickCalculatorApp()
{
}

bool calculatorQuickAccessAvailable(HostApp foreground)
{
    return foreground != HostApp::Calculator;
}
