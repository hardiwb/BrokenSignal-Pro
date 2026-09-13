#include "apps/infrared/Infrared.h"
#include "apps/infrared/InfraredInternal.h"

#include "core/System.h"
#include "core/Keyboard.h"
#include "module/shell/Help.h"
#include <SD.h>
#include "UI/List.h"
#include "UI/Overlay.h"

using namespace InfraredInternal;

namespace
{
bool validLeafName(const String &value)
{
    if (!value.length() || value == "." || value == "..") return false;
    const char forbidden[] = {'/', '\\', ':', '*', '?', '"', '<', '>', '|'};
    for (size_t i = 0; i < value.length(); ++i)
    {
        const char c = value.charAt(i);
        if (c < 32) return false;
        for (char blocked : forbidden) if (c == blocked) return false;
    }
    return true;
}

String parentDirectory(const String &path)
{
    const int slash = path.lastIndexOf('/');
    return slash <= 0 ? "/Infrared" : path.substring(0, slash);
}

void finishNameModal()
{
    String leaf = nameInput;
    leaf.trim();
    if (nameModal == NameModal::RenameFile || nameModal == NameModal::NewFile)
    {
        String lower = leaf; lower.toLowerCase();
        if (lower.endsWith(".ir")) leaf = leaf.substring(0, leaf.length() - 3);
        leaf.trim();
    }
    if (!validLeafName(leaf)) { nameModalError = "INVALID NAME"; drawInfrared(); return; }

    if (nameModal == NameModal::CaptureCommand)
    {
        String error;
        if (!appendRemoteSignal(openPath, leaf, capturedSignal, error))
        { nameModalError = error; drawInfrared(); return; }
        capturedSignal = {}; nameModal = NameModal::None;
        nameInput = nameModalError = renamePath = "";
        if (!loadRemoteCommands(openPath, error)) { drawInfrared(); showHdrMsg(error.c_str()); return; }
        view = View::Commands; selected = max(0, static_cast<int>(commands.size()) - 1);
        scrollTop = max(0, selected - LIST_VISIBLE_ITEM + 1); marqueeStartMs = millis(); drawInfrared();
        return;
    }

    String focusPath;
    if (nameModal == NameModal::NewFolder)
    {
        const String target = currentDirectory + "/" + leaf;
        if (SD.exists(target.c_str())) { nameModalError = "ALREADY EXISTS"; drawInfrared(); return; }
        if (!SD.mkdir(target.c_str())) { nameModalError = "CREATE FAILED"; drawInfrared(); return; }
    }
    else if (nameModal == NameModal::NewFile)
    {
        const String target = currentDirectory + "/" + leaf + ".ir";
        if (SD.exists(target.c_str())) { nameModalError = "ALREADY EXISTS"; drawInfrared(); return; }
        File file = SD.open(target.c_str(), FILE_WRITE);
        const char header[] = "Filetype: IR signals file\nVersion: 1\n";
        if (!file || file.print(header) != sizeof(header) - 1)
        {
            if (file) file.close();
            SD.remove(target.c_str());
            nameModalError = "CREATE FAILED"; drawInfrared(); return;
        }
        file.close(); focusPath = target;
    }
    else
    {
        const String target = parentDirectory(renamePath) + "/" + leaf + ".ir";
        if (target != renamePath && SD.exists(target.c_str()))
        { nameModalError = "ALREADY EXISTS"; drawInfrared(); return; }
        if (target != renamePath && !SD.rename(renamePath.c_str(), target.c_str()))
        { nameModalError = "RENAME FAILED"; drawInfrared(); return; }
    }
    nameModal = NameModal::None; nameInput = nameModalError = renamePath = "";
    openPath = openName = ""; infraredReload();
    if (focusPath.length())
    {
        for (int i = 0; i < static_cast<int>(files.size()); ++i)
            if (files[i].path == focusPath) { selected = i; clampSelection(); drawInfrared(); break; }
    }
}
}

void handleInfraredInput(Keyboard_Class::KeysState &keys)
{
    if (captureActive) return;
    if (nameModal != NameModal::None)
    {
        if (keys.enter) { finishNameModal(); return; }
        if (keys.del && nameInput.length())
        {
            nameInput.remove(nameInput.length() - 1);
            if (nameModalError.length()) { nameModalError = ""; drawInfrared(); }
            else drawOverlayInputValue(nameInput);
            return;
        }
        for (char c : keys.word)
        {
            if (keyboardTextInputChar(keys, c) && nameInput.length() < 48)
            {
                nameInput += c;
                if (nameModalError.length()) { nameModalError = ""; drawInfrared(); }
                else drawOverlayInputValue(nameInput);
            }
        }
        return;
    }
    if (deleteConfirmVisible)
    {
        if (keys.enter)
        {
            const String path = deletePath;
            String lowerPath = path;
            lowerPath.toLowerCase();
            deleteConfirmVisible = false;
            deletePath = deleteName = "";
            if (!path.startsWith("/Infrared/") || !lowerPath.endsWith(".ir") || !SD.remove(path.c_str()))
            {
                drawInfrared();
                showHdrMsg("Delete failed");
                return;
            }
            infraredReload();
        }
        return;
    }
    if (keys.del)
    {
        if (view == View::Commands) returnToFiles(); else goToParentDirectory();
        return;
    }
    if (keys.enter) { activateSelection(); return; }
    for (char c : keys.word)
    {
        const int count = view == View::Files ? files.size() : commands.size();
        const int shortcut = listVisibleShortcutTarget(c, scrollTop, count);
        if (shortcut >= 0) { selected = shortcut; marqueeStartMs = millis(); activateSelection(); return; }
        if (c == 'r' || c == 'R') { infraredReload(); return; }
        if ((c == 'c' || c == 'C') && infraredCanCapture()) { infraredRequestCapture(); return; }
        if (c == 'h' || c == 'H') { toggleHelp(); return; }
        if (c == ';') { moveSelection(-1); return; }
        if (c == '.') { moveSelection(+1); return; }
    }
}
