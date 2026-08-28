#include "core/State.h"

AudioGeneratorMP3 *mp3 = nullptr;
AudioGeneratorAAC *aac = nullptr;
AudioFileSourceSD *fileSrc = nullptr;
AudioFileSourceID3 *id3Src = nullptr;
AudioFileSourceM4A *m4aSrc = nullptr;
AudioOutputM5Speaker *output = nullptr;

std::vector<FolderEntry> allFolders;
std::vector<int> folderStack;
int currentFolderIdx = 0;
std::vector<BrowserItem> items;
String viewFolder = "/Music";
int folderPage = 0;
int scanLRU[SCAN_CACHE_MAX];
int scanLRUCount = 0;
bool isScanning = false;

int currentTrack = -1;
int selectedItem = 0;
bool isPlaying = false;
bool isPaused = false;
uint8_t repeatMode = 2;
bool shuffleOn = false;
uint8_t volume = 128;
unsigned long trackStartMs = 0;
unsigned long pausedElapsedMs = 0;
unsigned long trackDurationMs = 0;

String recentPaths[RECENT_MAX];
int recentCount = 0;
bool isRecentView = false;

bool webRadioMode = false;
bool wifiConnected = false;
String wifiSSID = "";
RadioEntry radioList[RADIO_MAX];
int radioCount = 0;
int radioSelected = 0;
int radioScrollTop = 0;
int radioPlaying = -1;
bool radioIsPlaying = false;
bool radioForceAac = false;
AudioFileSource *httpSrc = nullptr;
AudioFileSourceBuffer *radioBuf = nullptr;
AudioGeneratorMP3 *radioMp3 = nullptr;
WifiNet wifiNets[WIFI_SCAN_MAX];
int wifiNetCount = 0;
int wifiNetSel = 0;
int wifiNetScroll = 0;

bool wifiMenuVisible = false;
bool wifiPassOverlayVisible = false;
bool addUrlOverlayVisible = false;
bool addNameOverlayVisible = false;
bool removeConfirmVisible = false;
char inputBuf[RADIO_INPUT_MAX + 1] = "";
int inputLen = 0;
String inputSaved = "";

bool toastActive = false;
unsigned long toastEnd = 0;
String hdrMsg = "";
unsigned long hdrMsgEnd = 0;
bool helpVisible = false;
bool screenOn = true;
bool cursorVisible = true;
bool settingsDirty = false;
unsigned long settingsDirtyMs = 0;
int batteryLevel = -1;
unsigned long batteryLastMs = 0;

uint8_t themeIdx = 0;
const Theme *T = &T_NEON;

uint8_t seekSeconds = SEEK_SECONDS_DEFAULT;
bool wifiPowerSave = false;
uint8_t screenBrightness = SCREEN_BRIGHTNESS_DEFAULT;
uint16_t autoScreenOffSec = AUTO_SCREEN_OFF_DEFAULT;
uint32_t deepSleepSec = DEEP_SLEEP_DEFAULT;
uint32_t playbackOffSec = PLAYBACK_OFF_DEFAULT;
HostApp lastOpenedApp = HostApp::Notes;
HostApp foregroundApp = HostApp::Notes;
AudioSource audioSource = AudioSource::None;
int8_t calculatorDecimalPlaces = -1;
uint8_t calculatorRoundingMode = 0;
bool calculatorThousandsSeparator = true;
unsigned long lastActivityMs = 0;
bool settingsMenuVisible = false;
int settingsSel = 0;
bool debugOverlayVisible = false;
bool calculatorVisible = false;
bool calculatorOverlayMode = false;

//notes
bool notesMode = false;
int notesSelected = 0;
int notesScrollTop = 0;
uint32_t notesMarqueeStartMs = 0;

void rememberLastOpenedApp(HostApp app)
{
    foregroundApp = app;

    if (lastOpenedApp == app)
        return;

    lastOpenedApp = app;
    settingsDirty = true;
    settingsDirtyMs = millis();
}
