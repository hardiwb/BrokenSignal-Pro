#pragma once
#include <Arduino.h>
#include <SD.h>
#include <algorithm>
#include <AudioFileSource.h>
#include <AudioOutput.h>
#include <M5Unified.h>

class AudioFileSourceM4A : public AudioFileSource
{
public:
    AudioFileSourceM4A() : _ok(false)
    {
        memset(&_st, 0, sizeof(_st));
        memset(&_cur, 0, sizeof(_cur));
    }

    bool open(const char *path)
    {
        _f = SD.open(path);
        if (!_f)
            return false;
        _ok = _parse();
        if (!_ok)
        {
            _f.close();
            return false;
        }
        _rewind();
        return true;
    }

    uint32_t read(void *buf, uint32_t len) override
    {
        uint8_t *out = (uint8_t *)buf;
        uint32_t total = 0;
        while (total < len && _cur.sampleIdx < _st.sampleCount)
        {
            if (_st.adtsValid && _cur.posInFrame == 0)
            {
                uint8_t hdr[7];
                _makeAdtsHeader(hdr, _cur.frameSize);
                uint32_t hdrRem = 7 - _cur.adtsBytesOut;
                uint32_t hCopy = min(hdrRem, len - total);
                memcpy(out + total, hdr + _cur.adtsBytesOut, hCopy);
                total += hCopy;
                _cur.adtsBytesOut += hCopy;
                if (_cur.adtsBytesOut < 7)
                    break;
            }
            uint32_t frameRem = _cur.frameSize - _cur.posInFrame;
            uint32_t chunk = min(frameRem, len - total);
            if (chunk == 0)
                break;
            _f.seek(_cur.frameOffset + _cur.posInFrame);
            uint32_t got = _f.read(out + total, chunk);
            total += got;
            _cur.posInFrame += got;
            _cur.streamPos += got;
            if (_cur.posInFrame >= _cur.frameSize)
            {
                _cur.sampleIdx++;
                _cur.posInFrame = 0;
                _cur.adtsBytesOut = 0;
                _advanceFrame();
            }
            if (got < chunk)
                break;
        }
        return total;
    }

    bool seek(int32_t pos, int dir) override
    {
        if (dir == SEEK_SET && pos == 0)
        {
            _rewind();
            return true;
        }
        return false;
    }
    bool close() override
    {
        _f.close();
        _ok = false;
        return true;
    }
    bool isOpen() override { return _ok; }
    uint32_t getSize() override { return _st.totalBytes; }
    uint32_t getPos() override { return _cur.streamPos; }

    uint32_t getCurrentSampleIdx() const { return _cur.sampleIdx; }
    uint32_t getSampleCount() const { return _st.sampleCount; }

    void seekToSample(uint32_t targetSample)
    {
        if (targetSample >= _st.sampleCount)
            targetSample = _st.sampleCount > 0 ? _st.sampleCount - 1 : 0;
        if (_st.sampleCount == 0 || _st.chunkCount == 0)
            return;

        uint32_t samplesBefore = 0;
        uint32_t chunkIdx = 0;
        uint32_t spc = _spcForChunk(1);
        uint32_t sampleInChunk = 0;

        while (chunkIdx < _st.chunkCount)
        {
            if (samplesBefore + spc > targetSample)
            {
                sampleInChunk = targetSample - samplesBefore;
                break;
            }
            samplesBefore += spc;
            chunkIdx++;
            spc = _spcForChunk(chunkIdx + 1);
        }

        if (chunkIdx >= _st.chunkCount)
        {
            chunkIdx = _st.chunkCount - 1;
            sampleInChunk = 0;
        }

        _cur.sampleIdx = targetSample;
        _cur.chunkIdx = chunkIdx;
        _cur.sampleInChunk = sampleInChunk;
        _cur.samplesPerChunk = spc;
        _cur.frameOffset = _st.chunkOffsets[chunkIdx];

        for (uint32_t i = 0; i < sampleInChunk; i++)
        {
            _cur.frameOffset += _sampleSize(targetSample - sampleInChunk + i);
        }

        _cur.frameSize = _sampleSize(targetSample);
        _cur.posInFrame = 0;
        _cur.adtsBytesOut = 0;
        _cur.streamPos = 0;
    }

    bool loadSampleSizes()
    {
        if (_st.defaultSize)
            return true;
        _fillWindow(0);
        return true;
    }

private:
    static const uint32_t STSZ_WIN = 256;
    struct SampleTable
    {
        uint32_t *chunkOffsets;
        uint32_t chunkCount;
        uint32_t stszDataOffset;
        uint32_t sampleCount;
        uint32_t defaultSize;
        uint32_t win[STSZ_WIN];
        uint32_t winBase;
        bool winLoaded;
        static const int MAX_STSC = 64;
        struct StscEntry
        {
            uint32_t firstChunk, spc;
        };
        StscEntry stsc[MAX_STSC];
        uint32_t stscCount;
        uint32_t totalBytes;
        uint8_t adtsSrIdx;
        uint8_t adtsChans;
        bool adtsValid;

        SampleTable() : chunkOffsets(nullptr), chunkCount(0),
                        stszDataOffset(0), sampleCount(0), defaultSize(0),
                        winBase(0), winLoaded(false), stscCount(0), totalBytes(0),
                        adtsSrIdx(4), adtsChans(2), adtsValid(false) {}
        ~SampleTable() { free(chunkOffsets); }
    } _st;

    struct Cursor
    {
        uint32_t sampleIdx, posInFrame, adtsBytesOut, frameOffset, frameSize, streamPos, chunkIdx, sampleInChunk, samplesPerChunk;
    } _cur;

    File _f;
    bool _ok;

    static uint32_t r32(File &f)
    {
        uint8_t b[4];
        f.read(b, 4);
        return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    }
    static uint64_t r64(File &f)
    {
        uint64_t hi = r32(f);
        return (hi << 32) | r32(f);
    }
    static constexpr uint32_t FCC(char a, char b, char c, char d)
    {
        return ((uint32_t)(uint8_t)a << 24) | ((uint32_t)(uint8_t)b << 16) | ((uint32_t)(uint8_t)c << 8) | (uint8_t)d;
    }

    uint32_t findAtom(uint32_t base, uint32_t len, uint32_t target, uint32_t &payloadLen)
    {
        uint32_t cur = base, end = base + len;
        while (cur + 8 <= end)
        {
            _f.seek(cur);
            uint32_t sz = r32(_f);
            uint32_t fcc = r32(_f);
            if (sz < 8)
                break;
            if (fcc == target)
            {
                payloadLen = sz - 8;
                return cur + 8;
            }
            cur += sz;
        }
        return 0;
    }

    void _fillWindow(uint32_t base)
    {
        _st.winBase = base;
        uint32_t n = min(_st.sampleCount - base, STSZ_WIN);
        _f.seek(_st.stszDataOffset + base * 4);
        uint8_t buf[STSZ_WIN * 4];
        _f.read(buf, n * 4);
        for (uint32_t i = 0; i < n; i++)
            _st.win[i] = ((uint32_t)buf[i * 4] << 24) | ((uint32_t)buf[i * 4 + 1] << 16) | ((uint32_t)buf[i * 4 + 2] << 8) | buf[i * 4 + 3];
        _st.winLoaded = true;
    }

    uint32_t _sampleSize(uint32_t si)
    {
        if (_st.defaultSize)
            return _st.defaultSize;
        if (!_st.winLoaded || si < _st.winBase || si >= _st.winBase + STSZ_WIN)
            _fillWindow(si);
        return _st.win[si - _st.winBase];
    }

    uint32_t _spcForChunk(uint32_t chunkNum1)
    {
        uint32_t spc = _st.stsc[0].spc;
        for (int e = (int)_st.stscCount - 1; e >= 0; e--)
            if (chunkNum1 >= _st.stsc[e].firstChunk)
            {
                spc = _st.stsc[e].spc;
                break;
            }
        return spc;
    }

    void _makeAdtsHeader(uint8_t *hdr, uint32_t frameBytes)
    {
        uint32_t frameLen = frameBytes + 7;
        hdr[0] = 0xFF;
        hdr[1] = 0xF1;
        hdr[2] = (uint8_t)(((1 & 0x3) << 6) | ((_st.adtsSrIdx & 0xF) << 2) | ((_st.adtsChans >> 2) & 0x1));
        hdr[3] = (uint8_t)(((_st.adtsChans & 0x3) << 6) | ((frameLen >> 11) & 0x3));
        hdr[4] = (uint8_t)((frameLen >> 3) & 0xFF);
        hdr[5] = (uint8_t)(((frameLen & 0x7) << 5) | 0x1F);
        hdr[6] = 0xFC;
    }

    void _rewind()
    {
        _cur.sampleIdx = 0;
        _cur.posInFrame = 0;
        _cur.adtsBytesOut = 0;
        _cur.streamPos = 0;
        _cur.chunkIdx = 0;
        _cur.sampleInChunk = 0;
        if (_st.sampleCount > 0)
        {
            _cur.samplesPerChunk = _spcForChunk(1);
            _cur.frameOffset = _st.chunkOffsets[0];
            _cur.frameSize = _sampleSize(0);
        }
    }

    void _advanceFrame()
    {
        if (_cur.sampleIdx >= _st.sampleCount)
            return;
        _cur.sampleInChunk++;
        if (_cur.sampleInChunk >= _cur.samplesPerChunk)
        {
            _cur.chunkIdx++;
            _cur.sampleInChunk = 0;
            _cur.samplesPerChunk = _spcForChunk(_cur.chunkIdx + 1);
            _cur.frameOffset = _st.chunkOffsets[_cur.chunkIdx];
        }
        else
        {
            _cur.frameOffset += _cur.frameSize;
        }
        _cur.frameSize = _sampleSize(_cur.sampleIdx);
        _cur.streamPos += _cur.frameSize;
    }

    bool _parse()
    {
        uint32_t fsz = _f.size();
        uint32_t moovLen = 0;
        uint32_t moov = findAtom(0, fsz, FCC('m', 'o', 'o', 'v'), moovLen);
        if (!moov)
            return false;

        uint32_t stblPos = 0, stblLen = 0;
        {
            uint32_t cur = moov, end = moov + moovLen;
            while (cur + 8 <= end)
            {
                _f.seek(cur);
                uint32_t sz = r32(_f);
                uint32_t fcc = r32(_f);
                if (sz < 8)
                    break;
                if (fcc == FCC('t', 'r', 'a', 'k'))
                {
                    uint32_t mdiaLen = 0;
                    uint32_t mdia = findAtom(cur + 8, sz - 8, FCC('m', 'd', 'i', 'a'), mdiaLen);
                    if (mdia)
                    {
                        uint32_t hdlrLen = 0;
                        uint32_t hdlr = findAtom(mdia, mdiaLen, FCC('h', 'd', 'l', 'r'), hdlrLen);
                        if (hdlr && hdlrLen >= 12)
                        {
                            _f.seek(hdlr + 4);
                            r32(_f);
                            uint32_t htype = r32(_f);
                            if (htype == FCC('s', 'o', 'u', 'n'))
                            {
                                uint32_t minfLen = 0;
                                uint32_t minf = findAtom(mdia, mdiaLen, FCC('m', 'i', 'n', 'f'), minfLen);
                                if (minf)
                                    stblPos = findAtom(minf, minfLen, FCC('s', 't', 'b', 'l'), stblLen);
                            }
                        }
                    }
                }
                if (stblPos)
                    break;
                cur += sz;
            }
        }
        if (!stblPos)
            return false;

        {
            bool use64 = false;
            uint32_t plen = 0;
            uint32_t p = findAtom(stblPos, stblLen, FCC('s', 't', 'c', 'o'), plen);
            if (!p)
            {
                p = findAtom(stblPos, stblLen, FCC('c', 'o', '6', '4'), plen);
                if (!p)
                    return false;
                use64 = true;
            }
            _f.seek(p + 4);
            _st.chunkCount = r32(_f);
            _st.chunkOffsets = (uint32_t *)malloc(_st.chunkCount * sizeof(uint32_t));
            if (!_st.chunkOffsets)
                return false;
            for (uint32_t i = 0; i < _st.chunkCount; i++)
                _st.chunkOffsets[i] = use64 ? (uint32_t)r64(_f) : r32(_f);
        }

        {
            uint32_t plen = 0;
            uint32_t p = findAtom(stblPos, stblLen, FCC('s', 't', 's', 'z'), plen);
            if (!p)
                return false;
            _f.seek(p + 4);
            _st.defaultSize = r32(_f);
            _st.sampleCount = r32(_f);
            _st.stszDataOffset = p + 12;
        }

        {
            uint32_t plen = 0;
            uint32_t p = findAtom(stblPos, stblLen, FCC('s', 't', 's', 'c'), plen);
            if (!p)
                return false;
            _f.seek(p + 4);
            uint32_t n = r32(_f);
            _st.stscCount = min(n, (uint32_t)SampleTable::MAX_STSC);
            for (uint32_t i = 0; i < _st.stscCount; i++)
            {
                _st.stsc[i].firstChunk = r32(_f);
                _st.stsc[i].spc = r32(_f);
                r32(_f);
            }
        }

        if (_st.defaultSize)
        {
            _st.totalBytes = (_st.defaultSize + 7) * _st.sampleCount;
        }
        else
        {
            uint32_t sum = 0, n = min(_st.sampleCount, (uint32_t)16);
            _f.seek(_st.stszDataOffset);
            for (uint32_t i = 0; i < n; i++)
                sum += r32(_f);
            _st.totalBytes = (uint32_t)((float)sum / n * (_st.sampleCount + 7));
        }

        _st.adtsValid = false;
        {
            uint32_t stsdLen = 0;
            uint32_t stsd = findAtom(stblPos, stblLen, FCC('s', 't', 's', 'd'), stsdLen);
            if (stsd)
            {
                uint32_t mp4aLen = 0;
                uint32_t mp4a = findAtom(stsd + 8, stsdLen - 8, FCC('m', 'p', '4', 'a'), mp4aLen);
                if (mp4a)
                {
                    _f.seek(mp4a + 16);
                    uint16_t chans = (uint16_t)(_f.read() << 8 | _f.read());
                    _f.seek(mp4a + 24);
                    uint32_t sr = r32(_f) >> 16;
                    const uint32_t srTable[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};
                    uint8_t srIdx = 4;
                    for (int i = 0; i < 13; i++)
                        if (srTable[i] == sr)
                        {
                            srIdx = i;
                            break;
                        }
                    _st.adtsSrIdx = srIdx;
                    _st.adtsChans = (uint8_t)chans;
                    _st.adtsValid = true;
                }
            }
        }
        return _st.sampleCount > 0 && _st.chunkCount > 0;
    }
};

class AudioOutputM5Speaker : public AudioOutput
{
public:
    static const size_t BUF_VALS = 1024 * 2;
    AudioOutputM5Speaker(m5::Speaker_Class *spk, uint8_t ch = 0)
        : _spk(spk), _ch(ch), _wi(0), _wv(0)
    {
        for (int i = 0; i < 3; i++)
        {
            _buf[i] = new int16_t[BUF_VALS];
            memset(_buf[i], 0, BUF_VALS * sizeof(int16_t));
        }
    }
    ~AudioOutputM5Speaker()
    {
        for (int i = 0; i < 3; i++)
            delete[] _buf[i];
    }

    bool begin() override
    {
        _wi = 0;
        _wv = 0;
        return true;
    }
    bool ConsumeSample(int16_t sample[2]) override
    {
        if (_wv >= BUF_VALS)
        {
            flush();
            return false;
        }
        _buf[_wi][_wv++] = sample[0];
        _buf[_wi][_wv++] = sample[1];
        return true;
    }
    void flush() override
    {
        if (_wv == 0)
            return;
        while (!_spk->playRaw(_buf[_wi], _wv, hertz, true, 1, _ch))
            taskYIELD();
        _wi = (_wi < 2) ? _wi + 1 : 0;
        _wv = 0;
    }
    bool stop() override
    {
        flush();
        _spk->stop(_ch);
        return true;
    }
    bool SetRate(int hz) override
    {
        hertz = hz;
        return true;
    }
    bool SetChannels(int ch) override
    {
        channels = ch;
        return true;
    }
    bool SetBitsPerSample(int) { return true; }

private:
    m5::Speaker_Class *_spk;
    uint8_t _ch;
    int16_t *_buf[3];
    int _wi;
    size_t _wv;
};