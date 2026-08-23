#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

void notesBegin();
void notesLoop();
void notesDraw();
bool notesHandleKey(char key);
void notesOpen();
void notesClose();
void handleNotesInput(Keyboard_Class::KeysState &ks);