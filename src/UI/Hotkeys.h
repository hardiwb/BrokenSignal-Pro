#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

#include "Themes.h"

extern const Theme *T;

// Draws legacy [KEY] hints without brackets, with the key highlighted.
inline void drawHotkeyText(
    const String &text,
    int x,
    int y,
    textdatum_t datum,
    uint16_t bodyColor,
    const lgfx::IFont *font = &fonts::Font0)
{
    struct Segment
    {
        String text;
        bool key;
    };

    Segment segments[32];
    int count = 0;
    String plain;

    auto pushPlain = [&]()
    {
        if (plain.length() == 0 || count >= 32)
            return;
        segments[count++] = {plain, false};
        plain = "";
    };

    for (int i = 0; i < (int)text.length();)
    {
        if (text[i] != '[')
        {
            plain += text[i++];
            continue;
        }

        int close = text.indexOf(']', i + 1);
        if (close < 0)
        {
            plain += text[i++];
            continue;
        }

        pushPlain();
        if (count < 31)
        {
            segments[count++] = {text.substring(i + 1, close), true};
            i = close + 1;
            if (i < (int)text.length() && text[i] != ' ')
                plain += ' ';
        }
    }
    pushPlain();

    int totalW = 0;
    for (int i = 0; i < count; i++)
        totalW += M5Cardputer.Display.textWidth(segments[i].text, font);

    int cursorX = x;
    const uint8_t horizontalDatum = static_cast<uint8_t>(datum) & 0x03;
    if (horizontalDatum == 1)
        cursorX -= totalW / 2;
    else if (horizontalDatum == 2)
        cursorX -= totalW;

    for (int i = 0; i < count; i++)
    {
        M5Cardputer.Display.setTextDatum(middle_left);
        M5Cardputer.Display.setTextColor(
            segments[i].key ? T->accent2 : bodyColor);
        M5Cardputer.Display.drawString(
            segments[i].text,
            cursorX,
            y,
            font);
        cursorX += M5Cardputer.Display.textWidth(segments[i].text, font);
    }
}
