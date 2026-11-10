#include "app/game_settings.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using namespace locus::app;

namespace {
bool Check(bool ok, const char* label) {
    if (!ok) std::cerr << "SETTINGS_FAIL " << label << '\n';
    return ok;
}
}

int main() {
    bool ok = true;
    GameSettings options;
    std::string error;
    ok &= Check(options.Valid(error) &&
                options.Key(GameAction::Up) == KEY_J &&
                options.Key(GameAction::Right) == KEY_K &&
                options.Key(GameAction::Down) == KEY_F &&
                options.Key(GameAction::Left) == KEY_D &&
                options.Key(GameAction::Pause) == KEY_ESCAPE &&
                options.Shortcut(ShortcutAction::EditorSave).key == KEY_S &&
                options.Shortcut(ShortcutAction::EditorSave).ctrl &&
                options.vsync && options.fpsLimit == 240 && !options.showFps,
                "default play pause is ESC and editor actions are configurable");
    ok &= Check(IsGameDirectionKey(options, KEY_D) &&
                !IsGameDirectionKey(options, KEY_LEFT),
                "direction bindings belong to scored gameplay only");
    ok &= Check(options.Rebind(GameAction::Left, KEY_A, error) &&
                options.Rebind(GameAction::Up, KEY_W, error) &&
                options.Rebind(GameAction::Right, KEY_D, error) &&
                options.Key(GameAction::Up) == KEY_W,
                "gameplay directions can use WASD or other keyboard keys");
    ok &= Check(IsGameDirectionKey(options, KEY_W) &&
                IsGameDirectionKey(options, KEY_D),
                "rebound gameplay keys apply immediately");
    ok &= Check(!options.Rebind(GameAction::Left, KEY_W, error) &&
                options.Key(GameAction::Left) == KEY_A,
                "duplicate binding is rejected without changing old key");
    ok &= Check(options.Rebind(GameAction::Pause, KEY_SPACE, error) &&
                options.Rebind(GameAction::Pause, KEY_ESCAPE, error) &&
                !options.Rebind(GameAction::Retry, KEY_F11, error) &&
                !options.Rebind(GameAction::Up, KEY_ESCAPE, error) &&
                options.Key(GameAction::Pause) == KEY_ESCAPE,
                "ESC can bind to pause only, F11 remains fullscreen-specific");
    ok &= Check(options.RebindShortcut(
                    ShortcutAction::EditorSave, {KEY_B, true}, error) &&
                options.RebindShortcut(
                    ShortcutAction::EditorSaveAs, {KEY_B, true, true}, error) &&
                !options.RebindShortcut(
                    ShortcutAction::EditorOpen, {KEY_B, true}, error) &&
                options.Shortcut(ShortcutAction::EditorOpen).key == KEY_O &&
                KeyboardChordName(options.Shortcut(ShortcutAction::EditorSave)) ==
                    "CTRL+B",
                "editor hotkeys include modifiers, reject duplicate chords");

    options.volume = 0.4f;
    options.hitFlash = false;
    options.fullscreen = true;
    options.vsync = false;
    options.fpsLimit = 144;
    options.showFps = true;
    const std::string serialized = SerializeGameSettings(options);
    GameSettings parsed;
    ok &= Check(ParseGameSettings(serialized, parsed, error) &&
                parsed.Key(GameAction::Up) == KEY_W &&
                parsed.Key(GameAction::Right) == KEY_D &&
                parsed.volume > 0.39f && parsed.volume < 0.41f &&
                !parsed.hitFlash && parsed.fullscreen &&
                !parsed.vsync && parsed.fpsLimit == 144 && parsed.showFps,
                parsed.Shortcut(ShortcutAction::EditorSave).key == KEY_B &&
                parsed.Shortcut(ShortcutAction::EditorSave).ctrl &&
                !parsed.Shortcut(ShortcutAction::EditorSave).shift &&
                !parsed.vsync && parsed.fpsLimit == 144 && parsed.showFps,
                "all audio, visuals, play keys and editor shortcuts survive JSON");
    ok &= Check(parsed.Key(GameAction::Up) == KEY_W,
                "persisted gameplay direction keys survive restarting");
    GameSettings display = parsed;
    display.StepFpsLimit(1);
    ok &= Check(display.fpsLimit == 165 &&
                display.Valid(error) &&
                display.FpsLimitLabel() == "165 FPS",
                "FPS preset moves forward and keeps a valid cap");
    display.StepFpsLimit(100);
    ok &= Check(display.fpsLimit == 0 &&
                display.FpsLimitLabel() == "UNLIMITED" &&
                display.Valid(error),
                "unlimited FPS can be selected as the last preset");
    display.StepFpsLimit(-100);
    ok &= Check(display.fpsLimit == 30,
                "FPS presets safely clamp at 30");
    display.fpsLimit = 1000;
    ok &= Check(!display.Valid(error),
                "unsupported FPS limit is rejected");
    std::string legacyDisplay = serialized;
    const auto removeField = [&](std::string field) {
        const auto start = legacyDisplay.find("  \"" + field + "\":");
        if (start != std::string::npos) {
            const auto end = legacyDisplay.find('\n', start);
            legacyDisplay.erase(start, end - start + 1);
        }
    };
    removeField("vsync");
    removeField("fpsLimit");
    removeField("showFps");
    GameSettings restoredLegacy;
    ok &= Check(ParseGameSettings(legacyDisplay, restoredLegacy, error) &&
                restoredLegacy.vsync && restoredLegacy.fpsLimit == 240 &&
                !restoredLegacy.showFps,
                "settings created before DISPLAY options adopt safe defaults");
    auto invalidDisplay = nlohmann::json::parse(serialized);
    invalidDisplay["vsync"] = "yes";
    GameSettings unmodified = parsed;
    ok &= Check(!ParseGameSettings(invalidDisplay.dump(), unmodified, error) &&
                !unmodified.vsync && unmodified.fpsLimit == 144,
                "malformed display option cannot partially overwrite settings");
    invalidDisplay["vsync"] = true;
    invalidDisplay["fpsLimit"] = 150;
    ok &= Check(!ParseGameSettings(invalidDisplay.dump(), unmodified, error) &&
                !unmodified.vsync && unmodified.fpsLimit == 144,
                "unsupported frame caps are rejected atomically");
    // Previously saved untouched arrow defaults move to the DFJK preset,
    // but explicit arrow bindings stored by v2 remain customizable.
    GameSettings previousDefault;
    previousDefault.keys = {{KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT,
                              KEY_SPACE, KEY_R, KEY_H}};
    std::string oldConfig = SerializeGameSettings(previousDefault);
    const auto oldVersion = oldConfig.find("\"version\": 3");
    if (oldVersion != std::string::npos)
        oldConfig.replace(oldVersion, sizeof("\"version\": 3") - 1,
                          "\"version\": 1");
    GameSettings migrated;
    ok &= Check(ParseGameSettings(oldConfig, migrated, error) &&
                migrated.Key(GameAction::Up) == KEY_J &&
                migrated.Key(GameAction::Right) == KEY_K &&
                migrated.Key(GameAction::Down) == KEY_F &&
                migrated.Key(GameAction::Left) == KEY_D,
                "legacy default arrow settings automatically migrate once");
    GameSettings deliberateArrows;
    ok &= Check(ParseGameSettings(SerializeGameSettings(previousDefault),
                                  deliberateArrows, error) &&
                deliberateArrows.Key(GameAction::Up) == KEY_UP &&
                deliberateArrows.Key(GameAction::Left) == KEY_LEFT,
                "explicit arrow bindings in current settings version are preserved");

    GameSettings unchanged = parsed;
    const std::string corrupt =
        std::string(serialized).replace(serialized.find("\"version\": 3"),
                                        sizeof("\"version\": 3") - 1,
                                        "\"version\": 999");
    ok &= Check(!ParseGameSettings(corrupt, unchanged, error) &&
                unchanged.Key(GameAction::Up) == KEY_W,
                "unknown settings versions are rejected without partial mutation");

    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto folder = std::filesystem::temp_directory_path() /
                        ("locus-settings-test-" + unique);
    const auto file = folder / "settings.json";
    GameSettings onDisk;
    ok &= Check(LoadGameSettings(file, onDisk, error) &&
                onDisk.Key(GameAction::Up) == KEY_J,
                "missing settings file uses new shared DFJK defaults");
    ok &= Check(SaveGameSettings(file, options, error) &&
                LoadGameSettings(file, onDisk, error) &&
                onDisk.Key(GameAction::Up) == KEY_W &&
                !onDisk.hitFlash &&
                !onDisk.vsync && onDisk.fpsLimit == 144 && onDisk.showFps,
                onDisk.Shortcut(ShortcutAction::EditorSave).key == KEY_B &&
                "settings persist across application restart");
    options.volume = 0.9f;
    ok &= Check(SaveGameSettings(file, options, error) &&
                LoadGameSettings(file, onDisk, error) &&
                onDisk.volume > 0.89f,
                "changing settings atomically replaces previous config file");

    std::error_code ec;
    std::filesystem::remove_all(folder, ec);
    if (!ok) return 1;
    std::cout << "GAME_SETTINGS_PASS persistent bindings, validation, settings UI model\n";
    return 0;
}
