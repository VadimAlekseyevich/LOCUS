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
    "FIELD BORDER",
    "HEB PERFECT ZONE",
    "HEB GOOD ZONE",
    "HEB BAD ZONE",
    "HEB PERFECT MARK",
    "HEB GOOD MARK",
    "HEB BAD MARK",
    "HEB CENTER"
};
constexpr const char* kColorKeys[kSkinColorCount] = {
    "background", "trajectory", "outline", "junction",
    "player", "playerCore", "hudBackground", "hudText", "hitFlash",
    "fieldBorder",
    "hitErrorPerfectZone",
    "hitErrorGoodZone",
    "hitErrorBadZone",
    "hitErrorPerfectMark",
    "hitErrorGoodMark",
    "hitErrorBadMark",
    "hitErrorCenter"
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
        case 9: return skin.fieldBorder;
        case 10: return skin.hitErrorPerfectZone;
        case 11: return skin.hitErrorGoodZone;
        case 12: return skin.hitErrorBadZone;
        case 13: return skin.hitErrorPerfectMark;
        case 14: return skin.hitErrorGoodMark;
        case 15: return skin.hitErrorBadMark;
        case 16: return skin.hitErrorCenter;
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
        {playerRadius, 1.0f, 80.0f, "Player radius"},
        {baseSpeedPxPerSecond, kMinBaseSpeed, kMaxBaseSpeed, "Base movement speed"},
        {cameraLagMs, 0.0f, 1500.0f, "Camera lag (ms)"},
        {hitFlashScale, 0.1f, 8.0f, "Hit flash scale"},
        {hitErrorLength, 40.0f, 480.0f, "BAR LENGTH"},
        {hitErrorThickness, 1.0f, 24.0f, "BAR THICKNESS"},
        {hitErrorMarkWidth, 1.0f, 12.0f, "MARK WIDTH"},
        {hitErrorMarkHeight, 2.0f, 48.0f, "MARK HEIGHT"},
        {hitErrorFadeMs, 100.0f, 5000.0f, "FADE TIME (MS)"},
        {hitErrorZoneOpacity, 0.0f, 1.0f, "ZONE OPACITY"},
        {hitErrorMarkOpacity, 0.0f, 1.0f, "MARK OPACITY"},
        {hitErrorBottomOffset, 0.0f, 300.0f, "BOTTOM OFFSET"}
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
        {"version", 2},
        {"name", skin.name},
        {"path", {
            {"lookaheadMs", skin.lookaheadMs},
            {"width", skin.pathWidth},
            {"outlineScale", skin.outlineScale},
            {"fadePower", skin.fadePower},
            {"opacity", skin.pathOpacity},
            {"junctionRadius", skin.junctionRadius},
            {"junctionGlow", skin.junctionGlow},
            {"reverseCapScale", skin.reverseCapScale}
        }},
        {"player", {
            {"radius", skin.playerRadius},
            {"speedPxPerSecond", skin.baseSpeedPxPerSecond},
            {"cameraLagMs", skin.cameraLagMs},
            {"glow", skin.playerGlow},
            {"hitFlashScale", skin.hitFlashScale}
        }},
        {"hud", {{"visible", skin.showHud}, {"hitErrorBar", skin.showHitErrorBar},
            {"hitErrorStyle", {
                {"length", skin.hitErrorLength},
                {"thickness", skin.hitErrorThickness},
                {"markWidth", skin.hitErrorMarkWidth},
                {"markHeight", skin.hitErrorMarkHeight},
                {"fadeMs", skin.hitErrorFadeMs},
                {"zoneOpacity", skin.hitErrorZoneOpacity},
                {"markOpacity", skin.hitErrorMarkOpacity},
                {"bottomOffset", skin.hitErrorBottomOffset}
            }}
        }},
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
        if (object.at("version").get<int>() != 2) {
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
        const auto& player = object.at("player");
        parsed.playerRadius = player.at("radius").get<float>();
        parsed.baseSpeedPxPerSecond = player.at("speedPxPerSecond").get<float>();
        parsed.cameraLagMs = player.at("cameraLagMs").get<float>();
        parsed.playerGlow = player.at("glow").get<bool>();
        parsed.hitFlashScale = player.at("hitFlashScale").get<float>();
        parsed.showHud = object.at("hud").at("visible").get<bool>();
        parsed.showHitErrorBar = object.at("hud").value("hitErrorBar", false);
        const auto& hud = object.at("hud");
        if (hud.contains("hitErrorStyle")) {
            const auto& style = hud.at("hitErrorStyle");
            if (!style.is_object()) { error = "Invalid hit error style."; return false; }
            parsed.hitErrorLength = style.value("length", parsed.hitErrorLength);
            parsed.hitErrorThickness = style.value("thickness", parsed.hitErrorThickness);
            parsed.hitErrorMarkWidth = style.value("markWidth", parsed.hitErrorMarkWidth);
            parsed.hitErrorMarkHeight = style.value("markHeight", parsed.hitErrorMarkHeight);
            parsed.hitErrorFadeMs = style.value("fadeMs", parsed.hitErrorFadeMs);
            parsed.hitErrorZoneOpacity = style.value("zoneOpacity", parsed.hitErrorZoneOpacity);
            parsed.hitErrorMarkOpacity = style.value("markOpacity", parsed.hitErrorMarkOpacity);
            parsed.hitErrorBottomOffset = style.value("bottomOffset", parsed.hitErrorBottomOffset);
        }
        const auto& colors = object.at("colors");
        for (int i = 0; i < kSkinColorCount; ++i) {
            if (i >= kBaseSkinColorCount && !colors.contains(kColorKeys[i])) continue;
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
