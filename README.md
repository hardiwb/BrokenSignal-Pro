# BrokenSignal Pro

An audio player, web radio, quick notes, and utility shell for the **Cardputer ADV**.

**Pro stands for Productivity**: fast access to music, radio, notes,
calculations, and everyday utilities from one keyboard-driven interface.

**Current development version:** `v1.2.0-dev`  
**Latest tagged release:** `v1.1.0`

BrokenSignal Pro is a fork of the **BrokenSignal-Next** fork by Rythlan, rebuilt around a compact Glitch Terminal UI for small-screen daily use.

<p align="center">
  <img src="./images/BSPRoSplash.jpg" width="420" alt="BrokenSignal Pro splash screen">
</p>

## Current Status

BrokenSignal Pro is preparing for the `v1.2.0` release. The main UI uses a
modular primitive architecture:

- `Header`
- `List`
- `Footer`
- `Overlay`
- `Toast`

Themes change color and mood, not layout. Music Player, Web Radio, Notes,
Calculator, Control Panel, Help, Debug, and WiFi share the same UI primitives.

## v1.2.0 Development Changes

- Added the Applications shell with Music Player, Web Radio, Notes, and Calculator.
- Added contextual Options for Music, Radio, Notes, and Calculator.
- Standardized modifier routing: `Alt` toggles Applications, `Opt` toggles
  Options, and `Ctrl` toggles Control Panel.
- Expanded Calculator with full-screen history, operator-row editing, decimal
  precision, rounding modes, and optional thousands separators.
- Expanded Notes with Day/Month filtering in Options, date-aware editors, and
  contextual note actions.
- Added granular redraw for two-field overlays and guarded covered screens from
  background refresh.
- Reorganized source modules into `programs`, `shell`, and `service` folders.

## Gallery

<p align="center">
  <img src="./images/BSProApps.jpg" width="360" alt="Applications menu showing Music Player, Web Radio, Notes, and Calculator">
  <br>
  <strong>Applications</strong>
</p>

<p align="center">
  <img src="./images/BSPRoMusic.jpg" width="360" alt="Music player">
  <br>
  <strong>Music Player</strong>
</p>

<p align="center">
  <img src="./images/BSProRadio.jpg" width="360" alt="Web radio">
  <br>
  <strong>Web Radio</strong>
</p>

<p align="center">
  <img src="./images/BSProNote.jpg" width="360" alt="Notes app">
  <br>
  <strong>Notes</strong>
</p>

<p align="center">
  <img src="./images/BSProContextualMenu.jpg" width="360" alt="Contextual Notes Options menu">
  <br>
  <strong>Contextual Options</strong>
</p>

<p align="center">
  <img src="./images/BSProControlPanel.jpg" width="360" alt="System Control Panel">
  <br>
  <strong>Control Panel</strong>
</p>

<p align="center">
  <img src="./images/BSProHelp.jpg" width="360" alt="Context-aware help">
  <br>
  <strong>Context Help</strong>
</p>

<p align="center">
  <img src="./images/BSProCalc.jpg" width="360" alt="Calculator overlay">
  <br>
  <strong>Calculator</strong>
</p>

<p align="center">
  <img src="./images/BSProCalcHistory.jpg" width="360" alt="Calculator history with right-aligned result and thousands separators">
  <br>
  <strong>Calculation History</strong>
</p>

## Features

- **Music playback**: MP3 and M4A/AAC-LC playback from MicroSD.
- **Web radio**: Stream MP3/AAC stations over WiFi.
- **Background audio**: Music or radio can keep playing while opening Settings, Notes, Help, WiFi, Debug, or Calculator.
- **Notes**: Monthly note storage with quick note overlay, day/month filtering, and done-state dimming.
- **Applications**: `Alt` opens a full application list for switching between Music Player, Web Radio, Notes, and Calculator.
- **Contextual Options**: `Opt` opens settings and actions belonging to the active application.
- **Context-aware Help**: `H` opens help in application screens; Calculator reserves `H` for history.
- **Calculator**: `C` opens the calculator overlay; `H` toggles its full-screen calculation history.
- **RTC clock**: DS3231 RTC support with optional NTP sync.
- **WiFi service UI**: WiFi menu is handled by the WiFi service and accessible from Settings.
- **Granular redraw**: Header, list rows, footer slots, progress bar, and overlay input can update independently.
- **Themes**: Five visual themes with persistent settings.
- **Toast primitive**: Small transient popups, currently used for theme name feedback.
- **Power saving**: Brightness setting, screen-off control, and auto screen-off timer.

## UI Architecture

The production UI is built from reusable primitives:

```text
Screen
|-- Header
|-- List
|-- Footer
|-- Overlay / Toast
```

The default layout is Glitch Terminal:

```text
+------------------------------------------+
| Header: app tag, title, WiFi, clock      |
+------------------------------------------+
| List: selected row, values, scrollbar    |
+------------------------------------------+
| Footer: navigation hints          B:95%  |
+------------------------------------------+
```

Footer convention:

```text
Left slot / navigation hints        Battery
```

Music can replace the footer center area with a segmented progress bar.

Application-facing code is organized by responsibility:

```text
src/module/
|-- programs/   Music, Radio, Notes, and Calculator
|-- shell/      Applications, Options, Control Panel, Help, and Debug
`-- service/    Clock and WiFi
```

## Controls

### Global

| Key | Action |
| --- | ------ |
| `H` | Context help; Calculator uses it for history |
| `Opt` | Toggle contextual Options |
| `Alt` | Toggle Applications |
| `Ctrl` | Toggle Control Panel |
| `C` | Calculator |
| `N` | Quick note |
| `Fn+N` | Notes app |
| `D` | Debug |
| `O` | Screen on / off |
| `1` to `5` | Switch theme |
| `+` / `-` | Volume up / down |

Volume changes are shown as a transient header message, for example `VOL 50%`.

### Music

| Key | Action |
| --- | ------ |
| `;` / `.` | Cursor up / down |
| `Ok` | Open folder / play selected track |
| `SPACE` | Pause / resume |
| `DEL` | Parent folder |
| `,` / `/` | Seek back / forward |
| `W` | Switch to Radio |

### Radio

| Key | Action |
| --- | ------ |
| `;` / `.` | Cursor up / down |
| `Ok` / `SPACE` | Play / stop stream |
| `A` | Add station |
| `X` | Remove station |
| `R` | Reconnect |
| `I` | Force AAC |
| `W` / `Esc` | Back to Music |

### Notes

| Key | Action |
| --- | ------ |
| `A` | Add note |
| `R` | Remove note |
| `X` | Toggle done |
| `Ok` | Edit selected note |
| `;` / `.` | Cursor up / down |
| `,` / `/` | Previous / next date |
| `Opt` then `+` / `-` | Change Day/Month filter |
| `T` | Today |
| `U` / `B` | Top / bottom |
| `Esc` | Back |
| `N` | Close Notes |

Notes use monthly files under `/Notes/`.

In the note editor, `Tab` switches between note text and date. `Fn+Up/Down`
changes the date, while `Fn+Left/Right` moves the active field cursor.

### WiFi

WiFi is available from Settings.

| Key | Action |
| --- | ------ |
| `R` | Refresh scan |
| `;` / `.` | Cursor up / down |
| `Ok` | Connect |
| `Esc` / `DEL` | Back / cancel |

### Control Panel

| Key | Action |
| --- | ------ |
| `;` / `.` | Select setting |
| `+` / `-` | Change value |
| `Ok` | Apply / open selected setting |
| `Ctrl` / `Esc` | Close and save |

Settings include:

| Option | Notes |
| ------ | ----- |
| Brightness | Display brightness |
| Volume | System playback volume |
| Screen off timer | Idle screen timeout |
| Deep Sleep timer | Idle deep-sleep timeout |
| Playback timer | Stop playback after a selected interval |
| Theme | Active color theme |
| Time Zone | UTC offset |
| Sync Clock | NTP sync when WiFi is available |
| Manual Clock | Time and date editor |
| WiFi power save | Toggle WiFi modem sleep |
| WiFi menu | Opens WiFi service menu |

### Calculator

| Key | Action |
| --- | ------ |
| `0` to `9` / `.` | Enter number |
| `A` / `+` | Add |
| `S` / `-` | Subtract |
| `X` / `*` | Multiply |
| `D` / `/` | Divide |
| `Ok` | Calculate |
| `DEL` | Backspace |
| `Esc` | Clear / close |
| `H` | Toggle calculation history |
| `Opt` | Toggle Calculator Options |
| `C` | Close |

Calculator history uses `A`, `S`, `M`, and `D` to append addition,
subtraction, multiplication, and division rows. `I` inserts a row above the
current selection. Calculator Options control decimal places, rounding mode,
and thousands separators.

## Themes

| Key | Name |
| --- | ---- |
| `1` | Neon Noir |
| `2` | Glitch Terminal |
| `3` | Corpo Chrome |
| `4` | Miami Vice |
| `5` | Ash |

Theme selection is saved to `/Music/settings.cfg`.

## RTC Module

BrokenSignal Pro supports an external **DS3231 RTC module** connected through I2C.

| DS3231 RTC | Cardputer ADV |
| ---------- | ------------- |
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 13 |
| SCL | GPIO 15 |

<p align="center">
  <img src="./images/RTCwiring.jpeg" width="420" alt="DS3231 RTC wiring">
</p>

The firmware uses:

```cpp
#define RTC_SDA 13
#define RTC_SCL 15
```

## SD Card Layout

```text
SD/
|-- Music/
|   |-- settings.cfg
|   |-- track.mp3
|   |-- track.m4a
|   |-- _radio/
|   |   |-- webradio.cfg
|   |   `-- wifi.cfg
|   `-- Album Folder/
|       |-- 01 - Track.mp3
|       `-- 02 - Track.m4a
|
`-- Notes/
    |-- 2026-08.txt
    |-- 2026-09.txt
    `-- 2026-10.txt
```

Notes are stored per month. Each note line is stored as:

```text
YYYY-MM-DD|-|note text
YYYY-MM-DD|x|done note text
```

`-` means active, `x` means done.

## Build

Build and upload with PlatformIO.

The active tested environment is:

```bash
platformio run -e cardputeradv
```

Dependencies are managed by `platformio.ini`.

## Hardware Requirements

- Cardputer ADV
- MicroSD card
- Optional DS3231 RTC module

## Roadmap

- Add infrared module.
- Add Bluetooth functionality.
- Continue removing legacy UI code.
- Further tune overlay and keyboard input behavior on-device.

## Credits

- Original codebase: [MarcoRR / BrokenSignal](https://github.com/MarcoRR/BrokenSignal)
- BrokenSignal-Next fork by Rythlan (https://github.com/Rythlan/BrokenSignal-Next)
- Audio engine: [earlephilhower / ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio)

## License

This project is licensed under the MIT License.
