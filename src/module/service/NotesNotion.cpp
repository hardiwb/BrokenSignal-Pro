#include "module/service/NotesNotion.h"

#include "UI/Overlay.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

namespace
{
constexpr uint16_t PORT = 80;
constexpr unsigned long SESSION_TIMEOUT_MS = 5UL * 60UL * 1000UL;
constexpr const char *PREF_NAMESPACE = "notes-notion";

WebServer server(PORT);
bool handlersConfigured = false;
bool sessionActive = false;
bool serverRunning = false;
bool mdnsRunning = false;
unsigned long sessionActivityMs = 0;
String password;
String statusText;

String htmlEscape(const String &source)
{
    String result;
    result.reserve(source.length() + 16);
    for (size_t i = 0; i < source.length(); ++i)
    {
        switch (source.charAt(i))
        {
        case '&': result += F("&amp;"); break;
        case '<': result += F("&lt;"); break;
        case '>': result += F("&gt;"); break;
        case '"': result += F("&quot;"); break;
        case '\'': result += F("&#39;"); break;
        default: result += source.charAt(i); break;
        }
    }
    return result;
}

String normalizedUuid(String value)
{
    value.trim();
    const int query = value.indexOf('?');
    if (query >= 0)
        value = value.substring(0, query);
    const int slash = value.lastIndexOf('/');
    if (slash >= 0)
        value = value.substring(slash + 1);
    value.replace("-", "");
    if (value.length() > 32)
        value = value.substring(value.length() - 32);
    if (value.length() != 32)
        return "";
    for (size_t i = 0; i < value.length(); ++i)
        if (!isHexadecimalDigit(value.charAt(i)))
            return "";
    value.toLowerCase();
    return value.substring(0, 8) + "-" + value.substring(8, 12) + "-" +
           value.substring(12, 16) + "-" + value.substring(16, 20) + "-" +
           value.substring(20);
}

bool authenticated()
{
    if (server.authenticate("admin", password.c_str()))
        return true;
    server.requestAuthentication(BASIC_AUTH, "BrokenSignal Notes", "One-time code required");
    return false;
}

String configurationPage()
{
    NotesNotionConfig config;
    loadNotesNotionConfig(config);
    String page = F(
        "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>BrokenSignal Notes</title><style>"
        "body{font:16px system-ui;max-width:38rem;margin:3rem auto;padding:0 1rem;background:#111;color:#eee}"
        "main{border:1px solid #777;padding:1.5rem}label{display:block;margin-top:1rem}"
        "input{box-sizing:border-box;font:inherit;padding:.5rem;width:100%}button{font:inherit;margin-top:1.2rem;padding:.55rem 1rem}"
        ".warn{color:#f6c85f}.ok{color:#73d673}</style></head><body><main><h1>Notes &amp; Notion</h1>");
    if (statusText.length())
        page += "<p class=\"ok\">" + htmlEscape(statusText) + "</p>";
    page += F("<p class=\"warn\">The token is sent over your local Wi-Fi and stored in device preferences. Use only on a trusted network.</p>"
              "<form method=\"POST\" action=\"/save\">"
              "<label>Integration token<input type=\"password\" name=\"token\" autocomplete=\"off\" placeholder=\"");
    page += config.apiToken.length() ? F("Saved - leave blank to keep it") : F("ntn_...");
    page += F("\"></label><label>Agenda database ID or URL<input name=\"database\" value=\"");
    page += htmlEscape(config.databaseId);
    page += F("\" required></label><button type=\"submit\">Save configuration</button></form>"
              "<form method=\"POST\" action=\"/clear\"><button type=\"submit\">Remove saved configuration</button></form>"
              "<p>The data-source ID is discovered automatically during the first sync.</p></main></body></html>");
    return page;
}

void configureHandlers()
{
    if (handlersConfigured)
        return;

    server.on("/", HTTP_GET, []()
    {
        sessionActivityMs = millis();
        if (!authenticated())
            return;
        server.send(200, "text/html", configurationPage());
    });

    server.on("/save", HTTP_POST, []()
    {
        sessionActivityMs = millis();
        if (!authenticated())
            return;

        NotesNotionConfig current;
        loadNotesNotionConfig(current);
        String token = server.arg("token");
        token.trim();
        if (!token.length())
            token = current.apiToken;
        const String databaseId = normalizedUuid(server.arg("database"));
        if (token.length() < 20 || databaseId.length() != 36)
        {
            server.send(400, "text/plain", "Enter a valid integration token and database ID.");
            return;
        }

        Preferences preferences;
        if (!preferences.begin(PREF_NAMESPACE, false))
        {
            server.send(500, "text/plain", "Could not open device preferences.");
            return;
        }
        const bool databaseChanged = current.databaseId != databaseId;
        const bool tokenSaved = preferences.putString("token", token) > 0;
        const bool databaseSaved = preferences.putString("database", databaseId) > 0;
        if (databaseChanged)
            preferences.remove("source");
        preferences.end();
        if (!tokenSaved || !databaseSaved)
        {
            server.send(500, "text/plain", "Could not save configuration.");
            return;
        }
        statusText = "Configuration saved. You may close this page.";
        drawNotesNotionSetupScreen();
        server.send(200, "text/html", configurationPage());
    });

    server.on("/clear", HTTP_POST, []()
    {
        sessionActivityMs = millis();
        if (!authenticated())
            return;
        Preferences preferences;
        if (preferences.begin(PREF_NAMESPACE, false))
        {
            preferences.clear();
            preferences.end();
        }
        statusText = "Saved Notes credentials removed.";
        drawNotesNotionSetupScreen();
        server.send(200, "text/html", configurationPage());
    });

    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    handlersConfigured = true;
}
} // namespace

bool loadNotesNotionConfig(NotesNotionConfig &config)
{
    config = {};
    Preferences preferences;
    if (!preferences.begin(PREF_NAMESPACE, true))
        return false;
    config.apiToken = preferences.getString("token", "");
    config.databaseId = preferences.getString("database", "");
    config.dataSourceId = preferences.getString("source", "");
    preferences.end();
    return config.apiToken.length() >= 20 && config.databaseId.length() == 36;
}

bool saveNotesNotionDataSourceId(const String &dataSourceId)
{
    const String normalized = normalizedUuid(dataSourceId);
    if (!normalized.length())
        return false;
    Preferences preferences;
    if (!preferences.begin(PREF_NAMESPACE, false))
        return false;
    const bool saved = preferences.putString("source", normalized) > 0;
    preferences.end();
    return saved;
}

bool beginNotesNotionSetup()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;
    configureHandlers();
    if (serverRunning)
        server.stop();
    password = String(100000UL + (esp_random() % 900000UL));
    statusText = "";
    sessionActivityMs = millis();
    sessionActive = true;
    server.begin();
    serverRunning = true;
    mdnsRunning = MDNS.begin("brokensignal");
    if (mdnsRunning)
        MDNS.addService("http", "tcp", PORT);
    drawNotesNotionSetupScreen();
    return true;
}

void tickNotesNotionSetup()
{
    if (!sessionActive)
        return;
    if (serverRunning)
        server.handleClient();
    if (serverRunning && millis() - sessionActivityMs >= SESSION_TIMEOUT_MS)
    {
        server.stop();
        serverRunning = false;
        if (mdnsRunning)
        {
            MDNS.end();
            mdnsRunning = false;
        }
        statusText = "Session expired";
        drawNotesNotionSetupScreen();
    }
}

void stopNotesNotionSetup()
{
    if (!sessionActive)
        return;
    if (serverRunning)
        server.stop();
    if (mdnsRunning)
        MDNS.end();
    serverRunning = false;
    mdnsRunning = false;
    sessionActive = false;
    password = "";
    statusText = "";
}

bool notesNotionSetupActive()
{
    return sessionActive;
}

void drawNotesNotionSetupScreen()
{
    if (!sessionActive)
        return;
    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = "Notes & Notion";
    if (statusText == "Session expired")
    {
        model.items = {"Setup unavailable", statusText};
        model.confirmText = "[Esc]Close";
    }
    else
    {
        model.items = {
            mdnsRunning ? "http://brokensignal.local/" : "http://" + WiFi.localIP().toString() + "/",
            "User: admin",
            "Code: " + password};
        model.confirmText = "5 min window  [Esc]Cancel";
    }
    drawOverlay(model);
}
