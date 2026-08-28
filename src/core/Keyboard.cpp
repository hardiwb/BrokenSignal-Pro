#include "Keyboard.h"
#include <M5Cardputer.h>
#include "State.h"

#include "System.h"
#include "core/AppRuntime.h"
#include "core/SurfaceManager.h"

#include "module/service/FileBrowser.h"
#include "apps/calculator/Calculator.h"
#include "module/shell/Debug.h"
#include "module/shell/Help.h"
#include "apps/music/MusicPlayer.h"
#include "module/shell/Settings.h"
#include "module/shell/Applications.h"
#include "module/shell/Options.h"
#include "apps/notes/Notes.h"
#include "apps/radio/Radio.h"
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

    appRuntimeOpen(HostApp::Notes);
}

KeyboardInputMode currentKeyboardInputMode()
{
    if (optionsMenuVisible)
        return KeyboardInputMode::Options;

    if (helpVisible)
        return KeyboardInputMode::Help;

    if (applicationsMenuVisible)
        return KeyboardInputMode::Applications;

    if (settingsMenuVisible)
        return KeyboardInputMode::Settings;

    if (debugOverlayVisible)
        return KeyboardInputMode::Debug;

    if (notesEditorVisible() || notesMoveDateInputActive())
        return KeyboardInputMode::Notes;

    if (calculatorInputActive())
        return KeyboardInputMode::Calculator;

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

bool handleGlobalHotkey(Keyboard_Class::KeysState &ks)
{
    for (auto c : ks.word)
    {
        switch (c)
        {
        case 'w':
        case 'W':
            appRuntimeOpen(webRadioMode ? HostApp::Music : HostApp::Radio);
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
            appRuntimeOpen(HostApp::Music);
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

    const ActiveSurface surface = resolveActiveSurface();

    if (keyboardBackPressed(ks) && closeTopmostSurface(surface))
        return;

    if (!optionsMenuVisible &&
        surface.kind == SurfaceKind::HostApp &&
        !notesMode &&
        !calculatorInputActive() &&
        isFnN(ks))
    {
        openNotesAppFromShortcut();
        return;
    }

    const bool modifierOnly = ks.word.empty();
    const bool modalInputActive =
        surface.kind == SurfaceKind::OverlayModal;

    if (surface.kind == SurfaceKind::HostApp &&
        !modalInputActive &&
        modifierOnly &&
        ks.ctrl)
    {
        enterSettingsMenu();
        return;
    }

    const bool applicationHostActive =
        surface.kind == SurfaceKind::HostApp;

    if (applicationHostActive && modifierOnly && ks.alt)
    {
        enterApplicationsMenu();
        return;
    }

    if (applicationHostActive && modifierOnly && ks.opt)
    {
        enterOptionsMenu();
        return;
    }

    KeyboardInputMode mode = currentKeyboardInputMode();

    if (surface.kind == SurfaceKind::HostApp &&
        appRuntimeHandleQuickAccess(ks))
        return;

    if (surface.kind == SurfaceKind::HostApp &&
        (mode == KeyboardInputMode::Player ||
         mode == KeyboardInputMode::Radio) &&
        handleGlobalHotkey(ks))
        return;

    if (!surfaceBlocksHostInput(surface))
    {
        appRuntimeHandleForegroundInput(ks);
        return;
    }

    if (surface.kind == SurfaceKind::OverlayModal &&
        appRuntimeHandleQuickAccessInput(ks))
    {
        return;
    }

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
    case KeyboardInputMode::Player:
        appRuntimeHandleForegroundInput(ks);
        return;
    }
}
