#include "module/shell/Help.h"

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/programs/Calculator.h"
#include "module/shell/Debug.h"
#include "module/programs/Notes.h"
#include "module/programs/Radio.h"
#include "module/shell/Settings.h"
#include "module/service/WiFi.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"

namespace
{
struct HelpRow
{
    const char *key;
    const char *desc;
};

enum class HelpContext
{
    Music,
    Radio,
    Notes,
    Wifi,
    Settings,
    Debug,
    Calculator
};

static int helpSelected = 0;
static int helpScrollTop = 0;
static HelpContext helpContext = HelpContext::Music;

static const HelpRow MUSIC_HELP[] = {
    {"[Ok]", "Open / play"},
    {"[Del]", "Parent folder"},
    {"[Space]", "Pause / resume"},
    {"[;/.]", "Cursor up / down"},
    {"[,/]", "Seek back / forward"},
    {"[+/-]", "Volume"},
    {"[W]", "Switch to radio"},
    {"[N]", "Quick note"},
    {"[Opt]", "Toggle Options"},
    {"[Alt]", "Toggle Applications"},
    {"[Ctrl]", "Toggle Control Panel"},
    {"[C]", "Calculator"},
    {"[D]", "Debug"},
    {"[O]", "Screen on / off"},
    {"[1-5]", "Theme"},
    {"[H]", "Close help"},
};

static const HelpRow RADIO_HELP[] = {
    {"[Ok/Spc]", "Play / stop stream"},
    {"[;/.]", "Cursor up / down"},
    {"[A]", "Add station"},
    {"[X]", "Remove station"},
    {"[R]", "Reconnect"},
    {"[I]", "Force AAC"},
    {"[+/-]", "Volume"},
    {"[W/Esc]", "Back to music"},
    {"[N]", "Quick note"},
    {"[Opt]", "Toggle Options"},
    {"[Alt]", "Toggle Applications"},
    {"[Ctrl]", "Toggle Control Panel"},
    {"[C]", "Calculator"},
    {"[D]", "Debug"},
    {"[O]", "Screen on / off"},
    {"[1-5]", "Theme"},
    {"[H]", "Close help"},
};

static const HelpRow NOTES_HELP[] = {
    {"[Opt]", "Toggle Options"},
    {"[Alt]", "Toggle Applications"},
    {"[Ctrl]", "Toggle Control Panel"},
    {"[A]", "Add note"},
    {"[R]", "Remove note"},
    {"[X]", "Toggle done"},
    {"[Ok]", "Edit note"},
    {"[;/.]", "Cursor up / down"},
    {"[Tab]", "Switch editor field"},
    {"[Fn arrows]", "Cursor / editor date"},
    {"[T]", "Today"},
    {"[U/B]", "Top / bottom"},
    {"[Esc]", "Back to player"},
    {"[N]", "Close notes"},
    {"[H]", "Close help"},
};

static const HelpRow WIFI_HELP[] = {
    {"[Ok]", "Connect"},
    {"[R]", "Refresh scan"},
    {"[;/.]", "Cursor up / down"},
    {"[Esc]", "Back"},
    {"[Del]", "Back"},
    {"[H]", "Close help"},
};

static const HelpRow SETTINGS_HELP[] = {
    {"[;/.]", "Cursor up / down"},
    {"[+/-]", "Change value"},
    {"[Ok]", "Open / apply"},
    {"[Ctrl]", "Close Control Panel"},
    {"[Esc]", "Close Control Panel"},
    {"[H]", "Close help"},
};

static const HelpRow DEBUG_HELP[] = {
    {"[;/.]", "Cursor up / down"},
    {"[D]", "Close debug"},
    {"[Esc]", "Close debug"},
    {"[H]", "Close help"},
};

static const HelpRow CALC_HELP[] = {
    {"[Opt]", "Toggle Options"},
    {"[0-9/.]", "Enter number"},
    {"[A/+]", "Add"},
    {"[S/-]", "Subtract"},
    {"[M]", "Add multiply row"},
    {"[X/*]", "Multiply"},
    {"[D//]", "Divide"},
    {"[Ok]", "Calculate"},
    {"[H]", "Toggle history"},
    {"[Del]", "Backspace"},
    {"[Esc]", "Clear / close"},
    {"[C]", "Close calculator"},
};

static HelpContext resolveHelpContext()
{
    if (calculatorVisible)
        return HelpContext::Calculator;
    if (debugOverlayVisible)
        return HelpContext::Debug;
    if (settingsMenuVisible)
        return HelpContext::Settings;
    if (notesMode)
        return HelpContext::Notes;
    if (wifiMenuVisible || wifiPassOverlayVisible)
        return HelpContext::Wifi;
    if (webRadioMode)
        return HelpContext::Radio;

    return HelpContext::Music;
}

static const HelpRow *helpRows(int &count)
{
    switch (helpContext)
    {
    case HelpContext::Radio:
        count = (int)(sizeof(RADIO_HELP) / sizeof(RADIO_HELP[0]));
        return RADIO_HELP;
    case HelpContext::Notes:
        count = (int)(sizeof(NOTES_HELP) / sizeof(NOTES_HELP[0]));
        return NOTES_HELP;
    case HelpContext::Wifi:
        count = (int)(sizeof(WIFI_HELP) / sizeof(WIFI_HELP[0]));
        return WIFI_HELP;
    case HelpContext::Settings:
        count = (int)(sizeof(SETTINGS_HELP) / sizeof(SETTINGS_HELP[0]));
        return SETTINGS_HELP;
    case HelpContext::Debug:
        count = (int)(sizeof(DEBUG_HELP) / sizeof(DEBUG_HELP[0]));
        return DEBUG_HELP;
    case HelpContext::Calculator:
        count = (int)(sizeof(CALC_HELP) / sizeof(CALC_HELP[0]));
        return CALC_HELP;
    case HelpContext::Music:
    default:
        count = (int)(sizeof(MUSIC_HELP) / sizeof(MUSIC_HELP[0]));
        return MUSIC_HELP;
    }
}

static int helpRowCount()
{
    int count = 0;
    helpRows(count);
    return count;
}

static const HelpRow &helpRowAt(int index)
{
    int count = 0;
    const HelpRow *rows =
        helpRows(count);
    return rows[index];
}

static const char *helpContextTitle()
{
    switch (helpContext)
    {
    case HelpContext::Radio:
        return "RADIO KEYS";
    case HelpContext::Notes:
        return "NOTES KEYS";
    case HelpContext::Wifi:
        return "WIFI KEYS";
    case HelpContext::Settings:
        return "CONTROL PANEL KEYS";
    case HelpContext::Debug:
        return "DEBUG KEYS";
    case HelpContext::Calculator:
        return "CALC KEYS";
    case HelpContext::Music:
    default:
        return "MUSIC KEYS";
    }
}

static void restoreHelpHost()
{
    switch (helpContext)
    {
    case HelpContext::Calculator:
        drawCalculator();
        break;
    case HelpContext::Debug:
        drawDebug();
        break;
    case HelpContext::Settings:
        drawSettingsMenu();
        break;
    case HelpContext::Notes:
        drawNotes();
        break;
    case HelpContext::Wifi:
        drawWifiMenu();
        break;
    case HelpContext::Radio:
        drawRadioAll();
        break;
    case HelpContext::Music:
    default:
        drawAll();
        break;
    }
}

static void clampHelpSelection()
{
    int count = helpRowCount();
    if (count <= 0)
    {
        helpSelected = 0;
        helpScrollTop = 0;
        return;
    }

    helpSelected = max(0, min(helpSelected, count - 1));
    if (helpSelected < helpScrollTop)
        helpScrollTop = helpSelected;
    if (helpSelected >= helpScrollTop + LIST_VISIBLE_ITEM)
        helpScrollTop = helpSelected - LIST_VISIBLE_ITEM + 1;
    helpScrollTop = max(0, helpScrollTop);
}

static void drawHelpHeader()
{
    HeaderModel model;
    model.appHeaderTag = "HELP";
    model.appHeaderTitle = helpContextTitle();
    model.cursor = true;
    drawHeader(model);
}

static void drawHelpList()
{
    ListModel model;
    model.selected = helpSelected;
    model.scrollTop = helpScrollTop;

    int count = helpRowCount();
    for (int i = 0; i < count; i++)
    {
        const HelpRow &row = helpRowAt(i);
        ListItemModel item;
        item.type = ListItemType::Property;
        item.label = row.desc;
        item.value = row.key;
        item.isSelected = i == helpSelected;
        model.items.push_back(item);
    }

    drawList(model);
}

static ListModel buildHelpListModel()
{
    ListModel model;
    model.selected = helpSelected;
    model.scrollTop = helpScrollTop;

    int count = helpRowCount();
    for (int i = 0; i < count; i++)
    {
        const HelpRow &row = helpRowAt(i);
        ListItemModel item;
        item.type = ListItemType::Property;
        item.label = row.desc;
        item.value = row.key;
        item.isSelected = i == helpSelected;
        model.items.push_back(item);
    }

    return model;
}

static void redrawHelpSelection(
    int oldSelected,
    int oldScrollTop)
{
    clampHelpSelection();

    if (helpScrollTop != oldScrollTop)
    {
        drawHelpList();
        return;
    }

    ListModel model =
        buildHelpListModel();

    drawListSelection(
        model,
        oldSelected,
        helpSelected);
}

static void drawHelpFooter()
{
    FooterModel model;
    model.left = "[;/.]Sel [Esc]Close";
    model.center = "";
    model.battery = footerBatteryText();
    drawFooter(model);
}
} // namespace

void toggleHelp()
{
    helpVisible = !helpVisible;
    if (helpVisible)
    {
        helpContext = resolveHelpContext();
        helpSelected = 0;
        helpScrollTop = 0;
        drawHelp();
    }
    else
    {
        restoreHelpHost();
    }
}

void drawHelp()
{
    clampHelpSelection();
    drawHelpHeader();
    drawHelpList();
    drawHelpFooter();
}

void handleHelpInput(Keyboard_Class::KeysState &ks)
{
    if (keyboardBackPressed(ks))
    {
        toggleHelp();
        return;
    }

    for (auto c : ks.word)
    {
        switch (c)
        {
        case 'h':
        case 'H':
            toggleHelp();
            return;

        case ';':
        {
            int oldSelected =
                helpSelected;
            int oldScrollTop =
                helpScrollTop;

            helpSelected =
                (helpSelected - 1 + helpRowCount()) %
                helpRowCount();
            redrawHelpSelection(
                oldSelected,
                oldScrollTop);
            return;
        }

        case '.':
        {
            int oldSelected =
                helpSelected;
            int oldScrollTop =
                helpScrollTop;

            helpSelected =
                (helpSelected + 1) %
                helpRowCount();
            redrawHelpSelection(
                oldSelected,
                oldScrollTop);
            return;
        }
        }
    }
}
