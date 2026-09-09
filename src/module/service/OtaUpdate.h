#pragma once

#include <Arduino.h>

bool beginOtaUpdate();
void tickOtaUpdate();
void stopOtaUpdate();

bool otaUpdateActive();
bool otaUpdateInProgress();
void drawOtaUpdateScreen();

