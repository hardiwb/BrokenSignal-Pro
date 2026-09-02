#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

namespace BluetoothKeyboardInternal
{
extern int selected;
extern int scrollTop;

void initialize();
void refreshBonds();
int bondCount();
bool selectedIsBond();
String bondAddressText(int index);
bool renameModalActive();

void drawListScreen();
void drawSessionOverlay();
void drawRenameOverlay();
void moveSelection(int direction);
void openSelected();
void beginRenameSelected();
void handleRenameInput(Keyboard_Class::KeysState &keys);
void disconnectSession();
void forgetSelectedBond();
void forgetAllBonds();
void tick();
void consumeModalMatrixInput(Keyboard_Class::KeysState &keys);
} // namespace BluetoothKeyboardInternal
