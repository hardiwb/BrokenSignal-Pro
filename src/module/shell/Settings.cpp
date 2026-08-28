#include "module/shell/Settings.h"

#include "core/Config.h"
#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/shell/Debug.h"
#include "module/shell/Help.h"
#include "apps/radio/Radio.h"
#include "module/service/Clock.h"
#include "module/service/WiFi.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"
#include "UI/Themes.h"

#include <SD.h>

static bool manualClockVisible = false;
static String manualClockTime = "";
static String manualClockDate = "";
static int manualClockField = 0;
static int manualClockTimeCursor = 0;
static int manualClockDateCursor = 0;
static const int SETTINGS_COUNT = 12;
static int settingsScrollTop = 0;

enum SettingRow
{
    SettingBrightness,
    SettingVolume,
    SettingScreenOff,
    SettingDeepSleep,
    SettingPlaybackTimer,
    SettingTheme,
    SettingTimezone,
    SettingSyncClock,
    SettingManualClock,
    SettingWifiPowerSave,
    SettingDebug,
    SettingWifi
};

void drawSettingsMenu();
static void drawManualClockEditor(bool inputOnly = false);

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
    String value;
    if (tz >= 0)
        value += '+';
    value += String((int)tz);
    return value;
}

static const char *settingsLabel(int index)
{
    static const char *labels[SETTINGS_COUNT] = {
        "Brightness",
        "Volume",
        "Screen off timer",
        "Deep Sleep timer",
        "Playback timer",
        "Theme",
        "Time Zone",
        "Sync Clock",
        "Manual Clock",
        "WiFi power save",
        "Debug",
        "WiFi"};

    if (index < 0 || index >= SETTINGS_COUNT)
        return "";

    return labels[index];
}

static String settingsValue(int index)
{
    switch (index)
    {
    case SettingBrightness:
        return String(screenBrightness);
    case SettingVolume:
        return String(((int)volume * 100 + 127) / 255) + "%";
    case SettingScreenOff:
        return autoScreenOffSec == 0 ? String("Off") : String(autoScreenOffSec) + "s";
    case SettingDeepSleep:
    case SettingPlaybackTimer:
    {
        const uint32_t timer = index == SettingDeepSleep ? deepSleepSec : playbackOffSec;
        if (timer == 0)
            return "Off";
        if (timer < 3600)
            return String(timer / 60) + "m";
        return String(timer / 3600) + "h";
    }
    case SettingTheme:
        return String(themeIdx + 1);
    case SettingTimezone:
        return formatTimezoneValue();
    case SettingSyncClock:
    case SettingManualClock:
    case SettingDebug:
    case SettingWifi:
        return "Enter";
    case SettingWifiPowerSave:
        return wifiPowerSave ? "On" : "Off";
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
    model.appHeaderTag = "SYSTEM";
    model.appHeaderTitle = "Control Panel";
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
    model.left = "[;/.]Sel [,/]Change [Ctrl]Close";
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
            "%02d:%02d:%02d",
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec);
        manualClockTime = String(buf);
        snprintf(
            buf,
            sizeof(buf),
            "%04d-%02d-%02d",
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday);
        manualClockDate = String(buf);
    }
    else
    {
        manualClockTime = "00:00:00";
        manualClockDate = "2000-01-01";
    }
    manualClockField = 0;
    manualClockTimeCursor = manualClockTime.length();
    manualClockDateCursor = manualClockDate.length();
    manualClockVisible = true;
    drawManualClockEditor();
}

static bool parseManualClockInput(
    uint16_t &year,
    uint8_t &month,
    uint8_t &day,
    uint8_t &hour,
    uint8_t &minute,
    uint8_t &second)
{
    int y = 0;
    int mo = 0;
    int d = 0;
    int h = 0;
    int mi = 0;
    int s = 0;

    if (sscanf(manualClockDate.c_str(), "%d-%d-%d", &y, &mo, &d) != 3 ||
        sscanf(manualClockTime.c_str(), "%d:%d:%d", &h, &mi, &s) != 3)
        return false;

    if (y < 2000 || y > 2099 || mo < 1 || mo > 12 || h < 0 || h > 23 ||
        mi < 0 || mi > 59 || s < 0 || s > 59)
        return false;

    uint8_t maxDay = daysInMonth((uint16_t)y, (uint8_t)mo);
    if (d < 1 || d > maxDay)
        return false;

    year = (uint16_t)y;
    month = (uint8_t)mo;
    day = (uint8_t)d;
    hour = (uint8_t)h;
    minute = (uint8_t)mi;
    second = (uint8_t)s;
    return true;
}

static void drawManualClockEditor(bool inputOnly)
{
    OverlayModel model;
    model.type = OverlayType::TwoFieldInput;
    model.title = "Time and Date";
    model.value = manualClockTime;
    model.secondValue = manualClockDate;
    model.activeField = manualClockField;
    model.cursorIndex = manualClockField == 0
                            ? manualClockTimeCursor
                            : manualClockDateCursor;
    model.centerFirstField = true;
    model.helperText = "[Tab]Switch [Fn L/R]Cursor";
    model.confirmText = "[Esc]Cancel [Ok]Save";
    if (inputOnly)
        drawOverlayTwoFieldInputValues(model);
    else
        drawOverlay(model);
}

static void saveManualClockEditor()
{
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;

    bool ok =
        parseManualClockInput(year, month, day, hour, minute, second) &&
        setClockFromDisplayDateTime(year, month, day, hour, minute, second);

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
    if (keyboardBackPressed(ks))
    {
        manualClockVisible = false;
        drawSettingsMenu();
        return;
    }

    if (ks.tab)
    {
        manualClockField = 1 - manualClockField;
        drawManualClockEditor(true);
        return;
    }

    if (ks.fn)
    {
        for (auto c : ks.word)
        {
            if (c == ',')
            {
                int &cursor = manualClockField == 0
                                  ? manualClockTimeCursor
                                  : manualClockDateCursor;
                cursor = max(0, cursor - 1);
                drawManualClockEditor(true);
                return;
            }
            if (c == '/')
            {
                String &value = manualClockField == 0
                                    ? manualClockTime
                                    : manualClockDate;
                int &cursor = manualClockField == 0
                                  ? manualClockTimeCursor
                                  : manualClockDateCursor;
                cursor = min((int)value.length(), cursor + 1);
                drawManualClockEditor(true);
                return;
            }
        }
        return;
    }

    String &activeValue = manualClockField == 0
                              ? manualClockTime
                              : manualClockDate;
    int &activeCursor = manualClockField == 0
                            ? manualClockTimeCursor
                            : manualClockDateCursor;

    if (ks.del)
    {
        if (activeCursor > 0)
        {
            activeValue.remove(activeCursor - 1, 1);
            activeCursor--;
            drawManualClockEditor(true);
        }
        return;
    }

    for (auto c : ks.word)
    {
        const bool accepted = manualClockField == 0
                                  ? ((c >= '0' && c <= '9') || c == ':')
                                  : ((c >= '0' && c <= '9') || c == '-');
        const int maxLength = manualClockField == 0 ? 8 : 10;
        if (keyboardTextInputChar(ks, c) && accepted &&
            (int)activeValue.length() < maxLength)
        {
            activeValue = activeValue.substring(0, activeCursor) +
                          String(c) +
                          activeValue.substring(activeCursor);
            activeCursor++;
            drawManualClockEditor(true);
        }
    }
}

static void adjustSetting(int sel, int dir)
{
    if (sel == SettingBrightness)
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
    else if (sel == SettingVolume)
    {
        volume = (uint8_t)constrain((int)volume + dir * 10, 0, 255);
        M5Cardputer.Speaker.setVolume(volume);
    }
    else if (sel == SettingScreenOff)
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
    else if (sel == SettingDeepSleep)
    {
        static const uint32_t opts[] = {0, 300, 900, 1800, 3600, 7200, 10800};
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
    else if (sel == SettingPlaybackTimer)
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
    else if (sel == SettingTheme)
    {
        themeIdx = (themeIdx + dir + 5) % 5;
        T = THEMES[themeIdx];
    }
    else if (sel == SettingTimezone)
    {
        setClockTimezoneOffsetHours((int8_t)(getClockTimezoneOffsetHours() + dir));
    }
    else if (sel == SettingWifiPowerSave)
    {
        wifiPowerSave = !wifiPowerSave;
        applyWifiPowerSave();
    }
    settingsDirty = true;
    settingsDirtyMs = millis();
}

static bool settingSupportsAdjustment(int sel)
{
    return sel == SettingBrightness || sel == SettingVolume ||
           sel == SettingScreenOff || sel == SettingDeepSleep ||
           sel == SettingPlaybackTimer || sel == SettingTheme ||
           sel == SettingTimezone || sel == SettingWifiPowerSave;
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

void cancelSettingsInputOverlay()
{
    manualClockVisible = false;
    drawSettingsMenu();
}

void enterSettingsMenu()
{
    applicationsMenuVisible = false;
    helpVisible = false;
    debugOverlayVisible = false;
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

    if ((ks.ctrl && ks.word.empty()) || keyboardBackPressed(ks))
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

        case '/':
            if (settingSupportsAdjustment(settingsSel))
            {
                adjustSetting(settingsSel, +1);
                if (settingsSel == SettingTheme)
                    drawSettingsMenu();
                else
                    drawSettingsRow(settingsSel);
            }
            return;

        case ',':
            if (settingSupportsAdjustment(settingsSel))
            {
                adjustSetting(settingsSel, -1);
                if (settingsSel == SettingTheme)
                    drawSettingsMenu();
                else
                    drawSettingsRow(settingsSel);
            }
            return;
        }
    }
    if (ks.enter)
    {
        if (settingsSel == SettingSyncClock)
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
        else if (settingsSel == SettingManualClock)
        {
            beginManualClockEdit();
            return;
        }
        else if (settingsSel == SettingWifi)
        {
            settingsMenuVisible = false;
            manualClockVisible = false;
            saveSettings();
            settingsDirty = false;
            openWifiMenu();
            return;
        }
        else if (settingsSel == SettingDebug)
        {
            toggleDebug();
            return;
        }
        changed = true;
    }
    if (changed)
        drawSettingsMenu();
}
