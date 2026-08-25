#include "module/Notes.h"

#include <SD.h>
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
struct NoteEntry
{
    String stamp;
    String text;
};

std::vector<NoteEntry> noteEntries;

String currentMonth;
String notePath;
String noteEditText;

bool initialized = false;
bool noteEditorVisible = false;
int noteEditIndex = -1;
int notesMonthOffset = 0;

void viewedDateParts(
    int &year,
    int &month,
    int &day,
    int &hour,
    int &minute)
{
    struct tm now{};

    if (!getCurrentTime(now))
    {
        year = 2000;
        month = 1;
        day = 1;
        hour = 0;
        minute = 0;
        return;
    }

    int monthIndex =
        now.tm_mon +
        notesMonthOffset;

    year =
        now.tm_year +
        1900 +
        monthIndex / 12;

    monthIndex %= 12;
    if (monthIndex < 0)
    {
        monthIndex += 12;
        year--;
    }

    month = monthIndex + 1;
    day = now.tm_mday;
    hour = now.tm_hour;
    minute = now.tm_min;
}

String getMonthString()
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    viewedDateParts(
        year,
        month,
        day,
        hour,
        minute);

    char buf[8];
    snprintf(
        buf,
        sizeof(buf),
        "%04d-%02d",
        year,
        month);

    return String(buf);
}

String getDisplayDateString()
{
    struct tm now{};
    if (!getCurrentTime(now))
        return "DATE UNAVAILABLE";

    if (notesMonthOffset != 0)
    {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        viewedDateParts(
            year,
            month,
            day,
            hour,
            minute);

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

        return String(monthNames[month - 1]) + " " + String(year);
    }

    char buf[32];
    strftime(buf, sizeof(buf), "%a, %b %d %Y", &now);
    return String(buf);
}

String getEntryStamp()
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    viewedDateParts(
        year,
        month,
        day,
        hour,
        minute);

    char buf[20];
    snprintf(
        buf,
        sizeof(buf),
        "%04d-%02d-%02d %02d:%02d",
        year,
        month,
        day,
        hour,
        minute);

    return String(buf);
}

String formatEntryLabel(const NoteEntry &entry, int index)
{
    char prefix[4];
    snprintf(prefix, sizeof(prefix), "%02d", index + 1);

    return String(prefix) + " " + entry.text;
}

void clampNotesSelection()
{
    if (noteEntries.empty())
    {
        notesSelected = 0;
        notesScrollTop = 0;
        return;
    }

    notesSelected = max(0, min(notesSelected, (int)noteEntries.size() - 1));
    if (notesSelected < notesScrollTop)
        notesScrollTop = notesSelected;
    if (notesSelected >= notesScrollTop + VISIBLE_TRACKS)
        notesScrollTop = notesSelected - VISIBLE_TRACKS + 1;
    notesScrollTop = max(0, notesScrollTop);
}

void loadNote()
{
    noteEntries.clear();

    currentMonth = getMonthString();
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

        int sep = line.indexOf('|');
        NoteEntry entry;
        if (sep >= 0)
        {
            entry.stamp = line.substring(0, sep);
            entry.text = line.substring(sep + 1);
        }
        else
        {
            // Backward-compatible import for older daily/plain note files.
            entry.stamp = getEntryStamp();
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

    File f = SD.open(notePath, FILE_WRITE);
    if (!f)
    {
        showHdrMsg("NOTE SAVE FAIL");
        return;
    }

    for (const NoteEntry &entry : noteEntries)
        f.printf("%s|%s\n", entry.stamp.c_str(), entry.text.c_str());

    f.close();
}

void removeSelectedNote()
{
    if (noteEntries.empty())
        return;

    noteEntries.erase(noteEntries.begin() + notesSelected);
    clampNotesSelection();
    saveNote();
    drawNotes();
}

void changeNotesMonth(int delta)
{
    notesMonthOffset += delta;
    notesSelected = 0;
    notesScrollTop = 0;
    notesMarqueeStartMs = millis();
    loadNote();
    drawNotes();
}

void beginNoteEditor(int editIndex)
{
    noteEditIndex = editIndex;
    noteEditText = "";
    if (editIndex >= 0 && editIndex < (int)noteEntries.size())
        noteEditText = noteEntries[editIndex].text;

    noteEditorVisible = true;
    drawNotesEditor();
}

void saveNoteEditor()
{
    noteEditText.trim();
    if (noteEditText.length() > 0)
    {
        if (noteEditIndex >= 0 && noteEditIndex < (int)noteEntries.size())
            noteEntries[noteEditIndex].text = noteEditText;
        else
            noteEntries.push_back({getEntryStamp(), noteEditText});

        notesSelected = max(0, (int)noteEntries.size() - 1);
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

    if (noteEntries.empty())
    {
        ListItemModel item;
        item.label = "No notes this month";
        item.value = "A";
        item.type = ListItemType::Property;
        item.isSelected = true;
        model.items.push_back(item);
    }
    else
    {
        for (int i = 0; i < (int)noteEntries.size(); i++)
        {
            ListItemModel item;
            item.label = formatEntryLabel(noteEntries[i], i);
            item.type = ListItemType::Normal;
            item.isSelected = i == notesSelected;
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
    model.left = "[A]Add [R]Rm [Ent]Ed";
    model.center = "";
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
    notesMonthOffset = 0;
    loadNote();

    notesMode = true;
    noteEditorVisible = false;
    notesSelected = 0;
    notesScrollTop = 0;
    notesMarqueeStartMs = millis();

    Serial.println("Notes: OPEN");
    drawNotes();
}

void notesQuickOpen()
{
    notesBegin();
    notesMonthOffset = 0;
    loadNote();

    notesMode = false;
    notesSelected = 0;
    notesScrollTop = 0;
    notesMarqueeStartMs = millis();

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
    model.confirmText = notesMode
                            ? "[Esc]Close   [Ent]Save"
                            : "[Ctrl+N]Notes   [Ent]Save";
    model.tallInput = true;
    drawOverlay(model);
}

bool notesInputActive()
{
    return notesMode || noteEditorVisible;
}

void handleNotesInput(Keyboard_Class::KeysState &ks)
{
    if (!notesInputActive())
        return;

    if (noteEditorVisible)
    {
        if (keyboardBackPressed(ks))
        {
            cancelNoteEditor();
            return;
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

    if (ks.enter && !noteEntries.empty())
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

        case 'r':
        case 'R':
        case 'x':
        case 'X':
            removeSelectedNote();
            return;

        case ';':
            if (!noteEntries.empty())
            {
                int oldSelected =
                    notesSelected;
                int oldScrollTop =
                    notesScrollTop;

                notesSelected =
                    (notesSelected - 1 + (int)noteEntries.size()) %
                    (int)noteEntries.size();
                notesMarqueeStartMs = millis();
                redrawNotesSelection(
                    oldSelected,
                    oldScrollTop);
            }
            return;

        case '.':
            if (!noteEntries.empty())
            {
                int oldSelected =
                    notesSelected;
                int oldScrollTop =
                    notesScrollTop;

                notesSelected =
                    (notesSelected + 1) %
                    (int)noteEntries.size();
                notesMarqueeStartMs = millis();
                redrawNotesSelection(
                    oldSelected,
                    oldScrollTop);
            }
            return;

        case ',':
            changeNotesMonth(-1);
            return;

        case '/':
            changeNotesMonth(1);
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
            drawNotesHeader();
    }

    if (!notesMode ||
        noteEditorVisible ||
        helpVisible ||
        noteEntries.empty())
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
