#include "apps/bluetoothkeyboard/BluetoothKeyboardMetadata.h"

#include "apps/bluetoothkeyboard/BluetoothKeyboardInternal.h"

const HelpEntry BLUETOOTH_KEYBOARD_HELP_ENTRIES[] = {
    {"[Ok]", "Connect / pair"},
    {"[;/.]", "Cursor up / down"},
    {"[Opt]", "Manage pairings"},
    {"[BtnG0]", "Disconnect typing mode"},
    {"[Fn+M]", "Enter gyroscope mouse"},
    {"[`]", "Mouse: return to keyboard"},
    {"[Ok]", "Mouse: left button"},
    {"[Opt]", "Mouse: right button"},
    {"[Alt]", "Mouse: middle button"},
    {"[;/.]", "Mouse: scroll up/down"},
    {"[,//]", "Mouse: scroll left/right"},
    {"[Ctrl]", "Mouse: center cursor"},
    {"[Space]", "Mouse: hold gyro clutch"},
    {"[O/P]", "Mouse: upper quadrants"},
    {"[K/L]", "Mouse: lower quadrants"},
    {"[Tab]", "Mouse: switch screen"},
};

const uint8_t BLUETOOTH_KEYBOARD_HELP_COUNT =
    sizeof(BLUETOOTH_KEYBOARD_HELP_ENTRIES) /
    sizeof(BLUETOOTH_KEYBOARD_HELP_ENTRIES[0]);

namespace
{
void renameSelected(int)
{
    BluetoothKeyboardInternal::beginRenameSelected();
}

void forgetSelected(int)
{
    BluetoothKeyboardInternal::forgetSelectedBond();
}

void forgetAll(int)
{
    BluetoothKeyboardInternal::forgetAllBonds();
}

void adjustInvertMouseY(int)
{
    BluetoothKeyboardInternal::toggleMouseYInverted();
}

void adjustMouseSensitivity(int direction)
{
    BluetoothKeyboardInternal::adjustMouseSensitivity(direction);
}

void adjustMouseScreenCount(int direction)
{
    BluetoothKeyboardInternal::adjustMouseScreenCount(direction);
}
} // namespace

void buildBluetoothKeyboardOptions(std::vector<AppOption> &options)
{
    const int count = BluetoothKeyboardInternal::bondCount();
    options.push_back({
        "Rename Selected", "", BluetoothKeyboardInternal::selectedIsBond(),
        false, true, renameSelected});
    options.push_back({
        "Forget Selected", "", BluetoothKeyboardInternal::selectedIsBond(),
        false, true, forgetSelected});
    options.push_back({
        "Forget All", "", count > 0, false, true, forgetAll});
    options.push_back({
        "Invert Mouse Y",
        BluetoothKeyboardInternal::mouseYInverted() ? "On" : "Off",
        true, true, false, adjustInvertMouseY});
    options.push_back({
        "Mouse Sensitivity",
        String(BluetoothKeyboardInternal::mouseSensitivityPercent()) + "%",
        true, true, false, adjustMouseSensitivity});
    options.push_back({
        "Mouse Screens",
        String(BluetoothKeyboardInternal::mouseScreenCount()),
        true, true, false, adjustMouseScreenCount});
}
