#pragma once
#include <Arduino.h>
#include <vector>

struct TrackEntry
{
    String path;
    String label;
    size_t fileSize;
    unsigned long durationMs;
    bool isM4A;
};

struct NameEntry
{
    String label;
    String fullPath;
    size_t fileSize;
    bool isM4A;
};

struct FolderEntry
{
    String path;
    String label;
    std::vector<TrackEntry> tracks;
    std::vector<NameEntry> nameCache;
    std::vector<int> subFolderIds;
    bool scanned = false;
    bool nameCacheReady = false;
    int totalItems = 0;
    int trackWindowPage = -1;
    int nameCacheStart = 0;
};

struct BrowserItem
{
    bool isFolder;
    String path;
    String label;
    size_t fileSize;
    unsigned long durationMs;
};

struct RadioEntry
{
    String name;
    String url;
};

struct WifiNet
{
    String ssid;
    int32_t rssi;
    bool encrypted;
};