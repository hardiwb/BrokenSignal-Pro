#include "apps/notes/NotesInternal.h"

#include "core/State.h"
#include "core/System.h"
#include "UI/Footer.h"
#include "UI/Header.h"

namespace NotesInternal
{
String editorDisplayText()
{
    String value = noteEditText;
    if ((int)value.length() > 116)
        value = value.substring((int)value.length() - 116);
    return value;
}

void drawNotesHeader()
{
    HeaderModel model;
    model.appHeaderTag = "NOTES";
    model.appHeaderTitle = getDisplayDateString();
    model.cursor = true;
    drawHeader(model);
}

ListModel buildNotesListModel()
{
    ListModel model;
    model.selected = notesSelected;
    model.scrollTop = notesScrollTop;
    model.marqueeStartMs = notesMarqueeStartMs;

    if (visibleNoteCount() == 0)
    {
        ListItemModel item;
        item.label = notesViewMode == NotesViewMode::Day
                         ? "No notes this day"
                         : "No notes this month";
        item.value = notesViewMode == NotesViewMode::Day ? "D" : "M";
        item.type = ListItemType::Property;
        item.isSelected = true;
        model.items.push_back(item);
    }
    else
    {
        for (int i = 0; i < visibleNoteCount(); ++i)
        {
            const int noteIndex = visibleNoteIndices[i];
            ListItemModel item;
            item.label = formatEntryLabel(noteEntries[noteIndex]);
            item.type = ListItemType::Normal;
            item.isSelected = i == notesSelected;
            item.isDimmed = noteEntries[noteIndex].done;
            model.items.push_back(item);
        }
    }

    return model;
}

void drawNotesList()
{
    drawList(buildNotesListModel());
}

void redrawNotesSelection(int oldSelected, int oldScrollTop)
{
    clampNotesSelection();
    if (notesScrollTop != oldScrollTop)
    {
        drawNotesList();
        return;
    }

    drawListSelection(buildNotesListModel(), oldSelected, notesSelected);
}

void drawNotesFooter()
{
    FooterModel model;
    model.left = "[A]Ad [X]Done";
    model.center = "[Ok]Edit";
    model.battery = footerBatteryText();
    drawFooter(model);
}
} // namespace NotesInternal
