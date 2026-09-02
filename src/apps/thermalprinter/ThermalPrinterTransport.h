#pragma once

#include <Arduino.h>

struct ThermalPrinterStatus
{
    bool busy = false;
    int queued = 0;
    unsigned long completed = 0;
    unsigned long failed = 0;
};

struct ThermalPrintLayout
{
    bool label = false;
    bool center = true;
    bool vertical = true;
    bool bold = false;
    bool doubleSize = false;
    bool includeDate = false;
    uint8_t bottomLines = 2;
    uint8_t labelLines = 8;
    String dateHeader;
};

bool fetchThermalPrinterStatus(
    const String &server,
    ThermalPrinterStatus &status,
    String &message);

bool queueThermalPrinterText(
    const String &server,
    const String &text,
    const ThermalPrintLayout &layout,
    String &message);

bool queueThermalPrinterAction(
    const String &server,
    const String &action,
    uint8_t lines,
    String &message);
