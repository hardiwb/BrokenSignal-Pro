#include "apps/mediacontrol/MediaControlMetadata.h"

#include "apps/mediacontrol/MediaControl.h"
#include "module/service/Bluetooth.h"

const HelpEntry MEDIA_CONTROL_HELP_ENTRIES[] = {
    {"[Ok]", "Connect / run selected"},
    {"[Space]", "Play or pause"},
    {"[,//]", "Previous / next track"},
    {"[-/=]", "Remote volume down / up"},
    {"[0]", "Mute remote audio"},
    {"[S]", "Stop playback"},
    {"[;/.]", "Cursor up / down"},
    {"[BtnG0]", "Disconnect media control"},
    {"[Fn+V]", "Open Media Control"},
};

const uint8_t MEDIA_CONTROL_HELP_COUNT =
    sizeof(MEDIA_CONTROL_HELP_ENTRIES) /
    sizeof(MEDIA_CONTROL_HELP_ENTRIES[0]);

namespace
{
void disconnect(int) { mediaControlDisconnect(); }
}

void buildMediaControlOptions(std::vector<AppOption> &options)
{
    options.push_back({
        "Disconnect", "", BluetoothService::keyboardSessionActive(),
        false, true, disconnect});
}
