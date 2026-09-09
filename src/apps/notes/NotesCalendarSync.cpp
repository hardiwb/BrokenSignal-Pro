#include "apps/notes/Notes.h"

#include <SD.h>

#include "apps/notes/NotesInternal.h"
#include "core/State.h"
#include "core/System.h"
#include "module/service/EspNowNotes.h"

namespace
{
enum class CalendarSyncState : uint8_t
{
    Idle,
    WaitingForBegin,
    WaitingForNote,
    WaitingForCommit
};

enum class ComposeResult : uint8_t
{
    Empty,
    Ready,
    TooLong,
    StorageError
};

enum class NextNoteResult : uint8_t
{
    Done,
    Ready,
    TooLong,
    StorageError,
    InvalidDate
};

CalendarSyncState syncState = CalendarSyncState::Idle;
String syncMonth;
uint32_t syncDayMask = 0;
uint8_t syncNextDay = 1;
uint16_t syncEntryCount = 0;
uint16_t syncCompletedEntries = 0;
uint32_t syncDigest = 0;
uint32_t syncSequence = 0;
static char syncMessage[sticky_note::MAX_MESSAGE_BYTES + 1] = {};

bool parseMonthFilename(const String &path, String &month)
{
    const int slash = path.lastIndexOf('/');
    const String name = slash >= 0 ? path.substring(slash + 1) : path;
    if (name.length() != 11 || name.charAt(4) != '-' || name.substring(7) != ".txt")
        return false;
    for (int i = 0; i < 7; ++i)
    {
        if (i == 4)
            continue;
        if (!isDigit(name.charAt(i)))
            return false;
    }
    const int year = name.substring(0, 4).toInt();
    const int monthNumber = name.substring(5, 7).toInt();
    if (year < 2000 || year > 2099 || monthNumber < 1 || monthNumber > 12)
        return false;
    month = name.substring(0, 7);
    return true;
}

bool findNextMonth(const String &afterMonth, String &nextMonth)
{
    nextMonth = "";
    File directory = SD.open("/Notes");
    if (!directory || !directory.isDirectory())
    {
        if (directory)
            directory.close();
        return false;
    }

    File entry = directory.openNextFile();
    while (entry)
    {
        if (!entry.isDirectory())
        {
            String candidate;
            if (parseMonthFilename(String(entry.name()), candidate) && candidate > afterMonth &&
                (nextMonth.length() == 0 || candidate < nextMonth))
                nextMonth = candidate;
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
    return nextMonth.length() > 0;
}

bool parseStoredLine(const String &line, String &stamp, bool &done, String &text)
{
    NotesInternal::NoteEntry entry;
    if (!NotesInternal::parseNoteStorageLine(line, entry))
        return false;
    stamp = entry.stamp;
    done = entry.done;
    text = entry.text;
    return true;
}

bool loadMonthDayMask(const String &month, uint32_t &dayMask)
{
    dayMask = 0;
    File file = SD.open("/Notes/" + month + ".txt", FILE_READ);
    if (!file)
        return false;

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        String stamp;
        String text;
        bool done = false;
        if (!parseStoredLine(line, stamp, done, text) || stamp.substring(0, 7) != month)
            continue;
        const int day = stamp.substring(8, 10).toInt();
        if (day >= 1 && day <= 31)
            dayMask |= uint32_t{1} << (day - 1);
    }
    file.close();
    return true;
}

ComposeResult composeDay(const String &dateKey, size_t &messageLength)
{
    messageLength = 0;
    File file = SD.open("/Notes/" + dateKey.substring(0, 7) + ".txt", FILE_READ);
    if (!file)
        return ComposeResult::StorageError;

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        String stamp;
        String text;
        bool done = false;
        if (!parseStoredLine(line, stamp, done, text) || stamp.substring(0, 10) != dateKey)
            continue;
        constexpr size_t prefixLength = 4;
        const size_t separatorLength = messageLength == 0 ? 0 : 1;
        const size_t textLength = text.length();
        if (messageLength + separatorLength + prefixLength + textLength > sticky_note::MAX_MESSAGE_BYTES)
        {
            file.close();
            return ComposeResult::TooLong;
        }
        if (separatorLength != 0)
            syncMessage[messageLength++] = '\n';
        const char *prefix = done ? "[x] " : "[ ] ";
        memcpy(syncMessage + messageLength, prefix, prefixLength);
        messageLength += prefixLength;
        for (size_t i = 0; i < textLength; ++i)
        {
            const char value = text.charAt(i);
            syncMessage[messageLength++] = value == '\n' || value == '\r' || value == '\t' ? ' ' : value;
        }
    }
    file.close();
    syncMessage[messageLength] = '\0';
    return messageLength == 0 ? ComposeResult::Empty : ComposeResult::Ready;
}

void resetCalendarCursor()
{
    syncMonth = "";
    syncDayMask = 0;
    syncNextDay = 1;
}

NextNoteResult loadNextSnapshotNote(sticky_note::Note &note)
{
    while (true)
    {
        if (syncDayMask == 0)
        {
            String nextMonth;
            if (!findNextMonth(syncMonth, nextMonth))
                return NextNoteResult::Done;
            syncMonth = nextMonth;
            syncNextDay = 1;
            if (!loadMonthDayMask(syncMonth, syncDayMask))
                return NextNoteResult::StorageError;
            if (syncDayMask == 0)
                continue;
        }

        while (syncNextDay <= 31 && (syncDayMask & (uint32_t{1} << (syncNextDay - 1))) == 0)
            ++syncNextDay;
        if (syncNextDay > 31)
        {
            syncDayMask = 0;
            continue;
        }

        const uint8_t day = syncNextDay++;
        syncDayMask &= ~(uint32_t{1} << (day - 1));
        char dateBuffer[11];
        snprintf(dateBuffer, sizeof(dateBuffer), "%s-%02u", syncMonth.c_str(), static_cast<unsigned>(day));
        const String dateKey(dateBuffer);
        size_t messageLength = 0;
        const ComposeResult composeResult = composeDay(dateKey, messageLength);
        if (composeResult == ComposeResult::Empty)
            continue;
        if (composeResult == ComposeResult::TooLong)
            return NextNoteResult::TooLong;
        if (composeResult == ComposeResult::StorageError)
            return NextNoteResult::StorageError;

        struct tm date{};
        if (!NotesInternal::parseDateKey(dateKey, date))
            return NextNoteResult::InvalidDate;
        note.year = static_cast<uint16_t>(date.tm_year + 1900);
        note.month = static_cast<uint8_t>(date.tm_mon + 1);
        note.day = static_cast<uint8_t>(date.tm_mday);
        note.messageLength = static_cast<uint16_t>(messageLength);
        memcpy(note.message.data(), syncMessage, messageLength + 1);
        return NextNoteResult::Ready;
    }
}

void finishSync(const char *message)
{
    syncState = CalendarSyncState::Idle;
    resetCalendarCursor();
    syncSequence = 0;
    showHdrMsg(message);
}

bool handleSnapshotReadError(NextNoteResult result)
{
    if (result == NextNoteResult::TooLong)
        finishSync("TOO LONG");
    else if (result == NextNoteResult::StorageError)
        finishSync("SD ERROR");
    else if (result == NextNoteResult::InvalidDate)
        finishSync("BAD DATE");
    else
        return false;
    return true;
}

bool prepareSnapshot()
{
    resetCalendarCursor();
    syncEntryCount = 0;
    uint32_t digestState = 0xffffffffU;
    sticky_note::Note note;
    while (true)
    {
        const NextNoteResult result = loadNextSnapshotNote(note);
        if (result == NextNoteResult::Done)
            break;
        if (result != NextNoteResult::Ready)
        {
            handleSnapshotReadError(result);
            return false;
        }
        if (syncEntryCount == UINT16_MAX)
        {
            finishSync("TOO MANY");
            return false;
        }
        ++syncEntryCount;
        digestState = sticky_note::snapshotDigestUpdate(digestState, note);
    }
    syncDigest = sticky_note::snapshotDigestFinish(digestState);
    resetCalendarCursor();
    return true;
}

void showSyncProgress()
{
    if (syncEntryCount > 999)
    {
        showHdrMsg("SYNCING");
        return;
    }
    char status[9];
    snprintf(status, sizeof(status), "%u/%u",
             static_cast<unsigned>(syncCompletedEntries + 1),
             static_cast<unsigned>(syncEntryCount));
    showHdrMsg(status);
}

void startCommit()
{
    const EspNowNotesStartResult result = startEspNowSnapshotControl(
        sticky_note::TYPE_SNAPSHOT_COMMIT, syncSequence, syncEntryCount, syncDigest, true);
    if (result == EspNowNotesStartResult::Started)
    {
        syncState = CalendarSyncState::WaitingForCommit;
        showHdrMsg("COMMIT");
        return;
    }
    finishSync(result == EspNowNotesStartResult::RadioPlaying ? "RADIO ON" : "ERROR");
    cancelEspNowNotes();
}

void startNextCalendarDay()
{
    sticky_note::Note note;
    const NextNoteResult next = loadNextSnapshotNote(note);
    if (next == NextNoteResult::Done)
    {
        startCommit();
        return;
    }
    if (handleSnapshotReadError(next))
    {
        cancelEspNowNotes();
        return;
    }

    const EspNowNotesStartResult result = startEspNowNoteSend(
        note.message.data(), note.messageLength, note.year, note.month, note.day, true);
    if (result == EspNowNotesStartResult::Started)
    {
        syncState = CalendarSyncState::WaitingForNote;
        showSyncProgress();
        return;
    }
    finishSync(result == EspNowNotesStartResult::RadioPlaying ? "RADIO ON" :
               result == EspNowNotesStartResult::InvalidNote ? "BAD NOTE" : "ERROR");
    cancelEspNowNotes();
}
} // namespace

void notesSyncCalendarToXteink()
{
    if (syncState != CalendarSyncState::Idle || espNowNotesBusy())
    {
        showHdrMsg("BUSY");
        return;
    }
    if (!prepareSnapshot())
        return;

    syncCompletedEntries = 0;
    syncSequence = createEspNowNotesSequence();
    const EspNowNotesStartResult result = startEspNowSnapshotControl(
        sticky_note::TYPE_SNAPSHOT_BEGIN, syncSequence, syncEntryCount, syncDigest);
    if (result == EspNowNotesStartResult::Started)
    {
        syncState = CalendarSyncState::WaitingForBegin;
        showHdrMsg("BEGIN");
        return;
    }
    finishSync(result == EspNowNotesStartResult::RadioPlaying ? "RADIO ON" : "ERROR");
}

bool notesCalendarSyncActive()
{
    return syncState != CalendarSyncState::Idle;
}

void cancelNotesCalendarSync()
{
    if (syncState == CalendarSyncState::Idle)
        return;
    cancelEspNowNotes();
    finishSync("CANCELLED");
}

void tickNotesCalendarSync(const EspNowNotesResult result)
{
    if (syncState == CalendarSyncState::Idle || result == EspNowNotesResult::None)
        return;

    if (result == EspNowNotesResult::Timeout)
    {
        finishSync("TIMEOUT");
        return;
    }
    if (result == EspNowNotesResult::RadioError)
    {
        finishSync("ERROR");
        return;
    }

    switch (syncState)
    {
    case CalendarSyncState::WaitingForBegin:
        resetCalendarCursor();
        startNextCalendarDay();
        break;
    case CalendarSyncState::WaitingForNote:
        ++syncCompletedEntries;
        startNextCalendarDay();
        break;
    case CalendarSyncState::WaitingForCommit:
        finishSync("SYNCED");
        break;
    case CalendarSyncState::Idle:
        break;
    }
}
