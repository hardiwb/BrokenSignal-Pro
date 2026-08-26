#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

// Character-cell primitives owned by the UI.  They are drawn geometrically so
// the convention remains visible even when the selected bitmap font has no
// Unicode block glyphs.
inline void drawLeftHalfBlock(int x, int centerY, int cellW, int cellH, uint16_t color)
{
    M5Cardputer.Display.fillRect(x, centerY - cellH / 2, max(1, cellW / 2), cellH, color);
}

inline void drawRightHalfBlock(int x, int centerY, int cellW, int cellH, uint16_t color)
{
    const int halfW = max(1, cellW / 2);
    M5Cardputer.Display.fillRect(x + cellW - halfW, centerY - cellH / 2, halfW, cellH, color);
}

inline void drawFullBlock(int x, int centerY, int cellW, int cellH, uint16_t color)
{
    M5Cardputer.Display.fillRect(x, centerY - cellH / 2, cellW, cellH, color);
}

inline void drawDashedCellLine(int x, int y, int width, uint16_t color)
{
    String line;
    const int dashW = max(1, M5Cardputer.Display.textWidth("-", &fonts::Font0));
    for (int used = 0; used + dashW <= width; used += dashW)
        line += '-';

    M5Cardputer.Display.setTextDatum(middle_left);
    M5Cardputer.Display.setTextColor(color);
    M5Cardputer.Display.setClipRect(x, y - 4, width, 9);
    M5Cardputer.Display.drawString(line, x, y, &fonts::Font0);
    M5Cardputer.Display.clearClipRect();
}
