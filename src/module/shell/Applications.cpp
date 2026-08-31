#include "module/shell/Applications.h"

#include "core/Keyboard.h"
#include "core/AppRegistry.h"
#include "core/AppRuntime.h"
#include "core/State.h"
#include "core/System.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"

bool applicationsMenuVisible = false;

namespace
{
static int applicationSelected = 2;

static ListModel buildApplicationsListModel()
{
    ListModel model;
    model.selected = applicationSelected;
    model.scrollTop = 0;

    for (size_t i = 0; i < appCount(); ++i)
    {
        ListItemModel item;
        item.type = ListItemType::Normal;
        item.label = appDescriptorAt(i).name;
        item.isSelected = (int)i == applicationSelected;
        model.items.push_back(item);
    }
    return model;
}

static void openSelectedApplication()
{
    applicationsMenuVisible = false;
    appRuntimeOpen(appDescriptorAt(applicationSelected).id);
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
    settingsMenuVisible = false;
    helpVisible = false;
    debugOverlayVisible = false;
    applicationsMenuVisible = true;
    const int foregroundIndex = appIndex(foregroundApp);
    applicationSelected = foregroundIndex >= 0 ? foregroundIndex : 0;
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

    const int applicationCount = (int)appCount();
    if (applicationCount <= 0)
        return;

    for (auto c : ks.word)
    {
        if (c >= '0' && c <= '9')
        {
            // 1..9 open the matching visible row; 0 is the tenth row.
            const int shortcutIndex = c == '0' ? 9 : c - '1';
            if (shortcutIndex < applicationCount)
            {
                applicationSelected = shortcutIndex;
                openSelectedApplication();
            }
            return;
        }

        if (c == ';')
        {
            int oldSelected = applicationSelected;
            applicationSelected = (applicationSelected - 1 + applicationCount) % applicationCount;
            drawListSelection(buildApplicationsListModel(), oldSelected, applicationSelected);
            return;
        }
        if (c == '.')
        {
            int oldSelected = applicationSelected;
            applicationSelected = (applicationSelected + 1) % applicationCount;
            drawListSelection(buildApplicationsListModel(), oldSelected, applicationSelected);
            return;
        }
    }
}
