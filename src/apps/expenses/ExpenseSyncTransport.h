#pragma once

#include <Arduino.h>
#include <vector>

struct ExpenseSyncConfig
{
    String server;
    String token;
};

struct ExpenseSyncEntry
{
    String id;
    String name;
    String value;
    String currency;
    String date;
};

bool loadExpenseSyncConfig(ExpenseSyncConfig &config, String &error);

bool uploadExpenseBatch(
    const ExpenseSyncConfig &config,
    const std::vector<ExpenseSyncEntry> &entries,
    std::vector<String> &acceptedIds,
    String &message);
