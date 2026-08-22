# BrokenSignal Pro - WORK IN PROGRESS

An audio player and web radio for the **Cardputer ADV**. 

This project is a fork of the original **BrokenSignal** by MarcoRR, refactored from Arduino IDE `.ino` sketches into a organized **PlatformIO** project. It adds a settings menu, adjustable rewind steps, WiFi power-saving settings, screen brightness options, a live debug overlay, and more stable AAC radio stream playback. 

> ⚠️ **Status:** (Work In Progress adding calculator and digital diary)

<img src="./images/BS-NEXT.jpg" width="400" alt="Screenshot">

---

### What's New in this Fork

- **Settings menu** (press `M`): Configure seek/rewind step intervals, toggle WiFi power-save modes, adjust screen brightness, and choose auto screen-off timeouts.
- **Debug overlay** (press `D`): Monitor live diagnostics such as available heap space, min-heap, active codec, network buffer fill level, RSSI, system uptime.
- **HTTPS and robust AAC support**: Integrates a custom stream wrapper that enables HTTPS streaming and improves AAC stream decoding stability. Includes a Force AAC toggle (press `I`) to handle streams that do not broadcast correct MIME-type headers.
- **PlatformIO transition**: Migrated the codebase to a PlatformIO project with a standard `platformio.ini` environment for easier building and dependency management.

---

## Features

- **Help overlay**: Press `H` at any time to open a context-aware help screen for your current mode.
- **MP3 and M4A (AAC-LC) playback**: Plays native iTunes M4A files using a custom MP4 container demuxer, removing the need to convert your library beforehand.
- **Web radio streaming**: Press `W` to jump to radio mode and stream your favorite web stations (MP3/AAC) over WiFi.
- **Folder navigation**: Browse nested directories under `/Music/` with lazy-loading directory scans to ensure quick startups.
- **Large folder support**: Automatically paginates folders containing over 200 tracks into pages of 25.
- **Five visual themes**: Switch themes instantly using keys `1` to `5`, applying cohesive styling to both the media player and radio UI.
- **Repeat modes**: Toggle between off, repeat one, or repeat all.
- **Shuffle**: Play back tracks in random order.
- **Recent tracks**: A virtual playlist showing the 10 most recently played tracks.
- **Persistent settings**: Themes, volume levels, repeat preferences, shuffle states, seek steps, power-saving options, brightness, and auto-dim timers are saved automatically to `/Music/settings.cfg`.
- **Screen off**: Turn off display power to conserve battery life while your audio continues playing in the background.

---

## Themes

| Key | Name | Palette |
|-----|------|---------|
| `1` | Neon Noir | Magenta + cyan on dark background |
| `2` | Glitch Terminal | Phosphor green CRT, amber accents |
| `3` | Corpo Chrome | Gold + chrome on dark slate |
| `4` | Miami Vice | Hot pink + turquoise on dark navy |
| `5` | Ash | Monochrome white-on-black |

---

## Controls

### Music player

| Key | Action |
|-----|--------|
| `;` / `.` | Cursor up / down |
| `ENTER` | Open folder / Play track / Press again to stop |
| `DEL` | Back to parent folder |
| `SPACE` | Pause / Resume / Start playback if idle |
| `,` | Rewind (seek back) / Prev page or parent folder (if browsing and no track is active) |
| `/` | Forward (seek forward) / Next page (if browsing and no track is active) |
| `+` / `=` | Volume up |
| `-` | Volume down |
| `R` | Cycle repeat mode (off -> one -> all) |
| `S` | Toggle shuffle |
| `W` | Switch to web radio mode |
| `O` | Screen on / off |
| `M` | Settings menu |
| `D` | Debug overlay |
| `H` | Help overlay |
| `1`–`5` | Switch theme |

### Web radio

| Key | Action |
|-----|--------|
| `;` / `.` | Scroll up / down the station list |
| `ENTER` / `SPACE` | Play selected station / Press again to stop |
| `A` | Add station (prompts for URL and station name) |
| `X` | Remove selected station (requires confirmation) |
| `I` | Toggle Force AAC codec |
| `R` | Restart stream connection |
| `+` / `=` | Volume up |
| `-` | Volume down |
| `W` / `DEL` | Return to music player |
| `M` | Settings menu |
| `D` | Debug overlay |
| `O` | Screen on / off |
| `H` | Help overlay |
| `1`–`5` | Switch theme |

### WiFi overlay

If no saved network credentials are found on startup, a network selector will display automatically.

| Key | Action |
|-----|--------|
| `;` / `.` | Scroll network list |
| `ENTER` | Select network (prompts for password) |
| `DEL` | Cancel / dismiss |

### Settings menu

| Key | Action |
|-----|--------|
| `;` / `.` | Navigate options |
| `+` / `-` | Cycle values |
| `ENTER` | Toggle settings (e.g. WiFi powersave ON/OFF) |
| `M` / `DEL` | Exit and save |

**Settings options:**

| Option | Values |
|--------|--------|
| Seek step | `10s` (default) - adjustable |
| WiFi power save | ON / OFF |
| Brightness | 0 (off) / 16 / 64 / 128 / 200 / 255 |
| Auto screen off | OFF / 15s / 30s / 60s / 120s |

---

## Hardware Requirements

- **Cardputer ADV**
- **MicroSD card**

---

## SD Card Layout

```
SD/
└── Music/
    ├── track.mp3
    ├── track.m4a
    ├── settings.cfg           ← theme, volume, repeat, shuffle, seek, powersave, brightness, auto screen-off (auto-created)
    ├── _radio/
    │   ├── webradio.cfg       ← saved station list (auto-created)
    │   └── wifi.cfg           ← WiFi credentials (auto-created on first connect)
    └── Album Folder/
        ├── 01 - Track.mp3
        └── 02 - Track.m4a
```

The app handles folder trees to any depth. The system skips the `_radio/` configuration directory automatically during regular music scans. All required configuration files are generated by the software; you do not need to create them manually.

---

## Installation

### M5Launcher

Download the precompiled `.bin` from the releases section, save it to your SD card, and launch it via [M5 Launcher](https://github.com/bmorcelli/Launcher). You can also search for "BrokenSignal" directly in the online OTA repository.

### M5Burner

The build is available for direct flashing on M5Burner.

---

## Build via PlatformIO (Recommended)

### Prerequisites

Install [PlatformIO](https://platformio.org/) for your favorite editor (VS Code, CLI, etc.).

### Build and upload

```bash
platformio run --environment m5stack-stamps3
platformio run --environment m5stack-stamps3 --upload
```

Alternatively, you can build and flash the device using the integrated PlatformIO extension buttons in your IDE.

### Dependencies

Managed automatically inside `platformio.ini`:

| Library | Version |
|---------|---------|
| M5Cardputer | 1.1.1 |
| ESP8266Audio | 2.2.0 |

---

## Screenshots

> TODO

---

## Recommended Stations

### 🟢 Reliable & Stable Streams

These stations utilize continuous streaming configurations that play reliably with the Cardputer's audio buffer:

| Station | Stream URL |
|---------|------------|
| **Radio Paradise (AAC/HTTPS)** | `https://stream.radioparadise.com/aac-128` *(Consistent stability and high quality)* |
| **Laut.fm Ambient (MP3/HTTP)** | `http://ambient.stream.laut.fm/ambient` *(Stable, continuous ambient stream)* |
| **Laut.fm Lofi (MP3/HTTP)** | `http://lofi.stream.laut.fm/lofi` *(Stable lofi stream)* |

---

## Radio Streaming Details

### Protocol Support

Supports HTTP and HTTPS streams with MP3 and AAC decoding. A custom stream wrapper (`AudioFileSourceHTTPSStream`) handles both protocols dynamically: it uses `WiFiClient` for standard HTTP streams and `WiFiClientSecure` for HTTPS, helping save valuable RAM overhead on unencrypted streams.

### Stability Tweaks

- **ICY Metadata Filtering**: An `Icy-MetaData: 0` header is sent with outgoing web requests to prevent decoder desynchronization and pops caused by inline song titles.
- **Graceful Reconnects**: If the network buffer empties temporarily, the wrapper returns 0 instead of immediately closing and reopening the TCP connection. This minimizes reconnect loops, reducing screen freezes and stuttering.
- **Proactive Buffering**: The `radioBuf->loop()` function runs inside the `pumpRadioAudio()` iteration to keep the RAM buffer filled in the background, separating network speeds from audio decoding routines and keeping the UI snappy.
- **5-Second Connection Timeout**: Unresponsive radio streams time out after 5 seconds instead of locking up the system indefinitely.

### Force AAC Codec

Some web radio stations serve AAC streams without defining the proper file extension. Pressing **I** in radio mode overrides auto-detection and forces all streams to be decoded as AAC instead of MP3.

---

## Technical Notes

- **M4A Playback**: The player utilizes a custom MP4 container demuxer (`AudioFileSourceM4A`) that parses the `moov` atom tree, extracts sample tables, and feeds audio frames with pre-pended ADTS headers directly to `AudioGeneratorAAC`. File durations are calculated starting from the end of the file to bypass long, sequential FAT chain walks over large `mdat` data blocks.
- **Custom HTTPS Stream**: `AudioFileSourceHTTPSStream` extends the standard `AudioFileSource` to support dynamic client allocation, ICY metadata filtering, and optimized reconnection behavior. It replaces the stock HTTP stream wrapper to provide uniform stability fixes.

---

## License

This project is licensed under the `MIT License`.

---

## Credits

- Original codebase: [MarcoRR / BrokenSignal](https://github.com/MarcoRR/BrokenSignal)
- Underlying audio engine: [earlephilhower / ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio/tree/master)
