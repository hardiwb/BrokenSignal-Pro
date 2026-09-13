#pragma once

#include <M5Cardputer.h>

void infraredOpen();
void drawInfrared();
void handleInfraredInput(Keyboard_Class::KeysState &keys);
void tickInfrared();
void infraredReload();
bool infraredHasSelectedFile();
void infraredRequestNewFolder();
void infraredRequestNewFile();
void infraredRequestRename();
void infraredRequestCapture();
bool infraredCanCapture();
void infraredRequestDelete();
bool infraredModalActive();
void infraredCancelModal();
void infraredAdjustTxSource(int direction);
void infraredAdjustTxPin(int direction);
void infraredAdjustRxEnabled(int direction);
void infraredAdjustRxPin(int direction);
void infraredAdjustRawFrequency(int direction);
void infraredAdjustRawDuty(int direction);
String infraredTxSettingLabel();
String infraredTxPinLabel();
String infraredRxSettingLabel();
String infraredRxPinLabel();
String infraredRawFrequencyLabel();
String infraredRawDutyLabel();
bool infraredExternalTxSelected();
bool infraredRxEnabled();
