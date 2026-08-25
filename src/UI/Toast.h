#pragma once

#include <Arduino.h>

struct ToastModel
{
    String text;
    unsigned long durationMs = 750;
};

void drawToast(const ToastModel &model);
void showToast(const String &text, unsigned long durationMs = 750);
void dismissToast();
