#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

void thermalPrinterOpen();
void drawThermalPrinter();
void handleThermalPrinterInput(Keyboard_Class::KeysState &keys);

bool thermalPrinterModalActive();
void cancelThermalPrinterModal();
bool thermalPrinterResumePendingOperation();
void thermalPrinterCancelPendingOperation();

void thermalPrinterEditText();
void thermalPrinterEditServer();
void thermalPrinterPrint();
void thermalPrinterRefreshStatus();
void thermalPrinterFeed();
void thermalPrinterNextLabel();
void thermalPrinterTestPage();

String thermalPrinterServer();
String thermalPrinterMediaLabel();
String thermalPrinterAlignmentLabel();
String thermalPrinterVerticalLabel();
String thermalPrinterBoldLabel();
String thermalPrinterSizeLabel();
String thermalPrinterDateLabel();
String thermalPrinterBottomLabel();
String thermalPrinterLabelHeightLabel();

void thermalPrinterAdjustMedia(int direction);
void thermalPrinterAdjustAlignment(int direction);
void thermalPrinterAdjustVertical(int direction);
void thermalPrinterAdjustBold(int direction);
void thermalPrinterAdjustSize(int direction);
void thermalPrinterAdjustDate(int direction);
void thermalPrinterAdjustBottom(int direction);
void thermalPrinterAdjustLabelHeight(int direction);
