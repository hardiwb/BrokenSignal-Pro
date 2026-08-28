#pragma once

#include "core/App.h"

void openNotesApp();
bool handleNotesAppInput(Keyboard_Class::KeysState &keys);
void tickNotesApp();
bool notesQuickAccessAvailable(HostApp foreground);
