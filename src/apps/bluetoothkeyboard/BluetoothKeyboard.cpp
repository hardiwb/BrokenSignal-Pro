#include "apps/bluetoothkeyboard/BluetoothKeyboard.h"

#include "apps/bluetoothkeyboard/BluetoothKeyboardInternal.h"
#include "module/service/Bluetooth.h"
#include "module/shell/Help.h"
#include "UI/List.h"

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
        const int itemCount = bondCount() +
                              (BluetoothService::canPairNew() ? 1 : 0);
        const int shortcutTarget =
            listVisibleShortcutTarget(c, scrollTop, itemCount);
        if (shortcutTarget >= 0)
        {
            selected = shortcutTarget;
            openSelected();
            return true;
        }

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

int bluetoothKeyboardBondForShortcut(char key)
{
    BluetoothKeyboardInternal::initialize();
    BluetoothKeyboardInternal::refreshBonds();
    return BluetoothService::bondIndexForQuickKey(key);
}

bool connectBluetoothKeyboardBond(int bondIndex)
{
    using namespace BluetoothKeyboardInternal;
    refreshBonds();
    if (bondIndex < 0 || bondIndex >= bondCount())
        return false;
    selected = bondIndex;
    scrollTop = min(scrollTop, selected);
    openSelected();
    return BluetoothService::keyboardSessionActive();
}

bool bluetoothKeyboardModalActive()
{
    return BluetoothService::keyboardSessionActive() ||
           BluetoothKeyboardInternal::renameModalActive();
}
