#include "apps/infrared/InfraredMetadata.h"
#include "apps/infrared/Infrared.h"

const HelpEntry INFRARED_HELP_ENTRIES[] = {
    {"[Ok]", "Open folder/file or send"}, {"[Del]", "Back / parent folder"},
    {"[C]", "Capture into open IR file"}, {"[R]", "Reload IR files"},
    {"[;/.]", "Cursor up / down"},
    {"[Opt]", "IR settings and files"}, {"[Fn+I]", "Open Infrared"},
    {"[Esc]", "Applications"}, {"[H]", "Close help"},
};
const uint8_t INFRARED_HELP_COUNT = sizeof(INFRARED_HELP_ENTRIES) / sizeof(INFRARED_HELP_ENTRIES[0]);

namespace {
void adjustTxSource(int d) { infraredAdjustTxSource(d); }
void adjustTxPin(int d) { infraredAdjustTxPin(d); }
void adjustRxEnabled(int d) { infraredAdjustRxEnabled(d); }
void adjustRxPin(int d) { infraredAdjustRxPin(d); }
void adjustFrequency(int d) { infraredAdjustRawFrequency(d); }
void adjustDuty(int d) { infraredAdjustRawDuty(d); }
void reloadFiles(int) { infraredReload(); }
void newFolder(int) { infraredRequestNewFolder(); }
void newFile(int) { infraredRequestNewFile(); }
void captureSignal(int) { infraredRequestCapture(); }
void renameFile(int) { infraredRequestRename(); }
void deleteFile(int) { infraredRequestDelete(); }
}

void buildInfraredOptions(std::vector<AppOption> &options)
{
    options.push_back({"TX Source", infraredTxSettingLabel(), true, true, false, adjustTxSource});
    options.push_back({"External TX Pin", infraredTxPinLabel(), infraredExternalTxSelected(), true, false, adjustTxPin});
    options.push_back({"RX Module", infraredRxSettingLabel(), true, true, false, adjustRxEnabled});
    options.push_back({"RX Pin", infraredRxPinLabel(), infraredRxEnabled(), true, false, adjustRxPin});
    options.push_back({"Raw Frequency", infraredRawFrequencyLabel(), true, true, false, adjustFrequency});
    options.push_back({"Raw Duty Cycle", infraredRawDutyLabel(), true, true, false, adjustDuty});
    options.push_back({"Reload IR Files", "", true, false, true, reloadFiles});
    options.push_back({"New Folder", "", true, false, true, newFolder});
    options.push_back({"New IR File", "", true, false, true, newFile});
    options.push_back({"Capture IR Signal", "", infraredCanCapture(), false, true, captureSignal});
    options.push_back({"Rename IR File", "", infraredHasSelectedFile(), false, true, renameFile});
    options.push_back({"Delete IR File", "", infraredHasSelectedFile(), false, true, deleteFile});
}
