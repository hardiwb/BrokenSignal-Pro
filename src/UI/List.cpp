#include "List.h"
#include "Themes.h"
#include <M5Cardputer.h>
#include "../core/Config.h"

extern uint8_t themeIdx;
extern const Theme *T;
extern bool cursorVisible;

namespace
{
String fitTextToWidth(
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

void drawTextOrMarquee(
    const String &text,
    int x,
    int y,
    int width,
    uint16_t color,
    bool selected,
    unsigned long marqueeStartMs)
{
    if (M5Cardputer.Display.textWidth(text) <= width)
    {
        M5Cardputer.Display.drawString(
            text,
            x,
            y,
            &fonts::Font0);
        return;
    }

    if (selected)
    {
        drawListMarquee(
            text,
            x,
            y,
            width,
            color,
            marqueeStartMs);
        return;
    }

    M5Cardputer.Display.drawString(
        fitTextToWidth(text, width),
        x,
        y,
        &fonts::Font0);
}
} // namespace

// ============================================================
// LIST
// Glitch Terminal layout
//
// Theme controls COLORS ONLY.
// Layout remains identical for all themes.
// ============================================================

void drawList(
    const ListModel &model)
{
    if (T == nullptr)
    {
        T = THEMES[0];
        themeIdx = 0;
    }

    const int listH =
        VISIBLE_TRACKS *
        LIST_ITEM_H;

    // --------------------------------------------------------
    // Clear list area
    // --------------------------------------------------------

    M5Cardputer.Display.fillRect(
        0,
        LIST_Y,
        SCREEN_W,
        listH,
        T->bg);

    // --------------------------------------------------------
    // Draw rows
    // --------------------------------------------------------

    for (int i = 0;
         i < VISIBLE_TRACKS;
         i++)
    {
        int index =
            model.scrollTop + i;

        if (index >=
            (int)model.items.size())
            break;

        drawListRow(
            model,
            index);
    }

    // --------------------------------------------------------
    // Scrollbar
    // --------------------------------------------------------

    drawListScrollbar(
        model);
}


// ============================================================
// SINGLE ROW
// ============================================================

void drawListRow(
    const ListModel &model,
    int index)
{
    if (index < 0 ||
        index >=
            (int)model.items.size())
        return;

    const ListItemModel &item =
        model.items[index];

    const int row =
        index - model.scrollTop;

    if (row < 0 ||
        row >= VISIBLE_TRACKS)
        return;

    const int y =
        LIST_Y +
        row * LIST_ITEM_H;

    const bool selected =
        (index == model.selected) ||
        item.isSelected;

    // --------------------------------------------------------
    // Row background
    // --------------------------------------------------------

    M5Cardputer.Display.fillRect(
        0,
        y,
        SCREEN_W -
            LIST_SCROLLBAR_W,
        LIST_ITEM_H,
        selected
            ? T->selRow
            : T->bg);

    // --------------------------------------------------------
    // Row type
    // --------------------------------------------------------

    switch (item.type)
    {
    case ListItemType::Folder:

        drawFolderItem(
            item,
            y,
            selected,
            model.marqueeStartMs);

        break;

    case ListItemType::Property:

        drawPropertyItem(
            item,
            y,
            selected,
            model.marqueeStartMs);

        break;

    case ListItemType::Normal:

    default:

        drawNormalItem(
            item,
            y,
            selected,
            model.marqueeStartMs);

        break;
    }
}


// ============================================================
// NORMAL ITEM
//
// Example:
//
// > 01 Track Name
//   02 Another Track
//
// ============================================================

void drawNormalItem(
    const ListItemModel &item,
    int y,
    bool selected,
    unsigned long marqueeStartMs)
{
    const int midY =
        y + LIST_ITEM_H / 2;

    // --------------------------------------------------------
    // Playing / selection indicator
    // --------------------------------------------------------

    if (item.isPlaying)
    {
        M5Cardputer.Display.fillRect(
            0,
            y,
            3,
            LIST_ITEM_H,
            T->accent1);
    }
    else if (selected)
    {
        M5Cardputer.Display.fillRect(
            0,
            y,
            3,
            LIST_ITEM_H,
            T->accent2);
    }

    // --------------------------------------------------------
    // Prefix
    // --------------------------------------------------------

    M5Cardputer.Display.setTextDatum(
        middle_left);

    if (item.isPlaying)
    {
        M5Cardputer.Display.setTextColor(
            T->accent1);

        M5Cardputer.Display.drawString(
            ">",
            4,
            midY,
            &fonts::Font0);
    }
    else if (selected)
    {
        M5Cardputer.Display.setTextColor(
            T->accent2);

        M5Cardputer.Display.drawString(
            "|",
            4,
            midY,
            &fonts::Font0);
    }

    // --------------------------------------------------------
    // Label
    // --------------------------------------------------------

    const int NAME_X = 18;
    const int DURATION_W =
        item.durationMs > 0
            ? 42
            : 0;
    const int NAME_W =
        SCREEN_W -
        LIST_SCROLLBAR_W -
        3 -
        NAME_X -
        DURATION_W;

    uint16_t textColor =
        item.isPlaying
            ? T->accent1
            : selected
                ? T->accent2
                : T->textMid;

    M5Cardputer.Display.setTextColor(
        textColor);

    drawTextOrMarquee(
        item.label,
        NAME_X,
        midY,
        NAME_W,
        textColor,
        selected,
        marqueeStartMs);

    // --------------------------------------------------------
    // Duration
    // --------------------------------------------------------

    if (item.durationMs > 0)
    {
        unsigned long seconds =
            item.durationMs / 1000;

        char duration[8];

        snprintf(
            duration,
            sizeof(duration),
            "%02lu:%02lu",
            seconds / 60,
            seconds % 60);

        M5Cardputer.Display.setTextDatum(
            middle_right);

        M5Cardputer.Display.setTextColor(
            item.isPlaying
                ? T->accent1
                : T->textDim);

        M5Cardputer.Display.drawString(
            duration,
            SCREEN_W -
                LIST_SCROLLBAR_W -
                3,
            midY,
            &fonts::Font0);

        M5Cardputer.Display.setTextDatum(
            middle_left);
    }

    // --------------------------------------------------------
    // Glitch cursor
    // --------------------------------------------------------

    if (selected)
    {
        uint16_t cursorColor =
            cursorVisible
                ? T->accent1
                : T->selRow;

        M5Cardputer.Display.fillRect(
            SCREEN_W -
                LIST_SCROLLBAR_W -
                8,
            y + 3,
            5,
            LIST_ITEM_H - 6,
            cursorColor);
    }
}


// ============================================================
// FOLDER ITEM
//
// Example:
//
// # /Album
//
// ============================================================

void drawFolderItem(
    const ListItemModel &item,
    int y,
    bool selected,
    unsigned long marqueeStartMs)
{
    const int midY =
        y + LIST_ITEM_H / 2;

    // --------------------------------------------------------
    // Left indicator
    // --------------------------------------------------------

    if (selected)
    {
        M5Cardputer.Display.fillRect(
            0,
            y,
            3,
            LIST_ITEM_H,
            T->accent1);
    }

    // --------------------------------------------------------
    // Folder symbol
    // --------------------------------------------------------

    uint16_t folderColor =
        selected
            ? T->accent1
            : T->accent2;

    M5Cardputer.Display.setTextDatum(
        middle_left);

    M5Cardputer.Display.setTextColor(
        folderColor);

    M5Cardputer.Display.drawString(
        "#",
        4,
        midY,
        &fonts::Font0);

    M5Cardputer.Display.drawString(
        "/",
        18,
        midY,
        &fonts::Font0);

    // --------------------------------------------------------
    // Folder name
    // --------------------------------------------------------

    drawTextOrMarquee(
        item.label,
        28,
        midY,
        SCREEN_W -
            LIST_SCROLLBAR_W -
            3 -
            28,
        folderColor,
        selected,
        marqueeStartMs);
}


// ============================================================
// PROPERTY ITEM
//
// Example:
//
// > Brightness             <50%>
//   Screen Dim             <14s>
//   Deep Sleep              <1h>
//
// ============================================================

void drawPropertyItem(
    const ListItemModel &item,
    int y,
    bool selected,
    unsigned long marqueeStartMs)
{
    const int midY =
        y + LIST_ITEM_H / 2;

    // --------------------------------------------------------
    // Selection indicator
    // --------------------------------------------------------

    if (selected)
    {
        M5Cardputer.Display.fillRect(
            0,
            y,
            3,
            LIST_ITEM_H,
            T->accent1);

        M5Cardputer.Display.setTextColor(
            T->accent1);

        M5Cardputer.Display.setTextDatum(
            middle_left);

        M5Cardputer.Display.drawString(
            ">",
            4,
            midY,
            &fonts::Font0);
    }

    // --------------------------------------------------------
    // Property label
    // --------------------------------------------------------

    M5Cardputer.Display.setTextDatum(
        middle_left);

    M5Cardputer.Display.setTextColor(
        selected
            ? T->accent1
            : T->textMid);

    const int LABEL_X = 18;
    const int VALUE_W = 76;
    const int LABEL_W =
        SCREEN_W -
        LIST_SCROLLBAR_W -
        3 -
        LABEL_X -
        VALUE_W;

    drawTextOrMarquee(
        item.label,
        LABEL_X,
        midY,
        LABEL_W,
        selected
            ? T->accent1
            : T->textMid,
        selected,
        marqueeStartMs);

    // --------------------------------------------------------
    // Property value
    // --------------------------------------------------------

    if (item.value.length() > 0)
    {
        M5Cardputer.Display.setTextDatum(
            middle_right);

        M5Cardputer.Display.setTextColor(
            selected
                ? T->accent2
                : T->textDim);

        String value =
            "<" +
            item.value +
            ">";

        value =
            fitTextToWidth(
                value,
                VALUE_W - 2);

        M5Cardputer.Display.drawString(
            value,
            SCREEN_W -
                LIST_SCROLLBAR_W -
                3,
            midY,
            &fonts::Font0);

        M5Cardputer.Display.setTextDatum(
            middle_left);
    }
}


// ============================================================
// PARTIAL SELECTION REDRAW
// ============================================================

void drawListSelection(
    const ListModel &model,
    int oldSelected,
    int newSelected)
{
    if (oldSelected ==
        newSelected)
        return;

    drawListRow(
        model,
        oldSelected);

    drawListRow(
        model,
        newSelected);
}


// ============================================================
// SCROLLBAR
// ============================================================

void drawListScrollbar(
    const ListModel &model)
{
    const int sbX =
        SCREEN_W -
        LIST_SCROLLBAR_W;

    const int listH =
        VISIBLE_TRACKS *
        LIST_ITEM_H;

    // --------------------------------------------------------
    // Background
    // --------------------------------------------------------

    M5Cardputer.Display.fillRect(
        sbX,
        LIST_Y,
        LIST_SCROLLBAR_W,
        listH,
        T->barBg);

    // --------------------------------------------------------
    // No scrollbar required
    // --------------------------------------------------------

    if ((int)model.items.size() <=
        VISIBLE_TRACKS)
        return;

    // --------------------------------------------------------
    // Thumb
    // --------------------------------------------------------

    const int total =
        model.items.size();

    const int thumbH =
        max(
            5,
            listH *
                VISIBLE_TRACKS /
                total);

    const int maxScroll =
        max(
            1,
            total -
                VISIBLE_TRACKS);

    const int thumbY =
        LIST_Y +
        (listH - thumbH) *
            model.scrollTop /
            maxScroll;

    M5Cardputer.Display.fillRect(
        sbX,
        thumbY,
        LIST_SCROLLBAR_W,
        thumbH,
        T->textDim);

    M5Cardputer.Display.drawRect(
        sbX,
        thumbY,
        LIST_SCROLLBAR_W,
        thumbH,
        T->textMid);
}


// ============================================================
// MARQUEE
// ============================================================

void drawListMarquee(
    const String &text,
    int x,
    int y,
    int width,
    uint16_t color,
    unsigned long startMs)
{
    static String fallbackText = "";
    static int fallbackX = 0;
    static int fallbackY = 0;
    static int fallbackWidth = 0;
    static unsigned long fallbackStartMs = 0;

    if (startMs == 0)
    {
        if (fallbackText != text ||
            fallbackX != x ||
            fallbackY != y ||
            fallbackWidth != width)
        {
            fallbackText = text;
            fallbackX = x;
            fallbackY = y;
            fallbackWidth = width;
            fallbackStartMs = millis();
        }

        startMs = fallbackStartMs;
    }

    int textW =
        M5Cardputer.Display.textWidth(
            text);

    // --------------------------------------------------------
    // Text fits
    // --------------------------------------------------------

    if (textW <= width)
    {
        M5Cardputer.Display.setTextColor(
            color);

        M5Cardputer.Display.setTextDatum(
            middle_left);

        M5Cardputer.Display.drawString(
            text,
            x,
            y,
            &fonts::Font0);

        return;
    }

    // --------------------------------------------------------
    // Scroll
    // --------------------------------------------------------

    const unsigned long pauseMs = 500;
    const unsigned long pxMs = 35;
    const int maxOffset =
        max(0, textW - width + 8);
    const unsigned long moveMs =
        (unsigned long)maxOffset * pxMs;
    const unsigned long cycleMs =
        pauseMs + moveMs + pauseMs;
    const unsigned long phase =
        (millis() - startMs) %
        max(1UL, cycleMs);

    int offset = 0;
    if (phase < pauseMs)
    {
        offset = 0;
    }
    else if (phase < pauseMs + moveMs)
    {
        offset = min(
            maxOffset,
            (int)((phase - pauseMs) / pxMs));
    }
    else
    {
        offset = maxOffset;
    }

    M5Cardputer.Display.setTextColor(
        color);

    M5Cardputer.Display.setTextDatum(
        middle_left);

    // --------------------------------------------------------
    // Clip region
    // --------------------------------------------------------

    M5Cardputer.Display.setClipRect(
        x,
        y - 7,
        width,
        LIST_ITEM_H);

    M5Cardputer.Display.drawString(
        text,
        x - offset,
        y,
        &fonts::Font0);

    M5Cardputer.Display.clearClipRect();
}
