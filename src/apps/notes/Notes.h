#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

#include "module/service/EspNowNotes.h"

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
bool notesSyncResultActive();
void cancelNoteEditor();
void cancelNotesMoveDateInput();
void closeNotesSyncResult();
bool notesHasSelection();
String notesFilterLabel();
void notesAdjustFilter(int direction);
void notesMoveSelectedToTomorrow();
void notesPromptMoveSelectedToDate();
void notesMovePastIncompleteToToday();
void notesEditSelected();
void notesDeleteSelected();
void notesToggleSelectedCategory();
String notesSelectedCategoryLabel();
// Day view: viewed day. Month view: selected note's day.
void notesSendViewedDayToXteink(bool includeCompleted = false);
String notesCalendarSyncScopeLabel();
void notesAdjustCalendarSyncScope(int direction);
void notesSyncCalendarToXteink();
void notesSyncWithNotion();
bool notesResumePendingWifiAction();
bool notesCancelPendingWifiAction();
bool notesCalendarSyncActive();
void cancelNotesCalendarSync();
void tickNotesCalendarSync(EspNowNotesResult result);
void handleNotesInput(Keyboard_Class::KeysState &keys);
