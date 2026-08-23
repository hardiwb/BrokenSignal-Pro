#include "module/Clock.h"

#include <Arduino.h>
#include <Wire.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <sys/time.h>
#include "core/Config.h"
#include <WiFi.h>

namespace
{
    constexpr uint8_t RTC_ADDR = 0x68;
    constexpr time_t CLOCK_FALLBACK_EPOCH = 946684800L; // 2000-01-01 00:00:00 UTC

    TwoWire &rtcWire = Wire;
    bool rtcInitialized = false;
    int8_t timezoneOffsetHours = 0;
    char lastClockText[6] = "--:--";
    bool lastClockValid = false;

    static uint8_t bcdToDec(uint8_t value)
    {
        return (uint8_t)((value >> 4) * 10 + (value & 0x0F));
    }

    static uint8_t decToBcd(uint8_t value)
    {
        return (uint8_t)(((value / 10) << 4) | (value % 10));
    }

    static uint8_t daysInMonth(uint16_t year, uint8_t month)
    {
        static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month < 1 || month > 12)
            return 31;
        if (month == 2)
        {
            bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
            return leap ? 29 : 28;
        }
        return monthDays[month - 1];
    }

    static bool isLeapYear(int year)
    {
        return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    }

    static time_t epochFromUtcTm(const struct tm &src)
    {
        int year = src.tm_year + 1900;
        if (year < 1970)
            return (time_t)-1;

        static const int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

        int64_t days = 0;
        for (int y = 1970; y < year; ++y)
            days += isLeapYear(y) ? 366 : 365;

        for (int m = 0; m < src.tm_mon; ++m)
        {
            days += monthDays[m];
            if (m == 1 && isLeapYear(year))
                days += 1;
        }

        days += (int64_t)(src.tm_mday - 1);
        int64_t seconds = days * 86400LL + (int64_t)src.tm_hour * 3600LL +
                          (int64_t)src.tm_min * 60LL + (int64_t)src.tm_sec;
        return (seconds < 0) ? (time_t)-1 : (time_t)seconds;
    }

    static bool copyGmTime(time_t value, struct tm &out)
    {
        struct tm *ptr = gmtime(&value);
        if (!ptr)
            return false;
        out = *ptr;
        return true;
    }

    static bool readRtcRegs(uint8_t reg, uint8_t *buf, size_t len)
    {
        if (!rtcInitialized)
            return false;
        rtcWire.beginTransmission(RTC_ADDR);
        rtcWire.write(reg);
        if (rtcWire.endTransmission(false) != 0)
            return false;

        if (rtcWire.requestFrom((int)RTC_ADDR, (int)len) != len)
            return false;

        for (size_t i = 0; i < len; ++i)
            buf[i] = rtcWire.read();

        return true;
    }

    static bool writeRtcRegs(uint8_t reg, const uint8_t *buf, size_t len)
    {
        if (!rtcInitialized)
        {
            Serial.println("[CLOCK] I2C WRITE: RTC not initialized");
            return false;
        }

        rtcWire.beginTransmission(RTC_ADDR);

        rtcWire.write(reg);

        for (size_t i = 0; i < len; ++i)
            rtcWire.write(buf[i]);

        uint8_t error = rtcWire.endTransmission();

        if (error != 0)
        {
            Serial.printf(
                "[CLOCK] I2C WRITE FAILED: error=%u\n",
                error);

            switch (error)
            {
            case 1:
                Serial.println("[CLOCK] I2C: data too long");
                break;

            case 2:
                Serial.println("[CLOCK] I2C: address NACK");
                break;

            case 3:
                Serial.println("[CLOCK] I2C: data NACK");
                break;

            case 4:
                Serial.println("[CLOCK] I2C: other error");
                break;

            case 5:
                Serial.println("[CLOCK] I2C: timeout");
                break;
            }

            return false;
        }

        return true;
    }

    static bool readRtcTime(struct tm &out)
    {
        uint8_t data[7] = {};
        if (!readRtcRegs(0x00, data, sizeof(data)))
            return false;

        const uint8_t seconds = data[0] & 0x7F;
        const uint8_t hours = data[2] & 0x3F;
        const uint8_t dayOfWeek = data[3] & 0x07;
        const uint8_t month = data[5] & 0x1F;

        if (bcdToDec(seconds) > 59 || bcdToDec(data[1]) > 59 || bcdToDec(hours) > 23 ||
            bcdToDec(data[4]) < 1 || bcdToDec(data[4]) > 31 || bcdToDec(month) < 1 ||
            bcdToDec(month) > 12 || bcdToDec(data[6]) > 99 || dayOfWeek < 1 || dayOfWeek > 7)
            return false;

        memset(&out, 0, sizeof(out));
        out.tm_sec = bcdToDec(seconds);
        out.tm_min = bcdToDec(data[1]);
        out.tm_hour = bcdToDec(hours);
        out.tm_mday = bcdToDec(data[4]);
        out.tm_mon = bcdToDec(month) - 1;
        out.tm_year = 100 + bcdToDec(data[6]);
        out.tm_wday = (dayOfWeek == 1) ? 0 : (dayOfWeek - 1);
        out.tm_isdst = 0;
        return true;
    }

    static bool writeRtcTime(const struct tm &src)
    {
        struct tm tm = src;

        if (tm.tm_year < 100)
        {
            Serial.println("[CLOCK] RTC WRITE: invalid year");
            return false;
        }

        uint8_t data[7] = {
            decToBcd((uint8_t)tm.tm_sec),
            decToBcd((uint8_t)tm.tm_min),
            decToBcd((uint8_t)tm.tm_hour),
            decToBcd((uint8_t)((tm.tm_wday == 0) ? 1 : (tm.tm_wday + 1))),
            decToBcd((uint8_t)tm.tm_mday),
            decToBcd((uint8_t)(tm.tm_mon + 1)),
            decToBcd((uint8_t)(tm.tm_year - 100)),
        };

        Serial.printf(
            "[CLOCK] RTC WRITE: %04d-%02d-%02d %02d:%02d:%02d\n",
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec);

        // Write time registers.
        if (!writeRtcRegs(0x00, data, sizeof(data)))
        {
            Serial.println("[CLOCK] RTC WRITE: time registers FAILED");
            return false;
        }

        Serial.println("[CLOCK] RTC WRITE: time registers OK");

        // Clear RTC lost-power flag.
        uint8_t status = 0;

        if (!readRtcRegs(0x0F, &status, 1))
        {
            Serial.println("[CLOCK] RTC WRITE: status READ FAILED");
            return false;
        }

        Serial.printf(
            "[CLOCK] RTC WRITE: status before = 0x%02X\n",
            status);

        status &= (uint8_t)~0x80;

        if (!writeRtcRegs(0x0F, &status, 1))
        {
            Serial.println("[CLOCK] RTC WRITE: status WRITE FAILED");
            return false;
        }

        Serial.println("[CLOCK] RTC WRITE: status WRITE OK");

        return true;
    }

    static bool rtcLostPower()
    {
        uint8_t status = 0;
        if (!readRtcRegs(0x0F, &status, 1))
            return true;
        return (status & 0x80) != 0;
    }

    static bool buildTimeToTm(struct tm &out)
    {
        static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
        char mon[4] = {};
        strncpy(mon, __DATE__, 3);
        const char *found = strstr(months, mon);
        if (!found)
            return false;

        memset(&out, 0, sizeof(out));
        out.tm_mon = (int)((found - months) / 3);
        out.tm_mday = atoi(__DATE__ + 4);
        out.tm_year = atoi(__DATE__ + 7) - 1900;
        sscanf(__TIME__, "%d:%d:%d", &out.tm_hour, &out.tm_min, &out.tm_sec);
        out.tm_isdst = 0;
        out.tm_wday = 0;
        return true;
    }

    static bool getUtcNow(time_t &out)
    {
        time_t now = time(nullptr);
        if (now >= CLOCK_FALLBACK_EPOCH)
        {
            out = now;
            return true;
        }

        struct tm buildTm{};
        if (!buildTimeToTm(buildTm))
            return false;

        time_t buildEpoch = epochFromUtcTm(buildTm);
        if (buildEpoch < CLOCK_FALLBACK_EPOCH)
            return false;

        out = buildEpoch;
        return true;
    }

    static bool getDisplayTm(struct tm &out)
    {
        time_t utcNow = 0;
        if (!getUtcNow(utcNow))
            return false;

        time_t displayEpoch = utcNow + (time_t)timezoneOffsetHours * 3600;
        return copyGmTime(displayEpoch, out);
    }

    static bool syncSystemClockFromTm(const struct tm &src)
    {
        struct tm tm = src;
        time_t epoch = epochFromUtcTm(tm);
        if (epoch < 0)
            return false;

        timeval tv{};
        tv.tv_sec = epoch;
        tv.tv_usec = 0;
        return settimeofday(&tv, nullptr) == 0;
    }

    static bool ensureRtcClock()
    {
        if (rtcInitialized)
            return true;
        return initRtcClock();
    }
}

bool getCurrentTime(struct tm &out)
{
    time_t now = time(nullptr);

    if (now < 1000000000)
        return false;

    localtime_r(&now, &out);

    return true;
}

//==================================================
// NTP SYNC
//==================================================

bool syncClockFromNTP()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[CLOCK] NTP: WiFi not connected");
        return false;
    }

    Serial.println("[CLOCK] NTP: starting sync...");

    // Keep system clock in UTC.
    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.google.com",
        "time.cloudflare.com");

    struct tm utcTm{};

    // Wait up to 10 seconds for NTP.
    if (!getLocalTime(&utcTm, 10000))
    {
        Serial.println("[CLOCK] NTP: sync failed");
        return false;
    }

    Serial.printf(
        "[CLOCK] NTP UTC: %04d-%02d-%02d %02d:%02d:%02d\n",
        utcTm.tm_year + 1900,
        utcTm.tm_mon + 1,
        utcTm.tm_mday,
        utcTm.tm_hour,
        utcTm.tm_min,
        utcTm.tm_sec);

    // System clock has now been updated by NTP.
    // Store the same UTC value in the external RTC.
    struct tm rtcCheck{};

    if (!syncRtcFromSystemClock())
    {
        Serial.println("[CLOCK] NTP: RTC WRITE FAILED");
        return false;
    }

    if (readRtcTime(rtcCheck))
    {
        Serial.printf(
            "[CLOCK] RTC AFTER NTP: %04d-%02d-%02d %02d:%02d:%02d\n",
            rtcCheck.tm_year + 1900,
            rtcCheck.tm_mon + 1,
            rtcCheck.tm_mday,
            rtcCheck.tm_hour,
            rtcCheck.tm_min,
            rtcCheck.tm_sec);
    }

    Serial.println("[CLOCK] NTP: RTC updated");

    lastClockValid = false;
    formatClock(lastClockText, sizeof(lastClockText));

    return true;
}

//==================================================
// RTC I2C DEBUG
//==================================================

static void scanRtcI2C()
{
    Serial.println("[CLOCK] I2C scan START");

    int found = 0;

    for (uint8_t address = 1; address < 127; address++)
    {
        rtcWire.beginTransmission(address);

        uint8_t error = rtcWire.endTransmission();

        if (error == 0)
        {
            Serial.printf(
                "[CLOCK] I2C device found: 0x%02X\n",
                address);

            found++;
        }
    }

    if (found == 0)
        Serial.println("[CLOCK] I2C: no devices found");

    Serial.println("[CLOCK] I2C scan DONE");
}

bool initRtcClock()
{
    rtcWire.begin(RTC_SDA, RTC_SCL);
    rtcWire.setClock(100000);
    rtcInitialized = true;
    scanRtcI2C();

    if (rtcLostPower())
    {
        Serial.println("[CLOCK] RTC LOST POWER FLAG!");

        struct tm buildTm{};

        if (buildTimeToTm(buildTm))
        {
            if (writeRtcTime(buildTm))
            {
                syncSystemClockFromTm(buildTm);
                Serial.println("[CLOCK] RTC initialized from build time");
            }
        }
    }
    else
    {
        Serial.println("[CLOCK] RTC power OK");
    }

    //==============================================
    // Restore system clock from RTC
    //==============================================

    if (syncSystemClockFromRtc())
    {
        struct tm rtcNow{};

        if (readRtcTime(rtcNow))
        {
            Serial.printf(
                "[CLOCK] RTC boot time: %04d-%02d-%02d %02d:%02d:%02d\n",
                rtcNow.tm_year + 1900,
                rtcNow.tm_mon + 1,
                rtcNow.tm_mday,
                rtcNow.tm_hour,
                rtcNow.tm_min,
                rtcNow.tm_sec);
        }

        return true;
    }

    //==============================================
    // RTC read failed → fallback to build time
    //==============================================

    Serial.println("[CLOCK] RTC read failed, using build time");

    struct tm buildTm{};

    if (!buildTimeToTm(buildTm))
        return false;

    if (!syncSystemClockFromTm(buildTm))
        return false;

    return writeRtcTime(buildTm);
}

bool syncSystemClockFromRtc()
{
    if (!ensureRtcClock())
        return false;
    struct tm rtcTm{};
    if (!readRtcTime(rtcTm))
        return false;
    return syncSystemClockFromTm(rtcTm);
}

bool syncRtcFromSystemClock()
{
    if (!ensureRtcClock())
    {
        Serial.println("[CLOCK] RTC SYNC: RTC not initialized");
        return false;
    }

    time_t now = time(nullptr);

    if (now < CLOCK_FALLBACK_EPOCH)
    {
        Serial.println("[CLOCK] RTC SYNC: invalid system time");
        return false;
    }

    struct tm current{};

    if (!copyGmTime(now, current))
    {
        Serial.println("[CLOCK] RTC SYNC: failed to convert system time");
        return false;
    }

    Serial.printf(
        "[CLOCK] RTC SYNC: writing %04d-%02d-%02d %02d:%02d:%02d\n",
        current.tm_year + 1900,
        current.tm_mon + 1,
        current.tm_mday,
        current.tm_hour,
        current.tm_min,
        current.tm_sec);

    if (!writeRtcTime(current))
    {
        Serial.println("[CLOCK] RTC SYNC: writeRtcTime FAILED");
        return false;
    }

    Serial.println("[CLOCK] RTC SYNC: writeRtcTime OK");

    return true;
}

void setClockTimezoneOffsetHours(int8_t hours)
{
    if (hours < -12)
        hours = -12;
    if (hours > 14)
        hours = 14;
    timezoneOffsetHours = hours;
}

int8_t getClockTimezoneOffsetHours()
{
    return timezoneOffsetHours;
}

bool getDisplayClockTm(struct tm &out)
{
    return getDisplayTm(out);
}

bool formatClock(char *out, size_t outSize)
{
    if (outSize == 0)
        return false;

    struct tm displayTm{};
    if (getDisplayTm(displayTm))
    {
        snprintf(lastClockText, sizeof(lastClockText), "%02d:%02d", displayTm.tm_hour, displayTm.tm_min);
        lastClockValid = true;
    }
    else if (!lastClockValid)
    {
        strncpy(lastClockText, "--:--", sizeof(lastClockText) - 1);
        lastClockText[sizeof(lastClockText) - 1] = '\0';
    }

    strncpy(out, lastClockText, outSize - 1);
    out[outSize - 1] = '\0';
    return lastClockValid;
}

bool setClockFromDisplayDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute)
{
    if (!ensureRtcClock())
        return false;
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 ||
        day > daysInMonth(year, month) || hour > 23 || minute > 59)
        return false;

    struct tm displayTm{};
    if (!getDisplayTm(displayTm))
    {
        if (!buildTimeToTm(displayTm))
            return false;
    }

    displayTm.tm_year = (int)year - 1900;
    displayTm.tm_mon = (int)month - 1;
    displayTm.tm_mday = (int)day;
    displayTm.tm_hour = hour;
    displayTm.tm_min = minute;
    displayTm.tm_sec = 0;

    time_t displayEpoch = epochFromUtcTm(displayTm);
    if (displayEpoch < 0)
        return false;

    time_t utcEpoch = displayEpoch - (time_t)timezoneOffsetHours * 3600;
    struct tm utcTm{};
    if (!copyGmTime(utcEpoch, utcTm))
        return false;

    if (!syncSystemClockFromTm(utcTm))
        return false;

    // Write the new UTC time to RTC.
    if (!writeRtcTime(utcTm))
        return false;

    // Verify RTC actually retained the new value.
    struct tm verifyTm{};

    if (!readRtcTime(verifyTm))
        return false;

    time_t verifyEpoch = epochFromUtcTm(verifyTm);

    if (verifyEpoch != utcEpoch)
    {
        Serial.println("[CLOCK] RTC VERIFY FAILED");

        Serial.printf(
            "[CLOCK] expected UTC: %ld\n",
            (long)utcEpoch);

        Serial.printf(
            "[CLOCK] actual UTC:   %ld\n",
            (long)verifyEpoch);

        return false;
    }

    Serial.println("[CLOCK] RTC WRITE OK");

    snprintf(
        lastClockText,
        sizeof(lastClockText),
        "%02d:%02d",
        hour,
        minute);

    lastClockValid = true;

    return true;
}
