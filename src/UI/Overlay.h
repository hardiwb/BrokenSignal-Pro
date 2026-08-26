#pragma once

#include <Arduino.h>
#include <vector>
#include <M5Cardputer.h>

// ============================================================
// OVERLAY TYPES
// ============================================================

enum class OverlayType
{
    None,
    List,
    Message,
    TwoColumnInput,
    PasswordInput,
    TextInput,
    Confirm
};

enum class OverlayFontSize
{
    Small,
    Large
};

// ============================================================
// OVERLAY MODEL
// ============================================================

struct OverlayModel
{
    OverlayType type = OverlayType::None;

    String title;
    String value;
    String leftValue;

    std::vector<String> items;

    int selected = 0;

    String prompt;
    String helperText;
    String confirmText;

    bool passwordMode = false;
    bool tallInput = false;
    OverlayFontSize inputFont = OverlayFontSize::Small;
};

// ============================================================
// OVERLAY
// ============================================================

void drawOverlay(const OverlayModel &model);

void drawOverlayFrame(
    const String &title);

void drawOverlayList(
    const OverlayModel &model);

void drawOverlayMessage(
    const OverlayModel &model);

void drawOverlayMessageLine(
    const OverlayModel &model,
    int index);

void drawOverlayTwoColumnInput(
    const OverlayModel &model);

void drawOverlayTwoColumnInputValue(
    const OverlayModel &model);

void drawOverlayInput(
    const OverlayModel &model);

void drawOverlayInputValue(
    const String &value,
    bool passwordMode = false);

void drawOverlayConfirm(
    const OverlayModel &model);

void drawOverlayCursor(
    bool visible);
