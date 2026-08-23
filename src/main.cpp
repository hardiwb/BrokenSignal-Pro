#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <cstring>
#include "UI/UI.h"
#include "UI/RadioUI.h"
#include "core/State.h"
#include "core/Keyboard.h"
#include "core/System.h"
#include "module/Player.h"
#include "module/Browser.h"
#include "module/Radio.h"
#include "module/Clock.h"
#include "module/Notes.h"

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
  drawSplash("> SCANNING /MUSIC/...");

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

  Serial.println("[8] RTC");
  initRtcClock();

  Serial.println("[9] Audio");
  output = new AudioOutputM5Speaker(&M5Cardputer.Speaker, 0);

  Serial.println("[10] Canvas");
  statusCanvas.createSprite(SCREEN_W, STATUS_H);
  headerCanvas.createSprite(SCREEN_W, HEADER_H);

  Serial.println("[11] Scan music");
  allFolders.clear();
  scanDir("/Music", "Music");
  Serial.printf("Folders found: %d\n", allFolders.size());

  Serial.println("[12] Load settings");
  loadRecentFromSD();
  loadSettings();

  M5Cardputer.Display.setBrightness(screenBrightness);
  lastActivityMs = millis();

  if (allFolders.empty())
  {
    Serial.println("!! /Music NOT FOUND !!");
    while (true)
      delay(1000);
  }

  Serial.println("[13] Load folder");
  loadFolderIdx(0);

  int totalTracks =
      allFolders[0].tracks.size() +
      allFolders[0].subFolderIds.size();

  Serial.printf("Tracks: %d\n", totalTracks);

  if (totalTracks == 0)
  {
    Serial.println("!! NO TRACKS !!");
    while (true)
      delay(1000);
  }

  Serial.println("[14] Draw UI");
  drawAll();

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

  if (notesMode)
  {
    notesLoop();
  }

  // UI Update
  static unsigned long lastDraw = 0;
  if (screenOn && !helpVisible && !settingsMenuVisible && millis() - lastDraw >= 500)
  {
    lastDraw = millis();
    cursorVisible = !cursorVisible;

    if (debugOverlayVisible)
    {
      drawDebug();
    }
    else if (webRadioMode)
    {
      bool overlayOpen = wifiOverlayVisible || wifiPassOverlayVisible ||
                         addUrlOverlayVisible || addNameOverlayVisible ||
                         removeConfirmVisible;
      if (!overlayOpen)
      {
        if (radioIsPlaying)
        {
          drawRadioHeader();
        }
        if (themeIdx == 1 && radioCount > 0)
        {
          if (!radioIsPlaying)
            drawRadioHeader();
          int visIdx = radioSelected - radioScrollTop;
          if (visIdx >= 0 && visIdx < VISIBLE_TRACKS)
          {
            int y = LIST_Y + visIdx * LIST_ITEM_H;
            uint16_t cur = cursorVisible ? T->accent1 : T->bg;
            M5Cardputer.Display.fillRect(SCREEN_W - 7 - 8, y + 3, 5, LIST_ITEM_H - 6, cur);
          }
        }
      }
    }
    else
    {
      bool terminalNeedsBlink = (themeIdx == 1) &&
                                (isPlaying || isPaused ||
                                 (selectedItem >= 0 && selectedItem < (int)items.size() &&
                                  !items[selectedItem].isFolder));
      bool headerNeedsBlink = terminalNeedsBlink || isPlaying;
      if (headerNeedsBlink)
        drawHeader();
      if (isPlaying)
        drawStatus();
    }
  }

  if (!webRadioMode && screenOn && !helpVisible && !settingsMenuVisible)
  {
    static char lastClockText[6] = "";
    char clockText[6];
    formatClock(clockText, sizeof(clockText));
    if (strcmp(clockText, lastClockText) != 0)
    {
      strncpy(lastClockText, clockText, sizeof(lastClockText) - 1);
      lastClockText[sizeof(lastClockText) - 1] = '\0';
      drawHeader();
    }
  }

  if (toastActive && millis() > toastEnd)
  {
    toastActive = false;
    if (screenOn && !helpVisible && !settingsMenuVisible)
      drawAll();
  }

  if (hdrMsgEnd > 0 && millis() >= hdrMsgEnd)
  {
    hdrMsgEnd = 0;
    if (screenOn && !helpVisible && !settingsMenuVisible)
    {
      if (webRadioMode)
        drawRadioHeader();
      else
        drawHeader();
    }
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
    if (screenOn && !helpVisible && !settingsMenuVisible)
    {
      if (webRadioMode)
        drawRadioStatus();
      else
        drawStatus();
    }
  }

  // Battery-safe auto screen off: only when idle, never over an open menu/overlay.
  if (screenOn && autoScreenOffSec > 0 && !settingsMenuVisible && !helpVisible &&
      !debugOverlayVisible &&
      !wifiOverlayVisible && !wifiPassOverlayVisible && !addUrlOverlayVisible &&
      !addNameOverlayVisible && !removeConfirmVisible &&
      millis() - lastActivityMs >= (unsigned long)autoScreenOffSec * 1000UL)
  {
    screenOn = false;
    M5Cardputer.Display.setBrightness(0);
  }
}
