#pragma once

#include "core/App.h"

size_t appCount();
const AppDescriptor &appDescriptorAt(size_t index);
const AppDescriptor &appDescriptor(HostApp app);
int appIndex(HostApp app);
