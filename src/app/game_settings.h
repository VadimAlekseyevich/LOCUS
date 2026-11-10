#pragma once

#include "raylib.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace locus::app {

enum class GameAction : int {
    Up, Right, Down, Left, Pause, Retry, Home,
    Practice100, Practice75, Practice50, Practice25, Practice10,
    SpeedUp, SpeedDown, PreviewUp, PreviewDown, Count
};
constexpr int kGameActionCount = static_cast<int>(GameAction::Count);

// Menu, library/listen and editor shortcuts have their own context. A
// modifier combination is configurable; Ctrl+S and Ctrl+Shift+S can differ.
enum class ShortcutAction : int {
    Back, OpenLibrary, OpenEditor, OpenSkin, OpenSettings, Fullscreen,
    RefreshLibrary, ChooseMusic, ChooseChart, ListenPause, ListenReplay,
    EditorSave, EditorSaveAs, EditorOpen, EditorNew, EditorTest,
    EditorPreview, EditorRestart, EditorCopy, EditorCut, EditorPaste,
    EditorUndo, EditorRedo, EditorMirrorH, EditorMirrorV,
    EditorHeadingLeft, EditorHeadingRight, EditorZoomIn, EditorZoomOut,
    EditorSnapMore, EditorSnapLess, Count
};
constexpr int kNavigationShortcutCount =
    static_cast<int>(ShortcutAction::EditorSave);
constexpr int kShortcutActionCount = static_cast<int>(ShortcutAction::Count);

struct KeyChord {
    int key = KEY_NULL;
    bool ctrl = false, shift = false;
    bool operator==(const KeyChord& other) const noexcept {
        return key == other.key && ctrl == other.ctrl && shift == other.shift;
    }
};

const char* ActionName(GameAction action) noexcept;
const char* ShortcutName(ShortcutAction action) noexcept;
std::string KeyboardKeyName(int key);
std::string KeyboardChordName(KeyChord chord);

// Direction bindings control scored PLAY only. Editor notes are placed
// exclusively through clickable compass buttons, never by key presses.
struct GameSettings {
    std::array<int, kGameActionCount> keys{{
        KEY_J, KEY_K, KEY_F, KEY_D,
        KEY_ESCAPE, KEY_R, KEY_H,
        KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE,
        KEY_RIGHT_BRACKET, KEY_LEFT_BRACKET, KEY_PERIOD, KEY_COMMA
    }};
    std::array<KeyChord, kShortcutActionCount> shortcuts{{
        {KEY_ESCAPE}, {KEY_P}, {KEY_E}, {KEY_K}, {KEY_S},
        {KEY_F11}, {KEY_F5}, {KEY_O}, {KEY_C},
        {KEY_SPACE}, {KEY_R},
        {KEY_S, true}, {KEY_S, true, true},
        {KEY_O, true}, {KEY_N, true}, {KEY_T},
        {KEY_SPACE}, {KEY_R}, {KEY_C, true}, {KEY_X, true},
        {KEY_V, true}, {KEY_Z, true}, {KEY_Y, true},
        {KEY_I}, {KEY_I, false, true}, {KEY_Q}, {KEY_E},
        {KEY_EQUAL}, {KEY_MINUS}, {KEY_PERIOD}, {KEY_COMMA}
    }};
    float volume = 0.7f;
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
    bool RebindShortcut(ShortcutAction action, KeyChord chord,
                        std::string& error);
    KeyChord Shortcut(ShortcutAction action) const noexcept {
        return shortcuts[static_cast<int>(action)];
    }
    bool Valid(std::string& error) const;
};

// Strict modifier matching prevents Ctrl+S from also activating bare S.
inline bool ShortcutPressed(const GameSettings& settings,
                            ShortcutAction action) noexcept {
    const auto chord = settings.Shortcut(action);
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    return ctrl == chord.ctrl && shift == chord.shift &&
           IsKeyPressed(chord.key);
}

bool IsGameDirectionKey(const GameSettings& settings, int key) noexcept;

bool ParseGameSettings(std::string_view text, GameSettings& result,
                       std::string& error);
std::string SerializeGameSettings(const GameSettings& settings);
bool LoadGameSettings(const std::filesystem::path& file, GameSettings& result,
                      std::string& error);
bool SaveGameSettings(const std::filesystem::path& file,
                      const GameSettings& settings, std::string& error);

} // namespace locus::app
