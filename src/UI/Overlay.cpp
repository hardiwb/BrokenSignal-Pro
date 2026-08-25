#include "Overlay.h"
#include <M5Cardputer.h>
#include "Themes.h"
#include "../core/State.h"
#include "../core/Config.h"

static constexpr int OVERLAY_MARGIN = 15;
static constexpr int OVERLAY_X = OVERLAY_MARGIN;
static constexpr int OVERLAY_Y = OVERLAY_MARGIN;
static constexpr int OVERLAY_W = SCREEN_W - (OVERLAY_MARGIN * 2);
static constexpr int OVERLAY_H = SCREEN_H - (OVERLAY_MARGIN * 2);
static constexpr int OVERLAY_PAD = 8;
static constexpr int OVERLAY_TITLE_Y = OVERLAY_Y + 10;
static constexpr int OVERLAY_FOOTER_Y = OVERLAY_Y + OVERLAY_H - 7;
static constexpr int OVERLAY_INPUT_X = OVERLAY_X + OVERLAY_PAD;
static constexpr int OVERLAY_INPUT_Y_WITH_PROMPT = OVERLAY_Y + 40;
static constexpr int OVERLAY_INPUT_Y_COMPACT = OVERLAY_Y + 17;
static constexpr int OVERLAY_INPUT_W = OVERLAY_W - (OVERLAY_PAD * 2);
static constexpr int OVERLAY_INPUT_H = 24;
static constexpr int OVERLAY_INPUT_TALL_H = 36;

static int activeInputY = OVERLAY_INPUT_Y_WITH_PROMPT;
static int activeInputH = OVERLAY_INPUT_H;

static String fitOverlayText(
    const String &text,
    int width)
{
    if (M5Cardputer.Display.textWidth(text) <= width)
        return text;

    String fitted =
        text;

    while (fitted.length() > 0 &&
           M5Cardputer.Display.textWidth(fitted + ">") > width)
    {
        fitted.remove(
            fitted.length() - 1);
    }

    if (fitted.length() > 1)
    {
        fitted.remove(
            fitted.length() - 1);
    }

    return fitted + ">";
}

static String trimTextToWidthFromEnd(
    String text,
    int width,
    const lgfx::IFont *font = nullptr)
{
    while (text.length() > 0 &&
           M5Cardputer.Display.textWidth(text, font) > width)
    {
        text = text.substring(1);
    }

    return text;
}

void drawOverlay(const OverlayModel &model)
{
    if (model.type == OverlayType::None)
        return;

    switch (model.type)
    {
    case OverlayType::WifiList:
        drawOverlayList(model);
        break;

    case OverlayType::WifiPassword:
    case OverlayType::TextInput:
        drawOverlayInput(model);
        break;

    case OverlayType::Confirm:
        drawOverlayConfirm(model);
        break;

    default:
        break;
    }
}

void drawOverlayFrame(
    const String &title)
{
    // Dim background outside overlay
    M5Cardputer.Display.fillRect(
        0,
        0,
        SCREEN_W,
        SCREEN_H,
        T->bg);

    // Main panel
    M5Cardputer.Display.fillRect(
        OVERLAY_X,
        OVERLAY_Y,
        OVERLAY_W,
        OVERLAY_H,
        T->hdrBg);

    // Border
    M5Cardputer.Display.drawRect(
        OVERLAY_X,
        OVERLAY_Y,
        OVERLAY_W,
        OVERLAY_H,
        T->accent1);

    // Glitch corners
    M5Cardputer.Display.drawFastHLine(
        OVERLAY_X,
        OVERLAY_Y,
        8,
        T->accent1);

    M5Cardputer.Display.drawFastHLine(
        OVERLAY_X + OVERLAY_W - 8,
        OVERLAY_Y,
        8,
        T->accent1);

    M5Cardputer.Display.drawFastHLine(
        OVERLAY_X,
        OVERLAY_Y + OVERLAY_H - 1,
        8,
        T->accent1);

    M5Cardputer.Display.drawFastHLine(
        OVERLAY_X + OVERLAY_W - 8,
        OVERLAY_Y + OVERLAY_H - 1,
        8,
        T->accent1);

    // Title
    M5Cardputer.Display.setTextDatum(
        middle_center);

    M5Cardputer.Display.setTextColor(
        T->accent1);

    M5Cardputer.Display.drawString(
        title,
        SCREEN_W / 2,
        OVERLAY_TITLE_Y,
        &fonts::Font0);
}

void drawOverlayList(
    const OverlayModel &model)
{
    drawOverlayFrame(model.title);

    auto &D = M5Cardputer.Display;
    D.setTextDatum(middle_left);

    const int rowX = OVERLAY_X + OVERLAY_PAD;
    const int rowY = OVERLAY_Y + 30;
    const int rowW = OVERLAY_W - (OVERLAY_PAD * 2);
    const int rowH = 14;
    const int maxRows =
        (OVERLAY_FOOTER_Y - rowY - 4) / rowH;

    for (int i = 0; i < (int)model.items.size() && i < maxRows; i++)
    {
        const bool selected = i == model.selected;
        const int y = rowY + i * rowH;

        D.fillRect(rowX, y - 6, rowW, rowH, selected ? T->accent2 : T->hdrBg);
        D.setTextColor(selected ? T->bg : T->textMid);
        D.drawString(model.items[i], rowX + 4, y, &fonts::Font0);
    }
}

void drawOverlayInput(
    const OverlayModel &model)
{
    drawOverlayFrame(model.title);

    auto &D = M5Cardputer.Display;
    D.setTextDatum(middle_left);

    const bool hasPrompt =
        model.prompt.length() > 0;

    if (hasPrompt)
    {
        D.setTextColor(T->textDim);
        D.drawString(
            fitOverlayText(
                model.prompt,
                OVERLAY_W - (OVERLAY_PAD * 2)),
            OVERLAY_X + OVERLAY_PAD,
            OVERLAY_Y + 26,
            &fonts::Font0);
    }

    activeInputY =
        hasPrompt
            ? OVERLAY_INPUT_Y_WITH_PROMPT
            : OVERLAY_INPUT_Y_COMPACT;
    activeInputH =
        model.tallInput
            ? OVERLAY_INPUT_TALL_H
            : OVERLAY_INPUT_H;

    D.fillRect(
        OVERLAY_INPUT_X,
        activeInputY,
        OVERLAY_INPUT_W,
        activeInputH,
        T->bg);

    D.drawRect(
        OVERLAY_INPUT_X,
        activeInputY,
        OVERLAY_INPUT_W,
        activeInputH,
        T->accent2);

    drawOverlayInputValue(
        model.value,
        model.passwordMode);

    if (model.helperText.length() > 0)
    {
        D.setTextColor(T->textMid);
        D.setTextDatum(middle_center);
        D.drawString(
            fitOverlayText(
                model.helperText,
                OVERLAY_W - (OVERLAY_PAD * 2)),
            SCREEN_W / 2,
            activeInputY + activeInputH + 16,
            &fonts::Font0);
    }

    D.setTextColor(T->accent1);
    D.setTextDatum(middle_center);
    D.drawString(
        fitOverlayText(
            model.confirmText,
            OVERLAY_W - (OVERLAY_PAD * 2)),
        SCREEN_W / 2,
        OVERLAY_FOOTER_Y,
        &fonts::Font0);
}

void drawOverlayInputValue(
    const String &rawValue,
    bool passwordMode)
{
    auto &D = M5Cardputer.Display;

    const int textX = OVERLAY_INPUT_X + 4;
    const int textW = OVERLAY_INPUT_W - 8;

    D.fillRect(
        OVERLAY_INPUT_X + 1,
        activeInputY + 1,
        OVERLAY_INPUT_W - 2,
        activeInputH - 2,
        T->bg);

    String value = passwordMode ? String("") : rawValue;
    if (passwordMode)
    {
        for (int i = 0; i < (int)rawValue.length(); i++)
            value += '*';
    }

    D.setTextColor(T->textBright, T->bg);
    D.setTextDatum(middle_left);
    D.setClipRect(
        textX,
        activeInputY + 1,
        textW,
        activeInputH - 2);

    if (activeInputH == OVERLAY_INPUT_TALL_H)
    {
        String fitted =
            trimTextToWidthFromEnd(
                value,
                textW,
                &fonts::Font2);

        D.drawString(
            fitted + "_",
            textX,
            activeInputY + (activeInputH / 2),
            &fonts::Font2);
    }
    else
    {
        while (value.length() > 0 &&
               D.textWidth(value + "_") > textW)
        {
            value = value.substring(1);
        }

        D.drawString(
            value + "_",
            textX,
            activeInputY + (activeInputH / 2),
            &fonts::Font0);
    }

    D.clearClipRect();
}

void drawOverlayConfirm(
    const OverlayModel &model)
{
    drawOverlayFrame(model.title);

    auto &D = M5Cardputer.Display;
    D.setTextDatum(middle_center);

    D.setTextColor(T->textMid);
    D.drawString(
        fitOverlayText(
            model.prompt,
            OVERLAY_W - (OVERLAY_PAD * 2)),
        SCREEN_W / 2,
        OVERLAY_Y + 42,
        &fonts::Font0);

    D.setTextColor(T->accent1);
    D.drawString(
        fitOverlayText(
            model.confirmText,
            OVERLAY_W - (OVERLAY_PAD * 2)),
        SCREEN_W / 2,
        OVERLAY_FOOTER_Y,
        &fonts::Font0);
}

void drawOverlayCursor(
    bool visible)
{
    auto &D = M5Cardputer.Display;
    const int x = OVERLAY_INPUT_X + OVERLAY_INPUT_W - 14;
    const int y =
        activeInputY +
        max(7, activeInputH - 17);

    D.fillRect(x, y, 8, 10, T->bg);
    if (visible)
        D.fillRect(x, y, 6, 8, T->accent1);
}
