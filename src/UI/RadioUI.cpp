#include "RadioUI.h"

#include <M5Cardputer.h>

#include "../core/State.h"
#include "../core/Config.h"
#include "../core/Utils.h"
#include "../module/Clock.h"

#include "UI.h"
#include "Themes.h"

static String getRadioClock()
{
    char buf[6];

    formatClock(buf, sizeof(buf));

    return String(buf);
}

void drawRadioHeader()
{
#define D headerCanvas
    D.fillRect(0, 0, SCREEN_W, HEADER_H, T->hdrBg);

    String wifiStatus = wifiConnected ? wifiSSID : "NOT CONNECTED";

    char clockBuf[6];
    formatClock(clockBuf, sizeof(clockBuf));

    String suffix = " [" + String(clockBuf) + "]";

    int maxWidth = SCREEN_W - 8;

    while (wifiStatus.length() > 0 &&
           D.textWidth(wifiStatus + suffix) > maxWidth)
    {
        wifiStatus.remove(wifiStatus.length() - 1);
    }

    wifiStatus += suffix;

    String stationLine = "-- SELECT STATION --";
    if (radioIsPlaying && radioPlaying >= 0 && radioPlaying < radioCount)
        stationLine = radioList[radioPlaying].name;
    if ((int)stationLine.length() > 24)
        stationLine = stationLine.substring(0, 23) + ">";

    if (themeIdx == 0)
    {
        D.fillRect(0, 0, 3, HEADER_H, T->accent1);
        D.setTextDatum(middle_left);
        D.setTextColor(radioIsPlaying ? T->accent1 : T->textDim);
        D.drawString(radioIsPlaying ? ">> LIVE" : "-  RADIO", 8, 7, 1);
        D.setTextColor(wifiConnected ? T->accent3 : T->accent1);
        D.setTextDatum(middle_right);
        D.drawString(wifiStatus, SCREEN_W - 4, 7, 1);
        D.setTextColor(T->accent2);
        D.setTextDatum(middle_left);
        D.drawString(stationLine, 8, 22, 2);
        D.drawFastHLine(3, HEADER_H - 2, SCREEN_W - 3, T->accent1);
        D.drawFastHLine(3, HEADER_H - 1, SCREEN_W - 3, T->textDim);
        D.drawFastHLine(0, 0, 6, T->accent1);
        D.drawFastVLine(0, 0, 6, T->accent1);
        D.drawFastHLine(SCREEN_W - 6, 0, 6, T->accent1);
        D.drawFastVLine(SCREEN_W - 1, 0, 6, T->accent1);
    }
    else if (themeIdx == 1)
    {
        D.setTextDatum(middle_left);
        D.setTextColor(T->textDim);
        D.drawString("BRKN_SIGNAL // RADIO", 4, 7, 1);
        D.setTextColor(radioIsPlaying ? T->accent1 : T->textDim);
        D.setTextDatum(middle_right);
        D.drawString(wifiConnected ? wifiStatus : "NO WIFI", SCREEN_W - 4, 7, 1);
        const int CUR_X = 4, CUR_W = 5, CUR_GAP = 3;
        D.fillRect(CUR_X, 16, CUR_W, 10, (radioIsPlaying && cursorVisible) ? T->accent1 : T->hdrBg);
        D.setTextColor(radioIsPlaying ? T->accent1 : T->textMid);
        D.setTextDatum(middle_left);
        D.drawString(stationLine, CUR_X + CUR_W + CUR_GAP, 22, 2);
        for (int x = 0; x < SCREEN_W; x += 5)
            D.drawFastHLine(x, HEADER_H - 1, 3, T->textDim);
    }
    else
    {
        D.fillRect(0, 0, 3, HEADER_H, T->accent1);
        D.drawFastVLine(0, 0, HEADER_H / 2, T->accent2);
        D.setTextDatum(middle_left);
        D.setTextColor(radioIsPlaying ? T->accent1 : T->textDim);
        D.drawString(radioIsPlaying ? "* ON AIR" : "* WEB RADIO", 8, 7, 1);
        D.setTextColor(wifiConnected ? T->accent3 : T->textDim);
        D.setTextDatum(middle_right);
        D.drawString(wifiStatus, SCREEN_W - 4, 7, 1);
        D.setTextColor(T->textBright);
        D.setTextDatum(middle_left);
        D.drawString(stationLine, 8, 22, 2);
        D.drawFastHLine(3, HEADER_H - 1, SCREEN_W - 3, T->textDim);
        if (themeIdx == 3)
        {
            D.drawFastHLine(0, HEADER_H - 2, SCREEN_W, T->accent2);
            D.drawFastHLine(0, HEADER_H - 1, SCREEN_W, rgb(0, 80, 70));
        }
        else if (themeIdx == 4)
        {
            D.drawFastHLine(0, HEADER_H - 1, SCREEN_W, T->accent1);
        }
    }

    if (hdrMsgEnd > 0 && millis() < hdrMsgEnd)
    {
        D.setTextDatum(middle_right);
        D.setTextColor(T->accent2);
        D.drawString(hdrMsg, SCREEN_W - 4, 22, 1);
    }

    headerCanvas.pushSprite(&M5Cardputer.Display, 0, 0);
#undef D
}

void drawRadioRow(int idx)
{
    if (idx < radioScrollTop || idx >= radioScrollTop + VISIBLE_TRACKS)
        return;
    if (idx < 0 || idx >= radioCount)
        return;

    auto &D = M5Cardputer.Display;
    int sbW = (themeIdx == 1) ? 7 : 3;
    int i = idx - radioScrollTop;
    int y = LIST_Y + i * LIST_ITEM_H;
    int midY = y + LIST_ITEM_H / 2;
    bool sel = (idx == radioSelected);
    bool play = (idx == radioPlaying && radioIsPlaying);

    int rowW = SCREEN_W - sbW;
    if (themeIdx == 0)
    {
        if (sel)
        {
            D.fillRect(0, y, rowW / 2, LIST_ITEM_H, T->selRow);
            D.fillRect(rowW / 2, y, rowW - rowW / 2, LIST_ITEM_H, T->bg);
        }
        else
        {
            D.fillRect(0, y, rowW, LIST_ITEM_H, T->bg);
        }
        D.drawFastHLine(0, y + LIST_ITEM_H - 1, rowW, rgb(9, 21, 32));
    }
    else if (themeIdx == 1)
    {
        D.fillRect(0, y, rowW, LIST_ITEM_H, sel ? T->selRow : T->bg);
    }
    else
    {
        if (sel)
        {
            D.fillRect(0, y, 80, LIST_ITEM_H, rgb(20, 16, 10));
            D.fillRect(80, y, rowW - 80, LIST_ITEM_H, T->bg);
        }
        else
        {
            D.fillRect(0, y, rowW, LIST_ITEM_H, T->bg);
        }
        D.drawFastHLine(0, y + LIST_ITEM_H - 1, rowW, T->textDim);
    }

    if (themeIdx == 0 || themeIdx == 3)
    {
        if (play)
            D.fillRect(3, y, 3, LIST_ITEM_H, T->accent1);
        else if (sel)
            D.fillRect(3, y, 2, LIST_ITEM_H, T->accent2);
    }
    else if (themeIdx == 2)
    {
        if (play)
            D.fillRect(3, y + 3, 3, LIST_ITEM_H - 6, T->accent1);
        if (sel)
        {
            D.drawFastHLine(SCREEN_W - sbW - 7, y + 3, 5, T->accent2);
            D.drawFastVLine(SCREEN_W - sbW - 3, y + 3, 5, T->accent2);
        }
    }
    else if (themeIdx == 4)
    {
        if (sel)
            D.drawFastVLine(0, y, LIST_ITEM_H, T->accent1);
        if (play)
            D.drawFastVLine(1, y, LIST_ITEM_H, T->accent2);
    }

    char numBuf[4];
    snprintf(numBuf, sizeof(numBuf), "%02d", idx + 1);
    if (themeIdx == 1)
    {
        D.setTextDatum(middle_left);
        if (play)
        {
            D.setTextColor(T->accent1);
            D.drawString(">", 2, midY, 1);
        }
        else if (sel)
        {
            D.setTextColor(T->accent2);
            D.drawString("|", 2, midY, 1);
        }
        D.setTextColor(T->textDim);
        D.drawString(numBuf, 12, midY, 1);
    }
    else
    {
        uint16_t nc = (themeIdx == 4) ? T->textDim
                      : play          ? T->accent1
                                      : (sel ? T->accent2 : T->textDim);
        D.setTextColor(nc);
        D.setTextDatum(middle_left);
        D.drawString(numBuf, 8, midY, 1);
    }

    int nameX = (themeIdx == 1) ? 30 : 26;
    String lbl = radioList[idx].name;
    int maxChars = (themeIdx == 1) ? 31 : 30;
    if ((int)lbl.length() > maxChars)
        lbl = lbl.substring(0, maxChars - 1) + ">";
    uint16_t nc;
    if (themeIdx == 1)
        nc = play ? T->accent1 : (sel ? T->accent2 : T->textMid);
    else
        nc = play ? T->accent1 : (sel ? T->textBright : T->textMid);
    D.setTextColor(nc);
    D.setTextDatum(middle_left);
    D.drawString(lbl, nameX, midY, 1);

    if (themeIdx == 0 && play)
    {
        uint16_t glowCol = rgb(80, 0, 40);
        D.drawFastHLine(0, y, rowW, glowCol);
        D.drawFastHLine(0, y + LIST_ITEM_H - 1, rowW, glowCol);
    }
    if (themeIdx == 1 && sel)
    {
        uint16_t cur = cursorVisible ? T->accent1 : T->bg;
        D.fillRect(SCREEN_W - sbW - 8, y + 3, 5, LIST_ITEM_H - 6, cur);
    }
}

void drawRadioList()
{
    auto &D = M5Cardputer.Display;
    int sbW = (themeIdx == 1) ? 7 : 3;

    D.drawFastHLine(0, HEADER_H, SCREEN_W, T->bg);
    D.drawFastHLine(0, STATUS_Y - 1, SCREEN_W, T->bg);

    if (radioCount == 0)
    {
        D.fillRect(0, LIST_Y, SCREEN_W, VISIBLE_TRACKS * LIST_ITEM_H, T->bg);
        D.setTextDatum(middle_center);
        D.setTextColor(T->textDim);
        D.drawString("No stations saved.", SCREEN_W / 2, LIST_Y + 28, 1);
        D.drawString("Press A to add one.", SCREEN_W / 2, LIST_Y + 42, 1);
        return;
    }

    int visCount = min(VISIBLE_TRACKS, radioCount - radioScrollTop);
    for (int i = 0; i < visCount; i++)
        drawRadioRow(radioScrollTop + i);
    for (int i = visCount; i < VISIBLE_TRACKS; i++)
        D.fillRect(0, LIST_Y + i * LIST_ITEM_H, SCREEN_W, LIST_ITEM_H, T->bg);

    if (radioCount > VISIBLE_TRACKS)
    {
        int listH = VISIBLE_TRACKS * LIST_ITEM_H;
        int thH = max(5, listH * VISIBLE_TRACKS / radioCount);
        int thY = LIST_Y + (listH - thH) * radioScrollTop / max(1, radioCount - VISIBLE_TRACKS);
        int sbX = SCREEN_W - sbW;
        if (themeIdx == 1)
        {
            D.fillRect(sbX, LIST_Y, sbW, listH, T->bg);
            D.drawFastVLine(sbX, LIST_Y, listH, T->textDim);
            D.fillRect(sbX + 1, thY, sbW - 1, thH, T->textDim);
            D.drawRect(sbX + 1, thY, sbW - 1, thH, T->textMid);
        }
        else
        {
            D.fillRect(sbX, LIST_Y, sbW, listH, T->barBg);
            D.fillRect(sbX, thY, sbW, thH, themeIdx == 2 ? T->accent2 : T->accent1);
        }
    }
}

void drawRadioStatus()
{
    int volPct = (volume * 100) / 255;
    statusCanvas.fillSprite(T->hdrBg);

    const char *streamLabel = radioIsPlaying ? "LIVE" : "IDLE";
    uint16_t streamCol = radioIsPlaying ? T->accent1 : T->textDim;

    if (themeIdx == 0)
    {
        statusCanvas.drawFastHLine(0, 0, SCREEN_W, T->textDim);
        statusCanvas.setTextDatum(middle_left);
        statusCanvas.setTextColor(T->accent2);
        statusCanvas.drawString("A", 4, STATUS_H / 2, 1);
        statusCanvas.setTextColor(T->textDim);
        statusCanvas.drawString(":ADD", 10, STATUS_H / 2, 1);
        statusCanvas.setTextColor(T->accent2);
        statusCanvas.drawString("X", 38, STATUS_H / 2, 1);
        statusCanvas.setTextColor(T->textDim);
        statusCanvas.drawString(":RMV", 44, STATUS_H / 2, 1);
        statusCanvas.setTextDatum(middle_center);
        statusCanvas.setTextColor(streamCol);
        statusCanvas.drawString(streamLabel, SCREEN_W / 2, STATUS_H / 2, 1);
        char vbuf[8];
        snprintf(vbuf, sizeof(vbuf), "VOL %d", volPct);
        statusCanvas.setTextColor(T->accent3);
        statusCanvas.setTextDatum(middle_right);
        statusCanvas.drawString(vbuf, SCREEN_W - 2, 5, 1);
        if (batteryLevel >= 0)
        {
            char bbuf[8];
            snprintf(bbuf, sizeof(bbuf), "BAT %d%%", batteryLevel);
            statusCanvas.setTextColor(T->textMid);
            statusCanvas.drawString(bbuf, SCREEN_W - 2, 13, 1);
        }
        statusCanvas.drawFastHLine(0, STATUS_H - 1, 5, T->accent2);
        statusCanvas.drawFastVLine(0, STATUS_H - 5, 5, T->accent2);
    }
    else if (themeIdx == 1)
    {
        for (int x = 0; x < SCREEN_W; x += 5)
            statusCanvas.drawFastHLine(x, 0, 3, T->textDim);
        statusCanvas.setTextDatum(middle_left);
        statusCanvas.setTextColor(T->textDim);
        statusCanvas.drawString("[A]ADD [X]RMV", 2, STATUS_H / 2, 1);
        statusCanvas.setTextColor(streamCol);
        statusCanvas.setTextDatum(middle_center);
        statusCanvas.drawString(streamLabel, SCREEN_W / 2, STATUS_H / 2, 1);
        char vtag[10];
        snprintf(vtag, sizeof(vtag), "VOL:%d", volPct);
        statusCanvas.setTextDatum(middle_right);
        statusCanvas.setTextColor(T->textDim);
        statusCanvas.drawString(vtag, SCREEN_W - 2, 5, 1);
        if (batteryLevel >= 0)
        {
            char bbuf[8];
            snprintf(bbuf, sizeof(bbuf), "B:%d%%", batteryLevel);
            statusCanvas.setTextColor(T->accent2);
            statusCanvas.drawString(bbuf, SCREEN_W - 2, 13, 1);
        }
    }
    else if (themeIdx == 2)
    {
        statusCanvas.drawFastHLine(0, 0, SCREEN_W, T->textDim);
        statusCanvas.setTextDatum(middle_left);
        statusCanvas.setTextColor(T->accent2);
        statusCanvas.drawString("A", 4, STATUS_H / 2, 1);
        statusCanvas.setTextColor(T->textMid);
        statusCanvas.drawString(":ADD", 10, STATUS_H / 2, 1);
        statusCanvas.setTextColor(T->accent2);
        statusCanvas.drawString("X", 40, STATUS_H / 2, 1);
        statusCanvas.setTextColor(T->textMid);
        statusCanvas.drawString(":RMV", 46, STATUS_H / 2, 1);
        statusCanvas.setTextColor(streamCol);
        statusCanvas.setTextDatum(middle_center);
        statusCanvas.drawString(streamLabel, SCREEN_W / 2, STATUS_H / 2, 1);
        char vbuf[6];
        snprintf(vbuf, sizeof(vbuf), "%d", volPct);
        statusCanvas.setTextDatum(middle_right);
        statusCanvas.setTextColor(T->accent2);
        statusCanvas.drawString(vbuf, SCREEN_W - 2, 5, 1);
        statusCanvas.setTextColor(T->textMid);
        statusCanvas.drawString("VOL", SCREEN_W - 2 - (int)strlen(vbuf) * 6 - 4, 5, 1);
        if (batteryLevel >= 0)
        {
            char bbuf[6];
            snprintf(bbuf, sizeof(bbuf), "%d%%", batteryLevel);
            statusCanvas.setTextColor(T->textBright);
            statusCanvas.drawString(bbuf, SCREEN_W - 2, 13, 1);
            statusCanvas.setTextColor(T->textMid);
            statusCanvas.drawString("BAT", SCREEN_W - 2 - (int)strlen(bbuf) * 6 - 4, 13, 1);
        }
    }
    else if (themeIdx == 3)
    {
        statusCanvas.drawFastHLine(0, 0, SCREEN_W, T->accent2);
        statusCanvas.setTextDatum(middle_left);
        statusCanvas.setTextColor(T->accent2);
        statusCanvas.drawString("A", 4, STATUS_H / 2, 1);
        statusCanvas.setTextColor(T->textDim);
        statusCanvas.drawString(":ADD", 10, STATUS_H / 2, 1);
        statusCanvas.setTextColor(T->accent2);
        statusCanvas.drawString("X", 38, STATUS_H / 2, 1);
        statusCanvas.setTextColor(T->textDim);
        statusCanvas.drawString(":RMV", 44, STATUS_H / 2, 1);
        statusCanvas.setTextColor(streamCol);
        statusCanvas.setTextDatum(middle_center);
        statusCanvas.drawString(streamLabel, SCREEN_W / 2, STATUS_H / 2, 1);
        char vbuf[8];
        snprintf(vbuf, sizeof(vbuf), "VOL %d", volPct);
        statusCanvas.setTextColor(T->accent3);
        statusCanvas.setTextDatum(middle_right);
        statusCanvas.drawString(vbuf, SCREEN_W - 2, 5, 1);
        if (batteryLevel >= 0)
        {
            char bbuf[8];
            snprintf(bbuf, sizeof(bbuf), "BAT %d%%", batteryLevel);
            statusCanvas.setTextColor(T->accent2);
            statusCanvas.drawString(bbuf, SCREEN_W - 2, 13, 1);
        }
    }
    else
    {
        statusCanvas.drawFastHLine(0, 0, SCREEN_W, T->textDim);
        statusCanvas.setTextDatum(middle_left);
        statusCanvas.setTextColor(T->textMid);
        statusCanvas.drawString("A:ADD", 4, STATUS_H / 2, 1);
        statusCanvas.drawString("X:RMV", 38, STATUS_H / 2, 1);
        statusCanvas.setTextColor(streamCol);
        statusCanvas.setTextDatum(middle_center);
        statusCanvas.drawString(streamLabel, SCREEN_W / 2, STATUS_H / 2, 1);
        int litBlocks = (int)(volPct * 6 / 100.0f + 0.5f);
        int batW = (batteryLevel >= 0) ? 28 : 0;
        int bx0 = SCREEN_W - 2 - batW - (6 * 6 - 2);
        for (int b = 0; b < 6; b++)
        {
            int bx = bx0 + b * 6;
            statusCanvas.fillRect(bx, (STATUS_H - 5) / 2, 4, 5, b < litBlocks ? T->accent2 : T->textDim);
        }
        if (batteryLevel >= 0)
        {
            char bbuf[6];
            snprintf(bbuf, sizeof(bbuf), "%d%%", batteryLevel);
            statusCanvas.setTextColor(T->textDim);
            statusCanvas.setTextDatum(middle_left);
            int sepX = bx0 + 34 + 4;
            statusCanvas.drawString("|", sepX, STATUS_H / 2, 1);
            statusCanvas.drawString(bbuf, sepX + 8, STATUS_H / 2, 1);
        }
    }

    statusCanvas.pushSprite(&M5Cardputer.Display, 0, STATUS_Y);
}

void drawRadioAll()
{
    drawRadioHeader();
    drawRadioList();
    drawRadioStatus();
}

void drawAddUrlOverlay(bool inputOnly)
{
    auto &D = M5Cardputer.Display;
    if (!inputOnly)
    {
        drawOverlayFrame("ADD RADIO STATION");
        D.setTextDatum(middle_left);
        D.setTextColor(T->textMid);
        D.drawString("Enter stream URL:", 8, 34, 1);
        D.setTextColor(T->textDim);
        D.setTextDatum(middle_center);
        D.drawString("Tip: http:// stream URL", SCREEN_W / 2, 80, 1);
        D.drawString("ENTER=next   DEL=backspace", SCREEN_W / 2, 100, 1);
        D.drawString("DEL on empty=cancel", SCREEN_W / 2, 112, 1);
        D.fillRect(6, 44, SCREEN_W - 12, 18, T->hdrBg);
        D.drawRect(6, 44, SCREEN_W - 12, 18, T->accent2);
    }
    String disp = String(inputBuf) + "_";
    if ((int)disp.length() > 30)
        disp = disp.substring((int)disp.length() - 30);
    while ((int)disp.length() < 30)
        disp += ' ';
    D.setTextDatum(middle_left);
    D.setTextColor(T->textBright, T->hdrBg);
    D.drawString(disp, 10, 53, 1);
    D.setTextColor(T->textBright);
}

void drawAddNameOverlay(bool inputOnly)
{
    auto &D = M5Cardputer.Display;
    if (!inputOnly)
    {
        drawOverlayFrame("STATION NAME");
        D.setTextDatum(middle_left);
        D.setTextColor(T->textMid);
        D.drawString("Name (or ENTER to accept):", 8, 34, 1);
        D.setTextColor(T->textDim);
        D.setTextDatum(middle_center);
        D.drawString("ENTER=add station   DEL=backspace", SCREEN_W / 2, 100, 1);
        D.fillRect(6, 44, SCREEN_W - 12, 18, T->hdrBg);
        D.drawRect(6, 44, SCREEN_W - 12, 18, T->accent2);
    }
    String disp = String(inputBuf) + "_";
    if ((int)disp.length() > 30)
        disp = disp.substring((int)disp.length() - 30);
    while ((int)disp.length() < 30)
        disp += ' ';
    D.setTextDatum(middle_left);
    D.setTextColor(T->textBright, T->hdrBg);
    D.drawString(disp, 10, 53, 1);
    D.setTextColor(T->textBright);
}

void drawRemoveConfirm()
{
    auto &D = M5Cardputer.Display;
    drawOverlayFrame("REMOVE STATION?");

    String name = (radioCount > 0 && radioSelected < radioCount)
                      ? radioList[radioSelected].name
                      : "?";
    if ((int)name.length() > 28)
        name = name.substring(0, 27) + ">";

    D.setTextDatum(middle_center);
    D.setTextColor(T->textBright);
    D.drawString(name, SCREEN_W / 2, 55, 1);
    D.setTextColor(T->textDim);
    D.drawString("This will be deleted.", SCREEN_W / 2, 75, 1);
    D.setTextColor(T->accent1);
    D.drawString("ENTER=remove", SCREEN_W / 2, 100, 1);
    D.setTextColor(T->textMid);
    D.drawString("DEL=cancel", SCREEN_W / 2, 114, 1);
}
