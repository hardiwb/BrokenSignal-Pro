#include "apps/notes/NotesInternal.h"

#include <SD.h>
#include <esp_system.h>
#include <time.h>

#include "core/State.h"
#include "core/System.h"
#include "apps/notes/Notes.h"

namespace NotesInternal
{
String createNoteId()
{
    char value[25];
    snprintf(value, sizeof(value), "bs-%08lx-%08lx",
             static_cast<unsigned long>(millis()),
             static_cast<unsigned long>(esp_random()));
    return String(value);
}

String noteContentHash(const NoteEntry &entry)
{
    uint32_t hash = 2166136261UL;
    const auto update = [&](const String &value)
    {
        for (size_t i = 0; i < value.length(); ++i)
        {
            hash ^= static_cast<uint8_t>(value.charAt(i));
            hash *= 16777619UL;
        }
        hash ^= 0xff;
        hash *= 16777619UL;
    };
    update(entry.stamp.substring(0, min(10, static_cast<int>(entry.stamp.length()))));
    update(entry.done ? "1" : "0");
    update(entry.category);
    update(entry.text);
    char value[9];
    snprintf(value, sizeof(value), "%08lx", static_cast<unsigned long>(hash));
    return String(value);
}

bool parseNoteStorageLine(const String &line, NoteEntry &entry)
{
    entry = {};
    const int firstSep = line.indexOf('|');
    const int secondSep = firstSep >= 0 ? line.indexOf('|', firstSep + 1) : -1;
    if (firstSep < 0 || secondSep < 0)
        return false;

    entry.stamp = line.substring(0, firstSep);
    const String status = line.substring(firstSep + 1, secondSep);
    entry.done = status.indexOf('x') >= 0 || status.indexOf('X') >= 0;

    const int thirdSep = line.indexOf('|', secondSep + 1);
    const int fourthSep = thirdSep >= 0 ? line.indexOf('|', thirdSep + 1) : -1;
    const int fifthSep = fourthSep >= 0 ? line.indexOf('|', fourthSep + 1) : -1;
    const int sixthSep = fifthSep >= 0 ? line.indexOf('|', fifthSep + 1) : -1;
    if (thirdSep >= 0 && fourthSep >= 0 && fifthSep >= 0 && sixthSep >= 0 &&
        line.substring(secondSep + 1, thirdSep) == "v2")
    {
        entry.id = line.substring(thirdSep + 1, fourthSep);
        entry.category = line.substring(fourthSep + 1, fifthSep);
        entry.syncHash = line.substring(fifthSep + 1, sixthSep);
        entry.text = line.substring(sixthSep + 1);
    }
    else
    {
        entry.text = line.substring(secondSep + 1);
    }
    entry.text.trim();
    entry.category.trim();
    if (!entry.category.length())
        entry.category = "Personal";
    return entry.stamp.length() >= 10 && entry.text.length() > 0;
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

    bool migrated = false;
    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;

        NoteEntry entry;
        if (!parseNoteStorageLine(line, entry))
        {
            const int firstSep = line.indexOf('|');
            entry.stamp = firstSep >= 0 ? line.substring(0, firstSep) : getEntryStamp();
            entry.text = firstSep >= 0 ? line.substring(firstSep + 1) : line;
            entry.text.trim();
        }
        if (entry.text.length() > 0)
        {
            if (!entry.id.length())
            {
                entry.id = createNoteId();
                migrated = true;
            }
            noteEntries.push_back(entry);
        }
    }

    f.close();
    if (migrated)
        saveNote();
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
        showHdrMsg("SD ERROR");
        return;
    }

    for (const NoteEntry &entry : noteEntries)
        f.printf("%s|%c|v2|%s|%s|%s|%s\n", entry.stamp.c_str(), entry.done ? 'x' : '-',
                 entry.id.c_str(), entry.category.c_str(), entry.syncHash.c_str(), entry.text.c_str());
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
    destination.printf("%s|%c|v2|%s|%s|%s|%s\n", entry.stamp.c_str(), entry.done ? 'x' : '-',
                       entry.id.c_str(), entry.category.c_str(), entry.syncHash.c_str(), entry.text.c_str());
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
        destination.printf("%s|%c|v2|%s|%s|%s|%s\n", moved.stamp.c_str(), moved.done ? 'x' : '-',
                           moved.id.c_str(), moved.category.c_str(), moved.syncHash.c_str(), moved.text.c_str());
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

void toggleSelectedNoteCategory()
{
    const int noteIndex = noteEntryIndexFromVisible(notesSelected);
    if (noteIndex < 0)
        return;
    NoteEntry &entry = noteEntries[noteIndex];
    entry.category = entry.category.equalsIgnoreCase("Work") ? "Personal" : "Work";
    saveNote();
    drawNotes();
}

void toggleSelectedNoteArtCategory()
{
    const int noteIndex = noteEntryIndexFromVisible(notesSelected);
    if (noteIndex < 0)
        return;
    NoteEntry &entry = noteEntries[noteIndex];
    entry.category = entry.category.equalsIgnoreCase("Art") ? "Personal" : "Art";
    saveNote();
    drawNotes();
}

void cycleSelectedNoteCategory()
{
    const int noteIndex = noteEntryIndexFromVisible(notesSelected);
    if (noteIndex < 0)
        return;
    NoteEntry &entry = noteEntries[noteIndex];
    if (entry.category.equalsIgnoreCase("Personal"))
        entry.category = "Work";
    else if (entry.category.equalsIgnoreCase("Work"))
        entry.category = "Art";
    else
        entry.category = "Personal";
    saveNote();
    drawNotes();
}

void changeNotesMonth(int delta)
{
    shiftNotesView(delta);
}
} // namespace NotesInternal
