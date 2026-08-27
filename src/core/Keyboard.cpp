#include "Keyboard.h"
#include <M5Cardputer.h>
#include "State.h"

#include "System.h"

#include "module/programs/Browser.h"
#include "module/programs/Calculator.h"
#include "module/shell/Debug.h"
#include "module/shell/Help.h"
#include "module/programs/Player.h"
#include "module/shell/Settings.h"
#include "module/shell/Applications.h"
#include "module/shell/Options.h"
#include "module/programs/Notes.h"
#include "module/programs/Radio.h"
#include "module/service/WiFi.h"

bool keyboardBackPressed(Keyboard_Class::KeysState &ks)
{
    static const uint8_t HID_ESCAPE = 0x29;

    for (auto c : ks.word)
    {
        if (c == 27 || (!ks.fn && c == '`'))
            return true;
    }

    for (auto key : ks.hid_keys)
    {
        if (key == HID_ESCAPE)
            return true;
    }

    return false;
}

bool keyboardTextInputChar(Keyboard_Class::KeysState &ks, char c)
{
    if (c < 32 || c >= 127)
        return false;

    if (c == '`' && !ks.fn)
        return false;

    return true;
}

namespace
{
static const uint8_t HID_KEY_N = 0x11;

enum class KeyboardInputMode
{
    Options,
    Calculator,
    Applications,
    Settings,
    Debug,
    Help,
    Notes,
    Wifi,
    RadioOverlay,
    Radio,
    Player
};

bool isFnN(
    Keyboard_Class::KeysState &ks)
{
    if (!ks.fn)
        return false;

    for (auto c : ks.word)
    {
        if (c == 'n' ||
            c == 'N')
            return true;
    }

    for (auto key : ks.hid_keys)
    {
        if (key == HID_KEY_N)
            return true;
    }

    return false;
}

void openNotesAppFromShortcut()
{
    settingsMenuVisible = false;
    debugOverlayVisible = false;
    calculatorVisible = false;
    helpVisible = false;

    wifiMenuVisible = false;
    wifiPassOverlayVisible = false;

    addUrlOverlayVisible = false;
    addNameOverlayVisible = false;
    removeConfirmVisible = false;

    notesOpen();
}

KeyboardInputMode currentKeyboardInputMode()
{
    if (optionsMenuVisible)
        return KeyboardInputMode::Options;

    // Calculator is an overlay and always receives input before its host screen.
    if (calculatorInputActive())
        return KeyboardInputMode::Calculator;

    if (helpVisible)
        return KeyboardInputMode::Help;

    if (applicationsMenuVisible)
        return KeyboardInputMode::Applications;

    if (settingsMenuVisible)
        return KeyboardInputMode::Settings;

    if (debugOverlayVisible)
        return KeyboardInputMode::Debug;

    if (notesInputActive())
        return KeyboardInputMode::Notes;

    if (wifiInputActive())
        return KeyboardInputMode::Wifi;

    if (webRadioMode)
        return radioOverlayActive()
                   ? KeyboardInputMode::RadioOverlay
                   : KeyboardInputMode::Radio;

    return KeyboardInputMode::Player;
}

bool calculatorHostAllowsOverlay(KeyboardInputMode mode)
{
    switch (mode)
    {
    case KeyboardInputMode::Options:
        return false;
    case KeyboardInputMode::Settings:
        return !settingsInputOverlayActive();
    case KeyboardInputMode::Applications:
        return false;
    case KeyboardInputMode::Notes:
        return !notesEditorVisible();
    case KeyboardInputMode::Wifi:
        return wifiMenuVisible && !wifiPassOverlayVisible;
    case KeyboardInputMode::RadioOverlay:
    case KeyboardInputMode::Calculator:
        return false;
    case KeyboardInputMode::Debug:
    case KeyboardInputMode::Help:
    case KeyboardInputMode::Radio:
    case KeyboardInputMode::Player:
        return true;
    }

    return false;
}

bool tryOpenCalculatorOverlay(
    KeyboardInputMode mode,
    const Keyboard_Class::KeysState &ks)
{
    if (!calculatorHostAllowsOverlay(mode))
        return false;

    for (auto c : ks.word)
    {
        if (c == 'c' || c == 'C')
        {
            openCalculator();
            return true;
        }
    }

    return false;
}

bool handleGlobalHotkey(Keyboard_Class::KeysState &ks)
{
    for (auto c : ks.word)
    {
        switch (c)
        {
        case 'n':
        case 'N':
            notesQuickOpen();
            return true;

        case 'w':
        case 'W':
            if (webRadioMode)
                exitWebRadioMode();
            else
                enterWebRadioMode();
            return true;

        case 'c':
        case 'C':
            openCalculator();
            return true;

        case 'h':
        case 'H':
            toggleHelp();
            return true;

        case 'd':
        case 'D':
            toggleDebug();
            return true;

        case 'o':
        case 'O':
            toggleScreen();
            return true;

        case '1':
            setTheme(0);
            return true;

        case '2':
            setTheme(1);
            return true;

        case '3':
            setTheme(2);
            return true;

        case '4':
            setTheme(3);
            return true;

        case '5':
            setTheme(4);
            return true;
        }
    }

    return false;
}

void handleWifiModeInput(Keyboard_Class::KeysState &ks)
{
    WifiInputResult result = handleWifiInput(ks);

    if (result == WifiInputResult::Connected ||
        result == WifiInputResult::ReturnToHost)
    {
        if (webRadioMode)
            drawRadioAll();
        else
            drawAll();
        return;
    }

    if (result == WifiInputResult::ExitRequested)
    {
        if (webRadioMode)
            exitWebRadioMode();
        else
            drawAll();
    }
}
} // namespace

void keyboardLoop()
{
    if (isScanning)
        return;

    if (!M5Cardputer.Keyboard.isChange())
        return;

    if (!M5Cardputer.Keyboard.isPressed())
        return;

    Keyboard_Class::KeysState ks =
        M5Cardputer.Keyboard.keysState();

    lastActivityMs = millis();

    // Screen sedang OFF:
    if (!screenOn)
    {
        wakeScreen();
        return;
    }

    if (!optionsMenuVisible &&
        !notesMode &&
        !calculatorInputActive() &&
        !settingsInputOverlayActive() &&
        !notesEditorVisible() &&
        !notesMoveDateInputActive() &&
        !wifiPassOverlayVisible &&
        !radioOverlayActive() &&
        isFnN(ks))
    {
        openNotesAppFromShortcut();
        return;
    }

    const bool modifierOnly = ks.word.empty();
    const bool modalInputActive =
        optionsMenuVisible || calculatorInputActive() ||
        settingsInputOverlayActive() ||
        notesEditorVisible() || notesMoveDateInputActive() ||
        wifiPassOverlayVisible || radioOverlayActive();

    if (!settingsMenuVisible && !modalInputActive && modifierOnly && ks.ctrl)
    {
        enterSettingsMenu();
        return;
    }

    const bool applicationHostActive =
        !applicationsMenuVisible && !settingsMenuVisible &&
        !helpVisible && !debugOverlayVisible &&
        !wifiInputActive() && !notesEditorVisible() &&
        !notesMoveDateInputActive() &&
        !radioOverlayActive();

    if (applicationHostActive && modifierOnly && ks.alt)
    {
        enterApplicationsMenu();
        return;
    }

    const bool optionsHostActive =
        !optionsMenuVisible && applicationHostActive;

    if (optionsHostActive && modifierOnly && ks.opt)
    {
        enterOptionsMenu();
        return;
    }

    KeyboardInputMode mode = currentKeyboardInputMode();

    if (tryOpenCalculatorOverlay(mode, ks))
        return;

    if ((mode == KeyboardInputMode::Player ||
         mode == KeyboardInputMode::Radio) &&
        handleGlobalHotkey(ks))
        return;

    switch (mode)
    {
    case KeyboardInputMode::Options:
        handleOptionsInput(ks);
        return;

    case KeyboardInputMode::Calculator:
        handleCalculatorInput(ks);
        return;

    case KeyboardInputMode::Applications:
        handleApplicationsInput(ks);
        return;

    case KeyboardInputMode::Settings:
        handleSettingsInput(ks);
        return;

    case KeyboardInputMode::Debug:
        handleDebugInput(ks);
        return;

    case KeyboardInputMode::Help:
        handleHelpInput(ks);
        return;

    case KeyboardInputMode::Notes:
        handleNotesInput(ks);
        return;

    case KeyboardInputMode::Wifi:
        handleWifiModeInput(ks);
        return;

    case KeyboardInputMode::RadioOverlay:
        handleRadioOverlayInput(ks);
        return;

    case KeyboardInputMode::Radio:
        handleRadioInput(ks);
        return;

    case KeyboardInputMode::Player:
        break;
    }

    if (handleBrowserInput(ks))
        return;

    handlePlayerInput(ks);
}
