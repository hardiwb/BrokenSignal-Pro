#pragma once

#include <M5Cardputer.h>
#include "core/State.h"

struct FileBrowserConfig
{
    const char *rootPath;
    const char *rootLabel;
    const char *recentPath;
    const char *recentLabel;
    bool showRecent;
    bool (*acceptFile)(const String &name, const String &path);
    String (*displayLabel)(const String &name, const String &path);
    unsigned long (*readDuration)(const String &path, size_t fileSize);
    void (*onOpenFile)(int itemIndex, const BrowserItem &item);
    void (*onStopActive)();
    void (*onSeek)(int deltaMs);
    void (*onSelectionChanged)(int oldSelected);
    void (*onRedraw)();
    void (*onClearActiveItem)();
    bool (*isPlaybackActive)();
    bool (*isItemActive)(int itemIndex, const BrowserItem &item);
};

void configureFileBrowser(const FileBrowserConfig &config);
void resetFileBrowserSession();
int scanFileBrowserRoot();

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
