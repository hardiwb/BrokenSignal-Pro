#pragma once
#include <Arduino.h>
// maxCh <= 0 returns the complete semantic name. UI components own fitting.
String folderName(const String &p, int maxCh = 0);
String shortName(const String &p, int maxCh = 0);
String formatTime(unsigned long ms);
