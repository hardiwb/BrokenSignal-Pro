#include "apps/expenses/ExpensesApp.h"
#include "apps/expenses/Expenses.h"
void openExpensesApp() { expensesOpen(); }
bool handleExpensesAppInput(Keyboard_Class::KeysState &keys) { handleExpensesInput(keys); return true; }
void tickExpensesApp() {}
