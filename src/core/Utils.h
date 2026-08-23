#pragma once
#include <Arduino.h>
String folderName(const String &p, int maxCh);
String shortName(const String &p, int maxCh);
String formatTime(unsigned long ms);