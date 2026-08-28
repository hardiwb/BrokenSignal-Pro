#pragma once

#include "core/App.h"

extern const HelpEntry CALCULATOR_HELP_ENTRIES[];
extern const uint8_t CALCULATOR_HELP_COUNT;

void buildCalculatorOptions(std::vector<AppOption> &options);
