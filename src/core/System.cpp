#include <SD.h>
#include <M5Cardputer.h>
#include <esp_sleep.h>
#include "core/System.h"
#include "core/State.h"
#include "core/Config.h"
#include "UI/Themes.h"
#include "UI/Toast.h"
#include "module/Player.h"
#include "module/Calculator.h"
#include "module/Notes.h"
#include "module/Radio.h"
#include "module/Settings.h"
#include "module/Help.h"
#include "module/Debug.h"
#include "module/service/WiFi.h"
#include "../module/service/Clock.h"

void drawCurrentScreen()
{
    if (calculatorVisible)
    {
        drawCalculator();
        return;
    }

    if (helpVisible)
    {
        drawHelp();
        return;
    }

    if (settingsMenuVisible)
    {
        drawSettingsMenu();
        return;
    }

    if (debugOverlayVisible)
    {
        drawDebug();
        return;
    }

    if (notesEditorVisible())
    {
        drawNotesEditor();
        return;
    }

    if (notesMode)
    {
        drawNotes();
        return;
    }

    if (wifiPassOverlayVisible)
    {
        drawWifiPassOverlay();
        return;
    }

    if (addUrlOverlayVisible)
    {
        drawAddUrlOverlay();
        return;
    }

    if (addNameOverlayVisible)
    {
        drawAddNameOverlay();
        return;
    }

    if (removeConfirmVisible)
    {
        drawRemoveConfirm();
        return;
    }

    if (wifiMenuVisible)
    {
        drawWifiMenu();
        return;
    }

    if (webRadioMode)
    {
        drawRadioAll();
        return;
    }
    drawPlayerHeader();
    pumpAudio();
    drawPlayerList();
    pumpAudio();
    drawPlayerStatus();
}

void drawAll()
{
    drawCurrentScreen();
}

void showHdrMsg(const char *msg)
{
    hdrMsg = String(msg);
    hdrMsgEnd = millis() + 1000;
    if (webRadioMode)
        drawRadioHeader();
    else
        drawPlayerHeader();
}

void setTheme(uint8_t idx)
{
    if (idx >= 5)
        return;
    themeIdx = idx;
    T = THEMES[idx];
    settingsDirty = true;
    settingsDirtyMs = millis();
    drawAll();
    showToast(T->name);
}

void toggleScreen()
{
    screenOn = !screenOn;
    M5Cardputer.Display.setBrightness(screenOn ? screenBrightness : 0);
    if (screenOn)
    {
        lastActivityMs = millis();
        drawAll();
    }
}

void wakeScreen()
{
    screenOn = true;
    M5Cardputer.Display.setBrightness(screenBrightness);
    lastActivityMs = millis();
    drawAll();
}

void loadSettings()
{
    themeIdx = 0;
    T = THEMES[0];

    File f = SD.open("/Music/settings.cfg", FILE_READ);
    if (!f)
        return;

    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();

        int eq = line.indexOf('=');
        if (eq < 0)
            continue;

        String key = line.substring(0, eq);
        int val = line.substring(eq + 1).toInt();

        if (key == "theme" && val >= 0 && val < 5)
        {
            themeIdx = val;
            T = THEMES[val];
        }

        if (key == "volume" && val >= 0 && val <= 255)
        {
            volume = (uint8_t)val;
            M5Cardputer.Speaker.setVolume(volume);
        }

        if (key == "repeat" && val >= 0 && val <= 2)
            repeatMode = (uint8_t)val;

        if (key == "shuffle")
            shuffleOn = (val != 0);

        if (key == "seek" && val >= 5 && val <= 60)
            seekSeconds = (uint8_t)val;

        if (key == "wifipowersave")
            wifiPowerSave = (val != 0);

        if (key == "brightness" && val >= 0 && val <= 255)
            screenBrightness = (uint8_t)val;

        if (key == "autoscreenoff" && val >= 0 && val <= 600)
            autoScreenOffSec = (uint16_t)val;

        if (key == "deepsleep" && val >= 0 && val <= 10800)
            deepSleepSec = (uint32_t)val;

        if (key == "playbackoff" && val >= 0 && val <= 10800)
            playbackOffSec = (uint32_t)val;

        if (key == "timezone")
            setClockTimezoneOffsetHours(
                (int8_t)max(-12, min(14, val))
            );
    }

    f.close();
}

void saveSettings()
{
    File f = SD.open("/Music/settings.cfg", FILE_WRITE);
    if (!f)
        return;
    f.printf("theme=%d\n", themeIdx);
    f.printf("volume=%d\n", volume);
    f.printf("repeat=%d\n", repeatMode);
    f.printf("shuffle=%d\n", shuffleOn ? 1 : 0);
    f.printf("seek=%d\n", seekSeconds);
    f.printf("wifipowersave=%d\n", wifiPowerSave ? 1 : 0);
    f.printf("brightness=%d\n", screenBrightness);
    f.printf("autoscreenoff=%d\n", autoScreenOffSec);
    f.printf("deepsleep=%lu\n", (unsigned long)deepSleepSec);
    f.printf("playbackoff=%lu\n", (unsigned long)playbackOffSec);
    f.printf("timezone=%d\n", (int)getClockTimezoneOffsetHours());
    f.close();
}

void enterDeepSleep()
{
    // Cardputer ADV exposes the keyboard controller interrupt on GPIO 11.
    // A key press pulls it low and wakes the ESP32 from deep sleep.
    pinMode(11, INPUT_PULLUP);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_11, 0);
    M5Cardputer.Display.sleep();
    WiFi.mode(WIFI_OFF);
    esp_deep_sleep_start();
}
