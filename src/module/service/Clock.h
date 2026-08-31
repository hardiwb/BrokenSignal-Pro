#pragma once
#include <Arduino.h>
#include <time.h>

bool initRtcClock();
// Configured local date/time (system clock and RTC remain UTC).
bool getCurrentTime(struct tm &out);
bool syncSystemClockFromRtc();
bool syncRtcFromSystemClock();
void setClockTimezoneOffsetHours(int8_t hours);
int8_t getClockTimezoneOffsetHours();
bool getDisplayClockTm(struct tm &out);
bool formatClock(char *out, size_t outSize);
bool setClockFromDisplayDateTime(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second = 0);
bool syncClockFromNTP();
