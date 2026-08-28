#include "apps/radio/Radio.h"
#include "core/AudioFileSourceHTTPSStream.h"
#include "core/State.h"
#include "core/System.h"
#include "apps/music/MusicPlayer.h"
#include "module/service/WiFi.h"
#include "UI/Header.h"

#if DEBUG_SERIAL
#define RDBG(...) Serial.printf(__VA_ARGS__)
#else
#define RDBG(...) ((void)0)
#endif

void purgeRadioMemory()
{
    stopRadioStream();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiConnected = false;
    delay(100);
}

void startRadioStream(int idx)
{
    if (!wifiConnected || idx < 0 || idx >= radioCount)
        return;
    if (audioSource == AudioSource::Music)
        stopAudio();
    int oldPlaying = radioPlaying;
    stopRadioStream();

    String url = radioList[idx].url;
    RDBG("[RADIO] start idx=%d url=%s free=%u\n", idx, url.c_str(), ESP.getFreeHeap());
    String loUrl = url;
    loUrl.toLowerCase();

    if (loUrl.endsWith(".ogg") || loUrl.indexOf("ogg") > 0)
    {
        showHdrMsg("OGG UNSUPPORTED");
        drawRadioAll();
        return;
    }

    // Use the fixed stream for both HTTP and HTTPS to avoid aggressive reconnects
    // and ICY metadata issues on chunked streams.
    httpSrc = new AudioFileSourceHTTPSStream(url.c_str());
    if (!httpSrc)
    {
        showHdrMsg("STREAM ALLOC FAIL");
        drawRadioAll();
        return;
    }
    static_cast<AudioFileSourceHTTPSStream *>(httpSrc)->SetReconnect(true, 0);
    radioBuf = new AudioFileSourceBuffer(httpSrc, RADIO_HTTP_BUF);
    if (!radioBuf)
    {
        showHdrMsg("BUFFER ALLOC FAIL");
        delete httpSrc;
        httpSrc = nullptr;
        drawRadioAll();
        return;
    }

    bool started = false;

    if (loUrl.endsWith(".aac") || loUrl.indexOf("aac") > 0 || loUrl.indexOf("m4a") > 0 || radioForceAac)
    {
        aac = new AudioGeneratorAAC();
        started = aac->begin(radioBuf, output);
        if (!started)
        {
            delete aac;
            aac = nullptr;
        }
    }
    else
    {
        radioMp3 = new AudioGeneratorMP3();
        started = radioMp3->begin(radioBuf, output);

        if (!started)
        {
            delete radioMp3;
            radioMp3 = nullptr;

            delete radioBuf;
            radioBuf = nullptr;
            httpSrc->close();
            delete httpSrc;
            httpSrc = nullptr;

            httpSrc = new AudioFileSourceHTTPSStream(url.c_str());
            static_cast<AudioFileSourceHTTPSStream *>(httpSrc)->SetReconnect(true, 0);
            radioBuf = new AudioFileSourceBuffer(httpSrc, RADIO_HTTP_BUF);

            aac = new AudioGeneratorAAC();
            started = aac->begin(radioBuf, output);
            if (!started)
            {
                delete aac;
                aac = nullptr;
            }
        }
    }

    if (!started)
    {
        if (radioBuf)
        {
            delete radioBuf;
            radioBuf = nullptr;
        }
        if (httpSrc)
        {
            httpSrc->close();
            delete httpSrc;
            httpSrc = nullptr;
        }
        showHdrMsg("STREAM ERROR");
        RDBG("[RADIO] STREAM ERROR (begin failed)\n");
        drawRadioAll();
        return;
    }

    radioPlaying = idx;
    radioIsPlaying = true;
    audioSource = AudioSource::Radio;
    applyWifiPowerSave();
    RDBG("[RADIO] playing codec=%s free=%u\n", aac ? "AAC" : "MP3", ESP.getFreeHeap());
    if (oldPlaying >= 0 && oldPlaying != idx)
        drawRadioRow(oldPlaying);
    drawRadioRow(idx);
    drawRadioHeader();
    drawRadioStatus();
}

void stopRadioStream()
{
    if (radioMp3)
    {
        if (radioMp3->isRunning())
            radioMp3->stop();
        delete radioMp3;
        radioMp3 = nullptr;
    }
    if (aac)
    {
        if (aac->isRunning())
            aac->stop();
        delete aac;
        aac = nullptr;
    }
    if (radioBuf)
    {
        delete radioBuf;
        radioBuf = nullptr;
    }
    if (httpSrc)
    {
        httpSrc->close();
        delete httpSrc;
        httpSrc = nullptr;
    }
    if (output)
        output->stop();
    radioIsPlaying = false;
    radioPlaying = -1;
    if (audioSource == AudioSource::Radio)
        audioSource = AudioSource::None;
    applyWifiPowerSave();
    RDBG("[RADIO] stop\n");
}

void pumpRadioAudio()
{
    if (!radioIsPlaying)
        return;

    if (radioBuf)
        radioBuf->loop();

    AudioGenerator *gen = radioMp3 ? (AudioGenerator *)radioMp3 : (aac ? (AudioGenerator *)aac : nullptr);
    if (!gen)
        return;

    if (!gen->isRunning())
    {
        stopRadioStream();
        showHdrMsg("STREAM LOST");
        RDBG("[RADIO] STREAM LOST (gen stopped)\n");
        drawAll();
        return;
    }
    for (int i = 0; i < 4; i++)
    {
        if (!gen->loop())
            break;
    }
}
