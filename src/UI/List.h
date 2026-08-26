#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>
#include <vector>
#include "Header.h"
#include "Footer.h"


// ============================================================
// LIST ITEM TYPE
// ============================================================

enum class ListItemType
{
    Normal,
    Folder,
    Property
};

// ============================================================
// LIST ITEM
// ============================================================

struct ListItemModel
{
    String label;
    String value;

    ListItemType type = ListItemType::Normal;

    bool isSelected = false;
    bool isActive = false;
    bool isDimmed = false;

    // Optional duration for music/radio items
    unsigned long durationMs = 0;
};

// ============================================================
// LIST MODEL
// ============================================================

struct ListModel
{
    std::vector<ListItemModel> items;

    int selected = 0;
    int scrollTop = 0;

    // Used by marquee
    unsigned long marqueeStartMs = 0;
};

// ============================================================
// GEOMETRY
// ============================================================

constexpr int LIST_ITEM_H = 12;
constexpr int LIST_SCROLLBAR_W = 7;
constexpr int LIST_PREFIX_X = 4;
constexpr int LIST_INDEX_X = 16;
constexpr int LIST_INDEX_W = 16;
constexpr int LIST_CONTENT_X = 34;
constexpr int LIST_FOLDER_SLASH_X = 34;
constexpr int LIST_FOLDER_NAME_X = 46;

constexpr int LIST_Y = HEADER_H;

constexpr int LIST_HEIGHT =
    FOOTER_Y - LIST_Y;

constexpr int LIST_VISIBLE_ITEM =
    LIST_HEIGHT / LIST_ITEM_H;

// ============================================================
// FULL LIST
// ============================================================

void drawList(
    const ListModel &model);

// ============================================================
// SINGLE ROW
// ============================================================

void drawListRow(
    const ListModel &model,
    int index);

// ============================================================
// PARTIAL SELECTION
// ============================================================

void drawListSelection(
    const ListModel &model,
    int oldSelected,
    int newSelected);

// ============================================================
// SCROLLBAR
// ============================================================

void drawListScrollbar(
    const ListModel &model);

// ============================================================
// ROW TYPES
// ============================================================

void drawNormalItem(
    const ListItemModel &item,
    int y,
    int index,
    bool selected,
    unsigned long marqueeStartMs);

void drawFolderItem(
    const ListItemModel &item,
    int y,
    int index,
    bool selected,
    unsigned long marqueeStartMs);

void drawPropertyItem(
    const ListItemModel &item,
    int y,
    int index,
    bool selected,
    unsigned long marqueeStartMs);

// ============================================================
// MARQUEE
// ============================================================

void drawListMarquee(
    const String &text,
    int x,
    int y,
    int width,
    uint16_t color,
    unsigned long startMs);
