#include "apps/notes/Notes.h"

#include "apps/notes/NotesInternal.h"
#include <SD.h>
#include <time.h>
#include <vector>

#include "core/Config.h"
#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/shell/Help.h"
#include "module/service/Clock.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"
#include "UI/Themes.h"

void drawNotes();
void drawNotesEditor(bool inputOnly);

namespace NotesInternal
{
void loadNote();
void drawNotesList();
bool parseDateKey(const String &dateKey, struct tm &date);
String formatDateKey(const struct tm &date);

std::vector<NoteEntry> noteEntries;
std::vector<int> visibleNoteIndices;

String currentMonth;
String currentDay;
String notePath;
String noteEditText;
String noteEditDate;

bool initialized = false;
bool noteEditorVisible = false;
bool noteEditorDateInvalid = false;
int noteEditorField = 0;
int noteEditTextCursor = 0;
int noteEditDateCursor = 0;
bool noteMoveDateVisible = false;
bool noteMoveDateInvalid = false;
String noteMoveDateInput;
int noteEditIndex = -1;
NotesViewMode notesViewMode = NotesViewMode::Day;
int notesDayOffset = 0;
int notesMonthOffset = 0;

bool getViewedTime(struct tm &viewed)
{
    if (!getCurrentTime(viewed))
    {
        memset(&viewed, 0, sizeof(viewed));
        viewed.tm_year = 100;
        viewed.tm_mon = 0;
        viewed.tm_mday = 1;
        viewed.tm_hour = 0;
        viewed.tm_min = 0;
        viewed.tm_sec = 0;
        viewed.tm_isdst = -1;
        return false;
    }

    if (notesViewMode == NotesViewMode::Day)
    {
        viewed.tm_mday += notesDayOffset;
    }
    else
    {
        viewed.tm_mon += notesMonthOffset;
    }

    viewed.tm_isdst = -1;
    mktime(&viewed);
    return true;
}

String viewedDateKey()
{
    struct tm viewed{};
    getViewedTime(viewed);

    char buf[16];
    snprintf(
        buf,
        sizeof(buf),
        "%04d-%02d-%02d",
        viewed.tm_year + 1900,
        viewed.tm_mon + 1,
        viewed.tm_mday);

    return String(buf);
}

String viewedMonthKey()
{
    struct tm viewed{};
    getViewedTime(viewed);

    char buf[8];
    snprintf(
        buf,
        sizeof(buf),
        "%04d-%02d",
        viewed.tm_year + 1900,
        viewed.tm_mon + 1);

    return String(buf);
}

void updateViewedDateCache()
{
    struct tm viewed{};
    getViewedTime(viewed);
    currentDay = viewedDateKey();
    currentMonth = viewedMonthKey();
}

String getDisplayDateString()
{
    struct tm viewed{};
    if (!getViewedTime(viewed))
        return "DATE UNAVAILABLE";

    if (notesViewMode == NotesViewMode::Month)
    {
        static const char *monthNames[] = {
            "Jan",
            "Feb",
            "Mar",
            "Apr",
            "May",
            "Jun",
            "Jul",
            "Aug",
            "Sep",
            "Oct",
            "Nov",
            "Dec"};

        return String(monthNames[viewed.tm_mon]) + " " + String(viewed.tm_year + 1900);
    }

    char prefix[24];
    strftime(prefix, sizeof(prefix), "%A, %b ", &viewed);
    return String(prefix) + String(viewed.tm_mday) + " " +
           String(viewed.tm_year + 1900);
}

String getEntryStamp()
{
    struct tm viewed{};
    getViewedTime(viewed);

    char buf[20];
    snprintf(
        buf,
        sizeof(buf),
        "%04d-%02d-%02d %02d:%02d",
        viewed.tm_year + 1900,
        viewed.tm_mon + 1,
        viewed.tm_mday,
        viewed.tm_hour,
        viewed.tm_min);

    return String(buf);
}

bool parseNoteStamp(
    const String &stamp,
    String &dayKey,
    String &monthKey)
{
    if (stamp.length() < 7)
        return false;

    monthKey = stamp.substring(0, 7);
    if (stamp.length() >= 10)
        dayKey = stamp.substring(0, 10);
    else
        dayKey = stamp;

    return monthKey.length() == 7 && dayKey.length() >= 7;
}

bool noteMatchesView(const NoteEntry &entry)
{
    String dayKey;
    String monthKey;
    if (!parseNoteStamp(entry.stamp, dayKey, monthKey))
        return false;

    if (notesViewMode == NotesViewMode::Month)
        return monthKey == currentMonth;

    return dayKey == currentDay;
}

String formatEntryLabel(const NoteEntry &entry)
{
    if (notesViewMode == NotesViewMode::Month && entry.stamp.length() >= 10)
        return entry.stamp.substring(5, 10) + " " + entry.text;

    return entry.text;
}

void rebuildVisibleNoteIndices()
{
    visibleNoteIndices.clear();

    for (int i = 0; i < (int)noteEntries.size(); i++)
    {
        if (noteMatchesView(noteEntries[i]))
            visibleNoteIndices.push_back(i);
    }
}

int visibleNoteCount()
{
    return (int)visibleNoteIndices.size();
}

int noteEntryIndexFromVisible(int visibleIndex)
{
    if (visibleIndex < 0 || visibleIndex >= visibleNoteCount())
        return -1;

    return visibleNoteIndices[visibleIndex];
}

void resetNotesCursor()
{
    notesSelected = 0;
    notesScrollTop = 0;
    notesMarqueeStartMs = millis();
}

void redrawQuickNoteEditor()
{
    if (!noteEditorVisible)
        return;

    drawNotesEditor(true);
}

void shiftQuickNoteDate(int delta)
{
    struct tm date{};
    if (!parseDateKey(noteEditDate, date))
        return;

    date.tm_mday += delta;
    mktime(&date);
    noteEditDate = formatDateKey(date);
    noteEditDateCursor = min(noteEditDateCursor, (int)noteEditDate.length());
    noteEditorDateInvalid = false;
    redrawQuickNoteEditor();
}

void selectTopNote()
{
    if (visibleNoteCount() == 0)
        return;

    notesSelected = 0;
    notesScrollTop = 0;
    notesMarqueeStartMs = millis();
    drawNotesList();
}

void selectBottomNote()
{
    int count = visibleNoteCount();
    if (count == 0)
        return;

    notesSelected = count - 1;
    if (notesSelected >= LIST_VISIBLE_ITEM)
        notesScrollTop = notesSelected - LIST_VISIBLE_ITEM + 1;
    else
        notesScrollTop = 0;
    notesMarqueeStartMs = millis();
    drawNotesList();
}

void setNotesViewMode(NotesViewMode mode, bool redraw = true)
{
    notesViewMode = mode;
    if (mode == NotesViewMode::Day)
        notesDayOffset = 0;
    else
        notesMonthOffset = 0;

    resetNotesCursor();
    loadNote();
    if (redraw)
        drawNotes();
}

void shiftNotesView(int delta)
{
    if (notesViewMode == NotesViewMode::Day)
        notesDayOffset += delta;
    else
        notesMonthOffset += delta;

    resetNotesCursor();
    loadNote();
    drawNotes();
}

void jumpToToday()
{
    notesViewMode = NotesViewMode::Day;
    notesDayOffset = 0;
    notesMonthOffset = 0;
    resetNotesCursor();
    loadNote();
    drawNotes();
}

void clampNotesSelection()
{
    rebuildVisibleNoteIndices();

    if (visibleNoteIndices.empty())
    {
        notesSelected = 0;
        notesScrollTop = 0;
        return;
    }

    notesSelected = max(0, min(notesSelected, visibleNoteCount() - 1));
    if (notesSelected < notesScrollTop)
        notesScrollTop = notesSelected;
    if (notesSelected >= notesScrollTop + LIST_VISIBLE_ITEM)
        notesScrollTop = notesSelected - LIST_VISIBLE_ITEM + 1;
    notesScrollTop = max(0, notesScrollTop);
}


void beginNoteEditor(int editIndex)
{
    noteEditIndex = editIndex;
    noteEditText = "";
    noteEditDate = viewedDateKey();
    int noteIndex = noteEntryIndexFromVisible(editIndex);
    if (noteIndex >= 0 && noteIndex < (int)noteEntries.size())
    {
        noteEditText = noteEntries[noteIndex].text;
        String monthKey;
        parseNoteStamp(noteEntries[noteIndex].stamp, noteEditDate, monthKey);
    }

    noteEditorField = 0;
    noteEditTextCursor = noteEditText.length();
    noteEditDateCursor = noteEditDate.length();
    noteEditorDateInvalid = false;
    noteEditorVisible = true;
    drawNotesEditor();
}

void saveNoteEditor()
{
    noteEditText.trim();
    struct tm parsedDate{};
    if (!parseDateKey(noteEditDate, parsedDate))
    {
        noteEditorDateInvalid = true;
        noteEditorField = 1;
        noteEditDateCursor = noteEditDate.length();
        drawNotesEditor();
        return;
    }

    bool saved = true;
    if (noteEditText.length() > 0)
    {
        int noteIndex = noteEntryIndexFromVisible(noteEditIndex);
        if (noteIndex >= 0 && noteIndex < (int)noteEntries.size())
        {
            noteEntries[noteIndex].text = noteEditText;
            noteEditorVisible = false;
            saved = moveSelectedNoteToDate(noteEditDate);
        }
        else
        {
            NoteEntry entry;
            entry.stamp = noteEditDate + getEntryStamp().substring(10);
            entry.done = false;
            entry.text = noteEditText;
            saved = appendEntryToMonth(entry, noteEditDate);
            if (saved)
                loadNote();
        }
    }

    if (!saved)
    {
        noteEditorVisible = true;
        drawNotesEditor();
        return;
    }

    noteEditorVisible = false;
    noteEditorDateInvalid = false;
    noteEditIndex = -1;

    if (notesMode)
        drawNotes();
    else
        drawAll();
}

void cancelNoteEditorImpl()
{
    noteEditorVisible = false;
    noteEditorDateInvalid = false;
    noteEditIndex = -1;

    if (notesMode)
        drawNotes();
    else
        drawAll();
}

static void cancelNotesMoveDateInputImpl()
{
    noteMoveDateVisible = false;
    noteMoveDateInvalid = false;

    if (notesMode)
        drawNotes();
    else
        drawAll();
}

} // namespace NotesInternal

using namespace NotesInternal;

void cancelNoteEditor()
{
    cancelNoteEditorImpl();
}

void cancelNotesMoveDateInput()
{
    cancelNotesMoveDateInputImpl();
}

String notesFilterLabel()
{
    return notesViewMode == NotesViewMode::Day ? "Day" : "Month";
}

void notesAdjustFilter(int direction)
{
    const int current = notesViewMode == NotesViewMode::Day ? 0 : 1;
    const int next = (current + direction + 2) % 2;
    setNotesViewMode(next == 0 ? NotesViewMode::Day : NotesViewMode::Month, false);
}

void notesBegin()
{
    if (initialized)
        return;

    Serial.println("Notes: begin");
    initialized = true;
    loadNote();
}

void notesOpen()
{
    calculatorVisible = false;
    calculatorOverlayMode = false;
    rememberLastOpenedApp(HostApp::Notes);
    notesBegin();
    notesViewMode = NotesViewMode::Day;
    notesDayOffset = 0;
    notesMonthOffset = 0;
    loadNote();

    notesMode = true;
    noteEditorVisible = false;
    noteMoveDateVisible = false;
    resetNotesCursor();
    clampNotesSelection();

    Serial.println("Notes: OPEN");
    drawNotes();
}

void notesQuickOpen()
{
    notesBegin();
    notesViewMode = NotesViewMode::Day;
    notesDayOffset = 0;
    notesMonthOffset = 0;
    loadNote();

    notesMode = false;
    resetNotesCursor();
    clampNotesSelection();

    Serial.println("Notes: QUICK");
    beginNoteEditor(-1);
}

void notesNew()
{
    notesBegin();
    beginNoteEditor(-1);
}

void notesClose()
{
    rememberLastOpenedApp(webRadioMode ? HostApp::Radio : HostApp::Music);
    notesMode = false;
    noteEditorVisible = false;
    noteMoveDateVisible = false;
    noteEditIndex = -1;

    Serial.println("Notes: CLOSE");

    drawAll();
}

void drawNotes()
{
    if (!notesMode)
        return;

    clampNotesSelection();
    drawNotesHeader();
    drawNotesList();
    drawNotesFooter();
}

void drawNotesEditor(bool inputOnly)
{
    OverlayModel model;
    model.type = OverlayType::TwoFieldInput;
    model.title = noteEditorDateInvalid
                      ? "Invalid Date"
                      : noteEditIndex >= 0 ? "Edit Entry" : "New Entry";
    model.value = noteEditText;
    model.secondValue = noteEditDate;
    model.activeField = noteEditorField;
    model.cursorIndex = noteEditorField == 0
                            ? noteEditTextCursor
                            : noteEditDateCursor;
    model.helperText = "Fn Up/Down +/- date";
    model.confirmText = "[Esc]Close [Tab]Field [Ok]Save";
    if (inputOnly)
        drawOverlayTwoFieldInputValues(model);
    else
        drawOverlay(model);
}

void drawNotesMoveDateEditor()
{
    OverlayModel model;
    model.type = OverlayType::TextInput;
    model.title = noteMoveDateInvalid ? "Invalid Date" : "Move to Date";
    model.prompt = "YYYY-MM-DD";
    model.value = noteMoveDateInput;
    model.confirmText = "[Esc]Close   [Ok]Save";
    drawOverlay(model);
}

bool notesInputActive()
{
    return notesMode || noteEditorVisible || noteMoveDateVisible;
}

bool notesEditorVisible()
{
    return noteEditorVisible;
}

bool notesMoveDateInputActive()
{
    return noteMoveDateVisible;
}

bool notesHasSelection()
{
    return noteEntryIndexFromVisible(notesSelected) >= 0;
}

void notesMoveSelectedToTomorrow()
{
    int noteIndex = noteEntryIndexFromVisible(notesSelected);
    if (noteIndex < 0 || noteIndex >= (int)noteEntries.size())
    {
        drawNotes();
        return;
    }

    String dayKey;
    String monthKey;
    struct tm date{};
    if (!parseNoteStamp(noteEntries[noteIndex].stamp, dayKey, monthKey) ||
        !parseDateKey(dayKey, date))
    {
        drawNotes();
        return;
    }

    date.tm_mday += 1;
    mktime(&date);
    moveSelectedNoteToDate(formatDateKey(date));
}

void notesPromptMoveSelectedToDate()
{
    int noteIndex = noteEntryIndexFromVisible(notesSelected);
    if (noteIndex < 0 || noteIndex >= (int)noteEntries.size())
    {
        drawNotes();
        return;
    }

    String monthKey;
    parseNoteStamp(noteEntries[noteIndex].stamp, noteMoveDateInput, monthKey);
    noteMoveDateInvalid = false;
    noteMoveDateVisible = true;
    drawNotesMoveDateEditor();
}

void notesEditSelected()
{
    if (notesHasSelection())
        beginNoteEditor(notesSelected);
    else
        drawNotes();
}

void notesDeleteSelected()
{
    removeSelectedNote();
}


void notesLoop()
{
    static char lastClock[6] = "";
    static unsigned long lastMarqueeDrawMs = 0;

    if (!notesInputActive())
        return;

    const bool notesScreenVisible =
        notesMode && !noteEditorVisible && !noteMoveDateVisible &&
        !optionsMenuVisible && !applicationsMenuVisible &&
        !settingsMenuVisible && !helpVisible &&
        !debugOverlayVisible && !calculatorVisible;
    if (!notesScreenVisible)
        return;

    char clockBuf[6];
    formatClock(clockBuf, sizeof(clockBuf));

    if (strcmp(clockBuf, lastClock) != 0)
    {
        strcpy(lastClock, clockBuf);

        if (notesMode)
            drawHeaderClock(String(clockBuf));
    }

    if (visibleNoteCount() == 0)
        return;

    if (millis() - lastMarqueeDrawMs < 80)
        return;

    lastMarqueeDrawMs = millis();

    ListModel model =
        buildNotesListModel();

    drawListRow(
        model,
        notesSelected);
}
