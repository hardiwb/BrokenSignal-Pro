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
    WaitingForResult,
    WaitingForRadio
};

enum class ComposeResult : uint8_t
{
    Empty,
    Ready,
    TooLong,
    StorageError
};

CalendarSyncState syncState = CalendarSyncState::Idle;
String syncMonth;
uint32_t syncDayMask = 0;
uint8_t syncNextDay = 1;
uint16_t syncSentDays = 0;
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
    const int firstSep = line.indexOf('|');
    const int secondSep = firstSep >= 0 ? line.indexOf('|', firstSep + 1) : -1;
    if (firstSep < 0 || secondSep < 0)
        return false;
    stamp = line.substring(0, firstSep);
    const String status = line.substring(firstSep + 1, secondSep);
    done = status.indexOf('x') >= 0 || status.indexOf('X') >= 0;
    text = line.substring(secondSep + 1);
    text.trim();
    return stamp.length() >= 10 && text.length() > 0;
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

        const char *separator = messageLength == 0 ? "" : "\n";
        const char *status = done ? "[x] " : "[ ] ";
        const size_t separatorLength = strlen(separator);
        const size_t statusLength = strlen(status);
        const size_t textLength = text.length();
        if (messageLength + separatorLength + statusLength + textLength > sticky_note::MAX_MESSAGE_BYTES)
        {
            file.close();
            return ComposeResult::TooLong;
        }
        memcpy(syncMessage + messageLength, separator, separatorLength);
        messageLength += separatorLength;
        memcpy(syncMessage + messageLength, status, statusLength);
        messageLength += statusLength;
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

void finishSync(const char *message)
{
    syncState = CalendarSyncState::Idle;
    syncMonth = "";
    syncDayMask = 0;
    syncNextDay = 1;
    showHdrMsg(message);
}

void startNextCalendarDay()
{
    while (true)
    {
        if (syncDayMask == 0)
        {
            String nextMonth;
            if (!findNextMonth(syncMonth, nextMonth))
            {
                finishSync(syncSentDays == 0 ? "NO NOTES" : "SYNCED");
                return;
            }
            syncMonth = nextMonth;
            syncNextDay = 1;
            if (!loadMonthDayMask(syncMonth, syncDayMask))
            {
                finishSync("SD ERROR");
                return;
            }
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
        switch (composeResult)
        {
        case ComposeResult::Empty:
            continue;
        case ComposeResult::TooLong:
            Serial.printf("Notes calendar sync: %s exceeds %u bytes\n", dateBuffer,
                          static_cast<unsigned>(sticky_note::MAX_MESSAGE_BYTES));
            finishSync("TOO LONG");
            return;
        case ComposeResult::StorageError:
            finishSync("SD ERROR");
            return;
        case ComposeResult::Ready:
            break;
        }

        struct tm date{};
        if (!NotesInternal::parseDateKey(dateKey, date))
            continue;
        const EspNowNotesStartResult startResult = startEspNowNoteSend(
            syncMessage, messageLength, static_cast<uint16_t>(date.tm_year + 1900),
            static_cast<uint8_t>(date.tm_mon + 1), static_cast<uint8_t>(date.tm_mday));
        switch (startResult)
        {
        case EspNowNotesStartResult::Started:
            syncState = CalendarSyncState::WaitingForResult;
            showHdrMsg("SYNCING");
            return;
        case EspNowNotesStartResult::Busy:
            syncState = CalendarSyncState::WaitingForRadio;
            return;
        case EspNowNotesStartResult::RadioPlaying:
            finishSync("RADIO ON");
            return;
        case EspNowNotesStartResult::InvalidNote:
            finishSync("BAD NOTE");
            return;
        case EspNowNotesStartResult::RadioError:
            finishSync("ERROR");
            return;
        }
    }
}
} // namespace

void notesSyncCalendarToXteink()
{
    if (syncState != CalendarSyncState::Idle || espNowNotesBusy())
    {
        showHdrMsg("BUSY");
        return;
    }
    syncMonth = "";
    syncDayMask = 0;
    syncNextDay = 1;
    syncSentDays = 0;
    syncState = CalendarSyncState::WaitingForRadio;
    startNextCalendarDay();
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
    if (syncState == CalendarSyncState::Idle)
        return;

    if (syncState == CalendarSyncState::WaitingForResult)
    {
        switch (result)
        {
        case EspNowNotesResult::None:
            return;
        case EspNowNotesResult::Sent:
            ++syncSentDays;
            syncState = CalendarSyncState::WaitingForRadio;
            break;
        case EspNowNotesResult::Timeout:
            finishSync("TIMEOUT");
            return;
        case EspNowNotesResult::RadioError:
            finishSync("ERROR");
            return;
        }
    }

    if (syncState == CalendarSyncState::WaitingForRadio && !espNowNotesBusy())
        startNextCalendarDay();
}
