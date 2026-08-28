#include "apps/music/MusicBrowser.h"

#include "module/service/FileBrowser.h"
#include "apps/music/MusicPlayer.h"
#include "core/State.h"
#include "core/System.h"

namespace
{
bool musicAcceptFile(const String &name, const String &)
{
    String lo = name;
    lo.toLowerCase();
    return lo.endsWith(".mp3") || lo.endsWith(".m4a");
}

String musicDisplayLabel(const String &name, const String &)
{
    int dot = name.lastIndexOf('.');
    String label = (dot > 0) ? name.substring(0, dot) : name;
    label.replace('_', ' ');
    return label;
}

unsigned long musicReadDuration(const String &path, size_t fileSize)
{
    String lo = path;
    lo.toLowerCase();
    if (lo.endsWith(".m4a"))
        return readM4ADuration(path.c_str());
    return readMP3Duration(path.c_str(), fileSize);
}

void musicOpenFile(int itemIndex, const BrowserItem &)
{
    if ((isPlaying || isPaused) && itemIndex == currentTrack)
        stopAudio();
    else
        startTrack(itemIndex);
}

void musicStopActive()
{
    stopAudio();
}

void musicSeek(int deltaMs)
{
    seekTrack(deltaMs);
}

void musicSelectionChanged(int oldSelected)
{
    drawPlayerSelection(oldSelected);
}

void musicRedraw()
{
    drawAll();
}

void musicClearActiveItem()
{
    currentTrack = -1;
}

bool musicPlaybackActive()
{
    return isPlaying || isPaused;
}

bool musicItemActive(int itemIndex, const BrowserItem &)
{
    return currentTrack == itemIndex;
}
} // namespace

void useMusicFileBrowser()
{
    FileBrowserConfig config = {
        "/Music",
        "Music",
        "/recent.txt",
        "RECENT",
        true,
        musicAcceptFile,
        musicDisplayLabel,
        musicReadDuration,
        musicOpenFile,
        musicStopActive,
        musicSeek,
        musicSelectionChanged,
        musicRedraw,
        musicClearActiveItem,
        musicPlaybackActive,
        musicItemActive};
    configureFileBrowser(config);
}
