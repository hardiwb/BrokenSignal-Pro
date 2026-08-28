#include "apps/calculator/Calculator.h"
#include "apps/calculator/CalculatorInternal.h"

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/shell/Help.h"
#include "UI/Overlay.h"

namespace CalculatorInternal
{
void handleCalculationHistoryInput(Keyboard_Class::KeysState &ks)
{
    if (keyboardBackPressed(ks))
    {
        closeCalculator();
        return;
    }

    if (ks.enter)
    {
        beginHistoryEdit();
        return;
    }

    if (ks.del)
    {
        removeSelectedHistoryStep();
        return;
    }

    for (auto c : ks.word)
    {
        if (c == 'f' || c == 'F')
        {
            calculatorHistoryVisible = false;
            drawOverlay(calculatorOverlayModel());
            return;
        }

        if (c == 'h' || c == 'H')
        {
            toggleHelp();
            return;
        }

        if (c == 'a' || c == 'A')
        {
            insertHistoryStep(calcHistory.size(), '+');
            return;
        }
        if (c == 's' || c == 'S')
        {
            insertHistoryStep(calcHistory.size(), '-');
            return;
        }
        if (c == 'm' || c == 'M')
        {
            insertHistoryStep(calcHistory.size(), 'x');
            return;
        }
        if (c == 'd' || c == 'D')
        {
            insertHistoryStep(calcHistory.size(), '/');
            return;
        }
        if (c == 'i' || c == 'I')
        {
            const int stepIndex = historyStepIndexFromDisplay(calcHistorySelected);
            const char op = (stepIndex >= 0 && stepIndex < (int)calcHistory.size())
                                ? calcHistory[stepIndex].op
                                : '+';
            insertHistoryStep(stepIndex < 0 ? 0 : stepIndex, op);
            return;
        }
        if (c == ';' && historyDisplayCount() > 0)
        {
            calcHistorySelected =
                (calcHistorySelected - 1 + historyDisplayCount()) % historyDisplayCount();
            ensureHistorySelectionVisible();
            drawCalculationHistory();
            return;
        }
        if (c == '.' && historyDisplayCount() > 0)
        {
            calcHistorySelected = (calcHistorySelected + 1) % historyDisplayCount();
            ensureHistorySelectionVisible();
            drawCalculationHistory();
            return;
        }
        if (c == '+' || c == '=')
        {
            changeSelectedHistoryOperator(+1);
            return;
        }
        if (c == '-')
        {
            changeSelectedHistoryOperator(-1);
            return;
        }
    }
}
} // namespace CalculatorInternal

using namespace CalculatorInternal;

void handleCalculatorInput(Keyboard_Class::KeysState &ks)
{
    if (!calculatorVisible)
        return;

    if (calculatorHistoryVisible)
    {
        handleCalculationHistoryInput(ks);
        return;
    }

    if (calcEditingHistory)
    {
        if (keyboardBackPressed(ks))
        {
            cancelHistoryEdit();
            return;
        }
        if (ks.enter)
        {
            saveHistoryEdit();
            return;
        }

        if (ks.del)
        {
            if (calcInput.length() > 0)
            {
                calcInput.remove(calcInput.length() - 1);
                calcDisplay = calcInput.length() > 0 ? calcInput : "0";
                drawCalculatorDisplay();
            }
            return;
        }

        for (auto c : ks.word)
        {
            if ((c >= '0' && c <= '9') || c == '.')
            {
                appendCalcChar(c);
                return;
            }

            char op = 0;
            if (c == 'a' || c == 'A' || c == '+' || c == '=')
                op = '+';
            else if (c == 's' || c == 'S' || c == '-')
                op = '-';
            else if (c == 'x' || c == 'X' || c == '*')
                op = 'x';
            else if (c == 'd' || c == 'D' || c == '/')
                op = '/';

            if (op != 0 && calcHistoryEditIndex >= 0 &&
                calcHistoryEditIndex < (int)calcHistory.size())
            {
                calcOperator = op;
                drawCalculatorDisplay();
                return;
            }
        }
        return;
    }

    if (keyboardBackPressed(ks))
    {
        deleteCalcInput();
        return;
    }

    if (ks.enter)
    {
        calculateResult();
        return;
    }

    if (ks.del)
    {
        deleteCalcInput();
        return;
    }

    for (auto c : ks.word)
    {
        if (c == 'f' || c == 'F')
        {
            if (calculatorOverlayMode)
            {
                calculatorOverlayMode = false;
                notesMode = false;
                rememberLastOpenedApp(HostApp::Calculator);
            }
            calculatorHistoryVisible = true;
            ensureHistorySelectionVisible();
            drawCalculationHistory();
            return;
        }

        if (c == 'h' || c == 'H')
        {
            if (!calculatorOverlayMode)
                toggleHelp();
            return;
        }

        if (c >= '0' && c <= '9')
        {
            appendCalcChar(c);
            return;
        }

        if (c == '.')
        {
            appendCalcChar(c);
            return;
        }

        switch (c)
        {
        case 'a':
        case 'A':
        case '+':
        case '=':
            setOperator('+');
            return;

        case 's':
        case 'S':
        case '-':
            setOperator('-');
            return;

        case 'x':
        case 'X':
        case '*':
            setOperator('x');
            return;

        case 'd':
        case 'D':
        case '/':
            setOperator('/');
            return;

        case 'c':
        case 'C':
            closeCalculator();
            return;
        }
    }
}
