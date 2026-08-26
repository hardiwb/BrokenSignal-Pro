#include "Footer.h"

#include "Themes.h"
#include "Cells.h"
#include "Hotkeys.h"

extern uint8_t themeIdx;
extern const Theme *T;
extern int batteryLevel;

namespace
{
FooterModel gFooterModel;

constexpr int FOOTER_TEXT_Y = FOOTER_Y + FOOTER_H / 2;
constexpr int FOOTER_TEXT_PAD_X = 2;
constexpr int FOOTER_TEXT_CHAR_W = 6;

void ensureTheme()
{
    if (T == nullptr)
    {
        T = THEMES[0];
        themeIdx = 0;
    }
}

void drawSeparator()
{
    for (int x = 0; x < SCREEN_W; x += 5)
    {
        M5Cardputer.Display.drawFastHLine(
            x,
            FOOTER_Y,
            3,
            T->textDim);
    }
}

void clearSlot(int x, int w)
{
    M5Cardputer.Display.fillRect(
        x,
        FOOTER_Y + 1,
        w,
        FOOTER_H - 1,
        T->hdrBg);
}

String fitFooterText(
    const String &text,
    int maxWidthPx)
{
    if (text.length() == 0 || maxWidthPx <= 0)
        return "";

    int maxChars = maxWidthPx / FOOTER_TEXT_CHAR_W;
    if (maxChars <= 0)
        return "";

    if ((int)text.length() <= maxChars)
        return text;

    if (maxChars <= 3)
        return text.substring(0, maxChars);

    return text.substring(0, maxChars - 3) + "...";
}

void drawFooterLeftText(const String &text)
{
    const int leftW =
        (!gFooterModel.showProgress &&
         gFooterModel.center.length() == 0)
            ? FOOTER_BATTERY_X
            : FOOTER_LEFT_W;

    clearSlot(FOOTER_LEFT_X, leftW);

    if (text.length() == 0)
        return;

    M5Cardputer.Display.setClipRect(
        FOOTER_LEFT_X,
        FOOTER_Y + 1,
        leftW,
        FOOTER_H - 1);

    if (text.indexOf('[') >= 0)
    {
        drawHotkeyText(
            text,
            FOOTER_LEFT_X + FOOTER_TEXT_PAD_X,
            FOOTER_TEXT_Y,
            middle_left,
            T->textMid);
    }
    else
    {
        M5Cardputer.Display.setTextDatum(middle_left);
        M5Cardputer.Display.setTextColor(T->textMid);
        M5Cardputer.Display.drawString(
            text,
            FOOTER_LEFT_X + FOOTER_TEXT_PAD_X,
            FOOTER_TEXT_Y,
            &fonts::Font0);
    }

    M5Cardputer.Display.clearClipRect();
}

void drawFooterCenterText(const String &text)
{
    clearSlot(FOOTER_CENTER_X, FOOTER_CENTER_W);

    if (text.length() == 0)
        return;

    String fitted = fitFooterText(
        text,
        FOOTER_CENTER_W - (FOOTER_TEXT_PAD_X * 2));

    if (fitted.indexOf('[') >= 0)
    {
        drawHotkeyText(
            fitted,
            FOOTER_CENTER_X + (FOOTER_CENTER_W / 2),
            FOOTER_TEXT_Y,
            middle_center,
            T->textMid);
    }
    else
    {
        M5Cardputer.Display.setTextDatum(middle_center);
        M5Cardputer.Display.setTextColor(T->textMid);
        M5Cardputer.Display.drawString(
            fitted,
            FOOTER_CENTER_X + (FOOTER_CENTER_W / 2),
            FOOTER_TEXT_Y,
            &fonts::Font0);
    }
}

void drawFooterBatteryText(const String &text)
{
    clearSlot(FOOTER_BATTERY_X, FOOTER_BATTERY_W);

    if (text.length() == 0)
        return;

    String fitted = fitFooterText(
        text,
        FOOTER_BATTERY_W - (FOOTER_TEXT_PAD_X * 2));

    M5Cardputer.Display.setTextDatum(middle_right);
    M5Cardputer.Display.setTextColor(T->textMid);
    M5Cardputer.Display.drawString(
        fitted,
        SCREEN_W - FOOTER_TEXT_PAD_X,
        FOOTER_TEXT_Y,
        &fonts::Font0);
}

void drawFooterProgressBar(float progress)
{
    clearSlot(FOOTER_CENTER_X, FOOTER_CENTER_W);

    progress = constrain(
        progress,
        0.0f,
        1.0f);

    constexpr int cellW = 5;
    constexpr int cellGap = 1;
    const int segs = max(1, FOOTER_PROGRESS_W / (cellW + cellGap));
    const int litSegs = (int)(segs * progress + 0.5f);
    const int barW = segs * (cellW + cellGap) - cellGap;
    const int barX = FOOTER_PROGRESS_X + (FOOTER_PROGRESS_W - barW) / 2;

    for (int s = 0; s < segs; s++)
    {
        const int bx = barX + s * (cellW + cellGap);
        drawFullBlock(
            bx,
            FOOTER_BAR_Y + FOOTER_BAR_H / 2,
            cellW,
            FOOTER_BAR_H,
            (s < litSegs) ? T->textMid : T->textDim);
    }
}
} // namespace

void drawFooter(
    const FooterModel &model)
{
    ensureTheme();
    gFooterModel = model;

    M5Cardputer.Display.fillRect(
        0,
        FOOTER_Y,
        SCREEN_W,
        FOOTER_H,
        T->hdrBg);

    drawSeparator();
    drawFooterLeftText(model.left);

    if (model.showProgress)
    {
        drawFooterProgressBar(model.progress);
    }
    else if (model.center.length() > 0)
    {
        drawFooterCenterText(model.center);
    }

    drawFooterBatteryText(model.battery);
}

void drawFooterLeft(
    const String &text)
{
    ensureTheme();
    gFooterModel.left = text;
    drawFooterLeftText(text);
}

void drawFooterCenter(
    const String &text)
{
    ensureTheme();
    gFooterModel.center = text;
    gFooterModel.showProgress = false;
    drawFooterCenterText(text);
}

void drawFooterBattery(
    const String &text)
{
    ensureTheme();
    gFooterModel.battery = text;
    drawFooterBatteryText(text);
}

void drawFooterProgress(
    float progress)
{
    ensureTheme();
    gFooterModel.progress = progress;
    gFooterModel.showProgress = true;
    drawFooterProgressBar(progress);
}

String footerBatteryText()
{
    if (batteryLevel < 0)
        return "";

    return "B:" + String(batteryLevel) + "%";
}
