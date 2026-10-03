#pragma once

#include "core/App.h"

extern const HelpEntry MEDIA_CONTROL_HELP_ENTRIES[];
extern const uint8_t MEDIA_CONTROL_HELP_COUNT;

void buildMediaControlOptions(std::vector<AppOption> &options);
