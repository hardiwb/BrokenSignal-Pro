#include "apps/notes/NotesApp.h"

#include "apps/notes/Notes.h"

void openNotesApp()
{
    notesOpen();
}

bool handleNotesAppInput(Keyboard_Class::KeysState &keys)
{
    handleNotesInput(keys);
    return true;
}

void tickNotesApp()
{
    notesLoop();
}

bool notesQuickAccessAvailable(HostApp foreground)
{
    return foreground != HostApp::Notes;
}
