#pragma once

#include <M5Cardputer.h>
#include "core/State.h"

unsigned long readM4ADuration(const char *path);
unsigned long readMP3Duration(const char *path, size_t fileSize);
void startTrack(int idx);
void stopAudio();
void pauseAudio();
void resumeAudio();
void pumpAudio();
int pickNextTrack();
unsigned long estimateDuration(int idx);
void saveSettings();
void loadSettings();
void drawPlayerHeader();
void drawPlayerList();
void drawPlayerRow(int idx);
void drawPlayerSelection(int oldSelected);
void drawPlayerStatus();
void updatePlayerStatus();
void cycleRepeat();
void toggleShuffle();
void seekTrack(int delta_ms);
void handlePlayerInput(Keyboard_Class::KeysState &ks);
