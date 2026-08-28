#pragma once

#include <Arduino.h>

namespace RadioInternal
{
extern String radioEditUrl;
extern String radioEditName;
extern int radioEditField;
extern int radioEditUrlCursor;
extern int radioEditNameCursor;

void redrawRadioSelection(int oldSelected, int oldScrollTop);
} // namespace RadioInternal
