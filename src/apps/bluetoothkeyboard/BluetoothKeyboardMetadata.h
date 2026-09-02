#pragma once

#include "core/App.h"

extern const HelpEntry BLUETOOTH_KEYBOARD_HELP_ENTRIES[];
extern const uint8_t BLUETOOTH_KEYBOARD_HELP_COUNT;

void buildBluetoothKeyboardOptions(std::vector<AppOption> &options);
