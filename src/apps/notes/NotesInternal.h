#pragma once

#include <Arduino.h>
#include <vector>

#include "UI/List.h"

namespace NotesInternal
{
enum class NotesViewMode
{
    Day,
    Month
};

struct NoteEntry
{
    String stamp;
    bool done = false;
    String id;
    String category = "Personal";
    String syncHash;
    String text;
};

extern std::vector<NoteEntry> noteEntries;
extern std::vector<int> visibleNoteIndices;
extern String currentMonth;
extern String currentDay;
extern String notePath;
extern String noteEditText;
extern String noteEditDate;
extern String noteMoveDateInput;
extern NotesViewMode notesViewMode;
extern bool noteEditorVisible;
extern bool noteEditorDateInvalid;
extern bool noteMoveDateVisible;
extern bool noteMoveDateInvalid;
extern int noteEditorField;
extern int noteEditTextCursor;
extern int noteEditDateCursor;

String getDisplayDateString();
String getEntryStamp();
String formatEntryLabel(const NoteEntry &entry);
int visibleNoteCount();
int noteEntryIndexFromVisible(int visibleIndex);
void updateViewedDateCache();
void resetNotesCursor();
void shiftNotesView(int delta);
void clampNotesSelection();

void loadNote();
void saveNote();
bool parseNoteStorageLine(const String &line, NoteEntry &entry);
String createNoteId();
String noteContentHash(const NoteEntry &entry);
void removeSelectedNote();
bool parseDateKey(const String &dateKey, struct tm &date);
String formatDateKey(const struct tm &date);
bool appendEntryToMonth(const NoteEntry &entry, const String &dateKey);
bool moveSelectedNoteToDate(const String &dateKey);
bool shouldMovePastIncompleteNote(const NoteEntry &entry, const String &targetDate);
bool movePastIncompleteNotesToDate(const String &targetDate, size_t &movedCount);
void toggleSelectedNoteDone();
void toggleSelectedNoteCategory();
void toggleSelectedNoteArtCategory();
void cycleSelectedNoteCategory();
void changeNotesMonth(int delta);
void shiftQuickNoteDate(int delta);
void selectTopNote();
void selectBottomNote();
void jumpToToday();
void beginNoteEditor(int editIndex);
void saveNoteEditor();
void cancelNoteEditorImpl();

String editorDisplayText();
void drawNotesHeader();
ListModel buildNotesListModel();
void drawNotesList();
void redrawNotesSelection(int oldSelected, int oldScrollTop);
void drawNotesFooter();
} // namespace NotesInternal
