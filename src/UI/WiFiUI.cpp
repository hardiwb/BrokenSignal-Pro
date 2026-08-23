#include "WiFiUI.h"
#include <M5Cardputer.h>
#include "../core/State.h"
#include "../core/Config.h"
#include "../module/WiFi.h"
#include "UI.h"
#include "Themes.h"
#include <SD.h>

#if DEBUG_SERIAL
#define RDBG(...) Serial.printf(__VA_ARGS__)
#else
#define RDBG(...) ((void)0)
#endif

void drawWifiOverlay()
{
    auto &D = M5Cardputer.Display;
    drawOverlayFrame("SELECT WIFI");

    const int LIST_START = 24;
    const int ROW_H = 14;
    const int ROWS_VIS = 6;
    const int FOOTER_Y = 121;

    D.setTextDatum(middle_right);
    D.setTextColor(T->textDim);
    D.drawString("SCAN=S  DEL=cancel", SCREEN_W - 6, 13, 1);

    if (wifiNetCount == 0)
    {
        D.setTextDatum(middle_center);
        D.setTextColor(T->textDim);
        D.drawString("No networks found", SCREEN_W / 2, 68, 1);
        D.drawString("Press S to rescan", SCREEN_W / 2, 82, 1);
    }
    else
    {
        for (int i = 0; i < ROWS_VIS; i++)
        {
            int idx = wifiNetScroll + i;
            if (idx >= wifiNetCount)
                break;
            int y = LIST_START + i * ROW_H;
            bool sel = (idx == wifiNetSel);
            D.fillRect(5, y, SCREEN_W - 10, ROW_H, sel ? T->selRow : T->bg);
            if (sel)
                D.fillRect(5, y, 3, ROW_H, T->accent1);

            String ssid = wifiNets[idx].ssid;
            if ((int)ssid.length() > 24)
                ssid = ssid.substring(0, 23) + ">";
            D.setTextDatum(middle_left);
            D.setTextColor(sel ? T->textBright : T->textMid);
            D.drawString(ssid, 12, y + ROW_H / 2, 1);

            if (wifiNets[idx].encrypted)
            {
                D.setTextColor(sel ? T->accent3 : T->textDim);
                D.drawString("*", SCREEN_W - 14, y + ROW_H / 2, 1);
            }
        }
        if (wifiNetCount > ROWS_VIS)
        {
            int sbH = ROWS_VIS * ROW_H;
            int thH = max(5, sbH * ROWS_VIS / wifiNetCount);
            int thY = LIST_START + (sbH - thH) * wifiNetScroll / max(1, wifiNetCount - ROWS_VIS);
            D.fillRect(SCREEN_W - 8, LIST_START, 3, sbH, T->barBg);
            D.fillRect(SCREEN_W - 8, thY, 3, thH, T->accent2);
        }
    }
    D.setTextDatum(middle_center);
    D.setTextColor(T->textDim);
    D.drawString("ENTER=connect", SCREEN_W / 2, FOOTER_Y, 1);
}

void drawWifiPassOverlay(bool inputOnly)
{
    auto &D = M5Cardputer.Display;
    if (!inputOnly)
    {
        drawOverlayFrame("WIFI PASSWORD");
        D.setTextDatum(middle_left);
        D.setTextColor(T->textMid);
        String ssidLine = "Net: " + inputSaved;
        if ((int)ssidLine.length() > 32)
            ssidLine = ssidLine.substring(0, 31) + ">";
        D.drawString(ssidLine, 8, 34, 1);
        D.drawFastHLine(6, 42, SCREEN_W - 12, T->textDim);
        D.setTextColor(T->textDim);
        D.setTextDatum(middle_center);
        D.drawString("ENTER=connect   DEL=back", SCREEN_W / 2, 100, 1);
        D.fillRect(6, 52, SCREEN_W - 12, 18, T->hdrBg);
        D.drawRect(6, 52, SCREEN_W - 12, 18, T->accent2);
    }
    String disp = String(inputBuf) + "_";
    if ((int)disp.length() > 30)
        disp = disp.substring((int)disp.length() - 30);
    while ((int)disp.length() < 30)
        disp += ' ';
    D.setTextDatum(middle_left);
    D.setTextColor(T->textBright, T->hdrBg);
    D.drawString(disp, 10, 61, 1);
    D.setTextColor(T->textBright);
}

void showWifiOverlay()
{
    M5Cardputer.Display.fillRect(0, 0, SCREEN_W, SCREEN_H, T->hdrBg);
    M5Cardputer.Display.setTextDatum(middle_center);
    M5Cardputer.Display.setTextColor(T->accent1);
    M5Cardputer.Display.drawString("SCANNING WIFI...", SCREEN_W / 2, SCREEN_H / 2, 1);
    scanWifiNetworks();
    wifiNetSel = 0;
    wifiNetScroll = 0;
    wifiOverlayVisible = true;
    drawWifiOverlay();
}

void showWifiPassOverlay(const String &ssid)
{
    inputSaved = ssid;
    inputBuf[0] = '\0';
    inputLen = 0;
    wifiPassOverlayVisible = true;
    drawWifiPassOverlay();
}

bool connectWifi(const String &ssid, const String &pass)
{
    String truncSsid = ssid;
    if ((int)truncSsid.length() > 26)
        truncSsid = truncSsid.substring(0, 25) + ">";

    auto drawConnScreen = [&](uint8_t dotCount)
    {
        auto &D = M5Cardputer.Display;
        if (dotCount == 0)
        {
            D.fillRect(0, 0, SCREEN_W, SCREEN_H, T->hdrBg);
            D.drawRect(4, 4, SCREEN_W - 8, SCREEN_H - 8, T->accent1);
            D.setTextDatum(middle_center);
            D.setTextColor(T->accent2);
            D.drawString("WEB RADIO", SCREEN_W / 2, 18, 2);
            D.setTextColor(T->textMid);
            D.drawString(truncSsid, SCREEN_W / 2, SCREEN_H / 2, 1);
            D.setTextColor(T->textDim);
            D.drawString("DEL to cancel", SCREEN_W / 2, SCREEN_H / 2 + 16, 1);
        }
        D.fillRect(5, SCREEN_H / 2 - 24, SCREEN_W - 10, 16, T->hdrBg);
        char msg[18];
        snprintf(msg, sizeof(msg), "CONNECTING%.*s", dotCount, "...");
        D.setTextDatum(middle_center);
        D.setTextColor(T->accent1);
        D.drawString(msg, SCREEN_W / 2, SCREEN_H / 2 - 16, 1);
    };

    drawConnScreen(0);
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
            drawConnScreen(dots);
            lastAnim = millis();
        }
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
        {
            Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
            if (ks.del)
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
        RDBG("[WIFI] connected '%s' rssi=%d ip=%s free=%u\n",
             ssid.c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str(), ESP.getFreeHeap());
        saveWifiConfig(ssid, pass);
        return true;
    }
    WiFi.disconnect();
    wifiConnected = false;
    auto &D = M5Cardputer.Display;
    D.fillRect(0, 0, SCREEN_W, SCREEN_H, T->hdrBg);
    D.drawRect(4, 4, SCREEN_W - 8, SCREEN_H - 8, T->accent1);
    D.setTextDatum(middle_center);
    D.setTextColor(T->accent1);
    D.drawString("CONNECT FAILED", SCREEN_W / 2, SCREEN_H / 2 - 8, 1);
    D.setTextColor(T->textDim);
    D.drawString("any key to retry", SCREEN_W / 2, SCREEN_H / 2 + 8, 1);
    delay(2000);
    return false;
}