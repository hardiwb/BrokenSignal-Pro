#pragma once

#include "core/App.h"

extern const HelpEntry MACRO_PAD_HELP_ENTRIES[];
extern const uint8_t MACRO_PAD_HELP_COUNT;

void buildMacroPadOptions(std::vector<AppOption> &options);
