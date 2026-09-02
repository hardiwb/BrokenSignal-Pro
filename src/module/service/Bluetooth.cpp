#include "module/service/Bluetooth.h"

#include <BLEAdvertising.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <Preferences.h>
#include <esp_gap_ble_api.h>
#include <esp_system.h>
#include <cstring>
#include <vector>

namespace BluetoothService
{
namespace
{
const uint8_t HID_REPORT_MAP[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05,
    0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
    0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0xC0
};

BLEServer *server = nullptr;
BLEHIDDevice *hid = nullptr;
BLECharacteristic *inputReport = nullptr;
BLEAdvertising *advertising = nullptr;
BLESecurity *security = nullptr;
Preferences preferences;

esp_ble_bond_dev_t bonds[MAX_BONDS] = {};
int storedBondCount = 0;
int totalBondCount = 0;

volatile KeyboardLinkState linkState = KeyboardLinkState::Idle;
volatile bool sessionActive = false;
volatile bool linkConnected = false;
volatile bool authenticated = false;
volatile bool uiDirty = false;
volatile uint16_t connectionId = 0;
volatile uint32_t displayedPasskey = 0;
volatile bool pendingDisconnect = false;
volatile bool remoteDisconnected = false;
volatile bool authenticationFailed = false;
volatile bool targetMismatch = false;
volatile uint8_t authenticationFailureReason = 0;

bool initialized = false;
bool preferencesOpen = false;
bool selectedTarget = false;
int selectedBondIndex = -1;
esp_bd_addr_t targetAddress = {};
esp_bd_addr_t peerAddress = {};

bool addressesEqual(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, sizeof(esp_bd_addr_t)) == 0;
}

bool addressMatchesSelectedBond(const uint8_t *address)
{
    if (!selectedTarget || selectedBondIndex < 0 ||
        selectedBondIndex >= storedBondCount)
        return true;

    const esp_ble_bond_dev_t &bond = bonds[selectedBondIndex];
    if (addressesEqual(address, bond.bd_addr))
        return true;

    // A bonded host may reconnect using a resolvable private address. The
    // authentication result can then contain its stable identity address.
    return (bond.bond_key.key_mask & ESP_LE_KEY_PID) &&
           addressesEqual(address, bond.bond_key.pid_key.static_addr);
}

String formatAddress(const uint8_t *address)
{
    char text[18];
    snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
             address[0], address[1], address[2],
             address[3], address[4], address[5]);
    return String(text);
}

String bondNameKey(const uint8_t *address)
{
    char key[14];
    snprintf(key, sizeof(key), "n%02X%02X%02X%02X%02X%02X",
             address[0], address[1], address[2],
             address[3], address[4], address[5]);
    return String(key);
}

class SharedServerCallbacks final : public BLEServerCallbacks
{
public:
    void onConnect(BLEServer *, esp_ble_gatts_cb_param_t *param) override
    {
        connectionId = param->connect.conn_id;
        memcpy(peerAddress, param->connect.remote_bda, sizeof(peerAddress));
        linkConnected = true;
        authenticated = false;
        linkState = KeyboardLinkState::Connected;
        uiDirty = true;

        if (esp_ble_set_encryption(
                param->connect.remote_bda,
                ESP_BLE_SEC_ENCRYPT_MITM) != ESP_OK)
        {
            linkState = KeyboardLinkState::Failed;
            pendingDisconnect = true;
        }
    }

    void onDisconnect(BLEServer *, esp_ble_gatts_cb_param_t *) override
    {
        linkConnected = false;
        authenticated = false;
        displayedPasskey = 0;
        remoteDisconnected = true;
        uiDirty = true;
    }
};

class SharedSecurityCallbacks final : public BLESecurityCallbacks
{
public:
    uint32_t onPassKeyRequest() override
    {
        const uint32_t passkey = 100000U + (esp_random() % 900000U);
        displayedPasskey = passkey;
        linkState = KeyboardLinkState::Pairing;
        uiDirty = true;
        return passkey;
    }

    void onPassKeyNotify(uint32_t passkey) override
    {
        displayedPasskey = passkey;
        linkState = KeyboardLinkState::Pairing;
        uiDirty = true;
    }

    bool onSecurityRequest() override
    {
        return sessionActive;
    }

    bool onConfirmPIN(uint32_t passkey) override
    {
        displayedPasskey = passkey;
        linkState = KeyboardLinkState::Pairing;
        uiDirty = true;
        return true;
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override
    {
        const bool mismatch = result.success &&
                              !addressMatchesSelectedBond(result.bd_addr);
        if (!result.success || mismatch)
        {
            authenticated = false;
            authenticationFailed = !result.success;
            authenticationFailureReason = result.fail_reason;
            targetMismatch = mismatch;
            linkState = KeyboardLinkState::Failed;
            pendingDisconnect = true;
            uiDirty = true;
            Serial.printf("[Bluetooth] authentication rejected: success=%d reason=0x%02X mismatch=%d peer=%s\n",
                          result.success, result.fail_reason, mismatch,
                          formatAddress(result.bd_addr).c_str());
            return;
        }

        memcpy(peerAddress, result.bd_addr, sizeof(peerAddress));
        authenticated = true;
        authenticationFailed = false;
        targetMismatch = false;
        authenticationFailureReason = 0;
        displayedPasskey = 0;
        linkState = KeyboardLinkState::Ready;
        uiDirty = true;
    }
};

SharedServerCallbacks serverCallbacks;
SharedSecurityCallbacks securityCallbacks;

void sendReleaseReport()
{
    const uint8_t empty[8] = {};
    sendKeyboardReport(empty);
}
} // namespace

void begin()
{
    if (initialized)
        return;

    preferencesOpen = preferences.begin("bt-keyboard", false);

    BLEDevice::init("BrokenSignal Keyboard");
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
    BLEDevice::setSecurityCallbacks(&securityCallbacks);

    server = BLEDevice::createServer();
    server->setCallbacks(&serverCallbacks);

    hid = new BLEHIDDevice(server);
    inputReport = hid->inputReport(1);
    hid->outputReport(1);
    hid->manufacturer()->setValue("BrokenSignal Pro");
    hid->pnp(0x02, 0x303A, 0x4001, 0x0100);
    hid->hidInfo(0x00, 0x01);
    hid->reportMap(const_cast<uint8_t *>(HID_REPORT_MAP), sizeof(HID_REPORT_MAP));
    hid->startServices();

    security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    security->setCapability(ESP_IO_CAP_OUT);
    security->setKeySize(16);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    uint8_t requireConfiguredSecurity = 1;
    esp_ble_gap_set_security_param(
        ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH,
        &requireConfiguredSecurity,
        sizeof(requireConfiguredSecurity));

    advertising = server->getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);

    initialized = true;
    refreshBonds();
}

void tick()
{
    if (remoteDisconnected)
    {
        remoteDisconnected = false;
        refreshBonds();
        if (sessionActive && advertising)
        {
            if (authenticationFailed || targetMismatch)
            {
                // Do not create a reconnect storm when a host retained stale
                // keys or the wrong saved host answered the advertisement.
                advertising->stop();
            }
            else
            {
                // Keep the chosen keyboard channel alive, like an Easy-Switch
                // slot. A host can reconnect without reopening the app.
                linkState = KeyboardLinkState::Advertising;
                advertising->start();
            }
        }
        else
        {
            linkState = KeyboardLinkState::Idle;
        }
        uiDirty = true;
    }

    if (pendingDisconnect)
    {
        pendingDisconnect = false;
        if (linkConnected && server)
            server->disconnect(connectionId);
    }
}

void refreshBonds()
{
    storedBondCount = 0;
    totalBondCount = max(0, esp_ble_get_bond_device_num());
    if (totalBondCount <= 0)
        return;

    std::vector<esp_ble_bond_dev_t> allBonds(totalBondCount);
    int fetched = totalBondCount;
    if (esp_ble_get_bond_device_list(&fetched, allBonds.data()) != ESP_OK)
    {
        totalBondCount = 0;
        return;
    }

    storedBondCount = min(fetched, MAX_BONDS);
    for (int i = 0; i < storedBondCount; ++i)
        bonds[i] = allBonds[i];
    totalBondCount = fetched;
}

int bondCount()
{
    return storedBondCount;
}

bool canPairNew()
{
    return totalBondCount < MAX_BONDS;
}

String bondAddressText(int index)
{
    if (index < 0 || index >= storedBondCount)
        return "";
    return formatAddress(bonds[index].bd_addr);
}

String bondName(int index)
{
    if (!preferencesOpen || index < 0 || index >= storedBondCount)
        return "";
    return preferences.getString(bondNameKey(bonds[index].bd_addr).c_str(), "");
}

bool setBondName(int index, const String &name)
{
    if (!preferencesOpen || index < 0 || index >= storedBondCount)
        return false;

    String cleaned = name;
    cleaned.trim();
    if (cleaned.length() == 0)
        return preferences.remove(bondNameKey(bonds[index].bd_addr).c_str());

    return preferences.putString(
               bondNameKey(bonds[index].bd_addr).c_str(), cleaned) > 0;
}

bool forgetBond(int index)
{
    refreshBonds();
    if (index < 0 || index >= storedBondCount)
        return false;
    if (preferencesOpen)
        preferences.remove(bondNameKey(bonds[index].bd_addr).c_str());
    const bool removed = esp_ble_remove_bond_device(bonds[index].bd_addr) == ESP_OK;
    refreshBonds();
    uiDirty = true;
    return removed;
}

void forgetAllBonds()
{
    refreshBonds();
    for (int i = 0; i < storedBondCount; ++i)
    {
        if (preferencesOpen)
            preferences.remove(bondNameKey(bonds[i].bd_addr).c_str());
        esp_ble_remove_bond_device(bonds[i].bd_addr);
    }
    refreshBonds();
    uiDirty = true;
}

bool startKeyboardSession(int bondIndex)
{
    if (!initialized || sessionActive || !advertising)
        return false;
    if (bondIndex >= storedBondCount || (bondIndex < 0 && !canPairNew()))
        return false;

    selectedTarget = bondIndex >= 0;
    selectedBondIndex = selectedTarget ? bondIndex : -1;
    if (selectedTarget)
        memcpy(targetAddress, bonds[bondIndex].bd_addr, sizeof(targetAddress));
    else
        memset(targetAddress, 0, sizeof(targetAddress));

    displayedPasskey = 0;
    authenticated = false;
    linkConnected = false;
    pendingDisconnect = false;
    remoteDisconnected = false;
    authenticationFailed = false;
    targetMismatch = false;
    authenticationFailureReason = 0;
    linkState = KeyboardLinkState::Advertising;
    sessionActive = true;
    uiDirty = true;
    advertising->start();
    return true;
}

void stopKeyboardSession()
{
    if (!sessionActive)
        return;

    advertising->stop();
    sendReleaseReport();
    sessionActive = false;
    authenticated = false;
    displayedPasskey = 0;
    linkState = KeyboardLinkState::Idle;
    authenticationFailed = false;
    targetMismatch = false;
    authenticationFailureReason = 0;

    if (linkConnected && server)
        server->disconnect(connectionId);
    linkConnected = false;
    refreshBonds();
    uiDirty = true;
}

bool keyboardSessionActive()
{
    return sessionActive;
}

KeyboardLinkState keyboardLinkState()
{
    return linkState;
}

uint32_t pairingPasskey()
{
    return displayedPasskey;
}

String peerAddressText()
{
    return formatAddress(peerAddress);
}

String targetAddressText()
{
    return formatAddress(targetAddress);
}

bool targetBondSelected()
{
    return selectedTarget;
}

String keyboardFailureText()
{
    if (targetMismatch)
        return "Wrong saved device";
    if (authenticationFailed)
    {
        char text[28];
        snprintf(text, sizeof(text), "Pairing error 0x%02X",
                 authenticationFailureReason);
        return String(text);
    }
    return "Connection rejected";
}

bool keyboardReady()
{
    return sessionActive && linkConnected && authenticated;
}

void sendKeyboardReport(const uint8_t report[8])
{
    if (!inputReport || !keyboardReady())
        return;
    inputReport->setValue(const_cast<uint8_t *>(report), 8);
    inputReport->notify();
}

bool takeUiDirty()
{
    if (!uiDirty)
        return false;
    uiDirty = false;
    return true;
}
} // namespace BluetoothService
