#include "apps/expenses/ExpensesMetadata.h"
#include "apps/expenses/Expenses.h"
const HelpEntry EXPENSES_HELP_ENTRIES[] = {
    {"[Opt]", "Toggle Options"}, {"[Alt]", "Toggle Applications"},
    {"[A]", "Add expense"}, {"[R]", "Remove expense"}, {"[T]", "Displayed day total"},
    {"[X]", "Toggle expense hidden"}, {"[Ok]", "Edit expense"},
    {"[;/.]", "Cursor up / down"}, {"[,/]", "Previous / next date"},
    {"[Tab]", "Switch editor field"}, {"[Fn L/R]", "Move text cursor"}, {"[Esc]", "Applications"}};
const uint8_t EXPENSES_HELP_COUNT = sizeof(EXPENSES_HELP_ENTRIES) / sizeof(EXPENSES_HELP_ENTRIES[0]);
namespace {
void moveTomorrow(int) { expensesMoveTomorrow(); }
void moveDate(int) { expensesPromptMoveDate(); }
void editEntry(int) { expensesEdit(); }
void deleteEntry(int) { expensesDelete(); }
void uploadEntry(int) { expensesUploadSelected(); }
void shareQr(int) { expensesShareQr(); }
void editCurrency(int) { expensesEditDefaultCurrency(); }
}
void buildExpensesOptions(std::vector<AppOption> &options) {
    const bool selected = expensesHasSelection();
    options.push_back({"Move to Tomorrow", "", selected, false, true, moveTomorrow});
    options.push_back({"Move to Date", "", selected, false, true, moveDate});
    options.push_back({"Edit Entry", "", selected, false, true, editEntry});
    options.push_back({"Delete Entry", "", selected, false, true, deleteEntry});
    options.push_back({"Upload Entry", "", selected, false, true, uploadEntry});
    options.push_back({"Share as QR", "", selected, false, true, shareQr});
    options.push_back({"Default currency", expensesDefaultCurrency(), true, false, true, editCurrency});
}
