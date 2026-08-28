#include "apps/music/MusicApp.h"

#include "core/System.h"
#include "module/service/FileBrowser.h"
#include "apps/music/MusicPlayer.h"
#include "apps/radio/Radio.h"

void openMusicApp()
{
    calculatorVisible = false;
    calculatorOverlayMode = false;
    notesMode = false;

    if (webRadioMode)
        exitWebRadioMode();
    else
        drawMusicApp();
}

bool handleMusicAppInput(Keyboard_Class::KeysState &keys)
{
    if (handleBrowserInput(keys))
        return true;
    handlePlayerInput(keys);
    return true;
}

void tickMusicApp()
{
}
