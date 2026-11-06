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

// The four directional bindings are shared by scored gameplay and placement
// in the ROUTE editor. Non-direction actions remain gameplay bindings.
// Escape/F11 remain system controls.
struct GameSettings {
    std::array<int, kGameActionCount> keys{{
        KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT,
        KEY_SPACE, KEY_R, KEY_H
    }};
    float volume = 0.7f;
    int chordWindowMs = 85;
    bool hitFlash = true;
    bool fullscreen = false;

    int Key(GameAction action) const noexcept {
        return keys[static_cast<int>(action)];
    }
    bool Rebind(GameAction action, int key, std::string& error);
    bool Valid(std::string& error) const;
};

// The gameplay input array is Up, Right, Down, Left. Editor chord bits are
// Left=1, Down=2, Up=4, Right=8; both modes consume identical configured keys.
unsigned EditorDirectionMask(const std::array<bool, 4>& gameplayHeld) noexcept;
bool IsGameDirectionKey(const GameSettings& settings, int key) noexcept;
std::string EditorDirectionShortcut(const GameSettings& settings,
                                    unsigned editorDirectionMask);

bool ParseGameSettings(std::string_view text, GameSettings& result,
                       std::string& error);
std::string SerializeGameSettings(const GameSettings& settings);
bool LoadGameSettings(const std::filesystem::path& file, GameSettings& result,
                      std::string& error);
bool SaveGameSettings(const std::filesystem::path& file,
                      const GameSettings& settings, std::string& error);

} // namespace locus::app
