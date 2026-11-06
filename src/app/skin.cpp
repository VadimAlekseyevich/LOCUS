#include "app/skin.h"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#endif

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace locus::app {
namespace {
using Json = nlohmann::ordered_json;
constexpr const char* kColorNames[kSkinColorCount] = {
    "BACKGROUND", "TRAJECTORY", "OUTLINE", "JUNCTION",
    "PLAYER", "PLAYER CORE", "HUD BACKGROUND", "HUD TEXT", "HIT FLASH"
};
constexpr const char* kColorKeys[kSkinColorCount] = {
    "background", "trajectory", "outline", "junction",
    "player", "playerCore", "hudBackground", "hudText", "hitFlash"
};

Json EncodeColor(Color color) {
    return Json::array({color.r, color.g, color.b, color.a});
}

bool DecodeColor(const nlohmann::json& value, Color& output) {
    if (!value.is_array() || value.size() != 4) return false;
    std::array<unsigned char, 4> channels{};
    for (std::size_t i = 0; i < 4; ++i) {
        if (!value[i].is_number_integer()) return false;
        const auto number = value[i].get<int>();
        if (number < 0 || number > 255) return false;
        channels[i] = static_cast<unsigned char>(number);
    }
    output = {channels[0], channels[1], channels[2], channels[3]};
    return true;
}

bool InRange(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
} // namespace

const char* SkinColorLabel(int index) noexcept {
    return index >= 0 && index < kSkinColorCount ? kColorNames[index] : "UNKNOWN";
}

Color& SkinColor(Skin& skin, int index) noexcept {
    switch (index) {
        case 0: return skin.background;
        case 1: return skin.trajectory;
        case 2: return skin.outline;
        case 3: return skin.junction;
        case 4: return skin.player;
        case 5: return skin.playerCore;
        case 6: return skin.hudBackground;
        case 7: return skin.hudText;
        default: return skin.hitFlash;
    }
}

const Color& SkinColor(const Skin& skin, int index) noexcept {
    return SkinColor(const_cast<Skin&>(skin), index);
}

bool Skin::Valid(std::string& error) const {
    if (name.empty() || name.size() > 64 ||
        name.find_first_of("\r\n\t") != std::string::npos) {
        error = "Skin name must be 1..64 bytes without control characters.";
        return false;
    }
    const struct { float value, min, max; const char* label; } fields[] = {
        {lookaheadBeats, 0.5f, 16.0f, "Trajectory length"},
        {pathWidth, 1.0f, 16.0f, "Trajectory width"},
        {outlineScale, 1.0f, 4.0f, "Outline scale"},
        {fadePower, 0.25f, 4.0f, "Fade curve"},
        {pathOpacity, 0.1f, 1.0f, "Trajectory opacity"},
        {junctionRadius, 2.0f, 24.0f, "Junction radius"},
        {playerRadius, 4.0f, 30.0f, "Player radius"},
        {cameraStep, 25.0f, 100.0f, "Camera zoom"},
        {hitFlashScale, 0.5f, 3.0f, "Hit flash scale"}
    };
    for (const auto& field : fields) {
        if (!InRange(field.value, field.min, field.max)) {
            error = std::string(field.label) + " is out of range.";
            return false;
        }
    }
    error.clear();
    return true;
}

std::string SerializeSkin(const Skin& skin) {
    Json colors = Json::object();
    for (int i = 0; i < kSkinColorCount; ++i)
        colors[kColorKeys[i]] = EncodeColor(SkinColor(skin, i));
    const Json object = {
        {"version", 1},
        {"name", skin.name},
        {"path", {
            {"lookaheadBeats", skin.lookaheadBeats},
            {"width", skin.pathWidth},
            {"outlineScale", skin.outlineScale},
            {"fadePower", skin.fadePower},
            {"opacity", skin.pathOpacity},
            {"junctionRadius", skin.junctionRadius},
            {"junctionGlow", skin.junctionGlow}
        }},
        {"player", {
            {"radius", skin.playerRadius},
            {"cameraStep", skin.cameraStep},
            {"glow", skin.playerGlow},
            {"hitFlashScale", skin.hitFlashScale}
        }},
        {"hud", {{"visible", skin.showHud}}},
        {"colors", std::move(colors)}
    };
    return object.dump(2) + "\n";
}

bool ParseSkin(std::string_view text, Skin& result, std::string& error) {
    try {
        const auto object = nlohmann::json::parse(text.begin(), text.end());
        if (!object.is_object() || !object.contains("version") ||
            !object.at("version").is_number_integer() ||
            object.at("version").get<int>() != 1) {
            error = "Unsupported skin version.";
            return false;
        }
        Skin parsed;
        // Strictly require all version-1 fields; never partially apply bad data.
        parsed.name = object.at("name").get<std::string>();
        const auto& path = object.at("path");
        parsed.lookaheadBeats = path.at("lookaheadBeats").get<float>();
        parsed.pathWidth = path.at("width").get<float>();
        parsed.outlineScale = path.at("outlineScale").get<float>();
        parsed.fadePower = path.at("fadePower").get<float>();
        parsed.pathOpacity = path.at("opacity").get<float>();
        parsed.junctionRadius = path.at("junctionRadius").get<float>();
        parsed.junctionGlow = path.at("junctionGlow").get<bool>();
        const auto& player = object.at("player");
        parsed.playerRadius = player.at("radius").get<float>();
        parsed.cameraStep = player.at("cameraStep").get<float>();
        parsed.playerGlow = player.at("glow").get<bool>();
        parsed.hitFlashScale = player.at("hitFlashScale").get<float>();
        parsed.showHud = object.at("hud").at("visible").get<bool>();
        const auto& colors = object.at("colors");
        for (int i = 0; i < kSkinColorCount; ++i) {
            if (!DecodeColor(colors.at(kColorKeys[i]), SkinColor(parsed, i))) {
                error = std::string("Invalid RGBA color: ") + kColorKeys[i];
                return false;
            }
        }
        if (!parsed.Valid(error)) return false;
        result = std::move(parsed);
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = std::string("Invalid skin JSON: ") + e.what();
        return false;
    }
}

bool LoadSkin(const std::filesystem::path& file, Skin& result, std::string& error) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(file, ec);
    if (ec) {
        error = "Cannot access skin file: " + ec.message();
        return false;
    }
    if (!exists) {
        result = Skin{};
        error.clear();
        return true;
    }
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        error = "Cannot open skin JSON.";
        return false;
    }
    const std::string bytes((std::istreambuf_iterator<char>(stream)),
                             std::istreambuf_iterator<char>());
    if (bytes.size() > 65536) {
        error = "Skin JSON is too large.";
        return false;
    }
    return ParseSkin(bytes, result, error);
}

bool SaveSkin(const std::filesystem::path& file, const Skin& skin, std::string& error) {
    if (!skin.Valid(error)) return false;
    std::error_code ec;
    if (!file.parent_path().empty()) {
        std::filesystem::create_directories(file.parent_path(), ec);
        if (ec) {
            error = "Cannot create skin folder: " + ec.message();
            return false;
        }
    }
    const auto temporary = std::filesystem::path(file.wstring() + L".tmp");
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            error = "Cannot create skin temporary file.";
            return false;
        }
        stream << SerializeSkin(skin);
        stream.flush();
        if (!stream) {
            error = "Cannot finish writing skin JSON.";
            return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), file.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, ec);
        error = "Cannot replace skin JSON.";
        return false;
    }
#else
    std::filesystem::rename(temporary, file, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        error = "Cannot replace skin JSON.";
        return false;
    }
#endif
    error.clear();
    return true;
}

} // namespace locus::app
