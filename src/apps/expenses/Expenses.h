#pragma once
#include <M5Cardputer.h>
void expensesOpen();
void drawExpenses();
void handleExpensesInput(Keyboard_Class::KeysState &keys);
bool expensesModalActive();
void cancelExpensesModal();
bool expensesHasSelection();
void expensesNew();
void expensesMoveTomorrow();
void expensesPromptMoveDate();
void expensesEdit();
void expensesDelete();
void expensesShareQr();
void expensesEditDefaultCurrency();
String expensesDefaultCurrency();
