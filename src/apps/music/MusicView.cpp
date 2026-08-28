#include "apps/music/MusicPlayer.h"

#include "core/State.h"
#include "core/System.h"
#include "core/Utils.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"

#include <algorithm>
#include <climits>

namespace
{
int playerMarqueeSelected = -1;
unsigned long playerMarqueeStartMs = 0;
unsigned long playerStatusLastShownSecond = ULONG_MAX;

unsigned long currentPlayerMarqueeStartMs()
{
    if (playerMarqueeSelected != selectedItem)
    {
        playerMarqueeSelected = selectedItem;
        playerMarqueeStartMs = millis();
    }

    return playerMarqueeStartMs;
}

int getPlayerListTopForSelection(int selection)
{
    if (items.empty())
        return 0;

    int top = selection - LIST_VISIBLE_ITEM / 2;
    top = max(0, min(top, (int)items.size() - LIST_VISIBLE_ITEM));
    return max(0, top);
}

int getPlayerListTop()
{
    return getPlayerListTopForSelection(selectedItem);
}

void populatePlayerListModel(ListModel &model)
{
    for (int i = 0; i < (int)items.size(); i++)
    {
        ListItemModel item;
        item.label = items[i].label;
        item.isSelected = i == selectedItem;
        item.isActive = !items[i].isFolder && i == currentTrack;
        item.durationMs = items[i].durationMs;

        if (items[i].isFolder)
            item.type = ListItemType::Folder;

        if (items[i].path == "__PREV__" || items[i].path == "__MORE__")
        {
            item.type = ListItemType::Normal;
            item.label = items[i].label;
            item.durationMs = 0;
        }

        model.items.push_back(item);
    }
}
} // namespace

void drawPlayerHeader()
{
    String name;
    if (currentTrack >= 0 && currentTrack < (int)items.size())
        name = shortName(items[currentTrack].path);
    else if (viewFolder == "**RECENT**")
        name = "RECENT";
    else if (viewFolder != "/Music")
        name = folderName(viewFolder);
    else
        name = "---";

    String mode;
    if (!isPlaying && !isPaused && currentFolderIdx != 0)
        mode = isRecentView ? "RECENT" : folderName(viewFolder);
    else
        mode = isPlaying ? "PLAYING" : (isPaused ? "PAUSED" : "STOPPED");

    HeaderModel model;
    model.appHeaderTag = mode;
    model.appHeaderTitle = name;
    model.cursor = cursorVisible;

    drawHeader(model);

}

void drawPlayerList()
{
    if (items.empty())
    {
        ListModel model;
        drawList(model);
        return;
    }

    ListModel model;
    model.selected = selectedItem;
    model.scrollTop = getPlayerListTop();
    model.marqueeStartMs = currentPlayerMarqueeStartMs();
    populatePlayerListModel(model);

    drawList(model);
}

void drawPlayerRow(int idx)
{
    if (items.empty())
        return;

    ListModel model;
    model.selected = selectedItem;
    model.scrollTop = getPlayerListTop();
    model.marqueeStartMs = currentPlayerMarqueeStartMs();
    populatePlayerListModel(model);

    drawListRow(model, idx);
}

void drawPlayerSelection(int oldSelected)
{
    if (items.empty())
        return;

    int oldTop = getPlayerListTopForSelection(oldSelected);
    int newTop = getPlayerListTop();

    if (oldTop != newTop)
    {
        drawPlayerList();
        return;
    }

    ListModel model;
    model.selected = selectedItem;
    model.scrollTop = newTop;
    model.marqueeStartMs = currentPlayerMarqueeStartMs();
    populatePlayerListModel(model);

    drawListSelection(
        model,
        oldSelected,
        selectedItem);
}

void drawPlayerStatus()
{
    unsigned long elapsed =
        pausedElapsedMs +
        (isPlaying ? millis() - trackStartMs : 0UL);

    if (elapsed > trackDurationMs && trackDurationMs > 0)
        elapsed = trackDurationMs;

    float progress =
        (trackDurationMs > 0)
            ? min(1.0f, (float)elapsed / (float)trackDurationMs)
            : 0.0f;

    FooterModel model;
    model.left = "POS> " + formatTime(elapsed) + "/" + formatTime(trackDurationMs);
    model.battery = footerBatteryText();
    model.showProgress = true;
    model.progress = progress;

    drawFooter(model);
    playerStatusLastShownSecond = elapsed / 1000;
}

void updatePlayerStatus()
{
    unsigned long elapsed =
        pausedElapsedMs +
        (isPlaying ? millis() - trackStartMs : 0UL);

    if (elapsed > trackDurationMs && trackDurationMs > 0)
        elapsed = trackDurationMs;

    unsigned long shownSecond = elapsed / 1000;

    if (shownSecond != playerStatusLastShownSecond)
    {
        playerStatusLastShownSecond = shownSecond;
        drawFooterLeft(
            "POS> " +
            formatTime(elapsed) +
            "/" +
            formatTime(trackDurationMs));
    }

    float progress =
        (trackDurationMs > 0)
            ? min(1.0f, (float)elapsed / (float)trackDurationMs)
            : 0.0f;

    drawFooterProgress(progress);
}
