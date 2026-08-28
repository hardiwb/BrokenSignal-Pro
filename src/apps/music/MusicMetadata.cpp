#include "apps/music/MusicMetadata.h"

#include "apps/music/MusicPlayer.h"

const HelpEntry MUSIC_HELP_ENTRIES[] = {
    {"[Ok]", "Open / play"},
    {"[Del]", "Parent folder"},
    {"[Space]", "Pause / resume"},
    {"[;/.]", "Cursor up / down"},
    {"[,/]", "Seek back / forward"},
    {"[-/+]", "Volume"},
    {"[W]", "Switch to radio"},
    {"[N]", "Quick note"},
    {"[Esc]", "Applications"},
    {"[Opt]", "Toggle Options"},
    {"[Alt]", "Toggle Applications"},
    {"[Ctrl]", "Toggle Control Panel"},
    {"[C]", "Calculator"},
    {"[O]", "Screen on / off"},
    {"[H]", "Close help"},
};

const uint8_t MUSIC_HELP_COUNT =
    sizeof(MUSIC_HELP_ENTRIES) / sizeof(MUSIC_HELP_ENTRIES[0]);

namespace
{
String repeatLabel()
{
    static const char *labels[] = {"Off", "One", "All"};
    return labels[min((int)repeatMode, 2)];
}

String volumeLabel()
{
    return String(((int)volume * 100 + 127) / 255) + "%";
}

String playbackTimerLabel()
{
    if (playbackOffSec == 0)
        return "Off";
    if (playbackOffSec < 3600)
        return String(playbackOffSec / 60) + "m";
    return String(playbackOffSec / 3600) + "h";
}

void markSettingsDirty()
{
    settingsDirty = true;
    settingsDirtyMs = millis();
}

void adjustShuffle(int)
{
    toggleShuffle();
}

void adjustRepeat(int direction)
{
    repeatMode = (repeatMode + direction + 3) % 3;
    markSettingsDirty();
}

void adjustVolume(int direction)
{
    volume = (uint8_t)constrain((int)volume + direction * 10, 0, 255);
    M5Cardputer.Speaker.setVolume(volume);
    markSettingsDirty();
}

void adjustSeek(int direction)
{
    static const uint8_t steps[] = {5, 10, 15, 20, 30, 45, 60};
    constexpr int count = sizeof(steps) / sizeof(steps[0]);
    int index = 0;
    for (int i = 0; i < count; ++i)
    {
        if (steps[i] == seekSeconds)
        {
            index = i;
            break;
        }
    }
    seekSeconds = steps[(index + direction + count) % count];
    markSettingsDirty();
}

void adjustPlaybackTimer(int direction)
{
    static const uint32_t timers[] = {0, 1800, 3600, 7200, 10800};
    constexpr int count = sizeof(timers) / sizeof(timers[0]);
    int index = 0;
    for (int i = 0; i < count; ++i)
    {
        if (timers[i] == playbackOffSec)
        {
            index = i;
            break;
        }
    }
    playbackOffSec = timers[(index + direction + count) % count];
    markSettingsDirty();
}
} // namespace

void buildMusicOptions(std::vector<AppOption> &options)
{
    options.push_back({"Shuffle", shuffleOn ? "On" : "Off", true, true, false, adjustShuffle});
    options.push_back({"Repeat", repeatLabel(), true, true, false, adjustRepeat});
    options.push_back({"Volume", volumeLabel(), true, true, false, adjustVolume});
    options.push_back({"Seek Step", String(seekSeconds) + "s", true, true, false, adjustSeek});
    options.push_back({"Playback Timer", playbackTimerLabel(), true, true, false, adjustPlaybackTimer});
}
