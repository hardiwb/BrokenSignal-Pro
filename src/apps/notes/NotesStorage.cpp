#include "apps/notes/NotesInternal.h"

#include <SD.h>
#include <time.h>

#include "core/State.h"
#include "core/System.h"
#include "apps/notes/Notes.h"

namespace NotesInternal
{
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
        const int firstSep = line.indexOf('|');
        const int secondSep = firstSep >= 0 ? line.indexOf('|', firstSep + 1) : -1;
        if (firstSep >= 0 && secondSep >= 0)
        {
            entry.stamp = line.substring(0, firstSep);
            const String status = line.substring(firstSep + 1, secondSep);
            entry.done = status.indexOf('x') >= 0 || status.indexOf('X') >= 0;
            entry.text = line.substring(secondSep + 1);
        }
        else if (firstSep >= 0)
        {
            entry.stamp = line.substring(0, firstSep);
            entry.text = line.substring(firstSep + 1);
        }
        else
        {
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
    const int noteIndex = noteEntryIndexFromVisible(notesSelected);
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
        year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31)
        return false;

    memset(&date, 0, sizeof(date));
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_mday = day;
    date.tm_hour = 12;
    date.tm_isdst = -1;
    if (mktime(&date) == (time_t)-1)
        return false;
    return date.tm_year == year - 1900 && date.tm_mon == month - 1 && date.tm_mday == day;
}

String formatDateKey(const struct tm &date)
{
    char value[11];
    snprintf(value, sizeof(value), "%04d-%02d-%02d", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
    return String(value);
}

bool appendEntryToMonth(const NoteEntry &entry, const String &dateKey)
{
    const String destinationMonth = dateKey.substring(0, 7);
    if (destinationMonth == currentMonth)
    {
        noteEntries.push_back(entry);
        saveNote();
        return true;
    }

    SD.mkdir("/Notes");
    const String destinationPath = "/Notes/" + destinationMonth + ".txt";
    File destination = SD.open(destinationPath, FILE_APPEND);
    if (!destination)
        return false;
    destination.printf("%s|%c|%s\n", entry.stamp.c_str(), entry.done ? 'x' : '-', entry.text.c_str());
    destination.close();
    return true;
}

bool moveSelectedNoteToDate(const String &dateKey)
{
    struct tm targetDate{};
    if (!parseDateKey(dateKey, targetDate))
        return false;

    const int noteIndex = noteEntryIndexFromVisible(notesSelected);
    if (noteIndex < 0 || noteIndex >= (int)noteEntries.size())
        return false;

    NoteEntry moved = noteEntries[noteIndex];
    const String timeSuffix = moved.stamp.length() > 10 ? moved.stamp.substring(10) : getEntryStamp().substring(10);
    moved.stamp = dateKey + timeSuffix;

    const String destinationMonth = dateKey.substring(0, 7);
    if (destinationMonth == currentMonth)
    {
        noteEntries[noteIndex] = moved;
        saveNote();
    }
    else
    {
        SD.mkdir("/Notes");
        const String destinationPath = "/Notes/" + destinationMonth + ".txt";
        File destination = SD.open(destinationPath, FILE_APPEND);
        if (!destination)
            return false;
        destination.printf("%s|%c|%s\n", moved.stamp.c_str(), moved.done ? 'x' : '-', moved.text.c_str());
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
    const int noteIndex = noteEntryIndexFromVisible(notesSelected);
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
} // namespace NotesInternal
