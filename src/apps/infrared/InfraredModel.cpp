#include "apps/infrared/Infrared.h"
#include "apps/infrared/InfraredInternal.h"

#include "apps/music/MusicPlayer.h"
#include "apps/radio/Radio.h"
#include "core/System.h"
#include "UI/List.h"

namespace InfraredInternal
{
View view = View::Files;
InfraredSettings settings;
std::vector<RemoteFile> files;
std::vector<CommandSummary> commands;
int selected = 0;
int scrollTop = 0;
uint32_t marqueeStartMs = 0;
String openPath;
String openName;
String currentDirectory = "/Infrared";
bool deleteConfirmVisible = false;
String deletePath;
String deleteName;
bool deleteCommand = false;
int deleteCommandIndex = -1;
NameModal nameModal = NameModal::None;
String nameInput;
String nameModalError;
String renamePath;
bool captureActive = false;
InfraredSignal capturedSignal;

int itemCount()
{
    return view == View::Files ? static_cast<int>(files.size()) : static_cast<int>(commands.size());
}

void clampSelection()
{
    const int count = itemCount();
    if (count <= 0) { selected = 0; scrollTop = 0; return; }
    selected = constrain(selected, 0, count - 1);
    if (selected < scrollTop) scrollTop = selected;
    if (selected >= scrollTop + LIST_VISIBLE_ITEM) scrollTop = selected - LIST_VISIBLE_ITEM + 1;
    scrollTop = constrain(scrollTop, 0, max(0, count - LIST_VISIBLE_ITEM));
}

void moveSelection(int direction)
{
    const int count = itemCount();
    if (count <= 0) return;
    selected = (selected + direction + count) % count;
    marqueeStartMs = millis();
    clampSelection();
    drawInfrared();
}

void returnToFiles()
{
    view = View::Files; commands.clear(); openPath = ""; openName = "";
    selected = 0; scrollTop = 0; marqueeStartMs = millis(); drawInfrared();
}

void goToParentDirectory()
{
    if (view != View::Files || currentDirectory == "/Infrared") return;
    const int slash = currentDirectory.lastIndexOf('/');
    currentDirectory = slash <= 0 ? "/Infrared" : currentDirectory.substring(0, slash);
    if (!currentDirectory.startsWith("/Infrared")) currentDirectory = "/Infrared";
    String error;
    if (!loadRemoteFiles(currentDirectory, error)) showHdrMsg(error.c_str());
    selected = scrollTop = 0; marqueeStartMs = millis(); drawInfrared();
}

void activateSelection()
{
    if (view == View::Files)
    {
        if (files.empty()) return;
        if (files[selected].directory)
        {
            currentDirectory = files[selected].path;
            String error;
            if (!loadRemoteFiles(currentDirectory, error)) showHdrMsg(error.c_str());
            selected = scrollTop = 0; marqueeStartMs = millis(); drawInfrared();
            return;
        }
        String error;
        if (!loadRemoteCommands(files[selected].path, error))
        {
            showHdrMsg(error.c_str());
            return;
        }
        openPath = files[selected].path;
        openName = files[selected].name;
        const int dot = openName.lastIndexOf('.'); if (dot > 0) openName = openName.substring(0, dot);
        openName.replace('_', ' ');
        view = View::Commands; selected = 0; scrollTop = 0; marqueeStartMs = millis(); drawInfrared();
        return;
    }

    if (commands.empty()) return;
    if (!commands[selected].supported) { showHdrMsg("Unsupported protocol"); return; }
    InfraredSignal signal; String commandName; String error;
    if (!loadRemoteSignal(openPath, selected, signal, commandName, error))
    {
        showHdrMsg(error.c_str());
        return;
    }
    stopAudio(); stopRadioStream();
    if (!sendInfraredSignal(settings, signal, error))
    {
        showHdrMsg(error.c_str());
        return;
    }
    Serial.print("IR sent: "); Serial.print(openName); Serial.print(" / "); Serial.println(commandName);
    showHdrMsg("Sent");
}

void saveHardwareSettings()
{
    String error;
    if (!validateInfraredSettings(settings, error)) { showHdrMsg(error.c_str()); return; }
    if (!saveInfraredSettings(settings)) showHdrMsg("Save failed");
}
} // namespace InfraredInternal

using namespace InfraredInternal;

void infraredOpen()
{
    stopInfraredCapture();
    loadInfraredSettings(settings); view = View::Files; selected = scrollTop = 0;
    currentDirectory = "/Infrared"; openPath = openName = "";
    deleteConfirmVisible = false; deletePath = deleteName = ""; deleteCommand = false; deleteCommandIndex = -1;
    nameModal = NameModal::None; nameInput = nameModalError = renamePath = "";
    captureActive = false; capturedSignal = {}; infraredReload();
}

void infraredReload()
{
    String error;
    if (!loadRemoteFiles(currentDirectory, error)) showHdrMsg(error.c_str());
    view = View::Files; selected = scrollTop = 0; marqueeStartMs = millis(); drawInfrared();
}

void tickInfrared()
{
    if (!captureActive) return;
    bool received = false; String error;
    if (!pollInfraredCapture(capturedSignal, received, error))
    {
        captureActive = false; drawInfrared(); showHdrMsg(error.c_str()); return;
    }
    if (!received) return;
    captureActive = false;
    int suffix = static_cast<int>(commands.size()) + 1;
    do
    {
        nameInput = "Command " + String(suffix++);
        bool duplicate = false;
        for (const auto &command : commands) if (command.name == nameInput) { duplicate = true; break; }
        if (!duplicate) break;
    } while (suffix < 1000);
    nameModalError = ""; nameModal = NameModal::CaptureCommand; drawInfrared();
}

bool infraredHasSelectedFile()
{
    return view == View::Files && !files.empty() && selected >= 0 &&
        selected < static_cast<int>(files.size()) && !files[selected].directory;
}

bool infraredInCommandView() { return view == View::Commands; }

bool infraredHasSelectedCommand()
{
    return view == View::Commands && selected >= 0 && selected < static_cast<int>(commands.size());
}

void infraredRequestNewFolder()
{
    nameModal = NameModal::NewFolder; nameInput = nameModalError = renamePath = ""; drawInfrared();
}

void infraredRequestNewFile()
{
    nameModal = NameModal::NewFile; nameInput = nameModalError = renamePath = ""; drawInfrared();
}

void infraredRequestRename()
{
    if (infraredHasSelectedCommand())
    {
        renamePath = openPath;
        nameInput = commands[selected].name;
        nameModalError = ""; nameModal = NameModal::RenameCommand; drawInfrared();
        return;
    }
    if (!infraredHasSelectedFile()) return;
    renamePath = files[selected].path;
    nameInput = renamePath.substring(renamePath.lastIndexOf('/') + 1);
    const int dot = nameInput.lastIndexOf('.');
    if (dot > 0) nameInput = nameInput.substring(0, dot);
    nameModalError = ""; nameModal = NameModal::RenameFile; drawInfrared();
}

bool infraredCanCapture() { return view == View::Commands && openPath.length() > 0; }

void infraredRequestCapture()
{
    if (!infraredCanCapture()) return;
    String error;
    stopAudio(); stopRadioStream();
    if (!startInfraredCapture(settings, error)) { showHdrMsg(error.c_str()); return; }
    captureActive = true; capturedSignal = {}; drawInfrared();
}

void infraredRequestDelete()
{
    if (infraredHasSelectedCommand())
    {
        deletePath = openPath;
        deleteName = commands[selected].name;
        deleteCommand = true;
        deleteCommandIndex = selected;
    }
    else
    {
        if (!infraredHasSelectedFile()) return;
        deletePath = files[selected].path;
        deleteName = files[selected].name;
        const int dot = deleteName.lastIndexOf('.');
        if (dot > 0) deleteName = deleteName.substring(0, dot);
        deleteName.replace('_', ' ');
        deleteCommand = false;
        deleteCommandIndex = -1;
    }
    deleteConfirmVisible = true;
    drawInfrared();
}

bool infraredModalActive() { return deleteConfirmVisible || nameModal != NameModal::None || captureActive; }

void infraredCancelModal()
{
    if (captureActive) stopInfraredCapture();
    captureActive = false; capturedSignal = {};
    deleteConfirmVisible = false;
    deletePath = deleteName = ""; deleteCommand = false; deleteCommandIndex = -1;
    nameModal = NameModal::None;
    nameInput = nameModalError = renamePath = "";
    drawInfrared();
}

namespace
{
void keepPinsDistinct()
{
    if (settings.rxEnabled && settings.txSource == InfraredTxSource::External &&
        settings.externalTxPin == settings.rxPin)
        settings.rxPin = settings.externalTxPin == 1 ? 2 : 1;
}
}

void infraredAdjustTxSource(int direction)
{
    int value = static_cast<int>(settings.txSource);
    value = (value + (direction < 0 ? -1 : 1) + 3) % 3;
    settings.txSource = static_cast<InfraredTxSource>(value); keepPinsDistinct(); saveHardwareSettings();
}
void infraredAdjustTxPin(int) { settings.externalTxPin = settings.externalTxPin == 1 ? 2 : 1; keepPinsDistinct(); saveHardwareSettings(); }
void infraredAdjustRxEnabled(int) { settings.rxEnabled = !settings.rxEnabled; keepPinsDistinct(); saveHardwareSettings(); }
void infraredAdjustRxPin(int) { settings.rxPin = settings.rxPin == 1 ? 2 : 1; keepPinsDistinct(); saveHardwareSettings(); }
void infraredAdjustRawFrequency(int direction)
{
    constexpr uint32_t values[] = {36000, 38000, 40000, 56000}; int index = 1;
    for (int i = 0; i < 4; ++i) if (settings.rawFrequency == values[i]) index = i;
    index = (index + (direction < 0 ? -1 : 1) + 4) % 4; settings.rawFrequency = values[index]; saveHardwareSettings();
}
void infraredAdjustRawDuty(int direction)
{
    constexpr uint8_t values[] = {25, 33, 50}; int index = 1;
    for (int i = 0; i < 3; ++i) if (settings.rawDutyPercent == values[i]) index = i;
    index = (index + (direction < 0 ? -1 : 1) + 3) % 3; settings.rawDutyPercent = values[index]; saveHardwareSettings();
}
String infraredTxSettingLabel() { return infraredTxSourceLabel(settings); }
String infraredTxPinLabel() { return "G" + String(settings.externalTxPin); }
String infraredRxSettingLabel() { return infraredRxLabel(settings); }
String infraredRxPinLabel() { return "G" + String(settings.rxPin); }
String infraredRawFrequencyLabel() { return String(settings.rawFrequency) + " Hz"; }
String infraredRawDutyLabel() { return "0." + String(settings.rawDutyPercent); }
bool infraredExternalTxSelected() { return settings.txSource == InfraredTxSource::External; }
bool infraredRxEnabled() { return settings.rxEnabled; }
