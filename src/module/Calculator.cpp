#include "module/Calculator.h"

#include <cstdlib>

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/Help.h"
#include "UI/Overlay.h"

namespace
{
String calcInput = "";
String calcDisplay = "0";
double calcAccumulator = 0.0;
char calcOperator = 0;
bool calcHasAccumulator = false;
bool calcJustCalculated = false;

String formatNumber(double value)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%.6f", value);

    String result = String(buf);
    while (result.indexOf('.') >= 0 &&
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

double inputValue()
{
    if (calcInput.length() == 0 ||
        calcInput == "-")
        return 0.0;

    return atof(calcInput.c_str());
}

String operatorLabel()
{
    if (calcOperator == 0)
        return "";

    return String(calcOperator);
}

OverlayModel calculatorOverlayModel()
{
    OverlayModel model;
    model.type = OverlayType::TwoColumnInput;
    model.title = "CALCULATOR";
    model.leftValue = operatorLabel();
    model.value = calcDisplay;
    model.helperText = "[A]Add [S]Sub [X]Mul [D]Div";
    model.confirmText = "[Esc]Clear/Close   [Ent]Calc";
    model.inputFont = OverlayFontSize::Large;
    return model;
}

void resetCalculator()
{
    calcInput = "";
    calcDisplay = "0";
    calcAccumulator = 0.0;
    calcOperator = 0;
    calcHasAccumulator = false;
    calcJustCalculated = false;
}

void drawCalculatorDisplay()
{
    drawOverlayTwoColumnInputValue(calculatorOverlayModel());
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
    }
    else
    {
        switch (calcOperator)
        {
        case '+':
            calcAccumulator += rhs;
            break;
        case '-':
            calcAccumulator -= rhs;
            break;
        case 'x':
            calcAccumulator *= rhs;
            break;
        case '/':
            if (rhs == 0.0)
            {
                calcDisplay = "ERR DIV 0";
                calcInput = "";
                calcJustCalculated = true;
                return;
            }
            calcAccumulator /= rhs;
            break;
        default:
            calcAccumulator = rhs;
            break;
        }
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
        }
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
} // namespace

void openCalculator()
{
    calculatorVisible = true;
    resetCalculator();
    drawCalculator();
}

void closeCalculator()
{
    calculatorVisible = false;
    resetCalculator();
    drawAll();
}

bool calculatorInputActive()
{
    return calculatorVisible;
}

void drawCalculator()
{
    drawOverlay(calculatorOverlayModel());
}

void handleCalculatorInput(Keyboard_Class::KeysState &ks)
{
    if (!calculatorVisible)
        return;

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
