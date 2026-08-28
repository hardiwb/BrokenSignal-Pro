#include "apps/music/MusicPlayer.h"

#include "core/State.h"
#include "core/System.h"

void handlePlayerInput(Keyboard_Class::KeysState &ks)
{
    for (auto c : ks.word)
    {
        switch (c)
        {
        case ' ':
            if (isPlaying)
                pauseAudio();
            else if (isPaused)
                resumeAudio();
            break;

        case 'r':
        case 'R':
            cycleRepeat();
            break;

        case 's':
        case 'S':
            toggleShuffle();
            break;

        case '+':
        case '=':
            volume = (uint8_t)min(255, (int)volume + 10);
            M5Cardputer.Speaker.setVolume(volume);
            settingsDirty = true;
            settingsDirtyMs = millis();
            drawPlayerStatus();
            showVolumeMessage();
            break;

        case '-':
            volume = (uint8_t)max(0, (int)volume - 10);
            M5Cardputer.Speaker.setVolume(volume);
            settingsDirty = true;
            settingsDirtyMs = millis();
            drawPlayerStatus();
            showVolumeMessage();
            break;
        }
    }
}
