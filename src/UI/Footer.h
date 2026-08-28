#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

#include "../core/Config.h"

constexpr int FOOTER_H = 19;
constexpr int FOOTER_Y = SCREEN_H - FOOTER_H;

constexpr int FOOTER_LEFT_X = 0;
constexpr int FOOTER_LEFT_W = 112;

constexpr int FOOTER_CENTER_X = 112;
constexpr int FOOTER_CENTER_W = 78;

constexpr int FOOTER_BATTERY_X = 190;
constexpr int FOOTER_BATTERY_W = 50;

constexpr int FOOTER_BAR_H = 8;
constexpr int FOOTER_BAR_Y = FOOTER_Y + (FOOTER_H - FOOTER_BAR_H) / 2;
constexpr int FOOTER_PROGRESS_X = FOOTER_CENTER_X;
constexpr int FOOTER_PROGRESS_W = FOOTER_CENTER_W;

struct FooterModel
{
    String left;
    // Reserve center for status/progress. Put key hints in left when possible.
    String center;
    String battery;

    bool showProgress = false;
    float progress = 0.0f;
};

void drawFooter(const FooterModel &model);

void drawFooterLeft(const String &text);
void drawFooterCenter(const String &text);
void drawFooterBattery(const String &text);
void drawFooterProgress(float progress);

String footerBatteryText();
