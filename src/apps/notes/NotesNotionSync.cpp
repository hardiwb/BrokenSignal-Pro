#include "apps/notes/NotesNotionSync.h"

#include "apps/notes/NotesInternal.h"
#include "module/service/NotesNotion.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <vector>

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");

namespace
{
constexpr const char *NOTION_HOST = "https://api.notion.com";
constexpr const char *NOTION_VERSION = "2026-03-11";
constexpr size_t MAX_SYNC_NOTES = 200;
constexpr size_t NOTION_QUERY_PAGE_SIZE = 5;

struct RemoteNote
{
    String pageId;
    NotesInternal::NoteEntry entry;
};

enum NoteProperty : uint8_t
{
    NotePropertyCheckbox = 1 << 0,
    NotePropertyEntry = 1 << 1,
    NotePropertyDate = 1 << 2,
    NotePropertyCategory = 1 << 3,
    NotePropertyId = 1 << 4,
    NotePropertyAll = NotePropertyCheckbox | NotePropertyEntry | NotePropertyDate |
                      NotePropertyCategory | NotePropertyId,
};

bool uuidLike(const String &value)
{
    if (value.length() != 36)
        return false;
    for (size_t i = 0; i < value.length(); ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (value.charAt(i) != '-')
                return false;
        }
        else if (!isHexadecimalDigit(value.charAt(i)))
            return false;
    }
    return true;
}

bool notionRequest(const NotesNotionConfig &config, const char *method,
                   const String &path, const String &body,
                   JsonDocument &response, String &error)
{
    WiFiClientSecure client;
    client.setCACertBundle(x509_crt_bundle_start);
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(30000);
    // Notion normally uses chunked HTTP/1.1 responses. HTTPClient::getString()
    // cannot reserve those up front and may silently return a truncated String
    // when the ESP32 heap is fragmented. Request identity framing and parse the
    // body straight from the TLS stream so a second full response buffer is not
    // needed alongside ArduinoJson's document.
    http.useHTTP10(true);
    if (!http.begin(client, String(NOTION_HOST) + path))
    {
        error = "Invalid Notion address";
        return false;
    }
    http.addHeader("Authorization", "Bearer " + config.apiToken);
    http.addHeader("Notion-Version", NOTION_VERSION);
    if (body.length())
        http.addHeader("Content-Type", "application/json");

    int status = 0;
    if (!strcmp(method, "GET"))
        status = http.GET();
    else
        status = http.sendRequest(method, body);

    if (status <= 0)
    {
        http.end();
        error = "Notion connection failed " + String(status);
        return false;
    }

    const DeserializationError jsonError = deserializeJson(response, http.getStream());
    http.end();
    if (jsonError)
    {
        error = "Notion JSON ";
        error += jsonError.c_str();
        return false;
    }
    if (status >= 200 && status < 300)
        return true;

    const char *notionMessage = response["message"].as<const char *>();
    error = "Notion " + String(status);
    if (notionMessage && strlen(notionMessage))
        error += ": " + String(notionMessage);
    return false;
}

bool discoverDataSource(NotesNotionConfig &config, String &error)
{
    if (uuidLike(config.dataSourceId))
        return true;
    JsonDocument response;
    if (!notionRequest(config, "GET", "/v1/databases/" + config.databaseId, "", response, error))
        return false;
    JsonArrayConst sources = response["data_sources"].as<JsonArrayConst>();
    if (sources.size() != 1)
    {
        error = sources.size() == 0 ? "Database has no data source" : "Choose a data source in Notion";
        return false;
    }
    config.dataSourceId = String(sources[0]["id"] | "");
    if (!uuidLike(config.dataSourceId) || !saveNotesNotionDataSourceId(config.dataSourceId))
    {
        error = "Could not save data-source ID";
        return false;
    }
    return true;
}

String richTextValue(JsonVariantConst property)
{
    JsonArrayConst values = property["rich_text"].as<JsonArrayConst>();
    String result;
    for (JsonVariantConst value : values)
    {
        const char *plain = value["plain_text"] | "";
        result += plain;
    }
    return result;
}

String titleValue(JsonVariantConst property)
{
    JsonArrayConst values = property["title"].as<JsonArrayConst>();
    String result;
    for (JsonVariantConst value : values)
    {
        const char *plain = value["plain_text"] | "";
        result += plain;
    }
    return result;
}

bool parseRemotePage(JsonVariantConst page, RemoteNote &remote)
{
    remote = {};
    remote.pageId = String(page["id"] | "");
    JsonObjectConst properties = page["properties"].as<JsonObjectConst>();
    remote.entry.id = richTextValue(properties["ID"]);
    remote.entry.text = titleValue(properties["Entry"]);
    remote.entry.done = properties["Checkbox"]["checkbox"] | false;
    remote.entry.category = String(properties["Category"]["select"]["name"] | "Personal");
    const String date = String(properties["Date"]["date"]["start"] | "");
    remote.entry.stamp = date.length() >= 10 ? date.substring(0, 10) + " 00:00" : "";
    remote.entry.text.trim();
    remote.entry.id.trim();
    remote.entry.category.trim();
    remote.entry.text.replace("\r", " ");
    remote.entry.text.replace("\n", " ");
    remote.entry.category.replace("|", " ");
    remote.entry.category.replace("\r", " ");
    remote.entry.category.replace("\n", " ");
    if (!remote.entry.category.length())
        remote.entry.category = "Personal";
    return remote.pageId.length() && remote.entry.text.length() && remote.entry.stamp.length() >= 10;
}

bool queryRemoteNotes(const NotesNotionConfig &config, std::vector<RemoteNote> &notes, String &error)
{
    String cursor;
    bool hasMore = true;
    while (hasMore)
    {
        JsonDocument request;
        // Notion page objects are verbose. Keep each parsed page small enough
        // for the JSON document and accumulated RemoteNote records to coexist.
        request["page_size"] = NOTION_QUERY_PAGE_SIZE;
        if (cursor.length())
            request["start_cursor"] = cursor;
        String body;
        serializeJson(request, body);
        JsonDocument result;
        if (!notionRequest(config, "POST", "/v1/data_sources/" + config.dataSourceId + "/query",
                           body, result, error))
            return false;
        for (JsonVariantConst page : result["results"].as<JsonArrayConst>())
        {
            RemoteNote note;
            if (parseRemotePage(page, note))
            {
                if (notes.size() >= MAX_SYNC_NOTES)
                {
                    error = "More than 200 Notion notes";
                    return false;
                }
                notes.push_back(note);
            }
        }
        hasMore = result["has_more"] | false;
        cursor = String(result["next_cursor"] | "");
        if (hasMore && !cursor.length())
        {
            error = "Missing Notion cursor";
            return false;
        }
        delay(1);
    }
    return true;
}

uint8_t changedProperties(const NotesInternal::NoteEntry &local,
                          const NotesInternal::NoteEntry &remote)
{
    uint8_t changed = 0;
    if (local.done != remote.done)
        changed |= NotePropertyCheckbox;
    if (local.text != remote.text)
        changed |= NotePropertyEntry;
    if (local.stamp.substring(0, 10) != remote.stamp.substring(0, 10))
        changed |= NotePropertyDate;
    if (local.category != remote.category)
        changed |= NotePropertyCategory;
    if (local.id != remote.id)
        changed |= NotePropertyId;
    return changed;
}

String notePropertiesJson(const NotesInternal::NoteEntry &entry,
                          uint8_t included = NotePropertyAll)
{
    JsonDocument document;
    JsonObject properties = document["properties"].to<JsonObject>();
    if (included & NotePropertyCheckbox)
    {
        JsonObject checkbox = properties["Checkbox"].to<JsonObject>();
        checkbox["checkbox"] = entry.done;
    }
    if (included & NotePropertyEntry)
    {
        JsonArray title = properties["Entry"]["title"].to<JsonArray>();
        title.add<JsonObject>()["text"]["content"] = entry.text;
    }
    if (included & NotePropertyDate)
        properties["Date"]["date"]["start"] = entry.stamp.substring(0, 10);
    if (included & NotePropertyCategory)
        properties["Category"]["select"]["name"] = entry.category;
    if (included & NotePropertyId)
    {
        JsonArray id = properties["ID"]["rich_text"].to<JsonArray>();
        id.add<JsonObject>()["text"]["content"] = entry.id;
    }
    String body;
    serializeJson(document, body);
    return body;
}

bool updateRemoteNote(const NotesNotionConfig &config, const String &pageId,
                      const NotesInternal::NoteEntry &entry, uint8_t properties,
                      String &error)
{
    if (properties == 0)
        return true;
    JsonDocument response;
    return notionRequest(config, "PATCH", "/v1/pages/" + pageId,
                         notePropertiesJson(entry, properties), response, error);
}

bool createRemoteNote(const NotesNotionConfig &config, const NotesInternal::NoteEntry &entry,
                      String &error)
{
    JsonDocument document;
    document["parent"]["type"] = "data_source_id";
    document["parent"]["data_source_id"] = config.dataSourceId;
    JsonDocument properties;
    if (deserializeJson(properties, notePropertiesJson(entry, NotePropertyAll)))
    {
        error = "Could not build Notes request";
        return false;
    }
    document["properties"] = properties["properties"];
    String body;
    serializeJson(document, body);
    JsonDocument response;
    return notionRequest(config, "POST", "/v1/pages", body, response, error);
}

bool monthFilename(const String &name, String &month)
{
    const int slash = name.lastIndexOf('/');
    const String base = slash >= 0 ? name.substring(slash + 1) : name;
    if (base.length() != 11 || base.charAt(4) != '-' || base.substring(7) != ".txt")
        return false;
    month = base.substring(0, 7);
    return month.toInt() >= 2000 && month.substring(5).toInt() >= 1 && month.substring(5).toInt() <= 12;
}

bool loadLocalNotes(std::vector<NotesInternal::NoteEntry> &notes,
                    std::vector<String> &months, String &error)
{
    File directory = SD.open("/Notes");
    if (!directory)
        return true;
    if (!directory.isDirectory())
    {
        directory.close();
        error = "/Notes is not a directory";
        return false;
    }
    File file = directory.openNextFile();
    while (file)
    {
        String month;
        if (!file.isDirectory() && monthFilename(String(file.name()), month))
        {
            months.push_back(month);
            while (file.available())
            {
                String line = file.readStringUntil('\n');
                line.trim();
                if (!line.length())
                    continue;
                NotesInternal::NoteEntry entry;
                if (!NotesInternal::parseNoteStorageLine(line, entry))
                {
                    // Very old files may contain `stamp|text` or plain text.
                    const int separator = line.indexOf('|');
                    entry.stamp = separator >= 0 ? line.substring(0, separator) : month + "-01 00:00";
                    entry.text = separator >= 0 ? line.substring(separator + 1) : line;
                    entry.text.trim();
                    if (!entry.text.length() || entry.stamp.length() < 10)
                    {
                        file.close();
                        directory.close();
                        error = "Invalid note in " + month;
                        return false;
                    }
                }
                if (!entry.id.length())
                    entry.id = NotesInternal::createNoteId();
                notes.push_back(entry);
                if (notes.size() > MAX_SYNC_NOTES)
                {
                    file.close();
                    directory.close();
                    error = "More than 200 local notes";
                    return false;
                }
            }
        }
        file.close();
        file = directory.openNextFile();
    }
    directory.close();
    return true;
}

void addMonth(std::vector<String> &months, const String &month)
{
    if (month.length() == 7 && std::find(months.begin(), months.end(), month) == months.end())
        months.push_back(month);
}

bool writeLocalNotes(const std::vector<NotesInternal::NoteEntry> &notes,
                     std::vector<String> months, String &error)
{
    for (const auto &entry : notes)
        addMonth(months, entry.stamp.substring(0, 7));
    std::sort(months.begin(), months.end());
    months.erase(std::unique(months.begin(), months.end()), months.end());
    SD.mkdir("/Notes");

    for (const String &month : months)
    {
        const String destination = "/Notes/" + month + ".txt";
        const String temporary = destination + ".tmp";
        const String backup = destination + ".bak";
        SD.remove(temporary.c_str());
        File output = SD.open(temporary, FILE_WRITE);
        if (!output)
        {
            error = "Cannot write " + month;
            return false;
        }
        for (const auto &entry : notes)
            if (entry.stamp.startsWith(month))
                output.printf("%s|%c|v2|%s|%s|%s|%s\n", entry.stamp.c_str(), entry.done ? 'x' : '-',
                              entry.id.c_str(), entry.category.c_str(), entry.syncHash.c_str(), entry.text.c_str());
        output.close();

        SD.remove(backup.c_str());
        const bool existed = SD.exists(destination.c_str());
        if (existed && !SD.rename(destination.c_str(), backup.c_str()))
        {
            SD.remove(temporary.c_str());
            error = "Cannot back up " + month;
            return false;
        }
        if (!SD.rename(temporary.c_str(), destination.c_str()))
        {
            if (existed)
                SD.rename(backup.c_str(), destination.c_str());
            error = "Cannot replace " + month;
            return false;
        }
        SD.remove(backup.c_str());
    }
    return true;
}

int findLocal(const std::vector<NotesInternal::NoteEntry> &notes, const String &id)
{
    for (size_t i = 0; i < notes.size(); ++i)
        if (notes[i].id == id)
            return static_cast<int>(i);
    return -1;
}

int findRemote(const std::vector<RemoteNote> &notes, const String &id)
{
    for (size_t i = 0; i < notes.size(); ++i)
        if (notes[i].entry.id == id)
            return static_cast<int>(i);
    return -1;
}
} // namespace

bool syncNotesWithNotion(String &message)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        message = "Connect WiFi first";
        return false;
    }
    NotesNotionConfig config;
    if (!loadNotesNotionConfig(config))
    {
        message = "Configure Notes & Notion";
        return false;
    }
    if (!discoverDataSource(config, message))
        return false;

    std::vector<NotesInternal::NoteEntry> local;
    std::vector<String> months;
    if (!loadLocalNotes(local, months, message))
        return false;
    std::vector<RemoteNote> remote;
    if (!queryRemoteNotes(config, remote, message))
        return false;

    int pulled = 0;
    int pushed = 0;
    int conflicts = 0;
    for (auto &remoteNote : remote)
    {
        if (!remoteNote.entry.id.length())
        {
            // Pair an exported/legacy Notion row with an identical local note
            // before allocating a new ID. This makes the first migration
            // idempotent instead of duplicating the same note on both sides.
            for (auto &localNote : local)
            {
                const bool sameLegacyNote =
                    localNote.stamp.substring(0, 10) == remoteNote.entry.stamp.substring(0, 10) &&
                    localNote.done == remoteNote.entry.done &&
                    localNote.text == remoteNote.entry.text;
                if (sameLegacyNote &&
                    findRemote(remote, localNote.id) < 0)
                {
                    remoteNote.entry.id = localNote.id;
                    if (!localNote.syncHash.length())
                        localNote.category = remoteNote.entry.category;
                    break;
                }
            }
            if (!remoteNote.entry.id.length())
                remoteNote.entry.id = NotesInternal::createNoteId();
            if (!updateRemoteNote(config, remoteNote.pageId, remoteNote.entry,
                                  NotePropertyId, message))
                return false;
        }
        const String remoteHash = NotesInternal::noteContentHash(remoteNote.entry);
        const int localIndex = findLocal(local, remoteNote.entry.id);
        if (localIndex < 0)
        {
            remoteNote.entry.syncHash = remoteHash;
            local.push_back(remoteNote.entry);
            ++pulled;
            continue;
        }

        auto &localNote = local[localIndex];
        const String localHash = NotesInternal::noteContentHash(localNote);
        const bool localChanged = localNote.syncHash.length() && localHash != localNote.syncHash;
        const bool remoteChanged = localNote.syncHash.length() && remoteHash != localNote.syncHash;
        if (!localNote.syncHash.length())
        {
            // First match: preserve the Cardputer record and establish the baseline.
            if (localHash != remoteHash)
            {
                if (!updateRemoteNote(config, remoteNote.pageId, localNote,
                                      changedProperties(localNote, remoteNote.entry), message))
                    return false;
                ++pushed;
            }
            localNote.syncHash = localHash;
        }
        else if (localChanged)
        {
            if (!updateRemoteNote(config, remoteNote.pageId, localNote,
                                  changedProperties(localNote, remoteNote.entry), message))
                return false;
            localNote.syncHash = localHash;
            ++pushed;
            if (remoteChanged)
                ++conflicts;
        }
        else if (remoteChanged)
        {
            const String timeSuffix = localNote.stamp.length() > 10 ? localNote.stamp.substring(10) : " 00:00";
            localNote = remoteNote.entry;
            localNote.stamp = remoteNote.entry.stamp.substring(0, 10) + timeSuffix;
            localNote.syncHash = remoteHash;
            ++pulled;
        }
    }

    for (auto &localNote : local)
    {
        if (findRemote(remote, localNote.id) >= 0)
            continue;
        if (!createRemoteNote(config, localNote, message))
            return false;
        localNote.syncHash = NotesInternal::noteContentHash(localNote);
        ++pushed;
    }

    if (!writeLocalNotes(local, months, message))
        return false;
    message = "Notion " + String(pushed) + " up " + String(pulled) + " down";
    if (conflicts)
        message += " " + String(conflicts) + " conflict";
    return true;
}
