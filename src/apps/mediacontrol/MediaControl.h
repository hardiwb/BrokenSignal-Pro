#pragma once

#include <M5Cardputer.h>

void openMediaControlApp();
void drawMediaControlApp();
bool handleMediaControlAppInput(Keyboard_Class::KeysState &keys);
void tickMediaControlApp();

void mediaControlDisconnect();

// Connection and pairing overlays own all matrix input. Once ready, the
// control list behaves like a normal host surface.
bool mediaControlModalActive();
