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
static int applicationScrollTop = 0;

static void clampApplicationScroll()
{
    const int count = static_cast<int>(appCount());
    if (count <= 0)
    {
        applicationSelected = 0;
        applicationScrollTop = 0;
        return;
    }

    applicationSelected = constrain(applicationSelected, 0, count - 1);
    if (applicationSelected < applicationScrollTop)
        applicationScrollTop = applicationSelected;
    if (applicationSelected >= applicationScrollTop + LIST_VISIBLE_ITEM)
        applicationScrollTop = applicationSelected - LIST_VISIBLE_ITEM + 1;
    applicationScrollTop = constrain(
        applicationScrollTop,
        0,
        max(0, count - LIST_VISIBLE_ITEM));
}

static ListModel buildApplicationsListModel()
{
    clampApplicationScroll();
    ListModel model;
    model.selected = applicationSelected;
    model.scrollTop = applicationScrollTop;

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
    footer.left = "[" + String(keyboardApplicationsShortcutLabel()) + "]Close [Ok]Open";
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
    applicationScrollTop = 0;
    clampApplicationScroll();
    drawApplicationsMenu();
}

void exitApplicationsMenu()
{
    applicationsMenuVisible = false;
    drawAll();
}

void handleApplicationsInput(Keyboard_Class::KeysState &ks)
{
    if (keyboardApplicationsShortcutPressed(ks) || keyboardBackPressed(ks))
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
        const int shortcutTarget =
            listVisibleShortcutTarget(c, applicationScrollTop, applicationCount);
        if (shortcutTarget >= 0)
        {
            applicationSelected = shortcutTarget;
            openSelectedApplication();
            return;
        }

        if (c == ';')
        {
            int oldSelected = applicationSelected;
            int oldScrollTop = applicationScrollTop;
            applicationSelected = (applicationSelected - 1 + applicationCount) % applicationCount;
            clampApplicationScroll();
            if (oldScrollTop != applicationScrollTop)
                drawApplicationsMenu();
            else
                drawListSelection(buildApplicationsListModel(), oldSelected, applicationSelected);
            return;
        }
        if (c == '.')
        {
            int oldSelected = applicationSelected;
            int oldScrollTop = applicationScrollTop;
            applicationSelected = (applicationSelected + 1) % applicationCount;
            clampApplicationScroll();
            if (oldScrollTop != applicationScrollTop)
                drawApplicationsMenu();
            else
                drawListSelection(buildApplicationsListModel(), oldSelected, applicationSelected);
            return;
        }
    }
}
