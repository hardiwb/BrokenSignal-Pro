#pragma once

#include "core/App.h"

extern const HelpEntry THERMAL_PRINTER_HELP_ENTRIES[];
extern const uint8_t THERMAL_PRINTER_HELP_COUNT;

void buildThermalPrinterOptions(std::vector<AppOption> &options);
