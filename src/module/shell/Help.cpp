#include "module/shell/Help.h"

#include "core/AppRegistry.h"
#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"

namespace
{
int helpSelected = 0;
int helpScrollTop = 0;
HostApp helpOwner = HostApp::Music;

const AppDescriptor &helpApp()
{
    return appDescriptor(helpOwner);
}

int helpRowCount()
{
    return helpApp().helpCount;
}

const HelpEntry &helpRowAt(int index)
{
    return helpApp().helpEntries[index];
}

void clampHelpSelection()
{
    const int count = helpRowCount();
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

void drawHelpHeader()
{
    HeaderModel model;
    model.appHeaderTag = "HELP";
    model.appHeaderTitle = helpApp().helpTitle;
    model.cursor = true;
    drawHeader(model);
}

ListModel buildHelpListModel()
{
    ListModel model;
    model.selected = helpSelected;
    model.scrollTop = helpScrollTop;

    const int count = helpRowCount();
    for (int i = 0; i < count; ++i)
    {
        const HelpEntry &row = helpRowAt(i);
        ListItemModel item;
        item.type = ListItemType::Property;
        item.label = row.description;
        if (item.label == "Toggle Applications")
            item.value = "[" + String(keyboardApplicationsShortcutLabel()) + "]";
        else if (item.label == "Toggle Options")
            item.value = "[" + String(keyboardOptionsShortcutLabel()) + "]";
        else
            item.value = row.key;
        item.isSelected = i == helpSelected;
        model.items.push_back(item);
    }

    return model;
}

void drawHelpList()
{
    drawList(buildHelpListModel());
}

void redrawHelpSelection(int oldSelected, int oldScrollTop)
{
    clampHelpSelection();
    if (helpScrollTop != oldScrollTop)
    {
        drawHelpList();
        return;
    }

    drawListSelection(buildHelpListModel(), oldSelected, helpSelected);
}

void drawHelpFooter()
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
        helpOwner = foregroundApp;
        helpSelected = 0;
        helpScrollTop = 0;
        drawHelp();
        return;
    }

    drawAll();
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

    const int count = helpRowCount();
    for (auto c : ks.word)
    {
        if (c == 'h' || c == 'H')
        {
            toggleHelp();
            return;
        }

        if (count <= 0)
            return;

        if (c == ';')
        {
            const int oldSelected = helpSelected;
            const int oldScrollTop = helpScrollTop;
            helpSelected = (helpSelected - 1 + count) % count;
            redrawHelpSelection(oldSelected, oldScrollTop);
            return;
        }

        if (c == '.')
        {
            const int oldSelected = helpSelected;
            const int oldScrollTop = helpScrollTop;
            helpSelected = (helpSelected + 1) % count;
            redrawHelpSelection(oldSelected, oldScrollTop);
            return;
        }
    }
}
