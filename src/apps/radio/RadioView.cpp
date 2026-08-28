#include "apps/radio/Radio.h"

#include "apps/radio/RadioInternal.h"
#include "core/State.h"
#include "core/System.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"

namespace
{
void populateRadioListModel(ListModel &model)
{
    model.selected = radioSelected;
    model.scrollTop = radioScrollTop;
    for (int i = 0; i < radioCount; ++i)
    {
        ListItemModel item;
        item.label = radioList[i].name;
        item.isSelected = i == radioSelected;
        item.isActive = i == radioPlaying && radioIsPlaying;
        model.items.push_back(item);
    }
}
} // namespace

namespace RadioInternal
{
void redrawRadioSelection(int oldSelected, int oldScrollTop)
{
    if (radioScrollTop != oldScrollTop)
    {
        drawRadioList();
        return;
    }
    drawRadioRow(oldSelected);
    drawRadioRow(radioSelected);
}
} // namespace RadioInternal

void drawRadioHeader()
{
    String station = "SELECT STATION";
    if (radioIsPlaying && radioPlaying >= 0 && radioPlaying < radioCount)
        station = radioList[radioPlaying].name;
    HeaderModel model;
    model.appHeaderTag = "RADIO";
    model.appHeaderTitle = station;
    model.cursor = radioIsPlaying && cursorVisible;
    drawHeader(model);
    if (hdrMsgEnd > 0 && millis() < hdrMsgEnd)
        drawHeaderMessage(hdrMsg);
}

void drawRadioRow(int index)
{
    if (index < 0 || index >= radioCount)
        return;
    ListModel model;
    populateRadioListModel(model);
    drawListRow(model, index);
}

void drawRadioList()
{
    ListModel model;
    populateRadioListModel(model);
    drawList(model);
}

void drawRadioStatus()
{
    FooterModel model;
    model.left = "[A]Add [X]Rm";
    model.center = radioIsPlaying ? "Live" : "Idle";
    model.battery = footerBatteryText();
    drawFooter(model);
}

void drawRadioAll()
{
    drawRadioHeader();
    drawRadioList();
    drawRadioStatus();
}

void drawAddUrlOverlay(bool inputOnly)
{
    using namespace RadioInternal;
    OverlayModel model;
    model.type = OverlayType::TwoFieldInput;
    model.title = "Enter stream URL";
    model.value = radioEditUrl;
    model.secondValue = radioEditName;
    model.activeField = radioEditField;
    model.cursorIndex = radioEditField == 0 ? radioEditUrlCursor : radioEditNameCursor;
    model.prompt = "https://stream.com";
    model.secondPrompt = "Station Name";
    model.helperText = "[Tab]Switch [Fn L/R]Cursor";
    model.confirmText = "[Esc]Cancel [Ok]Save";
    inputOnly ? drawOverlayTwoFieldInputValues(model) : drawOverlay(model);
}

void drawAddNameOverlay(bool inputOnly)
{
    drawAddUrlOverlay(inputOnly);
}

void drawRemoveConfirm()
{
    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = "REMOVE STATION?";
    model.items.push_back(radioCount > 0 && radioSelected < radioCount ? radioList[radioSelected].name : "?");
    model.items.push_back("This will be deleted.");
    model.confirmText = "[Ok]Remove   [Esc]Cancel";
    drawOverlay(model);
}
