#include "module/shell/Options.h"

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/programs/Calculator.h"
#include "module/programs/Notes.h"
#include "module/programs/Player.h"
#include "module/programs/Radio.h"
#include "module/service/WiFi.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"

bool optionsMenuVisible = false;

namespace
{
enum class OptionsContext
{
    Player,
    Radio,
    Notes,
    Calculator
};

enum class OptionAction
{
    Shuffle,
    Repeat,
    Volume,
    SeekStep,
    NotesFilter,
    MoveNoteTomorrow,
    MoveNoteToDate,
    EditNote,
    DeleteNote,
    NewNote,
    NewRadio,
    DeleteRadio,
    ForceAac,
    PlaybackTimer,
    Wifi,
    CalculatorDecimals,
    CalculatorRounding,
    CalculatorThousands
};

struct OptionEntry
{
    OptionAction action;
    const char *label;
    String value;
    bool enabled;

    OptionEntry(
        OptionAction actionValue,
        const char *labelValue,
        const String &valueValue,
        bool enabledValue = true)
        : action(actionValue),
          label(labelValue),
          value(valueValue),
          enabled(enabledValue)
    {
    }
};

OptionsContext optionsContext = OptionsContext::Player;
std::vector<OptionEntry> optionEntries;
int optionSelected = 0;

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

String radioWifiLabel()
{
    if (wifiConnected && wifiSSID.length() > 0)
        return wifiSSID;
    return "Off";
}

String calculatorDecimalLabel()
{
    return calculatorDecimalPlaces < 0
               ? "Auto"
               : String(calculatorDecimalPlaces);
}

String calculatorRoundingLabel()
{
    static const char *labels[] = {
        "Half Up",
        "Half Even",
        "Truncate"};
    return labels[min((int)calculatorRoundingMode, 2)];
}

void rebuildOptionEntries()
{
    optionEntries.clear();

    switch (optionsContext)
    {
    case OptionsContext::Player:
        optionEntries.push_back({OptionAction::Shuffle, "Shuffle", shuffleOn ? "On" : "Off"});
        optionEntries.push_back({OptionAction::Repeat, "Repeat", repeatLabel()});
        optionEntries.push_back({OptionAction::Volume, "Volume", volumeLabel()});
        optionEntries.push_back({OptionAction::SeekStep, "Seek Step", String(seekSeconds) + "s"});
        optionEntries.push_back({OptionAction::PlaybackTimer, "Playback Timer", playbackTimerLabel()});
        break;

    case OptionsContext::Radio:
        optionEntries.push_back({OptionAction::NewRadio, "New Radio", "", radioCount < RADIO_MAX});
        optionEntries.push_back({OptionAction::DeleteRadio, "Delete Radio", "", radioCount > 0});
        optionEntries.push_back({OptionAction::ForceAac, "Force AAC", radioForceAac ? "On" : "Off"});
        optionEntries.push_back({OptionAction::PlaybackTimer, "Playback Timer", playbackTimerLabel()});
        optionEntries.push_back({OptionAction::Wifi, "WiFi", radioWifiLabel()});
        break;

    case OptionsContext::Notes:
    {
        const bool hasNote = notesHasSelection();
        optionEntries.push_back({OptionAction::NotesFilter, "Filter", notesFilterLabel()});
        optionEntries.push_back({OptionAction::MoveNoteTomorrow, "Move to Tomorrow", "", hasNote});
        optionEntries.push_back({OptionAction::MoveNoteToDate, "Move to Date", "", hasNote});
        optionEntries.push_back({OptionAction::EditNote, "Edit Note", "", hasNote});
        optionEntries.push_back({OptionAction::DeleteNote, "Delete Note", "", hasNote});
        optionEntries.push_back({OptionAction::NewNote, "New Note", ""});
        break;
    }

    case OptionsContext::Calculator:
        optionEntries.push_back({
            OptionAction::CalculatorDecimals,
            "Decimal Places",
            calculatorDecimalLabel()});
        optionEntries.push_back({
            OptionAction::CalculatorRounding,
            "Rounding",
            calculatorRoundingLabel()});
        optionEntries.push_back({
            OptionAction::CalculatorThousands,
            "Thousands",
            calculatorThousandsSeparator ? "On" : "Off"});
        break;

    }

    optionSelected = min(optionSelected, (int)optionEntries.size() - 1);
}

ListModel buildOptionsListModel()
{
    ListModel model;
    model.selected = optionSelected;

    for (int i = 0; i < (int)optionEntries.size(); ++i)
    {
        const OptionEntry &entry = optionEntries[i];
        ListItemModel item;
        item.type = entry.value.length() > 0 ? ListItemType::Property : ListItemType::Normal;
        item.label = entry.label;
        item.value = entry.value;
        item.isSelected = i == optionSelected;
        item.isDimmed = !entry.enabled;
        model.items.push_back(item);
    }

    return model;
}

void adjustSeekStep(int direction)
{
    static const uint8_t steps[] = {5, 10, 15, 20, 30, 45, 60};
    const int count = sizeof(steps) / sizeof(steps[0]);
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
    settingsDirty = true;
    settingsDirtyMs = millis();
}

void adjustPlaybackTimer(int direction)
{
    static const uint32_t timers[] = {0, 1800, 3600, 7200, 10800};
    const int count = sizeof(timers) / sizeof(timers[0]);
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

void adjustVolume(int direction)
{
    volume = (uint8_t)constrain((int)volume + direction * 10, 0, 255);
    M5Cardputer.Speaker.setVolume(volume);
    settingsDirty = true;
    settingsDirtyMs = millis();
}

void markCalculatorSettingsChanged()
{
    refreshCalculatorFormatting();
    settingsDirty = true;
    settingsDirtyMs = millis();
}

void adjustCalculatorDecimals(int direction)
{
    static const int8_t places[] = {-1, 0, 2, 4, 6};
    constexpr int count = sizeof(places) / sizeof(places[0]);
    int index = 0;
    for (int i = 0; i < count; ++i)
    {
        if (places[i] == calculatorDecimalPlaces)
        {
            index = i;
            break;
        }
    }
    calculatorDecimalPlaces = places[(index + direction + count) % count];
    markCalculatorSettingsChanged();
}

bool selectedOptionSupportsAdjustment()
{
    if (optionSelected < 0 || optionSelected >= (int)optionEntries.size())
        return false;

    switch (optionEntries[optionSelected].action)
    {
    case OptionAction::Shuffle:
    case OptionAction::Repeat:
    case OptionAction::Volume:
    case OptionAction::SeekStep:
    case OptionAction::NotesFilter:
    case OptionAction::ForceAac:
    case OptionAction::PlaybackTimer:
    case OptionAction::CalculatorDecimals:
    case OptionAction::CalculatorRounding:
    case OptionAction::CalculatorThousands:
        return true;
    default:
        return false;
    }
}

void activateSelectedOption(int direction = 1)
{
    if (optionSelected < 0 || optionSelected >= (int)optionEntries.size())
        return;

    const OptionEntry &entry = optionEntries[optionSelected];
    if (!entry.enabled)
        return;

    switch (entry.action)
    {
    case OptionAction::Shuffle:
        toggleShuffle();
        break;
    case OptionAction::Repeat:
        repeatMode = (repeatMode + direction + 3) % 3;
        settingsDirty = true;
        settingsDirtyMs = millis();
        break;
    case OptionAction::Volume:
        adjustVolume(direction);
        break;
    case OptionAction::SeekStep:
        adjustSeekStep(direction);
        break;
    case OptionAction::NotesFilter:
        notesAdjustFilter(direction);
        break;
    case OptionAction::MoveNoteTomorrow:
        optionsMenuVisible = false;
        notesMoveSelectedToTomorrow();
        return;
    case OptionAction::MoveNoteToDate:
        optionsMenuVisible = false;
        notesPromptMoveSelectedToDate();
        return;
    case OptionAction::EditNote:
        optionsMenuVisible = false;
        notesEditSelected();
        return;
    case OptionAction::DeleteNote:
        optionsMenuVisible = false;
        notesDeleteSelected();
        return;
    case OptionAction::NewNote:
        optionsMenuVisible = false;
        notesNew();
        return;
    case OptionAction::NewRadio:
        optionsMenuVisible = false;
        showAddUrlOverlay();
        return;
    case OptionAction::DeleteRadio:
        optionsMenuVisible = false;
        showRemoveConfirm();
        return;
    case OptionAction::ForceAac:
        toggleRadioForceAac();
        break;
    case OptionAction::PlaybackTimer:
        adjustPlaybackTimer(direction);
        break;
    case OptionAction::Wifi:
        optionsMenuVisible = false;
        openWifiMenu();
        return;
    case OptionAction::CalculatorDecimals:
        adjustCalculatorDecimals(direction);
        break;
    case OptionAction::CalculatorRounding:
        calculatorRoundingMode =
            (calculatorRoundingMode + direction + 3) % 3;
        markCalculatorSettingsChanged();
        break;
    case OptionAction::CalculatorThousands:
        calculatorThousandsSeparator = !calculatorThousandsSeparator;
        markCalculatorSettingsChanged();
        break;
    }

    rebuildOptionEntries();
    drawOptionsMenu();
}
} // namespace

void drawOptionsMenu()
{
    rebuildOptionEntries();

    HeaderModel header;
    switch (optionsContext)
    {
    case OptionsContext::Player:
        header.appHeaderTag = "MUSIC";
        break;
    case OptionsContext::Radio:
        header.appHeaderTag = "RADIO";
        break;
    case OptionsContext::Notes:
        header.appHeaderTag = "NOTES";
        break;
    case OptionsContext::Calculator:
        header.appHeaderTag = "CALCULATOR";
        break;
    }
    header.appHeaderTitle = "Options";
    header.cursor = true;
    drawHeader(header);

    drawList(buildOptionsListModel());

    FooterModel footer;
    if (optionsContext == OptionsContext::Radio)
    {
        footer.left = "[Opt]Close [Ok]Run";
        footer.center = "[+/-]Change";
    }
    else if (optionsContext == OptionsContext::Notes)
    {
        footer.left = "[Opt]Close [Ok]Run";
        footer.center = "[+/-]Change";
    }
    else if (optionsContext == OptionsContext::Player ||
             optionsContext == OptionsContext::Calculator)
    {
        footer.left = "[Opt]Close";
        footer.center = "[+/-]Change";
    }
    else
    {
        footer.left = "[Opt]Close [Ok]Select";
    }
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void enterOptionsMenu()
{
    if (calculatorInputActive())
        optionsContext = OptionsContext::Calculator;
    else if (notesMode)
        optionsContext = OptionsContext::Notes;
    else if (webRadioMode)
        optionsContext = OptionsContext::Radio;
    else
        optionsContext = OptionsContext::Player;

    optionsMenuVisible = true;
    optionSelected = 0;
    drawOptionsMenu();
}

void exitOptionsMenu()
{
    optionsMenuVisible = false;
    drawAll();
}

void handleOptionsInput(Keyboard_Class::KeysState &ks)
{
    if ((ks.opt && ks.word.empty()) || keyboardBackPressed(ks))
    {
        exitOptionsMenu();
        return;
    }

    if (ks.enter)
    {
        if (optionsContext != OptionsContext::Player)
            activateSelectedOption();
        return;
    }

    const int optionCount = (int)optionEntries.size();
    for (auto c : ks.word)
    {
        if (c == ';')
        {
            int oldSelected = optionSelected;
            optionSelected = (optionSelected - 1 + optionCount) % optionCount;
            drawListSelection(buildOptionsListModel(), oldSelected, optionSelected);
            return;
        }

        if (c == '.')
        {
            int oldSelected = optionSelected;
            optionSelected = (optionSelected + 1) % optionCount;
            drawListSelection(buildOptionsListModel(), oldSelected, optionSelected);
            return;
        }

        if (c == '+' || c == '=')
        {
            if (selectedOptionSupportsAdjustment())
                activateSelectedOption(+1);
            return;
        }

        if (c == '-')
        {
            if (selectedOptionSupportsAdjustment())
                activateSelectedOption(-1);
            return;
        }
    }
}
