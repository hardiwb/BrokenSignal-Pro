#pragma once

#include <M5Cardputer.h>
#include "core/State.h"

int scanDir(const String &path, const String &label);
void scanFolderNow(int idx);
void loadFolderIdx(int idx);
void loadFolder(const String &path);

void enterItem(int idx);
void goBack();

void loadRecentView();
void addRecent(const String &path);
void saveRecentToSD();
void loadRecentFromSD();

void evictFolderCachesForHeap(uint32_t threshold);
void purgeAudioPlayerMemory();

bool handleBrowserInput(Keyboard_Class::KeysState &ks);