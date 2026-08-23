#pragma once
#include <Arduino.h>

// SD Card pins
#define SD_CS 12
#define SD_MOSI 14
#define SD_CLK 40
#define SD_MISO 39

// DS3231 RTC pins on the Cardputer ADV wiring
#define RTC_SDA 13
#define RTC_SCL 15

// Screen layout
#define SCREEN_W 240
#define SCREEN_H 135
#define HEADER_H 32
#define LIST_Y 33
#define LIST_ITEM_H 14
#define VISIBLE_TRACKS 6
#define STATUS_H 17
#define STATUS_Y (SCREEN_H - STATUS_H)

// Memory limits
#define RECENT_MAX 10
#define SCAN_CACHE_MAX 11
#define PAGE_SIZE 25
#define NAME_CACHE_MAX 200

// Web Radio
#define RADIO_MAX 20
#define RADIO_HTTP_BUF 24576
#define WIFI_TIMEOUT 15000
#define WIFI_SCAN_MAX 10
#define RADIO_INPUT_MAX 200

// Compile-time serial debug logging (USB CDC). 0 = off, 1 = log radio events.
#define DEBUG_SERIAL 0

// Battery
#define BATTERY_INTERVAL (2UL * 60 * 1000)

// Settings (persisted in settings.cfg)
#define SEEK_SECONDS_DEFAULT 10
#define SCREEN_BRIGHTNESS_DEFAULT 128
#define AUTO_SCREEN_OFF_DEFAULT 0
