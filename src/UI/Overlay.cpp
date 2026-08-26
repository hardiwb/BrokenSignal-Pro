#include "Overlay.h"
#include <M5Cardputer.h>
#include "Themes.h"
#include "Cells.h"
#include "Hotkeys.h"
#include "../core/State.h"
#include "../core/Config.h"

static constexpr int OVERLAY_MARGIN = 15;
static constexpr int OVERLAY_X = OVERLAY_MARGIN;
static constexpr int OVERLAY_Y = OVERLAY_MARGIN;
static constexpr int OVERLAY_W = SCREEN_W - (OVERLAY_MARGIN * 2);
static constexpr int OVERLAY_H = SCREEN_H - (OVERLAY_MARGIN * 2);
static constexpr int OVERLAY_PAD = 8;
static constexpr int OVERLAY_TITLE_Y = OVERLAY_Y + 10;
static constexpr int OVERLAY_FOOTER_Y = OVERLAY_Y + OVERLAY_H - 10;
static constexpr int OVERLAY_LIST_X = OVERLAY_X + OVERLAY_PAD;
static constexpr int OVERLAY_LIST_Y = OVERLAY_Y + 30;
static constexpr int OVERLAY_LIST_W = OVERLAY_W - (OVERLAY_PAD * 2);
static constexpr int OVERLAY_TEXT_ROW_H = 14;
static constexpr int OVERLAY_LIST_ROW_TEXT_X = 4;
static constexpr int OVERLAY_LIST_ROW_TOP_PAD = 6;
static constexpr int OVERLAY_LIST_BOTTOM_GAP = 4;
static constexpr int OVERLAY_HELPER_GAP = 12;
static constexpr int OVERLAY_INPUT_X = OVERLAY_X + OVERLAY_PAD;
static constexpr int OVERLAY_INPUT_Y_WITH_PROMPT = OVERLAY_Y + 40;
static constexpr int OVERLAY_INPUT_Y_COMPACT = OVERLAY_Y + 17;
static constexpr int OVERLAY_INPUT_W = OVERLAY_W - (OVERLAY_PAD * 2);
static constexpr int OVERLAY_INPUT_H = 14;
static constexpr int OVERLAY_INPUT_TALL_H = 22;
static constexpr int OVERLAY_TWO_COL_Y = OVERLAY_Y + 17;
static constexpr int OVERLAY_TWO_COL_H = 38;
static constexpr int OVERLAY_TWO_COL_PAD = 6;
static constexpr int OVERLAY_TWO_COL_GAP = 10;
static constexpr int OVERLAY_TWO_COL_CONTENT_PAD = 3;
static constexpr int OVERLAY_LARGE_FONT_CENTER_OFFSET = 3;

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
    case OverlayType::List:
        drawOverlayList(model);
        break;

    case OverlayType::Message:
        drawOverlayMessage(model);
        break;

    case OverlayType::TwoColumnInput:
        drawOverlayTwoColumnInput(model);
        break;

    case OverlayType::PasswordInput:
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

    const int maxRows =
        (OVERLAY_FOOTER_Y - OVERLAY_LIST_Y - OVERLAY_LIST_BOTTOM_GAP) / OVERLAY_TEXT_ROW_H;

    for (int i = 0; i < (int)model.items.size() && i < maxRows; i++)
    {
        const bool selected = i == model.selected;
        const int y = OVERLAY_LIST_Y + i * OVERLAY_TEXT_ROW_H;

        D.fillRect(
            OVERLAY_LIST_X,
            y - OVERLAY_LIST_ROW_TOP_PAD,
            OVERLAY_LIST_W,
            OVERLAY_TEXT_ROW_H,
            selected ? T->accent2 : T->hdrBg);
        D.setTextColor(selected ? T->bg : T->textMid);
        D.drawString(model.items[i], OVERLAY_LIST_X + OVERLAY_LIST_ROW_TEXT_X, y, &fonts::Font0);
    }
}

void drawOverlayMessage(
    const OverlayModel &model)
{
    drawOverlayFrame(model.title);

    auto &D = M5Cardputer.Display;
    D.setTextDatum(middle_center);

    for (int i = 0; i < (int)model.items.size(); i++)
        drawOverlayMessageLine(model, i);

    if (model.confirmText.length() > 0)
    {
        D.setTextColor(T->textDim);
        D.drawString(
            fitOverlayText(model.confirmText, OVERLAY_LIST_W),
            SCREEN_W / 2,
            OVERLAY_FOOTER_Y,
            &fonts::Font0);
    }
}

void drawOverlayMessageLine(
    const OverlayModel &model,
    int index)
{
    if (index < 0 || index >= (int)model.items.size())
        return;

    constexpr int lineH = OVERLAY_TEXT_ROW_H;
    const int lineCount = (int)model.items.size();
    const int contentTop = OVERLAY_LIST_Y;
    const int contentBottom = OVERLAY_FOOTER_Y - 10;
    const int contentH = max(0, contentBottom - contentTop);
    const int firstY = contentTop +
        max(0, (contentH - max(0, lineCount - 1) * lineH) / 2);
    const int y = firstY + index * lineH;

    auto &D = M5Cardputer.Display;
    D.fillRect(
        OVERLAY_LIST_X,
        y - lineH / 2,
        OVERLAY_LIST_W,
        lineH,
        T->hdrBg);
    D.setTextDatum(middle_center);
    D.setTextColor(index == 0 ? T->accent1 : T->textMid);
    D.drawString(
        fitOverlayText(model.items[index], OVERLAY_LIST_W),
        SCREEN_W / 2,
        y,
        &fonts::Font0);
}

void drawOverlayTwoColumnInputValue(
    const OverlayModel &model)
{
    auto &D = M5Cardputer.Display;
    const lgfx::IFont *font = &fonts::Font0;
    if (model.inputFont == OverlayFontSize::Large)
        font = &fonts::Font4;

    D.fillRect(
        OVERLAY_INPUT_X,
        OVERLAY_TWO_COL_Y + OVERLAY_TWO_COL_CONTENT_PAD,
        OVERLAY_INPUT_W,
        OVERLAY_TWO_COL_H - (OVERLAY_TWO_COL_CONTENT_PAD * 2),
        T->bg);

    drawDashedCellLine(
        OVERLAY_INPUT_X,
        OVERLAY_TWO_COL_Y,
        OVERLAY_INPUT_W,
        T->textDim);
    drawDashedCellLine(
        OVERLAY_INPUT_X,
        OVERLAY_TWO_COL_Y + OVERLAY_TWO_COL_H - 1,
        OVERLAY_INPUT_W,
        T->textDim);

    const int innerW = OVERLAY_INPUT_W - (OVERLAY_TWO_COL_PAD * 2);
    int leftW = model.leftValue.length() > 0
        ? D.textWidth(model.leftValue, font) + OVERLAY_TWO_COL_GAP
        : 0;
    leftW = min(leftW, innerW / 3);
    const int valueW = max(0, innerW - leftW);
    // Font4's digit ink sits above its metric midpoint, so lower it visually.
    const int midY = OVERLAY_TWO_COL_Y + OVERLAY_TWO_COL_H / 2 +
        (model.inputFont == OverlayFontSize::Large
            ? OVERLAY_LARGE_FONT_CENTER_OFFSET
            : 0);

    if (model.leftValue.length() > 0)
    {
        D.setTextDatum(middle_left);
        D.setTextColor(T->accent1, T->bg);
        D.setClipRect(
            OVERLAY_INPUT_X + OVERLAY_TWO_COL_PAD,
            OVERLAY_TWO_COL_Y + OVERLAY_TWO_COL_CONTENT_PAD,
            leftW,
            OVERLAY_TWO_COL_H - (OVERLAY_TWO_COL_CONTENT_PAD * 2));
        D.drawString(
            model.leftValue,
            OVERLAY_INPUT_X + OVERLAY_TWO_COL_PAD,
            midY,
            font);
        D.clearClipRect();
    }

    String fitted = trimTextToWidthFromEnd(model.value, valueW, font);
    D.setTextDatum(middle_right);
    D.setTextColor(T->textBright, T->bg);
    D.setClipRect(
        OVERLAY_INPUT_X + OVERLAY_TWO_COL_PAD + leftW,
        OVERLAY_TWO_COL_Y + OVERLAY_TWO_COL_CONTENT_PAD,
        valueW,
        OVERLAY_TWO_COL_H - (OVERLAY_TWO_COL_CONTENT_PAD * 2));
    D.drawString(
        fitted,
        OVERLAY_INPUT_X + OVERLAY_INPUT_W - OVERLAY_TWO_COL_PAD,
        midY,
        font);
    D.clearClipRect();
}

void drawOverlayTwoColumnInput(
    const OverlayModel &model)
{
    drawOverlayFrame(model.title);
    drawOverlayTwoColumnInputValue(model);

    auto &D = M5Cardputer.Display;
    drawHotkeyText(
        fitOverlayText(model.helperText, OVERLAY_LIST_W),
        SCREEN_W / 2,
        OVERLAY_TWO_COL_Y + OVERLAY_TWO_COL_H + OVERLAY_HELPER_GAP,
        middle_center,
        T->textMid);

    drawHotkeyText(
        fitOverlayText(model.confirmText, OVERLAY_LIST_W),
        SCREEN_W / 2,
        OVERLAY_FOOTER_Y,
        middle_center,
        T->textDim);
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

    // Input fields are character rows: dim '-' cells above and below.
    drawDashedCellLine(OVERLAY_INPUT_X, activeInputY, OVERLAY_INPUT_W, T->textDim);
    drawDashedCellLine(
        OVERLAY_INPUT_X,
        activeInputY + activeInputH - 1,
        OVERLAY_INPUT_W,
        T->textDim);

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
            activeInputY + activeInputH + OVERLAY_HELPER_GAP,
            &fonts::Font0);
    }

    drawHotkeyText(
        fitOverlayText(
            model.confirmText,
            OVERLAY_W - (OVERLAY_PAD * 2)),
        SCREEN_W / 2,
        OVERLAY_FOOTER_Y,
        middle_center,
        T->accent1);
}

void drawOverlayInputValue(
    const String &rawValue,
    bool passwordMode)
{
    auto &D = M5Cardputer.Display;

    const int textX = OVERLAY_INPUT_X + 4;
    const int textW = OVERLAY_INPUT_W - 8;

    D.fillRect(
        OVERLAY_INPUT_X,
        activeInputY + 1,
        OVERLAY_INPUT_W,
        activeInputH - 2,
        T->bg);

    drawDashedCellLine(OVERLAY_INPUT_X, activeInputY, OVERLAY_INPUT_W, T->textDim);
    drawDashedCellLine(
        OVERLAY_INPUT_X,
        activeInputY + activeInputH - 1,
        OVERLAY_INPUT_W,
        T->textDim);

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

    D.setTextColor(T->textMid);
    D.drawString(
        fitOverlayText(
            model.confirmText,
            OVERLAY_W - (OVERLAY_PAD * 2)),
        SCREEN_W / 2,
        OVERLAY_FOOTER_Y - 2,
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
        drawLeftHalfBlock(x, y + 4, 6, 8, T->accent1);
}
