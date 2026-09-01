#include "core/System.h"
#include "core/State.h"
#include "apps/music/MusicBrowser.h"
#include "module/service/FileBrowser.h"
#include "apps/music/MusicPlayer.h"
#include "apps/radio/Radio.h"
#include "module/service/WiFi.h"

//==================================================
// RADIO MODE
//==================================================

void enterWebRadioMode()
{
    calculatorVisible = false;
    calculatorOverlayMode = false;
    notesMode = false;
    rememberLastOpenedApp(HostApp::Radio);
    purgeAudioPlayerMemory();
    webRadioMode = true;
    radioSelected = 0;
    radioScrollTop = 0;
    loadRadioList();

    if (ensureWifiConnected() == WifiStartupResult::Connected)
    {
        drawRadioAll();
        return;
    }
}

void exitWebRadioMode()
{
    rememberLastOpenedApp(HostApp::Music);
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
    useMusicFileBrowser();
    scanFileBrowserRoot();
    loadFolderIdx(0);
    drawAll();
}
