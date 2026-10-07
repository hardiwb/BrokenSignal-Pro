#include <SD.h>
#include <M5Cardputer.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "core/System.h"
#include "core/State.h"
#include "core/AppRegistry.h"
#include "core/AppRuntime.h"
#include "core/SurfaceManager.h"
#include "core/Config.h"
#include "UI/Themes.h"
#include "UI/Toast.h"
#include "apps/music/MusicPlayer.h"
#include "apps/calculator/Calculator.h"
#include "apps/notes/Notes.h"
#include "apps/radio/Radio.h"
#include "apps/bluetoothkeyboard/BluetoothKeyboard.h"
#include "apps/macropad/MacroPad.h"
#include "module/shell/Settings.h"
#include "module/shell/Applications.h"
#include "module/shell/Options.h"
#include "module/shell/Help.h"
#include "module/shell/Debug.h"
#include "module/service/WiFi.h"
#include "module/service/Clock.h"
#include "module/service/Bluetooth.h"

void drawCurrentScreen()
{
    if (optionsMenuVisible)
    {
        drawOptionsMenu();
        return;
    }

    if (helpVisible)
    {
        drawHelp();
        return;
    }

    if (applicationsMenuVisible)
    {
        drawApplicationsMenu();
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

    if (bluetoothKeyboardModalActive())
    {
        drawBluetoothKeyboardApp();
        return;
    }

    if (macroPadModalActive())
    {
        drawMacroPadApp();
        return;
    }

    if (notesMoveDateInputActive())
    {
        drawNotesMoveDateEditor();
        return;
    }

    if (notesEditorVisible())
    {
        drawNotesEditor();
        return;
    }

    if (calculatorOverlayActive())
    {
        drawCalculator();
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

    appRuntimeDrawForeground();
}

void drawAll()
{
    drawCurrentScreen();
}

void showHdrMsg(const char *msg)
{
    hdrMsg = String(msg);
    hdrMsgEnd = millis() + 1000;

    const bool hostScreenVisible =
        !surfaceBlocksHostInput(resolveActiveSurface());
    if (!hostScreenVisible)
        return;

    drawCurrentScreen();
}

void showVolumeMessage()
{
    char buf[16];
    snprintf(buf, sizeof(buf), "VOL %d%%", (volume * 100) / 255);
    showHdrMsg(buf);
}

void adjustSystemVolume(int direction)
{
    volume = (uint8_t)constrain((int)volume + direction * 10, 0, 255);
    M5Cardputer.Speaker.setVolume(volume);
    settingsDirty = true;
    settingsDirtyMs = millis();

    char buf[16];
    snprintf(buf, sizeof(buf), "VOL %d%%", (volume * 100) / 255);
    showHdrMsg(buf);
}

void adjustSystemBrightness(int direction)
{
    static const uint8_t options[] = {16, 64, 128, 200, 255};
    constexpr int count = sizeof(options) / sizeof(options[0]);

    int index = 0;
    int bestDistance = abs((int)screenBrightness - (int)options[0]);
    for (int i = 1; i < count; ++i)
    {
        const int distance = abs((int)screenBrightness - (int)options[i]);
        if (distance < bestDistance)
        {
            index = i;
            bestDistance = distance;
        }
    }

    index = (index + direction + count) % count;
    screenBrightness = options[index];
    if (screenOn)
        M5Cardputer.Display.setBrightness(screenBrightness);

    settingsDirty = true;
    settingsDirtyMs = millis();

    char buf[16];
    snprintf(buf, sizeof(buf), "BRI %d%%", (screenBrightness * 100) / 255);
    showHdrMsg(buf);
}

void setTheme(uint8_t idx)
{
    if (idx >= THEME_COUNT)
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

        if (key == "theme" && val >= 0 && val < THEME_COUNT)
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

        if (key == "swapaltopt")
            swapAltOpt = (val != 0);

        if (key == "webauth")
            localWebAuthEnabled = (val != 0);

        if (key == "brightness" && val >= 0 && val <= 255)
            screenBrightness = (uint8_t)val;

        if (key == "autoscreenoff" && val >= 0 && val <= 600)
            autoScreenOffSec = (uint16_t)val;

        if (key == "deepsleep" && val >= 0 && val <= 10800)
            deepSleepSec = (uint32_t)val;

        if (key == "playbackoff" && val >= 0 && val <= 10800)
            playbackOffSec = (uint32_t)val;

        if (key == "lastapp" && val >= 0 && val <= 255)
        {
            const HostApp savedApp = (HostApp)val;
            if (appIndex(savedApp) >= 0)
                lastOpenedApp = savedApp;
        }

        if (key == "calcdecimals" &&
            (val == -1 || val == 0 || val == 2 || val == 4 || val == 6))
            calculatorDecimalPlaces = (int8_t)val;

        if (key == "calcrounding" && val >= 0 && val <= 2)
            calculatorRoundingMode = (uint8_t)val;

        if (key == "calcthousands")
            calculatorThousandsSeparator = (val != 0);

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
    f.printf("swapaltopt=%d\n", swapAltOpt ? 1 : 0);
    f.printf("webauth=%d\n", localWebAuthEnabled ? 1 : 0);
    f.printf("brightness=%d\n", screenBrightness);
    f.printf("autoscreenoff=%d\n", autoScreenOffSec);
    f.printf("deepsleep=%lu\n", (unsigned long)deepSleepSec);
    f.printf("playbackoff=%lu\n", (unsigned long)playbackOffSec);
    f.printf("lastapp=%d\n", (int)lastOpenedApp);
    f.printf("calcdecimals=%d\n", (int)calculatorDecimalPlaces);
    f.printf("calcrounding=%d\n", calculatorRoundingMode);
    f.printf("calcthousands=%d\n", calculatorThousandsSeparator ? 1 : 0);
    f.printf("timezone=%d\n", (int)getClockTimezoneOffsetHours());
    f.close();
}

void enterDeepSleep()
{
    // BtnG0 is GPIO0 on both Cardputer and Cardputer ADV. It is normally
    // pulled high; pressing it pulls the RTC-capable pin low.
    pinMode(GPIO_NUM_0, INPUT_PULLUP);
    rtc_gpio_pulldown_dis(GPIO_NUM_0);
    rtc_gpio_pullup_en(GPIO_NUM_0);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);

    // Release active transports and peripherals before removing the display.
    BluetoothService::shutdown();
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    M5Cardputer.Speaker.end();
    SD.end();

    M5Cardputer.Display.setBrightness(0);
    M5Cardputer.Display.sleep();
    delay(20);
    esp_deep_sleep_start();
}

bool wokeFromG0DeepSleep()
{
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
}
