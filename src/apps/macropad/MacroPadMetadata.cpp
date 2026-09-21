#include "apps/macropad/MacroPadMetadata.h"

#include "apps/macropad/MacroPad.h"

const HelpEntry MACRO_PAD_HELP_ENTRIES[] = {
    {"[Ok]", "Open / connect / run"},
    {"[Hotkey]", "Run configured macro"},
    {"[Opt]", "Edit bindings and starter"},
    {"[Del]", "Previous screen"},
    {"[;/.]", "Cursor up / down"},
    {"[R]", "Reload game files"},
    {"[BtnG0]", "Disconnect macro pad"},
    {"[Fn+P]", "Open Macro Pad"},
};

const uint8_t MACRO_PAD_HELP_COUNT =
    sizeof(MACRO_PAD_HELP_ENTRIES) /
    sizeof(MACRO_PAD_HELP_ENTRIES[0]);

namespace
{
void addBinding(int) { macroPadRequestAddBinding(); }
void changeKey(int) { macroPadRequestChangeKey(); }
void deleteBinding(int) { macroPadRequestDeleteBinding(); }
void moveUp(int) { macroPadMoveSelectedBinding(-1); }
void moveDown(int) { macroPadMoveSelectedBinding(+1); }
void adjustStarter(int direction) { macroPadAdjustStarterInput(direction); }
}

void buildMacroPadOptions(std::vector<AppOption> &options)
{
    const bool editable = macroPadBindingsEditable();
    const bool selected = macroPadHasSelectedBinding();
    options.push_back({"Add Macro", "", macroPadCanAddBinding(), false, true, addBinding});
    options.push_back({"Change Key", "", editable && selected, false, true, changeKey});
    options.push_back({"Delete Macro", "", editable && selected, false, true, deleteBinding});
    options.push_back({"Move Up", "", editable && macroPadCanMoveSelectedBinding(-1), false, true, moveUp});
    options.push_back({"Move Down", "", editable && macroPadCanMoveSelectedBinding(+1), false, true, moveDown});
    options.push_back({"Starter Input", macroPadStarterInputLabel(), editable, true, false, adjustStarter});
}
