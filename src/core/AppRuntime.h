#pragma once

#include "core/App.h"

void appRuntimeOpen(HostApp app);
void appRuntimeDrawForeground();
bool appRuntimeHandleForegroundInput(Keyboard_Class::KeysState &keys);
bool appRuntimeHandleQuickAccess(const Keyboard_Class::KeysState &keys);
bool appRuntimeQuickAccessActive();
bool appRuntimeCloseQuickAccess();
bool appRuntimeHandleQuickAccessInput(Keyboard_Class::KeysState &keys);
void appRuntimeTickForeground();
HostApp appRuntimeBackgroundHost();
