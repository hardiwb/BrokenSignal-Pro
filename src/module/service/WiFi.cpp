#include "module/service/WiFi.h"

#include <M5Cardputer.h>
#include <SD.h>

#include "core/Config.h"
#include "core/Keyboard.h"
#include "module/shell/Help.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"
#include "UI/Themes.h"

#if DEBUG_SERIAL
#define WDBG(...) Serial.printf(__VA_ARGS__)
#else
#define WDBG(...) ((void)0)
#endif

static String wifiStatusText()
{
    if (!wifiConnected)
        return "NOT CONNECTED";

    if (wifiSSID.length() == 0)
        return "CONNECTED";

    return wifiSSID;
}

static unsigned long wifiMarqueeStartMs()
{
    static int lastSelected = -1;
    static unsigned long startMs = 0;

    if (lastSelected != wifiNetSel)
    {
        lastSelected = wifiNetSel;
        startMs = millis();
    }

    return startMs;
}

static void populateWifiListModel(ListModel &model)
{
    model.selected = wifiNetSel;
    model.scrollTop = wifiNetScroll;
    model.marqueeStartMs = wifiMarqueeStartMs();

    for (int i = 0; i < wifiNetCount; i++)
    {
        ListItemModel item;
        item.label = wifiNets[i].ssid;
        item.value = String(wifiNets[i].rssi) + "dB";
        item.type = ListItemType::Property;
        if (wifiNets[i].encrypted)
            item.value = "*" + item.value;
        item.isSelected = i == wifiNetSel;
        model.items.push_back(item);
    }
}

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

void scanWifiNetworks()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int n = WiFi.scanNetworks(false, false, false, 300);
    wifiNetCount = 0;
    for (int i = 0; i < n && wifiNetCount < WIFI_SCAN_MAX; i++)
    {
        wifiNets[wifiNetCount].ssid = WiFi.SSID(i);
        wifiNets[wifiNetCount].rssi = WiFi.RSSI(i);
        wifiNets[wifiNetCount].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        if (wifiNets[wifiNetCount].ssid.length() > 0)
            wifiNetCount++;
    }
    WiFi.scanDelete();
}

bool connectWifi(const String &ssid, const String &pass)
{
    String truncSsid = ssid;
    if ((int)truncSsid.length() > 26)
        truncSsid = truncSsid.substring(0, 25) + ">";

    auto drawConnBody = [&](uint8_t dotCount, bool fullDraw)
    {
        char msg[18];
        snprintf(msg, sizeof(msg), "CONNECTING%.*s", dotCount, "...");

        OverlayModel model;
        model.type = OverlayType::Message;
        model.title = "WIFI CONNECT";
        model.items.push_back(String(msg));
        model.items.push_back(truncSsid);
        model.confirmText = "ESC to cancel";
        if (fullDraw)
            drawOverlay(model);
        else
            drawOverlayMessageLine(model, 0);
    };

    drawConnBody(0, true);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long t = millis();
    unsigned long lastAnim = millis();
    uint8_t dots = 0;

    while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_TIMEOUT)
    {
        M5Cardputer.update();
        if (millis() - lastAnim >= 500)
        {
            dots = (dots % 3) + 1;
            drawConnBody(dots, false);
            lastAnim = millis();
        }
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
        {
            Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
            if (keyboardBackPressed(ks))
            {
                WiFi.disconnect();
                return false;
            }
        }
        delay(50);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        wifiConnected = true;
        wifiSSID = ssid;
        applyWifiPowerSave();
        WDBG("[WIFI] connected '%s' rssi=%d ip=%s free=%u\n",
             ssid.c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str(), ESP.getFreeHeap());
        saveWifiConfig(ssid, pass);
        return true;
    }

    WiFi.disconnect();
    wifiConnected = false;

    OverlayModel failed;
    failed.type = OverlayType::Message;
    failed.title = "WIFI CONNECT";
    failed.items.push_back("CONNECT FAILED");
    failed.items.push_back(truncSsid);
    failed.confirmText = "Returning to WiFi menu";
    drawOverlay(failed);

    delay(2000);
    return false;
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

void drawWifiHeader()
{
    HeaderModel model;
    model.appHeaderTag = "WIFI";
    model.appHeaderTitle = wifiStatusText();
    model.cursor = cursorVisible;

    drawHeader(model);
}

void drawWifiList()
{
    if (wifiNetCount == 0)
    {
        ListModel model;
        ListItemModel item;
        item.label = "No networks found";
        model.items.push_back(item);
        drawList(model);
        return;
    }

    ListModel model;
    populateWifiListModel(model);
    drawList(model);
}

void drawWifiRow(int idx)
{
    if (idx < 0 || idx >= wifiNetCount)
        return;

    ListModel model;
    populateWifiListModel(model);
    drawListRow(model, idx);
}

static void redrawWifiSelection(int oldSel, int oldScroll)
{
    if (wifiNetScroll != oldScroll)
    {
        drawWifiList();
        return;
    }

    drawWifiRow(oldSel);
    drawWifiRow(wifiNetSel);
}

void drawWifiFooter()
{
    FooterModel model;
    model.left = "[R]Refresh";
    model.center = "";
    model.battery = footerBatteryText();
    drawFooter(model);
}

void drawWifiMenu()
{
    drawWifiHeader();
    drawWifiList();
    drawWifiFooter();
}

void drawWifiPassOverlay(bool inputOnly)
{
    OverlayModel model;
    model.type = OverlayType::PasswordInput;
    model.title = "WIFI PASSWORD";
    model.prompt = "Net: " + inputSaved;
    model.value = String(inputBuf);
    model.confirmText = "[Ok]Connect   [Del]Back   [Esc]Cancel";
    model.passwordMode = true;

    if (inputOnly)
        drawOverlayInputValue(
            model.value,
            model.passwordMode);
    else
        drawOverlay(model);
}

void showWifiMenu()
{
    HeaderModel header;
    header.appHeaderTag = "WIFI";
    header.appHeaderTitle = "SCANNING";
    header.cursor = true;
    drawHeader(header);

    ListModel list;
    ListItemModel item;
    item.label = "Scanning networks...";
    item.isSelected = true;
    list.items.push_back(item);
    drawList(list);

    drawWifiFooter();

    scanWifiNetworks();
    wifiNetSel = 0;
    wifiNetScroll = 0;
    wifiMenuVisible = true;
    drawWifiMenu();
}

void openWifiMenu()
{
    showWifiMenu();
}

void showWifiPassOverlay(const String &ssid)
{
    inputSaved = ssid;
    inputBuf[0] = '\0';
    inputLen = 0;
    wifiPassOverlayVisible = true;
    drawWifiPassOverlay();
}

void closeWifiInput()
{
    wifiMenuVisible = false;
    wifiPassOverlayVisible = false;
}

WifiStartupResult ensureWifiForRadio()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        wifiConnected = true;
        wifiSSID = WiFi.SSID();
        return WifiStartupResult::Connected;
    }

    String cfgSsid, cfgPass;
    if (loadWifiConfig(cfgSsid, cfgPass) && cfgSsid.length() > 0)
    {
        if (connectWifi(cfgSsid, cfgPass))
            return WifiStartupResult::Connected;
    }

    openWifiMenu();
    return WifiStartupResult::AwaitingInput;
}

bool wifiInputActive()
{
    return wifiMenuVisible || wifiPassOverlayVisible;
}

WifiInputResult handleWifiInput(Keyboard_Class::KeysState &ks)
{
    if (wifiMenuVisible)
    {
        for (auto c : ks.word)
        {
            if (c == ';' && wifiNetCount > 0)
            {
                int oldSel = wifiNetSel;
                int oldScroll = wifiNetScroll;

                wifiNetSel = (wifiNetSel - 1 + wifiNetCount) % wifiNetCount;
                if (wifiNetSel < wifiNetScroll)
                    wifiNetScroll = wifiNetSel;

                redrawWifiSelection(oldSel, oldScroll);
            }
            if (c == '.' && wifiNetCount > 0)
            {
                int oldSel = wifiNetSel;
                int oldScroll = wifiNetScroll;

                wifiNetSel = (wifiNetSel + 1) % wifiNetCount;
                if (wifiNetSel >= wifiNetScroll + LIST_VISIBLE_ITEM)
                    wifiNetScroll = wifiNetSel - LIST_VISIBLE_ITEM + 1;

                redrawWifiSelection(oldSel, oldScroll);
            }
            if (c == 'r' || c == 'R' || c == 's' || c == 'S')
            {
                showWifiMenu();
                return WifiInputResult::Consumed;
            }
        }

        if (ks.enter && wifiNetCount > 0)
        {
            String ssid = wifiNets[wifiNetSel].ssid;
            String cfgSsid, cfgPass;

            wifiMenuVisible = false;
            if (loadWifiConfig(cfgSsid, cfgPass) && cfgSsid == ssid)
            {
                if (connectWifi(ssid, cfgPass))
                    return WifiInputResult::Connected;

                showWifiMenu();
                return WifiInputResult::Consumed;
            }

            showWifiPassOverlay(ssid);
            return WifiInputResult::Consumed;
        }

        if (keyboardBackPressed(ks) || ks.del)
        {
            wifiMenuVisible = false;
            return wifiConnected ? WifiInputResult::ReturnToHost : WifiInputResult::ExitRequested;
        }

        return WifiInputResult::Consumed;
    }

    if (wifiPassOverlayVisible)
    {
        if (keyboardBackPressed(ks))
        {
            wifiPassOverlayVisible = false;
            showWifiMenu();
            return WifiInputResult::Consumed;
        }

        for (auto c : ks.word)
        {
            if (keyboardTextInputChar(ks, c) && inputLen < RADIO_INPUT_MAX)
            {
                inputBuf[inputLen++] = c;
                inputBuf[inputLen] = '\0';
                drawWifiPassOverlay(true);
            }
        }

        if (ks.del)
        {
            if (inputLen > 0)
            {
                inputBuf[--inputLen] = '\0';
                drawWifiPassOverlay(true);
                return WifiInputResult::Consumed;
            }

            wifiPassOverlayVisible = false;
            showWifiMenu();
            return WifiInputResult::Consumed;
        }

        if (ks.enter)
        {
            String pass = String(inputBuf);

            wifiPassOverlayVisible = false;
            if (connectWifi(inputSaved, pass))
                return WifiInputResult::Connected;

            showWifiMenu();
            return WifiInputResult::Consumed;
        }

        return WifiInputResult::Consumed;
    }

    return WifiInputResult::Ignored;
}
