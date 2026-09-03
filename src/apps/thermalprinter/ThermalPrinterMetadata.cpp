#include "apps/thermalprinter/ThermalPrinterMetadata.h"

#include "apps/thermalprinter/ThermalPrinter.h"

const HelpEntry THERMAL_PRINTER_HELP_ENTRIES[] = {
    {"[Ok]", "Edit print text"},
    {"[;/.]", "Move through app rows"},
    {"[,/]", "Change selected setting"},
    {"[P]", "Queue text job"},
    {"[S]", "Refresh printer status"},
    {"[F]", "Feed paper"},
    {"[G]", "Advance to next gap"},
    {"[T]", "Queue printer test page"},
    {"[Fn+Ok]", "New line in editor"},
    {"[Opt]", "Print settings/actions"},
    {"[Esc]", "Close current surface"},
};

const uint8_t THERMAL_PRINTER_HELP_COUNT =
    sizeof(THERMAL_PRINTER_HELP_ENTRIES) /
    sizeof(THERMAL_PRINTER_HELP_ENTRIES[0]);

namespace
{
void editText(int) { thermalPrinterEditText(); }
void printText(int) { thermalPrinterPrint(); }
void refreshStatus(int) { thermalPrinterRefreshStatus(); }
void editServer(int) { thermalPrinterEditServer(); }
void feed(int) { thermalPrinterFeed(); }
void nextLabel(int) { thermalPrinterNextLabel(); }
void testPage(int) { thermalPrinterTestPage(); }
} // namespace

void buildThermalPrinterOptions(std::vector<AppOption> &options)
{
    options.push_back({"Edit Text", "", true, false, true, editText});
    options.push_back({"Print", "", true, false, true, printText});
    options.push_back({"Refresh Status", "", true, false, true, refreshStatus});
    options.push_back({"Media", thermalPrinterMediaLabel(), true, true, false, thermalPrinterAdjustMedia});
    options.push_back({"Text Align", thermalPrinterAlignmentLabel(), true, true, false, thermalPrinterAdjustAlignment});
    options.push_back({"Vertical Center", thermalPrinterVerticalLabel(), true, true, false, thermalPrinterAdjustVertical});
    options.push_back({"Bold", thermalPrinterBoldLabel(), true, true, false, thermalPrinterAdjustBold});
    options.push_back({"Text Size", thermalPrinterSizeLabel(), true, true, false, thermalPrinterAdjustSize});
    options.push_back({"Date Header", thermalPrinterDateLabel(), true, true, false, thermalPrinterAdjustDate});
    options.push_back({"Bottom Feed", thermalPrinterBottomLabel(), true, true, false, thermalPrinterAdjustBottom});
    options.push_back({"Label Height", thermalPrinterLabelHeightLabel(), true, true, false, thermalPrinterAdjustLabelHeight});
    options.push_back({"Printer URL", thermalPrinterServer(), true, false, true, editServer});
    options.push_back({"Feed Paper", "", true, false, true, feed});
    options.push_back({"Next Label", "", true, false, true, nextLabel});
    options.push_back({"Test Page", "", true, false, true, testPage});
}
