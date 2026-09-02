#include "apps/thermalprinter/ThermalPrinter.h"

#include <SD.h>
#include <WiFi.h>

#include "apps/thermalprinter/ThermalPrinterTransport.h"
#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/service/Clock.h"
#include "module/service/WiFi.h"
#include "module/shell/Help.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"

namespace
{
constexpr const char *SETTINGS_PATH = "/Printer/settings.cfg";
constexpr size_t MAX_TEXT_BYTES = 2048;
constexpr size_t MAX_SERVER_BYTES = 96;

enum class PrinterModal : uint8_t
{
    None,
    TextEditor,
    ServerEditor,
    Result
};

enum class PendingOperation : uint8_t
{
    None,
    Print,
    Status,
    Feed,
    NextLabel,
    TestPage
};

String serverUrl = "http://thermal-printer.local";
String draft;
String originalValue;
String resultTitle;
String resultMessage;
String printerState = "NOT CHECKED";
ThermalPrintLayout layout;
ThermalPrinterStatus lastStatus;
PrinterModal modal = PrinterModal::None;
PendingOperation pendingOperation = PendingOperation::None;

String onOff(bool value)
{
    return value ? "On" : "Off";
}

String cleanSingleLine(String value)
{
    value.replace("\r", " ");
    value.replace("\n", " ");
    value.replace("\t", " ");
    value.trim();
    return value;
}

String boolText(bool value)
{
    return value ? "1" : "0";
}

void savePrinterSettings()
{
    SD.mkdir("/Printer");
    SD.remove(SETTINGS_PATH);
    File file = SD.open(SETTINGS_PATH, FILE_WRITE);
    if (!file)
    {
        showHdrMsg("SD ERROR");
        return;
    }
    file.println("server=" + serverUrl);
    file.println("label=" + boolText(layout.label));
    file.println("center=" + boolText(layout.center));
    file.println("vertical=" + boolText(layout.vertical));
    file.println("bold=" + boolText(layout.bold));
    file.println("double=" + boolText(layout.doubleSize));
    file.println("date=" + boolText(layout.includeDate));
    file.println("bottom=" + String(layout.bottomLines));
    file.println("labelLines=" + String(layout.labelLines));
    file.close();
}

void loadPrinterSettings()
{
    serverUrl = "http://thermal-printer.local";
    layout = {};
    File file = SD.open(SETTINGS_PATH, FILE_READ);
    if (!file)
        return;

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        const int separator = line.indexOf('=');
        if (separator <= 0)
            continue;
        const String key = line.substring(0, separator);
        String value = line.substring(separator + 1);
        value.trim();
        if (key == "server") serverUrl = value;
        else if (key == "label") layout.label = value == "1";
        else if (key == "center") layout.center = value == "1";
        else if (key == "vertical") layout.vertical = value == "1";
        else if (key == "bold") layout.bold = value == "1";
        else if (key == "double") layout.doubleSize = value == "1";
        else if (key == "date") layout.includeDate = value == "1";
        else if (key == "bottom") layout.bottomLines = constrain(value.toInt(), 0, 40);
        else if (key == "labelLines") layout.labelLines = constrain(value.toInt(), 1, 40);
    }
    file.close();

    if (!serverUrl.startsWith("http://") || serverUrl.length() > MAX_SERVER_BYTES)
        serverUrl = "http://thermal-printer.local";
}

String draftSummary()
{
    if (!draft.length())
        return "<empty>";
    String summary = cleanSingleLine(draft);
    return String(draft.length()) + "B " + summary;
}

String styleSummary()
{
    String value = layout.center ? "Center" : "Left";
    if (layout.bold)
        value += " Bold";
    if (layout.doubleSize)
        value += " 2x";
    return value;
}

ListModel printerListModel()
{
    ListModel model;
    const struct Row { const char *label; String value; } rows[] = {
        {"Text", draftSummary()},
        {"Printer", printerState},
        {"Media", layout.label ? "Label" : "Continuous"},
        {"Style", styleSummary()},
        {"Bottom feed", String(layout.bottomLines) + " lines"},
        {"Label height", String(layout.labelLines) + " lines"},
        {"Server", serverUrl},
    };
    for (const Row &row : rows)
    {
        ListItemModel item;
        item.label = row.label;
        item.value = row.value;
        item.type = ListItemType::Property;
        model.items.push_back(item);
    }
    return model;
}

void drawTextEditor()
{
    OverlayModel model;
    model.type = OverlayType::TextInput;
    model.title = "PRINT TEXT";
    model.prompt = String(draft.length()) + "/2048 bytes";
    model.value = draft;
    model.tallInput = true;
    model.helperText = "[Fn+Ok] New line";
    model.confirmText = "[Esc]Cancel [Ok]Save";
    drawOverlay(model);
}

void drawServerEditor()
{
    OverlayModel model;
    model.type = OverlayType::TextInput;
    model.title = "PRINTER URL";
    model.prompt = "HTTP URL or printer IP";
    model.value = serverUrl;
    model.confirmText = "[Esc]Cancel [Ok]Save";
    drawOverlay(model);
}

void drawResult()
{
    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = resultTitle;
    model.items.push_back(resultMessage);
    model.confirmText = "[Esc/Ok] Close";
    drawOverlay(model);
}

void showResult(const String &title, const String &message)
{
    resultTitle = title;
    resultMessage = message;
    modal = PrinterModal::Result;
    drawResult();
}

void showContacting()
{
    resultTitle = "THERMAL PRINTER";
    resultMessage = "Contacting printer...";
    modal = PrinterModal::Result;
    drawResult();
}

void updateDateHeader()
{
    struct tm now{};
    if (!getCurrentTime(now))
    {
        layout.dateHeader = "";
        return;
    }
    char date[11];
    snprintf(date, sizeof(date), "%04d-%02d-%02d",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
    layout.dateHeader = date;
}

bool ensureNetwork(PendingOperation operation)
{
    pendingOperation = operation;
    if (WiFi.status() == WL_CONNECTED)
        return true;
    if (ensureWifiConnected() == WifiStartupResult::Connected)
        return true;
    return false;
}

void performPendingOperation()
{
    const PendingOperation operation = pendingOperation;
    pendingOperation = PendingOperation::None;
    if (operation == PendingOperation::None)
        return;

    showContacting();
    String message;
    bool success = false;
    if (operation == PendingOperation::Status)
    {
        success = fetchThermalPrinterStatus(serverUrl, lastStatus, message);
        if (success)
        {
            printerState = lastStatus.busy ? "PRINTING" : "READY";
            if (lastStatus.queued > 0)
                printerState += " Q" + String(lastStatus.queued);
            message = printerState + " | done " + String(lastStatus.completed) +
                      " | failed " + String(lastStatus.failed);
        }
    }
    else if (operation == PendingOperation::Print)
    {
        updateDateHeader();
        success = queueThermalPrinterText(serverUrl, draft, layout, message);
        if (success)
            printerState = "QUEUED";
    }
    else
    {
        String action;
        if (operation == PendingOperation::Feed) action = "feed";
        else if (operation == PendingOperation::NextLabel) action = "next";
        else if (operation == PendingOperation::TestPage) action = "test";
        success = queueThermalPrinterAction(serverUrl, action, layout.bottomLines, message);
        if (success)
            printerState = "QUEUED";
    }

    if (!success)
        printerState = "ERROR";
    showResult(success ? "PRINTER ACCEPTED" : "PRINTER ERROR", message);
}

void startOperation(PendingOperation operation)
{
    if (operation == PendingOperation::Print && draft.length() == 0)
    {
        showResult("PRINTER ERROR", "Enter text before printing");
        return;
    }
    if (ensureNetwork(operation))
        performPendingOperation();
}

void finishTextEdit()
{
    modal = PrinterModal::None;
    originalValue = "";
    drawThermalPrinter();
}

void finishServerEdit()
{
    serverUrl.trim();
    while (serverUrl.endsWith("/"))
        serverUrl.remove(serverUrl.length() - 1);
    if (!serverUrl.startsWith("http://") || serverUrl.length() <= 7)
    {
        showResult("INVALID URL", "Use http://host or http://IP");
        serverUrl = originalValue;
        originalValue = "";
        return;
    }
    originalValue = "";
    modal = PrinterModal::None;
    printerState = "NOT CHECKED";
    savePrinterSettings();
    drawThermalPrinter();
}

void handleEditorInput(Keyboard_Class::KeysState &keys)
{
    String &value = modal == PrinterModal::TextEditor ? draft : serverUrl;
    const size_t limit = modal == PrinterModal::TextEditor ? MAX_TEXT_BYTES : MAX_SERVER_BYTES;
    if (keys.enter)
    {
        if (modal == PrinterModal::TextEditor && keys.fn && value.length() < limit)
        {
            value += '\n';
            drawTextEditor();
        }
        else if (modal == PrinterModal::TextEditor)
            finishTextEdit();
        else
            finishServerEdit();
        return;
    }
    if (keys.del && value.length())
    {
        value.remove(value.length() - 1);
        if (modal == PrinterModal::TextEditor) drawTextEditor();
        else drawServerEditor();
        return;
    }
    for (char c : keys.word)
    {
        if (keyboardTextInputChar(keys, c) && value.length() < limit)
        {
            value += c;
            if (modal == PrinterModal::TextEditor) drawTextEditor();
            else drawServerEditor();
        }
    }
}

void saveAndRedraw()
{
    savePrinterSettings();
    if (!optionsMenuVisible)
        drawThermalPrinter();
}
} // namespace

void thermalPrinterOpen()
{
    loadPrinterSettings();
    modal = PrinterModal::None;
    pendingOperation = PendingOperation::None;
    printerState = WiFi.status() == WL_CONNECTED ? "PRESS S" : "WIFI OFF";
    drawThermalPrinter();
}

void drawThermalPrinter()
{
    if (modal == PrinterModal::TextEditor) return drawTextEditor();
    if (modal == PrinterModal::ServerEditor) return drawServerEditor();
    if (modal == PrinterModal::Result) return drawResult();

    HeaderModel header;
    header.appHeaderTag = "PRINTER";
    header.appHeaderTitle = "Thermal Printer";
    header.cursor = true;
    drawHeader(header);
    drawList(printerListModel());
    FooterModel footer;
    footer.left = "[Ok]Edit [P]Print";
    footer.center = "[S]Status";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void handleThermalPrinterInput(Keyboard_Class::KeysState &keys)
{
    if (modal == PrinterModal::TextEditor || modal == PrinterModal::ServerEditor)
    {
        handleEditorInput(keys);
        return;
    }
    if (modal == PrinterModal::Result)
    {
        if (keys.enter)
            cancelThermalPrinterModal();
        return;
    }
    if (keys.enter)
    {
        thermalPrinterEditText();
        return;
    }
    for (char c : keys.word)
    {
        if (c == 'p' || c == 'P') return thermalPrinterPrint();
        if (c == 's' || c == 'S') return thermalPrinterRefreshStatus();
        if (c == 'f' || c == 'F') return thermalPrinterFeed();
        if (c == 'g' || c == 'G') return thermalPrinterNextLabel();
        if (c == 't' || c == 'T') return thermalPrinterTestPage();
        if (c == 'h' || c == 'H') { toggleHelp(); return; }
    }
}

bool thermalPrinterModalActive()
{
    return modal != PrinterModal::None;
}

void cancelThermalPrinterModal()
{
    if (modal == PrinterModal::TextEditor)
        draft = originalValue;
    else if (modal == PrinterModal::ServerEditor)
        serverUrl = originalValue;
    originalValue = "";
    modal = PrinterModal::None;
    drawThermalPrinter();
}

bool thermalPrinterResumePendingOperation()
{
    if (pendingOperation == PendingOperation::None)
        return false;
    performPendingOperation();
    return true;
}

void thermalPrinterCancelPendingOperation()
{
    pendingOperation = PendingOperation::None;
}

void thermalPrinterEditText()
{
    originalValue = draft;
    modal = PrinterModal::TextEditor;
    drawTextEditor();
}

void thermalPrinterEditServer()
{
    originalValue = serverUrl;
    modal = PrinterModal::ServerEditor;
    drawServerEditor();
}

void thermalPrinterPrint() { startOperation(PendingOperation::Print); }
void thermalPrinterRefreshStatus() { startOperation(PendingOperation::Status); }
void thermalPrinterFeed() { startOperation(PendingOperation::Feed); }
void thermalPrinterNextLabel() { startOperation(PendingOperation::NextLabel); }
void thermalPrinterTestPage() { startOperation(PendingOperation::TestPage); }

String thermalPrinterServer() { return serverUrl; }
String thermalPrinterMediaLabel() { return layout.label ? "Label" : "Continuous"; }
String thermalPrinterAlignmentLabel() { return layout.center ? "Center" : "Left"; }
String thermalPrinterVerticalLabel() { return onOff(layout.vertical); }
String thermalPrinterBoldLabel() { return onOff(layout.bold); }
String thermalPrinterSizeLabel() { return layout.doubleSize ? "Double" : "Normal"; }
String thermalPrinterDateLabel() { return onOff(layout.includeDate); }
String thermalPrinterBottomLabel() { return String(layout.bottomLines); }
String thermalPrinterLabelHeightLabel() { return String(layout.labelLines); }

void thermalPrinterAdjustMedia(int) { layout.label = !layout.label; saveAndRedraw(); }
void thermalPrinterAdjustAlignment(int) { layout.center = !layout.center; saveAndRedraw(); }
void thermalPrinterAdjustVertical(int) { layout.vertical = !layout.vertical; saveAndRedraw(); }
void thermalPrinterAdjustBold(int) { layout.bold = !layout.bold; saveAndRedraw(); }
void thermalPrinterAdjustSize(int) { layout.doubleSize = !layout.doubleSize; saveAndRedraw(); }
void thermalPrinterAdjustDate(int) { layout.includeDate = !layout.includeDate; saveAndRedraw(); }
void thermalPrinterAdjustBottom(int direction)
{
    layout.bottomLines = constrain(static_cast<int>(layout.bottomLines) + direction, 0, 40);
    saveAndRedraw();
}
void thermalPrinterAdjustLabelHeight(int direction)
{
    layout.labelLines = constrain(static_cast<int>(layout.labelLines) + direction, 1, 40);
    saveAndRedraw();
}
