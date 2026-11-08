#pragma once

#include "app/movement_speed.h"
#include "raylib.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace locus::app {

// A skin describes presentation only. Never serialize it into a song/chart.
struct Skin {
    std::string name = "Default";
    float lookaheadBeats = 4.0f;  // distance ahead of the player, in song beats
    float pathWidth = 6.0f;      // pixels near the player
    float outlineScale = 1.8f;
    float fadePower = 1.0f;      // 1 = classic LOCUS falloff
    float pathOpacity = 1.0f;
    float junctionRadius = 9.5f;
    bool junctionGlow = true;

    float playerRadius = 12.5f;
    float baseSpeedPxPerSecond = kDefaultBaseSpeed; // all headings
    bool playerGlow = true;
    float hitFlashScale = 1.0f;
    bool showHud = true;

    Color background{21, 23, 33, 255};
    Color trajectory{255, 70, 84, 255};
    Color outline{49, 13, 24, 245};
    Color junction{255, 63, 72, 255};
    Color player{255, 140, 149, 255};
    Color playerCore{245, 253, 255, 255};
    Color hudBackground{22, 26, 37, 255};
    Color hudText{231, 237, 243, 255};
    Color hitFlash{255, 233, 174, 235};
    Color fieldBorder{245, 248, 252, 255}; // Thin square gameplay border.

    bool Valid(std::string& error) const;
};

// Stable names and indices used by the interactive color editor.
constexpr int kSkinColorCount = 10;
const char* SkinColorLabel(int index) noexcept;
Color& SkinColor(Skin& skin, int index) noexcept;
const Color& SkinColor(const Skin& skin, int index) noexcept;

std::string SerializeSkin(const Skin& skin);
bool ParseSkin(std::string_view json, Skin& result, std::string& error);
// Startup may use a default if the active skin file was never created.
 // Explicit user-selected files must pass allowMissing=false so a missing
 // file is an error and cannot silently replace a draft with defaults.
bool LoadSkin(const std::filesystem::path& file, Skin& result,
              std::string& error, bool allowMissing = true);
bool SaveSkin(const std::filesystem::path& file, const Skin& skin, std::string& error);

} // namespace locus::app
