#include <SD.h>
#include <M5Cardputer.h>
#include "core/System.h"
#include "core/State.h"
#include "core/Config.h"
#include "UI/Themes.h"
#include "../module/Clock.h"

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
    f.printf("timezone=%d\n", (int)getClockTimezoneOffsetHours());
    f.close();
}
