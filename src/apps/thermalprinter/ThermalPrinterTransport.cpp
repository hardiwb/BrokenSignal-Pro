#include "apps/thermalprinter/ThermalPrinterTransport.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>

namespace
{
constexpr int API_VERSION = 1;
bool mdnsStarted = false;

String endpoint(const String &server, const char *path)
{
    String base = server;
    while (base.endsWith("/"))
        base.remove(base.length() - 1);
    return base + path;
}

bool resolveLocalUrl(const String &url, String &resolved, String &message)
{
    resolved = url;
    constexpr int schemeLength = 7;
    if (!url.startsWith("http://"))
        return true;

    int authorityEnd = url.indexOf('/', schemeLength);
    if (authorityEnd < 0)
        authorityEnd = url.length();
    int portStart = url.indexOf(':', schemeLength);
    if (portStart < 0 || portStart > authorityEnd)
        portStart = authorityEnd;
    const String host = url.substring(schemeLength, portStart);
    if (!host.endsWith(".local"))
        return true;

    if (!mdnsStarted)
    {
        mdnsStarted = MDNS.begin("brokensignal-pro");
        if (!mdnsStarted)
        {
            message = "Unable to start mDNS";
            return false;
        }
    }
    const String mdnsHost = host.substring(0, host.length() - 6);
    const IPAddress address = MDNS.queryHost(mdnsHost, 3000);
    if (address == IPAddress(0, 0, 0, 0))
    {
        message = "Printer .local name not found";
        return false;
    }
    resolved = "http://" + address.toString() + url.substring(portStart);
    return true;
}

String formEncode(const String &value)
{
    static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
    String encoded;
    encoded.reserve(value.length() * 2);
    for (size_t i = 0; i < value.length(); ++i)
    {
        const uint8_t valueByte = static_cast<uint8_t>(value[i]);
        if ((valueByte >= 'a' && valueByte <= 'z') ||
            (valueByte >= 'A' && valueByte <= 'Z') ||
            (valueByte >= '0' && valueByte <= '9') ||
            valueByte == '-' || valueByte == '_' || valueByte == '.' || valueByte == '~')
        {
            encoded += static_cast<char>(valueByte);
        }
        else if (valueByte == ' ')
        {
            encoded += '+';
        }
        else
        {
            encoded += '%';
            encoded += HEX_DIGITS[valueByte >> 4];
            encoded += HEX_DIGITS[valueByte & 0x0f];
        }
    }
    return encoded;
}

bool decodeResponse(int httpStatus, const String &body, String &message)
{
    if (httpStatus <= 0)
    {
        message = "Printer connection failed " + String(httpStatus);
        return false;
    }

    JsonDocument response;
    if (deserializeJson(response, body))
    {
        message = "Invalid printer response (HTTP " + String(httpStatus) + ")";
        return false;
    }

    const char *detail = httpStatus >= 200 && httpStatus < 300
        ? response["message"] | "Request accepted"
        : response["error"] | "Printer rejected request";
    message = detail;
    return httpStatus >= 200 && httpStatus < 300;
}

bool postForm(const String &url, const String &body, int expectedStatus, String &message)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        message = "Connect WiFi first";
        return false;
    }

    String resolvedUrl;
    if (!resolveLocalUrl(url, resolvedUrl, message))
        return false;

    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    if (!http.begin(resolvedUrl))
    {
        message = "Invalid printer URL";
        return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    const int status = http.POST(body);
    const String response = status > 0 ? http.getString() : "";
    http.end();
    const bool decoded = decodeResponse(status, response, message);
    if (decoded && status != expectedStatus)
    {
        message = "Unexpected HTTP " + String(status);
        return false;
    }
    return decoded;
}
} // namespace

bool fetchThermalPrinterStatus(
    const String &server,
    ThermalPrinterStatus &status,
    String &message)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        message = "Connect WiFi first";
        return false;
    }

    String resolvedUrl;
    if (!resolveLocalUrl(endpoint(server, "/api/status"), resolvedUrl, message))
        return false;

    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    if (!http.begin(resolvedUrl))
    {
        message = "Invalid printer URL";
        return false;
    }
    const int httpStatus = http.GET();
    const String body = httpStatus > 0 ? http.getString() : "";
    http.end();
    if (httpStatus <= 0)
    {
        message = "Printer connection failed " + String(httpStatus);
        return false;
    }
    if (httpStatus != HTTP_CODE_OK)
        return decodeResponse(httpStatus, body, message);

    JsonDocument response;
    if (deserializeJson(response, body))
    {
        message = "Invalid printer status";
        return false;
    }
    if ((response["apiVersion"] | 0) != API_VERSION)
    {
        message = "Unsupported printer API";
        return false;
    }

    status.busy = response["busy"] | false;
    status.queued = response["queued"] | 0;
    status.completed = response["completed"] | 0UL;
    status.failed = response["failed"] | 0UL;
    message = status.busy ? "Printer is busy" : "Printer is ready";
    return true;
}

bool queueThermalPrinterText(
    const String &server,
    const String &text,
    const ThermalPrintLayout &layout,
    String &message)
{
    String body;
    body.reserve(text.length() * 2 + 180);
    body = "text=" + formEncode(text);
    body += "&media=";
    body += layout.label ? "label" : "continuous";
    body += "&bottom=" + String(layout.bottomLines);
    body += "&labelLines=" + String(layout.labelLines);
    body += "&dateHeader=" + formEncode(layout.dateHeader);
    body += "&includeDate=" + String(layout.includeDate ? 1 : 0);
    body += "&center=" + String(layout.center ? 1 : 0);
    body += "&vertical=" + String(layout.vertical ? 1 : 0);
    body += "&bold=" + String(layout.bold ? 1 : 0);
    body += "&double=" + String(layout.doubleSize ? 1 : 0);
    return postForm(endpoint(server, "/api/print/text"), body, HTTP_CODE_ACCEPTED, message);
}

bool queueThermalPrinterAction(
    const String &server,
    const String &action,
    uint8_t lines,
    String &message)
{
    const String body = "action=" + formEncode(action) + "&lines=" + String(lines);
    return postForm(endpoint(server, "/api/action"), body, HTTP_CODE_ACCEPTED, message);
}
