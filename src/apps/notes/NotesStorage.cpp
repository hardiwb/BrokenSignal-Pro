#include "apps/notes/NotesInternal.h"

#include <SD.h>
#include <algorithm>
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

bool shouldMovePastIncompleteNote(const NoteEntry &entry, const String &targetDate)
{
    struct tm entryDate{};
    struct tm target{};
    const String entryDateKey = entry.stamp.substring(0, 10);
    return !entry.done &&
           parseDateKey(entryDateKey, entryDate) &&
           parseDateKey(targetDate, target) &&
           entryDateKey < targetDate;
}

bool movePastIncompleteNotesToDate(const String &targetDate, size_t &movedCount)
{
    movedCount = 0;
    struct tm parsedTarget{};
    if (!parseDateKey(targetDate, parsedTarget))
        return false;

    struct PendingMove
    {
        NoteEntry entry;
        String sourcePath;
    };
    struct Rewrite
    {
        String path;
        String temporary;
        String backup;
        bool backupReady = false;
        bool committed = false;
    };

    std::vector<String> notePaths;
    File directory = SD.open("/Notes");
    if (directory)
    {
        if (!directory.isDirectory())
        {
            directory.close();
            return false;
        }
        File file = directory.openNextFile();
        while (file)
        {
            const String name(file.name());
            const int slash = name.lastIndexOf('/');
            const String base = slash >= 0 ? name.substring(slash + 1) : name;
            if (!file.isDirectory() && base.length() == 11 &&
                base.charAt(4) == '-' && base.substring(7) == ".txt")
                notePaths.push_back("/Notes/" + base);
            file.close();
            file = directory.openNextFile();
        }
        directory.close();
    }

    std::vector<PendingMove> pending;
    for (const String &path : notePaths)
    {
        File input = SD.open(path, FILE_READ);
        if (!input)
            return false;
        while (input.available())
        {
            String line = input.readStringUntil('\n');
            line.trim();
            if (!line.length())
                continue;
            NoteEntry entry;
            bool parsed = parseNoteStorageLine(line, entry);
            if (!parsed)
            {
                const int separator = line.indexOf('|');
                if (separator >= 0)
                {
                    entry.stamp = line.substring(0, separator);
                    entry.text = line.substring(separator + 1);
                    entry.text.trim();
                    parsed = entry.text.length() > 0;
                }
            }
            if (!parsed || !shouldMovePastIncompleteNote(entry, targetDate))
                continue;
            const String timeSuffix = entry.stamp.length() > 10
                                          ? entry.stamp.substring(10)
                                          : " 00:00";
            entry.stamp = targetDate + timeSuffix;
            if (!entry.id.length())
                entry.id = createNoteId();
            pending.push_back({entry, path});
        }
        input.close();
    }

    movedCount = pending.size();
    if (pending.empty())
        return true;

    const String targetPath = "/Notes/" + targetDate.substring(0, 7) + ".txt";
    std::vector<String> affectedPaths;
    for (const auto &move : pending)
        if (std::find(affectedPaths.begin(), affectedPaths.end(), move.sourcePath) == affectedPaths.end())
            affectedPaths.push_back(move.sourcePath);
    if (std::find(affectedPaths.begin(), affectedPaths.end(), targetPath) == affectedPaths.end())
        affectedPaths.push_back(targetPath);

    SD.mkdir("/Notes");
    std::vector<Rewrite> rewrites;
    for (const String &path : affectedPaths)
    {
        Rewrite rewrite;
        rewrite.path = path;
        rewrite.temporary = path + ".tmp";
        rewrite.backup = path + ".bak";
        rewrites.push_back(rewrite);
    }

    const auto cleanTemporaryFiles = [&]()
    {
        for (const auto &rewrite : rewrites)
            SD.remove(rewrite.temporary.c_str());
    };

    for (const auto &rewrite : rewrites)
    {
        SD.remove(rewrite.temporary.c_str());
        File output = SD.open(rewrite.temporary, FILE_WRITE);
        if (!output)
        {
            cleanTemporaryFiles();
            return false;
        }

        if (SD.exists(rewrite.path.c_str()))
        {
            File input = SD.open(rewrite.path, FILE_READ);
            if (!input)
            {
                output.close();
                cleanTemporaryFiles();
                return false;
            }
            while (input.available())
            {
                String rawLine = input.readStringUntil('\n');
                String line = rawLine;
                line.trim();
                NoteEntry entry;
                bool parsed = line.length() && parseNoteStorageLine(line, entry);
                if (!parsed && line.length())
                {
                    const int separator = line.indexOf('|');
                    if (separator >= 0)
                    {
                        entry.stamp = line.substring(0, separator);
                        entry.text = line.substring(separator + 1);
                        entry.text.trim();
                        parsed = entry.text.length() > 0;
                    }
                }

                if (parsed && shouldMovePastIncompleteNote(entry, targetDate))
                {
                    if (rewrite.path == targetPath)
                    {
                        const String timeSuffix = entry.stamp.length() > 10
                                                      ? entry.stamp.substring(10)
                                                      : " 00:00";
                        entry.stamp = targetDate + timeSuffix;
                        if (!entry.id.length())
                            entry.id = createNoteId();
                        output.printf("%s|%c|v2|%s|%s|%s|%s\n", entry.stamp.c_str(),
                                      entry.done ? 'x' : '-', entry.id.c_str(), entry.category.c_str(),
                                      entry.syncHash.c_str(), entry.text.c_str());
                    }
                }
                else
                {
                    output.print(rawLine);
                    output.print('\n');
                }
            }
            input.close();
        }

        if (rewrite.path == targetPath)
            for (const auto &move : pending)
                if (move.sourcePath != targetPath)
                    output.printf("%s|%c|v2|%s|%s|%s|%s\n", move.entry.stamp.c_str(),
                                  move.entry.done ? 'x' : '-', move.entry.id.c_str(),
                                  move.entry.category.c_str(), move.entry.syncHash.c_str(),
                                  move.entry.text.c_str());
        output.close();
    }

    const auto rollback = [&]()
    {
        for (auto &rewrite : rewrites)
        {
            if (rewrite.committed)
                SD.remove(rewrite.path.c_str());
            if (rewrite.backupReady)
                SD.rename(rewrite.backup.c_str(), rewrite.path.c_str());
            SD.remove(rewrite.temporary.c_str());
        }
    };

    for (auto &rewrite : rewrites)
    {
        SD.remove(rewrite.backup.c_str());
        if (SD.exists(rewrite.path.c_str()))
        {
            if (!SD.rename(rewrite.path.c_str(), rewrite.backup.c_str()))
            {
                rollback();
                return false;
            }
            rewrite.backupReady = true;
        }
        if (!SD.rename(rewrite.temporary.c_str(), rewrite.path.c_str()))
        {
            rollback();
            return false;
        }
        rewrite.committed = true;
    }

    for (const auto &rewrite : rewrites)
        SD.remove(rewrite.backup.c_str());
    return true;
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
