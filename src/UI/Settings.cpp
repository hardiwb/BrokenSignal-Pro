#include "UI/Settings.h"

#include "../core/State.h"
#include "../core/Config.h"
#include "../module/Clock.h"
#include "../module/Radio.h"
#include "../module/WiFi.h"
#include "UI/UI.h"
#include "UI/RadioUI.h"
#include "Themes.h"

#include <SD.h>

static bool manualClockVisible = false;
static uint16_t manualClockYear = 2000;
static uint8_t manualClockMonth = 1;
static uint8_t manualClockDay = 1;
static uint8_t manualClockHour = 0;
static uint8_t manualClockMinute = 0;
static uint8_t manualClockField = 0;

static uint8_t manualClockDaysInMonth(uint16_t year, uint8_t month)
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

static void manualClockClampDay()
{
    uint8_t maxDay = manualClockDaysInMonth(manualClockYear, manualClockMonth);
    if (manualClockDay < 1)
        manualClockDay = 1;
    if (manualClockDay > maxDay)
        manualClockDay = maxDay;
}

static String formatTimezoneValue()
{
    int8_t tz = getClockTimezoneOffsetHours();
    String value = "UTC";
    if (tz >= 0)
        value += '+';
    value += String((int)tz);
    return value;
}

static void beginManualClockEdit()
{
    struct tm tm{};
    if (getDisplayClockTm(tm))
    {
        manualClockYear = (uint16_t)(tm.tm_year + 1900);
        manualClockMonth = (uint8_t)(tm.tm_mon + 1);
        manualClockDay = (uint8_t)tm.tm_mday;
        manualClockHour = (uint8_t)tm.tm_hour;
        manualClockMinute = (uint8_t)tm.tm_min;
    }
    else
    {
        manualClockYear = 2000;
        manualClockMonth = 1;
        manualClockDay = 1;
        manualClockHour = 0;
        manualClockMinute = 0;
    }
    manualClockClampDay();
    manualClockField = 0;
    manualClockVisible = true;
}

static void adjustManualClockField(int dir)
{
    switch (manualClockField)
    {
    case 0:
        manualClockYear = (uint16_t)max(2000, min(2099, (int)manualClockYear + dir));
        manualClockClampDay();
        break;
    case 1:
        manualClockMonth = (uint8_t)((((int)manualClockMonth - 1 + dir) % 12 + 12) % 12 + 1);
        manualClockClampDay();
        break;
    case 2:
    {
        uint8_t maxDay = manualClockDaysInMonth(manualClockYear, manualClockMonth);
        manualClockDay = (uint8_t)((((int)manualClockDay - 1 + dir) % maxDay + maxDay) % maxDay + 1);
        break;
    }
    case 3:
        manualClockHour = (uint8_t)((((int)manualClockHour + dir) % 24 + 24) % 24);
        break;
    case 4:
        manualClockMinute = (uint8_t)((((int)manualClockMinute + dir) % 60 + 60) % 60);
        break;
    }
}

static void drawManualClockEditor()
{
    auto &D = M5Cardputer.Display;
    drawOverlayFrame("MANUAL CLOCK");

    D.setTextDatum(middle_right);
    D.setTextColor(T->textDim);
    D.drawString(";/.=field  +/-=chg", SCREEN_W - 6, 13, 1);

    D.setTextDatum(middle_center);
    D.setTextColor(T->textDim);
    D.drawString("ENTER=save  DEL=cancel", SCREEN_W / 2, SCREEN_H - 8, 1);

    char valueBuf[24];
    snprintf(valueBuf, sizeof(valueBuf), "%04u-%02u-%02u %02u:%02u",
             (unsigned)manualClockYear, (unsigned)manualClockMonth,
             (unsigned)manualClockDay, (unsigned)manualClockHour,
             (unsigned)manualClockMinute);

    const int fieldX[] = {18, 82, 118, 154, 190};
    const int fieldW[] = {54, 30, 30, 30, 30};
    const int fieldLen[] = {4, 2, 2, 2, 2};
    const int fieldPos[] = {0, 5, 8, 11, 14};
    const int fieldY = 66;
    const int fieldH = 18;

    D.setTextDatum(middle_left);
    for (int i = 0; i < 5; i++)
    {
        bool sel = (i == manualClockField);
        if (sel)
            D.fillRect(fieldX[i] - 3, fieldY - 2, fieldW[i], fieldH, T->selRow);
        D.setTextColor(sel ? T->accent1 : T->textBright);
        char part[5] = {};
        memcpy(part, valueBuf + fieldPos[i], fieldLen[i]);
        part[fieldLen[i]] = '\0';
        D.drawString(part, fieldX[i], fieldY, 2);
    }

    D.setTextColor(T->textDim);
    D.drawString("YEAR", 18, 42, 1);
    D.drawString("MONTH", 82, 42, 1);
    D.drawString("DAY", 118, 42, 1);
    D.drawString("HOUR", 154, 42, 1);
    D.drawString("MIN", 190, 42, 1);
}

static void saveManualClockEditor()
{
    bool ok = setClockFromDisplayDateTime(manualClockYear, manualClockMonth, manualClockDay, manualClockHour, manualClockMinute);
    manualClockVisible = false;
    if (ok)
        showHdrMsg("CLOCK SET");
    else
        showHdrMsg("CLOCK FAIL");
    drawSettingsMenu();
}

static void handleManualClockInput(Keyboard_Class::KeysState &ks)
{
    bool changed = false;
    for (auto c : ks.word)
    {
        switch (c)
        {
        case ';':
            manualClockField = (manualClockField + 4) % 5;
            changed = true;
            break;
        case '.':
            manualClockField = (manualClockField + 1) % 5;
            changed = true;
            break;
        case '+':
        case '=':
            adjustManualClockField(+1);
            changed = true;
            break;
        case '-':
            adjustManualClockField(-1);
            changed = true;
            break;
        }
    }

    if (ks.enter)
    {
        saveManualClockEditor();
        return;
    }
    if (ks.del)
    {
        manualClockVisible = false;
        drawSettingsMenu();
        return;
    }
    if (changed)
        drawManualClockEditor();
}

static void adjustSetting(int sel, int dir)
{
    if (sel == 0)
    {
        static const uint8_t opts[] = {5, 10, 15, 20, 30, 45, 60};
        const int n = sizeof(opts) / sizeof(opts[0]);
        int idx = 0;
        for (int i = 0; i < n; i++)
            if (opts[i] == seekSeconds)
            {
                idx = i;
                break;
            }
        idx = (idx + dir + n) % n;
        seekSeconds = opts[idx];
    }
    else if (sel == 1)
    {
        wifiPowerSave = !wifiPowerSave;
        applyWifiPowerSave();
    }
    else if (sel == 2)
    {
        static const uint8_t opts[] = {16, 64, 128, 200, 255};
        const int n = sizeof(opts) / sizeof(opts[0]);
        int idx = 0;
        for (int i = 0; i < n; i++)
            if (opts[i] == screenBrightness)
            {
                idx = i;
                break;
            }
        idx = (idx + dir + n) % n;
        screenBrightness = opts[idx];
        if (screenOn)
            M5Cardputer.Display.setBrightness(screenBrightness);
    }
    else if (sel == 3)
    {
        static const uint16_t opts[] = {0, 15, 30, 60, 120};
        const int n = sizeof(opts) / sizeof(opts[0]);
        int idx = 0;
        for (int i = 0; i < n; i++)
            if (opts[i] == autoScreenOffSec)
            {
                idx = i;
                break;
            }
        idx = (idx + dir + n) % n;
        autoScreenOffSec = opts[idx];
    }
    else if (sel == 5)
    {
        setClockTimezoneOffsetHours((int8_t)(getClockTimezoneOffsetHours() + dir));
    }
    settingsDirty = true;
    settingsDirtyMs = millis();
}

void drawSettingsMenu()
{
    if (manualClockVisible)
    {
        drawManualClockEditor();
        return;
    }

    auto &D = M5Cardputer.Display;
    drawOverlayFrame("SETTINGS");

    D.setTextDatum(middle_right);
    D.setTextColor(T->textDim);
    D.drawString("DEL=exit  ;/.=sel  +/-=chg", SCREEN_W - 6, 13, 1);

    const int NROWS = 7;
    const char *labels[NROWS] = {
        "Seek step",
        "WiFi power save",
        "Brightness",
        "Auto screen off",
        "Sync clock",
        "Timezone",
        "Manual clock"};
    String values[NROWS];
    values[0] = String(seekSeconds) + "s";
    values[1] = wifiPowerSave ? "ON" : "OFF";
    values[2] = String(screenBrightness);
    values[3] = (autoScreenOffSec == 0) ? String("OFF") : (String(autoScreenOffSec) + "s");
    values[4] = "ENTER";
    values[5] = formatTimezoneValue();
    values[6] = "ENTER";

    const int listY = 30;
    const int rowH = 14;
    for (int i = 0; i < NROWS; i++)
    {
        int y = listY + i * rowH;
        bool sel = (i == settingsSel);
        if (sel)
            D.fillRect(5, y, SCREEN_W - 10, rowH, T->selRow);
        D.setTextDatum(middle_left);
        D.setTextColor(sel ? T->accent2 : T->textMid);
        D.drawString(labels[i], 12, y + rowH / 2, 1);
        D.setTextDatum(middle_right);
        D.setTextColor(sel ? T->accent1 : T->textBright);
        D.drawString(values[i], SCREEN_W - 12, y + rowH / 2, 1);
    }
}

void enterSettingsMenu()
{
    settingsMenuVisible = true;
    settingsSel = 0;
    manualClockVisible = false;
    drawSettingsMenu();
}

void exitSettingsMenu()
{
    settingsMenuVisible = false;
    manualClockVisible = false;
    saveSettings();
    settingsDirty = false;
    if (webRadioMode)
        drawRadioAll();
    else
        drawAll();
}

void handleSettingsInput(Keyboard_Class::KeysState &ks)
{
    if (manualClockVisible)
    {
        handleManualClockInput(ks);
        return;
    }

    const int NSETTINGS = 7;
    if (ks.del)
    {
        exitSettingsMenu();
        return;
    }
    bool changed = false;
    for (auto c : ks.word)
    {
        switch (c)
        {
        case 'm':
        case 'M':
            exitSettingsMenu();
            return;
        case ';':
            settingsSel = (settingsSel - 1 + NSETTINGS) % NSETTINGS;
            changed = true;
            break;
        case '.':
            settingsSel = (settingsSel + 1) % NSETTINGS;
            changed = true;
            break;
        case '+':
        case '=':
            adjustSetting(settingsSel, +1);
            changed = true;
            break;
        case '-':
            adjustSetting(settingsSel, -1);
            changed = true;
            break;
        }
    }
    if (ks.enter)
    {
        if (settingsSel == 1)
        {
            wifiPowerSave = !wifiPowerSave;
            applyWifiPowerSave();
            settingsDirty = true;
            settingsDirtyMs = millis();
        }
else if (settingsSel == 4)
{
    if (!wifiConnected)
    {
        String ssid;
        String pass;

        if (!loadWifiConfig(ssid, pass) ||
            ssid.length() == 0)
        {
            showHdrMsg("NO WIFI CONFIG");
        }
        else if (!connectWifi(ssid, pass))
        {
            showHdrMsg("WIFI FAIL");
        }
        else if (syncClockFromNTP())
        {
            showHdrMsg("CLOCK SYNCED");
        }
        else
        {
            showHdrMsg("NTP FAIL");
        }
    }
    else if (syncClockFromNTP())
    {
        showHdrMsg("CLOCK SYNCED");
    }
    else
    {
        showHdrMsg("NTP FAIL");
    }
}
        else if (settingsSel == 6)
        {
            beginManualClockEdit();
            drawManualClockEditor();
            return;
        }
        changed = true;
    }
    if (changed)
        drawSettingsMenu();
}