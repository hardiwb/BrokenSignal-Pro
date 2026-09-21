#include "apps/macropad/MacroPad.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <algorithm>
#include <vector>

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/service/Bluetooth.h"
#include "module/shell/Help.h"
#include "module/shell/Options.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"

namespace
{
constexpr const char *MACRO_DIRECTORY = "/Macros";
constexpr size_t MAX_CONFIG_BYTES = 32768;
constexpr size_t MAX_MACROS = 36;
constexpr size_t MAX_CATALOG_MACROS = 128;
constexpr size_t MAX_SEQUENCE_KEYS = 32;
constexpr uint16_t DEFAULT_PRESS_MS = 55;
constexpr uint16_t DEFAULT_GAP_MS = 40;

struct GameFile
{
    String name;
    String path;
    bool valid = false;
};

struct MacroDefinition
{
    String id;
    String name;
    char hotkey = 0;
    uint8_t modifiers = 0;
    std::vector<uint8_t> sequence;
    uint16_t pressMs = DEFAULT_PRESS_MS;
    uint16_t gapMs = DEFAULT_GAP_MS;
    uint8_t activationMouseButton = 0;
    uint8_t activationModifier = 0;
    bool holdActivation = false;
};

struct CatalogMacro
{
    String id;
    MacroDefinition macro;
};

enum class View : uint8_t
{
    Games,
    Devices,
    Macros
};

enum class EditorMode : uint8_t
{
    None,
    Catalog,
    Hotkey,
    RemoveConfirm
};

enum class MacroPhase : uint8_t
{
    Idle,
    ActivationDown,
    ActivationGap,
    KeyDown,
    KeyGap,
    FinalGap
};

std::vector<GameFile> games;
std::vector<MacroDefinition> macros;
std::vector<CatalogMacro> catalog;
View view = View::Games;
EditorMode editorMode = EditorMode::None;
String currentGameName;
String currentGamePath;
String configError;
String editorError;
int currentGameVersion = 0;
uint8_t currentDefaultActivationButton = 0;
uint8_t currentDefaultActivationModifier = 0;
bool currentDefaultHoldActivation = false;
int selectedGame = 0;
int gameScrollTop = 0;
int selectedDevice = 0;
int deviceScrollTop = 0;
int selectedMacro = 0;
int macroScrollTop = 0;
int activeMacro = -1;
int selectedCatalog = 0;
int catalogScrollTop = 0;
int editingMacro = -1;
int pendingCatalog = -1;
size_t activeStep = 0;
MacroPhase macroPhase = MacroPhase::Idle;
unsigned long nextReportMs = 0;
uint32_t marqueeStartMs = 0;

String upperToken(String token)
{
    token.trim();
    token.toUpperCase();
    return token;
}

bool printableName(const String &value)
{
    if (value.length() == 0 || value.length() > 48)
        return false;
    for (size_t i = 0; i < value.length(); ++i)
        if (value.charAt(i) < 32 || value.charAt(i) > 126)
            return false;
    return true;
}

char normalizeHotkey(const String &value)
{
    if (value.length() != 1)
        return 0;
    char key = value.charAt(0);
    if (key >= 'A' && key <= 'Z')
        key = key - 'A' + 'a';
    return ((key >= 'a' && key <= 'z') || (key >= '0' && key <= '9'))
        ? key : 0;
}

char displayHotkey(char hotkey);

uint8_t hidUsageForToken(const String &rawToken)
{
    const String token = upperToken(rawToken);
    if (token.length() == 1 && token.charAt(0) >= 'A' && token.charAt(0) <= 'Z')
        return 0x04 + token.charAt(0) - 'A';
    if (token.length() == 1 && token.charAt(0) >= '1' && token.charAt(0) <= '9')
        return 0x1E + token.charAt(0) - '1';
    if (token == "0") return 0x27;
    if (token == "ENTER") return 0x28;
    if (token == "ESC" || token == "ESCAPE") return 0x29;
    if (token == "BACKSPACE") return 0x2A;
    if (token == "TAB") return 0x2B;
    if (token == "SPACE") return 0x2C;
    if (token == "MINUS") return 0x2D;
    if (token == "EQUAL") return 0x2E;
    if (token == "LEFT_BRACKET") return 0x2F;
    if (token == "RIGHT_BRACKET") return 0x30;
    if (token == "BACKSLASH") return 0x31;
    if (token == "SEMICOLON") return 0x33;
    if (token == "APOSTROPHE") return 0x34;
    if (token == "GRAVE") return 0x35;
    if (token == "COMMA") return 0x36;
    if (token == "PERIOD") return 0x37;
    if (token == "SLASH") return 0x38;
    if (token == "CAPS_LOCK") return 0x39;
    if (token == "RIGHT") return 0x4F;
    if (token == "LEFT") return 0x50;
    if (token == "DOWN") return 0x51;
    if (token == "UP") return 0x52;
    if (token == "DELETE") return 0x4C;
    if (token.startsWith("F"))
    {
        const int functionNumber = token.substring(1).toInt();
        if (functionNumber >= 1 && functionNumber <= 12 &&
            token == "F" + String(functionNumber))
            return 0x3A + functionNumber - 1;
    }
    return 0;
}

uint8_t modifierForToken(const String &rawToken)
{
    const String token = upperToken(rawToken);
    if (token == "LEFT_CTRL" || token == "CTRL") return 0x01;
    if (token == "LEFT_SHIFT" || token == "SHIFT") return 0x02;
    if (token == "LEFT_ALT" || token == "ALT") return 0x04;
    if (token == "LEFT_GUI" || token == "GUI" || token == "WIN") return 0x08;
    if (token == "RIGHT_CTRL") return 0x10;
    if (token == "RIGHT_SHIFT") return 0x20;
    if (token == "RIGHT_ALT") return 0x40;
    if (token == "RIGHT_GUI") return 0x80;
    return 0;
}

bool parseModifiers(JsonVariantConst value, uint8_t &modifiers)
{
    modifiers = 0;
    if (value.isNull())
        return true;
    if (!value.is<JsonArrayConst>())
        return false;
    for (JsonVariantConst entry : value.as<JsonArrayConst>())
    {
        if (!entry.is<const char *>())
            return false;
        const uint8_t modifier = modifierForToken(String(entry.as<const char *>()));
        if (modifier == 0)
            return false;
        modifiers |= modifier;
    }
    return true;
}

bool parseActivation(JsonVariantConst value, uint8_t &mouseButton,
                     uint8_t &keyModifier, bool &holdActivation)
{
    mouseButton = 0;
    keyModifier = 0;
    holdActivation = false;
    if (value.isNull())
        return true;
    if (!value.is<JsonObjectConst>())
        return false;
    const JsonObjectConst object = value.as<JsonObjectConst>();
    if (!object["mode"].is<const char *>())
        return false;
    const String mode = upperToken(String(object["mode"].as<const char *>()));
    if (mode != "PRESS" && mode != "HOLD")
        return false;
    const bool hasMouse = object["mouse_button"].is<int>();
    const bool hasKey = object["key"].is<const char *>();
    if (hasMouse == hasKey)
        return false;
    if (hasMouse)
    {
        const int button = object["mouse_button"].as<int>();
        if (button < 1 || button > 5)
            return false;
        mouseButton = static_cast<uint8_t>(button);
    }
    else
    {
        keyModifier = modifierForToken(String(object["key"].as<const char *>()));
        if (keyModifier == 0)
            return false;
    }
    holdActivation = mode == "HOLD";
    return true;
}

bool openJson(const String &path, JsonDocument &document, String &error)
{
    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        error = "Cannot read JSON";
        return false;
    }
    if (file.size() == 0 || file.size() > MAX_CONFIG_BYTES)
    {
        file.close();
        error = "JSON size invalid";
        return false;
    }
    const DeserializationError jsonError = deserializeJson(document, file);
    file.close();
    if (jsonError)
    {
        error = "Invalid JSON";
        return false;
    }
    return true;
}

bool readGameHeader(const String &path, String &name)
{
    JsonDocument document;
    String error;
    if (!openJson(path, document, error))
        return false;
    const int version = document["version"] | 0;
    if ((version != 1 && version != 2) ||
        !document["name"].is<const char *>() ||
        !document["macros"].is<JsonArrayConst>() ||
        (version == 2 && !document["bindings"].is<JsonObjectConst>()))
        return false;
    name = String(document["name"].as<const char *>());
    return printableName(name);
}

void scanGames()
{
    games.clear();
    configError = "";
    SD.mkdir(MACRO_DIRECTORY);
    File directory = SD.open(MACRO_DIRECTORY);
    if (!directory || !directory.isDirectory())
    {
        if (directory) directory.close();
        configError = "SD /Macros unavailable";
        return;
    }

    File file = directory.openNextFile();
    while (file)
    {
        String filename = file.name();
        const bool regularFile = !file.isDirectory();
        file.close();
        const int slash = filename.lastIndexOf('/');
        if (slash >= 0)
            filename = filename.substring(slash + 1);
        String lower = filename;
        lower.toLowerCase();
        if (regularFile && lower.endsWith(".json"))
        {
            GameFile game;
            game.path = String(MACRO_DIRECTORY) + "/" + filename;
            game.valid = readGameHeader(game.path, game.name);
            if (!game.valid)
            {
                game.name = filename.substring(0, filename.length() - 5);
                game.name.replace('_', ' ');
            }
            games.push_back(game);
        }
        file = directory.openNextFile();
    }
    directory.close();

    std::sort(games.begin(), games.end(), [](const GameFile &left, const GameFile &right)
    {
        String a = left.name;
        String b = right.name;
        a.toLowerCase();
        b.toLowerCase();
        return a < b;
    });
    selectedGame = games.empty()
        ? 0 : constrain(selectedGame, 0, static_cast<int>(games.size()) - 1);
    gameScrollTop = min(gameScrollTop, selectedGame);
}

uint16_t parseTiming(JsonVariantConst value, uint16_t fallback)
{
    if (!value.is<int>())
        return fallback;
    return static_cast<uint16_t>(constrain(value.as<int>(), 10, 1000));
}

bool validMacroId(String id)
{
    id.trim();
    id.toLowerCase();
    if (id.length() == 0 || id.length() > 32)
        return false;
    for (size_t i = 0; i < id.length(); ++i)
    {
        const char c = id.charAt(i);
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return false;
    }
    return true;
}

bool parseMacroDefinition(
    JsonObjectConst object, bool requireHotkey,
    uint8_t defaultModifiers, uint8_t defaultActivationButton,
    uint8_t defaultActivationModifier, bool defaultHoldActivation,
    uint16_t defaultPressMs,
    uint16_t defaultGapMs, MacroDefinition &macro)
{
    if (!object["name"].is<const char *>() ||
        !object["sequence"].is<JsonArrayConst>() ||
        (requireHotkey && !object["hotkey"].is<const char *>()))
    {
        configError = "Macro fields missing";
        return false;
    }

    macro = {};
    macro.name = String(object["name"].as<const char *>());
    macro.hotkey = requireHotkey
        ? normalizeHotkey(String(object["hotkey"].as<const char *>())) : 0;
    macro.modifiers = defaultModifiers;
    macro.pressMs = parseTiming(object["press_ms"], defaultPressMs);
    macro.gapMs = parseTiming(object["gap_ms"], defaultGapMs);
    macro.activationMouseButton = defaultActivationButton;
    macro.activationModifier = defaultActivationModifier;
    macro.holdActivation = defaultHoldActivation;
    if (!printableName(macro.name) || (requireHotkey && macro.hotkey == 0))
    {
        configError = requireHotkey ? "Invalid macro hotkey" : "Invalid macro name";
        return false;
    }
    if (!object["hold"].isNull() &&
        !parseModifiers(object["hold"], macro.modifiers))
    {
        configError = "Invalid macro hold";
        return false;
    }
    if (!object["activation"].isNull() &&
        !parseActivation(object["activation"], macro.activationMouseButton,
                         macro.activationModifier, macro.holdActivation))
    {
        configError = "Invalid macro activation";
        return false;
    }

    const JsonArrayConst sequence = object["sequence"].as<JsonArrayConst>();
    if (sequence.size() == 0 || sequence.size() > MAX_SEQUENCE_KEYS)
    {
        configError = "Invalid sequence length";
        return false;
    }
    for (JsonVariantConst key : sequence)
    {
        if (!key.is<const char *>())
        {
            configError = "Invalid sequence key";
            return false;
        }
        const String token = upperToken(String(key.as<const char *>()));
        const uint8_t usage = hidUsageForToken(token);
        if (usage == 0)
        {
            configError = "Unknown key: " + token;
            return false;
        }
        macro.sequence.push_back(usage);
    }
    return true;
}

bool loadGame(const GameFile &game)
{
    macros.clear();
    catalog.clear();
    configError = "";
    JsonDocument document;
    if (!openJson(game.path, document, configError))
        return false;
    const int version = document["version"] | 0;
    if ((version != 1 && version != 2) ||
        !document["name"].is<const char *>() ||
        !document["macros"].is<JsonArrayConst>() ||
        (version == 2 && !document["bindings"].is<JsonObjectConst>()))
    {
        configError = "Unsupported macro JSON";
        return false;
    }

    currentGameName = String(document["name"].as<const char *>());
    if (!printableName(currentGameName))
    {
        configError = "Invalid game name";
        return false;
    }

    uint8_t defaultModifiers = 0;
    if (!parseModifiers(document["default_hold"], defaultModifiers))
    {
        configError = "Invalid default_hold";
        return false;
    }
    uint8_t defaultActivationButton = 0;
    uint8_t defaultActivationModifier = 0;
    bool defaultHoldActivation = false;
    if (!parseActivation(document["activation"], defaultActivationButton,
                         defaultActivationModifier, defaultHoldActivation))
    {
        configError = "Invalid activation";
        return false;
    }
    const uint16_t defaultPressMs = parseTiming(document["press_ms"], DEFAULT_PRESS_MS);
    const uint16_t defaultGapMs = parseTiming(document["gap_ms"], DEFAULT_GAP_MS);
    bool usedHotkeys[128] = {};

    if (version == 1)
    {
        for (JsonVariantConst entry : document["macros"].as<JsonArrayConst>())
        {
            if (!entry.is<JsonObjectConst>() || macros.size() >= MAX_MACROS)
            {
                configError = macros.size() >= MAX_MACROS
                    ? "Too many macros" : "Invalid macro entry";
                macros.clear();
                return false;
            }
            MacroDefinition macro;
            if (!parseMacroDefinition(
                    entry.as<JsonObjectConst>(), true, defaultModifiers,
                    defaultActivationButton, defaultActivationModifier,
                    defaultHoldActivation,
                    defaultPressMs, defaultGapMs, macro) ||
                usedHotkeys[static_cast<uint8_t>(macro.hotkey)])
            {
                if (!configError.length())
                    configError = "Duplicate hotkey";
                macros.clear();
                return false;
            }
            usedHotkeys[static_cast<uint8_t>(macro.hotkey)] = true;
            macros.push_back(macro);
        }
    }
    else
    {
        for (JsonVariantConst entry : document["macros"].as<JsonArrayConst>())
        {
            if (!entry.is<JsonObjectConst>() || catalog.size() >= MAX_CATALOG_MACROS)
            {
                configError = catalog.size() >= MAX_CATALOG_MACROS
                    ? "Catalog too large" : "Invalid catalog entry";
                return false;
            }
            const JsonObjectConst object = entry.as<JsonObjectConst>();
            if (!object["id"].is<const char *>())
            {
                configError = "Macro id missing";
                return false;
            }
            CatalogMacro item;
            item.id = String(object["id"].as<const char *>());
            item.id.trim();
            item.id.toLowerCase();
            if (!validMacroId(item.id) ||
                !parseMacroDefinition(
                    object, false, defaultModifiers, defaultActivationButton,
                    defaultActivationModifier, defaultHoldActivation,
                    defaultPressMs, defaultGapMs,
                    item.macro))
                return false;
            for (const CatalogMacro &existing : catalog)
            {
                if (existing.id == item.id)
                {
                    configError = "Duplicate macro id";
                    return false;
                }
            }
            catalog.push_back(item);
        }

        for (JsonPairConst binding : document["bindings"].as<JsonObjectConst>())
        {
            if (macros.size() >= MAX_MACROS || !binding.value().is<const char *>())
            {
                configError = macros.size() >= MAX_MACROS
                    ? "Too many bindings" : "Invalid binding";
                macros.clear();
                return false;
            }
            const char hotkey = normalizeHotkey(String(binding.key().c_str()));
            String targetId = String(binding.value().as<const char *>());
            targetId.trim();
            targetId.toLowerCase();
            if (hotkey == 0 || usedHotkeys[static_cast<uint8_t>(hotkey)])
            {
                configError = "Invalid binding key";
                macros.clear();
                return false;
            }
            const CatalogMacro *target = nullptr;
            for (const CatalogMacro &candidate : catalog)
            {
                if (candidate.id == targetId)
                {
                    target = &candidate;
                    break;
                }
            }
            if (!target)
            {
                configError = "Unknown binding: " + targetId;
                macros.clear();
                return false;
            }
            MacroDefinition bound = target->macro;
            bound.id = target->id;
            bound.hotkey = hotkey;
            usedHotkeys[static_cast<uint8_t>(hotkey)] = true;
            macros.push_back(bound);
        }
    }

    if (version == 1 && macros.empty())
    {
        configError = "No macros configured";
        return false;
    }
    currentGamePath = game.path;
    currentGameVersion = version;
    currentDefaultActivationButton = defaultActivationButton;
    currentDefaultActivationModifier = defaultActivationModifier;
    currentDefaultHoldActivation = defaultHoldActivation;
    selectedMacro = 0;
    macroScrollTop = 0;
    selectedCatalog = 0;
    catalogScrollTop = 0;
    editorMode = EditorMode::None;
    return true;
}

bool catalogMacroBound(const String &id)
{
    for (const MacroDefinition &macro : macros)
        if (macro.id == id)
            return true;
    return false;
}

std::vector<int> availableCatalogIndexes()
{
    std::vector<int> indexes;
    indexes.reserve(catalog.size());
    for (int i = 0; i < static_cast<int>(catalog.size()); ++i)
        if (!catalogMacroBound(catalog[i].id))
            indexes.push_back(i);
    return indexes;
}

bool hotkeyInUse(char hotkey, int exceptIndex = -1)
{
    for (int i = 0; i < static_cast<int>(macros.size()); ++i)
        if (i != exceptIndex && macros[i].hotkey == hotkey)
            return true;
    return false;
}

bool writeCurrentGameJson(JsonDocument &document, String &error)
{
    const String temporaryPath = currentGamePath + ".tmp";
    const String backupPath = currentGamePath + ".bak";
    SD.remove(temporaryPath.c_str());
    File output = SD.open(temporaryPath.c_str(), FILE_WRITE);
    if (!output)
    {
        error = "Cannot create JSON";
        return false;
    }
    const size_t written = serializeJsonPretty(document, output);
    output.flush();
    const size_t fileSize = output.size();
    output.close();
    if (written == 0 || fileSize == 0 || fileSize > MAX_CONFIG_BYTES)
    {
        SD.remove(temporaryPath.c_str());
        error = "Saved JSON too large";
        return false;
    }
    JsonDocument validation;
    String validationError;
    if (!openJson(temporaryPath, validation, validationError) ||
        (validation["version"] | 0) != 2 ||
        !validation["bindings"].is<JsonObjectConst>())
    {
        SD.remove(temporaryPath.c_str());
        error = "Cannot verify saved JSON";
        return false;
    }

    SD.remove(backupPath.c_str());
    if (!SD.rename(currentGamePath.c_str(), backupPath.c_str()))
    {
        SD.remove(temporaryPath.c_str());
        error = "Cannot back up JSON";
        return false;
    }
    if (!SD.rename(temporaryPath.c_str(), currentGamePath.c_str()))
    {
        SD.rename(backupPath.c_str(), currentGamePath.c_str());
        SD.remove(temporaryPath.c_str());
        error = "Cannot replace JSON";
        return false;
    }
    SD.remove(backupPath.c_str());
    return true;
}

bool saveBindings(String &error)
{
    if (currentGameVersion != 2 || !currentGamePath.length())
    {
        error = "Version 2 required";
        return false;
    }

    JsonDocument document;
    if (!openJson(currentGamePath, document, error))
        return false;
    if ((document["version"] | 0) != 2 ||
        !document["bindings"].is<JsonObjectConst>())
    {
        error = "Bindings unavailable";
        return false;
    }

    JsonObject bindings = document["bindings"].as<JsonObject>();
    bindings.clear();
    for (const MacroDefinition &macro : macros)
    {
        char key[2] = {displayHotkey(macro.hotkey), '\0'};
        bindings[key] = macro.id;
    }

    return writeCurrentGameJson(document, error);
}

void closeEditor()
{
    editorMode = EditorMode::None;
    editorError = "";
    editingMacro = -1;
    pendingCatalog = -1;
    drawMacroPadApp();
}

void beginAddMacro()
{
    if (currentGameVersion != 2)
    {
        showHdrMsg("Version 2 required");
        return;
    }
    if (macros.size() >= MAX_MACROS)
    {
        showHdrMsg("No free macro keys");
        return;
    }
    const std::vector<int> available = availableCatalogIndexes();
    if (available.empty())
    {
        showHdrMsg("All macros assigned");
        return;
    }
    selectedCatalog = constrain(selectedCatalog, 0,
                                static_cast<int>(available.size()) - 1);
    catalogScrollTop = min(catalogScrollTop, selectedCatalog);
    editorMode = EditorMode::Catalog;
    editorError = "";
    drawMacroPadApp();
}

void beginHotkeyEdit(int macroIndex, int catalogIndex)
{
    editingMacro = macroIndex;
    pendingCatalog = catalogIndex;
    editorError = "";
    editorMode = EditorMode::Hotkey;
    drawMacroPadApp();
}

void applyHotkey(char hotkey)
{
    if (hotkeyInUse(hotkey, editingMacro))
    {
        editorError = "Key already assigned";
        drawMacroPadApp();
        return;
    }

    String error;
    if (editingMacro >= 0)
    {
        const char previous = macros[editingMacro].hotkey;
        macros[editingMacro].hotkey = hotkey;
        if (!saveBindings(error))
        {
            macros[editingMacro].hotkey = previous;
            editorError = error;
            drawMacroPadApp();
            return;
        }
        selectedMacro = editingMacro;
    }
    else
    {
        if (pendingCatalog < 0 || pendingCatalog >= static_cast<int>(catalog.size()))
        {
            editorError = "Macro unavailable";
            drawMacroPadApp();
            return;
        }
        MacroDefinition added = catalog[pendingCatalog].macro;
        added.id = catalog[pendingCatalog].id;
        added.hotkey = hotkey;
        macros.push_back(added);
        if (!saveBindings(error))
        {
            macros.pop_back();
            editorError = error;
            drawMacroPadApp();
            return;
        }
        selectedMacro = static_cast<int>(macros.size()) - 1;
        macroScrollTop = max(0, selectedMacro - LIST_VISIBLE_ITEM + 1);
    }

    editorMode = EditorMode::None;
    editorError = "";
    editingMacro = -1;
    pendingCatalog = -1;
    drawMacroPadApp();
    showHdrMsg("Binding saved");
}

void removeSelectedBinding()
{
    if (selectedMacro < 0 || selectedMacro >= static_cast<int>(macros.size()))
        return;
    const int removedIndex = selectedMacro;
    const MacroDefinition removed = macros[removedIndex];
    macros.erase(macros.begin() + removedIndex);
    String error;
    if (!saveBindings(error))
    {
        macros.insert(macros.begin() + removedIndex, removed);
        editorError = error;
        drawMacroPadApp();
        return;
    }
    selectedMacro = macros.empty()
        ? 0 : min(removedIndex, static_cast<int>(macros.size()) - 1);
    macroScrollTop = min(macroScrollTop, selectedMacro);
    editorMode = EditorMode::None;
    editorError = "";
    drawMacroPadApp();
    showHdrMsg("Binding removed");
}

int deviceItemCount()
{
    return BluetoothService::bondCount() +
           (BluetoothService::canPairNew() ? 1 : 0);
}

char displayHotkey(char hotkey)
{
    return hotkey >= 'a' && hotkey <= 'z' ? hotkey - 'a' + 'A' : hotkey;
}

ListModel gameListModel()
{
    ListModel model;
    model.selected = selectedGame;
    model.scrollTop = gameScrollTop;
    model.marqueeStartMs = marqueeStartMs;
    if (games.empty())
    {
        ListItemModel item;
        item.label = configError.length() ? configError : "No game JSON files";
        item.isSelected = true;
        item.isDimmed = true;
        model.items.push_back(item);
        return model;
    }
    for (int i = 0; i < static_cast<int>(games.size()); ++i)
    {
        ListItemModel item;
        item.type = ListItemType::Property;
        item.label = games[i].name;
        item.value = games[i].valid ? "JSON" : "INVALID";
        item.propertyWidth = 45;
        item.isSelected = i == selectedGame;
        item.isDimmed = !games[i].valid;
        model.items.push_back(item);
    }
    return model;
}

ListModel deviceListModel()
{
    ListModel model;
    model.selected = selectedDevice;
    model.scrollTop = deviceScrollTop;
    for (int i = 0; i < BluetoothService::bondCount(); ++i)
    {
        ListItemModel item;
        item.type = ListItemType::Property;
        const String name = BluetoothService::bondName(i);
        item.label = name.length() > 0 ? name : "PC " + String(i + 1);
        item.value = BluetoothService::bondAddressText(i);
        item.propertyWidth = 105;
        item.isSelected = i == selectedDevice;
        model.items.push_back(item);
    }
    if (BluetoothService::canPairNew())
    {
        ListItemModel item;
        item.label = "Pair New PC";
        item.isSelected = selectedDevice == BluetoothService::bondCount();
        model.items.push_back(item);
    }
    return model;
}

ListModel macroListModel()
{
    ListModel model;
    model.selected = selectedMacro;
    model.scrollTop = macroScrollTop;
    model.marqueeStartMs = marqueeStartMs;
    if (macros.empty())
    {
        ListItemModel item;
        item.label = currentGameVersion == 2
            ? "No macros assigned - open Options" : "No macros assigned";
        item.isSelected = true;
        item.isDimmed = true;
        model.items.push_back(item);
        return model;
    }
    for (int i = 0; i < static_cast<int>(macros.size()); ++i)
    {
        ListItemModel item;
        item.type = ListItemType::Property;
        item.label = macros[i].name;
        item.value = "[" + String(displayHotkey(macros[i].hotkey)) + "]";
        item.propertyWidth = 28;
        item.isSelected = i == selectedMacro;
        item.isActive = i == activeMacro;
        model.items.push_back(item);
    }
    return model;
}

ListModel catalogListModel()
{
    const std::vector<int> available = availableCatalogIndexes();
    ListModel model;
    model.selected = selectedCatalog;
    model.scrollTop = catalogScrollTop;
    model.marqueeStartMs = marqueeStartMs;
    for (int catalogIndex : available)
    {
        ListItemModel item;
        item.label = catalog[catalogIndex].macro.name;
        item.isSelected = static_cast<int>(model.items.size()) == selectedCatalog;
        model.items.push_back(item);
    }
    return model;
}

void drawGames()
{
    HeaderModel header;
    header.appHeaderTag = "MACRO PAD";
    header.appHeaderTitle = "/Macros games";
    header.cursor = true;
    drawHeader(header);
    drawList(gameListModel());
    FooterModel footer;
    footer.left = "[;/.]Move [Ok]Open";
    footer.center = "[R]Reload";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void drawDevices()
{
    BluetoothService::refreshBonds();
    const int count = deviceItemCount();
    selectedDevice = count > 0 ? constrain(selectedDevice, 0, count - 1) : 0;
    deviceScrollTop = min(deviceScrollTop, selectedDevice);
    HeaderModel header;
    header.appHeaderTag = "MACRO PAD";
    header.appHeaderTitle = currentGameName;
    header.cursor = true;
    drawHeader(header);
    drawList(deviceListModel());
    FooterModel footer;
    footer.left = "[Ok]Connect [Del]Games";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void drawConnectionOverlay()
{
    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = "MACRO PAD";
    model.confirmText = "[BtnG0] Disconnect";
    switch (BluetoothService::keyboardLinkState())
    {
    case BluetoothService::KeyboardLinkState::Advertising:
        model.items = {
            BluetoothService::targetBondSelected() ? "WAITING FOR SAVED PC" : "PAIRING MODE",
            BluetoothService::targetBondSelected()
                ? BluetoothService::targetAddressText()
                : "Select BrokenSignal Keyboard"};
        break;
    case BluetoothService::KeyboardLinkState::Connected:
        model.items = {"SECURING CONNECTION", BluetoothService::peerAddressText()};
        break;
    case BluetoothService::KeyboardLinkState::Pairing:
    {
        char pin[16];
        snprintf(pin, sizeof(pin), "PIN %06lu",
                 static_cast<unsigned long>(BluetoothService::pairingPasskey()));
        model.items = {pin, "Enter PIN on host"};
        break;
    }
    case BluetoothService::KeyboardLinkState::Failed:
        model.items = {"CONNECTION REJECTED", BluetoothService::keyboardFailureText()};
        break;
    case BluetoothService::KeyboardLinkState::Idle:
        model.items = {"DISCONNECTED"};
        break;
    case BluetoothService::KeyboardLinkState::Ready:
        return;
    }
    drawOverlay(model);
}

void drawMacros()
{
    HeaderModel header;
    header.appHeaderTag = "MACRO PAD";
    header.appHeaderTitle = activeMacro >= 0
        ? "Sending " + String(displayHotkey(macros[activeMacro].hotkey))
        : currentGameName;
    header.cursor = true;
    drawHeader(header);
    drawList(macroListModel());
    FooterModel footer;
    footer.left = "[Hotkey]Run [;/.]Move";
    footer.center = activeMacro >= 0 ? "SENDING" : "[Opt]Edit";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void drawCatalogEditor()
{
    HeaderModel header;
    header.appHeaderTag = "MACRO PAD";
    header.appHeaderTitle = "Add macro";
    header.cursor = true;
    drawHeader(header);
    drawList(catalogListModel());
    FooterModel footer;
    footer.left = "[;/.]Move [Ok]Select";
    footer.center = "[Esc]Cancel";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void drawHotkeyEditor()
{
    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = editorError.length() ? editorError : "ASSIGN MACRO KEY";
    String name;
    if (editingMacro >= 0 && editingMacro < static_cast<int>(macros.size()))
        name = macros[editingMacro].name;
    else if (pendingCatalog >= 0 && pendingCatalog < static_cast<int>(catalog.size()))
        name = catalog[pendingCatalog].macro.name;
    model.items = {name, "Press 0-9 or A-Z"};
    model.confirmText = "[Esc]Cancel";
    drawOverlay(model);
}

void drawRemoveBindingConfirm()
{
    OverlayModel model;
    model.type = OverlayType::Confirm;
    model.title = editorError.length() ? editorError : "REMOVE BINDING";
    if (selectedMacro >= 0 && selectedMacro < static_cast<int>(macros.size()))
        model.prompt = macros[selectedMacro].name + " [" +
                       String(displayHotkey(macros[selectedMacro].hotkey)) + "]";
    model.confirmText = "[Esc]Cancel [Ok]Remove";
    drawOverlay(model);
}

void drawMacroPadSurface()
{
    switch (editorMode)
    {
    case EditorMode::Catalog:
        drawCatalogEditor();
        break;
    case EditorMode::Hotkey:
        drawHotkeyEditor();
        break;
    case EditorMode::RemoveConfirm:
        drawRemoveBindingConfirm();
        break;
    case EditorMode::None:
        drawMacros();
        break;
    }
}

void moveListSelection(int &selected, int &scrollTop, int count, int direction,
                       void (*drawScreen)())
{
    if (count <= 0)
        return;
    selected = (selected + direction + count) % count;
    if (selected < scrollTop)
        scrollTop = selected;
    else if (selected >= scrollTop + LIST_VISIBLE_ITEM)
        scrollTop = selected - LIST_VISIBLE_ITEM + 1;
    marqueeStartMs = millis();
    drawScreen();
}

void sendMacroReport(uint8_t modifiers, uint8_t usage)
{
    uint8_t report[8] = {};
    report[0] = modifiers;
    report[2] = usage;
    BluetoothService::sendKeyboardReport(report);
}

void releaseMacro()
{
    const uint8_t report[8] = {};
    BluetoothService::sendKeyboardReport(report);
    BluetoothService::sendMouseButtons(0);
    activeMacro = -1;
    activeStep = 0;
    macroPhase = MacroPhase::Idle;
}

void startFirstSequenceKey()
{
    MacroDefinition &macro = macros[activeMacro];
    const uint8_t modifiers = macro.modifiers |
        (macro.holdActivation ? macro.activationModifier : 0);
    sendMacroReport(modifiers, macro.sequence[0]);
    macroPhase = MacroPhase::KeyDown;
    nextReportMs = millis() + macro.pressMs;
}

void startMacro(int index)
{
    if (!BluetoothService::keyboardReady() || macroPhase != MacroPhase::Idle ||
        index < 0 || index >= static_cast<int>(macros.size()))
        return;
    selectedMacro = index;
    if (selectedMacro < macroScrollTop)
        macroScrollTop = selectedMacro;
    else if (selectedMacro >= macroScrollTop + LIST_VISIBLE_ITEM)
        macroScrollTop = selectedMacro - LIST_VISIBLE_ITEM + 1;
    activeMacro = index;
    activeStep = 0;
    MacroDefinition &macro = macros[index];
    if (macro.activationMouseButton > 0)
    {
        BluetoothService::sendMouseButtons(
            1U << (macro.activationMouseButton - 1));
        if (macro.holdActivation)
            startFirstSequenceKey();
        else
        {
            macroPhase = MacroPhase::ActivationDown;
            nextReportMs = millis() + macro.pressMs;
        }
    }
    else if (macro.activationModifier > 0)
    {
        sendMacroReport(macro.activationModifier, 0);
        macroPhase = macro.holdActivation
            ? MacroPhase::ActivationGap : MacroPhase::ActivationDown;
        nextReportMs = millis() +
            (macro.holdActivation ? macro.gapMs : macro.pressMs);
    }
    else
        startFirstSequenceKey();
    lastActivityMs = millis();
    drawMacros();
}

void openSelectedGame()
{
    if (games.empty() || selectedGame < 0 ||
        selectedGame >= static_cast<int>(games.size()))
        return;
    if (!games[selectedGame].valid || !loadGame(games[selectedGame]))
    {
        games[selectedGame].valid = false;
        drawGames();
        showHdrMsg(configError.length() ? configError.c_str() : "Invalid macro JSON");
        return;
    }
    view = View::Devices;
    selectedDevice = 0;
    deviceScrollTop = 0;
    drawDevices();
}

void startSelectedDevice()
{
    const int bondIndex = selectedDevice < BluetoothService::bondCount()
        ? selectedDevice : -1;
    if (BluetoothService::startKeyboardSession(bondIndex))
    {
        view = View::Macros;
        drawConnectionOverlay();
    }
}

void disconnect()
{
    if (macroPhase != MacroPhase::Idle)
        releaseMacro();
    BluetoothService::stopKeyboardSession();
    BluetoothService::takeUiDirty();
    editorMode = EditorMode::None;
    editorError = "";
    view = View::Devices;
    drawDevices();
}

bool handleEditorInput(Keyboard_Class::KeysState &keys)
{
    if (keyboardBackPressed(keys))
    {
        closeEditor();
        return true;
    }

    if (editorMode == EditorMode::Catalog)
    {
        const std::vector<int> available = availableCatalogIndexes();
        if (keys.enter && !available.empty())
        {
            selectedCatalog = constrain(selectedCatalog, 0,
                                        static_cast<int>(available.size()) - 1);
            beginHotkeyEdit(-1, available[selectedCatalog]);
            return true;
        }
        for (char c : keys.word)
        {
            if (c == ';' || c == '.')
            {
                moveListSelection(selectedCatalog, catalogScrollTop,
                                  available.size(), c == ';' ? -1 : +1,
                                  drawCatalogEditor);
                return true;
            }
        }
        return true;
    }

    if (editorMode == EditorMode::Hotkey)
    {
        for (char c : keys.word)
        {
            const char hotkey = normalizeHotkey(String(c));
            if (hotkey)
            {
                applyHotkey(hotkey);
                return true;
            }
        }
        return true;
    }

    if (editorMode == EditorMode::RemoveConfirm)
    {
        if (keys.enter)
            removeSelectedBinding();
        return true;
    }
    return false;
}
} // namespace

void openMacroPadApp()
{
    BluetoothService::begin();
    if (BluetoothService::keyboardSessionActive())
        BluetoothService::stopKeyboardSession();
    activeMacro = -1;
    macroPhase = MacroPhase::Idle;
    editorMode = EditorMode::None;
    editorError = "";
    view = View::Games;
    marqueeStartMs = millis();
    scanGames();
    drawGames();
}

void drawMacroPadApp()
{
    if (BluetoothService::keyboardSessionActive())
    {
        if (BluetoothService::keyboardReady())
            drawMacroPadSurface();
        else
            drawConnectionOverlay();
        return;
    }
    if (view == View::Games)
        drawGames();
    else
        drawDevices();
}

bool handleMacroPadAppInput(Keyboard_Class::KeysState &keys)
{
    if (BluetoothService::keyboardSessionActive())
    {
        if (!BluetoothService::keyboardReady())
            return true;
        if (editorMode != EditorMode::None)
            return handleEditorInput(keys);
        if (macroPhase == MacroPhase::Idle &&
            keyboardOptionsShortcutPressed(keys))
        {
            enterOptionsMenu();
            return true;
        }
        if (keys.enter)
        {
            startMacro(selectedMacro);
            return true;
        }
        for (char c : keys.word)
        {
            char hotkey = c;
            if (hotkey >= 'A' && hotkey <= 'Z')
                hotkey = hotkey - 'A' + 'a';
            for (int i = 0; i < static_cast<int>(macros.size()); ++i)
            {
                if (macros[i].hotkey == hotkey)
                {
                    startMacro(i);
                    return true;
                }
            }
            if (c == ';')
            {
                moveListSelection(selectedMacro, macroScrollTop, macros.size(),
                                  -1, drawMacros);
                return true;
            }
            if (c == '.')
            {
                moveListSelection(selectedMacro, macroScrollTop, macros.size(),
                                  +1, drawMacros);
                return true;
            }
        }
        return true;
    }

    if (keys.del && view == View::Devices)
    {
        view = View::Games;
        drawGames();
        return true;
    }
    if (keys.enter)
    {
        if (view == View::Games)
            openSelectedGame();
        else
            startSelectedDevice();
        return true;
    }
    for (char c : keys.word)
    {
        const int count = view == View::Games
            ? games.size() : deviceItemCount();
        const int scrollTop = view == View::Games
            ? gameScrollTop : deviceScrollTop;
        const int target = listVisibleShortcutTarget(c, scrollTop, count);
        if (target >= 0)
        {
            if (view == View::Games)
            {
                selectedGame = target;
                openSelectedGame();
            }
            else
            {
                selectedDevice = target;
                startSelectedDevice();
            }
            return true;
        }
        if ((c == 'r' || c == 'R') && view == View::Games)
        {
            scanGames();
            drawGames();
            return true;
        }
        if (c == 'h' || c == 'H')
        {
            toggleHelp();
            return true;
        }
        if (c == ';' || c == '.')
        {
            const int direction = c == ';' ? -1 : +1;
            if (view == View::Games)
            {
                moveListSelection(selectedGame, gameScrollTop, games.size(),
                                  direction, drawGames);
            }
            else
            {
                moveListSelection(selectedDevice, deviceScrollTop,
                                  deviceItemCount(), direction, drawDevices);
            }
            return true;
        }
    }
    return true;
}

void tickMacroPadApp()
{
    if (!BluetoothService::keyboardSessionActive())
    {
        if (BluetoothService::takeUiDirty() && view == View::Devices)
            drawDevices();
        return;
    }
    if (M5Cardputer.BtnA.wasPressed())
    {
        disconnect();
        return;
    }
    if (BluetoothService::takeUiDirty() && !optionsMenuVisible)
        drawMacroPadApp();
    if (!BluetoothService::keyboardReady() || macroPhase == MacroPhase::Idle ||
        static_cast<int32_t>(millis() - nextReportMs) < 0)
        return;

    MacroDefinition &macro = macros[activeMacro];
    switch (macroPhase)
    {
    case MacroPhase::ActivationDown:
        if (macro.activationMouseButton > 0)
            BluetoothService::sendMouseButtons(0);
        if (macro.activationModifier > 0)
            sendMacroReport(0, 0);
        macroPhase = MacroPhase::ActivationGap;
        nextReportMs = millis() + macro.gapMs;
        break;
    case MacroPhase::ActivationGap:
        startFirstSequenceKey();
        break;
    case MacroPhase::KeyDown:
        sendMacroReport(
            macro.modifiers |
                (macro.holdActivation ? macro.activationModifier : 0),
            0);
        macroPhase = activeStep + 1 >= macro.sequence.size()
            ? MacroPhase::FinalGap : MacroPhase::KeyGap;
        nextReportMs = millis() + macro.gapMs;
        break;
    case MacroPhase::KeyGap:
        ++activeStep;
        sendMacroReport(macro.modifiers, macro.sequence[activeStep]);
        macroPhase = MacroPhase::KeyDown;
        nextReportMs = millis() + macro.pressMs;
        break;
    case MacroPhase::FinalGap:
        releaseMacro();
        drawMacros();
        break;
    case MacroPhase::Idle:
        break;
    }
}

bool macroPadBindingsEditable()
{
    return currentGameVersion == 2 &&
           BluetoothService::keyboardSessionActive() &&
           BluetoothService::keyboardReady() &&
           macroPhase == MacroPhase::Idle;
}

bool macroPadCanAddBinding()
{
    return macroPadBindingsEditable() && macros.size() < MAX_MACROS &&
           !availableCatalogIndexes().empty();
}

bool macroPadHasSelectedBinding()
{
    return macroPadBindingsEditable() && selectedMacro >= 0 &&
           selectedMacro < static_cast<int>(macros.size());
}

bool macroPadCanMoveSelectedBinding(int direction)
{
    if (!macroPadHasSelectedBinding())
        return false;
    const int destination = selectedMacro + (direction < 0 ? -1 : 1);
    return destination >= 0 && destination < static_cast<int>(macros.size());
}

void macroPadRequestAddBinding()
{
    beginAddMacro();
}

void macroPadRequestChangeKey()
{
    if (macroPadHasSelectedBinding())
        beginHotkeyEdit(selectedMacro, -1);
}

void macroPadRequestDeleteBinding()
{
    if (!macroPadHasSelectedBinding())
        return;
    editorMode = EditorMode::RemoveConfirm;
    editorError = "";
    drawMacroPadSurface();
}

void macroPadMoveSelectedBinding(int direction)
{
    if (!macroPadCanMoveSelectedBinding(direction))
        return;
    const int destination = selectedMacro + (direction < 0 ? -1 : 1);
    std::swap(macros[selectedMacro], macros[destination]);
    String error;
    if (!saveBindings(error))
    {
        std::swap(macros[selectedMacro], macros[destination]);
        drawMacroPadSurface();
        showHdrMsg(error.c_str());
        return;
    }
    selectedMacro = destination;
    if (selectedMacro < macroScrollTop)
        macroScrollTop = selectedMacro;
    else if (selectedMacro >= macroScrollTop + LIST_VISIBLE_ITEM)
        macroScrollTop = selectedMacro - LIST_VISIBLE_ITEM + 1;
    drawMacroPadSurface();
    showHdrMsg(direction < 0 ? "Binding moved up" : "Binding moved down");
}

int currentStarterInputIndex()
{
    if (currentDefaultActivationModifier == 0x01 &&
        currentDefaultHoldActivation)
        return 1;
    if (currentDefaultActivationButton >= 1 &&
        currentDefaultActivationButton <= 5 &&
        !currentDefaultHoldActivation)
        return currentDefaultActivationButton + 1;
    if (currentDefaultActivationModifier == 0x10 &&
        currentDefaultHoldActivation)
        return 7;
    if (currentDefaultActivationButton == 0 &&
        currentDefaultActivationModifier == 0)
        return 0;
    return -1;
}

String macroPadStarterInputLabel()
{
    switch (currentStarterInputIndex())
    {
    case 0: return "None";
    case 1: return "Ctrl Hold";
    case 2: return "Mouse 1 Click";
    case 3: return "Mouse 2 Click";
    case 4: return "Mouse 3 Click";
    case 5: return "Mouse 4 Click";
    case 6: return "Mouse 5 Click";
    case 7: return "Right Ctrl Hold";
    default: return "Custom JSON";
    }
}

void macroPadAdjustStarterInput(int direction)
{
    if (!macroPadBindingsEditable())
        return;
    int index = currentStarterInputIndex();
    if (index < 0)
        index = 0;
    index = (index + (direction < 0 ? -1 : 1) + 8) % 8;

    JsonDocument document;
    String error;
    if (!openJson(currentGamePath, document, error) ||
        (document["version"] | 0) != 2)
    {
        showHdrMsg(error.length() ? error.c_str() : "Invalid macro JSON");
        return;
    }
    if (index == 0)
    {
        document.remove("activation");
    }
    else
    {
        JsonObject activation = document["activation"].to<JsonObject>();
        if (index == 1 || index == 7)
        {
            activation["key"] = index == 1 ? "CTRL" : "RIGHT_CTRL";
            activation["mode"] = "hold";
        }
        else
        {
            activation["mouse_button"] = index - 1;
            activation["mode"] = "press";
        }
    }
    if (!writeCurrentGameJson(document, error))
    {
        showHdrMsg(error.c_str());
        return;
    }

    const int previousSelection = selectedMacro;
    GameFile game;
    game.name = currentGameName;
    game.path = currentGamePath;
    game.valid = true;
    if (!loadGame(game))
    {
        showHdrMsg(configError.c_str());
        return;
    }
    selectedMacro = macros.empty()
        ? 0 : constrain(previousSelection, 0, static_cast<int>(macros.size()) - 1);
}

bool macroPadModalActive()
{
    return foregroundApp == HostApp::MacroPad &&
           BluetoothService::keyboardSessionActive() &&
           !optionsMenuVisible;
}
