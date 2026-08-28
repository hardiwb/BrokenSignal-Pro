#include "apps/radio/RadioMetadata.h"

#include "apps/radio/Radio.h"
#include "module/service/WiFi.h"

const HelpEntry RADIO_HELP_ENTRIES[] = {
    {"[Ok/Spc]", "Play / stop stream"},
    {"[;/.]", "Cursor up / down"},
    {"[A]", "Add station"},
    {"[X]", "Remove station"},
    {"[R]", "Reconnect"},
    {"[I]", "Force AAC"},
    {"[-/+]", "Volume"},
    {"[W]", "Back to music"},
    {"[Esc]", "Applications"},
    {"[N]", "Quick note"},
    {"[Opt]", "Toggle Options"},
    {"[Alt]", "Toggle Applications"},
    {"[Ctrl]", "Toggle Control Panel"},
    {"[C]", "Calculator"},
    {"[O]", "Screen on / off"},
    {"[H]", "Close help"},
};

const uint8_t RADIO_HELP_COUNT =
    sizeof(RADIO_HELP_ENTRIES) / sizeof(RADIO_HELP_ENTRIES[0]);

namespace
{
String playbackTimerLabel()
{
    if (playbackOffSec == 0)
        return "Off";
    if (playbackOffSec < 3600)
        return String(playbackOffSec / 60) + "m";
    return String(playbackOffSec / 3600) + "h";
}

String wifiLabel()
{
    return wifiConnected && wifiSSID.length() > 0 ? wifiSSID : "Off";
}

void newRadio(int)
{
    showAddUrlOverlay();
}

void deleteRadio(int)
{
    showRemoveConfirm();
}

void adjustForceAac(int)
{
    toggleRadioForceAac();
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
    settingsDirty = true;
    settingsDirtyMs = millis();
}

void openWifi(int)
{
    openWifiMenu();
}
} // namespace

void buildRadioOptions(std::vector<AppOption> &options)
{
    options.push_back({"New Radio", "", radioCount < RADIO_MAX, false, true, newRadio});
    options.push_back({"Delete Radio", "", radioCount > 0, false, true, deleteRadio});
    options.push_back({"Force AAC", radioForceAac ? "On" : "Off", true, true, false, adjustForceAac});
    options.push_back({"Playback Timer", playbackTimerLabel(), true, true, false, adjustPlaybackTimer});
    options.push_back({"WiFi", wifiLabel(), true, false, true, openWifi});
}
