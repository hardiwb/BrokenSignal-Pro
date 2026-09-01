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

bool uploadExpensePreview(
    const ExpenseSyncConfig &config,
    const String &id,
    const String &name,
    const String &value,
    const String &currency,
    const String &date,
    String &message)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        message = "Connect WiFi first";
        return false;
    }

    JsonDocument request;
    request["device"] = "cardputer";
    JsonObject entry = request["entries"].to<JsonArray>().add<JsonObject>();
    entry["id"] = id;
    entry["name"] = name;
    entry["value"] = value;
    entry["currency"] = currency;
    entry["date"] = date;
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
    if (responseContainsId(result["processed"], id))
    {
        if (responseContainsId(result["synced"], id))
            message = "Synced to Notion";
        else
            message = "Processed by PC (dry run)";
        return true;
    }
    if (responseContainsId(result["already_processed"], id))
    {
        message = "Already processed by PC";
        return true;
    }
    if (responseContainsId(result["fallback_previewed"], id))
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
