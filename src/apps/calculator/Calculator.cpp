#include "apps/calculator/Calculator.h"
#include "apps/calculator/CalculatorInternal.h"

#include <cstdlib>
#include <cmath>

#include "core/State.h"
#include "core/System.h"

namespace CalculatorInternal
{
String calcInput = "";
String calcDisplay = "0";
double calcAccumulator = 0.0;
char calcOperator = 0;
bool calcHasAccumulator = false;
bool calcJustCalculated = false;
bool calculatorHistoryVisible = false;
bool calcEditingHistory = false;
double calcInitialValue = 0.0;
bool calcHasInitialValue = false;
int calcHistorySelected = 0;
int calcHistoryScrollTop = 0;
int calcHistoryEditIndex = -1;
bool calcHistoryEditIsNew = false;
bool calcHistoryEditIsInitial = false;

std::vector<CalculationStep> calcHistory;

String formatNumber(double value)
{
    const int places = calculatorDecimalPlaces < 0
                           ? 6
                           : calculatorDecimalPlaces;
    static const double factors[] = {
        1.0,
        10.0,
        100.0,
        1000.0,
        10000.0,
        100000.0,
        1000000.0};
    const double factor = factors[constrain(places, 0, 6)];
    const double scaled = fabs(value) * factor;
    const double whole = floor(scaled);
    const double fraction = scaled - whole;
    constexpr double tieTolerance = 1e-9;

    double roundedMagnitude = whole;
    if (calculatorRoundingMode == 0)
    {
        if (fraction >= 0.5 - tieTolerance)
            roundedMagnitude += 1.0;
    }
    else if (calculatorRoundingMode == 1)
    {
        if (fraction > 0.5 + tieTolerance ||
            (fabs(fraction - 0.5) <= tieTolerance && fmod(whole, 2.0) >= 1.0))
            roundedMagnitude += 1.0;
    }

    double rounded = roundedMagnitude / factor;
    if (value < 0.0)
        rounded = -rounded;
    if (rounded == 0.0)
        rounded = 0.0;

    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", places, rounded);

    String result = String(buf);
    while (calculatorDecimalPlaces < 0 &&
           result.indexOf('.') >= 0 &&
           result.endsWith("0"))
    {
        result.remove(result.length() - 1);
    }

    if (result.endsWith("."))
        result.remove(result.length() - 1);

    if (result.length() == 0)
        return "0";

    return result;
}

String formatNumberForDisplay(const String &number)
{
    if (!calculatorThousandsSeparator)
        return number;

    if (number.length() == 0)
        return "0";

    int integerStart = number[0] == '-' ? 1 : 0;
    int decimalIndex = number.indexOf('.');
    int integerEnd = decimalIndex >= 0 ? decimalIndex : number.length();

    if (integerStart >= integerEnd)
        return number;

    for (int i = integerStart; i < integerEnd; ++i)
    {
        if (number[i] < '0' || number[i] > '9')
            return number;
    }

    String result = number.substring(0, integerStart);
    for (int i = integerStart; i < integerEnd; ++i)
    {
        if (i > integerStart && (integerEnd - i) % 3 == 0)
            result += ',';
        result += number[i];
    }

    if (decimalIndex >= 0)
        result += number.substring(decimalIndex);

    return result;
}

double inputValue()
{
    if (calcInput.length() == 0 ||
        calcInput == "-")
        return 0.0;

    return atof(calcInput.c_str());
}

void resetCalculator()
{
    calcInput = "";
    calcDisplay = "0";
    calcAccumulator = 0.0;
    calcOperator = 0;
    calcHasAccumulator = false;
    calcJustCalculated = false;
    calculatorHistoryVisible = false;
    calcEditingHistory = false;
    calcInitialValue = 0.0;
    calcHasInitialValue = false;
    calcHistorySelected = 0;
    calcHistoryScrollTop = 0;
    calcHistoryEditIndex = -1;
    calcHistoryEditIsNew = false;
    calcHistoryEditIsInitial = false;
    calcHistory.clear();
}

bool applyCalculationStep(double &result, char op, double rhs)
{
    switch (op)
    {
    case '+':
        result += rhs;
        return true;
    case '-':
        result -= rhs;
        return true;
    case 'x':
        result *= rhs;
        return true;
    case '/':
        if (rhs == 0.0)
            return false;
        result /= rhs;
        return true;
    default:
        return false;
    }
}

int historyDisplayCount();
int historyStepIndexFromDisplay(int displayIndex);
int historyDisplayIndexFromStepIndex(int stepIndex);

void ensureHistorySelectionVisible()
{
    const int displayCount = (calcHasInitialValue ? 1 : 0) + (int)calcHistory.size();
    if (displayCount == 0)
    {
        calcHistorySelected = 0;
        calcHistoryScrollTop = 0;
        return;
    }

    calcHistorySelected = constrain(calcHistorySelected, 0, displayCount - 1);
    if (calcHistorySelected < calcHistoryScrollTop)
        calcHistoryScrollTop = calcHistorySelected;
    if (calcHistorySelected >= calcHistoryScrollTop + LIST_VISIBLE_ITEM)
        calcHistoryScrollTop = calcHistorySelected - LIST_VISIBLE_ITEM + 1;
}

int historyDisplayCount()
{
    return (calcHasInitialValue ? 1 : 0) + (int)calcHistory.size();
}

int historyStepIndexFromDisplay(int displayIndex)
{
    return calcHasInitialValue ? displayIndex - 1 : displayIndex;
}

int historyDisplayIndexFromStepIndex(int stepIndex)
{
    return calcHasInitialValue ? stepIndex + 1 : stepIndex;
}

void recalculateHistory()
{
    if (!calcHasInitialValue)
    {
        calcAccumulator = 0.0;
        calcDisplay = "0";
        calcHasAccumulator = false;
        return;
    }

    double result = calcInitialValue;
    for (const CalculationStep &step : calcHistory)
    {
        if (!applyCalculationStep(result, step.op, step.value))
        {
            calcDisplay = "ERR DIV 0";
            return;
        }
    }

    calcAccumulator = result;
    calcDisplay = formatNumber(result);
    calcInput = "";
    calcOperator = '=';
    calcHasAccumulator = true;
    calcJustCalculated = true;
}

void appendHistoryStep(char op, double value)
{
    constexpr int maxHistory = 32;
    if ((int)calcHistory.size() >= maxHistory)
    {
        applyCalculationStep(calcInitialValue, calcHistory.front().op, calcHistory.front().value);
        calcHistory.erase(calcHistory.begin());
    }
    calcHistory.push_back({op, value});
}

void deleteCalcInput()
{
    if (calcInput.length() > 0)
    {
        calcInput.remove(calcInput.length() - 1);
        calcDisplay = calcInput.length() > 0 ? calcInput : "0";
        drawCalculatorDisplay();
        return;
    }

    if (calcOperator != 0 || calcHasAccumulator || calcDisplay != "0")
    {
        resetCalculator();
        drawCalculatorDisplay();
        return;
    }

    closeCalculator();
}

void applyOperator()
{
    double rhs = inputValue();

    if (!calcHasAccumulator)
    {
        calcAccumulator = rhs;
        calcHasAccumulator = true;
        calcInitialValue = rhs;
        calcHasInitialValue = true;
    }
    else
    {
        if (calcOperator == '/' && rhs == 0.0)
        {
            calcDisplay = "ERR DIV 0";
            calcInput = "";
            calcJustCalculated = true;
            return;
        }

        if (applyCalculationStep(calcAccumulator, calcOperator, rhs))
            appendHistoryStep(calcOperator, rhs);
    }

    calcDisplay = formatNumber(calcAccumulator);
    calcInput = "";
    calcJustCalculated = true;
}

void setOperator(char op)
{
    if (calcInput.length() > 0 ||
        !calcHasAccumulator)
    {
        applyOperator();
    }

    calcOperator = op;
    calcJustCalculated = false;
    drawCalculatorDisplay();
}

void calculateResult()
{
    applyOperator();
    calcOperator = '=';
    drawCalculatorDisplay();
}

void appendCalcChar(char c)
{
    if (calcJustCalculated)
    {
        calcInput = "";
        calcJustCalculated = false;
        if (calcOperator == '=')
        {
            calcHasAccumulator = false;
            calcOperator = 0;
            calcHasInitialValue = false;
            calcHistory.clear();
            calcHistorySelected = 0;
            calcHistoryScrollTop = 0;
        }
    }

    if (c >= '0' && c <= '9' &&
        calcInput == "0" &&
        calcInput.indexOf('.') < 0)
    {
        calcInput = "";
    }

    if (c == '.' &&
        calcInput.indexOf('.') >= 0)
        return;

    if (c == '.' &&
        calcInput.length() == 0)
    {
        calcInput = "0";
    }

    if (calcInput.length() >= 18)
        return;

    calcInput += c;
    calcDisplay = calcInput;
    drawCalculatorDisplay();
}

void beginHistoryEdit(bool isNew)
{
    if (historyDisplayCount() == 0)
        return;

    calcHistoryEditIsNew = isNew;
    calcHistoryEditIsInitial = calcHasInitialValue && calcHistorySelected == 0;
    calcEditingHistory = true;
    calculatorHistoryVisible = false;
    if (calcHistoryEditIsInitial)
    {
        calcHistoryEditIndex = -1;
        calcInput = formatNumber(calcInitialValue);
        calcDisplay = calcInput;
        calcOperator = 0;
        calcJustCalculated = false;
        drawOverlay(calculatorOverlayModel());
        return;
    }

    calcHistoryEditIndex = historyStepIndexFromDisplay(calcHistorySelected);
    if (calcHistoryEditIndex < 0 || calcHistoryEditIndex >= (int)calcHistory.size())
    {
        calcEditingHistory = false;
        calcHistoryEditIndex = -1;
        calcHistoryEditIsNew = false;
        calcHistoryEditIsInitial = false;
        calculatorHistoryVisible = true;
        return;
    }

    calcInput = formatNumber(calcHistory[calcHistoryEditIndex].value);
    calcDisplay = calcInput;
    calcOperator = calcHistory[calcHistoryEditIndex].op;
    calcJustCalculated = false;
    drawOverlay(calculatorOverlayModel());
}

void insertHistoryStep(int index, char op)
{
    if (!calcHasInitialValue)
    {
        calcInitialValue = 0.0;
        calcHasInitialValue = true;
        calcHasAccumulator = true;
    }

    index = constrain(index, 0, (int)calcHistory.size());
    calcHistory.insert(calcHistory.begin() + index, {op, 0.0});
    calcHistorySelected = historyDisplayIndexFromStepIndex(index);
    ensureHistorySelectionVisible();
    recalculateHistory();
    beginHistoryEdit(true);
}

void cancelHistoryEdit()
{
    if (calcHistoryEditIsNew && !calcHistoryEditIsInitial &&
        calcHistoryEditIndex >= 0 &&
        calcHistoryEditIndex < (int)calcHistory.size())
    {
        calcHistory.erase(calcHistory.begin() + calcHistoryEditIndex);
        calcHistorySelected = min(calcHistorySelected, max(0, historyDisplayCount() - 1));
        ensureHistorySelectionVisible();
    }
    calcEditingHistory = false;
    calcHistoryEditIndex = -1;
    calcHistoryEditIsNew = false;
    calcHistoryEditIsInitial = false;
    calculatorHistoryVisible = true;
    recalculateHistory();
    drawCalculationHistory();
}

void saveHistoryEdit()
{
    if (calcHistoryEditIsInitial)
    {
        if (calcInput.length() > 0 && calcInput != "-")
            calcInitialValue = inputValue();
    }
    else if (calcHistoryEditIndex >= 0 &&
        calcHistoryEditIndex < (int)calcHistory.size() &&
        calcInput.length() > 0 && calcInput != "-")
    {
        calcHistory[calcHistoryEditIndex].value = inputValue();
        calcHistory[calcHistoryEditIndex].op = calcOperator;
    }
    calcHistoryEditIsNew = false;
    calcHistoryEditIsInitial = false;
    cancelHistoryEdit();
}

void removeSelectedHistoryStep()
{
    if (historyDisplayCount() == 0)
        return;

    if (calcHasInitialValue && calcHistorySelected == 0)
    {
        calcInitialValue = 0.0;
    }
    else
    {
        const int stepIndex = historyStepIndexFromDisplay(calcHistorySelected);
        if (stepIndex >= 0 && stepIndex < (int)calcHistory.size())
            calcHistory.erase(calcHistory.begin() + stepIndex);
    }

    calcHistorySelected = min(calcHistorySelected, max(0, historyDisplayCount() - 1));
    ensureHistorySelectionVisible();
    recalculateHistory();
    drawCalculationHistory();
}

void changeSelectedHistoryOperator(int direction)
{
    if (historyDisplayCount() == 0)
        return;

    const int stepIndex = historyStepIndexFromDisplay(calcHistorySelected);
    if (stepIndex < 0 || stepIndex >= (int)calcHistory.size())
        return;

    static const char operators[] = {'+', '-', '/', 'x'};
    int index = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (operators[i] == calcHistory[stepIndex].op)
        {
            index = i;
            break;
        }
    }
    calcHistory[stepIndex].op = operators[(index + direction + 4) % 4];
    recalculateHistory();
    drawCalculationHistory();
}

} // namespace CalculatorInternal

using namespace CalculatorInternal;

void openCalculator()
{
    calculatorOverlayMode = true;
    calculatorVisible = true;
    resetCalculator();
    drawCalculator();
}

void openCalculatorHistory()
{
    calculatorOverlayMode = false;
    notesMode = false;
    rememberLastOpenedApp(HostApp::Calculator);
    calculatorVisible = true;
    resetCalculator();
    calculatorHistoryVisible = true;
    calcInitialValue = 0.0;
    calcHasInitialValue = true;
    calcHasAccumulator = true;
    drawCalculationHistory();
}

void closeCalculator()
{
    const bool wasOverlay = calculatorOverlayMode;
    calculatorVisible = false;
    calculatorOverlayMode = false;
    resetCalculator();

    if (!wasOverlay)
        rememberLastOpenedApp(webRadioMode ? HostApp::Radio : HostApp::Music);

    drawAll();
}

bool calculatorInputActive()
{
    return calculatorVisible;
}

bool calculatorOverlayActive()
{
    return calculatorVisible && calculatorOverlayMode;
}

bool calculatorEditActive()
{
    return calculatorVisible && calcEditingHistory;
}

void cancelCalculatorEdit()
{
    if (calcEditingHistory)
        cancelHistoryEdit();
}

void refreshCalculatorFormatting()
{
    if (calcDisplay.startsWith("ERR"))
        return;

    if (calcInput.length() > 0)
        calcDisplay = calcInput;
    else if (calcHasAccumulator)
        calcDisplay = formatNumber(calcAccumulator);
    else
        calcDisplay = "0";
}
