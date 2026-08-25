#pragma once
#include "core/State.h"

// ============================================================
// RADIO DATA / STREAM LOGIC
// ============================================================

void loadRadioList();
void saveRadioList();
String generateRadioName(const String &url, int n);
void purgeRadioMemory();
void startRadioStream(int idx);
void stopRadioStream();
void pumpRadioAudio();
void radioScrollEnsureVisible();

// ============================================================
// RADIO UI
// ============================================================

void drawRadioAll();
void drawRadioHeader();
void drawRadioRow(int idx);
void drawRadioList();
void drawRadioStatus();
void drawAddUrlOverlay(bool inputOnly = false);
void drawAddNameOverlay(bool inputOnly = false);
void drawRemoveConfirm();
bool radioOverlayActive();

// ============================================================
// RADIO MODE / INPUT
// ============================================================

void enterWebRadioMode();
void exitWebRadioMode();

void handleRadioInput(Keyboard_Class::KeysState &ks);
void handleRadioOverlayInput(Keyboard_Class::KeysState &ks);
