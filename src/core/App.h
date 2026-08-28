#pragma once

#include <M5Cardputer.h>

#include "core/State.h"

struct HelpEntry
{
    const char *key;
    const char *description;
};

// App-owned option action. direction is +1 for Enter/+ and -1 for -.
struct AppOption
{
    const char *label;
    String value;
    bool enabled;
    bool adjustable;
    bool closeOnActivate;
    void (*activate)(int direction);
};

using BuildAppOptions = void (*)(std::vector<AppOption> &options);
using QuickAccessAvailable = bool (*)(HostApp foreground);

struct QuickAccessDescriptor
{
    char key;
    void (*open)();
    QuickAccessAvailable available;
    bool (*active)();
    void (*close)();
    bool (*handleInput)(Keyboard_Class::KeysState &keys);
};

struct AppDescriptor
{
    HostApp id;
    const char *name;
    const char *headerTag;
    void (*open)(); // Initialize/open and draw the app.
    void (*draw)(); // Redraw the complete host-app screen.
    bool (*handleInput)(Keyboard_Class::KeysState &keys); // true when consumed.
    void (*tick)(); // Short, non-blocking foreground work.
    const char *helpTitle;
    const HelpEntry *helpEntries;
    uint8_t helpCount;
    BuildAppOptions buildOptions;
    bool optionsEnterEnabled;
    bool optionsShowRunHint;
    QuickAccessDescriptor quickAccess;
};
