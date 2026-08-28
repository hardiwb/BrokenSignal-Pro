#include "apps/radio/Radio.h"
#include "apps/radio/RadioInternal.h"
#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "core/Utils.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"

namespace RadioInternal
{
String radioEditUrl;
String radioEditName;
int radioEditField = 0;
int radioEditUrlCursor = 0;
int radioEditNameCursor = 0;
} // namespace RadioInternal

using namespace RadioInternal;

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

void showAddUrlOverlay()
{
    radioEditUrl = "";
    radioEditName = "";
    radioEditField = 0;
    radioEditUrlCursor = 0;
    radioEditNameCursor = 0;
    addUrlOverlayVisible = true;
    addNameOverlayVisible = false;
    drawAddUrlOverlay();
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

void radioScrollEnsureVisible()
{
    if (radioSelected < radioScrollTop)
        radioScrollTop = radioSelected;
    if (radioSelected >= radioScrollTop + LIST_VISIBLE_ITEM)
        radioScrollTop = radioSelected - LIST_VISIBLE_ITEM + 1;
    if (radioScrollTop < 0)
        radioScrollTop = 0;
}

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

        if (ks.tab)
        {
            radioEditField = 1 - radioEditField;
            drawAddUrlOverlay(true);
            return;
        }

        if (ks.fn)
        {
            for (auto c : ks.word)
            {
                if (c == ',')
                {
                    int &cursor = radioEditField == 0
                                      ? radioEditUrlCursor
                                      : radioEditNameCursor;
                    cursor = max(0, cursor - 1);
                    drawAddUrlOverlay(true);
                    return;
                }
                if (c == '/')
                {
                    String &value = radioEditField == 0
                                        ? radioEditUrl
                                        : radioEditName;
                    int &cursor = radioEditField == 0
                                      ? radioEditUrlCursor
                                      : radioEditNameCursor;
                    cursor = min((int)value.length(), cursor + 1);
                    drawAddUrlOverlay(true);
                    return;
                }
            }
            return;
        }

        String &activeValue = radioEditField == 0
                                  ? radioEditUrl
                                  : radioEditName;
        int &activeCursor = radioEditField == 0
                                ? radioEditUrlCursor
                                : radioEditNameCursor;

        if (ks.del)
        {
            if (activeCursor > 0)
            {
                activeValue.remove(activeCursor - 1, 1);
                activeCursor--;
                drawAddUrlOverlay(true);
            }
            return;
        }

        if (ks.enter)
        {
            radioEditUrl.trim();
            radioEditName.trim();
            if (radioEditUrl.length() == 0)
                return;

            if (radioCount < RADIO_MAX)
            {
                radioList[radioCount].url = radioEditUrl;
                radioList[radioCount].name = radioEditName.length() > 0
                                                 ? radioEditName
                                                 : generateRadioName(radioEditUrl, radioCount + 1);
                radioCount++;
                saveRadioList();
                radioSelected = radioCount - 1;
                radioScrollTop = max(0, radioSelected - LIST_VISIBLE_ITEM + 1);
            }
            addUrlOverlayVisible = false;
            addNameOverlayVisible = false;
            drawRadioAll();
            return;
        }

        for (auto c : ks.word)
        {
            const int maxLength = radioEditField == 0 ? RADIO_INPUT_MAX : 31;
            if (keyboardTextInputChar(ks, c) && (int)activeValue.length() < maxLength)
            {
                activeValue = activeValue.substring(0, activeCursor) +
                              String(c) +
                              activeValue.substring(activeCursor);
                activeCursor++;
                drawAddUrlOverlay(true);
            }
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
        if (keyboardBackPressed(ks))
        {
            removeConfirmVisible = false;
            drawRadioAll();
        }
    }
}

void toggleRadioForceAac()
{
    radioForceAac = !radioForceAac;
    showHdrMsg(radioForceAac ? "FORCE AAC" : "AAC OFF");
}

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
        case 'w':
        case 'W':
            exitWebRadioMode();
            return;

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
            toggleRadioForceAac();
            drawRadioStatus();

            return;
        }
    }
}
