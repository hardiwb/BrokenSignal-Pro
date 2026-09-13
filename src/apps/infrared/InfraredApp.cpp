#include "apps/infrared/InfraredApp.h"
#include "apps/infrared/Infrared.h"

void openInfraredApp() { infraredOpen(); }
void drawInfraredApp() { drawInfrared(); }
bool handleInfraredAppInput(Keyboard_Class::KeysState &keys) { handleInfraredInput(keys); return true; }
void tickInfraredApp() { tickInfrared(); }
