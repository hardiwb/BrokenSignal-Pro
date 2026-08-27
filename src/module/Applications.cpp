#include "module/Applications.h"

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/Calculator.h"
#include "module/Notes.h"
#include "module/Radio.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"

bool applicationsMenuVisible = false;

namespace
{
static const int APPLICATION_COUNT = 4;
static const char *APPLICATION_LABELS[APPLICATION_COUNT] = {
    "Music Player", "Web Radio", "Notes", "Calculator"};
static int applicationSelected = 2;

static ListModel buildApplicationsListModel()
{
    ListModel model;
    model.selected = applicationSelected;
    model.scrollTop = 0;

    for (int i = 0; i < APPLICATION_COUNT; ++i)
    {
        ListItemModel item;
        item.type = ListItemType::Normal;
        item.label = APPLICATION_LABELS[i];
        item.isSelected = i == applicationSelected;
        model.items.push_back(item);
    }
    return model;
}

static void openSelectedApplication()
{
    applicationsMenuVisible = false;

    switch (applicationSelected)
    {
    case 0:
        notesClose();
        if (webRadioMode)
            exitWebRadioMode();
        else
            drawAll();
        break;
    case 1:
        notesClose();
        if (!webRadioMode)
            enterWebRadioMode();
        else
            drawRadioAll();
        break;
    case 2:
        notesOpen();
        break;
    case 3:
        openCalculator();
        break;
    }
}
} // namespace

void drawApplicationsMenu()
{
    HeaderModel header;
    header.mode = "APPLICATIONS";
    header.title = "APPLICATIONS";
    header.cursor = true;
    drawHeader(header);

    drawList(buildApplicationsListModel());

    FooterModel footer;
    footer.left = "[Esc]Close [Ent]Open";
    footer.center = "";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void enterApplicationsMenu()
{
    settingsMenuVisible = false;
    helpVisible = false;
    debugOverlayVisible = false;
    applicationsMenuVisible = true;
    applicationSelected = notesMode ? 2 : (webRadioMode ? 1 : 0);
    drawApplicationsMenu();
}

void exitApplicationsMenu()
{
    applicationsMenuVisible = false;
    drawAll();
}

void handleApplicationsInput(Keyboard_Class::KeysState &ks)
{
    if (keyboardBackPressed(ks) || ks.opt)
    {
        exitApplicationsMenu();
        return;
    }

    if (ks.enter)
    {
        openSelectedApplication();
        return;
    }

    for (auto c : ks.word)
    {
        if (c == ';')
        {
            int oldSelected = applicationSelected;
            applicationSelected = (applicationSelected - 1 + APPLICATION_COUNT) % APPLICATION_COUNT;
            drawListSelection(buildApplicationsListModel(), oldSelected, applicationSelected);
            return;
        }
        if (c == '.')
        {
            int oldSelected = applicationSelected;
            applicationSelected = (applicationSelected + 1) % APPLICATION_COUNT;
            drawListSelection(buildApplicationsListModel(), oldSelected, applicationSelected);
            return;
        }
    }
}
