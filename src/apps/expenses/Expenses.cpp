#include "apps/expenses/Expenses.h"
#include "apps/expenses/ExpenseSyncTransport.h"
#include <algorithm>
#include <SD.h>
#include <esp_system.h>
#include <qrcode.h>
#include <time.h>
#include <vector>
#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/service/Clock.h"
#include "module/service/WiFi.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"
#include "UI/Themes.h"
#include "UI/Toast.h"

namespace {
struct ExpenseEntry { String date; bool shared = false; String id; String name; String value; String currency; };
enum class Modal { None, Editor, MoveDate, Currency, Qr, UploadResult };
std::vector<ExpenseEntry> entries;
std::vector<int> visible;
Modal modal = Modal::None;
String currentDate, currentMonth, filePath, defaultCurrency = "IDR";
int dayOffset = 0, selected = 0, scrollTop = 0;
uint32_t marqueeStart = 0;
int editVisibleIndex = -1, editorField = 0, nameCursor = 0, amountCursor = 0;
String editName, editAmount, moveDate, currencyInput;
bool invalidInput = false;
bool quickLogActive = false;
std::vector<String> qrPages;
int qrPage = 0;
String uploadResult;
bool uploadPending = false;

struct CurrencyTotal { String currency; String value; };

String addWholeAmounts(const String &left, const String &right) {
    int leftIndex = left.length() - 1, rightIndex = right.length() - 1, carry = 0;
    String result;
    result.reserve(max(left.length(), right.length()) + 1);
    while (leftIndex >= 0 || rightIndex >= 0 || carry) {
        const int leftDigit = leftIndex >= 0 ? left[leftIndex--] - '0' : 0;
        const int rightDigit = rightIndex >= 0 ? right[rightIndex--] - '0' : 0;
        const int sum = leftDigit + rightDigit + carry;
        result = String(static_cast<char>('0' + sum % 10)) + result;
        carry = sum / 10;
    }
    int leadingZeros = 0;
    while (leadingZeros + 1 < (int)result.length() && result[leadingZeros] == '0') leadingZeros++;
    return leadingZeros ? result.substring(leadingZeros) : result;
}

String formatWholeAmount(const String &value) {
    String formatted;
    formatted.reserve(value.length() + value.length() / 3);
    for (int i = 0; i < (int)value.length(); ++i) {
        if (i > 0 && ((int)value.length() - i) % 3 == 0) formatted += '.';
        formatted += value[i];
    }
    return formatted;
}

String totalText(const std::vector<int> *entryIndexes, const String &prefix) {
    std::vector<CurrencyTotal> totals;
    const int count = entryIndexes ? entryIndexes->size() : entries.size();
    for (int i = 0; i < count; ++i) {
        const ExpenseEntry &entry = entries[entryIndexes ? (*entryIndexes)[i] : i];
        bool wholeAmount = entry.value.length() > 0;
        for (int i = 0; i < (int)entry.value.length(); ++i)
            if (entry.value[i] < '0' || entry.value[i] > '9') { wholeAmount = false; break; }
        if (!wholeAmount) continue;

        CurrencyTotal *total = nullptr;
        for (auto &candidate : totals)
            if (candidate.currency.equalsIgnoreCase(entry.currency)) { total = &candidate; break; }
        if (!total) {
            totals.push_back({entry.currency, "0"});
            total = &totals.back();
        }
        total->value = addWholeAmounts(total->value, entry.value);
    }

    String text = prefix;
    if (totals.empty()) text += "0 " + defaultCurrency;
    for (int i = 0; i < (int)totals.size(); ++i) {
        if (i > 0) text += " + ";
        text += formatWholeAmount(totals[i].value) + " " + totals[i].currency;
    }
    return text;
}

String dayTotalText() {
    return totalText(&visible, "Total: ");
}

void showMonthTotal() {
    showToast(totalText(nullptr, "Month: "), 2000);
}

String makeExpenseId(const String &date) {
    String compact = date; compact.replace("-", "");
    char suffix[18]; snprintf(suffix, sizeof(suffix), "-%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    return compact + suffix;
}

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
bool parseExpenseLine(String line, ExpenseEntry &entry, bool &migrated) {
    line.trim();
    int p1 = line.indexOf('|'), p2 = line.indexOf('|', p1 + 1);
    int p3 = line.indexOf('|', p2 + 1), p4 = line.indexOf('|', p3 + 1), p5 = line.indexOf('|', p4 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) return false;
    entry = {};
    entry.date = line.substring(0, p1);
    entry.shared = line.substring(p1 + 1, p2).equalsIgnoreCase("X");
    if (p5 >= 0) {
        entry.id = line.substring(p2 + 1, p3); entry.name = line.substring(p3 + 1, p4);
        entry.value = line.substring(p4 + 1, p5); entry.currency = line.substring(p5 + 1);
    } else {
        entry.id = makeExpenseId(entry.date); entry.name = line.substring(p2 + 1, p3);
        entry.value = line.substring(p3 + 1, p4); entry.currency = line.substring(p4 + 1); migrated = true;
    }
    entry.currency.trim();
    return true;
}
bool saveExpenseFile(const String &path, const std::vector<ExpenseEntry> &fileEntries) {
    SD.mkdir("/Expenses"); SD.remove(path.c_str()); File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    for (const auto &e : fileEntries)
        f.printf("%s|%c|%s|%s|%s|%s\n", e.date.c_str(), e.shared ? 'X' : '-', e.id.c_str(), e.name.c_str(), e.value.c_str(), e.currency.c_str());
    f.close(); return true;
}
bool loadExpenseFile(const String &path, std::vector<ExpenseEntry> &fileEntries, bool &migrated) {
    fileEntries.clear(); migrated = false; File f = SD.open(path, FILE_READ);
    if (!f) return false;
    while (f.available()) {
        ExpenseEntry entry;
        if (parseExpenseLine(f.readStringUntil('\n'), entry, migrated)) fileEntries.push_back(entry);
    }
    f.close(); return true;
}
bool expenseMonthFilename(const String &path, String &month) {
    const int slash = path.lastIndexOf('/');
    const String name = slash >= 0 ? path.substring(slash + 1) : path;
    if (name.length() != 11 || name.charAt(4) != '-' || name.substring(7) != ".txt") return false;
    for (int i = 0; i < 7; ++i)
        if (i != 4 && !isDigit(name.charAt(i))) return false;
    const int year = name.substring(0, 4).toInt(), monthNumber = name.substring(5, 7).toInt();
    if (year < 2000 || year > 2099 || monthNumber < 1 || monthNumber > 12) return false;
    month = name.substring(0, 7); return true;
}
bool listExpenseFiles(std::vector<String> &paths, String &error) {
    paths.clear(); File directory = SD.open("/Expenses");
    if (!directory) return true;
    if (!directory.isDirectory()) { directory.close(); error = "/Expenses is not a directory"; return false; }
    File file = directory.openNextFile();
    while (file) {
        String month;
        if (!file.isDirectory() && expenseMonthFilename(String(file.name()), month))
            paths.push_back("/Expenses/" + month + ".txt");
        file.close(); file = directory.openNextFile();
    }
    directory.close(); std::sort(paths.begin(), paths.end()); return true;
}
bool collectPendingExpenses(const String &throughDate, std::vector<ExpenseSyncEntry> &pending,
                            bool &morePending, String &error) {
    constexpr size_t MAX_BATCH = 50;
    pending.clear(); morePending = false;
    std::vector<String> paths;
    if (!listExpenseFiles(paths, error)) return false;
    for (const String &path : paths) {
        String month;
        if (!expenseMonthFilename(path, month) || month > throughDate.substring(0, 7)) continue;
        std::vector<ExpenseEntry> fileEntries; bool migrated = false;
        if (!loadExpenseFile(path, fileEntries, migrated)) { error = "Cannot read " + path; return false; }
        if (migrated && !saveExpenseFile(path, fileEntries)) { error = "Cannot update " + path; return false; }
        for (const auto &entry : fileEntries) {
            if (entry.shared || entry.date > throughDate) continue;
            if (pending.size() < MAX_BATCH)
                pending.push_back({entry.id, entry.name, entry.value, entry.currency, entry.date});
            else
                morePending = true;
        }
    }
    return true;
}
bool markAcceptedExpenses(const std::vector<String> &acceptedIds, String &error) {
    std::vector<String> paths;
    if (!listExpenseFiles(paths, error)) return false;
    for (const String &path : paths) {
        std::vector<ExpenseEntry> fileEntries; bool migrated = false, changed = false;
        if (!loadExpenseFile(path, fileEntries, migrated)) { error = "Cannot read " + path; return false; }
        for (auto &entry : fileEntries) {
            if (entry.shared) continue;
            for (const String &acceptedId : acceptedIds)
                if (entry.id == acceptedId) { entry.shared = true; changed = true; break; }
        }
        if ((changed || migrated) && !saveExpenseFile(path, fileEntries)) { error = "Cannot update " + path; return false; }
    }
    return true;
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
    if (!saveExpenseFile(filePath, entries)) showHdrMsg("SD ERROR");
}
void loadEntries() {
    entries.clear(); updateDate(); bool migrated = false; File f = SD.open(filePath, FILE_READ);
    if (f) {
        while (f.available()) {
            ExpenseEntry entry;
            if (parseExpenseLine(f.readStringUntil('\n'), entry, migrated)) entries.push_back(entry);
        }
        f.close();
    }
    if (migrated) saveEntries();
    rebuildVisible();
}
String footerDate() {
    struct tm date{}; if (!parseDate(currentDate, date)) return currentDate;
    char value[13]; strftime(value, sizeof(value), "%a %m/%d/%y", &date);
    return String(value);
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
void drawEditor(bool inputOnly = false) {
    OverlayModel model; model.type = OverlayType::TwoFieldInput;
    model.title = invalidInput ? "Invalid Entry" : (editVisibleIndex >= 0 ? "Edit Entry" : "New Expense");
    model.value = editAmount; model.secondValue = editName; model.activeField = editorField;
    model.cursorIndex = editorField == 0 ? amountCursor : nameCursor;
    model.prompt = defaultCurrency; model.secondPrompt = "Expense name";
    model.helperText = "[Tab]Switch [Fn L/R]Cursor"; model.confirmText = "[Esc]Close [Ok]Save";
    if (inputOnly) drawOverlayTwoFieldInputValues(model);
    else drawOverlay(model);
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
    editVisibleIndex = index; editName = ""; editAmount = " " + defaultCurrency;
    if (index >= 0 && index < (int)visible.size()) {
        const auto &e = entries[visible[index]]; editName = e.name; editAmount = e.value + " " + e.currency;
    }
    editorField = 0; nameCursor = editName.length();
    amountCursor = editAmount.indexOf(' ');
    if (amountCursor < 0) amountCursor = editAmount.length();
    invalidInput = false; modal = Modal::Editor; drawEditor();
}
void saveEditor() {
    editName = cleanField(editName); String value, currency;
    if (!editName.length() || !splitAmount(editAmount, value, currency)) { invalidInput = true; drawEditor(); return; }
    ExpenseEntry e; e.date = currentDate; e.name = editName; e.value = value; e.currency = cleanField(currency);
    if (editVisibleIndex >= 0 && editVisibleIndex < (int)visible.size()) {
        const int actual = visible[editVisibleIndex];
        e.id = entries[actual].shared ? makeExpenseId(currentDate) : entries[actual].id;
        e.shared = false;
        entries[actual] = e;
    } else { e.id = makeExpenseId(currentDate); entries.push_back(e); }
    saveEntries(); rebuildVisible();
    modal = Modal::None;
    if (quickLogActive) {
        quickLogActive = false;
        drawAll();
    } else drawExpenses();
}
bool appendToMonth(const ExpenseEntry &e, const String &target) {
    String month = target.substring(0, 7); if (month == currentMonth) return true;
    SD.mkdir("/Expenses"); File f = SD.open("/Expenses/" + month + ".txt", FILE_APPEND); if (!f) return false;
    f.printf("%s|%c|%s|%s|%s|%s\n", target.c_str(), e.shared ? 'X' : '-', e.id.c_str(), e.name.c_str(), e.value.c_str(), e.currency.c_str());
    f.close(); return true;
}
bool moveSelected(const String &target) {
    struct tm parsed{}; if (!parseDate(target, parsed) || visible.empty()) return false;
    int actual = visible[selected]; ExpenseEntry moved = entries[actual];
    moved.date = target;
    if (moved.shared) moved.id = makeExpenseId(target);
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
void drawUploadResult() {
    OverlayModel model; model.type = OverlayType::Message; model.title = "PC PREVIEW";
    model.items.push_back(uploadResult); model.confirmText = "[Esc/Ok] Close"; drawOverlay(model);
}
}

String expensesDefaultCurrency() { return defaultCurrency; }
bool expensesHasSelection() { return !visible.empty(); }
bool expensesModalActive() { return modal != Modal::None; }
bool expensesQuickActive() { return quickLogActive; }
void expensesOpen() {
    rememberLastOpenedApp(HostApp::Expenses); loadDefaultCurrency(); dayOffset = 0;
    selected = scrollTop = 0; modal = Modal::None; quickLogActive = false; loadEntries(); drawExpenses();
}
void expensesQuickOpen() {
    loadDefaultCurrency(); dayOffset = 0; selected = scrollTop = 0; loadEntries();
    quickLogActive = true; beginEditor(-1);
}
void drawExpenses() {
    if (foregroundApp != HostApp::Expenses) return;
    if (modal == Modal::Editor) { drawEditor(); return; }
    if (modal == Modal::MoveDate) { drawTextModal("Move to Date", "YYYY-MM-DD", moveDate); return; }
    if (modal == Modal::Currency) { drawTextModal("Edit Currency", "Default currency", currencyInput); return; }
    if (modal == Modal::Qr) { drawQr(); return; }
    if (modal == Modal::UploadResult) { drawUploadResult(); return; }
    HeaderModel header; header.appHeaderTag = "EXPENSE"; header.appHeaderTitle = dayTotalText(); header.cursor = true; drawHeader(header);
    drawList(listModel()); FooterModel footer; footer.left = "[A]Add [R]Rm"; footer.center = footerDate();
    footer.battery = footerBatteryText(); drawFooter(footer);
}
void expensesNew() { beginEditor(-1); }
void expensesEdit() { if (!visible.empty()) beginEditor(selected); }
void expensesDelete() {
    if (visible.empty()) return; entries.erase(entries.begin() + visible[selected]);
    saveEntries(); rebuildVisible(); drawExpenses();
}
void expensesToggleSynced() {
    if (visible.empty()) return;
    ExpenseEntry &entry = entries[visible[selected]];
    entry.shared = !entry.shared; saveEntries(); drawListRow(listModel(), selected);
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
void expensesUploadPending() {
    struct tm today{}; String throughDate = currentDate;
    if (getCurrentTime(today)) throughDate = dateKey(today);
    std::vector<ExpenseSyncEntry> pending;
    bool morePending = false; String error;
    if (!collectPendingExpenses(throughDate, pending, morePending, error)) {
        uploadResult = error; modal = Modal::UploadResult; drawExpenses(); return;
    }
    if (pending.empty()) {
        uploadResult = "All past expenses already processed";
        modal = Modal::UploadResult; drawExpenses(); return;
    }
    ExpenseSyncConfig config;
    if (!loadExpenseSyncConfig(config, error)) {
        uploadResult = error; modal = Modal::UploadResult; drawExpenses(); return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        uploadPending = true;
        if (ensureWifiConnected() != WifiStartupResult::Connected) return;
        uploadPending = false;
    }
    OverlayModel progress; progress.type = OverlayType::Message; progress.title = "PC PREVIEW";
    progress.items.push_back("Uploading " + String(pending.size()) + " expenses..."); progress.confirmText = "Please wait"; drawOverlay(progress);
    std::vector<String> acceptedIds;
    uploadExpenseBatch(config, pending, acceptedIds, uploadResult);
    if (!acceptedIds.empty()) {
        String saveError;
        if (!markAcceptedExpenses(acceptedIds, saveError)) uploadResult += "; " + saveError;
        loadEntries();
    }
    if (morePending && acceptedIds.size() == pending.size()) uploadResult += "; more pending";
    modal = Modal::UploadResult; drawExpenses();
}
bool expensesResumePendingUpload() {
    if (!uploadPending) return false;
    uploadPending = false;
    expensesUploadPending();
    return true;
}
void expensesCancelPendingUpload() { uploadPending = false; }
void expensesShareQr() {
    qrPages.clear(); String page; constexpr int maxPayload = 220;
    for (int actual = 0; actual < (int)entries.size(); ++actual) {
        if (entries[actual].shared || entries[actual].date.substring(0, 7) != currentMonth) continue;
        String line = payloadLine(entries[actual]);
        if (page.length() && page.length() + 1 + line.length() > maxPayload) { qrPages.push_back(page); page = ""; }
        if (page.length()) page += '\n'; page += line; entries[actual].shared = true;
    }
    if (page.length()) qrPages.push_back(page);
    saveEntries(); qrPage = 0; modal = Modal::Qr; drawExpenses();
}
void cancelExpensesModal() {
    modal = Modal::None;
    if (quickLogActive) {
        quickLogActive = false;
        drawAll();
    } else drawExpenses();
}

void handleExpensesInput(Keyboard_Class::KeysState &ks) {
    if (modal == Modal::UploadResult) {
        if (keyboardBackPressed(ks) || ks.enter) cancelExpensesModal();
        return;
    }
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
        if (ks.tab) { editorField = 1 - editorField; drawEditor(true); return; }
        if (ks.enter) { saveEditor(); return; }
        String &value = editorField == 0 ? editAmount : editName;
        int &cursor = editorField == 0 ? amountCursor : nameCursor;
        if (ks.fn) for (char c : ks.word) if (c == ',' || c == '/') {
            cursor = constrain(cursor + (c == ',' ? -1 : 1), 0, (int)value.length()); drawEditor(true); return;
        }
        if (ks.del && cursor > 0) {
            const bool redrawFrame = invalidInput;
            value.remove(cursor - 1, 1); cursor--; invalidInput = false; drawEditor(!redrawFrame); return;
        }
        for (char c : ks.word) if (keyboardTextInputChar(ks, c) && value.length() < 80) {
            const bool redrawFrame = invalidInput;
            value = value.substring(0, cursor) + String(c) + value.substring(cursor);
            cursor++; invalidInput = false; drawEditor(!redrawFrame);
        }
        return;
    }
    if (keyboardBackPressed(ks)) return;
    if (ks.enter) { expensesEdit(); return; }
    for (char c : ks.word) {
        int shortcutTarget = listVisibleShortcutTarget(c, scrollTop, visible.size());
        if (shortcutTarget >= 0) {
            selected = shortcutTarget; marqueeStart = millis();
            expensesEdit(); return;
        }
        if (c == 'a' || c == 'A' || c == 'e' || c == 'E') { expensesNew(); return; }
        if (c == 'r' || c == 'R') { expensesDelete(); return; }
        if (c == 't' || c == 'T') { showMonthTotal(); return; }
        if (c == 'x' || c == 'X') { expensesToggleSynced(); return; }
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
