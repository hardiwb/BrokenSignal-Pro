#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

void notesBegin();
void notesLoop();
void notesOpen();
void notesQuickOpen();
void notesNew();
void notesClose();
void drawNotes();
void drawNotesEditor(bool inputOnly = false);
void drawNotesMoveDateEditor();
bool notesInputActive();
bool notesEditorVisible();
bool notesMoveDateInputActive();
bool notesHasSelection();
String notesFilterLabel();
void notesAdjustFilter(int direction);
void notesMoveSelectedToTomorrow();
void notesPromptMoveSelectedToDate();
void notesEditSelected();
void notesDeleteSelected();
void handleNotesInput(Keyboard_Class::KeysState &ks);
