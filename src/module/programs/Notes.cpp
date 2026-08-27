#include "module/programs/Notes.h"

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

namespace
{
void loadNote();
void drawNotesList();
bool parseDateKey(const String &dateKey, struct tm &date);
String formatDateKey(const struct tm &date);

enum class NotesViewMode
{
    Day,
    Month
};

struct NoteEntry
{
    String stamp;
    bool done = false;
    String text;
};

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

void loadNote()
{
    noteEntries.clear();

    updateViewedDateCache();
    notePath = "/Notes/" + currentMonth + ".txt";

    Serial.print("Notes: ");
    Serial.println(notePath);

    File f = SD.open(notePath, FILE_READ);
    if (!f)
    {
        Serial.println("Notes: no note for this month");
        clampNotesSelection();
        return;
    }

    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;

        NoteEntry entry;
        int firstSep = line.indexOf('|');
        int secondSep = firstSep >= 0 ? line.indexOf('|', firstSep + 1) : -1;

        if (firstSep >= 0 && secondSep >= 0)
        {
            entry.stamp = line.substring(0, firstSep);
            String status = line.substring(firstSep + 1, secondSep);
            entry.done = status.indexOf('x') >= 0 || status.indexOf('X') >= 0;
            entry.text = line.substring(secondSep + 1);
        }
        else if (firstSep >= 0)
        {
            entry.stamp = line.substring(0, firstSep);
            entry.done = false;
            entry.text = line.substring(firstSep + 1);
        }
        else
        {
            // Backward-compatible import for older daily/plain note files.
            entry.stamp = getEntryStamp();
            entry.done = false;
            entry.text = line;
        }

        entry.text.trim();
        if (entry.text.length() > 0)
            noteEntries.push_back(entry);
    }

    f.close();
    clampNotesSelection();

    Serial.print("Notes lines: ");
    Serial.println(noteEntries.size());
}

void saveNote()
{
    SD.mkdir("/Notes");
    SD.remove(notePath.c_str());

    File f = SD.open(notePath, FILE_WRITE);
    if (!f)
    {
        showHdrMsg("NOTE SAVE FAIL");
        return;
    }

    for (const NoteEntry &entry : noteEntries)
        f.printf("%s|%c|%s\n", entry.stamp.c_str(), entry.done ? 'x' : '-', entry.text.c_str());

    f.close();
}

void removeSelectedNote()
{
    if (visibleNoteCount() == 0)
        return;

    int noteIndex = noteEntryIndexFromVisible(notesSelected);
    if (noteIndex < 0)
        return;

    noteEntries.erase(noteEntries.begin() + noteIndex);
    clampNotesSelection();
    saveNote();
    drawNotes();
}

bool parseDateKey(const String &dateKey, struct tm &date)
{
    int year = 0;
    int month = 0;
    int day = 0;
    char trailing = '\0';

    if (dateKey.length() != 10 ||
        sscanf(dateKey.c_str(), "%d-%d-%d%c", &year, &month, &day, &trailing) != 3 ||
        year < 2000 || year > 2099 || month < 1 || month > 12 ||
        day < 1 || day > 31)
        return false;

    memset(&date, 0, sizeof(date));
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_mday = day;
    date.tm_hour = 12;
    date.tm_isdst = -1;

    if (mktime(&date) == (time_t)-1)
        return false;

    return date.tm_year == year - 1900 &&
           date.tm_mon == month - 1 &&
           date.tm_mday == day;
}

String formatDateKey(const struct tm &date)
{
    char value[11];
    snprintf(
        value,
        sizeof(value),
        "%04d-%02d-%02d",
        date.tm_year + 1900,
        date.tm_mon + 1,
        date.tm_mday);
    return String(value);
}

bool appendEntryToMonth(const NoteEntry &entry, const String &dateKey)
{
    String destinationMonth = dateKey.substring(0, 7);
    if (destinationMonth == currentMonth)
    {
        noteEntries.push_back(entry);
        saveNote();
        return true;
    }

    SD.mkdir("/Notes");
    String destinationPath = "/Notes/" + destinationMonth + ".txt";
    File destination = SD.open(destinationPath, FILE_APPEND);
    if (!destination)
        return false;

    destination.printf(
        "%s|%c|%s\n",
        entry.stamp.c_str(),
        entry.done ? 'x' : '-',
        entry.text.c_str());
    destination.close();
    return true;
}

bool moveSelectedNoteToDate(const String &dateKey)
{
    struct tm targetDate{};
    if (!parseDateKey(dateKey, targetDate))
        return false;

    int noteIndex = noteEntryIndexFromVisible(notesSelected);
    if (noteIndex < 0 || noteIndex >= (int)noteEntries.size())
        return false;

    NoteEntry moved = noteEntries[noteIndex];
    String timeSuffix = moved.stamp.length() > 10
                            ? moved.stamp.substring(10)
                            : getEntryStamp().substring(10);
    moved.stamp = dateKey + timeSuffix;

    String destinationMonth = dateKey.substring(0, 7);
    if (destinationMonth == currentMonth)
    {
        noteEntries[noteIndex] = moved;
        saveNote();
    }
    else
    {
        SD.mkdir("/Notes");
        String destinationPath = "/Notes/" + destinationMonth + ".txt";
        File destination = SD.open(destinationPath, FILE_APPEND);
        if (!destination)
            return false;

        destination.printf(
            "%s|%c|%s\n",
            moved.stamp.c_str(),
            moved.done ? 'x' : '-',
            moved.text.c_str());
        destination.close();

        noteEntries.erase(noteEntries.begin() + noteIndex);
        saveNote();
    }

    loadNote();
    clampNotesSelection();
    drawNotes();
    return true;
}

void toggleSelectedNoteDone()
{
    int noteIndex = noteEntryIndexFromVisible(notesSelected);
    if (noteIndex < 0)
        return;

    noteEntries[noteIndex].done = !noteEntries[noteIndex].done;
    saveNote();
    drawNotes();
}

void changeNotesMonth(int delta)
{
    shiftNotesView(delta);
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

void cancelNoteEditor()
{
    noteEditorVisible = false;
    noteEditorDateInvalid = false;
    noteEditIndex = -1;

    if (notesMode)
        drawNotes();
    else
        drawAll();
}

String editorDisplayText()
{
    String value = noteEditText;
    if ((int)value.length() > 116)
        value = value.substring((int)value.length() - 116);
    return value;
}

void drawNotesHeader()
{
    HeaderModel model;
    model.appHeaderTag = "NOTES";
    model.appHeaderTitle = getDisplayDateString();
    model.cursor = true;
    drawHeader(model);
}

ListModel buildNotesListModel()
{
    ListModel model;
    model.selected = notesSelected;
    model.scrollTop = notesScrollTop;
    model.marqueeStartMs = notesMarqueeStartMs;

    if (visibleNoteCount() == 0)
    {
        ListItemModel item;
        item.label = notesViewMode == NotesViewMode::Day
                         ? "No notes this day"
                         : "No notes this month";
        item.value = notesViewMode == NotesViewMode::Day ? "D" : "M";
        item.type = ListItemType::Property;
        item.isSelected = true;
        model.items.push_back(item);
    }
    else
    {
        for (int i = 0; i < visibleNoteCount(); i++)
        {
            int noteIndex = visibleNoteIndices[i];
            ListItemModel item;
            item.label = formatEntryLabel(noteEntries[noteIndex]);
            item.type = ListItemType::Normal;
            item.isSelected = i == notesSelected;
            item.isDimmed = noteEntries[noteIndex].done;
            model.items.push_back(item);
        }
    }

    return model;
}

void drawNotesList()
{
    drawList(
        buildNotesListModel());
}

void redrawNotesSelection(
    int oldSelected,
    int oldScrollTop)
{
    clampNotesSelection();

    if (notesScrollTop != oldScrollTop)
    {
        drawNotesList();
        return;
    }

    ListModel model =
        buildNotesListModel();

    drawListSelection(
        model,
        oldSelected,
        notesSelected);
}

void drawNotesFooter()
{
    FooterModel model;
    model.left = "[A]Ad [X]Done";
    model.center = "[Ok]Edit";
    model.battery = footerBatteryText();
    drawFooter(model);
}
} // namespace

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

void handleNotesInput(Keyboard_Class::KeysState &ks)
{
    if (!notesInputActive())
        return;

    if (noteMoveDateVisible)
    {
        if (keyboardBackPressed(ks))
        {
            noteMoveDateVisible = false;
            noteMoveDateInvalid = false;
            drawNotes();
            return;
        }

        if (ks.enter)
        {
            if (moveSelectedNoteToDate(noteMoveDateInput))
            {
                noteMoveDateVisible = false;
                noteMoveDateInvalid = false;
            }
            else
            {
                noteMoveDateInvalid = true;
                drawNotesMoveDateEditor();
            }
            return;
        }

        if (ks.del)
        {
            if (noteMoveDateInput.length() > 0)
                noteMoveDateInput.remove(noteMoveDateInput.length() - 1);
            noteMoveDateInvalid = false;
            drawOverlayInputValue(noteMoveDateInput);
            return;
        }

        bool changed = false;
        for (auto c : ks.word)
        {
            if (((c >= '0' && c <= '9') || c == '-') &&
                noteMoveDateInput.length() < 10)
            {
                noteMoveDateInput += c;
                changed = true;
            }
        }

        if (changed)
        {
            noteMoveDateInvalid = false;
            drawOverlayInputValue(noteMoveDateInput);
        }
        return;
    }

    if (noteEditorVisible)
    {
        if (ks.fn)
        {
            for (auto c : ks.word)
            {
                if (c == ';')
                {
                    shiftQuickNoteDate(-1);
                    return;
                }

                if (c == '.')
                {
                    shiftQuickNoteDate(1);
                    return;
                }

                if (c == ',')
                {
                    if (noteEditorField == 0)
                        noteEditTextCursor = max(0, noteEditTextCursor - 1);
                    else
                        noteEditDateCursor = max(0, noteEditDateCursor - 1);
                    drawNotesEditor(true);
                    return;
                }

                if (c == '/')
                {
                    if (noteEditorField == 0)
                        noteEditTextCursor = min((int)noteEditText.length(), noteEditTextCursor + 1);
                    else
                        noteEditDateCursor = min((int)noteEditDate.length(), noteEditDateCursor + 1);
                    drawNotesEditor(true);
                    return;
                }
            }
        }

        if (ks.tab)
        {
            noteEditorField = (noteEditorField + 1) % 2;
            drawNotesEditor(true);
            return;
        }

        if (keyboardBackPressed(ks))
        {
            cancelNoteEditor();
            return;
        }

        if (!notesMode && ks.fn)
        {
            for (auto c : ks.word)
            {
                if (c == 'n' || c == 'N')
                {
                    noteEditorVisible = false;
                    notesOpen();
                    return;
                }
            }
        }

        if (ks.enter)
        {
            saveNoteEditor();
            return;
        }

        if (ks.del)
        {
            String *value = noteEditorField == 0 ? &noteEditText : &noteEditDate;
            int *cursor = noteEditorField == 0 ? &noteEditTextCursor : &noteEditDateCursor;
            if (*cursor > 0 && value->length() > 0)
            {
                value->remove(*cursor - 1, 1);
                *cursor -= 1;
                noteEditorDateInvalid = false;
                drawNotesEditor(true);
            }
            return;
        }

        for (auto c : ks.word)
        {
            if (!keyboardTextInputChar(ks, c))
                continue;

            if (noteEditorField == 0 && noteEditText.length() < RADIO_INPUT_MAX)
            {
                noteEditText = noteEditText.substring(0, noteEditTextCursor) +
                               String(c) + noteEditText.substring(noteEditTextCursor);
                noteEditTextCursor += 1;
                drawNotesEditor(true);
            }
            else if (noteEditorField == 1 &&
                     ((c >= '0' && c <= '9') || c == '-') &&
                     noteEditDate.length() < 10)
            {
                noteEditDate = noteEditDate.substring(0, noteEditDateCursor) +
                               String(c) + noteEditDate.substring(noteEditDateCursor);
                noteEditDateCursor += 1;
                noteEditorDateInvalid = false;
                drawNotesEditor(true);
            }
        }
        return;
    }

    if (keyboardBackPressed(ks))
    {
        notesClose();
        return;
    }

    if (ks.enter && visibleNoteCount() > 0)
    {
        beginNoteEditor(notesSelected);
        return;
    }

    for (auto c : ks.word)
    {
        switch (c)
        {
        case 'n':
        case 'N':
            notesClose();
            return;

        case 'h':
        case 'H':
            toggleHelp();
            return;

        case 'a':
        case 'A':
            beginNoteEditor(-1);
            return;

        case 't':
        case 'T':
            jumpToToday();
            return;

        case 'u':
        case 'U':
            selectTopNote();
            return;

        case 'b':
        case 'B':
            selectBottomNote();
            return;

        case 'r':
        case 'R':
            removeSelectedNote();
            return;

        case 'x':
        case 'X':
            toggleSelectedNoteDone();
            return;

        case ';':
            if (visibleNoteCount() > 0)
            {
                int oldSelected =
                    notesSelected;
                int oldScrollTop =
                    notesScrollTop;

                notesSelected =
                    (notesSelected - 1 + visibleNoteCount()) %
                    visibleNoteCount();
                notesMarqueeStartMs = millis();
                redrawNotesSelection(
                    oldSelected,
                    oldScrollTop);
            }
            return;

        case '.':
            if (visibleNoteCount() > 0)
            {
                int oldSelected =
                    notesSelected;
                int oldScrollTop =
                    notesScrollTop;

                notesSelected =
                    (notesSelected + 1) %
                    visibleNoteCount();
                notesMarqueeStartMs = millis();
                redrawNotesSelection(
                    oldSelected,
                    oldScrollTop);
            }
            return;

        case ',':
            shiftNotesView(-1);
            return;

        case '/':
            shiftNotesView(1);
            return;
        }
    }
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
