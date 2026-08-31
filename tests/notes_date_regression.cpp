// Host harness: run with tools/test_notes_dates.ps1. The runner inserts the
// actual firmware functions below, so these tests do not duplicate their logic.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <iostream>
#include "module/service/StickyNoteProtocol.h"

class String : public std::string {
public:
    using std::string::string;
    String(const std::string &value) : std::string(value) {}
    String substring(size_t begin, size_t end = std::string::npos) const {
        return substr(begin, end == std::string::npos ? end : end - begin);
    }
    char charAt(size_t index) const { return at(index); }
};

enum class NotesViewMode { Day, Month };
struct NoteEntry { String stamp; bool done; String text; };
enum class EspNowNotesStartResult { Started, Busy, RadioPlaying, InvalidNote, RadioError };
std::vector<NoteEntry> noteEntries;
std::vector<int> visibleNoteIndices;
NotesViewMode notesViewMode = NotesViewMode::Day;
int notesDayOffset = 0, notesMonthOffset = 0, notesSelected = 0;
String currentDay, currentMonth, statusMessage, sentMessage;
int sentYear = 0, sentMonth = 0, sentDay = 0, sends = 0, reloads = 0;
int8_t timezoneOffsetHours = 7;
time_t fakeEpoch = 0;
time_t fakeTime(time_t *) { return fakeEpoch; }
bool getUtcNow(time_t &out) { out = fakeEpoch; return true; }
bool copyGmTime(time_t value, tm &out) {
    const tm *result = gmtime(&value);
    if (!result) return false;
    out = *result;
    return true;
}
void resetNotesCursor() { notesSelected = 0; }
void drawNotes() {}
void showHdrMsg(const char *text) { statusMessage = text; }
int noteEntryIndexFromVisible(int index) {
    return index >= 0 && index < static_cast<int>(visibleNoteIndices.size())
        ? visibleNoteIndices[index] : -1;
}
String formatDateKey(const tm &date);
void loadNote();
EspNowNotesStartResult startEspNowNoteSend(const char *text, size_t length,
                                         uint16_t year, uint8_t month, uint8_t day) {
    sentMessage = std::string(text, length);
    sentYear = year; sentMonth = month; sentDay = day; ++sends;
    return EspNowNotesStartResult::Started;
}

#define time fakeTime
// INSERT_PRODUCTION_FUNCTIONS
#undef time

void loadNote() {
    ++reloads;
    updateViewedDateCache();
    visibleNoteIndices.clear();
    for (size_t i = 0; i < noteEntries.size(); ++i) {
        if (notesViewMode == NotesViewMode::Day
                ? noteEntries[i].stamp.substring(0, 10) == currentDay
                : noteEntries[i].stamp.substring(0, 7) == currentMonth)
            visibleNoteIndices.push_back(static_cast<int>(i));
    }
}

void setUtc(int year, int month, int day, int hour, int minute = 0, int second = 0) {
    tm value{};
    value.tm_year = year - 1900; value.tm_mon = month - 1; value.tm_mday = day;
    value.tm_hour = hour; value.tm_min = minute; value.tm_sec = second;
    fakeEpoch = mktime(&value);
}

int main() {
    // mktime is used only to normalize civil dates in production (system TZ UTC).
#ifdef _WIN32
    _putenv_s("TZ", "UTC0"); _tzset();
#else
    setenv("TZ", "UTC0", 1); tzset();
#endif
    setUtc(2026, 8, 30, 22);
    tm local{};
    assert(getCurrentTime(local));
    assert(local.tm_year == 126 && local.tm_mon == 7 && local.tm_mday == 31);
    assert(local.tm_hour == 5 && local.tm_wday == 1);
    assert(viewedDateKey() == "2026-08-31");
    notesDayOffset = 1;
    assert(viewedDateKey() == "2026-09-01");
    notesDayOffset = 0;
    notesViewMode = NotesViewMode::Month; notesMonthOffset = 1;
    assert(viewedDateKey() == "2026-09-01"); // Aug 31 must not skip September.
    notesMonthOffset = -6;
    assert(viewedDateKey() == "2026-02-01");
    notesMonthOffset = 0; notesViewMode = NotesViewMode::Day;

    setUtc(2026, 12, 31, 17);
    assert(viewedDateKey() == "2027-01-01");
    setUtc(2028, 2, 28, 17);
    assert(viewedDateKey() == "2028-02-29");
    timezoneOffsetHours = -7;
    setUtc(2026, 8, 31, 5);
    assert(viewedDateKey() == "2026-08-30");
    timezoneOffsetHours = 0;
    assert(getCurrentTime(local) && local.tm_hour == 5);
    fakeEpoch = 0;
    assert(!getCurrentTime(local));
    timezoneOffsetHours = 7;

    // Midnight refresh updates both date and loaded month, including empty lists.
    setUtc(2026, 8, 31, 16, 59, 59); loadNote();
    const int before = reloads;
    assert(!refreshViewedDate());
    ++fakeEpoch;
    assert(refreshViewedDate());
    assert(reloads == before + 1 && currentDay == "2026-09-01" && currentMonth == "2026-09");
    assert(!refreshViewedDate());

    noteEntries = {
        {"2026-09-01 05:00", false, "today"},
        {"2026-09-02 05:00", false, "tomorrow"},
        {"2026-09-02 05:00", true, "done"},
        {"2026-09-03 05:00", false, "later"}
    };
    notesDayOffset = 1; loadNote();
    notesSendViewedDayToXteink(false);
    assert(sends == 1 && sentYear == 2026 && sentMonth == 9 && sentDay == 2);
    assert(sentMessage == "[ ] tomorrow");
    notesSendViewedDayToXteink(true);
    assert(sentMessage == "[ ] tomorrow\n[x] done");

    notesViewMode = NotesViewMode::Month; loadNote(); notesSelected = 1;
    notesSendViewedDayToXteink(false);
    assert(sentDay == 2 && sentMessage == "[ ] tomorrow");
    notesSendViewedDayToXteink(true);
    assert(sentMessage == "[ ] tomorrow\n[x] done");
    assert(notesSelected == 1 && notesViewMode == NotesViewMode::Month);
    notesSelected = 2; // A completed row still identifies the same day.
    notesSendViewedDayToXteink(false);
    assert(sentDay == 2 && sentMessage == "[ ] tomorrow");

    noteEntries[1].done = true;
    int beforeSends = sends;
    notesSendViewedDayToXteink(false);
    assert(sends == beforeSends && statusMessage == "ALL DONE");
    noteEntries[1].done = false;
    noteEntries[1].text = std::string(sticky_note::MAX_MESSAGE_BYTES - 4, 'a');
    notesSendViewedDayToXteink(false);
    assert(sentMessage.length() == sticky_note::MAX_MESSAGE_BYTES);
    noteEntries[1].text += 'a';
    beforeSends = sends;
    notesSendViewedDayToXteink(false);
    assert(sends == beforeSends && statusMessage == "TOO LONG");

    tm parsed{};
    assert(parseDateKey("2028-02-29", parsed));
    assert(!parseDateKey("2026-02-29", parsed));
    assert(!parseDateKey("2026-09-31", parsed));
    noteEntries.clear(); loadNote();
    notesSendViewedDayToXteink(false);
    assert(statusMessage == "NO NOTES");

    // A future month keeps the selected date and does not include other days.
    setUtc(2026, 8, 30, 22);
    notesMonthOffset = 1;
    noteEntries = {{"2026-09-20 05:00", false, "next month"}};
    loadNote(); notesSelected = 0;
    notesSendViewedDayToXteink(false);
    assert(sentYear == 2026 && sentMonth == 9 && sentDay == 20);
    assert(sentMessage == "[ ] next month");

    // Sending immediately at midnight also refreshes the stale loaded date.
    notesViewMode = NotesViewMode::Day; notesDayOffset = 0; notesMonthOffset = 0;
    setUtc(2026, 8, 31, 16, 59, 59);
    noteEntries = {{"2026-09-01 05:00", false, "new day"}};
    loadNote(); ++fakeEpoch;
    notesSendViewedDayToXteink(false);
    assert(sentMonth == 9 && sentDay == 1 && sentMessage == "[ ] new day");
    std::cout << "PASS: local dates, weekday, midnight/month/year/leap rollovers, "
                 "future-day/month sends, completed filters, payload limits, invalid dates\n";
}
