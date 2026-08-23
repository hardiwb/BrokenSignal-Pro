#pragma once
#include <Arduino.h>
#include <AudioFileSource.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

class AudioFileSourceHTTPSStream : public AudioFileSource
{
public:
    AudioFileSourceHTTPSStream();
    AudioFileSourceHTTPSStream(const char *url);
    virtual ~AudioFileSourceHTTPSStream() override;

    virtual bool open(const char *url) override;
    virtual uint32_t read(void *data, uint32_t len) override;
    virtual uint32_t readNonBlock(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override;

    bool SetReconnect(int tries, int delayms)
    {
        reconnectTries = tries;
        reconnectDelayMs = delayms;
        return true;
    }

    static const int STATUS_HTTPFAIL = 2;
    static const int STATUS_DISCONNECTED = 3;
    static const int STATUS_RECONNECTING = 4;
    static const int STATUS_RECONNECTED = 5;
    static const int STATUS_NODATA = 6;

private:
    virtual uint32_t readInternal(void *data, uint32_t len, bool nonBlock);
    WiFiClient *client;
    HTTPClient http;
    int pos;
    int size;
    int reconnectTries;
    int reconnectDelayMs;
    char saveURL[256];
    bool isSecure;
};
