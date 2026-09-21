#include "module/service/SdTransfer.h"

#include "core/State.h"
#include "UI/Overlay.h"

#include <ESPmDNS.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

namespace
{
constexpr uint16_t PORT = 80;
constexpr unsigned long SESSION_TIMEOUT_MS = 10UL * 60UL * 1000UL;
constexpr size_t DISPLAY_UPDATE_BYTES = 64U * 1024U;

WebServer server(PORT);
bool handlersConfigured = false;
bool sessionActive = false;
bool serverRunning = false;
bool mdnsRunning = false;
bool uploadActive = false;
bool uploadSucceeded = false;
size_t uploadedBytes = 0;
size_t displayedBytes = 0;
unsigned long sessionActivityMs = 0;
String password;
String statusText;
String uploadError;
String uploadDirectory;
String uploadDestination;
String uploadTemporary;
String uploadBackup;
File uploadFile;

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

String urlEncode(const String &source)
{
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    String result;
    result.reserve(source.length() * 2);
    for (size_t i = 0; i < source.length(); ++i)
    {
        const uint8_t c = static_cast<uint8_t>(source.charAt(i));
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
        {
            result += static_cast<char>(c);
        }
        else
        {
            result += '%';
            result += HEX_DIGITS[c >> 4];
            result += HEX_DIGITS[c & 0x0f];
        }
    }
    return result;
}

bool normalizePath(const String &requested, String &normalized)
{
    String input = requested;
    input.trim();
    if (!input.length())
        input = "/";
    if (!input.startsWith("/") || input.indexOf('\\') >= 0)
        return false;

    normalized = "/";
    int start = 1;
    while (start <= static_cast<int>(input.length()))
    {
        int slash = input.indexOf('/', start);
        if (slash < 0)
            slash = input.length();
        const String part = input.substring(start, slash);
        if (part.length())
        {
            if (part == "." || part == "..")
                return false;
            for (size_t i = 0; i < part.length(); ++i)
                if (static_cast<uint8_t>(part.charAt(i)) < 32)
                    return false;
            if (normalized.length() > 1)
                normalized += '/';
            normalized += part;
        }
        start = slash + 1;
    }
    return normalized.length() <= 240;
}

String baseName(String path)
{
    path.replace('\\', '/');
    while (path.endsWith("/") && path.length() > 1)
        path.remove(path.length() - 1);
    const int slash = path.lastIndexOf('/');
    return slash >= 0 ? path.substring(slash + 1) : path;
}

bool safeFilename(const String &requested, String &filename)
{
    filename = baseName(requested);
    filename.trim();
    if (!filename.length() || filename == "." || filename == ".." || filename.length() > 128)
        return false;
    for (size_t i = 0; i < filename.length(); ++i)
    {
        const char c = filename.charAt(i);
        if (static_cast<uint8_t>(c) < 32 || c == '/' || c == '\\' || c == ':' || c == '"')
            return false;
    }
    return true;
}

String joinPath(const String &directory, const String &name)
{
    return directory == "/" ? "/" + name : directory + "/" + name;
}

String unusedSiblingPath(const String &destination, const char *suffix)
{
    for (uint8_t attempt = 0; attempt < 8; ++attempt)
    {
        char token[9];
        snprintf(token, sizeof(token), "%08lx", static_cast<unsigned long>(esp_random()));
        const String candidate = destination + ".bs-" + token + suffix;
        if (!SD.exists(candidate.c_str()))
            return candidate;
    }
    return "";
}

String parentPath(const String &path)
{
    if (path == "/")
        return "/";
    const int slash = path.lastIndexOf('/');
    return slash <= 0 ? "/" : path.substring(0, slash);
}

bool authenticated()
{
    if (!localWebAuthEnabled || server.authenticate("admin", password.c_str()))
        return true;
    server.requestAuthentication(BASIC_AUTH, "BrokenSignal SD Transfer", "One-time code required");
    return false;
}

bool openDirectoryArg(const char *argument, String &path, File &directory)
{
    if (!normalizePath(server.arg(argument), path))
        return false;
    directory = SD.open(path.c_str());
    return directory && directory.isDirectory();
}

void sendPageChunk(const String &chunk)
{
    server.sendContent(chunk);
}

void sendBrowserPage()
{
    sessionActivityMs = millis();
    if (!authenticated())
        return;
    String currentPath;
    File directory;
    if (!openDirectoryArg("path", currentPath, directory))
    {
        server.send(404, "text/plain", "Folder not found");
        return;
    }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");
    sendPageChunk(F(
        "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>BrokenSignal SD Transfer</title><style>"
        "body{font:16px system-ui;max-width:46rem;margin:2rem auto;padding:0 1rem;background:#111;color:#eee}"
        "main{border:1px solid #777;padding:1.25rem}a{color:#7bdcff}ul{padding-left:1.3rem}li{margin:.55rem 0}"
        "form{border-top:1px solid #555;margin-top:1.25rem;padding-top:1rem}input,button{font:inherit}"
        "button{padding:.5rem 1rem;margin-top:.75rem}.ok{color:#73d673}.warn{color:#f6c85f}code{word-break:break-all}"
        "</style></head><body><main><h1>SD Transfer</h1>"));
    sendPageChunk("<p>Folder: <code>" + htmlEscape(currentPath) + "</code></p>");
    if (statusText.length())
        sendPageChunk("<p class=\"ok\">" + htmlEscape(statusText) + "</p>");
    sendPageChunk(F("<ul>"));
    if (currentPath != "/")
        sendPageChunk("<li><a href=\"/?path=" + urlEncode(parentPath(currentPath)) + "\">[..] Parent folder</a></li>");

    File entry = directory.openNextFile();
    while (entry)
    {
        const String name = baseName(String(entry.name()));
        const String fullPath = joinPath(currentPath, name);
        if (entry.isDirectory())
        {
            sendPageChunk("<li>Folder: <a href=\"/?path=" + urlEncode(fullPath) + "\">" +
                          htmlEscape(name) + "/</a></li>");
        }
        else
        {
            sendPageChunk("<li>File: <a href=\"/download?path=" + urlEncode(fullPath) + "\">" +
                          htmlEscape(name) + "</a> (" + String(entry.size()) + " bytes)</li>");
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();

    sendPageChunk(F("</ul><form method=\"POST\" action=\"/upload?dir="));
    sendPageChunk(urlEncode(currentPath));
    sendPageChunk(F("\" enctype=\"multipart/form-data\"><label>Select a file to upload into this folder:<br>"
                    "<input type=\"file\" name=\"file\" required></label><br><button type=\"submit\">Upload to SD</button>"
                    "</form><p class=\"warn\">This page uses local HTTP. Use it only on a trusted Wi-Fi network. "
                    "Uploading a file with the same name replaces it.</p></main></body></html>"));
    server.sendContent("");
}

void failUpload(const String &message)
{
    if (uploadFile)
        uploadFile.close();
    if (uploadTemporary.length())
        SD.remove(uploadTemporary.c_str());
    uploadActive = false;
    uploadSucceeded = false;
    uploadError = message;
    drawSdTransferScreen();
}

void beginUpload(HTTPUpload &upload)
{
    uploadError = "";
    uploadSucceeded = false;
    uploadedBytes = 0;
    displayedBytes = 0;

    File directory;
    if (!openDirectoryArg("dir", uploadDirectory, directory))
    {
        uploadError = "Destination folder not found";
        return;
    }
    directory.close();

    String filename;
    if (!safeFilename(upload.filename, filename))
    {
        uploadError = "Invalid filename";
        return;
    }

    uploadDestination = joinPath(uploadDirectory, filename);
    if (uploadDestination.length() > 220)
    {
        uploadError = "Destination path is too long";
        return;
    }
    uploadTemporary = unusedSiblingPath(uploadDestination, ".part");
    uploadBackup = unusedSiblingPath(uploadDestination, ".bak");
    if (!uploadTemporary.length() || !uploadBackup.length())
    {
        uploadError = "Could not reserve upload files";
        return;
    }

    File existing = SD.open(uploadDestination.c_str());
    if (existing && existing.isDirectory())
    {
        existing.close();
        uploadError = "A folder already has that name";
        return;
    }
    existing.close();

    uploadFile = SD.open(uploadTemporary.c_str(), FILE_WRITE);
    if (!uploadFile)
    {
        uploadError = "Could not create file on SD card";
        return;
    }
    uploadActive = true;
    drawSdTransferScreen();
}

void finishUpload()
{
    if (!uploadActive)
        return;
    uploadFile.close();
    uploadActive = false;

    const bool destinationExisted = SD.exists(uploadDestination.c_str());
    if (destinationExisted)
    {
        if (!SD.rename(uploadDestination.c_str(), uploadBackup.c_str()))
        {
            failUpload("Could not replace existing file");
            return;
        }
    }

    if (!SD.rename(uploadTemporary.c_str(), uploadDestination.c_str()))
    {
        if (destinationExisted)
            SD.rename(uploadBackup.c_str(), uploadDestination.c_str());
        failUpload("Could not finalize uploaded file");
        return;
    }
    if (destinationExisted)
        SD.remove(uploadBackup.c_str());

    uploadSucceeded = true;
    statusText = "Uploaded " + baseName(uploadDestination) + " (" + String(uploadedBytes) + " bytes).";
    drawSdTransferScreen();
}

void configureHandlers()
{
    if (handlersConfigured)
        return;

    server.on("/", HTTP_GET, sendBrowserPage);

    server.on("/download", HTTP_GET, []()
    {
        sessionActivityMs = millis();
        if (!authenticated())
            return;
        String path;
        if (!normalizePath(server.arg("path"), path) || path == "/")
        {
            server.send(400, "text/plain", "Invalid file path");
            return;
        }
        File file = SD.open(path.c_str(), FILE_READ);
        if (!file || file.isDirectory())
        {
            file.close();
            server.send(404, "text/plain", "File not found");
            return;
        }
        String filename = baseName(path);
        filename.replace('"', '_');
        server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        server.streamFile(file, "application/octet-stream");
        file.close();
        sessionActivityMs = millis();
    });

    server.on("/upload", HTTP_POST, []()
    {
        sessionActivityMs = millis();
        if (!authenticated())
            return;
        if (!uploadSucceeded)
        {
            server.send(500, "text/plain", uploadError.length() ? uploadError : "Upload failed");
            return;
        }
        server.sendHeader("Location", "/?path=" + urlEncode(uploadDirectory));
        server.send(303, "text/plain", "Upload complete");
    }, []()
    {
        if (localWebAuthEnabled &&
            !server.authenticate("admin", password.c_str()))
            return;
        HTTPUpload &upload = server.upload();
        sessionActivityMs = millis();
        switch (upload.status)
        {
        case UPLOAD_FILE_START:
            beginUpload(upload);
            break;
        case UPLOAD_FILE_WRITE:
            if (!uploadActive)
                return;
            if (uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                failUpload("SD card write failed or is full");
                return;
            }
            uploadedBytes += upload.currentSize;
            if (uploadedBytes >= displayedBytes + DISPLAY_UPDATE_BYTES)
            {
                displayedBytes = uploadedBytes;
                drawSdTransferScreen();
            }
            break;
        case UPLOAD_FILE_END:
            finishUpload();
            break;
        case UPLOAD_FILE_ABORTED:
            failUpload("Upload cancelled by browser");
            break;
        default:
            break;
        }
    });

    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    handlersConfigured = true;
}
} // namespace

bool beginSdTransfer()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;
    configureHandlers();
    if (serverRunning)
        server.stop();

    password = localWebAuthEnabled
        ? String(100000UL + (esp_random() % 900000UL)) : String();
    statusText = "";
    uploadError = "";
    uploadActive = false;
    uploadSucceeded = false;
    uploadedBytes = 0;
    displayedBytes = 0;
    sessionActivityMs = millis();
    sessionActive = true;
    server.begin();
    serverRunning = true;
    mdnsRunning = MDNS.begin("brokensignal");
    if (mdnsRunning)
        MDNS.addService("http", "tcp", PORT);
    drawSdTransferScreen();
    return true;
}

void tickSdTransfer()
{
    if (!sessionActive)
        return;
    if (serverRunning)
        server.handleClient();
    if (!uploadActive && serverRunning && millis() - sessionActivityMs >= SESSION_TIMEOUT_MS)
    {
        server.stop();
        serverRunning = false;
        if (mdnsRunning)
        {
            MDNS.end();
            mdnsRunning = false;
        }
        statusText = "Session expired";
        drawSdTransferScreen();
    }
}

void stopSdTransfer()
{
    if (!sessionActive || uploadActive)
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
    uploadError = "";
}

bool sdTransferActive()
{
    return sessionActive;
}

bool sdTransferInProgress()
{
    return uploadActive;
}

void drawSdTransferScreen()
{
    if (!sessionActive)
        return;
    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = "SD Transfer";

    if (uploadActive)
    {
        model.items = {"Receiving file", "Received: " + String(uploadedBytes / 1024U) + " KB"};
    }
    else if (statusText == "Session expired")
    {
        model.items = {"Transfer unavailable", statusText};
        model.confirmText = "[Esc]Close";
    }
    else if (uploadError.length())
    {
        model.items = {"Upload failed", uploadError};
        model.confirmText = "Browser may retry  [Esc]Close";
    }
    else
    {
        const String mdnsAddress = mdnsRunning ? "http://brokensignal.local/"
                                               : "mDNS unavailable";
        const String ipAddress = "http://" + WiFi.localIP().toString() + "/";
        if (localWebAuthEnabled)
            model.items = {mdnsAddress, ipAddress, "User: admin", "Code: " + password};
        else
            model.items = {mdnsAddress, ipAddress, "No sign-in required"};
        model.confirmText = "10 min window  [Esc]Cancel";
    }
    drawOverlay(model);
}
