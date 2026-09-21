#pragma once

#include <Arduino.h>

bool beginSdTransfer();
void tickSdTransfer();
void stopSdTransfer();

bool sdTransferActive();
bool sdTransferInProgress();
void drawSdTransferScreen();
