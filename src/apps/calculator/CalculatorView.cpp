#include "apps/calculator/Calculator.h"
#include "apps/calculator/CalculatorInternal.h"

#include "core/State.h"
#include "core/System.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"

namespace CalculatorInternal
{
namespace
{
String operatorLabel()
{
    if (calcOperator == 0)
        return "";

    return String(calcOperator);
}
} // namespace

OverlayModel calculatorOverlayModel()
{
    OverlayModel model;
    model.type = OverlayType::TwoColumnInput;
    model.title = calcEditingHistory ? "Edit Calculation" : "CALCULATOR";
    model.leftValue = operatorLabel();
    model.value = formatNumberForDisplay(calcDisplay);
    model.helperText = "[A]Add [S]Sub [X]Mul [D]Div";
    model.confirmText = calcEditingHistory
                            ? "[Esc]Cancel [Ok]Save"
                            : calculatorOverlayMode
                                  ? "[Esc]Close [F]Full [Ok]="
                                  : "[Esc]Apps [F]Full [H]Help [Ok]=";
    model.inputFont = OverlayFontSize::Large;
    return model;
}

ListModel calculationHistoryListModel()
{
    ListModel model;
    model.selected = calcHistorySelected;
    model.scrollTop = calcHistoryScrollTop;

    if (historyDisplayCount() == 0)
    {
        ListItemModel empty;
        empty.label = "No calculations yet";
        empty.isSelected = true;
        empty.isDimmed = true;
        model.items.push_back(empty);
        return model;
    }

    if (calcHasInitialValue)
    {
        ListItemModel item;
        item.type = ListItemType::Property;
        item.label = formatNumberForDisplay(formatNumber(calcInitialValue));
        item.value = "=";
        item.propertyFirst = true;
        item.isSelected = calcHistorySelected == 0;
        model.items.push_back(item);
    }

    for (int i = 0; i < (int)calcHistory.size(); ++i)
    {
        ListItemModel item;
        item.type = ListItemType::Property;
        item.label = formatNumberForDisplay(
            formatNumber(calcHistory[i].value));
        item.value = String(calcHistory[i].op);
        item.propertyFirst = true;
        item.isSelected = i == historyStepIndexFromDisplay(calcHistorySelected);
        model.items.push_back(item);
    }
    return model;
}

void drawCalculatorDisplay()
{
    drawOverlayTwoColumnInputValue(calculatorOverlayModel());
}

void drawCalculationHistory()
{
    HeaderModel header;
    header.appHeaderTag = "CALCULATOR";
    header.appHeaderTitle = formatNumberForDisplay(calcDisplay);
    header.cursor = true;
    header.appHeaderTitleRightAligned = true;
    drawHeader(header);

    drawList(calculationHistoryListModel());

    FooterModel footer;
    footer.left = "[F]Calc [H]Help";
    footer.center = "[,/]Op [Ok]Edit";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}
} // namespace CalculatorInternal

using namespace CalculatorInternal;

void drawCalculator()
{
    if (calculatorHistoryVisible)
        drawCalculationHistory();
    else
        drawOverlay(calculatorOverlayModel());
}
