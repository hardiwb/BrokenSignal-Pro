#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>
#include <vector>

#include "UI/List.h"
#include "UI/Overlay.h"

namespace CalculatorInternal
{
struct CalculationStep
{
    char op;
    double value;
};

extern String calcInput;
extern String calcDisplay;
extern double calcAccumulator;
extern char calcOperator;
extern bool calcHasAccumulator;
extern bool calcJustCalculated;
extern bool calculatorHistoryVisible;
extern bool calcEditingHistory;
extern double calcInitialValue;
extern bool calcHasInitialValue;
extern int calcHistorySelected;
extern int calcHistoryScrollTop;
extern int calcHistoryEditIndex;
extern bool calcHistoryEditIsNew;
extern bool calcHistoryEditIsInitial;
extern std::vector<CalculationStep> calcHistory;

String formatNumber(double value);
String formatNumberForDisplay(const String &number);
double inputValue();
void resetCalculator();
bool applyCalculationStep(double &result, char op, double rhs);
int historyDisplayCount();
int historyStepIndexFromDisplay(int displayIndex);
int historyDisplayIndexFromStepIndex(int stepIndex);
void ensureHistorySelectionVisible();
void recalculateHistory();
void appendHistoryStep(char op, double value);
void deleteCalcInput();
void applyOperator();
void setOperator(char op);
void calculateResult();
void appendCalcChar(char c);
void beginHistoryEdit(bool isNew = false);
void insertHistoryStep(int index, char op);
void cancelHistoryEdit();
void saveHistoryEdit();
void removeSelectedHistoryStep();
void changeSelectedHistoryOperator(int direction);
void handleCalculationHistoryInput(Keyboard_Class::KeysState &ks);

OverlayModel calculatorOverlayModel();
ListModel calculationHistoryListModel();
void drawCalculatorDisplay();
void drawCalculationHistory();
}
