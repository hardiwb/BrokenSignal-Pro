#include "Header.h"

#include "Themes.h"
#include "../core/State.h"
#include "../module/service/Clock.h"

extern uint8_t themeIdx;
extern const Theme *T;
extern uint8_t volume;
extern String hdrMsg;
extern unsigned long hdrMsgEnd;

namespace
{
constexpr const char *HEADER_BRAND = "BRKN_SIGNAL //";
constexpr int HEADER_TITLE_X = HEADER_CURSOR_X + HEADER_CURSOR_W + 5;
constexpr int HEADER_TITLE_Y = 15;
constexpr int HEADER_TITLE_H = HEADER_H - HEADER_TITLE_Y - 1;

void ensureTheme()
{
    if (T == nullptr)
    {
        T = THEMES[0];
        themeIdx = 0;
    }
}

void drawBottomSeparator()
{
    for (int x = 0; x < SCREEN_W; x += 5)
    {
        M5Cardputer.Display.drawFastHLine(
            x,
            HEADER_H - 1,
            3,
            T->textDim);
    }
}

String headerClockText()
{
    char clock[6] = "";
    formatClock(clock, sizeof(clock));
    return String(clock);
}

void drawHeaderWifiStatus()
{
    M5Cardputer.Display.fillRect(
        HEADER_WIFI_X - 2,
        1,
        HEADER_CLOCK_CLEAR_X - HEADER_WIFI_X,
        13,
        T->hdrBg);

    if (!wifiConnected)
        return;

    String ssid = wifiSSID;
    if (ssid.length() == 0)
        ssid = "NET";
    if ((int)ssid.length() > 5)
        ssid = ssid.substring(0, 5);

    M5Cardputer.Display.setTextDatum(middle_left);
    M5Cardputer.Display.setTextColor(T->textMid);
    M5Cardputer.Display.drawString(
        ssid,
        HEADER_WIFI_X,
        HEADER_ROW1_Y,
        &fonts::Font0);
}

unsigned long headerTitleStartMs(
    const String &title)
{
    static String lastTitle = "";
    static unsigned long startMs = 0;

    if (lastTitle != title)
    {
        lastTitle = title;
        startMs = millis();
    }

    return startMs;
}

void drawHeaderTitle(
    const String &title)
{
    const int titleW =
        SCREEN_W -
        HEADER_TITLE_X -
        4;
    const int textW =
        M5Cardputer.Display.textWidth(
            title,
            &fonts::Font2);

    M5Cardputer.Display.setTextDatum(middle_left);
    M5Cardputer.Display.setTextColor(T->accent1);

    if (textW <= titleW)
    {
        M5Cardputer.Display.drawString(
            title,
            HEADER_TITLE_X,
            HEADER_ROW2_Y,
            &fonts::Font2);
        return;
    }

    const unsigned long startMs =
        headerTitleStartMs(title);
    const unsigned long pauseMs = 500;
    const unsigned long pxMs = 35;
    const int maxOffset =
        max(0, textW - titleW + 8);
    const unsigned long moveMs =
        (unsigned long)maxOffset * pxMs;
    const unsigned long cycleMs =
        pauseMs + moveMs + pauseMs;
    const unsigned long phase =
        (millis() - startMs) %
        max(1UL, cycleMs);

    int offset = 0;
    if (phase < pauseMs)
    {
        offset = 0;
    }
    else if (phase < pauseMs + moveMs)
    {
        offset = min(
            maxOffset,
            (int)((phase - pauseMs) / pxMs));
    }
    else
    {
        offset = maxOffset;
    }

    M5Cardputer.Display.setClipRect(
        HEADER_TITLE_X,
        HEADER_TITLE_Y,
        titleW,
        HEADER_TITLE_H);

    M5Cardputer.Display.drawString(
        title,
        HEADER_TITLE_X - offset,
        HEADER_ROW2_Y,
        &fonts::Font2);

    M5Cardputer.Display.clearClipRect();
}
} // namespace

void drawHeader(
    const HeaderModel &model)
{
    ensureTheme();

    M5Cardputer.Display.fillRect(
        0,
        0,
        SCREEN_W,
        HEADER_H,
        T->hdrBg);

    M5Cardputer.Display.setTextDatum(middle_left);
    M5Cardputer.Display.setTextColor(T->textMid);
    M5Cardputer.Display.drawString(
        HEADER_BRAND,
        HEADER_BRAND_X,
        HEADER_ROW1_Y,
        &fonts::Font0);

    M5Cardputer.Display.setTextColor(T->accent1);
    M5Cardputer.Display.drawString(
        model.mode,
        HEADER_MODE_X,
        HEADER_ROW1_Y,
        &fonts::Font0);

    drawHeaderWifiStatus();
    drawHeaderClock(headerClockText());

    drawHeaderTitle(model.title);

    drawHeaderCursor(model.cursor);
    drawBottomSeparator();
}

void drawHeaderMode(
    const String &mode)
{
    ensureTheme();

    M5Cardputer.Display.fillRect(
        HEADER_MODE_X - 2,
        1,
        HEADER_WIFI_X - HEADER_MODE_X - 2,
        13,
        T->hdrBg);

    M5Cardputer.Display.setTextDatum(middle_left);
    M5Cardputer.Display.setTextColor(T->accent1);
    M5Cardputer.Display.drawString(
        mode,
        HEADER_MODE_X,
        HEADER_ROW1_Y,
        &fonts::Font0);
}

void drawHeaderClock(
    const String &clock)
{
    ensureTheme();

    M5Cardputer.Display.fillRect(
        HEADER_CLOCK_CLEAR_X,
        1,
        SCREEN_W - HEADER_CLOCK_CLEAR_X,
        13,
        T->hdrBg);

    if (clock.length() == 0)
        return;

    M5Cardputer.Display.setTextDatum(middle_right);
    M5Cardputer.Display.setTextColor(T->textMid);
    M5Cardputer.Display.drawString(
        "[" + clock + "]",
        HEADER_CLOCK_X,
        HEADER_ROW1_Y,
        &fonts::Font0);
}

void drawHeaderCursor(
    bool visible)
{
    ensureTheme();

    M5Cardputer.Display.fillRect(
        HEADER_CURSOR_X,
        HEADER_CURSOR_Y,
        HEADER_CURSOR_W,
        HEADER_CURSOR_H,
        visible ? T->accent1 : T->hdrBg);
}

void drawHeaderMessage(
    const String &message)
{
    ensureTheme();

    const int textW =
        M5Cardputer.Display.textWidth(
            message,
            &fonts::Font0);
    const int boxW =
        max(36, textW + 10);
    const int boxX =
        SCREEN_W - boxW - 2;

    M5Cardputer.Display.fillRect(
        boxX,
        15,
        boxW,
        HEADER_H - 16,
        T->hdrBg);

    if (message.length() == 0)
        return;

    M5Cardputer.Display.setTextDatum(middle_right);
    M5Cardputer.Display.setTextColor(T->accent2);
    M5Cardputer.Display.drawString(
        message,
        SCREEN_W - 4,
        HEADER_MESSAGE_Y,
        &fonts::Font0);
}

void showVolumeMessage()
{
    int volPct = (volume * 100) / 255;
    char buf[16];

    snprintf(
        buf,
        sizeof(buf),
        "VOL %d%%",
        volPct);

    hdrMsg = String(buf);
    hdrMsgEnd = millis() + 1500;

    drawHeaderMessage(hdrMsg);
}
