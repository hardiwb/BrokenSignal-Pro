#include "apps/mediacontrol/MediaControl.h"

#include "core/Keyboard.h"
#include "core/State.h"
#include "module/service/Bluetooth.h"
#include "module/shell/Help.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"

namespace
{
struct MediaAction
{
    const char *name;
    const char *hotkey;
    char key;
    uint16_t usage;
};

constexpr MediaAction ACTIONS[] = {
    {"Play / Pause", "[Space]", ' ', 0x00CD},
    {"Previous Track", "[,]", ',', 0x00B6},
    {"Next Track", "[/]", '/', 0x00B5},
    {"Stop", "[S]", 's', 0x00B7},
    {"Mute", "[0]", '0', 0x00E2},
    {"Volume Down", "[-]", '-', 0x00EA},
    {"Volume Up", "[=]", '=', 0x00E9},
};
constexpr int ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);
constexpr uint32_t RELEASE_DELAY_MS = 60;

int selectedDevice = 0;
int deviceScrollTop = 0;
int selectedAction = 0;
int actionScrollTop = 0;
int activeAction = -1;
uint32_t releaseAtMs = 0;
uint32_t marqueeStartMs = 0;

int deviceItemCount()
{
    return BluetoothService::bondCount() +
           (BluetoothService::canPairNew() ? 1 : 0);
}

ListModel deviceListModel()
{
    ListModel model;
    model.selected = selectedDevice;
    model.scrollTop = deviceScrollTop;
    model.marqueeStartMs = marqueeStartMs;

    for (int i = 0; i < BluetoothService::bondCount(); ++i)
    {
        ListItemModel item;
        const String name = BluetoothService::bondName(i);
        item.label = name.length() ? name : "Saved PC " + String(i + 1);
        item.value = BluetoothService::bondAddressText(i);
        item.isSelected = i == selectedDevice;
        model.items.push_back(item);
    }
    if (BluetoothService::canPairNew())
    {
        ListItemModel item;
        item.label = "Pair New PC";
        item.value = "BLE";
        item.isSelected = selectedDevice == BluetoothService::bondCount();
        model.items.push_back(item);
    }
    return model;
}

ListModel actionListModel()
{
    ListModel model;
    model.selected = selectedAction;
    model.scrollTop = actionScrollTop;
    for (int i = 0; i < ACTION_COUNT; ++i)
    {
        ListItemModel item;
        item.label = ACTIONS[i].name;
        item.value = ACTIONS[i].hotkey;
        item.type = ListItemType::Property;
        item.propertyWidth = 54;
        item.isSelected = i == selectedAction;
        item.isActive = i == activeAction;
        model.items.push_back(item);
    }
    return model;
}

void drawDevices()
{
    const int count = deviceItemCount();
    selectedDevice = count > 0 ? constrain(selectedDevice, 0, count - 1) : 0;
    deviceScrollTop = min(deviceScrollTop, selectedDevice);

    HeaderModel header;
    header.appHeaderTag = "MEDIA";
    header.appHeaderTitle = "Select PC";
    header.cursor = true;
    drawHeader(header);
    drawList(deviceListModel());

    FooterModel footer;
    footer.left = "[Ok]Connect [;/.]Move";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void drawControls()
{
    HeaderModel header;
    header.appHeaderTag = "MEDIA";
    header.appHeaderTitle = activeAction >= 0
        ? "Sending " + String(ACTIONS[activeAction].name)
        : "Remote controls";
    header.cursor = true;
    drawHeader(header);
    drawList(actionListModel());

    FooterModel footer;
    footer.left = "[Hotkey]Send [;/.]Move";
    footer.center = activeAction >= 0 ? "SENDING" : "[Ok]Send";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void drawConnectionOverlay()
{
    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = "MEDIA CONTROL";
    model.confirmText = "[BtnG0] Disconnect";
    switch (BluetoothService::keyboardLinkState())
    {
    case BluetoothService::KeyboardLinkState::Advertising:
        model.items = {
            BluetoothService::targetBondSelected()
                ? "WAITING FOR SAVED PC" : "PAIRING MODE",
            BluetoothService::targetBondSelected()
                ? BluetoothService::targetAddressText()
                : "Select BrokenSignal Keyboard"};
        break;
    case BluetoothService::KeyboardLinkState::Connected:
        model.items = {"SECURING CONNECTION", BluetoothService::peerAddressText()};
        break;
    case BluetoothService::KeyboardLinkState::Pairing:
    {
        char pin[16];
        snprintf(pin, sizeof(pin), "PIN %06lu",
                 static_cast<unsigned long>(BluetoothService::pairingPasskey()));
        model.items = {pin, "Enter PIN on host"};
        break;
    }
    case BluetoothService::KeyboardLinkState::Failed:
        model.items = {"CONNECTION REJECTED", BluetoothService::keyboardFailureText()};
        break;
    case BluetoothService::KeyboardLinkState::Idle:
        model.items = {"DISCONNECTED"};
        break;
    case BluetoothService::KeyboardLinkState::Ready:
        return;
    }
    drawOverlay(model);
}

void moveSelection(int &selected, int &scrollTop, int count, int direction,
                   void (*redraw)())
{
    if (count <= 0)
        return;
    selected = (selected + direction + count) % count;
    if (selected < scrollTop)
        scrollTop = selected;
    else if (selected >= scrollTop + LIST_VISIBLE_ITEM)
        scrollTop = selected - LIST_VISIBLE_ITEM + 1;
    redraw();
}

void startSelectedDevice()
{
    const int bondIndex = selectedDevice < BluetoothService::bondCount()
        ? selectedDevice : -1;
    if (BluetoothService::startKeyboardSession(bondIndex))
    {
        drawConnectionOverlay();
    }
}

void sendAction(int index)
{
    if (!BluetoothService::keyboardReady() || activeAction >= 0 ||
        index < 0 || index >= ACTION_COUNT)
        return;

    selectedAction = index;
    activeAction = index;
    BluetoothService::sendConsumerControl(ACTIONS[index].usage);
    releaseAtMs = millis() + RELEASE_DELAY_MS;
    drawControls();
}
} // namespace

void openMediaControlApp()
{
    BluetoothService::begin();
    if (BluetoothService::keyboardSessionActive())
        BluetoothService::stopKeyboardSession();
    BluetoothService::refreshBonds();
    selectedDevice = 0;
    deviceScrollTop = 0;
    selectedAction = 0;
    actionScrollTop = 0;
    activeAction = -1;
    marqueeStartMs = millis();
    drawDevices();
}

void drawMediaControlApp()
{
    if (BluetoothService::keyboardSessionActive())
    {
        if (BluetoothService::keyboardReady())
            drawControls();
        else
            drawConnectionOverlay();
        return;
    }
    drawDevices();
}

bool handleMediaControlAppInput(Keyboard_Class::KeysState &keys)
{
    if (BluetoothService::keyboardSessionActive())
    {
        if (!BluetoothService::keyboardReady())
            return true;
        if (keys.enter)
        {
            sendAction(selectedAction);
            return true;
        }
        for (char c : keys.word)
        {
            char key = c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
            for (int i = 0; i < ACTION_COUNT; ++i)
            {
                if (ACTIONS[i].key == key)
                {
                    sendAction(i);
                    return true;
                }
            }
            if (c == ';' || c == '.')
            {
                moveSelection(selectedAction, actionScrollTop, ACTION_COUNT,
                              c == ';' ? -1 : +1, drawControls);
                return true;
            }
            if (c == 'h' || c == 'H')
            {
                toggleHelp();
                return true;
            }
        }
        return true;
    }

    if (keys.enter)
    {
        startSelectedDevice();
        return true;
    }
    for (char c : keys.word)
    {
        const int target = listVisibleShortcutTarget(
            c, deviceScrollTop, deviceItemCount());
        if (target >= 0)
        {
            selectedDevice = target;
            startSelectedDevice();
            return true;
        }
        if (c == ';' || c == '.')
        {
            moveSelection(selectedDevice, deviceScrollTop, deviceItemCount(),
                          c == ';' ? -1 : +1, drawDevices);
            return true;
        }
        if (c == 'h' || c == 'H')
        {
            toggleHelp();
            return true;
        }
    }
    return true;
}

void tickMediaControlApp()
{
    if (M5Cardputer.BtnA.wasPressed() &&
        BluetoothService::keyboardSessionActive())
    {
        mediaControlDisconnect();
        return;
    }

    if (BluetoothService::takeUiDirty())
        drawMediaControlApp();

    if (activeAction >= 0 &&
        static_cast<int32_t>(millis() - releaseAtMs) >= 0)
    {
        BluetoothService::sendConsumerControl(0);
        activeAction = -1;
        drawControls();
    }
}

void mediaControlDisconnect()
{
    if (activeAction >= 0)
        BluetoothService::sendConsumerControl(0);
    activeAction = -1;
    BluetoothService::stopKeyboardSession();
    BluetoothService::takeUiDirty();
    BluetoothService::refreshBonds();
    drawDevices();
}

bool mediaControlModalActive()
{
    return foregroundApp == HostApp::MediaControl &&
           BluetoothService::keyboardSessionActive() &&
           !BluetoothService::keyboardReady();
}
