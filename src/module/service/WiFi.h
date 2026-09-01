#pragma once

#include "core/State.h"

//==================================================
// WIFI CONFIG
//==================================================

bool loadWifiConfig(String &ssid, String &pass);
void saveWifiConfig(const String &ssid, const String &pass);

//==================================================
// WIFI CONNECTION
//==================================================

void scanWifiNetworks();
bool connectWifi(const String &ssid, const String &pass);
void applyWifiPowerSave();

enum class WifiInputResult
{
    Ignored,
    Consumed,
    Connected,
    ExitRequested,
    ReturnToHost
};

enum class WifiStartupResult
{
    Connected,
    AwaitingInput
};

//==================================================
// WIFI UI
//==================================================

void drawWifiHeader();
void drawWifiList();
void drawWifiRow(int idx);
void drawWifiFooter();
void drawWifiMenu();
void drawWifiPassOverlay(bool inputOnly = false);

void showWifiMenu();
void showWifiPassOverlay(const String &ssid);

void openWifiMenu();
void closeWifiInput();
WifiStartupResult ensureWifiConnected();
bool wifiInputActive();
WifiInputResult handleWifiInput(Keyboard_Class::KeysState &ks);
