#include "apps/macropad/MacroPadMetadata.h"

const HelpEntry MACRO_PAD_HELP_ENTRIES[] = {
    {"[Ok]", "Open / connect / run"},
    {"[Hotkey]", "Run configured macro"},
    {"[;/.]", "Cursor up / down"},
    {"[Del]", "Previous screen"},
    {"[R]", "Reload game files"},
    {"[BtnG0]", "Disconnect macro pad"},
    {"[Fn+P]", "Open Macro Pad"},
};

const uint8_t MACRO_PAD_HELP_COUNT =
    sizeof(MACRO_PAD_HELP_ENTRIES) /
    sizeof(MACRO_PAD_HELP_ENTRIES[0]);

void buildMacroPadOptions(std::vector<AppOption> &)
{
}
