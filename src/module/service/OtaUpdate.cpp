#include "module/service/OtaUpdate.h"

#include "core/State.h"
#include "UI/Overlay.h"

#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

namespace
{
constexpr uint16_t OTA_PORT = 80;
constexpr unsigned long OTA_SESSION_TIMEOUT_MS = 5UL * 60UL * 1000UL;
constexpr unsigned long OTA_REBOOT_DELAY_MS = 1500;

WebServer otaServer(OTA_PORT);
bool handlersConfigured = false;
bool sessionActive = false;
bool serverRunning = false;
bool mdnsRunning = false;
bool uploadActive = false;
bool uploadSucceeded = false;
bool rebootPending = false;
size_t uploadedBytes = 0;
size_t displayedBytes = 0;
unsigned long sessionActivityMs = 0;
unsigned long rebootAtMs = 0;
String otaPassword;
String otaError;

const char OTA_PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BrokenSignal Pro Update</title><style>
body{font:16px system-ui;max-width:34rem;margin:3rem auto;padding:0 1rem;background:#111;color:#eee}
main{border:1px solid #777;padding:1.5rem}input,button{font:inherit;margin-top:1rem}button{padding:.5rem 1rem}
</style></head><body><main><h1>Firmware update</h1>
<p>Select the <code>firmware.bin</code> produced by PlatformIO.</p>
<form method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="firmware" accept=".bin,application/octet-stream" required><br>
<button type="submit">Install and reboot</button></form></main></body></html>
)HTML";

bool requestAuthenticated()
{
    if (!localWebAuthEnabled ||
        otaServer.authenticate("admin", otaPassword.c_str()))
        return true;

    otaServer.requestAuthentication(BASIC_AUTH, "BrokenSignal OTA", "One-time code required");
    return false;
}

void setUploadError(const String &message)
{
    uploadActive = false;
    uploadSucceeded = false;
    otaError = message;
    drawOtaUpdateScreen();
}

void configureHandlers()
{
    if (handlersConfigured)
        return;

    otaServer.on("/", HTTP_GET, []()
    {
        sessionActivityMs = millis();
        if (!requestAuthenticated())
            return;
        otaServer.send_P(200, "text/html", OTA_PAGE);
    });

    otaServer.on("/update", HTTP_POST, []()
    {
        sessionActivityMs = millis();
        if (!requestAuthenticated())
            return;
        if (!uploadSucceeded)
        {
            const String message = otaError.length() > 0 ? otaError : "Firmware upload failed";
            otaServer.send(500, "text/plain", message);
            return;
        }

        otaServer.send(200, "text/html",
                       "<!doctype html><html><body><h1>Update installed</h1>"
                       "<p>BrokenSignal Pro is rebooting.</p></body></html>");
        rebootPending = true;
        rebootAtMs = millis() + OTA_REBOOT_DELAY_MS;
        drawOtaUpdateScreen();
    }, []()
    {
        if (localWebAuthEnabled &&
            !otaServer.authenticate("admin", otaPassword.c_str()))
            return;
        HTTPUpload &upload = otaServer.upload();
        sessionActivityMs = millis();

        switch (upload.status)
        {
        case UPLOAD_FILE_START:
            otaError = "";
            uploadSucceeded = false;
            uploadActive = true;
            uploadedBytes = 0;
            displayedBytes = 0;
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
            {
                setUploadError(String("Begin failed: ") + Update.errorString());
                return;
            }
            drawOtaUpdateScreen();
            break;

        case UPLOAD_FILE_WRITE:
            if (!uploadActive)
                return;
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                Update.abort();
                setUploadError(String("Write failed: ") + Update.errorString());
                return;
            }
            uploadedBytes += upload.currentSize;
            if (uploadedBytes >= displayedBytes + (64U * 1024U))
            {
                displayedBytes = uploadedBytes;
                drawOtaUpdateScreen();
            }
            break;

        case UPLOAD_FILE_END:
            if (!uploadActive)
                return;
            uploadActive = false;
            if (!Update.end(true))
            {
                setUploadError(String("Verify failed: ") + Update.errorString());
                return;
            }
            uploadSucceeded = true;
            drawOtaUpdateScreen();
            break;

        case UPLOAD_FILE_ABORTED:
            if (Update.isRunning())
                Update.abort();
            setUploadError("Upload cancelled by browser");
            break;

        default:
            break;
        }
    });

    otaServer.onNotFound([]()
    {
        otaServer.send(404, "text/plain", "Not found");
    });

    handlersConfigured = true;
}
}

bool beginOtaUpdate()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;

    configureHandlers();
    if (serverRunning)
        otaServer.stop();

    otaPassword = localWebAuthEnabled
        ? String(100000UL + (esp_random() % 900000UL)) : String();
    otaError = "";
    uploadActive = false;
    uploadSucceeded = false;
    rebootPending = false;
    uploadedBytes = 0;
    displayedBytes = 0;
    sessionActivityMs = millis();
    sessionActive = true;

    otaServer.begin();
    serverRunning = true;
    mdnsRunning = MDNS.begin("brokensignal");
    if (mdnsRunning)
        MDNS.addService("http", "tcp", OTA_PORT);
    drawOtaUpdateScreen();
    return true;
}

void tickOtaUpdate()
{
    if (!sessionActive)
        return;

    if (serverRunning)
        otaServer.handleClient();

    if (rebootPending && (long)(millis() - rebootAtMs) >= 0)
    {
        delay(50);
        ESP.restart();
    }

    if (!uploadActive && !rebootPending && serverRunning &&
        millis() - sessionActivityMs >= OTA_SESSION_TIMEOUT_MS)
    {
        otaServer.stop();
        serverRunning = false;
        if (mdnsRunning)
        {
            MDNS.end();
            mdnsRunning = false;
        }
        otaError = "Session expired";
        drawOtaUpdateScreen();
    }
}

void stopOtaUpdate()
{
    if (!sessionActive || uploadActive || rebootPending)
        return;

    if (serverRunning)
        otaServer.stop();
    if (mdnsRunning)
        MDNS.end();
    serverRunning = false;
    mdnsRunning = false;
    sessionActive = false;
    otaPassword = "";
    otaError = "";
}

bool otaUpdateActive()
{
    return sessionActive;
}

bool otaUpdateInProgress()
{
    return uploadActive || rebootPending;
}

void drawOtaUpdateScreen()
{
    if (!sessionActive)
        return;

    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = "Firmware Update";

    if (rebootPending)
    {
        model.items = {"Update installed", "Rebooting..."};
    }
    else if (uploadSucceeded)
    {
        model.items = {"Verifying upload", "Please wait..."};
    }
    else if (uploadActive)
    {
        model.items = {"Receiving firmware", "Received: " + String(uploadedBytes / 1024U) + " KB"};
    }
    else if (otaError.length() > 0)
    {
        model.items = {"Update unavailable", otaError};
        model.confirmText = "[Esc]Close";
    }
    else
    {
        const String address = mdnsRunning ? "http://brokensignal.local/"
                                           : "http://" + WiFi.localIP().toString() + "/";
        if (localWebAuthEnabled)
            model.items = {address, "User: admin", "Code: " + otaPassword};
        else
            model.items = {address, "No sign-in required"};
        model.confirmText = "5 min window  [Esc]Cancel";
    }

    drawOverlay(model);
}
