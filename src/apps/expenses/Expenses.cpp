#include "apps/expenses/Expenses.h"
#include <SD.h>
#include <qrcode.h>
#include <time.h>
#include <vector>
#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/service/Clock.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"
#include "UI/Themes.h"

namespace {
struct ExpenseEntry { String date; bool shared = false; String name; String value; String currency; };
enum class Modal { None, Editor, MoveDate, Currency, Qr };
std::vector<ExpenseEntry> entries;
std::vector<int> visible;
Modal modal = Modal::None;
String currentDate, currentMonth, filePath, defaultCurrency = "IDR";
int dayOffset = 0, selected = 0, scrollTop = 0;
uint32_t marqueeStart = 0;
int editVisibleIndex = -1, editorField = 0, nameCursor = 0, amountCursor = 0;
String editName, editAmount, moveDate, currencyInput;
bool invalidInput = false;
std::vector<String> qrPages;
int qrPage = 0;

bool parseDate(const String &text, struct tm &date) {
    int y, m, d; char tail;
    if (text.length() != 10 || sscanf(text.c_str(), "%d-%d-%d%c", &y, &m, &d, &tail) != 3 ||
        y < 2000 || y > 2099 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    memset(&date, 0, sizeof(date)); date.tm_year = y - 1900; date.tm_mon = m - 1;
    date.tm_mday = d; date.tm_hour = 12; date.tm_isdst = -1;
    if (mktime(&date) == (time_t)-1) return false;
    return date.tm_year == y - 1900 && date.tm_mon == m - 1 && date.tm_mday == d;
}
String dateKey(struct tm &date) {
    char out[11]; snprintf(out, sizeof(out), "%04d-%02d-%02d", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
    return String(out);
}
void updateDate() {
    struct tm date{};
    if (!getCurrentTime(date)) { date.tm_year = 126; date.tm_mon = 0; date.tm_mday = 1; date.tm_hour = 12; }
    date.tm_mday += dayOffset; date.tm_isdst = -1; mktime(&date);
    currentDate = dateKey(date); currentMonth = currentDate.substring(0, 7);
    filePath = "/Expenses/" + currentMonth + ".txt";
}
String cleanField(String value) {
    value.replace("|", " "); value.replace("\r", " "); value.replace("\n", " "); value.trim(); return value;
}
void rebuildVisible() {
    visible.clear();
    for (int i = 0; i < (int)entries.size(); ++i) if (entries[i].date == currentDate) visible.push_back(i);
    if (visible.empty()) { selected = 0; scrollTop = 0; return; }
    selected = constrain(selected, 0, (int)visible.size() - 1);
    if (selected < scrollTop) scrollTop = selected;
    if (selected >= scrollTop + LIST_VISIBLE_ITEM) scrollTop = selected - LIST_VISIBLE_ITEM + 1;
}
void loadDefaultCurrency() {
    File f = SD.open("/Expenses/settings.cfg", FILE_READ);
    if (f) { defaultCurrency = f.readStringUntil('\n'); f.close(); defaultCurrency.trim(); defaultCurrency.toUpperCase(); }
    if (!defaultCurrency.length()) defaultCurrency = "IDR";
}
void saveDefaultCurrency() {
    SD.mkdir("/Expenses"); SD.remove("/Expenses/settings.cfg"); File f = SD.open("/Expenses/settings.cfg", FILE_WRITE);
    if (f) { f.println(defaultCurrency); f.close(); } else showHdrMsg("SD ERROR");
}
void saveEntries() {
    SD.mkdir("/Expenses"); SD.remove(filePath.c_str()); File f = SD.open(filePath, FILE_WRITE);
    if (!f) { showHdrMsg("SD ERROR"); return; }
    for (const auto &e : entries)
        f.printf("%s|%c|%s|%s|%s\n", e.date.c_str(), e.shared ? 'X' : '-', e.name.c_str(), e.value.c_str(), e.currency.c_str());
    f.close();
}
void loadEntries() {
    entries.clear(); updateDate(); File f = SD.open(filePath, FILE_READ);
    if (f) {
        while (f.available()) {
            String line = f.readStringUntil('\n'); line.trim();
            int p1 = line.indexOf('|'), p2 = line.indexOf('|', p1 + 1);
            int p3 = line.indexOf('|', p2 + 1), p4 = line.indexOf('|', p3 + 1);
            if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) continue;
            ExpenseEntry e; e.date = line.substring(0, p1);
            e.shared = line.substring(p1 + 1, p2).equalsIgnoreCase("X");
            e.name = line.substring(p2 + 1, p3); e.value = line.substring(p3 + 1, p4);
            e.currency = line.substring(p4 + 1); e.currency.trim(); entries.push_back(e);
        }
        f.close();
    }
    rebuildVisible();
}
String displayDate() {
    struct tm date{}; if (!parseDate(currentDate, date)) return currentDate;
    char prefix[24]; strftime(prefix, sizeof(prefix), "%A, %b ", &date);
    return String(prefix) + String(date.tm_mday) + " " + String(date.tm_year + 1900);
}
ListModel listModel() {
    ListModel model; model.selected = selected; model.scrollTop = scrollTop; model.marqueeStartMs = marqueeStart;
    if (visible.empty()) {
        ListItemModel item; item.label = "No expenses this day"; item.isSelected = true; model.items.push_back(item);
    }
    for (int i = 0; i < (int)visible.size(); ++i) {
        const auto &e = entries[visible[i]]; ListItemModel item;
        item.label = e.name; item.value = e.value + " " + e.currency; item.type = ListItemType::Property;
        item.isSelected = i == selected; item.isDimmed = e.shared; model.items.push_back(item);
    }
    return model;
}
void drawEditor() {
    OverlayModel model; model.type = OverlayType::TwoFieldInput;
    model.title = invalidInput ? "Invalid Entry" : (editVisibleIndex >= 0 ? "Edit Entry" : "New Entry");
    model.value = editName; model.secondValue = editAmount; model.activeField = editorField;
    model.cursorIndex = editorField == 0 ? nameCursor : amountCursor;
    model.prompt = "Entry name"; model.secondPrompt = "5000 " + defaultCurrency;
    model.helperText = "[Tab]Switch [Fn L/R]Cursor"; model.confirmText = "[Esc]Close [Ok]Save"; drawOverlay(model);
}
void drawTextModal(const String &title, const String &prompt, const String &value) {
    OverlayModel model; model.type = OverlayType::TextInput; model.title = invalidInput ? "Invalid " + title : title;
    model.prompt = prompt; model.value = value; model.confirmText = "[Esc]Close   [Ok]Save"; drawOverlay(model);
}
bool splitAmount(const String &input, String &value, String &currency) {
    String text = input; text.trim(); int space = text.lastIndexOf(' '); if (space <= 0) return false;
    value = text.substring(0, space); currency = text.substring(space + 1); value.trim(); currency.trim(); currency.toUpperCase();
    if (!value.length() || !currency.length()) return false;
    for (int i = 0; i < (int)value.length(); ++i) if (value[i] < '0' || value[i] > '9') return false;
    return true;
}
void beginEditor(int index) {
    editVisibleIndex = index; editName = ""; editAmount = "5000 " + defaultCurrency;
    if (index >= 0 && index < (int)visible.size()) {
        const auto &e = entries[visible[index]]; editName = e.name; editAmount = e.value + " " + e.currency;
    }
    editorField = 0; nameCursor = editName.length(); amountCursor = editAmount.length();
    invalidInput = false; modal = Modal::Editor; drawEditor();
}
void saveEditor() {
    editName = cleanField(editName); String value, currency;
    if (!editName.length() || !splitAmount(editAmount, value, currency)) { invalidInput = true; drawEditor(); return; }
    ExpenseEntry e; e.date = currentDate; e.name = editName; e.value = value; e.currency = cleanField(currency);
    if (editVisibleIndex >= 0 && editVisibleIndex < (int)visible.size()) {
        const int actual = visible[editVisibleIndex];
        // Editing changes the serialized payload, so it must be shared again.
        e.shared = false;
        entries[actual] = e;
    } else entries.push_back(e);
    saveEntries(); modal = Modal::None; rebuildVisible(); drawExpenses();
}
bool appendToMonth(const ExpenseEntry &e, const String &target) {
    String month = target.substring(0, 7); if (month == currentMonth) return true;
    SD.mkdir("/Expenses"); File f = SD.open("/Expenses/" + month + ".txt", FILE_APPEND); if (!f) return false;
    f.printf("%s|%c|%s|%s|%s\n", target.c_str(), e.shared ? 'X' : '-', e.name.c_str(), e.value.c_str(), e.currency.c_str());
    f.close(); return true;
}
bool moveSelected(const String &target) {
    struct tm parsed{}; if (!parseDate(target, parsed) || visible.empty()) return false;
    int actual = visible[selected]; ExpenseEntry moved = entries[actual];
    moved.date = target;
    // The date is part of the QR payload.
    moved.shared = false;
    if (target.substring(0, 7) == currentMonth) entries[actual] = moved;
    else { if (!appendToMonth(moved, target)) return false; entries.erase(entries.begin() + actual); }
    saveEntries(); rebuildVisible(); return true;
}
String payloadLine(const ExpenseEntry &e) {
    return e.date + "|X|" + cleanField(e.name) + "|" + cleanField(e.value) + "|" + cleanField(e.currency);
}
void drawQr() {
    if (qrPages.empty()) { modal = Modal::None; drawExpenses(); return; }
    constexpr uint8_t version = 10;
    uint8_t buffer[qrcode_getBufferSize(version)]; QRCode qr;
    qrcode_initText(&qr, buffer, version, ECC_LOW, qrPages[qrPage].c_str());
    auto &d = M5Cardputer.Display; d.fillScreen(T->bg);
    const int scale = 2, qrPx = qr.size * scale, x0 = (240 - qrPx) / 2, y0 = 8;
    d.fillRect(x0 - 4, y0 - 4, qrPx + 8, qrPx + 8, TFT_WHITE);
    for (uint8_t y = 0; y < qr.size; ++y) for (uint8_t x = 0; x < qr.size; ++x)
        if (qrcode_getModule(&qr, x, y)) d.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, TFT_BLACK);
    d.setTextDatum(middle_center); d.setTextColor(T->accent1, T->bg);
    d.drawString("QR " + String(qrPage + 1) + "/" + String(qrPages.size()) + " [Esc]Close [,/]Page", 120, 130, &fonts::Font0);
}
}

String expensesDefaultCurrency() { return defaultCurrency; }
bool expensesHasSelection() { return !visible.empty(); }
bool expensesModalActive() { return modal != Modal::None; }
void expensesOpen() {
    rememberLastOpenedApp(HostApp::Expenses); loadDefaultCurrency(); dayOffset = 0;
    selected = scrollTop = 0; modal = Modal::None; loadEntries(); drawExpenses();
}
void drawExpenses() {
    if (foregroundApp != HostApp::Expenses) return;
    if (modal == Modal::Editor) { drawEditor(); return; }
    if (modal == Modal::MoveDate) { drawTextModal("Move to Date", "YYYY-MM-DD", moveDate); return; }
    if (modal == Modal::Currency) { drawTextModal("Edit Currency", "Default currency", currencyInput); return; }
    if (modal == Modal::Qr) { drawQr(); return; }
    HeaderModel header; header.appHeaderTag = "EXPENSES"; header.appHeaderTitle = displayDate(); header.cursor = true; drawHeader(header);
    drawList(listModel()); FooterModel footer; footer.left = "[A]Add [R]Rm"; footer.center = "[Ok]Edit";
    footer.battery = footerBatteryText(); drawFooter(footer);
}
void expensesNew() { beginEditor(-1); }
void expensesEdit() { if (!visible.empty()) beginEditor(selected); }
void expensesDelete() {
    if (visible.empty()) return; entries.erase(entries.begin() + visible[selected]);
    saveEntries(); rebuildVisible(); drawExpenses();
}
void expensesMoveTomorrow() {
    if (visible.empty()) return; struct tm date{};
    if (!parseDate(entries[visible[selected]].date, date)) return;
    date.tm_mday++; mktime(&date); moveSelected(dateKey(date)); drawExpenses();
}
void expensesPromptMoveDate() {
    if (!visible.empty()) { moveDate = currentDate; invalidInput = false; modal = Modal::MoveDate; drawExpenses(); }
}
void expensesEditDefaultCurrency() {
    currencyInput = defaultCurrency; invalidInput = false; modal = Modal::Currency; drawExpenses();
}
void expensesShareQr() {
    if (visible.empty()) return;
    qrPages.clear(); String page; constexpr int maxPayload = 220;
    for (int actual : visible) {
        String line = payloadLine(entries[actual]);
        if (page.length() && page.length() + 1 + line.length() > maxPayload) { qrPages.push_back(page); page = ""; }
        if (page.length()) page += '\n'; page += line; entries[actual].shared = true;
    }
    if (page.length()) qrPages.push_back(page);
    saveEntries(); qrPage = 0; modal = Modal::Qr; drawExpenses();
}
void cancelExpensesModal() { modal = Modal::None; drawExpenses(); }

void handleExpensesInput(Keyboard_Class::KeysState &ks) {
    if (modal == Modal::Qr) {
        if (keyboardBackPressed(ks)) { cancelExpensesModal(); return; }
        for (char c : ks.word) if (c == ',' || c == '/') {
            qrPage = (qrPage + (c == ',' ? -1 : 1) + qrPages.size()) % qrPages.size(); drawQr(); return;
        }
        return;
    }
    if (modal == Modal::MoveDate || modal == Modal::Currency) {
        String &value = modal == Modal::MoveDate ? moveDate : currencyInput;
        if (keyboardBackPressed(ks)) { cancelExpensesModal(); return; }
        if (ks.enter) {
            if (modal == Modal::MoveDate) {
                if (!moveSelected(moveDate)) { invalidInput = true; drawExpenses(); return; }
            } else {
                currencyInput.trim(); currencyInput.toUpperCase(); currencyInput = cleanField(currencyInput);
                if (!currencyInput.length()) { invalidInput = true; drawExpenses(); return; }
                defaultCurrency = currencyInput; saveDefaultCurrency();
            }
            modal = Modal::None; drawExpenses(); return;
        }
        if (ks.del && value.length()) { value.remove(value.length() - 1); invalidInput = false; drawOverlayInputValue(value); return; }
        for (char c : ks.word)
            if (keyboardTextInputChar(ks, c) && value.length() < 16 &&
                (modal == Modal::Currency || (c >= '0' && c <= '9') || c == '-')) {
                value += c; invalidInput = false; drawOverlayInputValue(value);
            }
        return;
    }
    if (modal == Modal::Editor) {
        if (keyboardBackPressed(ks)) { cancelExpensesModal(); return; }
        if (ks.tab) { editorField = 1 - editorField; drawEditor(); return; }
        if (ks.enter) { saveEditor(); return; }
        String &value = editorField == 0 ? editName : editAmount;
        int &cursor = editorField == 0 ? nameCursor : amountCursor;
        if (ks.fn) for (char c : ks.word) if (c == ',' || c == '/') {
            cursor = constrain(cursor + (c == ',' ? -1 : 1), 0, (int)value.length()); drawEditor(); return;
        }
        if (ks.del && cursor > 0) { value.remove(cursor - 1, 1); cursor--; invalidInput = false; drawEditor(); return; }
        for (char c : ks.word) if (keyboardTextInputChar(ks, c) && value.length() < 80) {
            value = value.substring(0, cursor) + String(c) + value.substring(cursor);
            cursor++; invalidInput = false; drawEditor();
        }
        return;
    }
    if (keyboardBackPressed(ks)) return;
    if (ks.enter) { expensesEdit(); return; }
    for (char c : ks.word) {
        if (c == 'a' || c == 'A') { expensesNew(); return; }
        if (c == 'r' || c == 'R') { expensesDelete(); return; }
        if (c == ',' || c == '/') {
            dayOffset += c == ',' ? -1 : 1; selected = scrollTop = 0; loadEntries(); drawExpenses(); return;
        }
        if ((c == ';' || c == '.') && !visible.empty()) {
            int old = selected, oldTop = scrollTop;
            selected = (selected + (c == ';' ? -1 : 1) + visible.size()) % visible.size();
            marqueeStart = millis(); rebuildVisible();
            if (oldTop == scrollTop) drawListSelection(listModel(), old, selected); else drawList(listModel());
            return;
        }
    }
}
