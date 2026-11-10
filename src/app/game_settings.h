#pragma once

#include "raylib.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace locus::app {

enum class GameAction : int {
    Up, Right, Down, Left, Pause, Retry, Home, Count
};
constexpr int kGameActionCount = static_cast<int>(GameAction::Count);

const char* ActionName(GameAction action) noexcept;
std::string KeyboardKeyName(int key);

// Direction bindings control scored PLAY only. Editor notes are placed
// exclusively through clickable compass buttons, never by key presses.
// Escape/F11 remain system controls.
struct GameSettings {
    std::array<int, kGameActionCount> keys{{
        KEY_J, KEY_K, KEY_F, KEY_D,
        KEY_SPACE, KEY_R, KEY_H
    }};
    float volume = 0.7f;
    int chordWindowMs = 85;
    bool hitFlash = true;
    bool fullscreen = false;
    bool vsync = true;        // Prevent screen tearing; can be overridden by driver.
    int fpsLimit = 240;       // Safety ceiling when VSync is unsupported.
    bool showFps = false;     // Small diagnostic overlay on all screens.

    // 0 = unlimited; rate caps do not speed up gameplay/audio clocks.
    static constexpr std::array<int, 8> kFpsPresets{{
        30, 60, 90, 120, 144, 165, 240, 0
    }};
    void StepFpsLimit(int direction) noexcept;
    static bool ValidFpsLimit(int value) noexcept;
    std::string FpsLimitLabel() const;

    int Key(GameAction action) const noexcept {
        return keys[static_cast<int>(action)];
    }
    bool Rebind(GameAction action, int key, std::string& error);
    bool Valid(std::string& error) const;
};

bool IsGameDirectionKey(const GameSettings& settings, int key) noexcept;

bool ParseGameSettings(std::string_view text, GameSettings& result,
                       std::string& error);
std::string SerializeGameSettings(const GameSettings& settings);
bool LoadGameSettings(const std::filesystem::path& file, GameSettings& result,
                      std::string& error);
bool SaveGameSettings(const std::filesystem::path& file,
                      const GameSettings& settings, std::string& error);

} // namespace locus::app
