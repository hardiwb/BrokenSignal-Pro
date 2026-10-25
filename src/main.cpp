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
#include "apps/expenses/Expenses.h"
#include "module/shell/Debug.h"
#include "apps/radio/Radio.h"
#include "module/shell/Settings.h"
#include "module/service/Clock.h"
#include "module/service/WiFi.h"
#include "module/service/EspNowNotes.h"
#include "module/service/OtaUpdate.h"
#include "module/service/NotesNotion.h"
#include "module/service/SdTransfer.h"
#include "module/service/Bluetooth.h"
#include "apps/notes/Notes.h"
#include "apps/bluetoothkeyboard/BluetoothKeyboard.h"
#include "apps/thermalprinter/ThermalPrinter.h"

namespace
{
unsigned long playbackSleepStartMs = 0;

constexpr unsigned long BATTERY_TREND_SAMPLE_MS = 30000UL;
constexpr unsigned long BATTERY_TREND_WINDOW_MS = 5UL * 60UL * 1000UL;
constexpr int BATTERY_CHARGING_RISE_MV = 30;
constexpr size_t BATTERY_TREND_SAMPLE_COUNT =
    BATTERY_TREND_WINDOW_MS / BATTERY_TREND_SAMPLE_MS + 1;

struct BatteryVoltageSample
{
  unsigned long atMs = 0;
  int16_t millivolts = 0;
};

BatteryVoltageSample batteryVoltageSamples[BATTERY_TREND_SAMPLE_COUNT];
size_t batteryVoltageSampleCount = 0;
size_t batteryVoltageSampleNext = 0;
unsigned long batteryVoltageLastSampleMs = 0;
bool batteryTrendCharging = false;

void recordBatteryVoltageSample()
{
  const unsigned long now = millis();
  const int16_t millivolts = M5.Power.getBatteryVoltage();
  if (millivolts <= 0)
    return;

  batteryVoltageSamples[batteryVoltageSampleNext].atMs = now;
  batteryVoltageSamples[batteryVoltageSampleNext].millivolts = millivolts;
  batteryVoltageSampleNext =
      (batteryVoltageSampleNext + 1) % BATTERY_TREND_SAMPLE_COUNT;
  if (batteryVoltageSampleCount < BATTERY_TREND_SAMPLE_COUNT)
    batteryVoltageSampleCount++;
  batteryVoltageLastSampleMs = now;

  if (batteryVoltageSampleCount < BATTERY_TREND_SAMPLE_COUNT)
  {
    batteryTrendCharging = false;
    return;
  }

  const BatteryVoltageSample &oldest =
      batteryVoltageSamples[batteryVoltageSampleNext];
  const bool fullWindow = now - oldest.atMs >= BATTERY_TREND_WINDOW_MS - 5000UL;
  batteryTrendCharging =
      fullWindow && millivolts - oldest.millivolts >= BATTERY_CHARGING_RISE_MV;
}

void tickBatteryVoltageTrend()
{
  if (batteryVoltageSampleCount == 0 ||
      millis() - batteryVoltageLastSampleMs >= BATTERY_TREND_SAMPLE_MS)
    recordBatteryVoltageSample();
}

bool chargingBehaviorActive()
{
  return chargingModeEnabled || batteryTrendCharging;
}

enum class PowerOffReminderStage : uint8_t
{
  None,
  FirstMinute,
  FinalMinute
};

PowerOffReminderStage powerOffReminderStage = PowerOffReminderStage::None;
unsigned long powerOffReminderStartMs = 0;
unsigned long powerOffReminderActivityMs = 0;

bool powerOffReminderActive()
{
  return powerOffReminderStage != PowerOffReminderStage::None;
}

void finishPowerOffSequence()
{
  saveSettings();
  settingsDirty = false;
  enterDeepSleep();
}

void beginPowerOffSequence()
{
  if (chargingBehaviorActive())
    return;

  if (!turnOffReminderEnabled || volume == 0)
  {
    finishPowerOffSequence();
    return;
  }

  playPowerOffReminder(1);
  powerOffReminderStage = PowerOffReminderStage::FirstMinute;
  powerOffReminderStartMs = millis();
  powerOffReminderActivityMs = lastActivityMs;
}

void tickPowerOffSequence()
{
  if (!powerOffReminderActive())
    return;

  if (chargingBehaviorActive())
  {
    powerOffReminderStage = PowerOffReminderStage::None;
    return;
  }

  // Any deliberate interaction means the user is still using the device.
  if (lastActivityMs != powerOffReminderActivityMs)
  {
    powerOffReminderStage = PowerOffReminderStage::None;
    return;
  }

  const unsigned long elapsed = millis() - powerOffReminderStartMs;
  if (elapsed >= 120000UL)
  {
    playPowerOffReminder(2);
    finishPowerOffSequence();
  }
  else if (powerOffReminderStage == PowerOffReminderStage::FirstMinute && elapsed >= 60000UL)
  {
    playPowerOffReminder(1);
    powerOffReminderStage = PowerOffReminderStage::FinalMinute;
  }
}

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

    if (key == "theme" && val >= 0 && val < THEME_COUNT)
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
  // Deep-sleep GPIO holds survive reset; release them before M5Unified
  // configures the display and SD interfaces again.
  releaseDeepSleepPinHolds();

  auto cfg = M5.config();
  // BrokenSignal uses an external DS3231 and does not use the ADV microphone.
  // Keep the internal IMU enabled because Bluetooth gyro mouse mode uses it.
  cfg.internal_rtc = false;
  cfg.internal_mic = false;
  cfg.internal_imu = true;

  M5Cardputer.begin(cfg, true);

  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("=== BOOT START ===");

  const bool g0Wake = wokeFromG0DeepSleep();
  Serial.printf("Wake source: %s\n", g0Wake ? "BtnG0 deep sleep" : "normal boot");

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
  recordBatteryVoltageSample();
  const HostApp bootApp =
      g0Wake
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

  tickEspNowNotes();
  if (notesCalendarSyncActive())
    tickNotesCalendarSync(takeEspNowNotesResult());
  tickOtaUpdate();
  tickNotesNotionSetup();
  tickSdTransfer();
  BluetoothService::tick();

  bool anyPlaying = isPlaying || radioIsPlaying;
  delay(anyPlaying ? 1 : 10);

  keyboardLoop();
  tickBatteryVoltageTrend();
  tickPowerOffSequence();

  if (!anyPlaying)
    playbackSleepStartMs = 0;
  else if (playbackSleepStartMs == 0)
    playbackSleepStartMs = millis();

  // A quick-access overlay owns the display until it closes. Do not let the
  // underlying foreground app repaint rows through it.
  if (!appRuntimeQuickAccessActive())
    appRuntimeTickForeground();

  // UI Update
  static unsigned long lastDraw = 0;
  if (screenOn && !optionsMenuVisible && !applicationsMenuVisible && !helpVisible &&
      !settingsMenuVisible && !notesInputActive() &&
      !calculatorInputActive() &&
      !expensesModalActive() &&
      !thermalPrinterModalActive() &&
      !bluetoothKeyboardModalActive() &&
      millis() - lastDraw >= 500)
  {
    lastDraw = millis();
    cursorVisible = !cursorVisible;

    if (debugOverlayVisible)
    {
      drawHeaderCursor(cursorVisible);
    }
    else if (foregroundApp == HostApp::Radio)
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
    else if (foregroundApp == HostApp::Music)
    {
      drawHeaderCursor(cursorVisible);
      if (selectedItem >= 0 && selectedItem < (int)items.size())
        drawPlayerRow(selectedItem);
      if (isPlaying)
        updatePlayerStatus();
    }
    else
    {
      drawHeaderCursor(cursorVisible);
    }
  }

  if (screenOn && !optionsMenuVisible && !applicationsMenuVisible && !helpVisible &&
      !settingsMenuVisible && !notesInputActive() &&
      !calculatorInputActive() &&
      !expensesModalActive() &&
      !thermalPrinterModalActive() &&
      !bluetoothKeyboardModalActive() &&
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
        !expensesModalActive() &&
        !thermalPrinterModalActive() &&
        !bluetoothKeyboardModalActive() &&
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
        !expensesModalActive() &&
        !thermalPrinterModalActive() &&
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
    sleepScreen();
  }

  if (!chargingBehaviorActive() && deepSleepSec > 0 &&
      !isPlaying && !radioIsPlaying && !espNowNotesBusy() &&
      !otaUpdateActive() && !notesNotionSetupActive() &&
      !sdTransferActive() &&
      !bluetoothKeyboardModalActive() &&
      !powerOffReminderActive() &&
      millis() - lastActivityMs >= (unsigned long)deepSleepSec * 1000UL)
  {
    beginPowerOffSequence();
  }

  if (!powerOffReminderActive() && playbackOffSec > 0 && anyPlaying && playbackSleepStartMs > 0 &&
      millis() - playbackSleepStartMs >= (unsigned long)playbackOffSec * 1000UL)
  {
    stopAudio();
    stopRadioStream();
    if (!chargingBehaviorActive())
      beginPowerOffSequence();
  }
}
