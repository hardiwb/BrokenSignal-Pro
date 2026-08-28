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

void drawMusicApp()
{
    drawPlayerHeader();
    pumpAudio();
    drawPlayerList();
    pumpAudio();
    drawPlayerStatus();
}

bool handleMusicAppInput(Keyboard_Class::KeysState &keys)
{
    for (auto c : keys.word)
    {
        if (c == 'w' || c == 'W')
        {
            enterWebRadioMode();
            return true;
        }
    }

    if (handleBrowserInput(keys))
        return true;
    handlePlayerInput(keys);
    return true;
}

void tickMusicApp()
{
}
