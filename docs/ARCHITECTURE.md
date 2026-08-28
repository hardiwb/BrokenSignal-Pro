# BrokenSignal Pro Architecture

BrokenSignal Pro is organized as an embedded shell runtime for the Cardputer ADV.
It is not a general-purpose operating system, but it uses OS-like ideas:
foreground apps, shell menus, modal surfaces, shared services, and a central
input router.

If an architecture word is unclear, check `docs/VOCABULARY.md` first. It defines
the project meaning of terms such as app, surface, modal, service, stream, and
metadata.

## Shell Runtime Architecture

BrokenSignal Pro is best described as a small embedded shell runtime. The core
runtime sits in the middle: it knows which app is foreground, which surface owns
input, how apps are registered, and how shared system behavior is redrawn.

```text
+---------------------------+     +---------------------------+
| Shell Surfaces            |     | Host Apps                 |
| Applications              |     | Music                     |
| Options                   |     | Radio                     |
| Control Panel             |     | Notes                     |
| Help / Debug              |     | Calculator                |
+-------------+-------------+     +-------------+-------------+
              |                                 |
              +---------------+-----------------+
                              |
              +---------------v-----------------+
              | Core Runtime                     |
              | State, System                    |
              | AppRegistry, AppRuntime          |
              | SurfaceManager, Keyboard         |
              +---------------+-----------------+
                              |
              +---------------v-----------------+
+-------------+-------------+     +-------------+-------------+
| UI Primitives             |     | Shared Services           |
| Header / List / Footer    |     | File Browser              |
| Overlay / Toast / Themes  |     | WiFi / Clock / Audio      |
+-------------+-------------+     +-------------+-------------+
              |                                 |
              +---------------+-----------------+
                              |
              +---------------v-----------------+
              | Platform + Hardware + Storage    |
              | M5 / Arduino / PlatformIO        |
              | Keyboard, display, speaker       |
              | SD, WiFi, RTC, settings/files    |
              +---------------------------------+
```

Read this diagram from the center outward:

- `Core Runtime` is the coordinator, not a feature app.
- `Shell Surfaces` are menus and overlays that sit above apps.
- `Host Apps` own the main full-screen experience.
- `UI Primitives` draw reusable screen parts but do not decide app behavior.
- `Shared Services` perform reusable work but are not themselves host apps.
- `Platform + Hardware + Storage` is the lower boundary the firmware runs on.

## Layer Map

```text
Cardputer hardware
  Keyboard, display, speaker, SD, WiFi, RTC
        |
M5 / Arduino / PlatformIO libraries
        |
Core runtime
  State, System, AppRegistry, AppRuntime, SurfaceManager, Keyboard
        |
Shell surfaces                 Host apps
  Applications                 Music Player
  Options                      Web Radio
  Control Panel                Notes
  Help                         Calculator
  Debug
        |
Shared UI primitives and services
  Header, List, Footer, Overlay, Toast, Themes
  File Browser, WiFi, Clock, audio helpers
        |
Storage
  /Music/settings.cfg
  /Music/_radio/
  /Notes/YYYY-MM.txt
```

## Runtime Responsibilities

| Area | Responsibility |
| --- | --- |
| `src/core/AppRegistry.*` | Owns the generated app catalog and descriptor lookup. |
| `src/core/AppRuntime.*` | Opens apps, routes foreground app input, handles descriptor-driven quick access. |
| `src/core/SurfaceManager.*` | Decides which UI surface is currently on top and how `Esc` closes it. |
| `src/core/Keyboard.*` | Reads physical keys, handles shell navigation priority, then dispatches to the active surface. |
| `src/core/System.*` | Shared drawing, settings persistence, power/screen behavior, and boot flow. |
| `src/module/shell/` | Shell-owned surfaces: Applications, Options, Control Panel, Help, Debug. |
| `src/module/service/` | Shared services such as WiFi, Clock, and File Browser. |
| `src/apps/` | App-owned lifecycle, input, metadata, views, and storage. |
| `src/UI/` | Reusable drawing primitives and themes. |

Footer convention: put key hints in the left slot whenever possible. The center
slot is reserved for status text or progress, which avoids truncating hints
between narrow footer regions.

## Input Flow

```text
M5Cardputer.Keyboard.keysState()
        |
Keyboard.cpp
        |
resolveActiveSurface()
        |
Back/Esc closes the topmost surface first
        |
Shell modifiers route between shell menus
  Alt  -> Applications
  Opt  -> active app Options
  Ctrl -> Control Panel
        |
Host-like surfaces check global quick access
  C -> quick calculator
  N -> quick note
        |
Host-like surfaces check hardware-safe global keys
  [ -> brightness down, ] -> brightness up
  -/+ -> volume on playback-safe host/list surfaces
        |
Collision-safe host utility hotkeys may run
  H -> context help, O -> screen
        |
Active surface handles input
  Host app, shell menu, modal editor, help/debug popup, or service menu
```

Input ownership rules:

| Shortcut type | Owner |
| --- | --- |
| Shell navigation (`Alt`, `Opt`, `Ctrl`) | `Keyboard.cpp`, using `SurfaceManager` state. |
| Back/Esc close behavior | `SurfaceManager.cpp`. |
| Global quick access (`C`, `N`) | App descriptors through `AppRuntime.cpp`, only on host-like surfaces. |
| Hardware hotkeys (`[`, `]`, safe `-`, `+`) | `Keyboard.cpp` calls shared system adjustment helpers. |
| Global utility keys (`H`, `O`) | `Keyboard.cpp`, only where they do not collide with app-local keys. |
| App-specific keys | The app's own input module. |
| Modal editor keys | The modal owner, routed by active surface. |

Shell navigation works from host apps and shell menus, so users can browse
Applications, Options, and Control Panel without first closing the current menu.
Modal editors and confirmation dialogs keep ownership of input until closed.
They reject global launch and utility hotkeys so normal text entry remains safe.
`QuickPopup` is the exception by design: it is visual-only feedback, such as a
header message, and does not own text input.

Header transient messages are drawn by the `Header` primitive. Normal headers
keep the title on the left and transient feedback on the right; value/result
headers can right-align the title and the transient feedback moves to the left.

## App Registration

Each app owns an `app.json` manifest under `src/apps/<app>/`. The PlatformIO
pre-build script reads those manifests and generates:

```text
src/core/generated/AppIds.generated.h
src/core/generated/AppIncludes.generated.inc
src/core/generated/AppCatalog.generated.inc
```

This keeps Applications, Options, Help, and quick access driven by app metadata
instead of hard-coded per-app lists in the keyboard layer.

## Surface Model

| Surface | Meaning |
| --- | --- |
| `HostApp` | The active full-screen app. |
| `MainMenu` | Applications launcher. |
| `ContextMenu` | Options, Control Panel, or WiFi menu. |
| `OverlayModal` | Text input, password entry, confirmation, quick note, calculator edit. |
| `OverlayPopup` | Help or Debug. |
| `QuickPopup` | Toast or temporary header message. |

The surface model is the main guardrail against shortcut collisions. New UI
states should either fit one of these categories or add a deliberate new surface
category.

## App Boundaries

App code should own its own local shortcuts and state. Shared shell behavior
should go through the runtime instead of direct cross-app checks.

Preferred patterns:

| Need | Preferred path |
| --- | --- |
| Open a full app | `appRuntimeOpen(HostApp::...)`. |
| Add a global quick tool | App `quick_access` descriptor. |
| Add app-local command | The app's `handleInput()` path. |
| Add contextual settings/actions | The app's `buildOptions()` callback. |
| Add a modal editor | App state plus `SurfaceManager` awareness if it must close via `Esc`. |

Some low-level coupling remains where Music and Radio share audio/file-browser
ownership. That coupling is intentional for now because only one audio source can
own playback at a time.

## App File Convention

App folders use role-based filenames. The suffix should describe the file's
responsibility, not its importance.

| File | Meaning |
| --- | --- |
| `<App>App.*` | Runtime adapter used by the generated app catalog. |
| `<App>.h` | Small public API for core, shell, or other apps. |
| `<App>Internal.h` | Private shared declarations for files in the same app. |
| `<App>Model.cpp` | State transitions, calculations, parsing, and commands. |
| `<App>Input.cpp` | App-local keyboard handling. |
| `<App>View.cpp` | UI model creation and drawing. |
| `<App>Storage.cpp` | SD card or file persistence. |
| `<App>Service.cpp` | Streaming, playback, scans, network work, or other external runtime work. |
| `<App>Metadata.*` | Help rows and Options rows. |

A small app can combine model/input/view in one file while it is still simple.
Once an app has modal state, storage, or multiple views, split it by these roles.
