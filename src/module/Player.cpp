#include "UI/UI.h"
#include <M5Cardputer.h>
#include "module/Player.h"
#include "module/Browser.h"
#include "module/Clock.h"
#include <algorithm>

//==================================================
// DURATION
//==================================================

unsigned long readM4ADuration(const char *path)
{
    File f = SD.open(path);
    if (!f)
        return 0;
    uint32_t fsz = f.size();

    auto r32 = [&]() -> uint32_t
    {
        uint8_t b[4];
        f.read(b, 4);
        return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    };
    auto fcc = [](char a, char b, char c, char d) -> uint32_t
    {
        return ((uint32_t)(uint8_t)a << 24) | ((uint32_t)(uint8_t)b << 16) | ((uint32_t)(uint8_t)c << 8) | (uint8_t)d;
    };

    uint32_t moovTag = fcc('m', 'o', 'o', 'v');
    uint32_t moovStart = 0, moovLen = 0;

    if (fsz > 8)
    {
        f.seek(fsz - 8);
        uint32_t lastSz = r32();
        uint32_t lastTag = r32();
        if (lastTag == moovTag && lastSz >= 8 && lastSz <= fsz)
        {
            moovStart = fsz - lastSz + 8;
            moovLen = lastSz - 8;
        }
    }

    if (!moovLen)
    {
        uint32_t pos = 0;
        while (pos + 8 <= fsz)
        {
            f.seek(pos);
            uint32_t sz = r32();
            uint32_t tag = r32();
            if (sz < 8)
                break;
            if (tag == moovTag)
            {
                moovStart = pos + 8;
                moovLen = sz - 8;
                break;
            }
            pos += sz;
        }
    }

    if (!moovLen)
    {
        f.close();
        return 0;
    }

    uint32_t pos = moovStart;
    uint32_t end = moovStart + moovLen;
    while (pos + 8 <= end)
    {
        f.seek(pos);
        uint32_t sz = r32();
        uint32_t tag = r32();
        if (sz < 8)
            break;
        if (tag == fcc('m', 'v', 'h', 'd'))
        {
            f.seek(pos + 8);
            uint8_t version = 0;
            f.read(&version, 1);
            if (version == 0)
            {
                f.seek(pos + 8 + 1 + 3 + 4 + 4);
                uint32_t timescale = r32();
                uint32_t duration = r32();
                f.close();
                if (timescale == 0)
                    return 0;
                return (unsigned long)((uint64_t)duration * 1000 / timescale);
            }
            else
            {
                f.seek(pos + 8 + 1 + 3 + 8 + 8);
                uint32_t timescale = r32();
                uint64_t hi = r32();
                uint64_t lo = r32();
                uint64_t duration = (hi << 32) | lo;
                f.close();
                if (timescale == 0)
                    return 0;
                return (unsigned long)(duration * 1000 / timescale);
            }
        }
        pos += sz;
    }
    f.close();
    return 0;
}

unsigned long readMP3Duration(const char *path, size_t fileSize)
{
    File f = SD.open(path);
    if (!f)
        return 0;

    uint32_t audioStart = 0;
    uint8_t hdr[10];
    f.read(hdr, 10);
    if (hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3')
    {
        uint32_t id3Size = ((uint32_t)(hdr[6] & 0x7F) << 21) | ((uint32_t)(hdr[7] & 0x7F) << 14) |
                           ((uint32_t)(hdr[8] & 0x7F) << 7) | (uint32_t)(hdr[9] & 0x7F);
        audioStart = 10 + id3Size;
    }

    f.seek(audioStart);
    uint8_t b[4];
    f.read(b, 4);
    f.close();

    if (b[0] != 0xFF || (b[1] & 0xE0) != 0xE0)
        return 0;

    static const uint16_t br1[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static const uint16_t br2[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};

    uint8_t version = (b[1] >> 3) & 0x3;
    uint8_t layer = (b[1] >> 1) & 0x3;
    uint8_t brIdx = (b[2] >> 4) & 0xF;

    if (layer != 1)
        return 0;
    uint16_t bitrateKbps = (version == 3) ? br1[brIdx] : br2[brIdx];
    if (bitrateKbps == 0)
        return 0;

    uint32_t audioBytes = fileSize - audioStart;
    return (unsigned long)((uint64_t)audioBytes * 8 / ((uint32_t)bitrateKbps * 1000) * 1000);
}

unsigned long estimateDuration(int idx)
{
    if (idx < 0 || idx >= (int)items.size() || items[idx].isFolder)
        return 3UL * 60 * 1000;
    if (items[idx].durationMs > 0)
        return items[idx].durationMs;

    unsigned long dur = 0;
    String p = items[idx].path;
    String lo = p;
    lo.toLowerCase();

    if (lo.endsWith(".m4a"))
        dur = readM4ADuration(p.c_str());
    else if (lo.endsWith(".mp3"))
        dur = readMP3Duration(p.c_str(), items[idx].fileSize);

    if (dur > 0)
    {
        items[idx].durationMs = dur;
        return dur;
    }
    dur = (unsigned long)(((float)items[idx].fileSize * 8.0f) / (192000.0f / 1000.0f));
    return max(5000UL, dur);
}

//==================================================
// PLAYBACK
//==================================================

void startTrack(int idx)
{
    stopAudio();
    if (helpVisible)
        helpVisible = false;
    if (idx < 0 || idx >= (int)items.size() || items[idx].isFolder)
        return;

    currentTrack = idx;
    selectedItem = idx;
    trackStartMs = millis();
    pausedElapsedMs = 0;
    isPaused = false;
    trackDurationMs = estimateDuration(idx);

    String path = items[idx].path;
    String lo = path;
    lo.toLowerCase();
    if (lo.endsWith(".m4a"))
    {
        evictFolderCachesForHeap(80 * 1024);
        m4aSrc = new AudioFileSourceM4A();
        aac = new AudioGeneratorAAC();
        if (m4aSrc->open(path.c_str()))
        {
            m4aSrc->loadSampleSizes();
            aac->begin(m4aSrc, output);
        }
        else
        {
            delete m4aSrc;
            m4aSrc = nullptr;
            delete aac;
            aac = nullptr;
            isPlaying = false;
            drawAll();
            return;
        }
    }
    else
    {
        evictFolderCachesForHeap(60 * 1024);
        if (isRecentView && !SD.exists(path.c_str()))
        {
            isPlaying = false;
            showHdrMsg("FILE NOT FOUND");
            drawAll();
            return;
        }
        fileSrc = new AudioFileSourceSD(path.c_str());
        id3Src = new AudioFileSourceID3(fileSrc);
        mp3 = new AudioGeneratorMP3();
        mp3->begin(id3Src, output);
    }

    isPlaying = true;
    addRecent(path);
    drawAll();
}

//==================================================
// TRACK SELECTION
//==================================================

int pickNextTrack()
{
    std::vector<int> tracks;
    tracks.reserve(items.size());
    for (int i = 0; i < (int)items.size(); i++)
        if (!items[i].isFolder)
            tracks.push_back(i);
    if (tracks.empty())
        return -1;
    if (shuffleOn && tracks.size() > 1)
    {
        int r;
        do
        {
            r = tracks[random(tracks.size())];
        } while (r == currentTrack);
        return r;
    }
    int n = currentTrack + 1;
    while (n < (int)items.size() && items[n].isFolder)
        n++;
    if (n < (int)items.size())
        return n;
    if (repeatMode == 2)
        return tracks[0];
    return -1;
}

//==================================================
// PLAYBACK CONTROL
//==================================================

void stopAudio()
{
    if (mp3)
    {
        if (mp3->isRunning())
            mp3->stop();
        delete mp3;
        mp3 = nullptr;
    }
    if (aac)
    {
        if (aac->isRunning())
            aac->stop();
        delete aac;
        aac = nullptr;
    }
    if (id3Src)
    {
        delete id3Src;
        id3Src = nullptr;
    }
    if (fileSrc)
    {
        delete fileSrc;
        fileSrc = nullptr;
    }
    if (m4aSrc)
    {
        m4aSrc->close();
        delete m4aSrc;
        m4aSrc = nullptr;
    }
    if (output)
        output->stop();
    isPlaying = false;
    isPaused = false;
}

void pauseAudio()
{
    if (!isPlaying || isPaused)
        return;
    pausedElapsedMs += millis() - trackStartMs;
    M5Cardputer.Speaker.stop(0);
    isPlaying = false;
    isPaused = true;
    drawHeader();
    drawStatus();
}

void resumeAudio()
{
    if (!isPaused || (!mp3 && !aac))
        return;
    output->begin();
    trackStartMs = millis();
    isPlaying = true;
    isPaused = false;
    drawHeader();
    drawStatus();
}

//==================================================
// AUDIO LOOP
//==================================================

void pumpAudio()
{
    if (!isPlaying)
        return;
    AudioGenerator *gen = mp3 ? (AudioGenerator *)mp3 : aac ? (AudioGenerator *)aac
                                                            : nullptr;
    if (!gen)
        return;
    if (!gen->isRunning())
        return;
    if (!gen->loop())
    {
        stopAudio();
        int next = (repeatMode == 1) ? currentTrack : pickNextTrack();
        if (next >= 0)
            startTrack(next);
        else
        {
            currentTrack = -1;
            drawAll();
        }
    }
}

//==================================================
// SEEK
//==================================================

void seekTrack(int delta_ms)
{
    if (!isPlaying && !isPaused)
        return;
    if (currentTrack < 0 || currentTrack >= (int)items.size() || items[currentTrack].isFolder)
        return;

    String path = items[currentTrack].path;
    unsigned long dur = trackDurationMs;
    if (dur == 0)
        return;

    // ----------------------------------------------
    // M4A / AAC
    // ----------------------------------------------

    if (m4aSrc)
    {
        if (m4aSrc->getSampleCount() == 0)
            return;
        int64_t curSample = m4aSrc->getCurrentSampleIdx();
        int64_t targetSample = curSample + ((int64_t)delta_ms * m4aSrc->getSampleCount() / dur);

        if (targetSample >= (int64_t)m4aSrc->getSampleCount())
        {
            int next = (repeatMode == 1) ? currentTrack : pickNextTrack();
            if (next >= 0)
                startTrack(next);
            else
            {
                stopAudio();
                currentTrack = -1;
                drawAll();
            }
            return;
        }
        if (targetSample < 0)
            targetSample = 0;

        m4aSrc->seekToSample((uint32_t)targetSample);

        if (isPaused)
        {
            long newElapsed = (long)pausedElapsedMs + delta_ms;
            pausedElapsedMs = (newElapsed < 0) ? 0 : (unsigned long)newElapsed;
        }
        else
        {
            unsigned long currentElapsed = millis() - trackStartMs;
            if (delta_ms < 0 && currentElapsed < (unsigned long)(-delta_ms))
            {
                trackStartMs = millis(); // Cap at 0:00
            }
            else
            {
                trackStartMs -= delta_ms;
            }
        }

        drawStatus();
    }

    // ----------------------------------------------
    // MP3
    // ----------------------------------------------

    else if (mp3)
    {
        size_t sz = items[currentTrack].fileSize;
        if (sz == 0 && fileSrc)
            sz = fileSrc->getSize();
        if (sz == 0)
            return;

        uint32_t curPos = fileSrc ? fileSrc->getPos() : 0;
        int64_t delta = ((int64_t)sz * delta_ms / dur);
        int64_t newPos = curPos + delta;

        if (newPos >= (int64_t)sz)
        {
            int next = (repeatMode == 1) ? currentTrack : pickNextTrack();
            if (next >= 0)
                startTrack(next);
            else
            {
                stopAudio();
                currentTrack = -1;
                drawAll();
            }
            return;
        }
        if (newPos < 0)
            newPos = 0;

        output->stop();
        if (mp3->isRunning())
            mp3->stop();
        delete mp3;
        mp3 = nullptr;
        if (id3Src)
        {
            delete id3Src;
            id3Src = nullptr;
        }
        if (fileSrc)
        {
            delete fileSrc;
            fileSrc = nullptr;
        }

        fileSrc = new AudioFileSourceSD(path.c_str());
        fileSrc->seek((uint32_t)newPos, SEEK_SET);
        id3Src = new AudioFileSourceID3(fileSrc);
        mp3 = new AudioGeneratorMP3();
        mp3->begin(id3Src, output);
        output->begin();

        if (isPaused)
        {
            pausedElapsedMs += delta_ms;
            M5Cardputer.Speaker.stop(0);
        }
        else
        {
            trackStartMs -= delta_ms;
        }

        drawStatus();
    }
}

//==================================================
// KEYBOARD INPUT
//==================================================

void handlePlayerInput(Keyboard_Class::KeysState &ks)
{
    for (auto c : ks.word)
    {
        switch (c)
        {
        case ' ':
            if (isPlaying)
                pauseAudio();
            else if (isPaused)
                resumeAudio();
            break;

        case 'r':
        case 'R':
            cycleRepeat();
            break;

        case 's':
        case 'S':
            toggleShuffle();
            break;

        case '+':
        case '=':
            volume = (uint8_t)min(255, (int)volume + 10);
            M5Cardputer.Speaker.setVolume(volume);
            settingsDirty = true;
            settingsDirtyMs = millis();
            drawStatus();
            break;

        case '-':
            volume = (uint8_t)max(0, (int)volume - 10);
            M5Cardputer.Speaker.setVolume(volume);
            settingsDirty = true;
            settingsDirtyMs = millis();
            drawStatus();
            break;
        }
    }
}
