#include "apps/bluetoothkeyboard/BluetoothKeyboardInternal.h"

#include <cstring>

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/service/Bluetooth.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"

namespace BluetoothKeyboardInternal
{
namespace
{
uint8_t lastReport[8] = {};
bool lastReportValid = false;
bool renaming = false;
String renameValue;

uint8_t fnMappedKey(uint8_t key)
{
    switch (key)
    {
    case 0x35: return 0x29; // ` -> Escape
    case 0x33: return 0x52; // ; -> Up
    case 0x37: return 0x51; // . -> Down
    case 0x36: return 0x50; // , -> Left
    case 0x38: return 0x4F; // / -> Right
    case 0x2A: return 0x4C; // Backspace -> Forward Delete
    default: return key;
    }
}

void buildKeyboardReport(const Keyboard_Class::KeysState &keys, uint8_t report[8])
{
    memset(report, 0, 8);
    report[0] = keys.modifiers;
    if (keys.opt)
        report[0] |= 0x08; // Cardputer Opt becomes Left GUI / Command.

    int outputIndex = 2;
    for (uint8_t key : keys.hid_keys)
    {
        if (outputIndex >= 8)
            break;
        report[outputIndex++] = keys.fn ? fnMappedKey(key) : key;
    }
}

int itemCount()
{
    return BluetoothService::bondCount() +
           (BluetoothService::canPairNew() ? 1 : 0);
}

ListModel buildListModel()
{
    ListModel model;
    model.selected = selected;
    model.scrollTop = scrollTop;

    for (int i = 0; i < BluetoothService::bondCount(); ++i)
    {
        ListItemModel item;
        item.type = ListItemType::Property;
        const String savedName = BluetoothService::bondName(i);
        item.label = savedName.length() > 0
            ? savedName : "PC " + String(i + 1);
        item.value = BluetoothService::bondAddressText(i);
        item.isSelected = i == selected;
        model.items.push_back(item);
    }

    if (BluetoothService::canPairNew())
    {
        ListItemModel item;
        item.type = ListItemType::Normal;
        item.label = "Pair New PC";
        item.isSelected = selected == BluetoothService::bondCount();
        model.items.push_back(item);
    }
    return model;
}
} // namespace

int selected = 0;
int scrollTop = 0;

void initialize()
{
    BluetoothService::begin();
}

void refreshBonds()
{
    BluetoothService::refreshBonds();
    selected = itemCount() > 0 ? constrain(selected, 0, itemCount() - 1) : 0;
    scrollTop = min(scrollTop, selected);
}

int bondCount()
{
    return BluetoothService::bondCount();
}

bool selectedIsBond()
{
    return selected >= 0 && selected < BluetoothService::bondCount();
}

String bondAddressText(int index)
{
    return BluetoothService::bondAddressText(index);
}

bool renameModalActive()
{
    return renaming;
}

void drawListScreen()
{
    refreshBonds();

    HeaderModel header;
    header.appHeaderTag = "BT KEYBOARD";
    header.appHeaderTitle = "Paired Devices";
    header.cursor = true;
    drawHeader(header);
    drawList(buildListModel());

    FooterModel footer;
    footer.left = "[;/.]Move [Ok]Open";
    footer.center = "[Opt]Manage";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void drawSessionOverlay()
{
    if (!BluetoothService::keyboardSessionActive())
        return;

    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = "BLUETOOTH KEYBOARD";
    model.confirmText = "[BtnG0] Disconnect";

    switch (BluetoothService::keyboardLinkState())
    {
    case BluetoothService::KeyboardLinkState::Advertising:
        model.items.push_back(BluetoothService::targetBondSelected()
            ? "WAITING FOR DEVICE" : "PAIRING MODE");
        model.items.push_back(BluetoothService::targetBondSelected()
            ? BluetoothService::targetAddressText()
            : "Select on host Bluetooth");
        break;
    case BluetoothService::KeyboardLinkState::Connected:
        model.items.push_back("SECURING CONNECTION");
        model.items.push_back(BluetoothService::peerAddressText());
        break;
    case BluetoothService::KeyboardLinkState::Pairing:
    {
        char pin[16];
        snprintf(pin, sizeof(pin), "PIN %06lu",
                 (unsigned long)BluetoothService::pairingPasskey());
        model.items.push_back(pin);
        model.items.push_back("Enter PIN on host");
        break;
    }
    case BluetoothService::KeyboardLinkState::Ready:
        model.items.push_back("CONNECTED + ENCRYPTED");
        model.items.push_back(BluetoothService::peerAddressText());
        model.items.push_back("Typing enabled");
        break;
    case BluetoothService::KeyboardLinkState::Failed:
        model.items.push_back("CONNECTION REJECTED");
        model.items.push_back(BluetoothService::keyboardFailureText());
        model.items.push_back("Forget on PC, then press BtnG0");
        break;
    case BluetoothService::KeyboardLinkState::Idle:
        model.items.push_back("DISCONNECTED");
        break;
    }
    drawOverlay(model);
}

void drawRenameOverlay()
{
    OverlayModel model;
    model.type = OverlayType::TextInput;
    model.title = "RENAME PAIRED PC";
    model.prompt = "Friendly name";
    model.value = renameValue;
    model.confirmText = "[Esc]Cancel   [Ok]Save";
    drawOverlay(model);
}

void moveSelection(int direction)
{
    const int count = itemCount();
    if (count <= 0)
        return;

    const int oldSelected = selected;
    const int oldScrollTop = scrollTop;
    selected = (selected + direction + count) % count;
    if (selected < scrollTop)
        scrollTop = selected;
    else if (selected >= scrollTop + LIST_VISIBLE_ITEM)
        scrollTop = selected - LIST_VISIBLE_ITEM + 1;

    if (oldScrollTop != scrollTop)
        drawListScreen();
    else
        drawListSelection(buildListModel(), oldSelected, selected);
}

void openSelected()
{
    const int bondIndex = selectedIsBond() ? selected : -1;
    memset(lastReport, 0, sizeof(lastReport));
    lastReportValid = false;
    if (BluetoothService::startKeyboardSession(bondIndex))
        drawSessionOverlay();
}

void beginRenameSelected()
{
    if (!selectedIsBond())
        return;

    renameValue = BluetoothService::bondName(selected);
    if (renameValue.length() == 0)
        renameValue = "PC " + String(selected + 1);
    renaming = true;
    drawRenameOverlay();
}

void handleRenameInput(Keyboard_Class::KeysState &keys)
{
    if (keyboardBackPressed(keys))
    {
        renaming = false;
        drawListScreen();
        return;
    }

    if (keys.enter)
    {
        renameValue.trim();
        BluetoothService::setBondName(selected, renameValue);
        renaming = false;
        drawListScreen();
        return;
    }

    if (keys.del && renameValue.length() > 0)
    {
        renameValue.remove(renameValue.length() - 1);
        drawOverlayInputValue(renameValue);
        return;
    }

    for (char c : keys.word)
    {
        if (keyboardTextInputChar(keys, c) && renameValue.length() < 24)
        {
            renameValue += c;
            drawOverlayInputValue(renameValue);
        }
    }
}

void disconnectSession()
{
    BluetoothService::stopKeyboardSession();
    memset(lastReport, 0, sizeof(lastReport));
    lastReportValid = false;
    drawListScreen();
}

void forgetSelectedBond()
{
    if (selectedIsBond())
        BluetoothService::forgetBond(selected);
    selected = 0;
    drawListScreen();
}

void forgetAllBonds()
{
    BluetoothService::forgetAllBonds();
    selected = 0;
    drawListScreen();
}

void consumeModalMatrixInput(Keyboard_Class::KeysState &)
{
    // Reports are polled in tick(), because M5's isChange() only compares the
    // key count and misses a same-count substitution.
}

void tick()
{
    if (!BluetoothService::keyboardSessionActive())
    {
        if (BluetoothService::takeUiDirty())
            drawListScreen();
        return;
    }

    if (M5Cardputer.BtnA.wasPressed())
    {
        disconnectSession();
        BluetoothService::takeUiDirty();
        return;
    }

    if (BluetoothService::keyboardReady())
    {
        uint8_t report[8];
        buildKeyboardReport(M5Cardputer.Keyboard.keysState(), report);
        if (!lastReportValid ||
            memcmp(report, lastReport, sizeof(report)) != 0)
        {
            BluetoothService::sendKeyboardReport(report);
            memcpy(lastReport, report, sizeof(lastReport));
            lastReportValid = true;
            lastActivityMs = millis();
            if (!screenOn)
                wakeScreen();
        }
    }
    else
    {
        // A new BLE connection has a new notification channel. Resend the
        // complete matrix state when it becomes ready, even if no key changed.
        lastReportValid = false;
    }

    if (BluetoothService::takeUiDirty())
    {
        refreshBonds();
        drawSessionOverlay();
    }
}
} // namespace BluetoothKeyboardInternal
