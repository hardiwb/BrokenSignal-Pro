#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <cstring>
#include <esp_sleep.h>
#include "UI/SplashScreen.h"
#include "UI/Header.h"
#include "UI/Footer.h"
#include "UI/Toast.h"
#include "core/State.h"
#include "core/AppRuntime.h"
#include "core/Keyboard.h"
#include "core/SurfaceManager.h"
#include "core/System.h"
#include "apps/music/MusicPlayer.h"
#include "module/service/FileBrowser.h"
#include "apps/music/MusicBrowser.h"
#include "apps/calculator/Calculator.h"
#include "module/shell/Debug.h"
#include "apps/radio/Radio.h"
#include "module/shell/Settings.h"
#include "module/service/Clock.h"
#include "module/service/WiFi.h"
#include "apps/notes/Notes.h"

namespace
{
unsigned long playbackSleepStartMs = 0;

bool loadBootThemeFrom(
    const char *path)
{
  File f = SD.open(path, FILE_READ);
  if (!f)
    return false;

  while (f.available())
  {
    String line = f.readStringUntil('\n');
    line.trim();

    int eq = line.indexOf('=');
    if (eq < 0)
      continue;

    String key = line.substring(0, eq);
    int val = line.substring(eq + 1).toInt();

    if (key == "theme" && val >= 0 && val < 5)
    {
      themeIdx = val;
      T = THEMES[val];
      f.close();
      Serial.print("Boot theme: ");
      Serial.println(T->name);
      return true;
    }
  }

  f.close();
  return false;
}

void loadBootTheme()
{
  if (loadBootThemeFrom("/Music/settings.cfg"))
    return;

  if (loadBootThemeFrom("/settings.cfg"))
    return;

  Serial.println("Boot theme: fallback");
}
}

void setup()
{
  auto cfg = M5.config();

  M5Cardputer.begin(cfg, true);

  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("=== BOOT START ===");

  Serial.println("[1] WiFi OFF");
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);

  Serial.println("[2] M5 config DONE");

  Serial.println("[3] M5 begin DONE");

  Serial.println("[4] Speaker");
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(volume);

  Serial.println("[5] Display");
  M5Cardputer.Display.setRotation(1);

  Serial.println("[6] SPI begin");
  SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  SPI.setFrequency(25000000);

  Serial.println("[7] SD begin");
  if (!SD.begin(SD_CS))
  {
    Serial.println("!! SD FAILED !!");
    drawSplash("> SD CARD ERROR");
    while (true)
      delay(1000);
  }

  loadBootTheme();
  drawSplash("> SCANNING /MUSIC/...", true);

  Serial.println("[8] RTC");
  initRtcClock();

  Serial.println("[9] Audio");
  output = new AudioOutputM5Speaker(&M5Cardputer.Speaker, 0);

  Serial.println("[10] Scan music");
  useMusicFileBrowser();
  resetFileBrowserSession();
  scanFileBrowserRoot();
  Serial.printf("Folders found: %d\n", allFolders.size());

  Serial.println("[11] Load settings");
  loadRecentFromSD();
  loadSettings();
  M5Cardputer.Display.setBrightness(screenBrightness);

  lastActivityMs = millis();

  Serial.println("[13] Load folder");
  if (!allFolders.empty())
    loadFolderIdx(0);

  int totalTracks =
      allFolders.empty() ? 0 :
      allFolders[0].tracks.size() + allFolders[0].subFolderIds.size();

  Serial.printf("Tracks: %d\n", totalTracks);

  Serial.println("[14] Draw UI");
  batteryLevel = (int)min((int32_t)99, M5.Power.getBatteryLevel());
  batteryLastMs = millis();
  const HostApp bootApp =
      esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0
          ? lastOpenedApp
          : HostApp::Notes;
  appRuntimeOpen(bootApp);

  Serial.println("=== SETUP DONE ===");
}

void loop()
{
  M5Cardputer.update();

  if (webRadioMode)
    pumpRadioAudio();
  else
    pumpAudio();

  bool anyPlaying = isPlaying || radioIsPlaying;
  delay(anyPlaying ? 1 : 10);

  keyboardLoop();

  if (!anyPlaying)
    playbackSleepStartMs = 0;
  else if (playbackSleepStartMs == 0)
    playbackSleepStartMs = millis();

  appRuntimeTickForeground();

  // UI Update
  static unsigned long lastDraw = 0;
  if (screenOn && !optionsMenuVisible && !applicationsMenuVisible && !helpVisible &&
      !settingsMenuVisible && !notesInputActive() &&
      !calculatorInputActive() &&
      millis() - lastDraw >= 500)
  {
    lastDraw = millis();
    cursorVisible = !cursorVisible;

    if (debugOverlayVisible)
    {
      drawHeaderCursor(cursorVisible);
    }
    else if (webRadioMode)
    {
      bool overlayOpen = wifiPassOverlayVisible || addUrlOverlayVisible || addNameOverlayVisible ||
                         removeConfirmVisible;
      if (wifiMenuVisible)
      {
        drawHeaderCursor(cursorVisible);
        if (wifiNetCount > 0)
          drawWifiRow(wifiNetSel);
      }
      else if (!overlayOpen)
      {
        drawHeaderCursor(radioIsPlaying && cursorVisible);
        if (radioCount > 0)
          drawRadioRow(radioSelected);
      }
    }
    else
    {
      drawHeaderCursor(cursorVisible);
      if (selectedItem >= 0 && selectedItem < (int)items.size())
        drawPlayerRow(selectedItem);
      if (isPlaying)
        updatePlayerStatus();
    }
  }

  if (screenOn && !optionsMenuVisible && !applicationsMenuVisible && !helpVisible &&
      !settingsMenuVisible && !notesInputActive() &&
      !calculatorInputActive() &&
      !wifiPassOverlayVisible && !addUrlOverlayVisible && !addNameOverlayVisible &&
      !removeConfirmVisible)
  {
    static char lastClockText[6] = "";
    char clockText[6];
    formatClock(clockText, sizeof(clockText));
    if (strcmp(clockText, lastClockText) != 0)
    {
      strncpy(lastClockText, clockText, sizeof(lastClockText) - 1);
      lastClockText[sizeof(lastClockText) - 1] = '\0';
      drawHeaderClock(String(clockText));
    }
  }

  if (toastActive && millis() > toastEnd)
  {
    dismissToast();
    if (screenOn && !optionsMenuVisible && !applicationsMenuVisible && !helpVisible &&
        !settingsMenuVisible && !notesInputActive() &&
        !calculatorInputActive() &&
        !wifiPassOverlayVisible && !addUrlOverlayVisible && !addNameOverlayVisible &&
        !removeConfirmVisible)
      drawAll();
  }

  if (hdrMsgEnd > 0 && millis() >= hdrMsgEnd)
  {
    hdrMsgEnd = 0;
    if (screenOn && !surfaceBlocksHostInput(resolveActiveSurface()))
      drawCurrentScreen();
  }

  if (settingsDirty && !isPlaying && !isPaused &&
      millis() - settingsDirtyMs >= 2000)
  {
    saveSettings();
    settingsDirty = false;
  }

  if (batteryLevel < 0 || millis() - batteryLastMs >= BATTERY_INTERVAL)
  {
    batteryLevel = (int)min((int32_t)99, M5.Power.getBatteryLevel());
    batteryLastMs = millis();
    if (screenOn && !optionsMenuVisible && !applicationsMenuVisible && !helpVisible &&
        !settingsMenuVisible && !notesInputActive() &&
        !calculatorInputActive() &&
        !wifiPassOverlayVisible && !addUrlOverlayVisible && !addNameOverlayVisible &&
        !removeConfirmVisible)
    {
      drawFooterBattery(footerBatteryText());
    }
  }

  // Battery-safe auto screen off: only based on idle time.
  if (screenOn && autoScreenOffSec > 0 &&
      millis() - lastActivityMs >= (unsigned long)autoScreenOffSec * 1000UL)
  {
    screenOn = false;
    M5Cardputer.Display.setBrightness(0);
  }

  if (deepSleepSec > 0 && !isPlaying && !radioIsPlaying &&
      millis() - lastActivityMs >= (unsigned long)deepSleepSec * 1000UL)
  {
    saveSettings();
    settingsDirty = false;
    enterDeepSleep();
  }

  if (playbackOffSec > 0 && anyPlaying && playbackSleepStartMs > 0 &&
      millis() - playbackSleepStartMs >= (unsigned long)playbackOffSec * 1000UL)
  {
    stopAudio();
    stopRadioStream();
    saveSettings();
    settingsDirty = false;
    enterDeepSleep();
  }
}
