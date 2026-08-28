#include "apps/notes/NotesMetadata.h"

#include "apps/notes/Notes.h"

const HelpEntry NOTES_HELP_ENTRIES[] = {
    {"[Opt]", "Toggle Options"},
    {"[Alt]", "Toggle Applications"},
    {"[Ctrl]", "Toggle Control Panel"},
    {"[A]", "Add note"},
    {"[C]", "Quick calculator"},
    {"[R]", "Remove note"},
    {"[X]", "Toggle done"},
    {"[Ok]", "Edit note"},
    {"[;/.]", "Cursor up / down"},
    {"[Tab]", "Switch editor field"},
    {"[Fn arrows]", "Cursor / editor date"},
    {"[T]", "Today"},
    {"[U/B]", "Top / bottom"},
    {"[Esc]", "Applications"},
    {"[N]", "Close notes"},
    {"[H]", "Close help"},
};

const uint8_t NOTES_HELP_COUNT =
    sizeof(NOTES_HELP_ENTRIES) / sizeof(NOTES_HELP_ENTRIES[0]);

namespace
{
void adjustFilter(int direction)
{
    notesAdjustFilter(direction);
}

void moveTomorrow(int)
{
    notesMoveSelectedToTomorrow();
}

void moveToDate(int)
{
    notesPromptMoveSelectedToDate();
}

void editNote(int)
{
    notesEditSelected();
}

void deleteNote(int)
{
    notesDeleteSelected();
}

void newNote(int)
{
    notesNew();
}
} // namespace

void buildNotesOptions(std::vector<AppOption> &options)
{
    const bool hasNote = notesHasSelection();
    options.push_back({"Filter", notesFilterLabel(), true, true, false, adjustFilter});
    options.push_back({"Move to Tomorrow", "", hasNote, false, true, moveTomorrow});
    options.push_back({"Move to Date", "", hasNote, false, true, moveToDate});
    options.push_back({"Edit Note", "", hasNote, false, true, editNote});
    options.push_back({"Delete Note", "", hasNote, false, true, deleteNote});
    options.push_back({"New Note", "", true, false, true, newNote});
}
