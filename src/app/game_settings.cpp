#include "app/game_settings.h"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// Win32 names collide with raylib's CloseWindow/ShowCursor declarations.
// Only the filesystem replacement API is used here.
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace locus::app {
namespace {
const char* kNames[kGameActionCount] = {
    "UP", "RIGHT", "DOWN", "LEFT", "PAUSE / MENU", "RETRY", "LEAVE LEVEL",
    "PRACTICE 100%", "PRACTICE 75%", "PRACTICE 50%", "PRACTICE 25%",
    "PRACTICE 10%", "SPEED +", "SPEED -", "PREVIEW +", "PREVIEW -"
};
const char* kJsonKeys[kGameActionCount] = {
    "up", "right", "down", "left", "pause", "retry", "home",
    "practice100", "practice75", "practice50", "practice25", "practice10",
    "speedUp", "speedDown", "previewUp", "previewDown"
};
const char* kShortcutNames[kShortcutActionCount] = {
    "BACK / CANCEL", "SONG LIBRARY", "OPEN EDITOR", "OPEN SKIN",
    "OPEN SETTINGS", "FULLSCREEN", "RESCAN SONGS", "CHOOSE MUSIC",
    "CHOOSE CHART", "LISTEN PAUSE", "LISTEN REPLAY",
    "SAVE CHART", "SAVE CHART AS", "OPEN CHART", "NEW CHART",
    "TEST PLAY", "PREVIEW PAUSE", "PREVIEW RESTART",
    "COPY", "CUT", "PASTE", "UNDO", "REDO",
    "MIRROR H", "MIRROR V",
    "ZOOM IN", "ZOOM OUT", "GRID FINER", "GRID COARSER"
};
const char* kShortcutJson[kShortcutActionCount] = {
    "back", "openLibrary", "openEditor", "openSkin", "openSettings",
    "fullscreen", "refreshLibrary", "chooseMusic", "chooseChart",
    "listenPause", "listenReplay",
    "editorSave", "editorSaveAs", "editorOpen", "editorNew", "editorTest",
    "editorPreview", "editorRestart", "editorCopy", "editorCut", "editorPaste",
    "editorUndo", "editorRedo", "editorMirrorH", "editorMirrorV",
    "editorZoomIn", "editorZoomOut", "editorSnapMore", "editorSnapLess"
};

bool AllowedKey(int key, bool escape = false, bool fullscreen = false) noexcept {
    // ESC is the default PAUSE key in PLAY and BACK elsewhere. F11 belongs
    // to the configurable fullscreen action; modifiers cannot be primary keys.
    if (key < KEY_SPACE || key > KEY_KB_MENU ||
        (key == KEY_ESCAPE && !escape) ||
        (key == KEY_F11 && !fullscreen) ||
        key == KEY_ENTER || key == KEY_TAB || key == KEY_BACKSPACE ||
        (key >= KEY_F1 && key <= KEY_F4) ||
        (key >= KEY_LEFT_SHIFT && key <= KEY_RIGHT_SUPER))
        return false;
    return true;
}
} // namespace

const char* ActionName(GameAction action) noexcept {
    const int index = static_cast<int>(action);
    return (index >= 0 && index < kGameActionCount) ? kNames[index] : "UNKNOWN";
}

const char* ShortcutName(ShortcutAction action) noexcept {
    const int index = static_cast<int>(action);
    return index >= 0 && index < kShortcutActionCount
        ? kShortcutNames[index] : "UNKNOWN";
}

std::string KeyboardChordName(KeyChord chord) {
    return std::string(chord.ctrl ? "CTRL+" : "") +
           (chord.shift ? "SHIFT+" : "") + KeyboardKeyName(chord.key);
}

std::string KeyboardKeyName(int key) {
    if (key >= KEY_A && key <= KEY_Z)
        return std::string(1, static_cast<char>('A' + key - KEY_A));
    if (key >= KEY_ZERO && key <= KEY_NINE)
        return std::string(1, static_cast<char>('0' + key - KEY_ZERO));
    if (key >= KEY_F5 && key <= KEY_F12)
        return "F" + std::to_string(5 + key - KEY_F5);
    if (key >= KEY_KP_0 && key <= KEY_KP_9)
        return "NUM" + std::to_string(key - KEY_KP_0);
    switch (key) {
        case KEY_UP: return "UP";
        case KEY_RIGHT: return "RIGHT";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT";
        case KEY_SPACE: return "SPACE";
        case KEY_ESCAPE: return "ESC";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_RIGHT_BRACKET: return "]";
        case KEY_SEMICOLON: return ";";
        case KEY_APOSTROPHE: return "'";
        case KEY_COMMA: return ",";
        case KEY_PERIOD: return ".";
        case KEY_SLASH: return "/";
        case KEY_BACKSLASH: return "\\";
        case KEY_MINUS: return "-";
        case KEY_EQUAL: return "=";
        case KEY_PAGE_UP: return "PGUP";
        case KEY_PAGE_DOWN: return "PGDN";
        case KEY_HOME: return "HOME";
        case KEY_END: return "END";
        case KEY_INSERT: return "INSERT";
        case KEY_DELETE: return "DELETE";
        case KEY_KP_ADD: return "NUM+";
        case KEY_KP_SUBTRACT: return "NUM-";
        case KEY_KP_MULTIPLY: return "NUM*";
        case KEY_KP_DIVIDE: return "NUM/";
        default: return "KEY " + std::to_string(key);
    }
}

bool IsGameDirectionKey(const GameSettings& settings, int key) noexcept {
    return key == settings.Key(GameAction::Left) ||
           key == settings.Key(GameAction::Down) ||
           key == settings.Key(GameAction::Up) ||
           key == settings.Key(GameAction::Right);
}

bool GameSettings::ValidFpsLimit(int value) noexcept {
    return std::find(kFpsPresets.begin(), kFpsPresets.end(), value) !=
           kFpsPresets.end();
}

void GameSettings::StepFpsLimit(int direction) noexcept {
    const auto it = std::find(kFpsPresets.begin(), kFpsPresets.end(), fpsLimit);
    if (it == kFpsPresets.end()) return;
    const int current = static_cast<int>(it - kFpsPresets.begin());
    const int next = std::clamp(current + direction, 0,
                              static_cast<int>(kFpsPresets.size()) - 1);
    fpsLimit = kFpsPresets[static_cast<std::size_t>(next)];
}

std::string GameSettings::FpsLimitLabel() const {
    return fpsLimit == 0 ? "UNLIMITED" : std::to_string(fpsLimit) + " FPS";
}

bool GameSettings::StepEditorPreviewSpeed(int direction) noexcept {
    if (direction == 0) return false;
    const float next = std::clamp(
        editorPreviewSpeed + (direction > 0 ? 25.0f : -25.0f),
        kEditorSpeedMin, kEditorSpeedMax);
    if (next == editorPreviewSpeed) return false;
    editorPreviewSpeed = next;
    return true;
}

bool GameSettings::StepEditorPreviewLength(int direction) noexcept {
    if (direction == 0) return false;
    const float next = std::clamp(
        editorPreviewLengthMs + (direction > 0 ? 100.0f : -100.0f),
        kEditorLengthMinMs, kEditorLengthMaxMs);
    if (next == editorPreviewLengthMs) return false;
    editorPreviewLengthMs = next;
    return true;
}

bool GameSettings::Valid(std::string& error) const {
    if (globalOffsetMs < -kGlobalOffsetLimitMs ||
        globalOffsetMs > kGlobalOffsetLimitMs) {
        error = "Global audio offset must be between -1000 and +1000 ms.";
        return false;
    }
    if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
        error = "Music volume must be between 0 and 100 percent.";
        return false;
    }
    if (!ValidFpsLimit(fpsLimit)) {
        error = "FPS limit must be one of the available display presets.";
        return false;
    }
    if (!std::isfinite(editorPreviewSpeed) ||
        editorPreviewSpeed < kEditorSpeedMin ||
        editorPreviewSpeed > kEditorSpeedMax ||
        !std::isfinite(editorPreviewLengthMs) ||
        editorPreviewLengthMs < kEditorLengthMinMs ||
        editorPreviewLengthMs > kEditorLengthMaxMs) {
        error = "Editor preview speed or path length is outside allowed limits.";
        return false;
    }
    for (int i = 0; i < kGameActionCount; ++i) {
        if (!AllowedKey(keys[i], i == static_cast<int>(GameAction::Pause))) {
            error = std::string(ActionName(static_cast<GameAction>(i))) +
                    ": key is reserved or unsupported.";
            return false;
        }
        for (int j = 0; j < i; ++j) {
            if (keys[i] == keys[j]) {
                error = std::string(ActionName(static_cast<GameAction>(i))) +
                        ": duplicate key bindings are not allowed.";
                return false;
            }
        }
    }
    for (int i = 0; i < kShortcutActionCount; ++i) {
        const auto value = shortcuts[i];
        if (!AllowedKey(value.key,
                        i == static_cast<int>(ShortcutAction::Back),
                        i == static_cast<int>(ShortcutAction::Fullscreen))) {
            error = std::string(ShortcutName(static_cast<ShortcutAction>(i))) +
                    ": unsupported or reserved shortcut.";
            return false;
        }
        // Duplicates are forbidden only within the same focused context.
        // Menu/Listen and Editor shortcuts intentionally share many keys.
        for (int j = (i < kNavigationShortcutCount ? 0 : kNavigationShortcutCount);
             j < i; ++j)
            if (shortcuts[i] == shortcuts[j]) {
                error = std::string(ShortcutName(static_cast<ShortcutAction>(i))) +
                        ": duplicate shortcut in this section.";
                return false;
            }
    }
    error.clear();
    return true;
}

bool GameSettings::Rebind(GameAction action, int key, std::string& error) {
    const int index = static_cast<int>(action);
    if (index < 0 || index >= kGameActionCount) {
        error = "Unknown gameplay action.";
        return false;
    }
    const int previous = keys[index];
    keys[index] = key;
    if (Valid(error)) return true;
    keys[index] = previous;
    return false;
}

bool GameSettings::RebindShortcut(ShortcutAction action, KeyChord chord,
                                  std::string& error) {
    const int index = static_cast<int>(action);
    if (index < 0 || index >= kShortcutActionCount) {
        error = "Unknown keyboard shortcut.";
        return false;
    }
    const auto previous = shortcuts[index];
    shortcuts[index] = chord;
    if (Valid(error)) return true;
    shortcuts[index] = previous;
    return false;
}

std::string SerializeGameSettings(const GameSettings& settings) {
    using Json = nlohmann::ordered_json;
    Json bindings = Json::object();
    for (int i = 0; i < kGameActionCount; ++i)
        bindings[kJsonKeys[i]] = settings.keys[i];
    Json shortcuts = Json::object();
    for (int i = 0; i < kShortcutActionCount; ++i) {
        const auto chord = settings.shortcuts[i];
        shortcuts[kShortcutJson[i]] = {
            {"key", chord.key}, {"ctrl", chord.ctrl}, {"shift", chord.shift}
        };
    }
    Json object = {
        {"version", 3},
        {"globalOffsetMs", settings.globalOffsetMs},
        {"volume", settings.volume},
        {"hitFlash", settings.hitFlash},
        {"fullscreen", settings.fullscreen},
        {"vsync", settings.vsync},
        {"fpsLimit", settings.fpsLimit},
        {"showFps", settings.showFps},
        {"editorPreviewSpeed", settings.editorPreviewSpeed},
        {"editorPreviewLengthMs", settings.editorPreviewLengthMs},
        {"bindings", std::move(bindings)},
        {"shortcuts", std::move(shortcuts)}
    };
    return object.dump(2) + "\n";
}

bool ParseGameSettings(std::string_view text, GameSettings& result,
                       std::string& error) {
    try {
        const auto object = nlohmann::json::parse(text.begin(), text.end());
        if (!object.is_object() || !object.contains("version") ||
            !object["version"].is_number_integer() ||
            object["version"].get<int>() != 3) {
            error = "Unsupported settings version.";
            return false;
        }
        GameSettings parsed;
        if (!object.contains("volume") || !object["volume"].is_number() ||
            !object.contains("hitFlash") || !object["hitFlash"].is_boolean() ||
            !object.contains("fullscreen") || !object["fullscreen"].is_boolean() ||
            !object.contains("bindings") || !object["bindings"].is_object()) {
            error = "Settings JSON is missing required fields or contains wrong types.";
            return false;
        }
        // Keep older v3 settings and custom key bindings valid; the global
        // offset is an optional additive preference, not a format reset.
        if (object.contains("globalOffsetMs")) {
            if (!object["globalOffsetMs"].is_number_integer()) {
                error = "Global audio offset must be an integer number of ms.";
                return false;
            }
            parsed.globalOffsetMs = object["globalOffsetMs"].get<int>();
        }
        parsed.volume = object["volume"].get<float>();
        parsed.hitFlash = object["hitFlash"].get<bool>();
        parsed.fullscreen = object["fullscreen"].get<bool>();
        for (const char* key : {"vsync", "fpsLimit", "showFps"}) {
            if (!object.contains(key)) {
                error = std::string("Missing display setting: ") + key;
                return false;
            }
            const auto& value = object[key];
            const bool validType = std::string_view(key) == "fpsLimit"
                ? value.is_number_integer() : value.is_boolean();
            if (!validType) {
                error = std::string("Invalid display setting: ") + key;
                return false;
            }
        }
        parsed.vsync = object["vsync"].get<bool>();
        parsed.fpsLimit = object["fpsLimit"].get<int>();
        parsed.showFps = object["showFps"].get<bool>();
        // Existing v3 settings files have no editor-only controls yet.
        // Preserve every existing binding and default just these new values.
        for (const char* key : {"editorPreviewSpeed", "editorPreviewLengthMs"}) {
            if (object.contains(key) && !object[key].is_number()) {
                error = std::string("Invalid editor preview setting: ") + key;
                return false;
            }
        }
        if (object.contains("editorPreviewSpeed"))
            parsed.editorPreviewSpeed = object["editorPreviewSpeed"].get<float>();
        if (object.contains("editorPreviewLengthMs"))
            parsed.editorPreviewLengthMs =
                object["editorPreviewLengthMs"].get<float>();
        for (int i = 0; i < kGameActionCount; ++i) {
            const auto& bindings = object["bindings"];
            if (!bindings.contains(kJsonKeys[i]) ||
                !bindings[kJsonKeys[i]].is_number_integer()) {
                error = "Missing or invalid binding for " + std::string(kNames[i]);
                return false;
            }
            parsed.keys[i] = bindings[kJsonKeys[i]].get<int>();
        }
        {
            if (!object.contains("shortcuts") ||
                !object["shortcuts"].is_object()) {
                error = "Settings JSON is missing shortcut bindings.";
                return false;
            }
            for (int i = 0; i < kShortcutActionCount; ++i) {
                const auto& shortcuts = object["shortcuts"];
                const char* key = kShortcutJson[i];
                if (!shortcuts.contains(key) || !shortcuts[key].is_object() ||
                    !shortcuts[key].contains("key") ||
                    !shortcuts[key]["key"].is_number_integer() ||
                    !shortcuts[key].contains("ctrl") ||
                    !shortcuts[key]["ctrl"].is_boolean() ||
                    !shortcuts[key].contains("shift") ||
                    !shortcuts[key]["shift"].is_boolean()) {
                    error = "Missing or invalid keyboard shortcut: " +
                            std::string(key);
                    return false;
                }
                parsed.shortcuts[i] = {
                    shortcuts[key]["key"].get<int>(),
                    shortcuts[key]["ctrl"].get<bool>(),
                    shortcuts[key]["shift"].get<bool>()
                };
            }
        }
        if (!parsed.Valid(error)) return false;
        result = parsed;
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = std::string("Invalid settings JSON: ") + e.what();
        return false;
    }
}

bool LoadGameSettings(const std::filesystem::path& file, GameSettings& result,
                      std::string& error) {
    std::error_code ec;
    if (!std::filesystem::exists(file, ec) && !ec) {
        result = GameSettings{};
        error.clear();
        return true;
    }
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        error = "Cannot read settings.json.";
        return false;
    }
    const std::string bytes((std::istreambuf_iterator<char>(stream)),
                             std::istreambuf_iterator<char>());
    if (bytes.size() > 32768) {
        error = "settings.json is too large.";
        return false;
    }
    return ParseGameSettings(bytes, result, error);
}

bool SaveGameSettings(const std::filesystem::path& file,
                      const GameSettings& settings, std::string& error) {
    if (!settings.Valid(error)) return false;
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) {
        error = "Cannot create settings folder: " + ec.message();
        return false;
    }
    const auto temporary = std::filesystem::path(file.wstring() + L".tmp");
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            error = "Cannot create settings temporary file.";
            return false;
        }
        stream << SerializeGameSettings(settings);
        stream.flush();
        if (!stream) {
            error = "Cannot finish writing settings.";
            return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), file.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, ec);
        error = "Cannot replace settings.json.";
        return false;
    }
#else
    std::filesystem::rename(temporary, file, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        error = "Cannot replace settings.json.";
        return false;
    }
#endif
    error.clear();
    return true;
}

} // namespace locus::app
