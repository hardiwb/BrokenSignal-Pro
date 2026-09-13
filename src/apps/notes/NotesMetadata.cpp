#include "apps/notes/NotesMetadata.h"

#include "apps/notes/Notes.h"

const HelpEntry NOTES_HELP_ENTRIES[] = {
    {"[Opt]", "Toggle Options"},
    {"[Alt]", "Toggle Applications"},
    {"[Ctrl]", "Toggle Control Panel"},
    {"[N]", "Add note"},
    {"[C]", "Quick calculator"},
    {"[E]", "Quick expense"},
    {"[-/+]", "Volume"},
    {"[Del]", "Delete note"},
    {"[S]", "Send active to Xteink"},
    {"[Shift+S]", "Send all to Xteink"},
    {"[X]", "Toggle done"},
    {"[W]", "Toggle Personal / Work"},
    {"[A]", "Toggle Personal / Art"},
    {"[Ok]", "Edit note"},
    {"[;/.]", "Cursor up / down"},
    {"[,/]", "Previous / next date"},
    {"[Tab]", "Switch editor field"},
    {"[Fn arrows]", "Cursor / editor date"},
    {"[T]", "Today"},
    {"[U/B]", "Top / bottom"},
    {"[Esc]", "Applications"},
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

void toggleCategory(int)
{
    notesToggleSelectedCategory();
}

void syncCalendar(int)
{
    notesSyncCalendarToXteink();
}

void syncNotion(int)
{
    notesSyncWithNotion();
}
} // namespace

void buildNotesOptions(std::vector<AppOption> &options)
{
    const bool hasNote = notesHasSelection();
    const bool mutationsAllowed = !notesCalendarSyncActive();
    options.push_back({"Filter", notesFilterLabel(), true, true, false, adjustFilter});
    options.push_back({"Move to Tomorrow", "", hasNote && mutationsAllowed, false, true, moveTomorrow});
    options.push_back({"Move to Date", "", hasNote && mutationsAllowed, false, true, moveToDate});
    options.push_back({"Category", notesSelectedCategoryLabel(), hasNote && mutationsAllowed, false, true, toggleCategory});
    options.push_back({"Sync Notion", "", mutationsAllowed, false, true, syncNotion});
    options.push_back({"Sync Calendar", "", mutationsAllowed, false, true, syncCalendar});
}
