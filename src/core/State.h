#pragma once
#include <Arduino.h>
#include <SD.h>
#include <vector>
#include <WiFi.h>
#include <M5Cardputer.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorAAC.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceID3.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceBuffer.h>
#include "Config.h"
#include "Types.h"
#include "UI/Themes.h"
#include "AudioCodecs.h"

extern AudioGeneratorMP3 *mp3;
extern AudioGeneratorAAC *aac;
extern AudioFileSourceSD *fileSrc;
extern AudioFileSourceID3 *id3Src;
extern AudioFileSourceM4A *m4aSrc;
extern AudioOutputM5Speaker *output;

extern std::vector<FolderEntry> allFolders;
extern std::vector<int> folderStack;
extern int currentFolderIdx;
extern std::vector<BrowserItem> items;
extern String viewFolder;
extern int folderPage;
extern int scanLRU[SCAN_CACHE_MAX];
extern int scanLRUCount;
extern bool isScanning;

extern int currentTrack;
extern int selectedItem;
extern bool isPlaying;
extern bool isPaused;
extern uint8_t repeatMode;
extern bool shuffleOn;
extern uint8_t volume;
extern unsigned long trackStartMs;
extern unsigned long pausedElapsedMs;
extern unsigned long trackDurationMs;

extern String recentPaths[RECENT_MAX];
extern int recentCount;
extern bool isRecentView;

extern bool webRadioMode;
extern bool wifiConnected;
extern String wifiSSID;
extern RadioEntry radioList[RADIO_MAX];
extern int radioCount;
extern int radioSelected;
extern int radioScrollTop;
extern int radioPlaying;
extern bool radioIsPlaying;
extern AudioFileSource *httpSrc;
extern AudioFileSourceBuffer *radioBuf;
extern AudioGeneratorMP3 *radioMp3;
extern WifiNet wifiNets[WIFI_SCAN_MAX];
extern int wifiNetCount;
extern int wifiNetSel;
extern int wifiNetScroll;

extern bool wifiMenuVisible;
extern bool wifiPassOverlayVisible;
extern bool addUrlOverlayVisible;
extern bool addNameOverlayVisible;
extern bool removeConfirmVisible;
extern char inputBuf[RADIO_INPUT_MAX + 1];
extern int inputLen;
extern String inputSaved;

extern bool toastActive;
extern unsigned long toastEnd;
extern String hdrMsg;
extern unsigned long hdrMsgEnd;
extern bool helpVisible;
extern bool screenOn;
extern bool cursorVisible;
extern bool settingsDirty;
extern unsigned long settingsDirtyMs;
extern int batteryLevel;
extern unsigned long batteryLastMs;

extern uint8_t themeIdx;
extern const Theme *T;

// User settings (persisted)
extern uint8_t seekSeconds;       // seek step in seconds (music mode)
extern bool wifiPowerSave;        // WiFi modem sleep for battery
extern uint8_t screenBrightness;  // display brightness 0..255
extern uint16_t autoScreenOffSec; // 0 = off, else idle seconds before screen off
extern uint32_t deepSleepSec;     // 0 = off, else idle seconds before deep sleep
extern uint32_t playbackOffSec;   // 0 = off, else playback seconds before deep sleep
extern unsigned long lastActivityMs;
extern bool settingsMenuVisible;
extern int settingsSel;
extern bool debugOverlayVisible;
extern bool calculatorVisible;

// Notes app
extern bool notesMode;
extern int notesSelected;
extern int notesScrollTop;
extern uint32_t notesMarqueeStartMs;
