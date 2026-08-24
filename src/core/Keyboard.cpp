#include "Keyboard.h"
#include <M5Cardputer.h>
#include "State.h"

#include "../UI/UI.h"
#include "../UI/RadioUI.h"

#include "../module/Browser.h"
#include "../module/Player.h"
#include "../module/Notes.h"
#include "../module/Radio.h"

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

    // -------------------------
    // System overlays
    // -------------------------

    if (settingsMenuVisible)
    {
        handleSettingsInput(ks);
        return;
    }

    if (debugOverlayVisible)
    {
        for (auto c : ks.word)
        {
            if (c == 'd' || c == 'D')
            {
                toggleDebug();
                return;
            }
        }

        return;
    }

    if (helpVisible)
    {
        for (auto c : ks.word)
        {
            if (c == 'h' || c == 'H')
            {
                toggleHelp();
                return;
            }
        }

        return;
    }

    // -------------------------
    // Notes application
    // -------------------------

    if (notesMode)
    {
        handleNotesInput(ks);
        return;
    }

    // -------------------------
    // Global hotkeys
    // -------------------------

    for (auto c : ks.word)
    {
        switch (c)
        {
        case 'n':
        case 'N':
            notesOpen();
            return;

        case 'w':
        case 'W':
            enterWebRadioMode();
            return;

        case 'm':
        case 'M':
            enterSettingsMenu();
            return;

        case 'h':
        case 'H':
            toggleHelp();
            return;

        case 'd':
        case 'D':
            toggleDebug();
            return;

        case 'o':
        case 'O':
            toggleScreen();
            return;

            //==================================================
            // THEME
            //==================================================

        case '1':
            setTheme(0);
            return;

        case '2':
            setTheme(1);
            return;

        case '3':
            setTheme(2);
            return;

        case '4':
            setTheme(3);
            return;

        case '5':
            setTheme(4);
            return;
        }
    }


    // -------------------------
    // Radio application
    // -------------------------

    if (webRadioMode)
    {
        bool overlayOpen =
            wifiOverlayVisible ||
            wifiPassOverlayVisible ||
            addUrlOverlayVisible ||
            addNameOverlayVisible ||
            removeConfirmVisible;

        if (overlayOpen)
            handleOverlayInput(ks);
        else
            handleRadioInput(ks);

        return;
    }

    if (handleBrowserInput(ks))
        return;

    handlePlayerInput(ks);
}
