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
    "UP", "RIGHT", "DOWN", "LEFT", "PAUSE", "RETRY", "HOME"
};
const char* kJsonKeys[kGameActionCount] = {
    "up", "right", "down", "left", "pause", "retry", "home"
};

bool AllowedKey(int key) noexcept {
    // Keep Escape reserved for backing out of the interface, F11 for window
    // mode and Enter for activating menu options. Exclude system modifiers.
    if (key < KEY_SPACE || key > KEY_KB_MENU ||
        key == KEY_ESCAPE || key == KEY_ENTER || key == KEY_TAB ||
        key == KEY_BACKSPACE || key == KEY_F11 ||
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

unsigned EditorDirectionMask(const std::array<bool, 4>& gameplayHeld) noexcept {
    return (gameplayHeld[static_cast<int>(GameAction::Left)] ? 1u : 0u) |
           (gameplayHeld[static_cast<int>(GameAction::Down)] ? 2u : 0u) |
           (gameplayHeld[static_cast<int>(GameAction::Up)] ? 4u : 0u) |
           (gameplayHeld[static_cast<int>(GameAction::Right)] ? 8u : 0u);
}

bool IsGameDirectionKey(const GameSettings& settings, int key) noexcept {
    return key == settings.Key(GameAction::Left) ||
           key == settings.Key(GameAction::Down) ||
           key == settings.Key(GameAction::Up) ||
           key == settings.Key(GameAction::Right);
}

std::string EditorDirectionShortcut(const GameSettings& settings, unsigned mask) {
    // Same chord order as the visual compass pad; no hard-coded D/F/J/K.
    switch (mask) {
        case 1u: return KeyboardKeyName(settings.Key(GameAction::Left));
        case 2u: return KeyboardKeyName(settings.Key(GameAction::Down));
        case 4u: return KeyboardKeyName(settings.Key(GameAction::Up));
        case 8u: return KeyboardKeyName(settings.Key(GameAction::Right));
        case 5u: return EditorDirectionShortcut(settings, 1u) + "+" +
                        EditorDirectionShortcut(settings, 4u); // up-left
        case 12u: return EditorDirectionShortcut(settings, 4u) + "+" +
                         EditorDirectionShortcut(settings, 8u); // up-right
        case 3u: return EditorDirectionShortcut(settings, 1u) + "+" +
                        EditorDirectionShortcut(settings, 2u); // down-left
        case 10u: return EditorDirectionShortcut(settings, 2u) + "+" +
                         EditorDirectionShortcut(settings, 8u); // down-right
        default: return "";
    }
}

bool GameSettings::Valid(std::string& error) const {
    if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
        error = "Music volume must be between 0 and 100 percent.";
        return false;
    }
    if (chordWindowMs < 40 || chordWindowMs > 160) {
        error = "Chord window must be 40..160 milliseconds.";
        return false;
    }
    for (int i = 0; i < kGameActionCount; ++i) {
        if (!AllowedKey(keys[i])) {
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

std::string SerializeGameSettings(const GameSettings& settings) {
    using Json = nlohmann::ordered_json;
    Json bindings = Json::object();
    for (int i = 0; i < kGameActionCount; ++i)
        bindings[kJsonKeys[i]] = settings.keys[i];
    Json object = {
        {"version", 2},
        {"volume", settings.volume},
        {"hitFlash", settings.hitFlash},
        {"chordWindowMs", settings.chordWindowMs},
        {"fullscreen", settings.fullscreen},
        {"bindings", std::move(bindings)}
    };
    return object.dump(2) + "\n";
}

bool ParseGameSettings(std::string_view text, GameSettings& result,
                       std::string& error) {
    try {
        const auto object = nlohmann::json::parse(text.begin(), text.end());
        if (!object.is_object() || !object.contains("version") ||
            !object["version"].is_number_integer() ||
            (object["version"].get<int>() != 1 &&
             object["version"].get<int>() != 2)) {
            error = "Unsupported settings version.";
            return false;
        }
        GameSettings parsed;
        if (!object.contains("volume") || !object["volume"].is_number() ||
            !object.contains("chordWindowMs") ||
            !object["chordWindowMs"].is_number_integer() ||
            !object.contains("hitFlash") || !object["hitFlash"].is_boolean() ||
            !object.contains("fullscreen") || !object["fullscreen"].is_boolean() ||
            !object.contains("bindings") || !object["bindings"].is_object()) {
            error = "Settings JSON is missing required fields or contains wrong types.";
            return false;
        }
        parsed.volume = object["volume"].get<float>();
        parsed.chordWindowMs = object["chordWindowMs"].get<int>();
        parsed.hitFlash = object["hitFlash"].get<bool>();
        parsed.fullscreen = object["fullscreen"].get<bool>();
        for (int i = 0; i < kGameActionCount; ++i) {
            const auto& bindings = object["bindings"];
            if (!bindings.contains(kJsonKeys[i]) ||
                !bindings[kJsonKeys[i]].is_number_integer()) {
                error = "Missing or invalid binding for " + std::string(kNames[i]);
                return false;
            }
            parsed.keys[i] = bindings[kJsonKeys[i]].get<int>();
        }
        if (!parsed.Valid(error)) return false;
        // Old v1 files commonly contain the prior default arrow bindings.
        // Migrate only that exact untouched directional preset; customized
        // old layouts remain untouched. V2 supports intentionally remapping
        // back to arrows without any automatic migration on next launch.
        if (object["version"].get<int>() == 1 &&
            parsed.Key(GameAction::Up) == KEY_UP &&
            parsed.Key(GameAction::Right) == KEY_RIGHT &&
            parsed.Key(GameAction::Down) == KEY_DOWN &&
            parsed.Key(GameAction::Left) == KEY_LEFT) {
            parsed.keys[static_cast<int>(GameAction::Up)] = KEY_J;
            parsed.keys[static_cast<int>(GameAction::Right)] = KEY_K;
            parsed.keys[static_cast<int>(GameAction::Down)] = KEY_F;
            parsed.keys[static_cast<int>(GameAction::Left)] = KEY_D;
        }
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
