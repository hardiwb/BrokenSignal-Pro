#include "apps/infrared/InfraredInternal.h"

#include <SD.h>
#include <algorithm>

namespace InfraredInternal
{
namespace
{
struct ParsedCommand
{
    InfraredSignal signal;
    String name;
    String type;
    uint8_t fields = 0;
};
enum Field : uint8_t
{
    NameField = 1 << 0, TypeField = 1 << 1, ProtocolField = 1 << 2,
    AddressField = 1 << 3, CommandField = 1 << 4, FrequencyField = 1 << 5,
    DutyField = 1 << 6, DataField = 1 << 7,
};

String afterColon(const String &line)
{
    String value = line.substring(line.indexOf(':') + 1); value.trim(); return value;
}
bool printableName(const String &name)
{
    if (!name.length() || name.length() > 80) return false;
    for (size_t i = 0; i < name.length(); ++i)
        if (name.charAt(i) < 32 || name.charAt(i) > 126) return false;
    return true;
}
bool parseBytes(const String &value, uint32_t &result)
{
    unsigned b0, b1, b2, b3; char trailing;
    if (sscanf(value.c_str(), "%x %x %x %x %c", &b0, &b1, &b2, &b3, &trailing) != 4 ||
        b0 > 255 || b1 > 255 || b2 > 255 || b3 > 255) return false;
    result = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24); return true;
}
bool parseUnsigned(const String &value, uint32_t minimum, uint32_t maximum, uint32_t &result)
{
    if (!value.length()) return false;
    uint64_t parsed = 0;
    for (size_t i = 0; i < value.length(); ++i)
    {
        if (!isDigit(value.charAt(i))) return false;
        parsed = parsed * 10 + static_cast<unsigned>(value.charAt(i) - '0');
        if (parsed > maximum) return false;
    }
    if (parsed < minimum) return false;
    result = parsed; return true;
}
bool parseDuty(const String &value, uint8_t &percent)
{
    const float parsed = value.toFloat();
    if (parsed < 0.10f || parsed > 0.90f) return false;
    percent = static_cast<uint8_t>(parsed * 100.0f + 0.5f); return true;
}
bool parseTimings(const String &value, std::vector<uint32_t> &timings, String &error)
{
    timings.clear(); int cursor = 0;
    while (cursor < static_cast<int>(value.length()))
    {
        while (cursor < static_cast<int>(value.length()) && value.charAt(cursor) == ' ') ++cursor;
        if (cursor >= static_cast<int>(value.length())) break;
        const int start = cursor;
        while (cursor < static_cast<int>(value.length()) && value.charAt(cursor) != ' ') ++cursor;
        uint32_t timing = 0;
        if (!parseUnsigned(value.substring(start, cursor), 1, 10000000, timing)) { error = "Invalid IR file"; return false; }
        if (timings.size() >= 1024) { error = "Raw data too long"; return false; }
        timings.push_back(timing);
    }
    if (timings.empty()) { error = "Invalid IR file"; return false; }
    return true;
}
bool setField(uint8_t &fields, Field field)
{
    if (fields & field) return false; fields |= field; return true;
}
bool protocolSupported(const ParsedCommand &command)
{
    return command.type == "parsed" &&
           (command.signal.protocol == "NEC" || command.signal.protocol == "NECext" || command.signal.protocol == "Samsung32") &&
           command.signal.address <= 0xffff && command.signal.command <= 0xffff;
}
bool finishCommand(ParsedCommand &command, int index, int targetIndex,
                   InfraredSignal *target, String *targetName, String &error)
{
    if (command.fields == 0) return true;
    if (!printableName(command.name) || !(command.fields & TypeField)) { error = "Invalid IR file"; return false; }
    bool supported = false;
    if (command.type == "parsed")
    {
        constexpr uint8_t required = NameField | TypeField | ProtocolField | AddressField | CommandField;
        if ((command.fields & required) != required) { error = "Invalid IR file"; return false; }
        command.signal.type = InfraredSignalType::Parsed; supported = protocolSupported(command);
    }
    else if (command.type == "raw")
    {
        constexpr uint8_t required = NameField | TypeField | FrequencyField | DutyField | DataField;
        if (!(command.fields & DataField)) { error = "Missing raw data"; return false; }
        if ((command.fields & required) != required) { error = "Invalid IR file"; return false; }
        command.signal.type = InfraredSignalType::Raw; supported = true;
    }
    else { error = "Invalid IR file"; return false; }
    for (const auto &existing : commands)
        if (existing.name == command.name) { error = "Invalid IR file"; return false; }
    CommandSummary summary;
    summary.name = command.name;
    summary.type = command.type == "raw" ? "RAW" : command.signal.protocol;
    summary.supported = supported;
    commands.push_back(summary);
    if (index == targetIndex && target && targetName) { *target = command.signal; *targetName = command.name; }
    command = {}; return true;
}
bool parseFile(const String &path, int targetIndex, InfraredSignal *target, String *targetName, String &error)
{
    commands.clear();
    File file = SD.open(path, FILE_READ);
    if (!file) { error = "Read failed"; return false; }
    String first = file.readStringUntil('\n'); first.trim();
    String second = file.readStringUntil('\n'); second.trim();
    const bool supportedHeader = first == "Filetype: IR signals file" || first == "Filetype: Bruce IR File";
    if (!supportedHeader || second != "Version: 1")
    { file.close(); error = "Invalid IR file"; return false; }
    ParsedCommand current; int index = 0;
    while (file.available())
    {
        String line = file.readStringUntil('\n'); line.trim();
        if (!line.length()) continue;
        if (line == "#")
        {
            if (current.fields && !finishCommand(current, index++, targetIndex, target, targetName, error))
            { file.close(); return false; }
            continue;
        }
        const int separator = line.indexOf(':'); if (separator < 0) continue;
        const String key = line.substring(0, separator); const String value = afterColon(line); bool ok = true;
        if (key == "name") { ok = setField(current.fields, NameField); current.name = value; }
        else if (key == "type") { ok = setField(current.fields, TypeField); current.type = value; }
        else if (key == "protocol") { ok = setField(current.fields, ProtocolField); current.signal.protocol = value; }
        else if (key == "address") ok = setField(current.fields, AddressField) && parseBytes(value, current.signal.address);
        else if (key == "command") ok = setField(current.fields, CommandField) && parseBytes(value, current.signal.command);
        else if (key == "frequency") ok = setField(current.fields, FrequencyField) && parseUnsigned(value, 30000, 60000, current.signal.frequency);
        else if (key == "duty_cycle") ok = setField(current.fields, DutyField) && parseDuty(value, current.signal.dutyPercent);
        else if (key == "data") ok = setField(current.fields, DataField) && parseTimings(value, current.signal.timings, error);
        if (!ok) { file.close(); if (!error.length()) error = "Invalid IR file"; return false; }
    }
    file.close();
    if (!finishCommand(current, index++, targetIndex, target, targetName, error))
    { if (!error.length()) error = "Invalid IR file"; return false; }
    if (commands.empty())
    {
        if (targetIndex < 0) return true;
        error = "No captured commands"; return false;
    }
    if (targetIndex >= static_cast<int>(commands.size())) { error = "Invalid IR command"; return false; }
    return true;
}
}

bool loadRemoteFiles(const String &directoryPath, String &error)
{
    files.clear(); SD.mkdir("/Infrared"); File directory = SD.open(directoryPath);
    if (!directory || !directory.isDirectory())
    { error = "SD card unavailable"; if (directory) directory.close(); return false; }
    File file = directory.openNextFile();
    while (file)
    {
        String name = file.name(); const bool directoryEntry = file.isDirectory(); file.close();
        const int slash = name.lastIndexOf('/'); if (slash >= 0) name = name.substring(slash + 1);
        String lower = name; lower.toLowerCase();
        if (directoryEntry || lower.endsWith(".ir"))
        {
            RemoteFile remote;
            remote.name = name;
            remote.path = directoryPath + "/" + name;
            remote.directory = directoryEntry;
            files.push_back(remote);
        }
        file = directory.openNextFile();
    }
    directory.close();
    std::sort(files.begin(), files.end(), [](const RemoteFile &left, const RemoteFile &right)
    {
        if (left.directory != right.directory) return left.directory;
        String a = left.name, b = right.name; a.toLowerCase(); b.toLowerCase(); return a < b;
    });
    return true;
}
bool loadRemoteCommands(const String &path, String &error) { return parseFile(path, -1, nullptr, nullptr, error); }
bool loadRemoteSignal(const String &path, int index, InfraredSignal &signal, String &name, String &error)
{ return parseFile(path, index, &signal, &name, error); }

bool appendRemoteSignal(const String &path, const String &name, const InfraredSignal &signal, String &error)
{
    if (!path.startsWith("/Infrared/") || !printableName(name) || signal.type != InfraredSignalType::Raw ||
        signal.timings.empty() || signal.timings.size() > 1024)
    { error = "Invalid captured signal"; return false; }
    for (const auto &command : commands)
        if (command.name == name) { error = "Name already exists"; return false; }

    File file = SD.open(path.c_str(), FILE_APPEND);
    if (!file) { error = "Write failed"; return false; }
    bool ok = file.print("\n#\nname: ") > 0 && file.println(name) > 0 &&
              file.print("type: raw\nfrequency: ") > 0 && file.println(signal.frequency) > 0 &&
              file.print("duty_cycle: ") > 0 &&
              file.println(static_cast<float>(signal.dutyPercent) / 100.0f, 2) > 0 &&
              file.print("data:") > 0;
    for (uint32_t timing : signal.timings)
        if (ok) ok = file.print(' ') > 0 && file.print(timing) > 0;
    if (ok) ok = file.println() > 0;
    file.close();
    if (!ok) { error = "Write failed"; return false; }
    return true;
}

namespace
{
bool commitCommandEdit(const String &path, const String &temporary, String &error)
{
    const String backup = path + ".bak";
    SD.remove(backup.c_str());
    if (!SD.rename(path.c_str(), backup.c_str()))
    {
        SD.remove(temporary.c_str()); error = "Write failed"; return false;
    }
    if (!SD.rename(temporary.c_str(), path.c_str()))
    {
        SD.rename(backup.c_str(), path.c_str()); error = "Write failed"; return false;
    }
    SD.remove(backup.c_str());
    return true;
}

bool editRemoteCommand(const String &path, int targetIndex, const String *newName, String &error)
{
    String lowerPath = path; lowerPath.toLowerCase();
    if (!path.startsWith("/Infrared/") || !lowerPath.endsWith(".ir") ||
        targetIndex < 0 || targetIndex >= static_cast<int>(commands.size()))
    { error = "Invalid IR command"; return false; }
    if (newName)
    {
        if (!printableName(*newName)) { error = "Invalid command name"; return false; }
        for (int i = 0; i < static_cast<int>(commands.size()); ++i)
            if (i != targetIndex && commands[i].name == *newName)
            { error = "Name already exists"; return false; }
    }

    File source = SD.open(path.c_str(), FILE_READ);
    if (!source) { error = "Read failed"; return false; }
    const String temporary = path + ".tmp";
    SD.remove(temporary.c_str());
    File output = SD.open(temporary.c_str(), FILE_WRITE);
    if (!output) { source.close(); error = "Write failed"; return false; }

    for (int headerLine = 0; headerLine < 2; ++headerLine)
    {
        if (!source.available())
        {
            source.close(); output.close(); SD.remove(temporary.c_str());
            error = "Invalid IR file"; return false;
        }
        const String line = source.readStringUntil('\n');
        if (output.print(line) != line.length() || output.print('\n') != 1)
        {
            source.close(); output.close(); SD.remove(temporary.c_str());
            error = "Write failed"; return false;
        }
    }

    String block;
    bool blockHasCommand = false;
    int commandIndex = 0;
    bool ok = true;
    auto flushBlock = [&]()
    {
        if (!ok || !block.length()) return;
        if (!blockHasCommand || newName || commandIndex != targetIndex)
            ok = output.print(block) == block.length();
        if (blockHasCommand) ++commandIndex;
        block = ""; blockHasCommand = false;
    };

    while (source.available() && ok)
    {
        String line = source.readStringUntil('\n');
        String trimmed = line; trimmed.trim();
        if (trimmed == "#") flushBlock();
        if (!ok) break;
        if (trimmed.startsWith("name:"))
        {
            blockHasCommand = true;
            if (newName && commandIndex == targetIndex) line = "name: " + *newName;
        }
        block += line + "\n";
    }
    flushBlock();
    source.close(); output.close();
    if (!ok || commandIndex != static_cast<int>(commands.size()))
    {
        SD.remove(temporary.c_str()); error = "Write failed"; return false;
    }
    return commitCommandEdit(path, temporary, error);
}
}

bool renameRemoteCommand(const String &path, int index, const String &name, String &error)
{
    return editRemoteCommand(path, index, &name, error);
}

bool deleteRemoteCommand(const String &path, int index, String &error)
{
    return editRemoteCommand(path, index, nullptr, error);
}
}
