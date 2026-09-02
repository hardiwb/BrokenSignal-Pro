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
#include "apps/expenses/Expenses.h"
#include "apps/radio/Radio.h"
#include "apps/bluetoothkeyboard/BluetoothKeyboard.h"
#include "apps/thermalprinter/ThermalPrinter.h"
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

bool keyboardApplicationsShortcutPressed(Keyboard_Class::KeysState &ks)
{
    return ks.word.empty() && (swapAltOpt ? ks.opt : ks.alt);
}

bool keyboardOptionsShortcutPressed(Keyboard_Class::KeysState &ks)
{
    return ks.word.empty() && (swapAltOpt ? ks.alt : ks.opt);
}

const char *keyboardApplicationsShortcutLabel()
{
    return swapAltOpt ? "Opt" : "Alt";
}

const char *keyboardOptionsShortcutLabel()
{
    return swapAltOpt ? "Alt" : "Opt";
}

namespace
{
bool hostLikeGlobalHotkeysAllowed(const ActiveSurface &surface)
{
    // QuickPopup is visual-only feedback, not a text/input overlay, so it
    // passes keys through to the host. OverlayModal owns text input and rejects
    // global hotkeys by never reaching this path.
    return surface.kind == SurfaceKind::HostApp ||
           surface.kind == SurfaceKind::QuickPopup;
}

bool foregroundAllowsGlobalUtilityHotkeys()
{
    // Text-heavy apps may reserve common letters. Keep letter utilities only on
    // hosts that do not collide today.
    return foregroundApp == HostApp::Music ||
           foregroundApp == HostApp::Radio;
}

bool foregroundAllowsGlobalVolumeHotkeys()
{
    // Text/edit overlays are protected by OverlayModal routing. Calculator only
    // joins global volume on its full-screen history/list surface.
    return foregroundApp == HostApp::Music ||
           foregroundApp == HostApp::Radio ||
           foregroundApp == HostApp::Notes ||
           (foregroundApp == HostApp::Calculator && calculatorHistoryActive());
}

bool handleGlobalQuickAccessHotkey(
    const ActiveSurface &surface,
    Keyboard_Class::KeysState &ks)
{
    if (!hostLikeGlobalHotkeysAllowed(surface))
        return false;

    return appRuntimeHandleQuickAccess(ks);
}

bool handleGlobalHardwareHotkey(
    const ActiveSurface &surface,
    Keyboard_Class::KeysState &ks)
{
    if (!hostLikeGlobalHotkeysAllowed(surface))
        return false;

    for (auto c : ks.word)
    {
        switch (c)
        {
        case '[':
            adjustSystemBrightness(-1);
            return true;

        case ']':
            adjustSystemBrightness(+1);
            return true;

        case '-':
            if (foregroundAllowsGlobalVolumeHotkeys())
            {
                adjustSystemVolume(-1);
                return true;
            }
            break;

        case '+':
        case '=':
            if (foregroundAllowsGlobalVolumeHotkeys())
            {
                adjustSystemVolume(+1);
                return true;
            }
            break;
        }
    }

    return false;
}

bool handleGlobalUtilityHotkey(
    const ActiveSurface &surface,
    Keyboard_Class::KeysState &ks)
{
    if (!hostLikeGlobalHotkeysAllowed(surface) ||
        !foregroundAllowsGlobalUtilityHotkeys())
    {
        return false;
    }

    for (auto c : ks.word)
    {
        switch (c)
        {
        case 'h':
        case 'H':
            toggleHelp();
            return true;

        case 'o':
        case 'O':
            toggleScreen();
            return true;
        }
    }

    return false;
}

void handleWifiModeInput(Keyboard_Class::KeysState &ks)
{
    WifiInputResult result = handleWifiInput(ks);

    if (result == WifiInputResult::Connected)
    {
        if (thermalPrinterResumePendingOperation())
            return;
        if (expensesResumePendingUpload())
            return;
        if (webRadioMode)
            drawRadioAll();
        else
            drawAll();
        return;
    }

    if (result == WifiInputResult::ReturnToHost)
    {
        if (webRadioMode)
            drawRadioAll();
        else
            drawAll();
        return;
    }

    if (result == WifiInputResult::ExitRequested)
    {
        expensesCancelPendingUpload();
        thermalPrinterCancelPendingOperation();
        if (webRadioMode)
            appRuntimeOpen(HostApp::Music);
        else
            drawAll();
    }
}

bool shellNavigationAllowed(const ActiveSurface &surface)
{
    return surface.kind == SurfaceKind::HostApp ||
           surface.kind == SurfaceKind::MainMenu ||
           surface.kind == SurfaceKind::ContextMenu ||
           surface.kind == SurfaceKind::QuickPopup;
}

void closeActiveShellMenu()
{
    if (applicationsMenuVisible)
    {
        exitApplicationsMenu();
        return;
    }

    if (optionsMenuVisible)
    {
        exitOptionsMenu();
        return;
    }

    if (settingsMenuVisible)
    {
        exitSettingsMenu();
        return;
    }

    if (wifiMenuVisible)
    {
        expensesCancelPendingUpload();
        thermalPrinterCancelPendingOperation();
        closeWifiInput();
        if (webRadioMode)
            drawRadioAll();
        else
            drawAll();
    }
}

bool handleShellNavigationShortcut(
    const ActiveSurface &surface,
    Keyboard_Class::KeysState &ks)
{
    if (!shellNavigationAllowed(surface) || !ks.word.empty())
        return false;

    if (ks.ctrl)
    {
        if (settingsMenuVisible)
            exitSettingsMenu();
        else
        {
            closeActiveShellMenu();
            enterSettingsMenu();
        }
        return true;
    }

    if (keyboardApplicationsShortcutPressed(ks))
    {
        if (applicationsMenuVisible)
            exitApplicationsMenu();
        else
        {
            closeActiveShellMenu();
            enterApplicationsMenu();
        }
        return true;
    }

    if (keyboardOptionsShortcutPressed(ks))
    {
        if (optionsMenuVisible)
            exitOptionsMenu();
        else
        {
            closeActiveShellMenu();
            enterOptionsMenu();
        }
        return true;
    }

    return false;
}

void handleModalSurfaceInput(Keyboard_Class::KeysState &ks)
{
    if (appRuntimeHandleQuickAccessInput(ks))
        return;

    if (settingsInputOverlayActive())
    {
        handleSettingsInput(ks);
        return;
    }

    if (notesMoveDateInputActive())
    {
        handleNotesInput(ks);
        return;
    }

    if (expensesModalActive())
    {
        handleExpensesInput(ks);
        return;
    }

    if (thermalPrinterModalActive())
    {
        handleThermalPrinterInput(ks);
        return;
    }

    if (calculatorEditActive())
    {
        handleCalculatorInput(ks);
        return;
    }

    if (wifiPassOverlayVisible)
    {
        handleWifiModeInput(ks);
        return;
    }

    if (radioOverlayActive())
    {
        handleRadioOverlayInput(ks);
        return;
    }
}

void handlePopupSurfaceInput(Keyboard_Class::KeysState &ks)
{
    if (helpVisible)
    {
        handleHelpInput(ks);
        return;
    }

    if (debugOverlayVisible)
    {
        handleDebugInput(ks);
        return;
    }
}

void handleContextSurfaceInput(Keyboard_Class::KeysState &ks)
{
    if (optionsMenuVisible)
    {
        handleOptionsInput(ks);
        return;
    }

    if (settingsMenuVisible)
    {
        handleSettingsInput(ks);
        return;
    }

    if (wifiMenuVisible)
    {
        handleWifiModeInput(ks);
        return;
    }
}

void handleHostSurfaceInput(Keyboard_Class::KeysState &ks)
{
    const ActiveSurface surface = resolveActiveSurface();

    if (handleGlobalQuickAccessHotkey(surface, ks))
        return;

    if (handleGlobalHardwareHotkey(surface, ks))
        return;

    if (handleGlobalUtilityHotkey(surface, ks))
        return;

    appRuntimeHandleForegroundInput(ks);
}
} // namespace

void keyboardLoop()
{
    if (isScanning)
        return;

    if (!M5Cardputer.Keyboard.isChange())
        return;

    Keyboard_Class::KeysState ks =
        M5Cardputer.Keyboard.keysState();

    // Bluetooth typing owns the complete matrix, including Esc and shell
    // modifiers. BtnG0 is intentionally the only local disconnect control.
    if (bluetoothKeyboardModalActive())
    {
        lastActivityMs = millis();
        handleBluetoothKeyboardAppInput(ks);
        return;
    }

    if (!M5Cardputer.Keyboard.isPressed())
        return;

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

    if (handleShellNavigationShortcut(surface, ks))
        return;

    switch (surface.kind)
    {
    case SurfaceKind::HostApp:
    case SurfaceKind::QuickPopup:
        handleHostSurfaceInput(ks);
        return;

    case SurfaceKind::MainMenu:
        handleApplicationsInput(ks);
        return;

    case SurfaceKind::ContextMenu:
        handleContextSurfaceInput(ks);
        return;

    case SurfaceKind::OverlayModal:
        handleModalSurfaceInput(ks);
        return;

    case SurfaceKind::OverlayPopup:
        handlePopupSurfaceInput(ks);
        return;

    case SurfaceKind::None:
        return;
    }
}
