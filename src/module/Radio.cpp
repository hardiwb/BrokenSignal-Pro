#include "UI/UI.h"
#include "UI/WiFiUI.h"
#include "UI/RadioUI.h"
#include "core/AudioFileSourceHTTPSStream.h"
#include "module/Browser.h"
#include "module/Radio.h"
#include "module/WiFi.h"
#include "module/Notes.h"

extern bool radioForceAac;

#if DEBUG_SERIAL
#define RDBG(...) Serial.printf(__VA_ARGS__)
#else
#define RDBG(...) ((void)0)
#endif

//==================================================
// RADIO CONFIG
//==================================================

void loadRadioList()
{
    radioCount = 0;
    File f = SD.open("/Music/_radio/webradio.cfg", FILE_READ);
    if (!f)
        return;
    while (f.available() && radioCount < RADIO_MAX)
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;
        int sep = line.indexOf('|');
        if (sep < 0)
        {
            radioList[radioCount].name = "Radio " + String(radioCount + 1);
            radioList[radioCount].url = line;
        }
        else
        {
            radioList[radioCount].name = line.substring(0, sep);
            radioList[radioCount].url = line.substring(sep + 1);
        }
        if (radioList[radioCount].url.length() > 0)
            radioCount++;
    }
    f.close();
}

void saveRadioList()
{
    SD.mkdir("/Music/_radio");
    File f = SD.open("/Music/_radio/webradio.cfg", FILE_WRITE);
    if (!f)
        return;
    for (int i = 0; i < radioCount; i++)
        f.printf("%s|%s\n", radioList[i].name.c_str(), radioList[i].url.c_str());
    f.close();
}

String generateRadioName(const String &url, int n)
{
    int start = url.indexOf("://");
    if (start >= 0)
    {
        start += 3;
        int end = url.indexOf('/', start);
        String domain = (end > 0) ? url.substring(start, end) : url.substring(start);
        if (domain.startsWith("www."))
            domain = domain.substring(4);
        int dot = domain.lastIndexOf('.');
        if (dot > 0)
            domain = domain.substring(0, dot);
        domain.replace('-', ' ');
        if (domain.length() > 0 && domain.length() <= 20)
        {
            domain.toUpperCase();
            return domain;
        }
    }
    return "Radio " + String(n);
}

//==================================================
// RADIO MEMORY
//==================================================

void purgeRadioMemory()
{
    stopRadioStream();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiConnected = false;
    delay(100);
}

//==================================================
// RADIO STREAM
//==================================================

void startRadioStream(int idx)
{
    if (!wifiConnected || idx < 0 || idx >= radioCount)
        return;
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

    // Use the FIXED class for BOTH HTTP and HTTPS to prevent aggressive reconnects
    // and ICY metadata issues (SomaFM uses chunked encoding with micro-pauses)
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

            // CRITICAL: The MP3 parser consumed bytes from radioBuf.
            // We must recreate the stream so AAC gets a fresh ADTS header.
            delete radioBuf;
            radioBuf = nullptr;
            httpSrc->close();
            delete httpSrc;
            httpSrc = nullptr;

            // Use the fixed class for both HTTP and HTTPS
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
    applyWifiPowerSave();
    RDBG("[RADIO] playing codec=%s free=%u\n", aac ? "AAC" : "MP3", ESP.getFreeHeap());
    if (oldPlaying >= 0 && oldPlaying != idx)
        drawRadioRow(oldPlaying);
    drawRadioRow(idx);
    drawRadioHeader();
    drawRadioStatus();
}

//==================================================
// RADIO STREAM
//==================================================
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
        int oldPlaying = radioPlaying;
        stopRadioStream();
        showHdrMsg("STREAM LOST");
        RDBG("[RADIO] STREAM LOST (gen stopped)\n");
        if (oldPlaying >= 0)
            drawRadioRow(oldPlaying);
        drawRadioHeader();
        drawRadioStatus();
        return;
    }
    for (int i = 0; i < 4; i++)
    {
        if (!gen->loop())
            break;
    }
}

//==================================================
// WIFI
//==================================================
void scanWifiNetworks()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int n = WiFi.scanNetworks(false, false, false, 300);
    wifiNetCount = 0;
    for (int i = 0; i < n && wifiNetCount < WIFI_SCAN_MAX; i++)
    {
        wifiNets[wifiNetCount].ssid = WiFi.SSID(i);
        wifiNets[wifiNetCount].rssi = WiFi.RSSI(i);
        wifiNets[wifiNetCount].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        if (wifiNets[wifiNetCount].ssid.length() > 0)
            wifiNetCount++;
    }
    WiFi.scanDelete();
}

//==================================================
// RADIO OVERLAYS
//==================================================

void showAddUrlOverlay()
{
    inputBuf[0] = '\0';
    inputLen = 0;
    addUrlOverlayVisible = true;
    drawAddUrlOverlay();
}

void showAddNameOverlay(const String &defaultName)
{
    strncpy(inputBuf, defaultName.c_str(), RADIO_INPUT_MAX);
    inputBuf[RADIO_INPUT_MAX] = '\0';
    inputLen = strlen(inputBuf);
    addNameOverlayVisible = true;
    drawAddNameOverlay();
}

void showRemoveConfirm()
{
    removeConfirmVisible = true;
    drawRemoveConfirm();
}

//==================================================
// OVERLAY INPUT
//==================================================

void handleOverlayInput(Keyboard_Class::KeysState &ks)
{
    if (wifiOverlayVisible)
    {
        for (auto c : ks.word)
        {
            if (c == ';' && wifiNetCount > 0)
            {
                wifiNetSel = (wifiNetSel - 1 + wifiNetCount) % wifiNetCount;
                if (wifiNetSel < wifiNetScroll)
                    wifiNetScroll = wifiNetSel;
                drawWifiOverlay();
            }
            if (c == '.' && wifiNetCount > 0)
            {
                wifiNetSel = (wifiNetSel + 1) % wifiNetCount;
                if (wifiNetSel >= wifiNetScroll + 6)
                    wifiNetScroll = wifiNetSel - 5;
                drawWifiOverlay();
            }
            if (c == 's' || c == 'S')
            {
                M5Cardputer.Display.setTextDatum(middle_center);
                M5Cardputer.Display.setTextColor(T->accent2);
                M5Cardputer.Display.drawString("SCANNING...", SCREEN_W / 2, 68, 1);
                scanWifiNetworks();
                wifiNetSel = 0;
                wifiNetScroll = 0;
                drawWifiOverlay();
            }
        }
        if (ks.enter && wifiNetCount > 0)
        {
            String ssid = wifiNets[wifiNetSel].ssid;
            String cfgSsid, cfgPass;
            wifiOverlayVisible = false;
            if (loadWifiConfig(cfgSsid, cfgPass) && cfgSsid == ssid)
            {
                if (connectWifi(ssid, cfgPass))
                    drawRadioAll();
                else
                    showWifiOverlay();
            }
            else
            {
                showWifiPassOverlay(ssid);
            }
        }
        if (ks.del)
        {
            wifiOverlayVisible = false;
            if (!wifiConnected)
                exitWebRadioMode();
            else
                drawRadioAll();
        }
    }
    else if (wifiPassOverlayVisible)
    {
        for (auto c : ks.word)
        {
            if (c >= 32 && c < 127 && inputLen < RADIO_INPUT_MAX)
            {
                inputBuf[inputLen++] = c;
                inputBuf[inputLen] = '\0';
                drawWifiPassOverlay(true);
            }
        }
        if (ks.del)
        {
            if (inputLen > 0)
            {
                inputBuf[--inputLen] = '\0';
                drawWifiPassOverlay(true);
            }
            else
            {
                wifiPassOverlayVisible = false;
                showWifiOverlay();
            }
        }
        if (ks.enter)
        {
            String pass = String(inputBuf);
            wifiPassOverlayVisible = false;
            if (connectWifi(inputSaved, pass))
                drawRadioAll();
            else
                showWifiOverlay();
        }
    }
    else if (addUrlOverlayVisible)
    {
        for (auto c : ks.word)
        {
            if (c >= 32 && c < 127 && inputLen < RADIO_INPUT_MAX)
            {
                inputBuf[inputLen++] = c;
                inputBuf[inputLen] = '\0';
                drawAddUrlOverlay(true);
            }
        }
        if (ks.del)
        {
            if (inputLen > 0)
            {
                inputBuf[--inputLen] = '\0';
                drawAddUrlOverlay(true);
            }
            else
            {
                addUrlOverlayVisible = false;
                drawRadioAll();
            }
        }
        if (ks.enter && inputLen > 0)
        {
            inputSaved = String(inputBuf);
            addUrlOverlayVisible = false;
            String defName = generateRadioName(inputSaved, radioCount + 1);
            showAddNameOverlay(defName);
        }
    }
    else if (addNameOverlayVisible)
    {
        for (auto c : ks.word)
        {
            if (c >= 32 && c < 127 && inputLen < 31)
            {
                inputBuf[inputLen++] = c;
                inputBuf[inputLen] = '\0';
                drawAddNameOverlay(true);
            }
        }
        if (ks.del && inputLen > 0)
        {
            inputBuf[--inputLen] = '\0';
            drawAddNameOverlay(true);
        }
        if (ks.enter)
        {
            if (radioCount < RADIO_MAX)
            {
                radioList[radioCount].url = inputSaved;
                radioList[radioCount].name = (inputLen > 0) ? String(inputBuf) : ("Radio " + String(radioCount + 1));
                radioCount++;
                saveRadioList();
                radioSelected = radioCount - 1;
                radioScrollTop = max(0, radioSelected - VISIBLE_TRACKS + 1);
            }
            addNameOverlayVisible = false;
            drawRadioAll();
        }
    }
    else if (removeConfirmVisible)
    {
        if (ks.enter)
        {
            if (radioSelected == radioPlaying && radioIsPlaying)
                stopRadioStream();
            for (int i = radioSelected; i < radioCount - 1; i++)
                radioList[i] = radioList[i + 1];
            radioCount--;
            if (radioPlaying >= radioCount)
                radioPlaying = -1;
            if (radioSelected >= radioCount)
                radioSelected = max(0, radioCount - 1);
            radioScrollEnsureVisible();
            saveRadioList();
            removeConfirmVisible = false;
            drawRadioAll();
        }
        if (ks.del)
        {
            removeConfirmVisible = false;
            drawRadioAll();
        }
    }
}

//==================================================
// RADIO NAVIGATION / INPUT
//==================================================

void radioScrollEnsureVisible()
{
    if (radioSelected < radioScrollTop)
        radioScrollTop = radioSelected;
    if (radioSelected >= radioScrollTop + VISIBLE_TRACKS)
        radioScrollTop = radioSelected - VISIBLE_TRACKS + 1;
    if (radioScrollTop < 0)
        radioScrollTop = 0;
}

void handleRadioInput(Keyboard_Class::KeysState &ks)
{
    //==================================================
    // BACK / EXIT RADIO
    //==================================================

    if (ks.del)
    {
        exitWebRadioMode();
        return;
    }

    //==================================================
    // ENTER - PLAY / STOP
    //==================================================

    if (ks.enter)
    {
        if (radioCount == 0)
        {
            showAddUrlOverlay();
            return;
        }

        if (radioIsPlaying &&
            radioPlaying == radioSelected)
        {
            int oldPlaying = radioPlaying;

            stopRadioStream();

            drawRadioRow(oldPlaying);
            drawRadioHeader();
            drawRadioStatus();
        }
        else
        {
            startRadioStream(radioSelected);
        }

        return;
    }

    //==================================================
    // KEYBOARD COMMANDS
    //==================================================

    for (auto c : ks.word)
    {
        switch (c)
        {
            //==================================================
            // UP
            //==================================================

        case ';':
            if (radioCount > 0)
            {
                int oldSel = radioSelected;
                int oldScroll = radioScrollTop;

                radioSelected =
                    (radioSelected - 1 + radioCount) %
                    radioCount;

                radioScrollEnsureVisible();

                if (radioScrollTop != oldScroll)
                {
                    drawRadioList();
                }
                else
                {
                    drawRadioRow(oldSel);
                    drawRadioRow(radioSelected);
                }
            }

            return;

            //==================================================
            // DOWN
            //==================================================

        case '.':
            if (radioCount > 0)
            {
                int oldSel = radioSelected;
                int oldScroll = radioScrollTop;

                radioSelected =
                    (radioSelected + 1) %
                    radioCount;

                radioScrollEnsureVisible();

                if (radioScrollTop != oldScroll)
                {
                    drawRadioList();
                }
                else
                {
                    drawRadioRow(oldSel);
                    drawRadioRow(radioSelected);
                }
            }

            return;

            //==================================================
            // SPACE - PLAY / STOP
            //==================================================

        case ' ':
            if (radioIsPlaying &&
                radioPlaying == radioSelected)
            {
                int oldPlaying = radioPlaying;

                stopRadioStream();

                drawRadioRow(oldPlaying);
                drawRadioHeader();
                drawRadioStatus();
            }
            else if (radioCount > 0)
            {
                startRadioStream(radioSelected);
            }

            return;

            //==================================================
            // ADD RADIO
            //==================================================

        case 'a':
        case 'A':
            if (radioCount < RADIO_MAX)
                showAddUrlOverlay();
            else
                showHdrMsg("LIST FULL");

            return;

            //==================================================
            // REMOVE RADIO
            //==================================================

        case 'x':
        case 'X':
            if (radioCount > 0)
                showRemoveConfirm();

            return;

            //==================================================
            // RECONNECT
            //==================================================

        case 'r':
        case 'R':
            if (!wifiConnected)
            {
                showHdrMsg("NO WIFI");
            }
            else if (radioIsPlaying &&
                     radioPlaying >= 0)
            {
                startRadioStream(radioPlaying);
            }
            else
            {
                showHdrMsg("NOT PLAYING");
            }

            return;

            //==================================================
            // VOLUME UP
            //==================================================

        case '+':
        case '=':
            volume =
                (uint8_t)min(255, (int)volume + 10);

            M5Cardputer.Speaker.setVolume(volume);

            settingsDirty = true;
            settingsDirtyMs = millis();

            drawRadioStatus();

            return;

            //==================================================
            // VOLUME DOWN
            //==================================================

        case '-':
            volume =
                (uint8_t)max(0, (int)volume - 10);

            M5Cardputer.Speaker.setVolume(volume);

            settingsDirty = true;
            settingsDirtyMs = millis();

            drawRadioStatus();

            return;

            //==================================================
            // FORCE AAC
            //==================================================

        case 'i':
        case 'I':
            radioForceAac = !radioForceAac;

            showHdrMsg(
                radioForceAac
                    ? "FORCE AAC"
                    : "AAC OFF");

            drawRadioStatus();

            return;
            //==================================================
            // WIFI / CONNECTION
            //==================================================

        case 'c':
        case 'C':
            showWifiOverlay();
            return;
        }
    }
}

//==================================================
// RADIO MODE
//==================================================

void enterWebRadioMode()
{
    purgeAudioPlayerMemory();
    webRadioMode = true;
    radioSelected = 0;
    radioScrollTop = 0;
    loadRadioList();

    if (WiFi.status() == WL_CONNECTED)
    {
        wifiConnected = true;
        wifiSSID = WiFi.SSID();
        drawRadioAll();
        return;
    }

    String cfgSsid, cfgPass;
    if (loadWifiConfig(cfgSsid, cfgPass) && cfgSsid.length() > 0)
    {
        if (connectWifi(cfgSsid, cfgPass))
        {
            drawRadioAll();
            return;
        }
    }
    showWifiOverlay();
}

void exitWebRadioMode()
{
    purgeRadioMemory();
    webRadioMode = false;
    wifiOverlayVisible = false;
    wifiPassOverlayVisible = false;
    addUrlOverlayVisible = false;
    addNameOverlayVisible = false;
    removeConfirmVisible = false;
    helpVisible = false;

    allFolders.clear();
    allFolders.shrink_to_fit();
    scanLRUCount = 0;
    folderStack.clear();
    folderPage = 0;
    currentFolderIdx = 0;
    isRecentView = false;
    scanDir("/Music", "Music");
    loadFolderIdx(0);
    drawAll();
}