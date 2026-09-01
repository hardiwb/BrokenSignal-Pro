#pragma once

#include <Arduino.h>

struct ExpenseSyncConfig
{
    String server;
    String token;
};

bool loadExpenseSyncConfig(ExpenseSyncConfig &config, String &error);

bool uploadExpensePreview(
    const ExpenseSyncConfig &config,
    const String &id,
    const String &name,
    const String &value,
    const String &currency,
    const String &date,
    String &message);
