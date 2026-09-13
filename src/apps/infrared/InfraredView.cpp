#include "apps/infrared/Infrared.h"
#include "apps/infrared/InfraredInternal.h"

#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"

using namespace InfraredInternal;

namespace
{
ListModel infraredListModel()
{
    ListModel model; model.selected = selected; model.scrollTop = scrollTop; model.marqueeStartMs = marqueeStartMs;
    if (view == View::Files)
    {
        if (files.empty())
        {
            ListItemModel item; item.label = "Folder is empty"; item.isSelected = true; item.isDimmed = true;
            model.items.push_back(item); return model;
        }
        for (const auto &file : files)
        {
            ListItemModel item; item.label = file.name;
            const int dot = item.label.lastIndexOf('.'); if (!file.directory && dot > 0) item.label = item.label.substring(0, dot);
            item.label.replace('_', ' '); model.items.push_back(item);
            if (file.directory)
            {
                model.items.back().type = ListItemType::Property;
                model.items.back().value = "Folder";
                model.items.back().propertyWidth = 45;
            }
        }
    }
    else
    {
        if (commands.empty())
        {
            ListItemModel item; item.label = "No captured commands"; item.isSelected = true; item.isDimmed = true;
            model.items.push_back(item); return model;
        }
        for (const auto &command : commands)
        {
            ListItemModel item; item.label = command.name; item.value = command.type;
            item.type = ListItemType::Property; item.propertyWidth = 62; item.isDimmed = !command.supported;
            model.items.push_back(item);
        }
    }
    for (int i = 0; i < static_cast<int>(model.items.size()); ++i) model.items[i].isSelected = i == selected;
    return model;
}
}

void drawInfrared()
{
    if (captureActive)
    {
        OverlayModel model;
        model.type = OverlayType::Message;
        model.title = "CAPTURE IR SIGNAL";
        model.items = {"Point remote at RX", "Press one remote button"};
        model.confirmText = "[Esc]Cancel";
        drawOverlay(model);
        return;
    }
    if (nameModal != NameModal::None)
    {
        OverlayModel model;
        model.type = OverlayType::TextInput;
        model.title = nameModalError.length() ? nameModalError :
            (nameModal == NameModal::NewFolder ? "NEW IR FOLDER" :
             nameModal == NameModal::NewFile ? "NEW IR FILE" :
             nameModal == NameModal::CaptureCommand ? "NAME CAPTURE" :
             nameModal == NameModal::RenameCommand ? "RENAME COMMAND" : "RENAME IR FILE");
        model.prompt = nameModal == NameModal::NewFolder ? "Folder name" :
            (nameModal == NameModal::CaptureCommand || nameModal == NameModal::RenameCommand) ?
                "Command name" : "File name (.ir added)";
        model.value = nameInput;
        model.confirmText = (nameModal == NameModal::RenameFile || nameModal == NameModal::RenameCommand) ? "[Esc]Cancel [Ok]Rename" :
            nameModal == NameModal::CaptureCommand ? "[Esc]Cancel [Ok]Save" : "[Esc]Cancel [Ok]Create";
        drawOverlay(model);
        return;
    }
    if (deleteConfirmVisible)
    {
        OverlayModel model;
        model.type = OverlayType::Confirm;
        model.title = deleteCommand ? "DELETE COMMAND" : "DELETE IR FILE";
        model.prompt = deleteName;
        model.confirmText = "[Esc]Cancel [Ok]Delete";
        drawOverlay(model);
        return;
    }
    clampSelection();
    HeaderModel header; header.appHeaderTag = "IR";
    if (view == View::Files)
    {
        if (currentDirectory == "/Infrared") header.appHeaderTitle = "Browse IR files";
        else header.appHeaderTitle = currentDirectory.substring(currentDirectory.lastIndexOf('/') + 1);
    }
    else header.appHeaderTitle = openName;
    header.cursor = true; drawHeader(header); drawList(infraredListModel());
    FooterModel footer;
    if (view == View::Files)
    {
        footer.left = currentDirectory == "/Infrared" ? "[Ok]Open [R]Reload" : "[Ok]Open [Del]Up";
        footer.center = "[Opt]Manage";
    }
    else { footer.left = "[Ok]Send [C]Cap"; footer.center = "[Del]Back"; }
    footer.battery = footerBatteryText(); drawFooter(footer);
}
