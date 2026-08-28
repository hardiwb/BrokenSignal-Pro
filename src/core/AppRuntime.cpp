#include "core/AppRuntime.h"

#include "core/AppRegistry.h"

void appRuntimeOpen(HostApp app)
{
    rememberLastOpenedApp(app);
    appDescriptor(app).open();
}

void appRuntimeDrawForeground()
{
    appDescriptor(foregroundApp).draw();
}

bool appRuntimeHandleForegroundInput(Keyboard_Class::KeysState &keys)
{
    return appDescriptor(foregroundApp).handleInput(keys);
}

bool appRuntimeHandleQuickAccess(const Keyboard_Class::KeysState &keys)
{
    for (auto c : keys.word)
    {
        const char pressed = (char)tolower((unsigned char)c);
        for (size_t i = 0; i < appCount(); ++i)
        {
            const AppDescriptor &app = appDescriptorAt(i);
            if (app.quickAccess.key == 0 ||
                pressed != (char)tolower((unsigned char)app.quickAccess.key) ||
                app.quickAccess.open == nullptr)
            {
                continue;
            }

            if (app.quickAccess.available != nullptr &&
                !app.quickAccess.available(foregroundApp))
            {
                continue;
            }

            app.quickAccess.open();
            return true;
        }
    }

    return false;
}

bool appRuntimeQuickAccessActive()
{
    for (size_t i = 0; i < appCount(); ++i)
    {
        const QuickAccessDescriptor &quick = appDescriptorAt(i).quickAccess;
        if (quick.active != nullptr && quick.active())
            return true;
    }
    return false;
}

bool appRuntimeCloseQuickAccess()
{
    for (size_t i = 0; i < appCount(); ++i)
    {
        const QuickAccessDescriptor &quick = appDescriptorAt(i).quickAccess;
        if (quick.active != nullptr && quick.active() && quick.close != nullptr)
        {
            quick.close();
            return true;
        }
    }
    return false;
}

bool appRuntimeHandleQuickAccessInput(Keyboard_Class::KeysState &keys)
{
    for (size_t i = 0; i < appCount(); ++i)
    {
        const QuickAccessDescriptor &quick = appDescriptorAt(i).quickAccess;
        if (quick.active != nullptr && quick.active() && quick.handleInput != nullptr)
            return quick.handleInput(keys);
    }
    return false;
}

void appRuntimeTickForeground()
{
    appDescriptor(foregroundApp).tick();
}

HostApp appRuntimeBackgroundHost()
{
    return webRadioMode ? HostApp::Radio : HostApp::Music;
}
