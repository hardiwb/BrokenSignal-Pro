#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

void loadRadioList();
void saveRadioList();
String generateRadioName(const String &url, int number);
void purgeRadioMemory();
void startRadioStream(int index);
void stopRadioStream();
void pumpRadioAudio();
void radioScrollEnsureVisible();

void drawRadioAll();
void drawRadioHeader();
void drawRadioRow(int index);
void drawRadioList();
void drawRadioStatus();
void drawAddUrlOverlay(bool inputOnly = false);
void drawAddNameOverlay(bool inputOnly = false);
void drawRemoveConfirm();
void showAddUrlOverlay();
void showRemoveConfirm();
bool radioOverlayActive();
void toggleRadioForceAac();

void enterWebRadioMode();
void exitWebRadioMode();
void handleRadioInput(Keyboard_Class::KeysState &keys);
void handleRadioOverlayInput(Keyboard_Class::KeysState &keys);
