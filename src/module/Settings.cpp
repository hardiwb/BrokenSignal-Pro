#include "module/Settings.h"

#include "../core/Config.h"
#include "../core/Keyboard.h"
#include "../core/State.h"
#include "../core/System.h"
#include "../module/Help.h"
#include "../module/Radio.h"
#include "../module/service/Clock.h"
#include "../module/service/WiFi.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"
#include "UI/Themes.h"

#include <SD.h>

static bool manualClockVisible = false;
static String manualClockInput = "";
static const int SETTINGS_COUNT = 10;
static int settingsScrollTop = 0;

void drawSettingsMenu();
static void drawManualClockEditor();

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

static String formatTimezoneValue()
{
    int8_t tz = getClockTimezoneOffsetHours();
    String value = "UTC";
    if (tz >= 0)
        value += '+';
    value += String((int)tz);
    return value;
}

static const char *settingsLabel(int index)
{
    static const char *labels[SETTINGS_COUNT] = {
        "Seek step",
        "WiFi power save",
        "Brightness",
        "Auto screen off",
        "Deep sleep",
        "Playback off",
        "Sync clock",
        "Timezone",
        "Manual clock",
        "WiFi menu"};

    if (index < 0 || index >= SETTINGS_COUNT)
        return "";

    return labels[index];
}

static String settingsValue(int index)
{
    switch (index)
    {
    case 0:
        return String(seekSeconds) + "s";
    case 1:
        return wifiPowerSave ? "ON" : "OFF";
    case 2:
        return String(screenBrightness);
    case 3:
        return autoScreenOffSec == 0 ? String("OFF") : String(autoScreenOffSec) + "s";
    case 4:
    case 5:
    {
        const uint32_t timer = index == 4 ? deepSleepSec : playbackOffSec;
        if (timer == 0)
            return "NONE";
        if (timer < 3600)
            return String(timer / 60) + "m";
        return String(timer / 3600) + "h";
    }
    case 6:
    case 8:
    case 9:
        return "ENTER";
    case 7:
        return formatTimezoneValue();
    default:
        return "";
    }
}

static void clampSettingsSelection()
{
    settingsSel = max(0, min(settingsSel, SETTINGS_COUNT - 1));

    if (settingsSel < settingsScrollTop)
        settingsScrollTop = settingsSel;
    if (settingsSel >= settingsScrollTop + LIST_VISIBLE_ITEM)
        settingsScrollTop = settingsSel - LIST_VISIBLE_ITEM + 1;

    settingsScrollTop = max(0, settingsScrollTop);
}

static ListModel buildSettingsListModel()
{
    ListModel model;
    model.selected = settingsSel;
    model.scrollTop = settingsScrollTop;

    for (int i = 0; i < SETTINGS_COUNT; i++)
    {
        ListItemModel item;
        item.type = ListItemType::Property;
        item.label = settingsLabel(i);
        item.value = settingsValue(i);
        item.isSelected = i == settingsSel;
        model.items.push_back(item);
    }

    return model;
}

static void drawSettingsHeader()
{
    HeaderModel model;
    model.mode = "SETTINGS";
    model.title = "SYSTEM";
    model.cursor = true;
    drawHeader(model);
}

static void drawSettingsList()
{
    clampSettingsSelection();
    drawList(buildSettingsListModel());
}

static void drawSettingsRow(int index)
{
    clampSettingsSelection();

    ListModel model =
        buildSettingsListModel();

    drawListRow(
        model,
        index);
}

static void redrawSettingsSelection(
    int oldSelected,
    int oldScrollTop)
{
    clampSettingsSelection();

    if (settingsScrollTop != oldScrollTop)
    {
        drawSettingsList();
        return;
    }

    ListModel model =
        buildSettingsListModel();

    drawListSelection(
        model,
        oldSelected,
        settingsSel);
}

static void drawSettingsFooter()
{
    FooterModel model;
    model.left = "[;/.]Sel [+/-]Change [Esc]Close";
    model.center = "";
    model.battery = footerBatteryText();
    drawFooter(model);
}

static void beginManualClockEdit()
{
    struct tm tm{};
    if (getDisplayClockTm(tm))
    {
        char buf[20];
        snprintf(
            buf,
            sizeof(buf),
            "%04d-%02d-%02d %02d:%02d",
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min);
        manualClockInput = String(buf);
    }
    else
    {
        manualClockInput = "2000-01-01 00:00";
    }
    manualClockVisible = true;
    drawManualClockEditor();
}

static bool parseManualClockInput(
    uint16_t &year,
    uint8_t &month,
    uint8_t &day,
    uint8_t &hour,
    uint8_t &minute)
{
    int y = 0;
    int mo = 0;
    int d = 0;
    int h = 0;
    int mi = 0;

    if (sscanf(manualClockInput.c_str(), "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5)
        return false;

    if (y < 2000 || y > 2099 || mo < 1 || mo > 12 || h < 0 || h > 23 || mi < 0 || mi > 59)
        return false;

    uint8_t maxDay = daysInMonth((uint16_t)y, (uint8_t)mo);
    if (d < 1 || d > maxDay)
        return false;

    year = (uint16_t)y;
    month = (uint8_t)mo;
    day = (uint8_t)d;
    hour = (uint8_t)h;
    minute = (uint8_t)mi;
    return true;
}

static void drawManualClockEditor()
{
    OverlayModel model;
    model.type = OverlayType::TextInput;
    model.title = "MANUAL CLOCK";
    model.prompt = "YYYY-MM-DD HH:MM";
    model.value = manualClockInput;
    model.confirmText = "[Esc]Cancel   [Ent]Save";
    drawOverlay(model);
}

static void saveManualClockEditor()
{
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;

    bool ok =
        parseManualClockInput(year, month, day, hour, minute) &&
        setClockFromDisplayDateTime(year, month, day, hour, minute);

    manualClockVisible = false;
    if (ok)
        showHdrMsg("CLOCK SET");
    else
        showHdrMsg("CLOCK FAIL");
    drawSettingsMenu();
}

static void handleManualClockInput(Keyboard_Class::KeysState &ks)
{
    if (ks.enter)
    {
        saveManualClockEditor();
        return;
    }
    if (keyboardBackPressed(ks) || ks.opt)
    {
        manualClockVisible = false;
        drawSettingsMenu();
        return;
    }

    if (ks.del)
    {
        if (manualClockInput.length() > 0)
        {
            manualClockInput.remove(manualClockInput.length() - 1);
            drawOverlayInputValue(manualClockInput);
        }
        return;
    }

    bool changed = false;
    for (auto c : ks.word)
    {
        if (keyboardTextInputChar(ks, c) && manualClockInput.length() < 16)
        {
            manualClockInput += c;
            changed = true;
        }
    }

    if (changed)
        drawOverlayInputValue(manualClockInput);
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
    else if (sel == 4)
    {
        static const uint32_t opts[] = {0, 1800, 3600, 7200, 10800};
        const int n = sizeof(opts) / sizeof(opts[0]);
        int idx = 0;
        for (int i = 0; i < n; i++)
            if (opts[i] == deepSleepSec)
            {
                idx = i;
                break;
            }
        idx = (idx + dir + n) % n;
        deepSleepSec = opts[idx];
    }
    else if (sel == 5)
    {
        static const uint32_t opts[] = {0, 1800, 3600, 7200, 10800};
        const int n = sizeof(opts) / sizeof(opts[0]);
        int idx = 0;
        for (int i = 0; i < n; i++)
            if (opts[i] == playbackOffSec)
            {
                idx = i;
                break;
            }
        idx = (idx + dir + n) % n;
        playbackOffSec = opts[idx];
    }
    else if (sel == 7)
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

    drawSettingsHeader();
    drawSettingsList();
    drawSettingsFooter();
}

bool settingsInputOverlayActive()
{
    return manualClockVisible;
}

void enterSettingsMenu()
{
    settingsMenuVisible = true;
    settingsSel = 0;
    settingsScrollTop = 0;
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

    if (keyboardBackPressed(ks) || ks.opt || ks.del)
    {
        exitSettingsMenu();
        return;
    }
    bool changed = false;
    for (auto c : ks.word)
    {
        switch (c)
        {
        case ';':
        {
            int oldSelected =
                settingsSel;
            int oldScrollTop =
                settingsScrollTop;

            settingsSel = (settingsSel - 1 + SETTINGS_COUNT) % SETTINGS_COUNT;
            redrawSettingsSelection(
                oldSelected,
                oldScrollTop);
            return;
        }

        case '.':
        {
            int oldSelected =
                settingsSel;
            int oldScrollTop =
                settingsScrollTop;

            settingsSel = (settingsSel + 1) % SETTINGS_COUNT;
            redrawSettingsSelection(
                oldSelected,
                oldScrollTop);
            return;
        }

        case 'h':
        case 'H':
            toggleHelp();
            return;

        case '+':
        case '=':
            adjustSetting(settingsSel, +1);
            drawSettingsRow(settingsSel);
            return;

        case '-':
            adjustSetting(settingsSel, -1);
            drawSettingsRow(settingsSel);
            return;
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
        else if (settingsSel == 6)
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
        else if (settingsSel == 8)
        {
            beginManualClockEdit();
            return;
        }
        else if (settingsSel == 9)
        {
            settingsMenuVisible = false;
            manualClockVisible = false;
            saveSettings();
            settingsDirty = false;
            openWifiMenu();
            return;
        }
        changed = true;
    }
    if (changed)
        drawSettingsMenu();
}
