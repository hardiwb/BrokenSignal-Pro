#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace BluetoothService
{
constexpr int MAX_BONDS = 3;

enum class KeyboardLinkState : uint8_t
{
    Idle,
    Advertising,
    Connected,
    Pairing,
    Ready,
    Failed
};

// Shared BLE lifecycle. Future profiles should be registered here so apps do
// not initialize or compete for the ESP32 Bluetooth controller independently.
void begin();
void tick();

void refreshBonds();
int bondCount();
bool canPairNew();
String bondAddressText(int index);
String bondName(int index);
bool setBondName(int index, const String &name);
bool forgetBond(int index);
void forgetAllBonds();
bool mouseInvertY();
void setMouseInvertY(bool inverted);
uint8_t mouseSensitivityPercent();
void setMouseSensitivityPercent(uint8_t percent);
uint8_t mouseScreenCount();
void setMouseScreenCount(uint8_t count);

// Pass -1 to accept a new bond, or a bond-list index to restrict the session
// to that saved peer.
bool startKeyboardSession(int bondIndex);
void stopKeyboardSession();
bool keyboardSessionActive();
KeyboardLinkState keyboardLinkState();
uint32_t pairingPasskey();
String peerAddressText();
String targetAddressText();
bool targetBondSelected();
String keyboardFailureText();
bool keyboardReady();
void sendKeyboardReport(const uint8_t report[8]);
void sendMouseReport(
    uint8_t buttons, uint16_t x, uint16_t y,
    int8_t wheel, int8_t horizontalWheel = 0);

// Returns and clears the service-to-UI notification flag.
bool takeUiDirty();
} // namespace BluetoothService
