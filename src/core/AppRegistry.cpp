#include "core/AppRegistry.h"

#include "core/generated/AppIncludes.generated.inc"

namespace
{
const AppDescriptor APPS[] = {
#include "core/generated/AppCatalog.generated.inc"
};
} // namespace

size_t appCount()
{
    return sizeof(APPS) / sizeof(APPS[0]);
}

const AppDescriptor &appDescriptorAt(size_t index)
{
    return APPS[index < appCount() ? index : 0];
}

const AppDescriptor &appDescriptor(HostApp app)
{
    const int index = appIndex(app);
    return APPS[index >= 0 ? index : 0];
}

int appIndex(HostApp app)
{
    for (size_t i = 0; i < appCount(); ++i)
    {
        if (APPS[i].id == app)
            return (int)i;
    }
    return -1;
}
