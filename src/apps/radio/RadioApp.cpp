#include "apps/radio/RadioApp.h"

#include "apps/radio/Radio.h"

void openRadioApp()
{
    calculatorVisible = false;
    calculatorOverlayMode = false;
    notesMode = false;
    if (!webRadioMode)
        enterWebRadioMode();
    else
        drawRadioAll();
}

bool handleRadioAppInput(Keyboard_Class::KeysState &keys)
{
    handleRadioInput(keys);
    return true;
}

void tickRadioApp()
{
}
