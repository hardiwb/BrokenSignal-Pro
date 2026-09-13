#pragma once

#include "module/service/Infrared.h"
#include <vector>

namespace InfraredInternal
{
enum class View : uint8_t { Files, Commands };
enum class NameModal : uint8_t { None, NewFolder, NewFile, RenameFile, CaptureCommand };
struct RemoteFile { String name; String path; bool directory = false; };
struct CommandSummary { String name; String type; bool supported = false; };

extern View view;
extern InfraredSettings settings;
extern std::vector<RemoteFile> files;
extern std::vector<CommandSummary> commands;
extern int selected;
extern int scrollTop;
extern uint32_t marqueeStartMs;
extern String openPath;
extern String openName;
extern String currentDirectory;
extern bool deleteConfirmVisible;
extern String deletePath;
extern String deleteName;
extern NameModal nameModal;
extern String nameInput;
extern String nameModalError;
extern String renamePath;
extern bool captureActive;
extern InfraredSignal capturedSignal;

bool loadRemoteFiles(const String &directoryPath, String &error);
bool loadRemoteCommands(const String &path, String &error);
bool loadRemoteSignal(const String &path, int index, InfraredSignal &signal, String &name, String &error);
bool appendRemoteSignal(const String &path, const String &name, const InfraredSignal &signal, String &error);
void clampSelection();
void moveSelection(int direction);
void activateSelection();
void returnToFiles();
void goToParentDirectory();
void saveHardwareSettings();
}
