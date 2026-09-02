#include "apps/expenses/ExpenseSyncTransport.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>

namespace
{
constexpr const char *CONFIG_PATH = "/Expenses/sync.cfg";

bool responseContainsId(JsonVariantConst values, const String &id)
{
    if (!values.is<JsonArrayConst>())
        return false;
    for (JsonVariantConst value : values.as<JsonArrayConst>())
        if (value.is<const char *>() && id == value.as<const char *>())
            return true;
    return false;
}
}

bool loadExpenseSyncConfig(ExpenseSyncConfig &config, String &error)
{
    config = {};
    File file = SD.open(CONFIG_PATH, FILE_READ);
    if (!file)
    {
        error = "Missing /Expenses/sync.cfg";
        return false;
    }

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        if (!line.length() || line.startsWith("#"))
            continue;
        int separator = line.indexOf('=');
        if (separator <= 0)
            continue;
        String key = line.substring(0, separator);
        String value = line.substring(separator + 1);
        key.trim();
        value.trim();
        if (key == "server")
            config.server = value;
        else if (key == "token")
            config.token = value;
    }
    file.close();

    while (config.server.endsWith("/"))
        config.server.remove(config.server.length() - 1);
    if (!config.server.startsWith("http://"))
    {
        error = "Server must start with http://";
        return false;
    }
    if (config.token.length() < 16)
    {
        error = "Token missing or too short";
        return false;
    }
    return true;
}

bool uploadExpenseBatch(
    const ExpenseSyncConfig &config,
    const std::vector<ExpenseSyncEntry> &entries,
    std::vector<String> &acceptedIds,
    String &message)
{
    acceptedIds.clear();
    if (entries.empty())
    {
        message = "No pending expenses";
        return true;
    }
    if (entries.size() > 50)
    {
        message = "Maximum 50 expenses per sync";
        return false;
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        message = "Connect WiFi first";
        return false;
    }

    JsonDocument request;
    request["device"] = "cardputer";
    JsonArray requestEntries = request["entries"].to<JsonArray>();
    for (const auto &source : entries)
    {
        JsonObject entry = requestEntries.add<JsonObject>();
        entry["id"] = source.id;
        entry["name"] = source.name;
        entry["value"] = source.value;
        entry["currency"] = source.currency;
        entry["date"] = source.date;
    }
    String body;
    serializeJson(request, body);

    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(60000);
    const String endpoint = config.server + "/expenses/sync";
    if (!http.begin(endpoint))
    {
        message = "Invalid server address";
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Cardputer-Token", config.token);
    const int status = http.POST(body);
    const String response = status > 0 ? http.getString() : "";
    http.end();

    if (status <= 0)
    {
        message = "PC connection failed " + String(status);
        return false;
    }

    JsonDocument result;
    if (deserializeJson(result, response))
    {
        message = "Invalid PC response (HTTP " + String(status) + ")";
        return false;
    }
    if (status != HTTP_CODE_OK)
    {
        const char *detail = result["error"] | "Request rejected";
        message = String(detail);
        return false;
    }
    for (const auto &entry : entries)
    {
        if (responseContainsId(result["processed"], entry.id) ||
            responseContainsId(result["already_processed"], entry.id))
            acceptedIds.push_back(entry.id);
    }

    const int acceptedCount = acceptedIds.size();
    const int requestedCount = entries.size();
    if (acceptedCount == requestedCount)
    {
        if (result["notion_enabled"] | false)
            message = "Synced " + String(acceptedCount) + " to Notion";
        else
            message = "Processed " + String(acceptedCount) + " by PC (dry run)";
        return true;
    }
    if (acceptedCount > 0)
    {
        message = "Processed " + String(acceptedCount) + "/" + String(requestedCount) + "; retry pending";
        return false;
    }
    if (result["fallback_previewed"].as<JsonArrayConst>().size())
    {
        message = "Codex fallback; retry later";
        return false;
    }
    const JsonArrayConst failed = result["failed"].as<JsonArrayConst>();
    if (!failed.isNull() && failed.size())
    {
        const char *detail = failed[0]["error"] | "PC processing failed";
        message = String(detail);
        return false;
    }
    message = "PC did not confirm entry";
    return false;
}
