# BrokenSignal Pro Vocabulary

This dictionary defines project words as they are used in this firmware. Use it
when a file name, module, or doc says "app", "surface", "metadata", "service",
or another architecture term and the boundary is not obvious.

## Core Terms

| Term | Meaning | Usually lives in | Not the same as |
| --- | --- | --- | --- |
| Firmware | The complete program flashed to the Cardputer. | Whole repository | A single app |
| Runtime | The code that keeps apps, surfaces, input, drawing, settings, and boot behavior coordinated. | `src/core/` | Arduino framework |
| Shell | The firmware UI around apps: app launcher, options, control panel, help, debug, and routing behavior. | `src/module/shell/`, `src/core/Keyboard.cpp`, `src/core/SurfaceManager.cpp` | A host app |
| Host app | A full-screen foreground application registered in the app catalog. | `src/apps/<app>/` | Overlay, modal, menu, quick popup |
| Surface | The currently active UI/input layer. A surface may be a host app, menu, modal, popup, or quick popup. | `src/core/SurfaceManager.*` | A C++ file or app folder |
| Foreground app | The host app currently underneath shell surfaces. | `foregroundApp` in `src/core/State.*` | The topmost surface |
| App catalog | The generated list of available host apps and their callbacks. | `src/core/generated/` | The Applications menu UI |
| App descriptor | One app's runtime contract: ID, open/draw/input/tick callbacks, Help, Options, and quick access. | `src/core/App.h`, generated catalog | App metadata file only |
| Manifest | `app.json`, the folder-local source that generates one app descriptor. | `src/apps/<app>/app.json` | C++ metadata |
| Metadata | Static or descriptor-driven app information such as Help rows and Options rows. | `<App>Metadata.*`, `app.json` | Runtime state |

## UI Terms

| Term | Meaning | Usually lives in | Not the same as |
| --- | --- | --- | --- |
| Applications menu | The shell menu for selecting a host app. | `src/module/shell/Applications.*` | App catalog |
| Options menu | App-owned settings/actions shown by the shell for the foreground app. | `src/module/shell/Options.*`, app `buildOptions()` | Control Panel |
| Control Panel | Shell/system settings such as brightness, sleep, WiFi, clock, and theme. | `src/module/shell/Settings.*` | App Options |
| Modal | A blocking input or confirmation surface that owns keyboard input until closed. | App state plus `SurfaceManager` routing | Popup |
| Overlay | A drawn UI layer over the host screen. Some overlays are modal, some are informational. | `src/UI/Overlay.*`, app modal code | Host app |
| Popup | A temporary or non-editing overlay such as Help or Debug. | `src/module/shell/Help.*`, `Debug.*` | Modal editor |
| Quick popup | A transient visual message such as a toast or header message. | `src/UI/Toast.*`, `showHdrMsg()` | Quick access |
| Quick access | A descriptor-owned shortcut that opens a small tool from another host app. | App `quick_access` manifest field, `AppRuntime.cpp` | Global hotkey |
| Help row | One static key/description pair for an app's Help screen. | `<App>Metadata.cpp` | Keyboard routing |
| Option row | One dynamic app setting/action exposed through the shell Options menu. | `<App>Metadata.cpp` | Control Panel setting |

## Input Terms

| Term | Meaning | Usually lives in | Not the same as |
| --- | --- | --- | --- |
| Keyboard router | The central input layer that reads physical keys and dispatches to the active surface. | `src/core/Keyboard.cpp` | App input handler |
| Input owner | The layer that is allowed to consume the current key event. | Resolved by `SurfaceManager` and `Keyboard.cpp` | Whoever sees the key first |
| Shell navigation | Modifier-only shortcuts for moving between shell menus: `Alt`, `Opt`, and `Ctrl`. | `src/core/Keyboard.cpp` | App-specific hotkeys |
| App-specific hotkey | A key that only means something inside one host app. | `<App>Input.cpp` or app input callback | Quick access launcher |
| Global utility hotkey | A firmware-level action allowed on host screens, such as Help, Debug, or screen toggle. | `src/core/Keyboard.cpp` | Text/editor input |
| Back/Esc | The close/back action. It closes the topmost surface before app input sees it. | `SurfaceManager.cpp`, `Keyboard.cpp` | App-local delete |

## App File Terms

| Term | Meaning | Usually lives in | Not the same as |
| --- | --- | --- | --- |
| `<App>.h` | Small public API intentionally usable outside the app folder. | `src/apps/<app>/` | Internal scratch header |
| `<App>App.*` | Thin adapter implementing descriptor callbacks: open, draw, input, tick. | `src/apps/<app>/` | Main business logic |
| `<App>Internal.h` | Private declarations shared by split implementation files in the same app. | `src/apps/<app>/` | Public API |
| `<App>Model.cpp` | State transitions, calculations, selection math, and app behavior. | `src/apps/<app>/` | Drawing code |
| `<App>Input.cpp` | App-local keyboard handling after shell routing. | `src/apps/<app>/` | Global keyboard router |
| `<App>View.cpp` | Drawing and UI model construction. | `src/apps/<app>/` | App state mutation |
| `<App>Storage.cpp` | SD card or file persistence owned by the app. | `src/apps/<app>/` | Runtime settings persistence |
| `<App>Service.cpp` | Long-lived external work such as streaming, playback, scanning, or network jobs. | `src/apps/<app>/` or `src/module/service/` | UI view |
| `app.json` | App registration source for generated catalog code. | `src/apps/<app>/` | User settings |

## Service Terms

| Term | Meaning | Usually lives in | Not the same as |
| --- | --- | --- | --- |
| Service | Shared code that performs reusable work for apps or shell surfaces. | `src/module/service/` | Host app |
| File Browser | Shared singleton browser for folders, pages, recent items, and file selection. | `src/module/service/FileBrowser.*` | Music Player |
| Stream | A live data source consumed over time, usually audio over network. | Radio/audio code | Screen drawing |
| Playback | Local audio file playing, pause/resume, track choice, and seek behavior. | Music playback code | File browsing |
| Audio source | The current owner of the speaker pipeline: none, music, or radio. | `AudioSource` in `src/core/State.*` | Host app |
| Storage | Files persisted on SD card. | `/Music/settings.cfg`, `/Music/_radio/`, `/Notes/` | In-memory state |

## Current App Names

| Name | Meaning | Main files |
| --- | --- | --- |
| Music | Host app for browsing and playing local audio files from `/Music`. | `src/apps/music/` |
| Radio | Host app for streaming web radio stations. | `src/apps/radio/` |
| Notes | Host app and quick-access modal for daily/monthly notes. | `src/apps/notes/` |
| Calculator | Host app and quick-access modal for calculations. | `src/apps/calculator/` |

## Naming Rules

Use these rules when adding docs or files:

- Say `host app` when the feature owns the full screen and appears in Applications.
- Say `surface` when talking about what currently owns input or drawing priority.
- Say `modal` when the user must finish/cancel it before normal app input resumes.
- Say `metadata` only for app catalog, Help, Options, and registration data.
- Say `service` when code performs reusable work and is not itself a screen.
- Say `stream` only for live, ongoing data flow such as network audio.
- Say `playback` for local audio file control.
- Say `quick access` for descriptor-owned tools launched from another host app.
