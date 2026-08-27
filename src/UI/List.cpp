#include "List.h"
#include "Themes.h"
#include "Cells.h"
#include "Hotkeys.h"
#include <M5Cardputer.h>
#include "../core/Config.h"

extern uint8_t themeIdx;
extern const Theme *T;
extern bool cursorVisible;

namespace
{
String listItemNumber(
    int index)
{
    int displayIndex = index + 1;
    if (displayIndex > 99)
        displayIndex = 99;

    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", displayIndex);
    return String(buf);
}

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
        LIST_VISIBLE_ITEM *
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
         i < LIST_VISIBLE_ITEM;
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
        row >= LIST_VISIBLE_ITEM)
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
            index,
            selected,
            model.marqueeStartMs);

        break;

    case ListItemType::Property:

        drawPropertyItem(
            item,
            y,
            index,
            selected,
            model.marqueeStartMs);

        break;

    case ListItemType::Normal:

    default:

        drawNormalItem(
            item,
            y,
            index,
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
    int index,
    bool selected,
    unsigned long marqueeStartMs)
{
    const int midY =
        y + LIST_ITEM_H / 2;

    // --------------------------------------------------------
    // Playing / selection indicator
    // --------------------------------------------------------

    if (item.isActive)
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

    M5Cardputer.Display.setTextDatum(
        middle_left);
    M5Cardputer.Display.setTextColor(T->textDim);
    M5Cardputer.Display.drawString(
        listItemNumber(index),
        LIST_INDEX_X,
        midY,
        &fonts::Font0);

    if (item.isActive)
    {
        M5Cardputer.Display.setTextColor(
            T->accent1);

        M5Cardputer.Display.drawString(
            ">",
            LIST_PREFIX_X,
            midY,
            &fonts::Font0);
    }
    else if (selected)
    {
        M5Cardputer.Display.setTextColor(
            T->accent2);

        M5Cardputer.Display.drawString(
            "|",
            LIST_PREFIX_X,
            midY,
            &fonts::Font0);
    }

    // --------------------------------------------------------
    // Label
    // --------------------------------------------------------

    const int NAME_X = LIST_CONTENT_X;
    const int DURATION_W =
        item.durationMs > 0
            ? 42
            : 0;
    const int NAME_W =
        LIST_RIGHT_CONTENT_X -
        NAME_X -
        DURATION_W;

    uint16_t textColor =
        item.isDimmed
            ? T->textDim
            : selected
                    ? T->accent2
                : item.isActive
                    ? T->accent1
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
            item.isDimmed
                ? T->textDim
                : selected
                    ? T->accent2
                    : item.isActive
                        ? T->accent1
                        : T->textDim);

        M5Cardputer.Display.drawString(
            duration,
            LIST_RIGHT_CONTENT_X,
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

        drawRightHalfBlock(
            LIST_BLINKER_X,
            midY,
            LIST_BLINKER_W,
            LIST_ITEM_H - 4,
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
    int index,
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

    M5Cardputer.Display.setTextDatum(
        middle_left);
    M5Cardputer.Display.setTextColor(T->textDim);
    M5Cardputer.Display.drawString(
        listItemNumber(index),
        LIST_INDEX_X,
        midY,
        &fonts::Font0);

    uint16_t folderColor =
        item.isDimmed
            ? T->textDim
            : selected
                ? T->accent2
                : T->accent3;

    M5Cardputer.Display.setTextDatum(
        middle_left);

    M5Cardputer.Display.setTextColor(
        folderColor);

    M5Cardputer.Display.drawString(
        "/",
        LIST_FOLDER_SLASH_X,
        midY,
        &fonts::Font0);

    // --------------------------------------------------------
    // Folder name
    // --------------------------------------------------------

    drawTextOrMarquee(
        item.label,
        LIST_FOLDER_NAME_X,
        midY,
        LIST_RIGHT_CONTENT_X - LIST_FOLDER_NAME_X,
        folderColor,
        selected,
        marqueeStartMs);

    if (selected)
        drawRightHalfBlock(
            LIST_BLINKER_X,
            midY,
            LIST_BLINKER_W,
            LIST_ITEM_H - 4,
            cursorVisible ? T->accent1 : T->selRow);
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
    int index,
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
            LIST_PREFIX_X,
            midY,
            &fonts::Font0);
    }

    M5Cardputer.Display.setTextDatum(
        middle_left);
    M5Cardputer.Display.setTextColor(T->textDim);
    M5Cardputer.Display.drawString(
        listItemNumber(index),
        LIST_INDEX_X,
        midY,
        &fonts::Font0);

    if (item.propertyFirst)
    {
        constexpr int PROPERTY_W = 42;
        String property = "<" + item.value + ">";
        property = fitTextToWidth(property, PROPERTY_W);

        M5Cardputer.Display.setTextDatum(middle_left);
        M5Cardputer.Display.setTextColor(
            item.isDimmed
                ? T->textDim
                : selected ? T->accent2 : T->textDim);
        M5Cardputer.Display.drawString(
            property,
            LIST_CONTENT_X,
            midY,
            &fonts::Font0);

        const int itemX = LIST_CONTENT_X + PROPERTY_W;
        const int itemW = LIST_RIGHT_CONTENT_X - itemX;
        String fittedLabel = fitTextToWidth(item.label, itemW);
        M5Cardputer.Display.setTextDatum(middle_right);
        M5Cardputer.Display.setTextColor(
            item.isDimmed
                ? T->textDim
                : selected ? T->accent2 : T->textMid);
        M5Cardputer.Display.drawString(
            fittedLabel,
            LIST_RIGHT_CONTENT_X,
            midY,
            &fonts::Font0);

        if (selected)
            drawRightHalfBlock(
                LIST_BLINKER_X,
                midY,
                LIST_BLINKER_W,
                LIST_ITEM_H - 4,
                cursorVisible ? T->accent1 : T->selRow);
        return;
    }

    // --------------------------------------------------------
    // Property label
    // --------------------------------------------------------

    M5Cardputer.Display.setTextColor(
        item.isDimmed
            ? T->textDim
            : selected
                ? T->accent2
                : T->textMid);

    const int LABEL_X = LIST_CONTENT_X;
    const int VALUE_W = 76;
    const int LABEL_W =
        LIST_RIGHT_CONTENT_X -
        LABEL_X -
        VALUE_W;

    drawTextOrMarquee(
        item.label,
        LABEL_X,
        midY,
        LABEL_W,
        selected
            ? T->accent2
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
            item.isDimmed
                ? T->textDim
                : selected
                    ? T->accent2
                    : T->textDim);

        if (item.value.indexOf('[') >= 0)
        {
            drawHotkeyText(
                item.value,
                LIST_RIGHT_CONTENT_X,
                midY,
                middle_right,
                selected ? T->accent2 : T->textDim);
        }
        else
        {
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
                LIST_RIGHT_CONTENT_X,
                midY,
                &fonts::Font0);
        }

        M5Cardputer.Display.setTextDatum(
            middle_left);
    }

    if (selected)
        drawRightHalfBlock(
            LIST_BLINKER_X,
            midY,
            LIST_BLINKER_W,
            LIST_ITEM_H - 4,
            cursorVisible ? T->accent1 : T->selRow);
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
        LIST_VISIBLE_ITEM *
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
        LIST_VISIBLE_ITEM)
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
                LIST_VISIBLE_ITEM /
                total);

    const int maxScroll =
        max(
            1,
            total -
                LIST_VISIBLE_ITEM);

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
