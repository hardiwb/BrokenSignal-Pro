# BrokenSignal Pro App Development

This guide describes the host-app API used by Music, Radio, Notes, and
Calculator. BrokenSignal Pro is organized as an embedded shell runtime: a host
app owns the full content area and shell surfaces sit above it.

Menus, dialogs, Help, Debug, toasts, and quick tools are surfaces, not host apps.
See `docs/ARCHITECTURE.md` for the full layer map and input-routing model. See
`docs/VOCABULARY.md` for the project dictionary.

Start from the files in `docs/app-template/`. They use the same callback shapes
as production apps but are not compiled because their names end in `.example`.

## App Contract

Every host app has one generated `AppDescriptor`. Its folder-local `app.json`
declares the descriptor fields and callback symbols:

```cpp
struct AppDescriptor
{
    HostApp id;
    const char *name;
    const char *headerTag;
    void (*open)();
    void (*draw)();
    bool (*handleInput)(Keyboard_Class::KeysState &keys);
    void (*tick)();
    const char *helpTitle;
    const HelpEntry *helpEntries;
    uint8_t helpCount;
    BuildAppOptions buildOptions;
    bool optionsEnterEnabled;
    bool optionsShowRunHint;
    QuickAccessDescriptor quickAccess;
};
```

Callback guarantees:

| Callback | Responsibility |
| --- | --- |
| `open` | Initialize or reset transient state, then draw the app. |
| `draw` | Redraw the complete host screen using UI primitives. |
| `handleInput` | Handle app-local keys and return `true` when consumed. |
| `tick` | Perform short, non-blocking foreground work. Use a no-op if unused. |
| `buildOptions` | Rebuild current option rows whenever Options redraws. |

The runtime sets and persists `foregroundApp` before calling `open`. App code
should normally be opened through `appRuntimeOpen()`, not by calling its `open`
callback directly.

## Required Files

Recommended layout for an app named Example:

```text
src/apps/example/
|-- app.json
|-- Example.h
|-- ExampleApp.cpp
|-- ExampleApp.h
|-- ExampleModel.cpp
|-- ExampleInput.cpp
|-- ExampleView.cpp
|-- ExampleMetadata.h
`-- ExampleMetadata.cpp
```

Small apps may combine model, input, and view into one implementation file, but
larger apps should use the same names consistently:

```text
src/apps/example/
|-- app.json
|-- Example.h            Public app API used by core, shell, or other apps.
|-- ExampleApp.cpp       Thin AppDescriptor adapter: open/draw/input/tick.
|-- ExampleApp.h
|-- ExampleInternal.h    Private shared state/types/prototypes for split files.
|-- ExampleModel.cpp     State transitions and app behavior.
|-- ExampleInput.cpp
|-- ExampleView.cpp
|-- ExampleStorage.cpp
|-- ExampleService.cpp
|-- ExampleMetadata.h
`-- ExampleMetadata.cpp
```

File naming convention:

| File | Role |
| --- | --- |
| `<App>.h` | Public API only. Keep this small and intentional. |
| `<App>App.*` | Runtime adapter named by `app.json`. It should mostly call public app functions. |
| `<App>Internal.h` | Private declarations shared only inside the app folder. |
| `<App>Model.cpp` | State changes, selection math, parsing, calculations, and commands. |
| `<App>Input.cpp` | App-local key handling after shell routing. |
| `<App>View.cpp` | Drawing and UI model construction. |
| `<App>Storage.cpp` | SD card or file persistence. |
| `<App>Service.cpp` | Long-lived external work such as streaming, playback, scans, or network jobs. |
| `<App>Metadata.*` | Help entries and Options rows. |
| `app.json` | App registration, callbacks, Help, Options, and quick-access metadata. |

Only public lifecycle callbacks belong in `Example.h`. Keep internal state and
helpers private to the app rather than adding them to `core/State.h` unless the
shell or another app truly needs them.

## Registration Checklist

1. Copy `docs/app-template/` to `src/apps/<name>/` and remove `.example` from
   the filenames.
2. Choose an unused `persistent_id` from 0 to 255. Never reuse an ID released
   in a published firmware because it is stored in `settings.cfg`.
3. Implement the symbols named by `app.json`.
4. Build with `platformio run -e cardputeradv`.

The PlatformIO pre-build script scans `src/apps/*/app.json`, validates the
catalog, and generates `HostApp`, includes, and descriptor rows under
`src/core/generated/`. Applications reads that catalog directly. No core,
shell, keyboard, or other app files need to be edited.

Generation rejects invalid JSON, duplicate app IDs, duplicate persisted IDs,
duplicate quick-access keys, malformed callback symbols, and missing required
manifest fields before C++ compilation starts.

## Drawing

Host apps should compose the shared UI API:

- `UI/Header.h` for app identity and status.
- `UI/List.h` for selectable rows and properties.
- `UI/Footer.h` for concise key hints and battery status.
- `UI/Overlay.h` for modal input and confirmation surfaces.
- `UI/Toast.h` for short transient messages.

`draw()` must be safe after a menu or popup closes. Prefer granular redraw APIs
for frequent changes, but always retain one complete redraw path.

## Input Ownership

The keyboard router first asks `SurfaceManager` which surface is on top. It then
handles shell navigation before handing input to the active surface.

| Input | Owner |
| --- | --- |
| `Esc` | Surface manager: close topmost surface or open Applications. |
| `Alt` | Applications menu navigation. |
| `Opt` | Active app's Options navigation. |
| `Ctrl` | Control Panel navigation. |
| `C` | Quick Calculator where supported. |
| `N` | Quick Note where supported. |

`Alt`, `Opt`, and `Ctrl` work from host apps and shell menus, so users can move
between Applications, Options, and Control Panel without closing the current menu
first. Modal editors and confirmation dialogs keep input until closed.

Do not reimplement shell menu or ESC behavior in a new app. App input should
only handle local commands left after shell routing. Modal editors must expose
their state to `SurfaceManager` if ESC needs to close them before the host app.

## Quick Access

A host app can optionally publish one quick-overlay shortcut through its
descriptor:

| Field | Meaning |
| --- | --- |
| `key` | Case-insensitive key, or `0` when unused. |
| `open` | Opens and draws the quick overlay. |
| `available` | Returns whether it may open from the foreground app. |
| `active` | Reports that this app's quick modal currently owns input. |
| `close` | Cancels/closes the modal and restores the underlying screen. |
| `input` | Handles input while the quick modal is active. |

Quick access, modal input, and ESC closure are routed across the complete app
registry. Do not add host-app checks to `Keyboard.cpp`, `SurfaceManager.cpp`, or
to every app. To make a shortcut available from every other host app:

```cpp
bool exampleQuickAccessAvailable(HostApp foreground)
{
    return foreground != HostApp::Example;
}
```

The shell evaluates quick-access keys only on host-like surfaces where they will
not steal text input from a modal editor. Menus, Help, Debug, editors,
confirmations, and existing quick overlays therefore retain their input. A
quick-access key takes precedence over an app-local key on eligible host screens,
so keys must be unique and intentionally reserved.

## Help Metadata

Help rows are static and app-owned:

```cpp
const HelpEntry EXAMPLE_HELP_ENTRIES[] = {
    {"[Ok]", "Run selected item"},
    {"[;/.]", "Cursor up / down"},
    {"[H]", "Close help"},
};
```

Keep key labels short enough for the right side of a Cardputer list row. Help
should describe actual behavior rather than duplicate generic implementation.

## Options Metadata

Options are rebuilt on every draw, so labels can reflect current state. Each row
contains:

| Field | Meaning |
| --- | --- |
| `label` | Left-side row text. |
| `value` | Current value; empty for an action row. |
| `enabled` | Disabled rows are dimmed and cannot activate. |
| `adjustable` | Allows `+` and `-` to call the action. |
| `closeOnActivate` | Closes Options before opening another surface. |
| `activate` | Receives `+1` or `-1`. |

Set descriptor `optionsEnterEnabled` when `Ok` should activate rows. Set
`optionsShowRunHint` when the footer should advertise `[Ok]Run`.

Option callbacks that update persisted settings should set `settingsDirty` and
`settingsDirtyMs`. Do not write the settings file for every key press.

## Background Work

`tick()` runs only for the foreground app and must not block. Shared services and
background audio belong outside app ticks. Music and Radio are mutually exclusive
and currently coordinated by the audio runtime; a new audio app must integrate
with that ownership before starting playback.

## Persistence and Boot

`appRuntimeOpen()` calls `rememberLastOpenedApp()`. Cold boot intentionally opens
Notes, while deep-sleep wake restores the persisted host app. The manifest's
`persistent_id` is its serialized ID. Never change or reuse a released ID.

## Surfaces

Use the existing surface categories consistently:

| Surface | Use |
| --- | --- |
| `HostApp` | Full-screen application. |
| `MainMenu` | Applications root. |
| `ContextMenu` | Options, Control Panel, WiFi. |
| `OverlayModal` | Input, password, and confirmation dialogs. |
| `OverlayPopup` | Help and Debug. |
| `QuickPopup` | Toasts and temporary header messages. |

Quick-access modals use their manifest `active`, `close`, and `input` callbacks.
They do not require SurfaceManager changes. A new system-level surface category
still requires an explicit core change because it is not an app-owned surface.
