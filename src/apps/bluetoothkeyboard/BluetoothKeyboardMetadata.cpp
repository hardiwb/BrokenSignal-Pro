#include "apps/bluetoothkeyboard/BluetoothKeyboardMetadata.h"

#include "apps/bluetoothkeyboard/BluetoothKeyboardInternal.h"

const HelpEntry BLUETOOTH_KEYBOARD_HELP_ENTRIES[] = {
    {"[Ok]", "Connect / pair"},
    {"[;/.]", "Cursor up / down"},
    {"[Opt]", "Manage pairings"},
    {"[BtnG0]", "Disconnect typing mode"},
    {"[Fn+`]", "Send Escape"},
    {"[Fn+;/.]", "Send Up / Down"},
    {"[Fn+,//]", "Send Left / Right"},
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
}
