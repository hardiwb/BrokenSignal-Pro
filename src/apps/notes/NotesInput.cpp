#include "apps/notes/NotesInternal.h"

#include "core/Config.h"
#include "core/Keyboard.h"
#include "core/State.h"
#include "apps/notes/Notes.h"
#include "module/shell/Help.h"
#include "UI/Overlay.h"

using namespace NotesInternal;

void handleNotesInput(Keyboard_Class::KeysState &ks)
{
    if (!notesInputActive())
        return;

    if (noteMoveDateVisible)
    {
        if (keyboardBackPressed(ks))
        {
            noteMoveDateVisible = false;
            noteMoveDateInvalid = false;
            drawNotes();
            return;
        }
        if (ks.enter)
        {
            if (moveSelectedNoteToDate(noteMoveDateInput))
            {
                noteMoveDateVisible = false;
                noteMoveDateInvalid = false;
            }
            else
            {
                noteMoveDateInvalid = true;
                drawNotesMoveDateEditor();
            }
            return;
        }
        if (ks.del)
        {
            if (noteMoveDateInput.length() > 0)
                noteMoveDateInput.remove(noteMoveDateInput.length() - 1);
            noteMoveDateInvalid = false;
            drawOverlayInputValue(noteMoveDateInput);
            return;
        }

        bool changed = false;
        for (auto c : ks.word)
        {
            if (((c >= '0' && c <= '9') || c == '-') && noteMoveDateInput.length() < 10)
            {
                noteMoveDateInput += c;
                changed = true;
            }
        }
        if (changed)
        {
            noteMoveDateInvalid = false;
            drawOverlayInputValue(noteMoveDateInput);
        }
        return;
    }

    if (noteEditorVisible)
    {
        if (ks.fn)
        {
            for (auto c : ks.word)
            {
                if (c == ';' || c == '.')
                {
                    shiftQuickNoteDate(c == ';' ? 1 : -1);
                    return;
                }
                if (c == ',' || c == '/')
                {
                    const int direction = c == ',' ? -1 : 1;
                    int &cursor = noteEditorField == 0 ? noteEditTextCursor : noteEditDateCursor;
                    const int length = noteEditorField == 0 ? noteEditText.length() : noteEditDate.length();
                    cursor = constrain(cursor + direction, 0, length);
                    drawNotesEditor(true);
                    return;
                }
            }
        }

        if (ks.tab)
        {
            noteEditorField = (noteEditorField + 1) % 2;
            drawNotesEditor(true);
            return;
        }
        if (keyboardBackPressed(ks))
        {
            cancelNoteEditorImpl();
            return;
        }
        if (!notesMode && ks.fn)
        {
            for (auto c : ks.word)
            {
                if (c == 'n' || c == 'N')
                {
                    noteEditorVisible = false;
                    notesOpen();
                    return;
                }
            }
        }
        if (ks.enter)
        {
            saveNoteEditor();
            return;
        }
        if (ks.del)
        {
            String &value = noteEditorField == 0 ? noteEditText : noteEditDate;
            int &cursor = noteEditorField == 0 ? noteEditTextCursor : noteEditDateCursor;
            if (cursor > 0 && value.length() > 0)
            {
                value.remove(cursor - 1, 1);
                --cursor;
                noteEditorDateInvalid = false;
                drawNotesEditor(true);
            }
            return;
        }

        for (auto c : ks.word)
        {
            if (!keyboardTextInputChar(ks, c))
                continue;
            if (noteEditorField == 0 && noteEditText.length() < RADIO_INPUT_MAX)
            {
                noteEditText = noteEditText.substring(0, noteEditTextCursor) +
                               String(c) + noteEditText.substring(noteEditTextCursor);
                ++noteEditTextCursor;
                drawNotesEditor(true);
            }
            else if (noteEditorField == 1 && ((c >= '0' && c <= '9') || c == '-') && noteEditDate.length() < 10)
            {
                noteEditDate = noteEditDate.substring(0, noteEditDateCursor) +
                               String(c) + noteEditDate.substring(noteEditDateCursor);
                ++noteEditDateCursor;
                noteEditorDateInvalid = false;
                drawNotesEditor(true);
            }
        }
        return;
    }

    if (keyboardBackPressed(ks))
    {
        notesClose();
        return;
    }
    if (ks.enter && visibleNoteCount() > 0)
    {
        beginNoteEditor(notesSelected);
        return;
    }

    for (auto c : ks.word)
    {
        switch (c)
        {
        case 'h': case 'H': toggleHelp(); return;
        case 'a': case 'A': beginNoteEditor(-1); return;
        case 't': case 'T': jumpToToday(); return;
        case 'u': case 'U': selectTopNote(); return;
        case 'b': case 'B': selectBottomNote(); return;
        case 'r': case 'R': removeSelectedNote(); return;
        case 's': notesSendViewedDayToXteink(false); return;
        case 'S': notesSendViewedDayToXteink(true); return;
        case 'x': case 'X': toggleSelectedNoteDone(); return;
        case ';':
        case '.':
            if (visibleNoteCount() > 0)
            {
                const int oldSelected = notesSelected;
                const int oldScrollTop = notesScrollTop;
                const int direction = c == ';' ? -1 : 1;
                notesSelected = (notesSelected + direction + visibleNoteCount()) % visibleNoteCount();
                notesMarqueeStartMs = millis();
                redrawNotesSelection(oldSelected, oldScrollTop);
            }
            return;
        case ',': shiftNotesView(-1); return;
        case '/': shiftNotesView(1); return;
        }
    }
}
