#include "Toast.h"

#include <M5Cardputer.h>

#include "Themes.h"
#include "../core/Config.h"
#include "../core/State.h"

extern uint8_t themeIdx;
extern const Theme *T;

namespace
{
constexpr int TOAST_W = SCREEN_W - 40;
constexpr int TOAST_H = 18;
constexpr int TOAST_X = 20;
constexpr int TOAST_Y = (SCREEN_H - TOAST_H) / 2;
constexpr int TOAST_RADIUS = 3;

void ensureTheme()
{
    if (T == nullptr)
    {
        T = THEMES[0];
        themeIdx = 0;
    }
}
} // namespace

void drawToast(const ToastModel &model)
{
    ensureTheme();

    auto &D = M5Cardputer.Display;

    D.fillRoundRect(
        TOAST_X,
        TOAST_Y,
        TOAST_W,
        TOAST_H,
        TOAST_RADIUS,
        T->accent1);

    D.drawRoundRect(
        TOAST_X,
        TOAST_Y,
        TOAST_W,
        TOAST_H,
        TOAST_RADIUS,
        T->accent2);

    D.setTextColor(T->bg);
    D.setTextDatum(middle_center);
    D.drawString(
        model.text,
        SCREEN_W / 2,
        TOAST_Y + TOAST_H / 2,
        &fonts::Font0);
}

void showToast(
    const String &text,
    unsigned long durationMs)
{
    ToastModel model;
    model.text = text;
    model.durationMs = durationMs;

    drawToast(model);

    toastActive = true;
    toastEnd = millis() + durationMs;
}

void dismissToast()
{
    toastActive = false;
    toastEnd = 0;
}
