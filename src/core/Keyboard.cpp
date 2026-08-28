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
bool globalHotkeysAllowed(const ActiveSurface &surface)
{
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

bool handleGlobalQuickAccessHotkey(
    const ActiveSurface &surface,
    Keyboard_Class::KeysState &ks)
{
    if (!globalHotkeysAllowed(surface))
        return false;

    return appRuntimeHandleQuickAccess(ks);
}

bool handleGlobalUtilityHotkey(
    const ActiveSurface &surface,
    Keyboard_Class::KeysState &ks)
{
    if (!globalHotkeysAllowed(surface) ||
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

    if (ks.alt)
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

    if (ks.opt)
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
