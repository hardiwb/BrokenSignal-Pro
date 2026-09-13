#pragma once

#include "core/App.h"

extern const HelpEntry INFRARED_HELP_ENTRIES[];
extern const uint8_t INFRARED_HELP_COUNT;
void buildInfraredOptions(std::vector<AppOption> &options);
