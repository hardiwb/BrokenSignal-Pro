#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

#include "../core/Config.h"

constexpr int HEADER_H = 32;
constexpr int HEADER_BRAND_X = 4;
constexpr int HEADER_MODE_X = 92;
constexpr int HEADER_WIFI_X = 150;
constexpr int HEADER_CLOCK_X = SCREEN_W - 4;
constexpr int HEADER_RIGHT_X = SCREEN_W - 4;
constexpr int HEADER_ROW1_Y = 7;
constexpr int HEADER_ROW2_Y = 22;
constexpr int HEADER_CURSOR_X = 4;
constexpr int HEADER_CURSOR_Y = 16;
constexpr int HEADER_CURSOR_W = 5;
constexpr int HEADER_CURSOR_H = 10;
constexpr int HEADER_MESSAGE_X = 100;
constexpr int HEADER_MESSAGE_Y = 22;
constexpr int HEADER_CLOCK_CLEAR_X = 198;

struct HeaderModel
{
    String mode;
    String title;

    bool cursor = false;
};

void drawHeader(const HeaderModel &model);

void drawHeaderMode(const String &mode);
void drawHeaderClock(const String &clock);
void drawHeaderCursor(bool visible);
void drawHeaderMessage(const String &message);
void showVolumeMessage();
