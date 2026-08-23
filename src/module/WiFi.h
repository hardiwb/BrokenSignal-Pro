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