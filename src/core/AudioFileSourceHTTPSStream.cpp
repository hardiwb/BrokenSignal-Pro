#include "AudioFileSourceHTTPSStream.h"

AudioFileSourceHTTPSStream::AudioFileSourceHTTPSStream()
{
    pos = 0;
    reconnectTries = 0;
    reconnectDelayMs = 0;
    saveURL[0] = 0;
    client = nullptr;
    isSecure = false;
}

AudioFileSourceHTTPSStream::AudioFileSourceHTTPSStream(const char *url)
{
    saveURL[0] = 0;
    reconnectTries = 0;
    client = nullptr;
    isSecure = false;
    open(url);
}

bool AudioFileSourceHTTPSStream::open(const char *url)
{
    pos = 0;

    // Clean up previous client if reconnecting
    if (client)
    {
        client->stop();
        delete client;
        client = nullptr;
    }

    isSecure = (strncmp(url, "https://", 8) == 0);

    // DYNAMIC CLIENT ALLOCATION
    if (isSecure)
    {
        WiFiClientSecure *secureClient = new WiFiClientSecure();
        secureClient->setInsecure(); // Bypass SSL cert validation for radios
        client = secureClient;
    }
    else
    {
        client = new WiFiClient();
    }

    http.begin(*client, url);
    http.setReuse(true);
    http.setTimeout(5000);

    http.addHeader("Icy-MetaData", "0");
    http.addHeader("User-Agent", "VLC/3.0.16 LibVLC/3.0.16");

#ifndef ESP32
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
#endif

    int code = http.GET();
    if (code != HTTP_CODE_OK && code != HTTP_CODE_MOVED_PERMANENTLY && code != HTTP_CODE_FOUND)
    {
        http.end();
        delete client;
        client = nullptr;
        cb.st(STATUS_HTTPFAIL, "Can't open request");
        return false;
    }
    size = http.getSize();
    strncpy(saveURL, url, sizeof(saveURL));
    saveURL[sizeof(saveURL) - 1] = 0;
    return true;
}

AudioFileSourceHTTPSStream::~AudioFileSourceHTTPSStream()
{
    http.end();
    if (client)
    {
        delete client;
        client = nullptr;
    }
}

uint32_t AudioFileSourceHTTPSStream::read(void *data, uint32_t len)
{
    if (data == NULL)
        return 0;
    return readInternal(data, len, false);
}

uint32_t AudioFileSourceHTTPSStream::readNonBlock(void *data, uint32_t len)
{
    if (data == NULL)
        return 0;
    return readInternal(data, len, true);
}

uint32_t AudioFileSourceHTTPSStream::readInternal(void *data, uint32_t len, bool nonBlock)
{
retry:
    if (!http.connected())
    {
        cb.st(STATUS_DISCONNECTED, "Stream disconnected");
        http.end();
        if (client)
        {
            delete client;
            client = nullptr;
        }
        for (int i = 0; i < reconnectTries; i++)
        {
            cb.st(STATUS_RECONNECTING, "Attempting to reconnect...");
            delay(reconnectDelayMs);
            if (open(saveURL))
            {
                cb.st(STATUS_RECONNECTED, "Stream reconnected");
                break;
            }
        }
        if (!http.connected())
        {
            cb.st(STATUS_DISCONNECTED, "Unable to reconnect");
            return 0;
        }
    }
    if ((size > 0) && (pos >= size))
        return 0;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    NetworkClient *stream = http.getStreamPtr();
#else
    WiFiClient *stream = http.getStreamPtr();
#endif

    if ((size > 0) && (len > (uint32_t)(size - pos)))
    {
        len = size - pos;
    }

    if (!nonBlock)
    {
        int start = millis();
        while ((stream->available() < (int)len) && (millis() - start < 500))
        {
            delay(1);
        }
    }

    size_t avail = stream->available();
    if (!nonBlock && !avail)
    {
        return 0;
    }

    if (avail == 0)
        return 0;
    if (avail < len)
        len = avail;

    int read = stream->read(reinterpret_cast<uint8_t *>(data), len);
    pos += read;
    return read;
}

bool AudioFileSourceHTTPSStream::seek(int32_t pos, int dir)
{
    (void)pos;
    (void)dir;
    return false;
}

bool AudioFileSourceHTTPSStream::close()
{
    http.end();
    if (client)
    {
        delete client;
        client = nullptr;
    }
    return true;
}

bool AudioFileSourceHTTPSStream::isOpen()
{
    return http.connected();
}

uint32_t AudioFileSourceHTTPSStream::getSize()
{
    return size;
}

uint32_t AudioFileSourceHTTPSStream::getPos()
{
    return pos;
}
