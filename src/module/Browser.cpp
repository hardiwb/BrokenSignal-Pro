#include "UI/UI.h"
#include "module/Browser.h"
#include "module/Player.h"
#include <algorithm>


//==================================================
// CACHE / LRU
//==================================================

bool naturalLess(const String &a, const String &b)
{
    int ia = 0;
    int ib = 0;

    while (ia < (int)a.length() &&
           ib < (int)b.length())
    {
        if (isdigit(a[ia]) && isdigit(b[ib]))
        {
            unsigned long na = 0;
            unsigned long nb = 0;

            while (ia < (int)a.length() && isdigit(a[ia]))
            {
                na = na * 10 + (a[ia] - '0');
                ia++;
            }

            while (ib < (int)b.length() && isdigit(b[ib]))
            {
                nb = nb * 10 + (b[ib] - '0');
                ib++;
            }

            if (na != nb)
                return na < nb;
        }
        else
        {
            char ca = tolower(a[ia]);
            char cb = tolower(b[ib]);

            if (ca != cb)
                return ca < cb;

            ia++;
            ib++;
        }
    }

    return a.length() < b.length();
}

static void lruMoveToFront(int idx)
{
    for (int i = 0; i < scanLRUCount; i++)
    {
        if (scanLRU[i] == idx)
        {
            for (int j = i; j > 0; j--)
                scanLRU[j] = scanLRU[j - 1];
            scanLRU[0] = idx;
            return;
        }
    }
}

//==================================================
// FOLDER SCANNING
//==================================================
void scanFolderNow(int idx)
{
    if (idx < 0 || idx >= (int)allFolders.size())
        return;
    FolderEntry &fe = allFolders[idx];

    bool needsDirectoryScan = !fe.nameCacheReady;
    if (fe.scanned && fe.trackWindowPage == folderPage)
    {
        lruMoveToFront(idx);
        return;
    }

    if (needsDirectoryScan)
    {
        if (scanLRUCount >= SCAN_CACHE_MAX)
        {
            for (int i = scanLRUCount - 1; i >= 0; i--)
            {
                int evict = scanLRU[i];
                if (evict == 0 || evict == currentFolderIdx)
                    continue;
                allFolders[evict].tracks.clear();
                allFolders[evict].tracks.shrink_to_fit();
                allFolders[evict].nameCache.clear();
                allFolders[evict].nameCache.shrink_to_fit();
                allFolders[evict].scanned = false;
                allFolders[evict].nameCacheReady = false;
                allFolders[evict].nameCacheStart = 0;
                allFolders[evict].totalItems = 0;
                allFolders[evict].trackWindowPage = -1;
                for (int j = i; j < scanLRUCount - 1; j++)
                    scanLRU[j] = scanLRU[j + 1];
                scanLRUCount--;
                break;
            }
        }
        for (int j = min(scanLRUCount, SCAN_CACHE_MAX - 1); j > 0; j--)
            scanLRU[j] = scanLRU[j - 1];
        scanLRU[0] = idx;
        if (scanLRUCount < SCAN_CACHE_MAX)
            scanLRUCount++;
    }
    else
    {
        lruMoveToFront(idx);
    }

    std::vector<String> subDirs, subDirNames;
    int totalTrackCount = 0;

    if (needsDirectoryScan)
    {
        fe.nameCache.clear();
        File dir = SD.open(fe.path.c_str());
        if (!dir || !dir.isDirectory())
            return;

        int cacheStartTrack = folderPage * PAGE_SIZE;
        File f;
        while ((f = dir.openNextFile()))
        {
            String raw = f.name();
            String nm = raw;
            int sl = raw.lastIndexOf('/');
            if (sl >= 0)
                nm = raw.substring(sl + 1);
            if (nm.isEmpty() || nm.startsWith("."))
            {
                f.close();
                continue;
            }
            String lo = nm;
            lo.toLowerCase();
            String fullPath = fe.path + "/" + nm;

            if (f.isDirectory() && lo == "_radio")
            {
                f.close();
                continue;
            }

            if (f.isDirectory())
            {
                subDirs.push_back(fullPath);
                subDirNames.push_back(nm);
            }
            else if (lo.endsWith(".mp3") || lo.endsWith(".m4a"))
            {
                totalTrackCount++;
                NameEntry ne;
                ne.fullPath = fullPath;
                ne.fileSize = f.size();
                ne.isM4A = lo.endsWith(".m4a");
                int dot = nm.lastIndexOf('.');
                ne.label = (dot > 0) ? nm.substring(0, dot) : nm;
                ne.label.replace('_', ' ');
                auto pos = std::lower_bound(
                    fe.nameCache.begin(),
                    fe.nameCache.end(),
                    ne,
                    [](const NameEntry &a, const NameEntry &b)
                    {
                        return naturalLess(a.label, b.label);
                    });
                fe.nameCache.insert(pos, ne);
                if ((int)fe.nameCache.size() > cacheStartTrack + NAME_CACHE_MAX)
                    fe.nameCache.pop_back();
            }
            f.close();
        }
        dir.close();

        if (cacheStartTrack > 0 && cacheStartTrack < (int)fe.nameCache.size())
            fe.nameCache.erase(fe.nameCache.begin(), fe.nameCache.begin() + cacheStartTrack);
        if ((int)fe.nameCache.size() > NAME_CACHE_MAX)
            fe.nameCache.resize(NAME_CACHE_MAX);
        allFolders[idx].nameCacheStart = cacheStartTrack;

        std::vector<std::pair<String, String>> pairs;
        for (int i = 0; i < (int)subDirs.size(); i++)
            pairs.push_back({subDirNames[i], subDirs[i]});
        std::sort(
            pairs.begin(),
            pairs.end(),
            [](const std::pair<String, String> &a,
               const std::pair<String, String> &b)
            {
                return naturalLess(a.first, b.first);
            });
        for (int i = 0; i < (int)pairs.size(); i++)
        {
            subDirNames[i] = pairs[i].first;
            subDirs[i] = pairs[i].second;
        }

        allFolders[idx].subFolderIds.clear();
        for (int i = 0; i < (int)subDirs.size(); i++)
        {
            int subIdx = -1;
            for (int j = 0; j < (int)allFolders.size(); j++)
            {
                if (allFolders[j].path == subDirs[i])
                {
                    subIdx = j;
                    break;
                }
            }
            if (subIdx < 0)
            {
                FolderEntry stub;
                stub.path = subDirs[i];
                stub.label = subDirNames[i];
                stub.scanned = false;
                subIdx = (int)allFolders.size();
                allFolders.push_back(stub);
            }
            allFolders[idx].subFolderIds.push_back(subIdx);
        }
        fe.nameCacheReady = true;
    }

    int numSubs = (int)allFolders[idx].subFolderIds.size();
    int numCachedTracks = (int)allFolders[idx].nameCache.size();

    if (needsDirectoryScan)
    {
        allFolders[idx].totalItems = numSubs + totalTrackCount;
    }
    int total = allFolders[idx].totalItems;

    int winStart = folderPage * PAGE_SIZE;
    int tStart = max(0, winStart - numSubs) - allFolders[idx].nameCacheStart;
    int tEnd = min(tStart + PAGE_SIZE, numCachedTracks);

    allFolders[idx].tracks.clear();
    for (int i = tStart; i < tEnd; i++)
    {
        const NameEntry &ne = allFolders[idx].nameCache[i];
        TrackEntry te;
        te.path = ne.fullPath;
        te.fileSize = ne.fileSize;
        te.isM4A = ne.isM4A;
        te.label = ne.label;
        te.durationMs = ne.isM4A ? readM4ADuration(ne.fullPath.c_str()) : readMP3Duration(ne.fullPath.c_str(), ne.fileSize);
        allFolders[idx].tracks.push_back(te);
    }

    allFolders[idx].trackWindowPage = folderPage;
    allFolders[idx].scanned = true;
}

int scanDir(const String &path, const String &label)
{
    FolderEntry fe;
    fe.path = path;
    fe.label = label;
    fe.scanned = false;
    int myIdx = (int)allFolders.size();
    allFolders.push_back(fe);
    scanFolderNow(myIdx);
    return myIdx;
}

//==================================================
// FOLDER LOADING
//==================================================

void loadFolderIdx(int idx)
{
    if (idx < 0 || idx >= (int)allFolders.size())
        return;

    int nSubs_ = (int)allFolders[idx].subFolderIds.size();
    int tStart_ = max(0, folderPage * PAGE_SIZE - nSubs_);
    bool cacheMiss = allFolders[idx].nameCacheReady && tStart_ >= (int)allFolders[idx].nameCache.size();
    if (cacheMiss)
    {
        allFolders[idx].nameCacheReady = false;
        allFolders[idx].scanned = false;
    }
    bool needsScan = !allFolders[idx].nameCacheReady || allFolders[idx].trackWindowPage != folderPage;
    if (needsScan)
    {
        hdrMsgEnd = 0;
        hdrMsg[0] = '\0';
        M5Cardputer.Display.fillRect(SCREEN_W / 2, 15, SCREEN_W / 2, 14, T->hdrBg);
        M5Cardputer.Display.setTextDatum(middle_right);
        M5Cardputer.Display.setTextColor(T->accent2);
        M5Cardputer.Display.drawString("SCAN...", SCREEN_W - 4, 22, 1);
    }

    isScanning = true;
    scanFolderNow(idx);
    isScanning = false;
    M5Cardputer.update();
    currentFolderIdx = idx;
    viewFolder = allFolders[idx].path;
    currentTrack = -1;
    selectedItem = 0;
    isRecentView = false;
    items.clear();

    if (idx == 0 && recentCount > 0)
    {
        BrowserItem ri;
        ri.isFolder = true;
        ri.path = "__RECENT__";
        ri.label = "RECENT";
        items.push_back(ri);
    }

    const FolderEntry &fe = allFolders[idx];
    int total = fe.totalItems;
    int numSubs = (int)fe.subFolderIds.size();
    int tStart = max(0, folderPage * PAGE_SIZE - numSubs) - fe.nameCacheStart;
    int pageStart = folderPage * PAGE_SIZE;
    int pageEnd = min(pageStart + PAGE_SIZE, total);

    if (pageStart > 0)
    {
        BrowserItem back;
        back.isFolder = true;
        back.path = "__PREV__";
        back.label = "< BACK";
        items.push_back(back);
    }

    for (int flatIdx = pageStart; flatIdx < pageEnd; flatIdx++)
    {
        BrowserItem bi;
        if (flatIdx < numSubs)
        {
            const FolderEntry &sub = allFolders[fe.subFolderIds[flatIdx]];
            bi.isFolder = true;
            bi.path = sub.path;
            bi.label = sub.label;
        }
        else
        {
            int ti = flatIdx - numSubs - tStart;
            if (ti < 0 || ti >= (int)fe.tracks.size())
                continue;
            const TrackEntry &te = fe.tracks[ti];
            bi.isFolder = false;
            bi.path = te.path;
            bi.label = te.label;
            bi.fileSize = te.fileSize;
            bi.durationMs = te.durationMs;
        }
        items.push_back(bi);
    }

    if (pageEnd < total)
    {
        BrowserItem more;
        more.isFolder = true;
        more.path = "__MORE__";
        more.label = "MORE >";
        items.push_back(more);
    }
}

void loadFolder(const String &path)
{
    for (int i = 0; i < (int)allFolders.size(); i++)
    {
        if (allFolders[i].path == path)
        {
            loadFolderIdx(i);
            return;
        }
    }
    loadFolderIdx(0);
}

//==================================================
// NAVIGATION
//==================================================

void enterItem(int idx)
{
    if (idx < 0 || idx >= (int)items.size())
        return;
    if (items[idx].isFolder)
    {
        if (items[idx].path == "__RECENT__")
        {
            stopAudio();
            folderStack.push_back(currentFolderIdx);
            loadRecentView();
            drawAll();
            return;
        }
        if (items[idx].path == "__PREV__")
        {
            stopAudio();
            folderPage--;
            loadFolderIdx(currentFolderIdx);
            selectedItem = (int)items.size() - 1;
            drawAll();
            return;
        }
        if (items[idx].path == "__MORE__")
        {
            stopAudio();
            folderPage++;
            loadFolderIdx(currentFolderIdx);
            selectedItem = 0;
            drawAll();
            return;
        }
        stopAudio();
        for (int i = 0; i < (int)allFolders.size(); i++)
        {
            if (allFolders[i].path == items[idx].path)
            {
                folderStack.push_back(currentFolderIdx);
                folderPage = 0;
                loadFolderIdx(i);
                drawAll();
                return;
            }
        }
    }
    else
    {
        if ((isPlaying || isPaused) && idx == currentTrack)
            stopAudio();
        else
            startTrack(idx);
    }
}

void goBack()
{
    if (folderStack.empty())
        return;
    int parentIdx = folderStack.back();
    folderStack.pop_back();
    stopAudio();
    isRecentView = false;
    folderPage = 0;
    loadFolderIdx(parentIdx);
    drawAll();
}

//==================================================
// RECENT TRACKS
//==================================================

void loadRecentView()
{
    isRecentView = true;
    viewFolder = "__RECENT__";
    currentTrack = -1;
    selectedItem = 0;
    items.clear();
    for (int i = 0; i < recentCount; i++)
    {
        BrowserItem bi;
        bi.isFolder = false;
        bi.path = recentPaths[i];
        bi.fileSize = 0;
        bi.durationMs = 0;

        bool found = false;
        for (const FolderEntry &fe : allFolders)
        {
            if (!fe.scanned)
                continue;
            for (const TrackEntry &te : fe.tracks)
            {
                if (te.path == recentPaths[i])
                {
                    bi.fileSize = te.fileSize;
                    bi.durationMs = te.durationMs;
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }

        if (!found)
        {
            File f = SD.open(recentPaths[i].c_str());
            if (!f)
                continue;
            bi.fileSize = f.size();
            f.close();
            String lo = recentPaths[i];
            lo.toLowerCase();
            if (lo.endsWith(".m4a"))
                bi.durationMs = readM4ADuration(recentPaths[i].c_str());
            else
                bi.durationMs = readMP3Duration(recentPaths[i].c_str(), bi.fileSize);
        }

        int sl = recentPaths[i].lastIndexOf('/');
        String n = (sl >= 0) ? recentPaths[i].substring(sl + 1) : recentPaths[i];
        int dot = n.lastIndexOf('.');
        if (dot > 0)
            n = n.substring(0, dot);
        n.replace('_', ' ');
        if ((int)n.length() > 19)
            n = n.substring(0, 18) + ">";
        bi.label = n;
        items.push_back(bi);
    }
}

void addRecent(const String &path)
{
    int existing = -1;
    for (int i = 0; i < recentCount; i++)
        if (recentPaths[i] == path)
        {
            existing = i;
            break;
        }
    if (existing >= 0)
    {
        for (int i = existing; i > 0; i--)
            recentPaths[i] = recentPaths[i - 1];
    }
    else
    {
        int slots = min(recentCount, RECENT_MAX - 1);
        for (int i = slots; i > 0; i--)
            recentPaths[i] = recentPaths[i - 1];
        if (recentCount < RECENT_MAX)
            recentCount++;
    }
    recentPaths[0] = path;
    saveRecentToSD();
}

void saveRecentToSD()
{
    File f = SD.open("/recent.txt", FILE_WRITE);
    if (!f)
        return;
    for (int i = 0; i < recentCount; i++)
        f.println(recentPaths[i]);
    f.close();
}

void loadRecentFromSD()
{
    recentCount = 0;
    File f = SD.open("/recent.txt", FILE_READ);
    if (!f)
        return;
    while (f.available() && recentCount < RECENT_MAX)
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0)
            recentPaths[recentCount++] = line;
    }
    f.close();
}

//==================================================
// MEMORY MANAGEMENT
//==================================================

void evictFolderCachesForHeap(uint32_t threshold)
{
    int pass = 0;
    while (ESP.getFreeHeap() < threshold && pass < scanLRUCount)
    {
        for (int i = scanLRUCount - 1; i >= 0; i--)
        {
            int evict = scanLRU[i];
            if (evict == 0 || evict == currentFolderIdx)
                continue;
            allFolders[evict].tracks.clear();
            allFolders[evict].tracks.shrink_to_fit();
            allFolders[evict].nameCache.clear();
            allFolders[evict].nameCache.shrink_to_fit();
            allFolders[evict].scanned = false;
            allFolders[evict].nameCacheReady = false;
            allFolders[evict].nameCacheStart = 0;
            allFolders[evict].trackWindowPage = -1;
            for (int j = i; j < scanLRUCount - 1; j++)
                scanLRU[j] = scanLRU[j + 1];
            scanLRUCount--;
            break;
        }
        pass++;
    }
}

void purgeAudioPlayerMemory()
{
    stopAudio();
    allFolders.clear();
    allFolders.shrink_to_fit();
    folderStack.clear();
    folderStack.shrink_to_fit();
    items.clear();
    items.shrink_to_fit();
    scanLRUCount = 0;
    currentFolderIdx = 0;
    folderPage = 0;
    isRecentView = false;
}
//==================================================
// KEYBOARD INPUT
//==================================================

bool handleBrowserInput(Keyboard_Class::KeysState &ks)
{
    //==================================================
    // ENTER
    // PLAY / STOP / OPEN
    //==================================================

    if (ks.enter)
    {
        if (selectedItem >= 0 &&
            selectedItem < (int)items.size())
        {
            BrowserItem &item = items[selectedItem];

            // Folder / special item
            if (item.isFolder)
            {
                enterItem(selectedItem);
                return true;
            }

            // Track
            if (isPlaying || isPaused)
            {
                if (currentTrack == selectedItem)
                {
                    // ENTER on current track = STOP
                    stopAudio();
                }
                else
                {
                    // ENTER on another track = switch track
                    stopAudio();
                    startTrack(selectedItem);
                }
            }
            else
            {
                // Nothing playing = PLAY
                startTrack(selectedItem);
            }

            return true;
        }

        // ENTER was pressed, even if there is no item.
        return true;
    }

    //==================================================
    // BACK
    //==================================================

    if (ks.del)
    {
        goBack();
        return true;
    }

    //==================================================
    // NAVIGATION
    //==================================================

    for (auto c : ks.word)
    {
        switch (c)
        {
        //==================================================
        // UP
        //==================================================

        case ';':
            if (!items.empty())
            {
                selectedItem =
                    (selectedItem - 1 + (int)items.size()) %
                    (int)items.size();

                drawTrackList();
            }

            return true;

        //==================================================
        // DOWN
        //==================================================

        case '.':
            if (!items.empty())
            {
                selectedItem =
                    (selectedItem + 1) %
                    (int)items.size();

                drawTrackList();
            }

            return true;

        //==================================================
        // LEFT
        // SEEK BACK / PREVIOUS PAGE
        //==================================================

        case ',':
            if (isPlaying || isPaused)
            {
                seekTrack(-(int)seekSeconds * 1000);
            }
            else if (folderPage > 0)
            {
                stopAudio();
                folderPage--;
                loadFolderIdx(currentFolderIdx);
                selectedItem = 0;
                drawAll();
            }
            else
            {
                goBack();
            }
            return true;

        //==================================================
        // RIGHT
        // SEEK FORWARD / NEXT PAGE
        //==================================================

        case '/':
            if (isPlaying || isPaused)
            {
                seekTrack((int)seekSeconds * 1000);
            }
            else
            {
                int total =
                    allFolders[currentFolderIdx].totalItems;
                if ((folderPage + 1) * PAGE_SIZE < total)
                {
                    stopAudio();
                    folderPage++;
                    loadFolderIdx(currentFolderIdx);
                    selectedItem = 0;
                    drawAll();
                }
            }
            return true;
        }
    }
    //==================================================
    // NOT HANDLED BY BROWSER
    //==================================================
    return false;
}