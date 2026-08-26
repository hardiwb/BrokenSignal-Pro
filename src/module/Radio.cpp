#include "core/System.h"
#include "core/AudioFileSourceHTTPSStream.h"
#include "core/Keyboard.h"
#include "core/Utils.h"
#include "module/Browser.h"
#include "module/Radio.h"
#include "module/service/WiFi.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"
#include "UI/Themes.h"

extern bool radioForceAac;

#if DEBUG_SERIAL
#define RDBG(...) Serial.printf(__VA_ARGS__)
#else
#define RDBG(...) ((void)0)
#endif

//==================================================
// RADIO LOGIC HELPERS
//==================================================

static void populateRadioListModel(ListModel &model)
{
    model.selected = radioSelected;
    model.scrollTop = radioScrollTop;

    for (int i = 0; i < radioCount; i++)
    {
        ListItemModel item;
        item.label = radioList[i].name;
        item.isSelected = i == radioSelected;
        item.isActive = i == radioPlaying && radioIsPlaying;
        model.items.push_back(item);
    }
}

static void redrawRadioSelection(int oldSel, int oldScroll)
{
    if (radioScrollTop != oldScroll)
    {
        drawRadioList();
        return;
    }

    drawRadioRow(oldSel);
    drawRadioRow(radioSelected);
}

static void toggleSelectedRadioPlayback()
{
    if (radioIsPlaying &&
        radioPlaying == radioSelected)
    {
        int oldPlaying = radioPlaying;

        stopRadioStream();

        drawRadioRow(oldPlaying);
        drawRadioHeader();
        drawRadioStatus();
        return;
    }

    if (radioCount > 0)
        startRadioStream(radioSelected);
}

//==================================================
// RADIO DATABASE
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
// RADIO STREAM LOGIC
//==================================================

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
// RADIO UI
//==================================================

void drawRadioHeader()
{
    String stationLine = "SELECT STATION";
    if (radioIsPlaying && radioPlaying >= 0 && radioPlaying < radioCount)
        stationLine = radioList[radioPlaying].name;

    HeaderModel model;
    model.mode = "RADIO";
    model.title = stationLine;
    model.cursor = radioIsPlaying && cursorVisible;

    drawHeader(model);

    if (hdrMsgEnd > 0 && millis() < hdrMsgEnd)
        drawHeaderMessage(hdrMsg);
}

void drawRadioRow(int idx)
{
    if (idx < 0 || idx >= radioCount)
        return;

    ListModel model;
    populateRadioListModel(model);
    drawListRow(model, idx);
}

void drawRadioList()
{
    ListModel model;
    populateRadioListModel(model);
    drawList(model);
}

void drawRadioStatus()
{
    FooterModel model;
    model.left = "[A]Add [X]Rm";
    model.center = radioIsPlaying ? "Live" : "Idle";
    model.battery = footerBatteryText();

    drawFooter(model);
}

void drawRadioAll()
{
    drawRadioHeader();
    drawRadioList();
    drawRadioStatus();
}

//==================================================
// RADIO OVERLAY UI
//==================================================

void drawAddUrlOverlay(bool inputOnly)
{
    OverlayModel model;
    model.type = OverlayType::TextInput;
    model.title = "ADD RADIO STATION";
    model.prompt = "Enter stream URL";
    model.value = String(inputBuf);
    model.confirmText = "[Ent]Next   [Del]Back   [Esc]Cancel";

    if (inputOnly)
        drawOverlayInputValue(model.value);
    else
        drawOverlay(model);
}

void drawAddNameOverlay(bool inputOnly)
{
    OverlayModel model;
    model.type = OverlayType::TextInput;
    model.title = "STATION NAME";
    model.prompt = "Name or ENTER to accept";
    model.value = String(inputBuf);
    model.confirmText = "[Ent]Add   [Del]Back   [Esc]Cancel";

    if (inputOnly)
        drawOverlayInputValue(model.value);
    else
        drawOverlay(model);
}

void drawRemoveConfirm()
{
    auto &D = M5Cardputer.Display;
    drawOverlayFrame("REMOVE STATION?");

    String name = (radioCount > 0 && radioSelected < radioCount)
                      ? radioList[radioSelected].name
                      : "?";
    if ((int)name.length() > 28)
        name = name.substring(0, 27) + ">";

    D.setTextDatum(middle_center);
    D.setTextColor(T->textBright);
    D.drawString(name, SCREEN_W / 2, 55, &fonts::Font0);
    D.setTextColor(T->textDim);
    D.drawString("This will be deleted.", SCREEN_W / 2, 75, &fonts::Font0);
    D.setTextColor(T->accent1);
    D.drawString("[Ent]Remove", SCREEN_W / 2, 100, &fonts::Font0);
    D.setTextColor(T->textMid);
    D.drawString("[Esc/Del]Cancel", SCREEN_W / 2, 114, &fonts::Font0);
}

//==================================================
// RADIO OVERLAY STATE
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

bool radioOverlayActive()
{
    return addUrlOverlayVisible ||
           addNameOverlayVisible ||
           removeConfirmVisible;
}

//==================================================
// RADIO NAVIGATION
//==================================================

void radioScrollEnsureVisible()
{
    if (radioSelected < radioScrollTop)
        radioScrollTop = radioSelected;
    if (radioSelected >= radioScrollTop + LIST_VISIBLE_ITEM)
        radioScrollTop = radioSelected - LIST_VISIBLE_ITEM + 1;
    if (radioScrollTop < 0)
        radioScrollTop = 0;
}

//==================================================
// RADIO OVERLAY INPUT
//==================================================

void handleRadioOverlayInput(Keyboard_Class::KeysState &ks)
{
    if (addUrlOverlayVisible)
    {
        if (keyboardBackPressed(ks))
        {
            addUrlOverlayVisible = false;
            drawRadioAll();
            return;
        }

        for (auto c : ks.word)
        {
            if (keyboardTextInputChar(ks, c) && inputLen < RADIO_INPUT_MAX)
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
        if (keyboardBackPressed(ks))
        {
            addNameOverlayVisible = false;
            drawRadioAll();
            return;
        }

        for (auto c : ks.word)
        {
            if (keyboardTextInputChar(ks, c) && inputLen < 31)
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
                radioScrollTop = max(0, radioSelected - LIST_VISIBLE_ITEM + 1);
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
        if (keyboardBackPressed(ks) || ks.del)
        {
            removeConfirmVisible = false;
            drawRadioAll();
        }
    }
}

//==================================================
// RADIO INPUT
//==================================================

void handleRadioInput(Keyboard_Class::KeysState &ks)
{
    if (keyboardBackPressed(ks))
    {
        exitWebRadioMode();
        return;
    }

    if (ks.enter)
    {
        if (radioCount == 0)
        {
            showAddUrlOverlay();
            return;
        }

        toggleSelectedRadioPlayback();

        return;
    }

    for (auto c : ks.word)
    {
        switch (c)
        {
        case ';':
            if (radioCount > 0)
            {
                int oldSel = radioSelected;
                int oldScroll = radioScrollTop;

                radioSelected =
                    (radioSelected - 1 + radioCount) %
                    radioCount;

                radioScrollEnsureVisible();
                redrawRadioSelection(oldSel, oldScroll);
            }

            return;

        case '.':
            if (radioCount > 0)
            {
                int oldSel = radioSelected;
                int oldScroll = radioScrollTop;

                radioSelected =
                    (radioSelected + 1) %
                    radioCount;

                radioScrollEnsureVisible();
                redrawRadioSelection(oldSel, oldScroll);
            }

            return;

        case ' ':
            toggleSelectedRadioPlayback();
            return;

        case 'a':
        case 'A':
            if (radioCount < RADIO_MAX)
                showAddUrlOverlay();
            else
                showHdrMsg("LIST FULL");

            return;

        case 'x':
        case 'X':
            if (radioCount > 0)
                showRemoveConfirm();

            return;

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

        case '+':
        case '=':
            volume =
                (uint8_t)min(255, (int)volume + 10);

            M5Cardputer.Speaker.setVolume(volume);

            settingsDirty = true;
            settingsDirtyMs = millis();

            drawRadioStatus();
            showVolumeMessage();

            return;

        case '-':
            volume =
                (uint8_t)max(0, (int)volume - 10);

            M5Cardputer.Speaker.setVolume(volume);

            settingsDirty = true;
            settingsDirtyMs = millis();

            drawRadioStatus();
            showVolumeMessage();

            return;

        case 'i':
        case 'I':
            radioForceAac = !radioForceAac;

            showHdrMsg(
                radioForceAac
                    ? "FORCE AAC"
                    : "AAC OFF");

            drawRadioStatus();

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

    if (ensureWifiForRadio() == WifiStartupResult::Connected)
    {
        drawRadioAll();
        return;
    }
}

void exitWebRadioMode()
{
    purgeRadioMemory();
    webRadioMode = false;
    closeWifiInput();
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
