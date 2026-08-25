#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

void notesBegin();
void notesLoop();
void notesOpen();
void notesQuickOpen();
void notesClose();
void drawNotes();
void drawNotesEditor();
bool notesInputActive();
void handleNotesInput(Keyboard_Class::KeysState &ks);
