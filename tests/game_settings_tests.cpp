#include "app/game_settings.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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
                options.Key(GameAction::Up) == KEY_UP &&
                options.Key(GameAction::Right) == KEY_RIGHT &&
                options.Key(GameAction::Down) == KEY_DOWN &&
                options.Key(GameAction::Left) == KEY_LEFT,
                "defaults preserve four arrow bindings");
    // Gameplay input ordering (Up,Right,Down,Left) must resolve to the
    // same absolute directions used to author notes in ROUTE.
    ok &= Check(EditorDirectionMask({true, false, false, false}) == 4u &&
                EditorDirectionMask({false, true, false, false}) == 8u &&
                EditorDirectionMask({false, false, true, false}) == 2u &&
                EditorDirectionMask({false, false, false, true}) == 1u &&
                EditorDirectionMask({true, true, false, false}) == 12u &&
                EditorDirectionMask({false, false, true, true}) == 3u,
                "game directions and diagonal chords map consistently to editor pad");
    ok &= Check(EditorDirectionShortcut(options, 5u) == "LEFT+UP" &&
                EditorDirectionShortcut(options, 12u) == "UP+RIGHT" &&
                IsGameDirectionKey(options, KEY_LEFT) &&
                !IsGameDirectionKey(options, KEY_D),
                "default editor labels and conflict detection use arrow bindings");
    ok &= Check(options.Rebind(GameAction::Up, KEY_W, error) &&
                options.Rebind(GameAction::Right, KEY_D, error) &&
                options.Key(GameAction::Up) == KEY_W,
                "gameplay directions can use WASD or other keyboard keys");
    ok &= Check(EditorDirectionShortcut(options, 12u) == "W+D" &&
                EditorDirectionShortcut(options, 5u) == "LEFT+W" &&
                IsGameDirectionKey(options, KEY_W) &&
                IsGameDirectionKey(options, KEY_D) &&
                !IsGameDirectionKey(options, KEY_UP),
                "rebound gameplay keys immediately update editor compass shortcuts");
    ok &= Check(!options.Rebind(GameAction::Left, KEY_W, error) &&
                options.Key(GameAction::Left) == KEY_LEFT,
                "duplicate binding is rejected without changing old key");
    ok &= Check(!options.Rebind(GameAction::Pause, KEY_ESCAPE, error) &&
                !options.Rebind(GameAction::Retry, KEY_F11, error) &&
                options.Key(GameAction::Pause) == KEY_SPACE,
                "reserved Escape and fullscreen shortcut cannot be rebound");

    options.volume = 0.4f;
    options.hitFlash = false;
    options.fullscreen = true;
    options.chordWindowMs = 120;
    const std::string serialized = SerializeGameSettings(options);
    GameSettings parsed;
    ok &= Check(ParseGameSettings(serialized, parsed, error) &&
                parsed.Key(GameAction::Up) == KEY_W &&
                parsed.Key(GameAction::Right) == KEY_D &&
                parsed.volume > 0.39f && parsed.volume < 0.41f &&
                !parsed.hitFlash && parsed.fullscreen &&
                parsed.chordWindowMs == 120,
                "all audio, visuals, chords and custom keys survive JSON roundtrip");
    ok &= Check(EditorDirectionShortcut(parsed, 12u) == "W+D" &&
                EditorDirectionMask({true, true, false, false}) == 12u,
                "persisted bindings drive the same editor chords after restarting");
    GameSettings unchanged = parsed;
    const std::string corrupt =
        std::string(serialized).replace(serialized.find("\"version\": 1"),
                                        sizeof("\"version\": 1") - 1,
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
                onDisk.Key(GameAction::Up) == KEY_UP,
                "missing settings file uses defaults");
    ok &= Check(SaveGameSettings(file, options, error) &&
                LoadGameSettings(file, onDisk, error) &&
                onDisk.Key(GameAction::Up) == KEY_W &&
                onDisk.chordWindowMs == 120 &&
                !onDisk.hitFlash,
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
