#include "apps/calculator/CalculatorMetadata.h"

#include "apps/calculator/Calculator.h"

const HelpEntry CALCULATOR_HELP_ENTRIES[] = {
    {"[Opt]", "Toggle Options"},
    {"[0-9/.]", "Enter number"},
    {"[A/+]", "Add"},
    {"[S/-]", "Subtract"},
    {"[M]", "Add multiply row"},
    {"[X/*]", "Multiply"},
    {"[D//]", "Divide"},
    {"[Ok]", "Calculate"},
    {"[F]", "Toggle full screen"},
    {"[-/+]", "Volume in history"},
    {"[H]", "Toggle Help"},
    {"[N]", "Quick note"},
    {"[Del]", "Backspace"},
    {"[Esc]", "Close / Applications"},
    {"[C]", "Close calculator"},
};

const uint8_t CALCULATOR_HELP_COUNT =
    sizeof(CALCULATOR_HELP_ENTRIES) / sizeof(CALCULATOR_HELP_ENTRIES[0]);

namespace
{
String decimalLabel()
{
    return calculatorDecimalPlaces < 0 ? "Auto" : String(calculatorDecimalPlaces);
}

String roundingLabel()
{
    static const char *labels[] = {"Half Up", "Half Even", "Truncate"};
    return labels[min((int)calculatorRoundingMode, 2)];
}

void markSettingsChanged()
{
    refreshCalculatorFormatting();
    settingsDirty = true;
    settingsDirtyMs = millis();
}

void adjustDecimals(int direction)
{
    static const int8_t places[] = {-1, 0, 2, 4, 6};
    constexpr int count = sizeof(places) / sizeof(places[0]);
    int index = 0;
    for (int i = 0; i < count; ++i)
    {
        if (places[i] == calculatorDecimalPlaces)
        {
            index = i;
            break;
        }
    }
    calculatorDecimalPlaces = places[(index + direction + count) % count];
    markSettingsChanged();
}

void adjustRounding(int direction)
{
    calculatorRoundingMode =
        (calculatorRoundingMode + direction + 3) % 3;
    markSettingsChanged();
}

void adjustThousands(int)
{
    calculatorThousandsSeparator = !calculatorThousandsSeparator;
    markSettingsChanged();
}
} // namespace

void buildCalculatorOptions(std::vector<AppOption> &options)
{
    options.push_back({"Decimal Places", decimalLabel(), true, true, false, adjustDecimals});
    options.push_back({"Rounding", roundingLabel(), true, true, false, adjustRounding});
    options.push_back({"Thousands", calculatorThousandsSeparator ? "On" : "Off", true, true, false, adjustThousands});
}
