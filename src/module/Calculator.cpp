#include "module/Calculator.h"

#include <cstdlib>

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/Help.h"
#include "UI/Overlay.h"
#include "UI/Themes.h"

namespace
{
String calcInput = "";
String calcDisplay = "0";
double calcAccumulator = 0.0;
char calcOperator = 0;
bool calcHasAccumulator = false;
bool calcJustCalculated = false;

constexpr int CALC_MARGIN = 15;
constexpr int CALC_X = CALC_MARGIN;
constexpr int CALC_Y = CALC_MARGIN;
constexpr int CALC_W = SCREEN_W - (CALC_MARGIN * 2);
constexpr int CALC_PAD = 8;
constexpr int CALC_BOX_X = CALC_X + CALC_PAD;
constexpr int CALC_BOX_Y = CALC_Y + 17;
constexpr int CALC_BOX_W = CALC_W - (CALC_PAD * 2);
constexpr int CALC_BOX_H = 58;
constexpr int CALC_TEXT_PAD = 6;
constexpr int CALC_OPERATOR_GAP = 12;

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

String fitCalcText(String text, int width)
{
    while (text.length() > 0 &&
           M5Cardputer.Display.textWidth(text, &fonts::Font4) > width)
    {
        text = text.substring(1);
    }

    return text;
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
    auto &D = M5Cardputer.Display;

    D.fillRect(
        CALC_BOX_X + 1,
        CALC_BOX_Y + 1,
        CALC_BOX_W - 2,
        CALC_BOX_H - 2,
        T->bg);

    D.setClipRect(
        CALC_BOX_X + 4,
        CALC_BOX_Y + 1,
        CALC_BOX_W - 8,
        CALC_BOX_H - 2);

    String op =
        operatorLabel();
    const int opWidth =
        op.length() > 0
            ? D.textWidth(op, &fonts::Font4)
            : 0;
    const int displayWidth =
        CALC_BOX_W -
        (CALC_TEXT_PAD * 2) -
        (op.length() > 0
             ? opWidth + CALC_OPERATOR_GAP
             : 0);

    if (op.length() > 0)
    {
        D.setTextDatum(middle_left);
        D.setTextColor(T->accent1, T->bg);
        D.drawString(
            op,
            CALC_BOX_X + CALC_TEXT_PAD,
            CALC_BOX_Y + (CALC_BOX_H / 2),
            &fonts::Font4);
    }

    D.setTextDatum(middle_right);
    D.setTextColor(T->textBright, T->bg);
    D.drawString(
        fitCalcText(calcDisplay, displayWidth),
        CALC_BOX_X + CALC_BOX_W - CALC_TEXT_PAD,
        CALC_BOX_Y + (CALC_BOX_H / 2),
        &fonts::Font4);

    D.clearClipRect();
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
    drawOverlayFrame("CALCULATOR");

    auto &D = M5Cardputer.Display;

    D.fillRect(
        CALC_BOX_X,
        CALC_BOX_Y,
        CALC_BOX_W,
        CALC_BOX_H,
        T->bg);

    D.drawRect(
        CALC_BOX_X,
        CALC_BOX_Y,
        CALC_BOX_W,
        CALC_BOX_H,
        T->accent2);

    drawCalculatorDisplay();

    D.setTextDatum(middle_left);
    D.setTextColor(T->textMid);
    D.drawString(
        "[A]Add [S]Sub [X]Mul [D]Div",
        CALC_X + CALC_PAD,
        CALC_BOX_Y + CALC_BOX_H + 13,
        &fonts::Font0);

    D.setTextDatum(middle_center);
    D.setTextColor(T->textDim);
    D.drawString(
        "[Esc]Clear/Close   [Ent]Calc",
        SCREEN_W / 2,
        SCREEN_H - 21,
        &fonts::Font0);
}

void handleCalculatorInput(Keyboard_Class::KeysState &ks)
{
    if (!calculatorVisible)
        return;

    if (keyboardBackPressed(ks))
    {
        if (calcInput.length() == 0 &&
            calcDisplay == "0" &&
            calcOperator == 0)
        {
            closeCalculator();
            return;
        }

        resetCalculator();
        drawCalculatorDisplay();
        return;
    }

    if (ks.enter)
    {
        calculateResult();
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

        case 'h':
        case 'H':
            toggleHelp();
            return;
        }
    }
}
