#include "module/Debug.h"

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/Help.h"
#include "module/Radio.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"

namespace
{
static int debugSelected = 0;
static int debugScrollTop = 0;

static void addDebugRow(
    ListModel &model,
    const String &label,
    const String &value)
{
    ListItemModel item;
    item.type = ListItemType::Property;
    item.label = label;
    item.value = value;
    item.isSelected = (int)model.items.size() == debugSelected;
    model.items.push_back(item);
}

static ListModel buildDebugListModel()
{
    ListModel model;
    model.selected = debugSelected;
    model.scrollTop = debugScrollTop;

    addDebugRow(model, "Mode", webRadioMode ? "Radio" : "Music");
    addDebugRow(model, "Theme", String(themeIdx));
    addDebugRow(model, "Heap Free", String(ESP.getFreeHeap() / 1024) + "K");
    addDebugRow(model, "Heap Min", String(ESP.getMinFreeHeap() / 1024) + "K");

    if (webRadioMode)
    {
        addDebugRow(model, "Codec", radioMp3 ? "MP3" : (aac ? "AAC" : "--"));
        if (radioBuf)
        {
            uint32_t fill = radioBuf->getFillLevel();
            int pct = (int)(fill * 100 / RADIO_HTTP_BUF);
            addDebugRow(
                model,
                "Net Buffer",
                String((unsigned long)(fill / 1024)) + "K/" +
                    String((int)(RADIO_HTTP_BUF / 1024)) + "K " +
                    String(pct) + "%");
        }
        else
        {
            addDebugRow(model, "Net Buffer", "--");
        }

        addDebugRow(model, "WiFi", wifiConnected ? String(WiFi.RSSI()) + " dBm" : "Offline");
    }
    else
    {
        addDebugRow(model, "Codec", mp3 ? "MP3" : (aac ? "AAC/M4A" : "--"));
        addDebugRow(model, "State", isPlaying ? "Play" : (isPaused ? "Pause" : "Stop"));
        addDebugRow(model, "Items", String((int)items.size()));
    }

    unsigned long up = millis() / 1000;
    char upStr[12];
    snprintf(upStr, sizeof(upStr), "%02lu:%02lu:%02lu", up / 3600, (up / 60) % 60, up % 60);
    addDebugRow(model, "Uptime", String(upStr));
    addDebugRow(model, "Battery", batteryLevel >= 0 ? String(batteryLevel) + "%" : "--");

    return model;
}

static void clampDebugSelection()
{
    ListModel model = buildDebugListModel();
    int count = model.items.size();
    if (count <= 0)
    {
        debugSelected = 0;
        debugScrollTop = 0;
        return;
    }

    debugSelected = max(0, min(debugSelected, count - 1));
    if (debugSelected < debugScrollTop)
        debugScrollTop = debugSelected;
    if (debugSelected >= debugScrollTop + LIST_VISIBLE_ITEM)
        debugScrollTop = debugSelected - LIST_VISIBLE_ITEM + 1;
    debugScrollTop = max(0, debugScrollTop);
}

static void drawDebugHeader()
{
    HeaderModel model;
    model.mode = "DEBUG";
    model.title = "SYSTEM";
    model.cursor = true;
    drawHeader(model);
}

static void drawDebugFooter()
{
    FooterModel model;
    model.left = "[;/.]Sel [Esc]Close";
    model.center = "";
    model.battery = footerBatteryText();
    drawFooter(model);
}

static void redrawDebugSelection(
    int oldSelected,
    int oldScrollTop)
{
    clampDebugSelection();

    if (debugScrollTop != oldScrollTop)
    {
        ListModel list =
            buildDebugListModel();
        drawList(list);
        return;
    }

    ListModel list =
        buildDebugListModel();

    drawListSelection(
        list,
        oldSelected,
        debugSelected);
}
} // namespace

void drawDebug()
{
    clampDebugSelection();

    ListModel list = buildDebugListModel();
    drawDebugHeader();
    drawList(list);
    drawDebugFooter();
}

void toggleDebug()
{
    debugOverlayVisible = !debugOverlayVisible;
    if (debugOverlayVisible)
    {
        debugSelected = 0;
        debugScrollTop = 0;
        drawDebug();
    }
    else
    {
        drawAll();
    }
}

void handleDebugInput(Keyboard_Class::KeysState &ks)
{
    if (keyboardBackPressed(ks))
    {
        toggleDebug();
        return;
    }

    for (auto c : ks.word)
    {
        switch (c)
        {
        case 'd':
        case 'D':
            toggleDebug();
            return;

        case 'h':
        case 'H':
            toggleHelp();
            return;

        case ';':
        {
            int count = buildDebugListModel().items.size();
            int oldSelected =
                debugSelected;
            int oldScrollTop =
                debugScrollTop;

            debugSelected =
                (debugSelected - 1 + count) %
                count;
            redrawDebugSelection(
                oldSelected,
                oldScrollTop);
            return;
        }

        case '.':
        {
            int count = buildDebugListModel().items.size();
            int oldSelected =
                debugSelected;
            int oldScrollTop =
                debugScrollTop;

            debugSelected =
                (debugSelected + 1) %
                count;
            redrawDebugSelection(
                oldSelected,
                oldScrollTop);
            return;
        }
        }
    }
}
