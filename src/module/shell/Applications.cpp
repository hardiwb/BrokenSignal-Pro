#include "module/shell/Applications.h"

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/programs/Calculator.h"
#include "module/programs/Notes.h"
#include "module/programs/Radio.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"

bool applicationsMenuVisible = false;

namespace
{
enum class ApplicationId
{
    MusicPlayer,
    WebRadio,
    Notes,
    Calculator
};

struct ApplicationDescriptor
{
    ApplicationId id;
    const char *displayName;
    const char *headerTag;
};

static const ApplicationDescriptor APPLICATIONS[] = {
    {ApplicationId::MusicPlayer, "Music Player", "MUSIC"},
    {ApplicationId::WebRadio, "Web Radio", "RADIO"},
    {ApplicationId::Notes, "Notes", "NOTES"},
    {ApplicationId::Calculator, "Calculator", "CALCULATOR"}};
static constexpr int APPLICATION_COUNT =
    sizeof(APPLICATIONS) / sizeof(APPLICATIONS[0]);
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
        item.label = APPLICATIONS[i].displayName;
        item.isSelected = i == applicationSelected;
        model.items.push_back(item);
    }
    return model;
}

static void openSelectedApplication()
{
    applicationsMenuVisible = false;

    switch (APPLICATIONS[applicationSelected].id)
    {
    case ApplicationId::MusicPlayer:
        notesClose();
        if (webRadioMode)
            exitWebRadioMode();
        else
            drawAll();
        break;
    case ApplicationId::WebRadio:
        notesClose();
        if (!webRadioMode)
            enterWebRadioMode();
        else
            drawRadioAll();
        break;
    case ApplicationId::Notes:
        notesOpen();
        break;
    case ApplicationId::Calculator:
        openCalculatorHistory();
        break;
    }
}
} // namespace

void drawApplicationsMenu()
{
    HeaderModel header;
    header.appHeaderTag = "APPS";
    header.appHeaderTitle = "Applications";
    header.cursor = true;
    drawHeader(header);

    drawList(buildApplicationsListModel());

    FooterModel footer;
    footer.left = "[Alt]Close [Ok]Open";
    footer.center = "";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void enterApplicationsMenu()
{
    optionsMenuVisible = false;
    calculatorVisible = false;
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
    if ((ks.alt && ks.word.empty()) || keyboardBackPressed(ks))
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
