#include "module/shell/Options.h"

#include "core/AppRegistry.h"
#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"

bool optionsMenuVisible = false;

namespace
{
HostApp optionsOwner = HostApp::Music;
std::vector<AppOption> optionEntries;
int optionSelected = 0;

const AppDescriptor &optionsApp()
{
    return appDescriptor(optionsOwner);
}

void rebuildOptionEntries()
{
    optionEntries.clear();
    if (optionsApp().buildOptions != nullptr)
        optionsApp().buildOptions(optionEntries);

    if (optionEntries.empty())
        optionSelected = 0;
    else
        optionSelected = constrain(optionSelected, 0, (int)optionEntries.size() - 1);
}

ListModel buildOptionsListModel()
{
    ListModel model;
    model.selected = optionSelected;

    for (int i = 0; i < (int)optionEntries.size(); ++i)
    {
        const AppOption &entry = optionEntries[i];
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

bool selectedOptionSupportsAdjustment()
{
    return optionSelected >= 0 &&
           optionSelected < (int)optionEntries.size() &&
           optionEntries[optionSelected].adjustable;
}

void activateSelectedOption(int direction = 1)
{
    if (optionSelected < 0 || optionSelected >= (int)optionEntries.size())
        return;

    const AppOption entry = optionEntries[optionSelected];
    if (!entry.enabled || entry.activate == nullptr)
        return;

    if (entry.closeOnActivate)
        optionsMenuVisible = false;

    entry.activate(direction);
    if (entry.closeOnActivate)
        return;

    rebuildOptionEntries();
    drawOptionsMenu();
}
} // namespace

void drawOptionsMenu()
{
    rebuildOptionEntries();

    HeaderModel header;
    header.appHeaderTag = optionsApp().headerTag;
    header.appHeaderTitle = "Options";
    header.cursor = true;
    drawHeader(header);

    drawList(buildOptionsListModel());

    FooterModel footer;
    footer.left = "[Opt]Close";
    if (optionsApp().optionsShowRunHint)
        footer.left += " [Ok]Run";
    footer.left += " [,/]Change";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void enterOptionsMenu()
{
    optionsOwner = foregroundApp;
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
        if (optionsApp().optionsEnterEnabled)
            activateSelectedOption();
        return;
    }

    const int optionCount = (int)optionEntries.size();
    for (auto c : ks.word)
    {
        if (optionCount <= 0)
            return;

        if (c == ';')
        {
            const int oldSelected = optionSelected;
            optionSelected = (optionSelected - 1 + optionCount) % optionCount;
            drawListSelection(buildOptionsListModel(), oldSelected, optionSelected);
            return;
        }

        if (c == '.')
        {
            const int oldSelected = optionSelected;
            optionSelected = (optionSelected + 1) % optionCount;
            drawListSelection(buildOptionsListModel(), oldSelected, optionSelected);
            return;
        }

        if (c == '/')
        {
            if (selectedOptionSupportsAdjustment())
                activateSelectedOption(+1);
            return;
        }

        if (c == ',')
        {
            if (selectedOptionSupportsAdjustment())
                activateSelectedOption(-1);
            return;
        }
    }
}
