#include "app/skin.h"
#include "app/atomic_text_file.h"
#include "core/utf8.h"

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
    "PLAYER", "PLAYER CORE", "HUD BACKGROUND", "HUD TEXT", "HIT FLASH",
    "FIELD BORDER", "HOLD"
};
constexpr const char* kColorKeys[kSkinColorCount] = {
    "background", "trajectory", "outline", "junction",
    "player", "playerCore", "hudBackground", "hudText", "hitFlash",
    "fieldBorder", "hold"
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
        case 8: return skin.hitFlash;
        case 10: return skin.hold;
        default: return skin.fieldBorder;
    }
}

const Color& SkinColor(const Skin& skin, int index) noexcept {
    return SkinColor(const_cast<Skin&>(skin), index);
}

bool Skin::Valid(std::string& error) const {
    if (!core::utf8::ValidName(name, 64, 256)) {
        error = "Skin name must be 1..64 Unicode characters (valid UTF-8).";
        return false;
    }
    const struct { float value, min, max; const char* label; } fields[] = {
        {lookaheadMs, 50.0f, 10000.0f, "Preview time (ms)"},
        {pathWidth, 0.25f, 40.0f, "Trajectory width"},
        {outlineScale, 1.0f, 8.0f, "Outline scale"},
        {fadePower, 0.1f, 8.0f, "Fade curve"},
        {pathOpacity, 0.0f, 1.0f, "Trajectory opacity"},
        {junctionRadius, 1.0f, 64.0f, "Junction radius"},
        {reverseCapScale, 0.0f, 12.0f, "Reverse cap size"},
        {holdWidth, 0.25f, 60.0f, "Hold width"},
        {holdOpacity, 0.0f, 1.0f, "Hold opacity"},
        {holdHeadScale, 0.25f, 4.0f, "Hold head size"},
        {holdTailScale, 0.25f, 4.0f, "Hold tail size"},
        {playerRadius, 1.0f, 80.0f, "Player radius"},
        {baseSpeedPxPerSecond, kMinBaseSpeed, kMaxBaseSpeed, "Base movement speed"},
        {cameraLagMs, 0.0f, 1500.0f, "Camera lag (ms)"},
        {hitFlashScale, 0.1f, 8.0f, "Hit flash scale"}
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
        {"version", 3},
        {"name", skin.name},
        {"path", {
            {"lookaheadMs", skin.lookaheadMs},
            {"width", skin.pathWidth},
            {"outlineScale", skin.outlineScale},
            {"fadePower", skin.fadePower},
            {"opacity", skin.pathOpacity},
            {"junctionRadius", skin.junctionRadius},
            {"junctionGlow", skin.junctionGlow},
            {"reverseCapScale", skin.reverseCapScale},
            {"holdWidth", skin.holdWidth},
            {"holdOpacity", skin.holdOpacity},
            {"holdHeadScale", skin.holdHeadScale},
            {"holdTailScale", skin.holdTailScale}
        }},
        {"player", {
            {"radius", skin.playerRadius},
            {"speedPxPerSecond", skin.baseSpeedPxPerSecond},
            {"cameraLagMs", skin.cameraLagMs},
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
            !object.at("version").is_number_integer()) {
            error = "Unsupported skin version.";
            return false;
        }
        const int version = object.at("version").get<int>();
        if (version != 2 && version != 3) {
            error = "Unsupported skin version.";
            return false;
        }
        Skin parsed;
        parsed.name = object.at("name").get<std::string>();
        const auto& path = object.at("path");
        parsed.lookaheadMs = path.at("lookaheadMs").get<float>();
        parsed.pathWidth = path.at("width").get<float>();
        parsed.outlineScale = path.at("outlineScale").get<float>();
        parsed.fadePower = path.at("fadePower").get<float>();
        parsed.pathOpacity = path.at("opacity").get<float>();
        parsed.junctionRadius = path.at("junctionRadius").get<float>();
        parsed.junctionGlow = path.at("junctionGlow").get<bool>();
        parsed.reverseCapScale = path.at("reverseCapScale").get<float>();
        if (version == 3) {
            parsed.holdWidth = path.at("holdWidth").get<float>();
            parsed.holdOpacity = path.at("holdOpacity").get<float>();
            parsed.holdHeadScale = path.at("holdHeadScale").get<float>();
            parsed.holdTailScale = path.at("holdTailScale").get<float>();
        }
        const auto& player = object.at("player");
        parsed.playerRadius = player.at("radius").get<float>();
        parsed.baseSpeedPxPerSecond = player.at("speedPxPerSecond").get<float>();
        parsed.cameraLagMs = player.at("cameraLagMs").get<float>();
        parsed.playerGlow = player.at("glow").get<bool>();
        parsed.hitFlashScale = player.at("hitFlashScale").get<float>();
        parsed.showHud = object.at("hud").at("visible").get<bool>();
        const auto& colors = object.at("colors");
        const int colorCount = version == 2 ? kSkinColorCount - 1 : kSkinColorCount;
        for (int i = 0; i < colorCount; ++i) {
            if (!DecodeColor(colors.at(kColorKeys[i]), SkinColor(parsed, i))) {
                error = std::string("Invalid RGBA color: ") + kColorKeys[i];
                return false;
            }
        }
        if (version == 2) {
            // Preserve the prior implicit hold appearance on import.
            parsed.hold = parsed.playerCore;
            parsed.holdWidth = parsed.pathWidth * 1.35f;
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

bool LoadSkin(const std::filesystem::path& file, Skin& result,
              std::string& error, bool allowMissing) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(file, ec);
    if (ec) {
        error = "Cannot access skin file: " + ec.message();
        return false;
    }
    if (!exists) {
        if (!allowMissing) {
            error = "Selected skin file does not exist.";
            return false;
        }
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
    return AtomicReplaceTextFile(file, SerializeSkin(skin), error);
}

} // namespace locus::app
