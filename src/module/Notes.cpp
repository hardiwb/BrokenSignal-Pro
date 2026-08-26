#include "module/Notes.h"

#include <SD.h>
#include <time.h>
#include <vector>

#include "core/Config.h"
#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/Help.h"
#include "module/service/Clock.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"
#include "UI/Themes.h"

void drawNotes();
void drawNotesEditor();

namespace
{
void loadNote();
void drawNotesList();

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

bool initialized = false;
bool noteEditorVisible = false;
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

    char buf[32];
    strftime(buf, sizeof(buf), "%a, %b %d %Y", &viewed);
    return String(buf);
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

    drawNotesEditor();
}

void shiftQuickNoteDate(int delta)
{
    notesDayOffset += delta;
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

void setNotesViewMode(NotesViewMode mode)
{
    notesViewMode = mode;
    if (mode == NotesViewMode::Day)
        notesDayOffset = 0;
    else
        notesMonthOffset = 0;

    resetNotesCursor();
    loadNote();
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
    int noteIndex = noteEntryIndexFromVisible(editIndex);
    if (noteIndex >= 0 && noteIndex < (int)noteEntries.size())
        noteEditText = noteEntries[noteIndex].text;

    noteEditorVisible = true;
    drawNotesEditor();
}

void saveNoteEditor()
{
    noteEditText.trim();
    if (noteEditText.length() > 0)
    {
        int noteIndex = noteEntryIndexFromVisible(noteEditIndex);
        if (noteIndex >= 0 && noteIndex < (int)noteEntries.size())
        {
            noteEntries[noteIndex].text = noteEditText;
            noteEntries[noteIndex].stamp = getEntryStamp();
        }
        else
        {
            NoteEntry entry;
            entry.stamp = getEntryStamp();
            entry.done = false;
            entry.text = noteEditText;
            noteEntries.push_back(entry);
        }

        clampNotesSelection();
        saveNote();
    }

    noteEditorVisible = false;
    noteEditIndex = -1;

    if (notesMode)
        drawNotes();
    else
        drawAll();
}

void cancelNoteEditor()
{
    noteEditorVisible = false;
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
    model.mode = "NOTES";
    model.title = getDisplayDateString();
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
    model.left = "[A]Add [R]Rm [X]Done";
    model.center = notesViewMode == NotesViewMode::Day
                       ? "[D]Day [M]Mon"
                       : "[D]Day [M]Mon";
    model.battery = footerBatteryText();
    drawFooter(model);
}
} // namespace

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

void notesClose()
{
    notesMode = false;
    noteEditorVisible = false;
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

void drawNotesEditor()
{
    OverlayModel model;
    model.type = OverlayType::TextInput;
    model.title = getDisplayDateString();
    model.prompt = "";
    model.value = editorDisplayText();
    model.helperText = "[Fn+,]Prev [Fn+/]Next";
    model.confirmText = notesMode
                            ? "[Esc]Close [Ent]Save"
                            : "[Esc]Close [Ctrl+N]Open note [Ent]Save";
    model.tallInput = true;
    drawOverlay(model);
}

bool notesInputActive()
{
    return notesMode || noteEditorVisible;
}

bool notesEditorVisible()
{
    return noteEditorVisible;
}

void handleNotesInput(Keyboard_Class::KeysState &ks)
{
    if (!notesInputActive())
        return;

    if (noteEditorVisible)
    {
        if (ks.fn)
        {
            for (auto c : ks.word)
            {
                if (c == ',')
                {
                    shiftQuickNoteDate(-1);
                    return;
                }

                if (c == '/')
                {
                    shiftQuickNoteDate(1);
                    return;
                }
            }
        }

        if (keyboardBackPressed(ks))
        {
            cancelNoteEditor();
            return;
        }

        if (!notesMode && ks.ctrl)
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

        if (!notesMode && ks.opt)
        {
            noteEditorVisible = false;
            notesOpen();
            return;
        }

        if (ks.enter)
        {
            saveNoteEditor();
            return;
        }

        if (ks.del)
        {
            if (noteEditText.length() > 0)
            {
                noteEditText.remove(noteEditText.length() - 1);
                drawOverlayInputValue(editorDisplayText());
            }
            return;
        }

        for (auto c : ks.word)
        {
            if (keyboardTextInputChar(ks, c) && noteEditText.length() < RADIO_INPUT_MAX)
            {
                noteEditText += c;
                drawOverlayInputValue(editorDisplayText());
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

        case 'd':
        case 'D':
            setNotesViewMode(NotesViewMode::Day);
            return;

        case 'm':
        case 'M':
            setNotesViewMode(NotesViewMode::Month);
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

    char clockBuf[6];
    formatClock(clockBuf, sizeof(clockBuf));

    if (strcmp(clockBuf, lastClock) != 0)
    {
        strcpy(lastClock, clockBuf);

        if (notesMode)
            drawHeaderClock(String(clockBuf));
    }

    if (!notesMode ||
        noteEditorVisible ||
        helpVisible ||
        visibleNoteCount() == 0)
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
