#pragma once

#include "core/State.h"

enum class SurfaceKind
{
    None,
    HostApp,
    MainMenu,
    ContextMenu,
    OverlayModal,
    OverlayPopup,
    QuickPopup
};

struct ActiveSurface
{
    SurfaceKind kind;
    HostApp owner;
};

ActiveSurface resolveActiveSurface();
bool surfaceBlocksHostInput(const ActiveSurface &surface);
bool closeTopmostSurface(const ActiveSurface &surface);

