#include "SplashScreen.h"

#include <M5Cardputer.h>

#include "../core/Config.h"
#include "Themes.h"

extern const Theme *T;

void drawSplash(
    const char *statusLine,
    bool useTheme)
{
    const Theme *theme =
        (useTheme && T != nullptr)
            ? T
            : &T_TERM;

    uint16_t bg = theme->bg;
    uint16_t streamHead = theme->accent1;
    uint16_t streamBody = theme->textMid;
    uint16_t streamTail = theme->textDim;
    uint16_t lineStrong = theme->accent1;
    uint16_t lineMid = theme->textMid;
    uint16_t lineDim = theme->textDim;
    uint16_t titleShadow = theme->textDim;
    uint16_t cyan = rgb(0, 245, 255);
    uint16_t magenta = rgb(255, 45, 120);
    uint16_t yellow = rgb(245, 230, 66);
    uint16_t red = rgb(255, 30, 50);
    uint16_t titleMain = theme->textBright;
    uint16_t statusColor = theme->accent1;

    M5Cardputer.Display.fillScreen(bg);

    for (int x = 4; x < SCREEN_W; x += 12)
    {
        int StreamStart = (x * 7) % (SCREEN_H / 2);
        int StreamLen = 30 + ((x * 13) % 60);

        for (int y = StreamStart; y < StreamStart + StreamLen && y < SCREEN_H; y += 6)
        {
            uint16_t dotColor = (y < StreamStart + 12) ? streamHead : ((y < StreamStart + 35) ? streamBody : streamTail);

            M5Cardputer.Display.drawFastVLine(x, y, 3, dotColor);
        }
    }

    M5Cardputer.Display.fillRect(0, 22, 240, 2, lineMid);
    M5Cardputer.Display.fillRect(0, 23, 120, 2, lineDim);
    M5Cardputer.Display.fillRect(18, 44, 80, 1, lineStrong);
    M5Cardputer.Display.fillRect(140, 44, 60, 1, lineMid);
    M5Cardputer.Display.fillRect(0, 88, 240, 2, lineMid);
    M5Cardputer.Display.fillRect(60, 89, 180, 1, lineDim);
    M5Cardputer.Display.fillRect(10, 105, 50, 1, theme->accent2);
    M5Cardputer.Display.fillRect(180, 105, 40, 1, lineMid);

    M5Cardputer.Display.drawRect(0, 0, SCREEN_W, SCREEN_H, lineMid);
    M5Cardputer.Display.drawRect(2, 2, SCREEN_W - 4, SCREEN_H - 4, lineDim);

    M5Cardputer.Display.drawFastHLine(0, 0, 10, lineStrong);
    M5Cardputer.Display.drawFastVLine(0, 0, 10, lineStrong);
    M5Cardputer.Display.drawFastHLine(SCREEN_W - 10, 0, 10, lineStrong);
    M5Cardputer.Display.drawFastVLine(SCREEN_W - 1, 0, 10, lineStrong);
    M5Cardputer.Display.drawFastHLine(0, SCREEN_H - 1, 10, lineStrong);
    M5Cardputer.Display.drawFastVLine(0, SCREEN_H - 10, 10, lineStrong);
    M5Cardputer.Display.drawFastHLine(SCREEN_W - 10, SCREEN_H - 1, 10, lineStrong);
    M5Cardputer.Display.drawFastVLine(SCREEN_W - 1, SCREEN_H - 10, 10, lineStrong);

    M5Cardputer.Display.setTextDatum(middle_left);
    M5Cardputer.Display.setTextColor(lineMid);
    M5Cardputer.Display.drawString("// SYS:BOOT //", 8, 10, &fonts::Font0);

    M5Cardputer.Display.setTextDatum(middle_right);
    M5Cardputer.Display.setTextColor(red);
    M5Cardputer.Display.drawString("ERR_SIG", SCREEN_W - 8, 10, &fonts::Font0);

    int titleY1 = 46;
    int titleY2 = 68;
    int titleY3 = 90;
    M5Cardputer.Display.setTextDatum(middle_center);

    M5Cardputer.Display.setTextColor(titleShadow);
    M5Cardputer.Display.drawString("BROKEN", SCREEN_W / 2 - 2, titleY1, &fonts::Font4);
    M5Cardputer.Display.setTextColor(magenta);
    M5Cardputer.Display.drawString("BROKEN", SCREEN_W / 2 + 1, titleY1, &fonts::Font4);
    M5Cardputer.Display.setTextColor(titleMain);
    M5Cardputer.Display.drawString("BROKEN", SCREEN_W / 2, titleY1, &fonts::Font4);

    M5Cardputer.Display.setTextColor(titleShadow);
    M5Cardputer.Display.drawString("SIGNAL", SCREEN_W / 2 - 1, titleY2, &fonts::Font4);
    M5Cardputer.Display.setTextColor(cyan);
    M5Cardputer.Display.drawString("SIGNAL", SCREEN_W / 2 + 1, titleY2, &fonts::Font4);
    M5Cardputer.Display.setTextColor(titleMain);
    M5Cardputer.Display.drawString("SIGNAL", SCREEN_W / 2, titleY2, &fonts::Font4);

    M5Cardputer.Display.setTextColor(magenta);
    M5Cardputer.Display.drawString("PRO", SCREEN_W / 2 - 1, titleY3, &fonts::Font4);
    M5Cardputer.Display.setTextColor(cyan);
    M5Cardputer.Display.drawString("PRO", SCREEN_W / 2 + 1, titleY3, &fonts::Font4);
    M5Cardputer.Display.setTextColor(yellow);
    M5Cardputer.Display.drawString("PRO", SCREEN_W / 2, titleY3, &fonts::Font4);

    M5Cardputer.Display.setTextColor(statusColor);
    M5Cardputer.Display.drawString(statusLine, SCREEN_W / 2, SCREEN_H - 12, &fonts::Font0);
}
