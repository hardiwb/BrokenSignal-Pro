#include "core\Types.h"
#include "module\Browser.h"

bool loadWifiConfig(String &ssid, String &pass)
{
    ssid = "";
    pass = "";
    File f = SD.open("/Music/_radio/wifi.cfg", FILE_READ);
    if (!f)
        return false;
    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        int eq = line.indexOf('=');
        if (eq < 0)
            continue;
        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        if (key == "ssid")
            ssid = val;
        if (key == "password")
            pass = val;
    }
    f.close();
    return ssid.length() > 0;
}

void saveWifiConfig(const String &ssid, const String &pass)
{
    SD.mkdir("/Music/_radio");
    File f = SD.open("/Music/_radio/wifi.cfg", FILE_WRITE);
    if (!f)
        return;
    f.printf("ssid=%s\npassword=%s\n", ssid.c_str(), pass.c_str());
    f.close();
}

void applyWifiPowerSave()
{
    if (!wifiConnected)
        return;
    if (radioIsPlaying)
        WiFi.setSleep(WIFI_PS_NONE);
    else
        WiFi.setSleep(wifiPowerSave ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
}
