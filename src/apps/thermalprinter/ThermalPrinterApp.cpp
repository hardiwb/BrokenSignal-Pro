#include "apps/thermalprinter/ThermalPrinterApp.h"

#include "apps/thermalprinter/ThermalPrinter.h"

void openThermalPrinterApp()
{
    thermalPrinterOpen();
}

void drawThermalPrinterApp()
{
    drawThermalPrinter();
}

bool handleThermalPrinterAppInput(Keyboard_Class::KeysState &keys)
{
    handleThermalPrinterInput(keys);
    return true;
}

void tickThermalPrinterApp()
{
}
