#include "apps/bluetoothkeyboard/BluetoothKeyboard.h"

#include "apps/bluetoothkeyboard/BluetoothKeyboardInternal.h"
#include "module/service/Bluetooth.h"
#include "module/shell/Help.h"

void openBluetoothKeyboardApp()
{
    BluetoothKeyboardInternal::initialize();
    BluetoothKeyboardInternal::refreshBonds();
    BluetoothKeyboardInternal::drawListScreen();
}

void drawBluetoothKeyboardApp()
{
    if (BluetoothKeyboardInternal::renameModalActive())
        BluetoothKeyboardInternal::drawRenameOverlay();
    else if (BluetoothService::keyboardSessionActive())
        BluetoothKeyboardInternal::drawSessionOverlay();
    else
        BluetoothKeyboardInternal::drawListScreen();
}

bool handleBluetoothKeyboardAppInput(Keyboard_Class::KeysState &keys)
{
    using namespace BluetoothKeyboardInternal;

    if (renameModalActive())
    {
        handleRenameInput(keys);
        return true;
    }

    if (BluetoothService::keyboardSessionActive())
    {
        consumeModalMatrixInput(keys);
        return true;
    }

    if (keys.enter)
    {
        openSelected();
        return true;
    }

    for (char c : keys.word)
    {
        if (c == 'h' || c == 'H')
        {
            toggleHelp();
            return true;
        }
        if (c == ';')
        {
            moveSelection(-1);
            return true;
        }
        if (c == '.')
        {
            moveSelection(+1);
            return true;
        }
    }

    return true;
}

void tickBluetoothKeyboardApp()
{
    BluetoothKeyboardInternal::tick();
}

bool bluetoothKeyboardModalActive()
{
    return BluetoothService::keyboardSessionActive() ||
           BluetoothKeyboardInternal::renameModalActive();
}
