#include "core/SurfaceManager.h"

#include "core/AppRuntime.h"
#include "core/System.h"
#include "apps/calculator/Calculator.h"
#include "apps/notes/Notes.h"
#include "apps/radio/Radio.h"
#include "module/service/WiFi.h"
#include "module/shell/Applications.h"
#include "module/shell/Debug.h"
#include "module/shell/Help.h"
#include "module/shell/Options.h"
#include "module/shell/Settings.h"
#include "UI/Toast.h"

namespace
{
bool quickPopupVisible()
{
    return toastActive || (hdrMsgEnd > 0 && hdrMsg.length() > 0);
}
} // namespace

ActiveSurface resolveActiveSurface()
{
    const HostApp owner = foregroundApp;

    if (wifiPassOverlayVisible || addUrlOverlayVisible || addNameOverlayVisible ||
        removeConfirmVisible || settingsInputOverlayActive() ||
        notesMoveDateInputActive() || calculatorEditActive() ||
        appRuntimeQuickAccessActive())
    {
        return {SurfaceKind::OverlayModal, owner};
    }

    if (helpVisible || debugOverlayVisible)
        return {SurfaceKind::OverlayPopup, owner};

    if (applicationsMenuVisible)
        return {SurfaceKind::MainMenu, owner};

    if (settingsMenuVisible || optionsMenuVisible || wifiMenuVisible)
        return {SurfaceKind::ContextMenu, owner};

    if (quickPopupVisible())
        return {SurfaceKind::QuickPopup, owner};

    return {SurfaceKind::HostApp, owner};
}

bool surfaceBlocksHostInput(const ActiveSurface &surface)
{
    switch (surface.kind)
    {
    case SurfaceKind::HostApp:
    case SurfaceKind::QuickPopup:
    case SurfaceKind::None:
        return false;
    case SurfaceKind::MainMenu:
    case SurfaceKind::ContextMenu:
    case SurfaceKind::OverlayModal:
    case SurfaceKind::OverlayPopup:
        return true;
    }

    return true;
}

bool closeTopmostSurface(const ActiveSurface &surface)
{
    switch (surface.kind)
    {
    case SurfaceKind::OverlayModal:
        if (appRuntimeCloseQuickAccess())
            return true;

        if (settingsInputOverlayActive())
        {
            cancelSettingsInputOverlay();
            return true;
        }

        if (notesMoveDateInputActive())
        {
            cancelNotesMoveDateInput();
            return true;
        }

        if (calculatorEditActive())
        {
            cancelCalculatorEdit();
            return true;
        }

        if (wifiPassOverlayVisible)
        {
            wifiPassOverlayVisible = false;
            showWifiMenu();
            return true;
        }

        if (addUrlOverlayVisible)
        {
            addUrlOverlayVisible = false;
            drawRadioAll();
            return true;
        }

        if (addNameOverlayVisible)
        {
            addNameOverlayVisible = false;
            drawRadioAll();
            return true;
        }

        if (removeConfirmVisible)
        {
            removeConfirmVisible = false;
            drawRadioAll();
            return true;
        }

        return false;

    case SurfaceKind::OverlayPopup:
        if (helpVisible)
        {
            toggleHelp();
            return true;
        }

        if (debugOverlayVisible)
        {
            toggleDebug();
            return true;
        }
        return false;

    case SurfaceKind::MainMenu:
        if (applicationsMenuVisible)
        {
            exitApplicationsMenu();
            return true;
        }
        return false;

    case SurfaceKind::ContextMenu:
        if (optionsMenuVisible)
        {
            exitOptionsMenu();
            return true;
        }

        if (settingsMenuVisible)
        {
            exitSettingsMenu();
            return true;
        }

        if (wifiMenuVisible || wifiPassOverlayVisible)
        {
            closeWifiInput();
            if (webRadioMode)
                drawRadioAll();
            else
                drawAll();
            return true;
        }
        return false;

    case SurfaceKind::QuickPopup:
        if (toastActive)
        {
            dismissToast();
            if (screenOn)
                drawAll();
            return true;
        }

        if (hdrMsgEnd > 0)
        {
            hdrMsgEnd = 0;
            if (screenOn)
                drawAll();
            return true;
        }
        return false;

    case SurfaceKind::HostApp:
        enterApplicationsMenu();
        return true;

    case SurfaceKind::None:
        return false;
    }

    return false;
}
